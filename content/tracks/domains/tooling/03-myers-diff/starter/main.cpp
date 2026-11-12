// Comparing two versions of a file, and applying the difference. Read-only. Press Run.
#include <cstdio>
#include <string>

#include "diff.hpp"

int main() {
    const std::string old_text =
        "int main() {\n"
        "    int total = 0;\n"
        "    for (int i = 0; i < 10; ++i)\n"
        "        total += i;\n"
        "    printf(\"%d\\n\", total);\n"
        "    return 0;\n"
        "}\n";
    const std::string new_text =
        "#include <cstdio>\n"
        "\n"
        "int main() {\n"
        "    int total = 0;\n"
        "    for (int i = 0; i < 100; ++i)\n"
        "        total += i;\n"
        "    std::printf(\"%d\\n\", total);\n"
        "    return 0;\n"
        "}\n";
    const difftool::Lines a = difftool::split_lines(old_text), b = difftool::split_lines(new_text);
    const auto edits = difftool::diff(a, b);
    std::size_t del = 0, ins = 0, same = 0;
    for (const auto& e : edits) {
        if (e.op == difftool::Op::Delete) ++del;
        else if (e.op == difftool::Op::Insert) ++ins;
        else ++same;
    }
    std::printf("%zu lines unchanged, %zu deleted, %zu inserted (distance %zu)\n\n", same, del, ins, difftool::distance(a, b));
    const std::string patch = difftool::unified(a, b, edits, "main.cpp.orig", "main.cpp", 1);
    std::printf("%s\n", patch.c_str());

    const auto patched = difftool::apply_patch(a, patch);
    std::printf("applying the patch to the old text %s the new text\n", patched && difftool::join_lines(*patched) == new_text ? "gives exactly" : "does NOT give");
    const auto wrong = difftool::apply_patch(difftool::split_lines("something else entirely\n"), patch);
    std::printf("applying it to an unrelated file %s\n", wrong ? "succeeded?!" : "is refused");
}
