// Build a DNS response, serialize it with name compression, parse it back, and survive a malicious packet. Read-only. Press Run.
#include <cstdio>

#include "dns.hpp"
#include "types.hpp"

int main() {
    dns::Message m;
    m.id = 0x1234;
    m.flags = 0x8180;
    m.questions.push_back({"www.example.com", 1, 1});
    m.answers.push_back({"www.example.com", 5, 1, 300, *dns::encode_name("web.example.net")});
    m.answers.push_back({"web.example.net", 1, 1, 60, {192, 0, 2, 1}});
    m.authorities.push_back({"example.net", 2, 1, 86400, *dns::encode_name("ns1.example.net")});

    const auto wire = dns::serialize(m);
    if (!wire) return 1;
    std::printf("%zu bytes on the wire:", wire->size());
    for (std::size_t i = 0; i < wire->size(); ++i) std::printf("%s%02x", i % 16 == 0 ? "\n  " : " ", (*wire)[i]);
    std::printf("\n(pointers are the bytes starting with c0)\n");

    const auto back = dns::parse(*wire);
    std::printf("parsed back: %s; %zu question(s), %zu answer(s), CNAME target length %zu bytes\n", back && *back == m ? "identical" : "DIFFERENT", back ? back->questions.size() : 0, back ? back->answers.size() : 0,
                back ? back->answers[0].rdata.size() : 0);

    // a hostile packet: the question's name is a compression pointer that points at itself
    const std::vector<std::uint8_t> evil = {0, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0xC0, 0x0C, 0, 1, 0, 1};
    std::printf("the self-referencing packet is %s\n", dns::parse(evil) ? "ACCEPTED (an infinite loop in a naive parser)" : "rejected");
}
