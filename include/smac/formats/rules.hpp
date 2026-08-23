#pragma once
#include "smac/core/types.hpp"
#include "smac/formats/result.hpp"

#include <filesystem>
#include <map>
namespace smac::formats {
struct RuleLine {
    std::vector<std::string> fields;
    std::size_t source_line{};
};
struct ParsedRules {
    smac::core::RulesDatabase database;
    std::map<std::string, std::vector<RuleLine>, std::less<>> sections;
};
Result<ParsedRules> parse_rules(std::string_view);
Result<ParsedRules> load_rules(const std::filesystem::path&);
} // namespace smac::formats
