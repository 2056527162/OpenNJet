# vim:set ft= ts=4 sw=4 et fdm=marker:

use Test::Nginx::Socket::Lua;

repeat_each(2);

plan tests => repeat_each() * (2 * blocks() + 1);

$ENV{TEST_NGINX_HTML_DIR} ||= html_dir();

no_long_string();
#no_shuffle();

run_tests();

__DATA__

=== TEST 1: connection refused (unix domain socket)
--- config
    location /test {
        rewrite_by_lua '
            local sock = njt.socket.tcp()
            local ok, err = sock:connect("unix:/tmp/nosuchfile.sock")
            njt.say("connect: ", ok, " ", err)

            local bytes
            bytes, err = sock:send("hello")
            njt.say("send: ", bytes, " ", err)

            local line
            line, err = sock:receive()
            njt.say("receive: ", line, " ", err)

            ok, err = sock:close()
            njt.say("close: ", ok, " ", err)
        ';

        content_by_lua return;
    }
--- request
    GET /test
--- response_body
connect: nil no such file or directory
send: nil closed
receive: nil closed
close: nil closed
--- error_log eval
qr{\[crit\] .*? connect\(\) to unix:/tmp/nosuchfile\.sock failed}



=== TEST 2: invalid host argument
--- http_server
    server {
        listen /tmp/test-nginx.sock;
        default_type 'text/plain';

        server_tokens off;
        location /foo {
            content_by_lua 'njt.say("foo")';
            more_clear_headers Date;
        }
    }
--- config
    location /test {
        rewrite_by_lua '
            local sock = njt.socket.tcp()
            local ok, err = sock:connect("/tmp/test-nginx.sock")
            if not ok then
                njt.say("failed to connect: ", err)
                return
            end

            njt.say("connected: ", ok)
        ';

        content_by_lua return;
    }
--- request
    GET /test
--- response_body
failed to connect: missing the port number



=== TEST 3: sanity
--- http_config
    server {
        listen unix:$TEST_NGINX_HTML_DIR/nginx.sock;
        default_type 'text/plain';

        server_tokens off;
        location /foo {
            content_by_lua 'njt.say("foo")';
            more_clear_headers Date;
        }
    }
--- config
    location /test {
        rewrite_by_lua '
            local sock = njt.socket.tcp()
            local ok, err = sock:connect("unix:$TEST_NGINX_HTML_DIR/nginx.sock")
            if not ok then
                njt.say("failed to connect: ", err)
                return
            end

            njt.say("connected: ", ok)

            local req = "GET /foo HTTP/1.0\\r\\nHost: localhost\\r\\nConnection: close\\r\\n\\r\\n"
            -- req = "OK"

            local bytes, err = sock:send(req)
            if not bytes then
                njt.say("failed to send request: ", err)
                return
            end

            njt.say("request sent: ", bytes)

            while true do
                print("calling receive")
                local line, err = sock:receive()
                if line then
                    njt.say("received: ", line)

                else
                    njt.say("failed to receive a line: ", err)
                    break
                end
            end

            ok, err = sock:close()
            njt.say("close: ", ok, " ", err)
        ';

        content_by_lua return;
    }
--- request
    GET /test
--- response_body
connected: 1
request sent: 57
received: HTTP/1.1 200 OK
received: Server: nginx
received: Content-Type: text/plain
received: Content-Length: 4
received: Connection: close
received: 
received: foo
failed to receive a line: closed
close: 1 nil
