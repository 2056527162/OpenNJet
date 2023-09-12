
/*
 * Copyright (C) Nginx, Inc.
 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */


#include <njt_config.h>
#include <njt_core.h>
#include <njt_event.h>
#include <njt_event_quic_connection.h>


/* RFC 9001, 5.4.1.  Header Protection Application: 5-byte mask */
#define NJT_QUIC_HP_LEN               5

#define NJT_QUIC_AES_128_KEY_LEN      16


static njt_int_t njt_hkdf_expand(u_char *out_key, size_t out_len,
    const EVP_MD *digest, const u_char *prk, size_t prk_len,
    const u_char *info, size_t info_len);
static njt_int_t njt_hkdf_extract(u_char *out_key, size_t *out_len,
    const EVP_MD *digest, const u_char *secret, size_t secret_len,
    const u_char *salt, size_t salt_len);

static uint64_t njt_quic_parse_pn(u_char **pos, njt_int_t len, u_char *mask,
    uint64_t *largest_pn);

static njt_int_t njt_quic_tls_open(const njt_quic_cipher_t *cipher,
    njt_quic_secret_t *s, njt_str_t *out, u_char *nonce, njt_str_t *in,
    njt_str_t *ad, njt_log_t *log);
static njt_int_t njt_quic_tls_hp(njt_log_t *log, const EVP_CIPHER *cipher,
    njt_quic_secret_t *s, u_char *out, u_char *in);

static njt_int_t njt_quic_create_packet(njt_quic_header_t *pkt,
    njt_str_t *res);
static njt_int_t njt_quic_create_retry_packet(njt_quic_header_t *pkt,
    njt_str_t *res);


njt_int_t
njt_quic_ciphers(njt_uint_t id, njt_quic_ciphers_t *ciphers,
    enum ssl_encryption_level_t level)
{
    njt_int_t  len;

    if (level == ssl_encryption_initial) {
        id = TLS1_3_CK_AES_128_GCM_SHA256;
    }

    switch (id) {

    case TLS1_3_CK_AES_128_GCM_SHA256:
#ifdef OPENSSL_IS_BORINGSSL
        ciphers->c = EVP_aead_aes_128_gcm();
#else
        ciphers->c = EVP_aes_128_gcm();
#endif
        ciphers->hp = EVP_aes_128_ctr();
        ciphers->d = EVP_sha256();
        len = 16;
        break;

    case TLS1_3_CK_AES_256_GCM_SHA384:
#ifdef OPENSSL_IS_BORINGSSL
        ciphers->c = EVP_aead_aes_256_gcm();
#else
        ciphers->c = EVP_aes_256_gcm();
#endif
        ciphers->hp = EVP_aes_256_ctr();
        ciphers->d = EVP_sha384();
        len = 32;
        break;

    case TLS1_3_CK_CHACHA20_POLY1305_SHA256:
#ifdef OPENSSL_IS_BORINGSSL
        ciphers->c = EVP_aead_chacha20_poly1305();
#else
        ciphers->c = EVP_chacha20_poly1305();
#endif
#ifdef OPENSSL_IS_BORINGSSL
        ciphers->hp = (const EVP_CIPHER *) EVP_aead_chacha20_poly1305();
#else
        ciphers->hp = EVP_chacha20();
#endif
        ciphers->d = EVP_sha256();
        len = 32;
        break;

#ifndef OPENSSL_IS_BORINGSSL
    case TLS1_3_CK_AES_128_CCM_SHA256:
        ciphers->c = EVP_aes_128_ccm();
        ciphers->hp = EVP_aes_128_ctr();
        ciphers->d = EVP_sha256();
        len = 16;
        break;
#endif

    default:
        return NJT_ERROR;
    }

    return len;
}


