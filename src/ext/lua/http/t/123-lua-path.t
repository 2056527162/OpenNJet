# vim:set ft= ts=4 sw=4 et fdm=marker:
use Test::Nginx::Socket::Lua;

#worker_connections(1014);
#master_on();
#workers(2);
#log_level('warn');

repeat_each(2);
#repeat_each(1);

plan tests => repeat_each() * (blocks() * 3 + 1);

$ENV{LUA_PATH} = "../lua-resty-core/lib/?.lua;../lua-resty-lrucache/lib/?.lua;/foo/bar/baz";
$ENV{LUA_CPATH} = "/baz/bar/foo";
#no_diff();
#no_long_string();
master_on();
no_shuffle();
check_accum_error_log();
run_tests();

__DATA__

=== TEST 1: LUA_PATH & LUA_CPATH env (code cache on)
--- main_config
env LUA_PATH;
env LUA_CPATH;

--- config
    location /lua {
        content_by_lua '
            njt.say(package.path)
            njt.say(package.cpath)
        ';
    }
--- request
GET /lua
--- response_body_like
(?:\.\.\/lua-resty-core\/lib\/\?\.lua;\.\.\/lua-resty-lrucache\/lib\/\?\.lua;){1,2}\/foo\/bar\/baz
/baz/bar/foo

--- no_error_log
[error]



=== TEST 2: LUA_PATH & LUA_CPATH env (code cache off)
--- main_config
env LUA_PATH;
env LUA_CPATH;

--- config
    lua_code_cache off;
    location /lua {
        content_by_lua '
            njt.say(package.path)
            njt.say(package.cpath)
        ';
    }
--- request
GET /lua
--- response_body_like
(?:\.\.\/lua-resty-core\/lib\/\?\.lua;\.\.\/lua-resty-lrucache\/lib\/\?\.lua;){1,2}\/foo\/bar\/baz
/baz/bar/foo

--- no_error_log
[error]
--- error_log eval
qr/\[alert\] .*? lua_code_cache is off/
