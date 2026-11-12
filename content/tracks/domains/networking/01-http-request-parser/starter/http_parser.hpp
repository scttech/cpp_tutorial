#pragma once
// YOUR HTTP/1.1 REQUEST PARSER: incremental (bytes may arrive in any pieces, even one at a time), strict (it is the front door of a server),
// and bounded (hostile input cannot make it use unlimited memory or time).
//
//   RequestParser(limits = {})
//   Feed feed(std::string_view data)    consumes bytes belonging to THIS request and returns {state, consumed}.
//                                       It stops consuming at the end of the request (Complete): the remaining bytes are the next, pipelined request.
//                                       After Complete or Error, feed() consumes 0 bytes and returns the same state.
//   state(), error()
//   method(), target(), version_minor() (0 or 1), headers() (in order, duplicates kept), body(), header(name) (first match, case-insensitive) or nullptr
//   keep_alive()    HTTP/1.1: true unless a Connection header lists the token "close". HTTP/1.0: false unless it lists "keep-alive".
//                   Connection is a comma-separated list; tokens are trimmed and compared case-insensitively, whole-token only.
//   reset()         back to the initial state, ready for the next request
//
// THE RULES (see the lesson for the reasons)
//   Request line : METHOD SP target SP HTTP/1.0|HTTP/1.1 CRLF, single spaces, three non-empty parts.  Method: capital letters A-Z only.
//                  Target: starts with '/' or is exactly "*", no control characters (bytes < 0x20 or 0x7F) and no spaces.
//   Every line ends with CRLF. A lone LF, or a CR anywhere else in a line, is an error (BadRequestLine for the request line, BadHeader in the
//   header block, BadChunk in a chunked body).
//   Headers      : name ":" OWS value OWS. The name is one or more token characters (letters, digits and !#$%&'*+-.^_`|~): a space before the colon is an error.
//                  A line that starts with a space or a tab (an obsolete "folded" continuation) is an error. The value may contain tabs but no other control characters.
//                  Values are stored with the surrounding spaces and tabs removed.
//   Body framing : Transfer-Encoding and Content-Length both present: ConflictingFraming. Transfer-Encoding present: its (trimmed) value must be
//                  "chunked" in any case, otherwise UnsupportedTransferEncoding. Content-Length: every occurrence must be 1 or more digits only, must fit in
//                  size_t, and equal occurrences are fine but different ones are BadContentLength; a body longer than max_body is BodyTooLarge.
//                  Neither header: no body, the request is complete after the blank line that ends the headers.
//   Chunked body : { hex-size [";" extension] CRLF  data  CRLF }  "0" [";" extension] CRLF  { trailer-line CRLF }  CRLF
//                  The size has 1 to 16 hex digits (any case); the extension is ignored. Trailer lines are checked like headers and then dropped.
//                  The sum of the chunk sizes must stay within max_body (BodyTooLarge).
//   Limits       : see types.hpp. Check them AS BYTES ARRIVE: a 10 MB line with no CRLF must be rejected after max_line + a little bytes, not buffered.
#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "types.hpp"

namespace http {

class RequestParser {
public:
    struct Feed {
        State state;
        std::size_t consumed;
    };

    explicit RequestParser(Limits limits = {}) : limits_(limits) {}

    Feed feed(std::string_view data) {
        (void)data;
        return Feed{state_, 0};  // TODO
    }

    State state() const { return state_; }
    ErrorCode error() const { return error_; }
    const std::string& method() const { return method_; }
    const std::string& target() const { return target_; }
    int version_minor() const { return minor_; }
    const std::vector<Header>& headers() const { return headers_; }
    const std::string& body() const { return body_; }
    const std::string* header(std::string_view name) const {
        (void)name;
        return nullptr;  // TODO
    }
    bool keep_alive() const { return false; }  // TODO
    void reset() {}                             // TODO

private:
    Limits limits_;
    State state_ = State::NeedMore;
    ErrorCode error_ = ErrorCode::None;
    std::string method_, target_, body_;
    int minor_ = 1;
    std::vector<Header> headers_;
    // TODO: the phase, the partial line, the remaining body/chunk length, counters
};

}  // namespace http
