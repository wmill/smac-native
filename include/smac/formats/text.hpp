#pragma once
#include <string>
#include <string_view>
namespace smac::formats {
std::string cp1252_to_utf8(std::string_view);
std::string normalize_text(std::string_view);
} // namespace smac::formats
