#include "smac/formats/terran_map.hpp"

#include "smac/formats/text.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <limits>
namespace smac::formats {
static constexpr std::size_t max_map_bytes = 64 * 1024 * 1024;

static std::uint32_t u32(std::span<const std::byte> b, std::size_t o) {
    return static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[o])) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[o + 1])) << 8U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[o + 2])) << 16U) |
           (static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(b[o + 3])) << 24U);
}
Result<TerranMap> parse_terran_map(std::span<const std::byte> b) {
    constexpr std::size_t envelope = 15, legacy = 2724, tile_size = 44;
    constexpr char magic[] = "TERRANMAP";
    if (b.size() > max_map_bytes)
        return Error{"TERRANMAP data exceeds size limit", max_map_bytes};
    if (b.size() < envelope + legacy)
        return Error{"truncated TERRANMAP header", b.size()};
    for (std::size_t i = 0; i < 9; ++i)
        if (std::to_integer<char>(b[i]) != magic[i])
            return Error{"invalid TERRANMAP signature", i};
    TerranMap m;
    m.width = static_cast<std::int32_t>(u32(b, 15));
    m.height = static_cast<std::int32_t>(u32(b, 19));
    m.seed = u32(b, 23);
    if (m.width <= 0 || m.height <= 0 || (m.width & 1) || m.width > 2048 || m.height > 2048)
        return Error{"implausible map dimensions", 15};
    std::copy_n(b.begin() + envelope, legacy, m.legacy_header.begin());
    m.flat = u32(b, 31) != 0;
    auto lm_count = static_cast<std::int32_t>(u32(b, 47));
    if (lm_count < 0 || lm_count > 64)
        return Error{"invalid landmark count", 47};
    constexpr std::size_t landmark_base = 51;
    for (int i = 0; i < lm_count; ++i) {
        auto o = landmark_base + static_cast<std::size_t>(i) * 40;
        if (o + 40 > envelope + legacy)
            return Error{"landmark exceeds header", o};
        MapLandmark lm;
        lm.x = static_cast<std::int32_t>(u32(b, o));
        lm.y = static_cast<std::int32_t>(u32(b, o + 4));
        std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(o + 8), 32, lm.raw_name.begin());
        std::string raw;
        for (auto c : lm.raw_name) {
            char ch = std::to_integer<char>(c);
            if (ch == 0)
                break;
            raw.push_back(ch);
        }
        lm.name = cp1252_to_utf8(raw);
        m.landmarks.push_back(std::move(lm));
    }
    auto area = static_cast<std::uint64_t>(m.width / 2) * static_cast<std::uint64_t>(m.height);
    if (area > std::numeric_limits<std::size_t>::max() / tile_size)
        return Error{"map allocation overflow", 15};
    auto tile_bytes = static_cast<std::size_t>(area) * tile_size;
    auto pos = envelope + legacy;
    if (b.size() < pos + tile_bytes)
        return Error{"truncated tile records", b.size()};
    m.tiles.reserve(static_cast<std::size_t>(area));
    for (std::size_t i = 0; i < area; ++i) {
        auto o = pos + i * tile_size;
        RawMapTile t;
        std::copy_n(b.begin() + static_cast<std::ptrdiff_t>(o), tile_size, t.raw.begin());
        t.climate = std::to_integer<std::uint8_t>(b[o]);
        t.contour = std::to_integer<std::uint8_t>(b[o + 1]);
        t.site_owner = std::to_integer<std::uint8_t>(b[o + 2]);
        t.region = std::to_integer<std::uint8_t>(b[o + 3]);
        t.visibility = std::to_integer<std::uint8_t>(b[o + 4]);
        t.rock_lock_user = std::to_integer<std::uint8_t>(b[o + 5]);
        t.unknown1 = std::to_integer<std::uint8_t>(b[o + 6]);
        t.territory = static_cast<std::int8_t>(std::to_integer<std::uint8_t>(b[o + 7]));
        t.improvements = u32(b, o + 8);
        t.landmark = u32(b, o + 12);
        for (std::size_t j = 0; j < 7; ++j)
            t.visible_improvements[j] = u32(b, o + 16 + j * 4);
        m.tiles.push_back(t);
    }
    pos += tile_bytes;
    m.abstract_regions.assign(b.begin() + static_cast<std::ptrdiff_t>(pos), b.end());
    return m;
}
Result<TerranMap> load_terran_map(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary | std::ios::ate);
    if (!f)
        return Error{"cannot open map", 0};
    auto size = f.tellg();
    if (size < 0 || size > static_cast<std::streamoff>(max_map_bytes))
        return Error{"map file size is invalid", 0};
    std::vector<std::byte> b(static_cast<std::size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(b.data()), size);
    if (!f)
        return Error{"failed reading map", 0};
    return parse_terran_map(b);
}
smac::core::WorldMap TerranMap::to_world_map() const {
    smac::core::WorldMap out(width, height, !flat);
    for (std::size_t i = 0; i < tiles.size(); ++i) {
        auto& d = out.tiles()[i];
        auto& s = tiles[i];
        d.climate = s.climate;
        d.contour = s.contour;
        d.region = s.region;
        d.improvements = s.improvements;
        d.terrain =
            (s.climate & 0xE0U) < 0x60U ? smac::core::Terrain::ocean : smac::core::Terrain::land;
    }
    return out;
}
} // namespace smac::formats
