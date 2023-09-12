

/* This file was generated by JSON Schema to C.
 * Any changes made to it will be lost on regeneration. 

 * Copyright (C) 2021-2023  TMLake(Beijing) Technology Co., Ltd.
 */

#ifndef PARSER_BWLIST_H
#define PARSER_BWLIST_H
#include <stdint.h>
#include <stdbool.h>
#include "njt_core.h"
#include "js2c_njet_builtins.h"
/* ===================== Generated type declarations ===================== */
typedef struct dynbwlist_locationDef_t_s dynbwlist_locationDef_t; //forward decl for public definition
typedef njt_str_t dynbwlist_locationDef_location_t;

typedef enum dynbwlist_locationDef_accessIpv4_item_rule_t_e{
    DYNBWLIST_LOCATIONDEF_ACCESSIPV4_ITEM_RULE_ALLOW,
    DYNBWLIST_LOCATIONDEF_ACCESSIPV4_ITEM_RULE_DENY
} dynbwlist_locationDef_accessIpv4_item_rule_t;

typedef njt_str_t dynbwlist_locationDef_accessIpv4_item_addr_t;

typedef njt_str_t dynbwlist_locationDef_accessIpv4_item_mask_t;

typedef struct dynbwlist_locationDef_accessIpv4_item_t_s {
    dynbwlist_locationDef_accessIpv4_item_rule_t rule;
    dynbwlist_locationDef_accessIpv4_item_addr_t addr;
    dynbwlist_locationDef_accessIpv4_item_mask_t mask;
    unsigned int is_rule_set:1;
    unsigned int is_addr_set:1;
    unsigned int is_mask_set:1;
} dynbwlist_locationDef_accessIpv4_item_t;

typedef njt_array_t  dynbwlist_locationDef_accessIpv4_t;
typedef enum dynbwlist_locationDef_accessIpv6_item_rule_t_e{
    DYNBWLIST_LOCATIONDEF_ACCESSIPV6_ITEM_RULE_ALLOW,
    DYNBWLIST_LOCATIONDEF_ACCESSIPV6_ITEM_RULE_DENY
} dynbwlist_locationDef_accessIpv6_item_rule_t;

typedef njt_str_t dynbwlist_locationDef_accessIpv6_item_addr_t;

typedef njt_str_t dynbwlist_locationDef_accessIpv6_item_mask_t;

typedef struct dynbwlist_locationDef_accessIpv6_item_t_s {
    dynbwlist_locationDef_accessIpv6_item_rule_t rule;
    dynbwlist_locationDef_accessIpv6_item_addr_t addr;
    dynbwlist_locationDef_accessIpv6_item_mask_t mask;
    unsigned int is_rule_set:1;
    unsigned int is_addr_set:1;
    unsigned int is_mask_set:1;
} dynbwlist_locationDef_accessIpv6_item_t;

typedef njt_array_t  dynbwlist_locationDef_accessIpv6_t;
typedef dynbwlist_locationDef_t dynbwlist_locationDef_locations_item_t; //ref def
typedef njt_array_t  dynbwlist_locationDef_locations_t;
typedef struct dynbwlist_locationDef_t_s {
    dynbwlist_locationDef_location_t location;
    dynbwlist_locationDef_accessIpv4_t *accessIpv4;
    dynbwlist_locationDef_accessIpv6_t *accessIpv6;
    dynbwlist_locationDef_locations_t *locations;
    unsigned int is_location_set:1;
    unsigned int is_accessIpv4_set:1;
    unsigned int is_accessIpv6_set:1;
    unsigned int is_locations_set:1;
} dynbwlist_locationDef_t;

