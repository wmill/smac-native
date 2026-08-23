#include "smac/formats/terran_map.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto bytes = std::span(reinterpret_cast<const std::byte*>(data), size);
    (void)smac::formats::parse_terran_map(bytes);
    return 0;
}
