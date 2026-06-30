#pragma once

#include <string>
#include <vector>

namespace aethon::diagnostics {

struct TextTable {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
};

void add_row(TextTable& table, std::vector<std::string> row);
[[nodiscard]] std::string render_text_table(const TextTable& table);

} // namespace aethon::diagnostics
