#include "smac/formats/rules.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto text = std::string_view(reinterpret_cast<const char*>(data), size);
    (void)smac::formats::parse_rules(text);
    return 0;
}
