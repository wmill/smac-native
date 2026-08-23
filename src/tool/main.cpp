#include "smac/formats/data_directory.hpp"
#include "smac/formats/pcx.hpp"
#include "smac/formats/rules.hpp"
#include "smac/formats/terran_map.hpp"

#include <algorithm>
#include <array>
#include <iostream>
#include <variant>
namespace sf = smac::formats;
static int usage() {
    std::cerr << "usage:\n  smac-tool verify-data --data-dir DIR\n  smac-tool dump-rules "
                 "--data-dir DIR\n  smac-tool inspect-map FILE\n  smac-tool inspect-pcx FILE\n";
    return 2;
}
static const char* value(int argc, char** argv, const std::string& key) {
    for (int i = 2; i + 1 < argc; ++i)
        if (argv[i] == key)
            return argv[i + 1];
    return nullptr;
}
int main(int argc, char** argv) {
    if (argc < 2)
        return usage();
    std::string cmd = argv[1];
    if (cmd == "verify-data") {
        auto dir = value(argc, argv, "--data-dir");
        if (!dir)
            return usage();
        auto r = sf::validate_data_directory(dir);
        for (const auto& c : r.checks)
            std::cout << (c.path       ? "OK   "
                          : c.required ? "ERROR"
                                       : "WARN ")
                      << " " << c.logical_name << ": " << c.detail
                      << (c.path ? " [" + c.path->string() + "]" : "") << '\n';
        return r.valid() ? 0 : 1;
    }
    if (cmd == "dump-rules") {
        auto dir = value(argc, argv, "--data-dir");
        if (!dir)
            return usage();
        auto p = sf::find_case_insensitive(dir, "alphax.txt");
        if (!p) {
            std::cerr << "alphax.txt not found\n";
            return 1;
        }
        auto r = sf::load_rules(*p);
        if (auto e = std::get_if<sf::Error>(&r)) {
            std::cerr << e->message << " at " << e->offset << '\n';
            return 1;
        }
        auto& rules = std::get<sf::ParsedRules>(r);
        std::cout << "road_movement_rate: " << rules.database.road_movement_rate << '\n';
        for (auto& [name, lines] : rules.sections)
            std::cout << name << ": " << lines.size() << " records\n";
        return 0;
    }
    if (cmd == "inspect-map" && argc == 3) {
        auto r = sf::load_terran_map(argv[2]);
        if (auto e = std::get_if<sf::Error>(&r)) {
            std::cerr << e->message << " at byte " << e->offset << '\n';
            return 1;
        }
        auto& m = std::get<sf::TerranMap>(r);
        std::size_t land = 0;
        for (auto& t : m.tiles)
            if ((t.climate & 0xE0U) >= 0x60U)
                ++land;
        std::cout << "TERRANMAP " << m.width << 'x' << m.height << " (" << m.tiles.size()
                  << " tiles)\nseed: " << m.seed << "\nwraps: " << (m.flat ? "no" : "yes")
                  << "\nlandmarks: " << m.landmarks.size() << "\nland tiles: " << land
                  << "\nabstract bytes: " << m.abstract_regions.size() << '\n';
        for (auto& l : m.landmarks)
            std::cout << "  (" << l.x << ',' << l.y << ") " << l.name << '\n';
        return 0;
    }
    if (cmd == "inspect-pcx" && argc == 3) {
        auto result = sf::load_pcx(argv[2]);
        if (auto* error = std::get_if<sf::Error>(&result)) {
            std::cerr << error->message << " at byte " << error->offset << '\n';
            return 1;
        }
        const auto& image = std::get<sf::IndexedImage>(result);
        std::array<std::size_t, 256> counts{};
        for (const auto pixel : image.pixels)
            ++counts[pixel];
        std::size_t colors = 0;
        for (const auto count : counts)
            colors += count != 0;
        const auto background = static_cast<std::size_t>(
            std::distance(counts.begin(), std::max_element(counts.begin(), counts.end())));
        const auto& color = image.palette[background];
        std::cout << "PCX " << image.width << 'x' << image.height << " (" << colors
                  << " used palette indices)\nmost-common index: " << background << " ("
                  << static_cast<int>(color.red) << ',' << static_cast<int>(color.green) << ','
                  << static_cast<int>(color.blue) << ", " << counts[background]
                  << " pixels)\nindex-255 pixels: " << counts[255] << '\n';
        return 0;
    }
    return usage();
}
