#include "smac/formats/rules.hpp"

#include "smac/formats/text.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
namespace smac::formats {
static constexpr std::size_t max_rules_bytes = 16 * 1024 * 1024;
static constexpr std::size_t max_rule_line_bytes = 64 * 1024;
static constexpr std::size_t max_rule_records = 250'000;
static constexpr std::size_t max_rule_sections = 4096;

static std::string trim(std::string_view v) {
    auto a = v.find_first_not_of(" \t");
    if (a == v.npos)
        return {};
    auto b = v.find_last_not_of(" \t");
    return std::string(v.substr(a, b - a + 1));
}
static bool section_name(std::string_view s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](unsigned char c) {
        return std::isupper(c) != 0 || std::isdigit(c) != 0 || c == '_';
    });
}
Result<ParsedRules> parse_rules(std::string_view raw) {
    if (raw.size() > max_rules_bytes)
        return Error{"rules data exceeds size limit", max_rules_bytes};
    ParsedRules out;
    std::istringstream input(normalize_text(raw));
    std::string line, section;
    std::size_t number = 0;
    std::size_t record_count = 0;
    while (std::getline(input, line)) {
        ++number;
        if (line.size() > max_rule_line_bytes)
            return Error{"rules line exceeds size limit", number};
        auto semicolon = line.find(';');
        if (semicolon != line.npos)
            line.erase(semicolon);
        line = trim(line);
        if (line.empty())
            continue;
        if (line.front() == '#') {
            auto candidate = trim(std::string_view(line).substr(1));
            if (!section_name(candidate)) {
                section.clear();
                continue;
            }
            section = std::move(candidate);
            out.database.section_names.push_back(section);
            if (out.database.section_names.size() > max_rule_sections)
                return Error{"rules data has too many sections", number};
            out.sections.try_emplace(section);
            continue;
        }
        if (section.empty())
            continue;
        RuleLine item{{}, number};
        std::size_t start = 0;
        do {
            auto comma = line.find(',', start);
            auto end = comma == line.npos ? line.size() : comma;
            item.fields.push_back(trim(std::string_view(line).substr(start, end - start)));
            if (comma == line.npos)
                break;
            start = comma + 1;
        } while (start <= line.size());
        out.sections[section].push_back(std::move(item));
        if (++record_count > max_rule_records)
            return Error{"rules data has too many records", number};
    }
    auto rules = out.sections.find("RULES");
    if (rules != out.sections.end() && !rules->second.empty() &&
        !rules->second.front().fields.empty()) {
        int value{};
        auto& s = rules->second.front().fields.front();
        auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
        if (ec != std::errc{} || p != s.data() + s.size() || value <= 0)
            return Error{"invalid road movement rate", rules->second.front().source_line};
        out.database.road_movement_rate = value;
    }
    return out;
}
Result<ParsedRules> load_rules(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        return Error{"cannot open rules file", 0};
    const auto size = f.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(max_rules_bytes))
        return Error{"rules file size is invalid", 0};
    std::string contents(static_cast<std::size_t>(size), '\0');
    f.seekg(0);
    f.read(contents.data(), size);
    if (!f)
        return Error{"failed reading rules file", 0};
    return parse_rules(contents);
}
} // namespace smac::formats