njt_int_t
njt_quic_keys_set_initial_secret(njt_quic_keys_t *keys, njt_str_t *secret,
    njt_log_t *log)
{
    size_t              is_len;
    uint8_t             is[SHA256_DIGEST_LENGTH];
    njt_str_t           iss;
    njt_uint_t          i;
    const EVP_MD       *digest;
    njt_quic_hkdf_t     seq[8];
    njt_quic_secret_t  *client, *server;

    static const uint8_t salt[20] =
        "\x38\x76\x2c\xf7\xf5\x59\x34\xb3\x4d\x17"
        "\x9a\xe6\xa4\xc8\x0c\xad\xcc\xbb\x7f\x0a";

    client = &keys->secrets[ssl_encryption_initial].client;
    server = &keys->secrets[ssl_encryption_initial].server;

    /*
     * RFC 9001, section 5.  Packet Protection
     *
     * Initial packets use AEAD_AES_128_GCM.  The hash function
     * for HKDF when deriving initial secrets and keys is SHA-256.
     */

    digest = EVP_sha256();
    is_len = SHA256_DIGEST_LENGTH;

    if (njt_hkdf_extract(is, &is_len, digest, secret->data, secret->len,
                         salt, sizeof(salt))
        != NJT_OK)
    {
        return NJT_ERROR;
    }

    iss.len = is_len;
    iss.data = is;

    njt_log_debug0(NJT_LOG_DEBUG_EVENT, log, 0,
                   "quic njt_quic_set_initial_secret");
#ifdef NJT_QUIC_DEBUG_CRYPTO
    njt_log_debug3(NJT_LOG_DEBUG_EVENT, log, 0,
                   "quic salt len:%uz %*xs", sizeof(salt), sizeof(salt), salt);
    njt_log_debug3(NJT_LOG_DEBUG_EVENT, log, 0,
                   "quic initial secret len:%uz %*xs", is_len, is_len, is);
#endif

    client->secret.len = SHA256_DIGEST_LENGTH;
    server->secret.len = SHA256_DIGEST_LENGTH;

    client->key.len = NJT_QUIC_AES_128_KEY_LEN;
    server->key.len = NJT_QUIC_AES_128_KEY_LEN;

    client->hp.len = NJT_QUIC_AES_128_KEY_LEN;
    server->hp.len = NJT_QUIC_AES_128_KEY_LEN;

    client->iv.len = NJT_QUIC_IV_LEN;
    server->iv.len = NJT_QUIC_IV_LEN;

    /* labels per RFC 9001, 5.1. Packet Protection Keys */
    njt_quic_hkdf_set(&seq[0], "tls13 client in", &client->secret, &iss);
    njt_quic_hkdf_set(&seq[1], "tls13 quic key", &client->key, &client->secret);
    njt_quic_hkdf_set(&seq[2], "tls13 quic iv", &client->iv, &client->secret);
    njt_quic_hkdf_set(&seq[3], "tls13 quic hp", &client->hp, &client->secret);
    njt_quic_hkdf_set(&seq[4], "tls13 server in", &server->secret, &iss);
    njt_quic_hkdf_set(&seq[5], "tls13 quic key", &server->key, &server->secret);
    njt_quic_hkdf_set(&seq[6], "tls13 quic iv", &server->iv, &server->secret);
    njt_quic_hkdf_set(&seq[7], "tls13 quic hp", &server->hp, &server->secret);

    for (i = 0; i < (sizeof(seq) / sizeof(seq[0])); i++) {
        if (njt_quic_hkdf_expand(&seq[i], digest, log) != NJT_OK) {
            return NJT_ERROR;
        }
    }

    return NJT_OK;
}


njt_int_t
njt_quic_hkdf_expand(njt_quic_hkdf_t *h, const EVP_MD *digest, njt_log_t *log)
{
    size_t    info_len;
    uint8_t  *p;
    uint8_t   info[20];

    info_len = 2 + 1 + h->label_len + 1;

    info[0] = 0;
    info[1] = h->out_len;
    info[2] = h->label_len;

    p = njt_cpymem(&info[3], h->label, h->label_len);
    *p = '\0';

    if (njt_hkdf_expand(h->out, h->out_len, digest,
                        h->prk, h->prk_len, info, info_len)
        != NJT_OK)
    {
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "njt_hkdf_expand(%*s) failed", h->label_len, h->label);
        return NJT_ERROR;
    }

#ifdef NJT_QUIC_DEBUG_CRYPTO
    njt_log_debug5(NJT_LOG_DEBUG_EVENT, log, 0,
                   "quic expand \"%*s\" len:%uz %*xs",
                   h->label_len, h->label, h->out_len, h->out_len, h->out);
#endif

    return NJT_OK;
}


static njt_int_t
njt_hkdf_expand(u_char *out_key, size_t out_len, const EVP_MD *digest,
    const uint8_t *prk, size_t prk_len, const u_char *info, size_t info_len)
{
#ifdef OPENSSL_IS_BORINGSSL

    if (HKDF_expand(out_key, out_len, digest, prk, prk_len, info, info_len)
        == 0)
    {
        return NJT_ERROR;
    }

    return NJT_OK;

#else

    EVP_PKEY_CTX  *pctx;

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (pctx == NULL) {
        return NJT_ERROR;
    }

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXPAND_ONLY) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, digest) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, prk, prk_len) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info, info_len) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_derive(pctx, out_key, &out_len) <= 0) {
        goto failed;
    }

    EVP_PKEY_CTX_free(pctx);

    return NJT_OK;

