#pragma once

#include "aethon/storage/query.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace aethon::storage {

struct QueryToken {
    std::string key;
    std::string value;
};

struct QueryParseDiagnostic {
    std::string token;
    std::string message;
};

struct ParsedArchiveQuery {
    ArchiveQuery query;
    std::vector<QueryToken> tokens;
    std::vector<QueryParseDiagnostic> diagnostics;

    [[nodiscard]] bool ok() const noexcept {
        return diagnostics.empty();
    }
};

[[nodiscard]] std::vector<QueryToken> tokenize_query(std::string_view expression);
[[nodiscard]] ParsedArchiveQuery parse_archive_query(std::string_view expression);
[[nodiscard]] std::string render_query_diagnostics(const ParsedArchiveQuery& parsed);

} // namespace aethon::storage
