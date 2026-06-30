#include "aethon/diagnostics/text_table.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace aethon::diagnostics {
namespace {

std::vector<std::size_t> column_widths(const TextTable& table) {
    std::vector<std::size_t> widths(table.headers.size(), 0);
    for (std::size_t i = 0; i < table.headers.size(); ++i) {
        widths[i] = table.headers[i].size();
    }
    for (const auto& row : table.rows) {
        if (row.size() > widths.size()) {
            widths.resize(row.size(), 0);
        }
        for (std::size_t i = 0; i < row.size(); ++i) {
            widths[i] = std::max(widths[i], row[i].size());
        }
    }
    return widths;
}

void render_separator(std::ostringstream& out, const std::vector<std::size_t>& widths) {
    for (auto width : widths) {
        out << "+-";
        for (std::size_t i = 0; i < width; ++i) {
            out << "-";
        }
        out << "-";
    }
    out << "+\n";
}

void render_cells(std::ostringstream& out,
                  const std::vector<std::string>& cells,
                  const std::vector<std::size_t>& widths) {
    for (std::size_t i = 0; i < widths.size(); ++i) {
        auto value = i < cells.size() ? cells[i] : std::string{};
        out << "| "
            << value;
        for (std::size_t pad = value.size(); pad < widths[i]; ++pad) {
            out << " ";
        }
        out << " ";
    }
    out << "|\n";
}

} // namespace

void add_row(TextTable& table, std::vector<std::string> row) {
    table.rows.push_back(std::move(row));
}

std::string render_text_table(const TextTable& table) {
    auto widths = column_widths(table);
    std::ostringstream out;
    render_separator(out, widths);
    render_cells(out, table.headers, widths);
    render_separator(out, widths);
    for (const auto& row : table.rows) {
        render_cells(out, row, widths);
    }
    render_separator(out, widths);
    return out.str();
}

} // namespace aethon::diagnostics