failed:

    EVP_PKEY_CTX_free(pctx);

    return NJT_ERROR;

#endif
}


static njt_int_t
njt_hkdf_extract(u_char *out_key, size_t *out_len, const EVP_MD *digest,
    const u_char *secret, size_t secret_len, const u_char *salt,
    size_t salt_len)
{
#ifdef OPENSSL_IS_BORINGSSL

    if (HKDF_extract(out_key, out_len, digest, secret, secret_len, salt,
                     salt_len)
        == 0)
    {
        return NJT_ERROR;
    }

    return NJT_OK;

#else

    EVP_PKEY_CTX  *pctx;

    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (pctx == NULL) {
        return NJT_ERROR;
    }

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_hkdf_mode(pctx, EVP_PKEY_HKDEF_MODE_EXTRACT_ONLY) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, digest) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, secret, secret_len) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, salt_len) <= 0) {
        goto failed;
    }

    if (EVP_PKEY_derive(pctx, out_key, out_len) <= 0) {
        goto failed;
    }

    EVP_PKEY_CTX_free(pctx);

    return NJT_OK;

failed:

    EVP_PKEY_CTX_free(pctx);

    return NJT_ERROR;

#endif
}


static njt_int_t
njt_quic_tls_open(const njt_quic_cipher_t *cipher, njt_quic_secret_t *s,
    njt_str_t *out, u_char *nonce, njt_str_t *in, njt_str_t *ad, njt_log_t *log)
{

#ifdef OPENSSL_IS_BORINGSSL
    EVP_AEAD_CTX  *ctx;

    ctx = EVP_AEAD_CTX_new(cipher, s->key.data, s->key.len,
                           EVP_AEAD_DEFAULT_TAG_LENGTH);
    if (ctx == NULL) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_AEAD_CTX_new() failed");
        return NJT_ERROR;
    }

    if (EVP_AEAD_CTX_open(ctx, out->data, &out->len, out->len, nonce, s->iv.len,
                          in->data, in->len, ad->data, ad->len)
        != 1)
    {
        EVP_AEAD_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_AEAD_CTX_open() failed");
        return NJT_ERROR;
    }

    EVP_AEAD_CTX_free(ctx);