dynbwlist_locationDef_accessIpv4_item_rule_t get_dynbwlist_locationDef_accessIpv4_item_rule(dynbwlist_locationDef_accessIpv4_item_t *out);
dynbwlist_locationDef_accessIpv4_item_addr_t* get_dynbwlist_locationDef_accessIpv4_item_addr(dynbwlist_locationDef_accessIpv4_item_t *out);
dynbwlist_locationDef_accessIpv4_item_mask_t* get_dynbwlist_locationDef_accessIpv4_item_mask(dynbwlist_locationDef_accessIpv4_item_t *out);
dynbwlist_locationDef_accessIpv4_item_t* get_dynbwlist_locationDef_accessIpv4_item(dynbwlist_locationDef_accessIpv4_t *out, size_t idx);
dynbwlist_locationDef_accessIpv6_item_rule_t get_dynbwlist_locationDef_accessIpv6_item_rule(dynbwlist_locationDef_accessIpv6_item_t *out);
dynbwlist_locationDef_accessIpv6_item_addr_t* get_dynbwlist_locationDef_accessIpv6_item_addr(dynbwlist_locationDef_accessIpv6_item_t *out);
dynbwlist_locationDef_accessIpv6_item_mask_t* get_dynbwlist_locationDef_accessIpv6_item_mask(dynbwlist_locationDef_accessIpv6_item_t *out);
dynbwlist_locationDef_accessIpv6_item_t* get_dynbwlist_locationDef_accessIpv6_item(dynbwlist_locationDef_accessIpv6_t *out, size_t idx);
dynbwlist_locationDef_locations_item_t* get_dynbwlist_locationDef_locations_item(dynbwlist_locationDef_locations_t *out, size_t idx);
dynbwlist_locationDef_location_t* get_dynbwlist_locationDef_location(dynbwlist_locationDef_t *out);
// CHECK ARRAY not exceeding bounds before calling this func
dynbwlist_locationDef_accessIpv4_t* get_dynbwlist_locationDef_accessIpv4(dynbwlist_locationDef_t *out);
// CHECK ARRAY not exceeding bounds before calling this func
dynbwlist_locationDef_accessIpv6_t* get_dynbwlist_locationDef_accessIpv6(dynbwlist_locationDef_t *out);
// CHECK ARRAY not exceeding bounds before calling this func
dynbwlist_locationDef_locations_t* get_dynbwlist_locationDef_locations(dynbwlist_locationDef_t *out);
void set_dynbwlist_locationDef_location(dynbwlist_locationDef_t* obj, dynbwlist_locationDef_location_t* field);
void set_dynbwlist_locationDef_accessIpv4_item_rule(dynbwlist_locationDef_accessIpv4_item_t* obj, dynbwlist_locationDef_accessIpv4_item_rule_t field);
void set_dynbwlist_locationDef_accessIpv4_item_addr(dynbwlist_locationDef_accessIpv4_item_t* obj, dynbwlist_locationDef_accessIpv4_item_addr_t* field);
void set_dynbwlist_locationDef_accessIpv4_item_mask(dynbwlist_locationDef_accessIpv4_item_t* obj, dynbwlist_locationDef_accessIpv4_item_mask_t* field);
dynbwlist_locationDef_accessIpv4_item_t* create_dynbwlist_locationDef_accessIpv4_item(njt_pool_t *pool);
int add_item_dynbwlist_locationDef_accessIpv4(dynbwlist_locationDef_accessIpv4_t *src, dynbwlist_locationDef_accessIpv4_item_t* items);
dynbwlist_locationDef_accessIpv4_t* create_dynbwlist_locationDef_accessIpv4(njt_pool_t *pool, size_t nelts);
void set_dynbwlist_locationDef_accessIpv4(dynbwlist_locationDef_t* obj, dynbwlist_locationDef_accessIpv4_t* field);
void set_dynbwlist_locationDef_accessIpv6_item_rule(dynbwlist_locationDef_accessIpv6_item_t* obj, dynbwlist_locationDef_accessIpv6_item_rule_t field);
void set_dynbwlist_locationDef_accessIpv6_item_addr(dynbwlist_locationDef_accessIpv6_item_t* obj, dynbwlist_locationDef_accessIpv6_item_addr_t* field);
void set_dynbwlist_locationDef_accessIpv6_item_mask(dynbwlist_locationDef_accessIpv6_item_t* obj, dynbwlist_locationDef_accessIpv6_item_mask_t* field);
dynbwlist_locationDef_accessIpv6_item_t* create_dynbwlist_locationDef_accessIpv6_item(njt_pool_t *pool);
int add_item_dynbwlist_locationDef_accessIpv6(dynbwlist_locationDef_accessIpv6_t *src, dynbwlist_locationDef_accessIpv6_item_t* items);
dynbwlist_locationDef_accessIpv6_t* create_dynbwlist_locationDef_accessIpv6(njt_pool_t *pool, size_t nelts);
void set_dynbwlist_locationDef_accessIpv6(dynbwlist_locationDef_t* obj, dynbwlist_locationDef_accessIpv6_t* field);
int add_item_dynbwlist_locationDef_locations(dynbwlist_locationDef_locations_t *src, dynbwlist_locationDef_locations_item_t* items);
dynbwlist_locationDef_locations_t* create_dynbwlist_locationDef_locations(njt_pool_t *pool, size_t nelts);
void set_dynbwlist_locationDef_locations(dynbwlist_locationDef_t* obj, dynbwlist_locationDef_locations_t* field);
dynbwlist_locationDef_t* create_dynbwlist_locationDef(njt_pool_t *pool);
typedef njt_str_t dynbwlist_servers_item_listens_item_t;

