// Feeding an HTTP request to the parser in pieces, the way a socket delivers it. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "http_parser.hpp"
#include "types.hpp"

int main() {
    const std::string wire =
        "POST /upload?id=7 HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "content-type: text/plain\r\n"
        "Transfer-Encoding: chunked\r\n"
        "\r\n"
        "5\r\nhello\r\n"
        "7\r\n, world\r\n"
        "0\r\n\r\n"
        "GET /next HTTP/1.1\r\n\r\n";   // a second (pipelined) request follows
    http::RequestParser parser;
    std::size_t pos = 0;
    while (parser.state() == http::State::NeedMore && pos < wire.size()) {
        const std::size_t n = std::min<std::size_t>(7, wire.size() - pos);   // the "network" delivers 7 bytes at a time
        const http::RequestParser::Feed f = parser.feed(std::string_view(wire).substr(pos, n));
        pos += f.consumed;
        std::printf("fed %zu bytes -> consumed %zu, %s\n", n, f.consumed, f.state == http::State::NeedMore ? "need more" : f.state == http::State::Complete ? "complete" : "error");
    }
    std::printf("%s %s HTTP/1.%d, body \"%s\", %zu headers, keep-alive: %s\n", parser.method().c_str(), parser.target().c_str(), parser.version_minor(), parser.body().c_str(), parser.headers().size(),
                parser.keep_alive() ? "yes" : "no");
    const std::string* type = parser.header("Content-Type");
    std::printf("Content-Type (any capitalisation): %s\n", type ? type->c_str() : "(none)");
    std::printf("%zu bytes of the next request are still unread\n", wire.size() - pos);
}