#else
    int              len;
    EVP_CIPHER_CTX  *ctx;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_CIPHER_CTX_new() failed");
        return NJT_ERROR;
    }

    if (EVP_DecryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_DecryptInit_ex() failed");
        return NJT_ERROR;
    }

    in->len -= NJT_QUIC_TAG_LEN;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, NJT_QUIC_TAG_LEN, 
                            in->data + in->len)
        == 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "EVP_CIPHER_CTX_ctrl(EVP_CTRL_AEAD_SET_TAG) failed");
        return NJT_ERROR;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, s->iv.len, NULL)
        == 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "EVP_CIPHER_CTX_ctrl(EVP_CTRL_AEAD_SET_IVLEN) failed");
        return NJT_ERROR;
    }

    if (EVP_DecryptInit_ex(ctx, NULL, NULL, s->key.data, nonce) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_DecryptInit_ex() failed");
        return NJT_ERROR;
    }

    if (EVP_CIPHER_mode(cipher) == EVP_CIPH_CCM_MODE
        && EVP_DecryptUpdate(ctx, NULL, &len, NULL, in->len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_DecryptUpdate() failed");
        return NJT_ERROR;
    }

    if (EVP_DecryptUpdate(ctx, NULL, &len, ad->data, ad->len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_DecryptUpdate() failed");
        return NJT_ERROR;
    }

    if (EVP_DecryptUpdate(ctx, out->data, &len, in->data, in->len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_DecryptUpdate() failed");
        return NJT_ERROR;
    }

    out->len = len;

    if (EVP_DecryptFinal_ex(ctx, out->data + out->len, &len) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_DecryptFinal_ex failed");
        return NJT_ERROR;
    }

    out->len += len;

    EVP_CIPHER_CTX_free(ctx);
#endif

    return NJT_OK;
}


njt_int_t
njt_quic_tls_seal(const njt_quic_cipher_t *cipher, njt_quic_secret_t *s,
    njt_str_t *out, u_char *nonce, njt_str_t *in, njt_str_t *ad, njt_log_t *log)
{

#ifdef OPENSSL_IS_BORINGSSL
    EVP_AEAD_CTX  *ctx;

    ctx = EVP_AEAD_CTX_new(cipher, s->key.data, s->key.len,
                           EVP_AEAD_DEFAULT_TAG_LENGTH);
    if (ctx == NULL) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_AEAD_CTX_new() failed");
        return NJT_ERROR;
    }

    if (EVP_AEAD_CTX_seal(ctx, out->data, &out->len, out->len, nonce, s->iv.len,
                          in->data, in->len, ad->data, ad->len)
        != 1)
    {
        EVP_AEAD_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_AEAD_CTX_seal() failed");
        return NJT_ERROR;
    }

    EVP_AEAD_CTX_free(ctx);
#else
    int              len;
    EVP_CIPHER_CTX  *ctx;

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_CIPHER_CTX_new() failed");
        return NJT_ERROR;
    }

    if (EVP_EncryptInit_ex(ctx, cipher, NULL, NULL, NULL) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptInit_ex() failed");
        return NJT_ERROR;
    }

    if (EVP_CIPHER_mode(cipher) == EVP_CIPH_CCM_MODE
        && EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, NJT_QUIC_TAG_LEN,
                               NULL)
           == 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "EVP_CIPHER_CTX_ctrl(EVP_CTRL_AEAD_SET_TAG) failed");
        return NJT_ERROR;
    }

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, s->iv.len, NULL)
        == 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "EVP_CIPHER_CTX_ctrl(EVP_CTRL_AEAD_SET_IVLEN) failed");
        return NJT_ERROR;
    }

    if (EVP_EncryptInit_ex(ctx, NULL, NULL, s->key.data, nonce) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptInit_ex() failed");
        return NJT_ERROR;
    }

    if (EVP_CIPHER_mode(cipher) == EVP_CIPH_CCM_MODE
        && EVP_EncryptUpdate(ctx, NULL, &len, NULL, in->len) != 1)
    {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptUpdate() failed");
        return NJT_ERROR;
    }

    if (EVP_EncryptUpdate(ctx, NULL, &len, ad->data, ad->len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptUpdate() failed");
        return NJT_ERROR;
    }

    if (EVP_EncryptUpdate(ctx, out->data, &len, in->data, in->len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptUpdate() failed");
        return NJT_ERROR;
    }

    out->len = len;

    if (EVP_EncryptFinal_ex(ctx, out->data + out->len, &len) <= 0) {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptFinal_ex failed");
        return NJT_ERROR;
    }

    out->len += len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_GET_TAG, NJT_QUIC_TAG_LEN,
                            out->data + out->len)
        == 0)
    {
        EVP_CIPHER_CTX_free(ctx);
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "EVP_CIPHER_CTX_ctrl(EVP_CTRL_AEAD_GET_TAG) failed");
        return NJT_ERROR;
    }

    out->len += NJT_QUIC_TAG_LEN;

    EVP_CIPHER_CTX_free(ctx);
#endif
    return NJT_OK;
}


static njt_int_t
njt_quic_tls_hp(njt_log_t *log, const EVP_CIPHER *cipher,
    njt_quic_secret_t *s, u_char *out, u_char *in)
{
    int              outlen;
    EVP_CIPHER_CTX  *ctx;
    u_char           zero[NJT_QUIC_HP_LEN] = {0};

#ifdef OPENSSL_IS_BORINGSSL
    uint32_t         cnt;

    njt_memcpy(&cnt, in, sizeof(uint32_t));

    if (cipher == (const EVP_CIPHER *) EVP_aead_chacha20_poly1305()) {
        CRYPTO_chacha_20(out, zero, NJT_QUIC_HP_LEN, s->hp.data, &in[4], cnt);
        return NJT_OK;
    }
#endif

    ctx = EVP_CIPHER_CTX_new();
    if (ctx == NULL) {
        return NJT_ERROR;
    }

    if (EVP_EncryptInit_ex(ctx, cipher, NULL, s->hp.data, in) != 1) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptInit_ex() failed");
        goto failed;
    }

    if (!EVP_EncryptUpdate(ctx, out, &outlen, zero, NJT_QUIC_HP_LEN)) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptUpdate() failed");
        goto failed;
    }

    if (!EVP_EncryptFinal_ex(ctx, out + NJT_QUIC_HP_LEN, &outlen)) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "EVP_EncryptFinal_Ex() failed");
        goto failed;
    }

    EVP_CIPHER_CTX_free(ctx);

    return NJT_OK;

