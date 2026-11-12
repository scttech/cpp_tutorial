#pragma once
// READ-ONLY. A parsed absolute URL of the hierarchical kind  scheme://[userinfo@]host[:port]/path[?query][#fragment].
#include <cstdint>
#include <optional>
#include <string>

namespace urlkit {

struct Url {
    std::string scheme;                    // lower case, no "://"
    std::string userinfo;                  // empty if there is none ("user:pass"), no '@'
    std::string host;                      // lower case; an IPv6 literal keeps its brackets: "[::1]"
    std::optional<std::uint16_t> port;     // nullopt if absent (or written empty as in  http://a:/ )
    std::string path;                      // may be empty; no '?' or '#'
    std::optional<std::string> query;      // without the '?'; nullopt if there is no '?', "" for a '?' with nothing after it
    std::optional<std::string> fragment;   // without the '#'; same distinction
};

}  // namespace urlkit
