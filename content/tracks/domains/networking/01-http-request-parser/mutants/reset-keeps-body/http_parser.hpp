// MUTANT: reset() keeps the previous body
#pragma once
// A phase machine fed with slices of the input. Lines are accumulated in line_ (only the NEW bytes are searched for the LF); body bytes are
// appended directly; limits are checked as bytes arrive.
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
        std::size_t used = 0;
        while (used < data.size() && state_ == State::NeedMore) used += step(data.substr(used));
        return Feed{state_, used};
    }

    State state() const { return state_; }
    ErrorCode error() const { return error_; }
    const std::string& method() const { return method_; }
    const std::string& target() const { return target_; }
    int version_minor() const { return minor_; }
    const std::vector<Header>& headers() const { return headers_; }
    const std::string& body() const { return body_; }

    const std::string* header(std::string_view name) const {
        for (const Header& h : headers_) {
            if (iequals(h.name, name)) return &h.value;
        }
        return nullptr;
    }

    bool keep_alive() const {
        bool has_close = false, has_keep_alive = false;
        for (const Header& h : headers_) {
            if (!iequals(h.name, "connection")) continue;
            std::string_view rest = h.value;
            while (true) {
                const std::size_t comma = rest.find(',');
                const std::string_view token = trim(rest.substr(0, comma));
                if (iequals(token, "close")) has_close = true;
                if (iequals(token, "keep-alive")) has_keep_alive = true;
                if (comma == std::string_view::npos) break;
                rest.remove_prefix(comma + 1);
            }
        }
        if (minor_ == 1) return !has_close;
        return has_keep_alive && !has_close;
    }

    void reset() {
        state_ = State::NeedMore;
        error_ = ErrorCode::None;
        method_.clear();
        target_.clear();
        minor_ = 1;
        headers_.clear();
        phase_ = Phase::RequestLine;
        line_.clear();
        remaining_ = 0;
        header_lines_ = 0;
        total_chunked_ = 0;
        chunked_ = false;
    }