failed:

    EVP_CIPHER_CTX_free(ctx);

    return NJT_ERROR;
}


njt_int_t
njt_quic_keys_set_encryption_secret(njt_log_t *log, njt_uint_t is_write,
    njt_quic_keys_t *keys, enum ssl_encryption_level_t level,
    const SSL_CIPHER *cipher, const uint8_t *secret, size_t secret_len)
{
    njt_int_t            key_len;
    njt_str_t            secret_str;
    njt_uint_t           i;
    njt_quic_hkdf_t      seq[3];
    njt_quic_secret_t   *peer_secret;
    njt_quic_ciphers_t   ciphers;

    peer_secret = is_write ? &keys->secrets[level].server
                           : &keys->secrets[level].client;

    keys->cipher = SSL_CIPHER_get_id(cipher);

    key_len = njt_quic_ciphers(keys->cipher, &ciphers, level);

    if (key_len == NJT_ERROR) {
        njt_ssl_error(NJT_LOG_INFO, log, 0, "unexpected cipher");
        return NJT_ERROR;
    }

    if (sizeof(peer_secret->secret.data) < secret_len) {
        njt_log_error(NJT_LOG_ALERT, log, 0,
                      "unexpected secret len: %uz", secret_len);
        return NJT_ERROR;
    }

    peer_secret->secret.len = secret_len;
    njt_memcpy(peer_secret->secret.data, secret, secret_len);

    peer_secret->key.len = key_len;
    peer_secret->iv.len = NJT_QUIC_IV_LEN;
    peer_secret->hp.len = key_len;

    secret_str.len = secret_len;
    secret_str.data = (u_char *) secret;

    njt_quic_hkdf_set(&seq[0], "tls13 quic key",
                      &peer_secret->key, &secret_str);
    njt_quic_hkdf_set(&seq[1], "tls13 quic iv", &peer_secret->iv, &secret_str);
    njt_quic_hkdf_set(&seq[2], "tls13 quic hp", &peer_secret->hp, &secret_str);

    for (i = 0; i < (sizeof(seq) / sizeof(seq[0])); i++) {
        if (njt_quic_hkdf_expand(&seq[i], ciphers.d, log) != NJT_OK) {
            return NJT_ERROR;
        }
    }

    return NJT_OK;
}


njt_uint_t
njt_quic_keys_available(njt_quic_keys_t *keys,
    enum ssl_encryption_level_t level)
{
    return keys->secrets[level].client.key.len != 0;
}


void
njt_quic_keys_discard(njt_quic_keys_t *keys,
    enum ssl_encryption_level_t level)
{
    keys->secrets[level].client.key.len = 0;
}


void
njt_quic_keys_switch(njt_connection_t *c, njt_quic_keys_t *keys)
{
    njt_quic_secrets_t  *current, *next, tmp;

    current = &keys->secrets[ssl_encryption_application];
    next = &keys->next_key;

    tmp = *current;
    *current = *next;
    *next = tmp;
}


njt_int_t
njt_quic_keys_update(njt_connection_t *c, njt_quic_keys_t *keys)
{
    njt_uint_t           i;
    njt_quic_hkdf_t      seq[6];
    njt_quic_ciphers_t   ciphers;
    njt_quic_secrets_t  *current, *next;

    current = &keys->secrets[ssl_encryption_application];
    next = &keys->next_key;

    njt_log_debug0(NJT_LOG_DEBUG_EVENT, c->log, 0, "quic key update");

    if (njt_quic_ciphers(keys->cipher, &ciphers, ssl_encryption_application)
        == NJT_ERROR)
    {
        return NJT_ERROR;
    }

    next->client.secret.len = current->client.secret.len;
    next->client.key.len = current->client.key.len;
    next->client.iv.len = NJT_QUIC_IV_LEN;
    next->client.hp = current->client.hp;

    next->server.secret.len = current->server.secret.len;
    next->server.key.len = current->server.key.len;
    next->server.iv.len = NJT_QUIC_IV_LEN;
    next->server.hp = current->server.hp;

    njt_quic_hkdf_set(&seq[0], "tls13 quic ku",
                      &next->client.secret, &current->client.secret);
    njt_quic_hkdf_set(&seq[1], "tls13 quic key",
                      &next->client.key, &next->client.secret);
    njt_quic_hkdf_set(&seq[2], "tls13 quic iv",
                      &next->client.iv, &next->client.secret);
    njt_quic_hkdf_set(&seq[3], "tls13 quic ku",
                      &next->server.secret, &current->server.secret);
    njt_quic_hkdf_set(&seq[4], "tls13 quic key",
                      &next->server.key, &next->server.secret);
    njt_quic_hkdf_set(&seq[5], "tls13 quic iv",
                      &next->server.iv, &next->server.secret);

    for (i = 0; i < (sizeof(seq) / sizeof(seq[0])); i++) {
        if (njt_quic_hkdf_expand(&seq[i], ciphers.d, c->log) != NJT_OK) {
            return NJT_ERROR;
        }
    }

    return NJT_OK;
}


