#pragma once
#include <filesystem>
#include <optional>
#include <string>
namespace smac::formats {
std::optional<std::string> sha256_file(const std::filesystem::path&);
}
