#pragma once
// READ-ONLY. The vocabulary of the HTTP request parser.
#include <cstddef>
#include <string>

namespace http {

enum class State {
    NeedMore,  // the request is not complete yet: feed more bytes
    Complete,  // one whole request has been parsed (the rest of the input, if any, belongs to the NEXT request)
    Error,     // the input is not a valid request; error() says why. The parser stays in this state until reset().
};

enum class ErrorCode {
    None,
    BadRequestLine,              // not exactly "METHOD SP target SP version CRLF"; or a line ended with a bare LF instead of CRLF
    BadMethod,                   // the method is empty or not made of the capital letters A-Z
    BadTarget,                   // the target does not start with '/' (and is not the single character '*') or contains a control character
    BadVersion,                  // anything but HTTP/1.0 and HTTP/1.1
    BadHeader,                   // no colon, empty name, space in the name (also "Name : value"), a continuation line, a control character
    HeaderTooLarge,              // a line (request line, header or chunk-size line) is longer than Limits::max_line
    TooManyHeaders,              // more than Limits::max_headers header lines (the trailers of a chunked body count too)
    BadContentLength,            // not 1 or more decimal digits; does not fit in size_t; or two Content-Length headers with different values
    BodyTooLarge,                // the body would be longer than Limits::max_body
    BadChunk,                    // a malformed chunk: bad size line, data not followed by CRLF
    ConflictingFraming,          // both Content-Length and Transfer-Encoding: the classic request-smuggling setup
    UnsupportedTransferEncoding, // Transfer-Encoding is present but is not exactly "chunked" (any case)
};

struct Limits {
    std::size_t max_line = 8192;   // longest line, not counting its CRLF
    std::size_t max_headers = 100;
    std::size_t max_body = 1 << 20;
};

struct Header {
    std::string name;   // exactly as received
    std::string value;  // without the spaces and tabs around it
};

}  // namespace http