static njt_int_t
njt_quic_create_packet(njt_quic_header_t *pkt, njt_str_t *res)
{
    u_char              *pnp, *sample;
    njt_str_t            ad, out;
    njt_uint_t           i;
    njt_quic_secret_t   *secret;
    njt_quic_ciphers_t   ciphers;
    u_char               nonce[NJT_QUIC_IV_LEN], mask[NJT_QUIC_HP_LEN];

    ad.data = res->data;
    ad.len = njt_quic_create_header(pkt, ad.data, &pnp);

    out.len = pkt->payload.len + NJT_QUIC_TAG_LEN;
    out.data = res->data + ad.len;

#ifdef NJT_QUIC_DEBUG_CRYPTO
    njt_log_debug2(NJT_LOG_DEBUG_EVENT, pkt->log, 0,
                   "quic ad len:%uz %xV", ad.len, &ad);
#endif

    if (njt_quic_ciphers(pkt->keys->cipher, &ciphers, pkt->level) == NJT_ERROR)
    {
        return NJT_ERROR;
    }

    secret = &pkt->keys->secrets[pkt->level].server;

    njt_memcpy(nonce, secret->iv.data, secret->iv.len);
    njt_quic_compute_nonce(nonce, sizeof(nonce), pkt->number);

    if (njt_quic_tls_seal(ciphers.c, secret, &out,
                          nonce, &pkt->payload, &ad, pkt->log)
        != NJT_OK)
    {
        return NJT_ERROR;
    }

    sample = &out.data[4 - pkt->num_len];
    if (njt_quic_tls_hp(pkt->log, ciphers.hp, secret, mask, sample)
        != NJT_OK)
    {
        return NJT_ERROR;
    }

    /* RFC 9001, 5.4.1.  Header Protection Application */
    ad.data[0] ^= mask[0] & njt_quic_pkt_hp_mask(pkt->flags);

    for (i = 0; i < pkt->num_len; i++) {
        pnp[i] ^= mask[i + 1];
    }

    res->len = ad.len + out.len;

    return NJT_OK;
}


static njt_int_t
njt_quic_create_retry_packet(njt_quic_header_t *pkt, njt_str_t *res)
{
    u_char              *start;
    njt_str_t            ad, itag;
    njt_quic_secret_t    secret;
    njt_quic_ciphers_t   ciphers;

    /* 5.8.  Retry Packet Integrity */
    static u_char     key[16] =
        "\xbe\x0c\x69\x0b\x9f\x66\x57\x5a\x1d\x76\x6b\x54\xe3\x68\xc8\x4e";
    static u_char     nonce[NJT_QUIC_IV_LEN] =
        "\x46\x15\x99\xd3\x5d\x63\x2b\xf2\x23\x98\x25\xbb";
    static njt_str_t  in = njt_string("");

    ad.data = res->data;
    ad.len = njt_quic_create_retry_itag(pkt, ad.data, &start);

    itag.data = ad.data + ad.len;
    itag.len = NJT_QUIC_TAG_LEN;

#ifdef NJT_QUIC_DEBUG_CRYPTO
    njt_log_debug2(NJT_LOG_DEBUG_EVENT, pkt->log, 0,
                   "quic retry itag len:%uz %xV", ad.len, &ad);
#endif

    if (njt_quic_ciphers(0, &ciphers, pkt->level) == NJT_ERROR) {
        return NJT_ERROR;
    }

    secret.key.len = sizeof(key);
    njt_memcpy(secret.key.data, key, sizeof(key));
    secret.iv.len = NJT_QUIC_IV_LEN;

    if (njt_quic_tls_seal(ciphers.c, &secret, &itag, nonce, &in, &ad, pkt->log)
        != NJT_OK)
    {
        return NJT_ERROR;
    }

    res->len = itag.data + itag.len - start;
    res->data = start;

    return NJT_OK;
}


