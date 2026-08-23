#include "smac/formats/data_directory.hpp"

#include "smac/formats/sha256.hpp"

#include <algorithm>
#include <cctype>
namespace smac::formats {
static std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}
std::optional<std::filesystem::path> find_case_insensitive(const std::filesystem::path& root,
                                                           const std::filesystem::path& relative) {
    auto cur = root;
    for (const auto& component : relative) {
        if (!std::filesystem::is_directory(cur))
            return std::nullopt;
        auto wanted = lower(component.string());
        std::optional<std::filesystem::path> found;
        std::error_code ec;
        for (const auto& e : std::filesystem::directory_iterator(cur, ec)) {
            if (lower(e.path().filename().string()) == wanted) {
                found = e.path();
                break;
            }
        }
        if (!found)
            return std::nullopt;
        cur = *found;
    }
    return cur;
}
bool ValidationReport::valid() const {
    return std::all_of(checks.begin(), checks.end(),
                       [](const DataCheck& c) { return !c.required || c.path.has_value(); });
}
ValidationReport validate_data_directory(const std::filesystem::path& root) {
    ValidationReport r{root, {}};
    auto add = [&](std::string name, std::filesystem::path p, bool req) {
        auto found = find_case_insensitive(root, p);
        r.checks.push_back({std::move(name), found, req, found ? "found" : "missing"});
    };
    add("SMACX rules", "alphax.txt", true);
    for (auto f : {"gaians.txt", "hive.txt", "spartans.txt", "believe.txt", "peace.txt",
                   "morgan.txt", "univ.txt"})
        add(std::string("faction ") + f, f, true);
    add("terrain atlas", "ter1.pcx", true);
    add("terrain texture atlas", "texture.pcx", true);
    add("unit atlas", "Units.pcx", true);
    add("original display font", "ALPHC___.TTF", false);
    add("Arial Narrow font", "arialn.ttf", false);
    add("Planet map", "maps/xplanet.MP", true);
    add("SMACX executable", "terranx.exe", true);
    auto& exe = r.checks.back();
    if (exe.path) {
        auto digest = sha256_file(*exe.path);
        if (digest)
            exe.detail =
                *digest == "01901cbf7196b0c5d0df9540a029520f5df8fd9a6b343deef8b5663872805fcf"
                    ? "known GOG SMACX 2.0 identity"
                    : "present; SHA-256 " + *digest + " (not baseline)";
    }
    return r;
}
} // namespace smac::formats
