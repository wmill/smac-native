#pragma once

#include "smac/formats/result.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace smac::formats {
struct PaletteColor {
    std::uint8_t red{};
    std::uint8_t green{};
    std::uint8_t blue{};
    std::uint8_t alpha{255};
    friend bool operator==(const PaletteColor&, const PaletteColor&) = default;
};

struct IndexedImage {
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<std::uint8_t> pixels;
    std::array<PaletteColor, 256> palette{};

    [[nodiscard]] std::uint8_t at(std::uint32_t x, std::uint32_t y) const;
};

Result<IndexedImage> parse_pcx(std::span<const std::byte> bytes);
Result<IndexedImage> load_pcx(const std::filesystem::path& path);
} // namespace smac::formats