njt_int_t
njt_quic_derive_key(njt_log_t *log, const char *label, njt_str_t *secret,
    njt_str_t *salt, u_char *out, size_t len)
{
    size_t         is_len, info_len;
    uint8_t       *p;
    const EVP_MD  *digest;

    uint8_t        is[SHA256_DIGEST_LENGTH];
    uint8_t        info[20];

    digest = EVP_sha256();
    is_len = SHA256_DIGEST_LENGTH;

    if (njt_hkdf_extract(is, &is_len, digest, secret->data, secret->len,
                         salt->data, salt->len)
        != NJT_OK)
    {
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "njt_hkdf_extract(%s) failed", label);
        return NJT_ERROR;
    }

    info[0] = 0;
    info[1] = len;
    info[2] = njt_strlen(label);

    info_len = 2 + 1 + info[2] + 1;

    if (info_len >= 20) {
        njt_log_error(NJT_LOG_INFO, log, 0,
                      "njt_quic_create_key label \"%s\" too long", label);
        return NJT_ERROR;
    }

    p = njt_cpymem(&info[3], label, info[2]);
    *p = '\0';

    if (njt_hkdf_expand(out, len, digest, is, is_len, info, info_len) != NJT_OK)
    {
        njt_ssl_error(NJT_LOG_INFO, log, 0,
                      "njt_hkdf_expand(%s) failed", label);
        return NJT_ERROR;
    }

    return NJT_OK;
}


static uint64_t
njt_quic_parse_pn(u_char **pos, njt_int_t len, u_char *mask,
    uint64_t *largest_pn)
{
    u_char    *p;
    uint64_t   truncated_pn, expected_pn, candidate_pn;
    uint64_t   pn_nbits, pn_win, pn_hwin, pn_mask;

    pn_nbits = njt_min(len * 8, 62);

    p = *pos;
    truncated_pn = *p++ ^ *mask++;

    while (--len) {
        truncated_pn = (truncated_pn << 8) + (*p++ ^ *mask++);
    }

    *pos = p;

    expected_pn = *largest_pn + 1;
    pn_win = 1ULL << pn_nbits;
    pn_hwin = pn_win / 2;
    pn_mask = pn_win - 1;

    candidate_pn = (expected_pn & ~pn_mask) | truncated_pn;

    if ((int64_t) candidate_pn <= (int64_t) (expected_pn - pn_hwin)
        && candidate_pn < (1ULL << 62) - pn_win)
    {
        candidate_pn += pn_win;

    } else if (candidate_pn > expected_pn + pn_hwin
               && candidate_pn >= pn_win)
    {
        candidate_pn -= pn_win;
    }

    *largest_pn = njt_max((int64_t) *largest_pn, (int64_t) candidate_pn);

    return candidate_pn;
}


void
njt_quic_compute_nonce(u_char *nonce, size_t len, uint64_t pn)
{
    nonce[len - 8] ^= (pn >> 56) & 0x3f;
    nonce[len - 7] ^= (pn >> 48) & 0xff;
    nonce[len - 6] ^= (pn >> 40) & 0xff;
    nonce[len - 5] ^= (pn >> 32) & 0xff;
    nonce[len - 4] ^= (pn >> 24) & 0xff;
    nonce[len - 3] ^= (pn >> 16) & 0xff;
    nonce[len - 2] ^= (pn >> 8) & 0xff;
    nonce[len - 1] ^= pn & 0xff;
}


njt_int_t
njt_quic_encrypt(njt_quic_header_t *pkt, njt_str_t *res)
{
    if (njt_quic_pkt_retry(pkt->flags)) {
        return njt_quic_create_retry_packet(pkt, res);
    }

    return njt_quic_create_packet(pkt, res);
}