private:
    enum class Phase { RequestLine, Headers, FixedBody, ChunkSize, ChunkData, ChunkDataEnd, Trailers };

    static bool iequals(std::string_view a, std::string_view b) {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i) {
            if (lower(a[i]) != lower(b[i])) return false;
        }
        return true;
    }
    static char lower(char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }
    static std::string_view trim(std::string_view s) {
        while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
        while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
        return s;
    }
    static bool is_control(char c) { return static_cast<unsigned char>(c) < 0x20 || c == 0x7F; }
    static bool is_token_char(char c) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) return true;
        for (char t : std::string_view("!#$%&'*+-.^_`|~")) {
            if (c == t) return true;
        }
        return false;
    }

    std::size_t fail(ErrorCode code) {
        state_ = State::Error;
        error_ = code;
        return 1;  // the caller adds this to `used`; nothing is consumed after an error anyway
    }

    // Collects one line into line_ (without its CRLF). Returns the number of bytes taken; `done` tells whether a complete line is ready.
    // A complete line that does not end in CRLF, or has a CR elsewhere, sets `bad_ending`. A line that cannot fit in max_line (plus its CRLF)
    // sets too_long_ AS SOON AS that is certain: at most max_line + 2 bytes of any line are ever stored.
    std::size_t take_line(std::string_view data, bool& done, bool& bad_ending) {
        done = false;
        bad_ending = false;
        const std::size_t cap = limits_.max_line + 2;
        const std::size_t lf = data.find('\n');
        const std::size_t take = lf == std::string_view::npos ? data.size() : lf + 1;
        const std::size_t room = cap - line_.size();
        if (take > room) {
            line_.append(data.data(), room);
            too_long_ = true;
            return room;
        }
        line_.append(data.data(), take);
        if (lf != std::string_view::npos) {
            done = true;
            if (line_.size() < 2 || line_[line_.size() - 2] != '\r') {
                bad_ending = true;
            } else {
                line_.resize(line_.size() - 2);
                if (line_.find('\r') != std::string::npos || line_.find('\n') != std::string::npos) bad_ending = true;
            }
            if (!bad_ending && line_.size() > limits_.max_line) too_long_ = true;
        } else if (line_.size() >= cap) {
            too_long_ = true;
        }
        return take;
    }

    std::size_t step(std::string_view data) {
        switch (phase_) {
            case Phase::FixedBody: return take_body(data, Phase::FixedBody);
            case Phase::ChunkData: return take_body(data, Phase::ChunkData);
            default: break;
        }
        bool done = false, bad_ending = false;
        too_long_ = false;
        const std::size_t taken = take_line(data, done, bad_ending);
        if (too_long_) {
            fail(ErrorCode::HeaderTooLarge);
            return taken;
        }
        if (!done) return taken;
        const ErrorCode bad = phase_ == Phase::RequestLine ? ErrorCode::BadRequestLine : (phase_ == Phase::Headers || phase_ == Phase::Trailers) ? ErrorCode::BadHeader : ErrorCode::BadChunk;
        if (bad_ending) {
            fail(bad);
            return taken;
        }
        std::string line;
        line.swap(line_);
        switch (phase_) {
            case Phase::RequestLine: request_line(line); break;
            case Phase::Headers: header_line(line); break;
            case Phase::ChunkSize: chunk_size_line(line); break;
            case Phase::ChunkDataEnd:
                if (!line.empty()) fail(ErrorCode::BadChunk);
                else phase_ = Phase::ChunkSize;
                break;
            case Phase::Trailers: trailer_line(line); break;
            default: break;
        }
        return taken;
    }

    std::size_t take_body(std::string_view data, Phase phase) {
        const std::size_t n = std::min(data.size(), remaining_);
        body_.append(data.data(), n);
        remaining_ -= n;
        if (remaining_ == 0) {
            if (phase == Phase::FixedBody) state_ = State::Complete;
            else phase_ = Phase::ChunkDataEnd;
        }
        return n;
    }

    void request_line(const std::string& line) {
        const std::size_t sp1 = line.find(' ');
        const std::size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);
        if (sp1 == std::string::npos || sp2 == std::string::npos || sp1 == 0 || sp2 == sp1 + 1 || sp2 + 1 >= line.size()) {
            fail(ErrorCode::BadRequestLine);
            return;
        }
        const std::string method = line.substr(0, sp1);
        const std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
        const std::string version = line.substr(sp2 + 1);
        for (char c : method) {
            if (c < 'A' || c > 'Z') {
                fail(ErrorCode::BadMethod);
                return;
            }
        }
        if (!(target == "*" || target.front() == '/')) {
            fail(ErrorCode::BadTarget);
            return;
        }
        for (char c : target) {
            if (is_control(c) || c == ' ') {
                fail(ErrorCode::BadTarget);
                return;
            }
        }
        if (version != "HTTP/1.1" && version != "HTTP/1.0") {
            fail(ErrorCode::BadVersion);
            return;
        }
        method_ = method;
        target_ = target;
        minor_ = version == "HTTP/1.1" ? 1 : 0;
        phase_ = Phase::Headers;
    }

    // Validates "name: value"; returns false (after setting the error) when malformed.
    bool parse_field(const std::string& line, Header& out) {
        if (line[0] == ' ' || line[0] == '\t') return fail(ErrorCode::BadHeader), false;
        const std::size_t colon = line.find(':');
        if (colon == std::string::npos || colon == 0) return fail(ErrorCode::BadHeader), false;
        for (std::size_t i = 0; i < colon; ++i) {
            if (!is_token_char(line[i])) return fail(ErrorCode::BadHeader), false;
        }
        const std::string_view value = trim(std::string_view(line).substr(colon + 1));
        for (char c : value) {
            if (is_control(c) && c != '\t') return fail(ErrorCode::BadHeader), false;
        }
        out.name = line.substr(0, colon);
        out.value = std::string(value);
        return true;
    }

    void header_line(const std::string& line) {
        if (line.empty()) {
            end_of_headers();
            return;
        }
        Header h;
        if (!parse_field(line, h)) return;
        if (++header_lines_ > limits_.max_headers) {
            fail(ErrorCode::TooManyHeaders);
            return;
        }
        headers_.push_back(std::move(h));
    }

    void trailer_line(const std::string& line) {
        if (line.empty()) {
            state_ = State::Complete;
            return;
        }
        Header h;
        if (!parse_field(line, h)) return;
        if (++header_lines_ > limits_.max_headers) fail(ErrorCode::TooManyHeaders);
    }

    void end_of_headers() {
        const std::string* te = nullptr;
        bool have_cl = false;
        std::size_t length = 0;
        for (const Header& h : headers_) {
            if (iequals(h.name, "transfer-encoding")) {
                if (te != nullptr) {
                    fail(ErrorCode::UnsupportedTransferEncoding);   // two Transfer-Encoding headers: not "exactly chunked"
                    return;
                }
                te = &h.value;
            } else if (iequals(h.name, "content-length")) {
                std::size_t value = 0;
                if (!parse_length(h.value, value)) return;
                if (have_cl && value != length) {
                    fail(ErrorCode::BadContentLength);
                    return;
                }
                have_cl = true;
                length = value;
            }
        }
        if (te != nullptr && have_cl) {
            fail(ErrorCode::ConflictingFraming);
            return;
        }
        if (te != nullptr) {
            if (!iequals(*te, "chunked")) {
                fail(ErrorCode::UnsupportedTransferEncoding);
                return;
            }
            chunked_ = true;
            phase_ = Phase::ChunkSize;
            return;
        }
        if (have_cl && length > limits_.max_body) {
            fail(ErrorCode::BodyTooLarge);
            return;
        }
        if (have_cl && length > 0) {
            remaining_ = length;
            phase_ = Phase::FixedBody;
            return;
        }
        state_ = State::Complete;
    }

    bool parse_length(const std::string& text, std::size_t& out) {
        if (text.empty()) return fail(ErrorCode::BadContentLength), false;
        std::size_t value = 0;
        for (char c : text) {
            if (c < '0' || c > '9') return fail(ErrorCode::BadContentLength), false;
            const std::size_t digit = static_cast<std::size_t>(c - '0');
            if (value > (static_cast<std::size_t>(-1) - digit) / 10) return fail(ErrorCode::BadContentLength), false;
            value = value * 10 + digit;
        }
        out = value;
        return true;
    }

    void chunk_size_line(const std::string& line) {
        const std::size_t semicolon = line.find(';');
        const std::string_view digits = std::string_view(line).substr(0, semicolon);
        if (digits.empty() || digits.size() > 16) {
            fail(ErrorCode::BadChunk);
            return;
        }
        unsigned long long size = 0;
        for (char c : digits) {
            int v;
            if (c >= '0' && c <= '9') v = c - '0';
            else if (c >= 'a' && c <= 'f') v = c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v = c - 'A' + 10;
            else {
                fail(ErrorCode::BadChunk);
                return;
            }
            size = size * 16 + static_cast<unsigned long long>(v);
        }
        if (size == 0) {
            phase_ = Phase::Trailers;
            return;
        }
        if (size > limits_.max_body || total_chunked_ + size > limits_.max_body) {
            fail(ErrorCode::BodyTooLarge);
            return;
        }
        total_chunked_ += static_cast<std::size_t>(size);
        remaining_ = static_cast<std::size_t>(size);
        phase_ = Phase::ChunkData;
    }

    Limits limits_;
    State state_ = State::NeedMore;
    ErrorCode error_ = ErrorCode::None;
    std::string method_, target_, body_;
    int minor_ = 1;
    std::vector<Header> headers_;
    Phase phase_ = Phase::RequestLine;
    std::string line_;
    std::size_t remaining_ = 0;
    std::size_t header_lines_ = 0;
    std::size_t total_chunked_ = 0;
    bool chunked_ = false;
    bool too_long_ = false;
};

}  // namespace http
