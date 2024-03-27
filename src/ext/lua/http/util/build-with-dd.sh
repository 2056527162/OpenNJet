#!/usr/bin/env bash

# this script is for developers only.
# dependent on the njt-build script from the nginx-devel-utils repository:
#   https://github.com/openresty/nginx-devel-utils/blob/master/njt-build
# the resulting nginx is located at ./work/nginx/sbin/nginx

root=`pwd`
version=${1:-1.4.1}
home=~
force=$2

add_fake_shm_module="--add-module=$root/t/data/fake-shm-module"

add_http3_module=--with-http_v3_module
answer=`$root/util/ver-ge "$NGINX_VERSION" 1.25.1`
if [ "$OPENSSL_VER" = "1.1.0l" ] || [ "$answer" = "N" ]; then
    add_http3_module=""
fi

disable_pcre2=--without-pcre2
answer=`$root/util/ver-ge "$NGINX_VERSION" 1.25.1`
if [ "$answer" = "N" ] || [ "$USE_PCRE2" = "Y" ]; then
    disable_pcre2=""
fi
if [ "$USE_PCRE2" = "Y" ]; then
    PCRE_INC=$PCRE2_INC
    PCRE_LIB=$PCRE2_LIB
fi

time njt-build $force $version \
            --with-threads \
            --with-pcre-jit \
            $disable_pcre2 \
            --with-ipv6 \
            --with-cc-opt="-DNGX_LUA_USE_ASSERT -I$PCRE_INC -I$OPENSSL_INC -DDDEBUG=1" \
            --with-http_v2_module \
            $add_http3_module \
            --with-http_realip_module \
            --with-http_ssl_module \
            --add-module=$root/../ndk-nginx-module \
            --add-module=$root/../set-misc-nginx-module \
            --with-ld-opt="-L$PCRE_LIB -L$OPENSSL_LIB -Wl,-rpath,$PCRE_LIB:$LIBDRIZZLE_LIB:$OPENSSL_LIB" \
            --without-mail_pop3_module \
            --without-mail_imap_module \
            --with-http_image_filter_module \
            --without-mail_smtp_module \
            --with-stream \
            --with-stream_ssl_module \
            --without-http_upstream_ip_hash_module \
            --without-http_memcached_module \
            --without-http_auth_basic_module \
            --without-http_userid_module \
            --with-http_auth_request_module \
            --add-module=$root/../echo-nginx-module \
            --add-module=$root/../memc-nginx-module \
            --add-module=$root/../srcache-nginx-module \
            --add-module=$root \
            --add-module=$root/../lua-upstream-nginx-module \
            --add-module=$root/../headers-more-nginx-module \
            --add-module=$root/../drizzle-nginx-module \
            --add-module=$root/../rds-json-nginx-module \
            --add-module=$root/../coolkit-nginx-module \
            --add-module=$root/../redis2-nginx-module \
            --add-module=$root/../stream-lua-nginx-module \
            --add-module=$root/t/data/fake-module \
            $add_fake_shm_module \
            --add-module=$root/t/data/fake-delayed-load-module \
            --with-http_gunzip_module \
            --with-http_dav_module \
            --with-select_module \
            --with-poll_module \
            $opts \
            --with-debug
