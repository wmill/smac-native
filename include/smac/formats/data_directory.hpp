#pragma once
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
namespace smac::formats {
struct DataCheck {
    std::string logical_name;
    std::optional<std::filesystem::path> path;
    bool required{};
    std::string detail;
};
struct ValidationReport {
    std::filesystem::path root;
    std::vector<DataCheck> checks;
    [[nodiscard]] bool valid() const;
};
std::optional<std::filesystem::path> find_case_insensitive(const std::filesystem::path&,
                                                           const std::filesystem::path&);
ValidationReport validate_data_directory(const std::filesystem::path&);
} // namespace smac::formats
