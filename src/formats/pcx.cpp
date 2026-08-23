#include "smac/formats/pcx.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace smac::formats {
namespace {
constexpr std::size_t header_size = 128;
constexpr std::size_t palette_size = 769;
constexpr std::size_t max_pcx_file_bytes = 32 * 1024 * 1024;
constexpr std::uint32_t max_dimension = 4096;

std::uint16_t u16(std::span<const std::byte> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
           static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1]) << 8U);
}
} // namespace

std::uint8_t IndexedImage::at(std::uint32_t x, std::uint32_t y) const {
    if (x >= width || y >= height)
        throw std::out_of_range("indexed image coordinate is out of range");
    return pixels[static_cast<std::size_t>(y) * width + x];
}

Result<IndexedImage> parse_pcx(std::span<const std::byte> bytes) {
    if (bytes.size() > max_pcx_file_bytes)
        return Error{"PCX data exceeds size limit", max_pcx_file_bytes};
    if (bytes.size() < header_size + palette_size)
        return Error{"truncated PCX file", bytes.size()};
    if (bytes[0] != std::byte{0x0A} || bytes[2] != std::byte{1} || bytes[3] != std::byte{8})
        return Error{"unsupported PCX header", 0};
    const auto x_min = u16(bytes, 4);
    const auto y_min = u16(bytes, 6);
    const auto x_max = u16(bytes, 8);
    const auto y_max = u16(bytes, 10);
    if (x_max < x_min || y_max < y_min)
        return Error{"invalid PCX bounds", 4};
    const auto width = static_cast<std::uint32_t>(x_max - x_min) + 1;
    const auto height = static_cast<std::uint32_t>(y_max - y_min) + 1;
    const auto planes = std::to_integer<std::uint8_t>(bytes[65]);
    const auto bytes_per_line = u16(bytes, 66);
    if (width == 0 || height == 0 || width > max_dimension || height > max_dimension ||
        planes != 1 || bytes_per_line < width || bytes_per_line > max_dimension)
        return Error{"unsupported PCX dimensions or plane layout", 65};

    const auto palette_offset = bytes.size() - palette_size;
    if (bytes[palette_offset] != std::byte{0x0C})
        return Error{"missing PCX 256-color palette", palette_offset};
    const auto decoded_size = static_cast<std::size_t>(bytes_per_line) * height;
    std::vector<std::uint8_t> decoded;
    decoded.reserve(decoded_size);
    std::size_t source = header_size;
    while (decoded.size() < decoded_size && source < palette_offset) {
        auto value = std::to_integer<std::uint8_t>(bytes[source++]);
        std::size_t count = 1;
        if ((value & 0xC0U) == 0xC0U) {
            count = value & 0x3FU;
            if (count == 0 || source >= palette_offset)
                return Error{"invalid PCX RLE run", source - 1};
            value = std::to_integer<std::uint8_t>(bytes[source++]);
        }
        if (count > decoded_size - decoded.size())
            return Error{"PCX RLE run exceeds image", source - 1};
        decoded.insert(decoded.end(), count, value);
    }
    if (decoded.size() != decoded_size)
        return Error{"truncated PCX pixel data", source};

    IndexedImage image;
    image.width = width;
    image.height = height;
    image.pixels.resize(static_cast<std::size_t>(width) * height);
    for (std::uint32_t y = 0; y < height; ++y) {
        const auto row = static_cast<std::size_t>(y) * bytes_per_line;
        std::copy_n(decoded.begin() + static_cast<std::ptrdiff_t>(row), width,
                    image.pixels.begin() + static_cast<std::ptrdiff_t>(y * width));
    }
    for (std::size_t i = 0; i < image.palette.size(); ++i) {
        const auto offset = palette_offset + 1 + i * 3;
        image.palette[i] = {std::to_integer<std::uint8_t>(bytes[offset]),
                            std::to_integer<std::uint8_t>(bytes[offset + 1]),
                            std::to_integer<std::uint8_t>(bytes[offset + 2]), 255};
    }
    return image;
}

Result<IndexedImage> load_pcx(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input)
        return Error{"cannot open PCX file", 0};
    const auto size = input.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(max_pcx_file_bytes))
        return Error{"PCX file size is invalid", 0};
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), size);
    if (!input)
        return Error{"failed reading PCX file", 0};
    return parse_pcx(bytes);
}
} // namespace smac::formats