njt_int_t
njt_quic_decrypt(njt_quic_header_t *pkt, uint64_t *largest_pn)
{
    u_char              *p, *sample;
    size_t               len;
    uint64_t             pn, lpn;
    njt_int_t            pnl, rc;
    njt_str_t            in, ad;
    njt_uint_t           key_phase;
    njt_quic_secret_t   *secret;
    njt_quic_ciphers_t   ciphers;
    uint8_t              nonce[NJT_QUIC_IV_LEN], mask[NJT_QUIC_HP_LEN];

    if (njt_quic_ciphers(pkt->keys->cipher, &ciphers, pkt->level) == NJT_ERROR)
    {
        return NJT_ERROR;
    }

    secret = &pkt->keys->secrets[pkt->level].client;

    p = pkt->raw->pos;
    len = pkt->data + pkt->len - p;

    /*
     * RFC 9001, 5.4.2. Header Protection Sample
     *           5.4.3. AES-Based Header Protection
     *           5.4.4. ChaCha20-Based Header Protection
     *
     * the Packet Number field is assumed to be 4 bytes long
     * AES and ChaCha20 algorithms sample 16 bytes
     */

    if (len < NJT_QUIC_TAG_LEN + 4) {
        return NJT_DECLINED;
    }

    sample = p + 4;

    /* header protection */

    if (njt_quic_tls_hp(pkt->log, ciphers.hp, secret, mask, sample)
        != NJT_OK)
    {
        return NJT_DECLINED;
    }

    pkt->flags ^= mask[0] & njt_quic_pkt_hp_mask(pkt->flags);

    if (njt_quic_short_pkt(pkt->flags)) {
        key_phase = (pkt->flags & NJT_QUIC_PKT_KPHASE) != 0;

        if (key_phase != pkt->key_phase) {
            secret = &pkt->keys->next_key.client;
            pkt->key_update = 1;
        }
    }

    lpn = *largest_pn;

    pnl = (pkt->flags & 0x03) + 1;
    pn = njt_quic_parse_pn(&p, pnl, &mask[1], &lpn);

    pkt->pn = pn;

    njt_log_debug1(NJT_LOG_DEBUG_EVENT, pkt->log, 0,
                   "quic packet rx clearflags:%xd", pkt->flags);
    njt_log_debug2(NJT_LOG_DEBUG_EVENT, pkt->log, 0,
                   "quic packet rx number:%uL len:%xi", pn, pnl);

    /* packet protection */

    in.data = p;
    in.len = len - pnl;

    ad.len = p - pkt->data;
    ad.data = pkt->plaintext;

    njt_memcpy(ad.data, pkt->data, ad.len);
    ad.data[0] = pkt->flags;

    do {
        ad.data[ad.len - pnl] = pn >> (8 * (pnl - 1)) % 256;
    } while (--pnl);

    njt_memcpy(nonce, secret->iv.data, secret->iv.len);
    njt_quic_compute_nonce(nonce, sizeof(nonce), pn);

#ifdef NJT_QUIC_DEBUG_CRYPTO
    njt_log_debug2(NJT_LOG_DEBUG_EVENT, pkt->log, 0,
                   "quic ad len:%uz %xV", ad.len, &ad);
#endif

    pkt->payload.len = in.len - NJT_QUIC_TAG_LEN;
    pkt->payload.data = pkt->plaintext + ad.len;

    rc = njt_quic_tls_open(ciphers.c, secret, &pkt->payload,
                           nonce, &in, &ad, pkt->log);
    if (rc != NJT_OK) {
        return NJT_DECLINED;
    }

    if (pkt->payload.len == 0) {
        /*
         * RFC 9000, 12.4.  Frames and Frame Types
         *
         * An endpoint MUST treat receipt of a packet containing no
         * frames as a connection error of type PROTOCOL_VIOLATION.
         */
        njt_log_error(NJT_LOG_INFO, pkt->log, 0, "quic zero-length packet");
        pkt->error = NJT_QUIC_ERR_PROTOCOL_VIOLATION;
        return NJT_ERROR;
    }

    if (pkt->flags & njt_quic_pkt_rb_mask(pkt->flags)) {
        /*
         * RFC 9000, Reserved Bits
         *
         * An endpoint MUST treat receipt of a packet that has
         * a non-zero value for these bits, after removing both
         * packet and header protection, as a connection error
         * of type PROTOCOL_VIOLATION.
         */
        njt_log_error(NJT_LOG_INFO, pkt->log, 0,
                      "quic reserved bit set in packet");
        pkt->error = NJT_QUIC_ERR_PROTOCOL_VIOLATION;
        return NJT_ERROR;
    }

#if defined(NJT_QUIC_DEBUG_CRYPTO) && defined(NJT_QUIC_DEBUG_PACKETS)
    njt_log_debug2(NJT_LOG_DEBUG_EVENT, pkt->log, 0,
                   "quic packet payload len:%uz %xV",
                   pkt->payload.len, &pkt->payload);
#endif

    *largest_pn = lpn;

    return NJT_OK;
}