// Reading a configuration file, and what a good error message looks like. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "config.hpp"

int main() {
    const std::string good =
        "# server settings\n"
        "name = \"demo server\"      # a comment after a value\n"
        "debug = false\n"
        "\n"
        "[server]\n"
        "port = 8_080\n"
        "timeout = 2.5e1\n"
        "hosts = [\"a.example\", 'b.example',\n"
        "         \"c.example\"]   # arrays may span lines\n"
        "\n"
        "[server.tls]\n"
        "cert = 'C:\\certs\\main.pem'\n"
        "ciphers.min = 0x0303\n";
    const conf::ParseResult ok = conf::parse(good);
    if (ok.config) {
        for (const auto& e : ok.config->entries()) {
            std::printf("%-20s = ", e.path.c_str());
            switch (e.value.type) {
                case conf::Value::Type::String: std::printf("\"%s\"\n", e.value.s.c_str()); break;
                case conf::Value::Type::Int: std::printf("%lld\n", e.value.i); break;
                case conf::Value::Type::Float: std::printf("%g\n", e.value.d); break;
                case conf::Value::Type::Bool: std::printf("%s\n", e.value.b ? "true" : "false"); break;
                case conf::Value::Type::Array: std::printf("[%zu items]\n", e.value.a.size()); break;
            }
        }
    }

    const char* broken[] = {
        "name = \"unterminated\nport = 1\n",
        "[server]\nport = 80x\n",
        "a = 1\nb = 2\na = 3\n",
        "list = [1, 2.5]\n",
        "path = \"C:\\data\"\n",
        "[server\nx = 1\n",
        "value = \n",
        "x = 1 2\n",
    };
    for (const char* text : broken) {
        const conf::ParseResult r = conf::parse(text);
        if (r.error) std::printf("\n%s", conf::render(text, *r.error, "app.conf").c_str());
    }
}