typedef njt_array_t  dynbwlist_servers_item_listens_t;
typedef njt_str_t dynbwlist_servers_item_serverNames_item_t;

typedef njt_array_t  dynbwlist_servers_item_serverNames_t;
typedef dynbwlist_locationDef_t dynbwlist_servers_item_locations_item_t; //ref def
typedef njt_array_t  dynbwlist_servers_item_locations_t;
typedef struct dynbwlist_servers_item_t_s {
    dynbwlist_servers_item_listens_t *listens;
    dynbwlist_servers_item_serverNames_t *serverNames;
    dynbwlist_servers_item_locations_t *locations;
    unsigned int is_listens_set:1;
    unsigned int is_serverNames_set:1;
    unsigned int is_locations_set:1;
} dynbwlist_servers_item_t;

typedef njt_array_t  dynbwlist_servers_t;
typedef struct dynbwlist_t_s {
    dynbwlist_servers_t *servers;
    unsigned int is_servers_set:1;
} dynbwlist_t;

dynbwlist_servers_item_listens_item_t* get_dynbwlist_servers_item_listens_item(dynbwlist_servers_item_listens_t *out, size_t idx);
dynbwlist_servers_item_serverNames_item_t* get_dynbwlist_servers_item_serverNames_item(dynbwlist_servers_item_serverNames_t *out, size_t idx);
dynbwlist_servers_item_locations_item_t* get_dynbwlist_servers_item_locations_item(dynbwlist_servers_item_locations_t *out, size_t idx);
// CHECK ARRAY not exceeding bounds before calling this func
dynbwlist_servers_item_listens_t* get_dynbwlist_servers_item_listens(dynbwlist_servers_item_t *out);
// CHECK ARRAY not exceeding bounds before calling this func
dynbwlist_servers_item_serverNames_t* get_dynbwlist_servers_item_serverNames(dynbwlist_servers_item_t *out);
// CHECK ARRAY not exceeding bounds before calling this func
dynbwlist_servers_item_locations_t* get_dynbwlist_servers_item_locations(dynbwlist_servers_item_t *out);
dynbwlist_servers_item_t* get_dynbwlist_servers_item(dynbwlist_servers_t *out, size_t idx);
// CHECK ARRAY not exceeding bounds before calling this func
dynbwlist_servers_t* get_dynbwlist_servers(dynbwlist_t *out);
int add_item_dynbwlist_servers_item_listens(dynbwlist_servers_item_listens_t *src, dynbwlist_servers_item_listens_item_t* items);
dynbwlist_servers_item_listens_t* create_dynbwlist_servers_item_listens(njt_pool_t *pool, size_t nelts);
void set_dynbwlist_servers_item_listens(dynbwlist_servers_item_t* obj, dynbwlist_servers_item_listens_t* field);
int add_item_dynbwlist_servers_item_serverNames(dynbwlist_servers_item_serverNames_t *src, dynbwlist_servers_item_serverNames_item_t* items);
dynbwlist_servers_item_serverNames_t* create_dynbwlist_servers_item_serverNames(njt_pool_t *pool, size_t nelts);
void set_dynbwlist_servers_item_serverNames(dynbwlist_servers_item_t* obj, dynbwlist_servers_item_serverNames_t* field);
int add_item_dynbwlist_servers_item_locations(dynbwlist_servers_item_locations_t *src, dynbwlist_servers_item_locations_item_t* items);
dynbwlist_servers_item_locations_t* create_dynbwlist_servers_item_locations(njt_pool_t *pool, size_t nelts);
void set_dynbwlist_servers_item_locations(dynbwlist_servers_item_t* obj, dynbwlist_servers_item_locations_t* field);
dynbwlist_servers_item_t* create_dynbwlist_servers_item(njt_pool_t *pool);
int add_item_dynbwlist_servers(dynbwlist_servers_t *src, dynbwlist_servers_item_t* items);
dynbwlist_servers_t* create_dynbwlist_servers(njt_pool_t *pool, size_t nelts);
void set_dynbwlist_servers(dynbwlist_t* obj, dynbwlist_servers_t* field);
dynbwlist_t* create_dynbwlist(njt_pool_t *pool);
dynbwlist_t* json_parse_dynbwlist(njt_pool_t *pool, const njt_str_t *json_string, js2c_parse_error_t *err_ret);
njt_str_t* to_json_dynbwlist(njt_pool_t *pool, dynbwlist_t *out, njt_int_t flags);
#endif /* PARSER_BWLIST_H */