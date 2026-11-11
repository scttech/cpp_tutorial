// A FIX-style order message (the separator is written as | here, and replaced by the real SOH character). Read-only. Press Run.
#include <cstdio>
#include <string>

#include "fix.hpp"

static std::string with_soh(std::string s) {
    for (char& c : s) {
        if (c == '|') c = fix::kSoh;
    }
    return s;
}

int main() {
    // 8=FIX.4.4|9=<length of the body>|35=D|49=ACME|56=EXCH|11=ORD1|55=MSFT|54=1|38=100|44=250.50|10=<checksum>|
    std::string body = with_soh("35=D|49=ACME|56=EXCH|11=ORD1|55=MSFT|54=1|38=100|44=250.50|");
    std::string head = "8=FIX.4.4" + std::string(1, fix::kSoh) + "9=" + std::to_string(body.size()) + std::string(1, fix::kSoh);
    std::string text = head + body;
    char checksum[8];
    std::snprintf(checksum, sizeof checksum, "%03d", fix::checksum_of(text));
    text += "10=" + std::string(checksum) + std::string(1, fix::kSoh);

    const fix::ParseResult r = fix::parse(text);
    if (!r.ok()) {
        std::printf("error %s at %zu\n", fix::name(r.error), r.pos);
        return 1;
    }
    for (const fix::Field& f : r.fields) std::printf("tag %3d = %.*s\n", f.tag, static_cast<int>(f.value.size()), f.value.data());
    std::printf("the value of tag 55 starts at byte %td of the message text\n", fix::find(r, 55)->value.data() - text.data());

    std::string broken = text;
    broken[broken.size() - 5] = '7';  // damage a byte of the checksum
    std::printf("damaged checksum: %s\n", fix::name(fix::parse(broken).error));
}
