#include "smac/core/replay.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto text = std::string_view(reinterpret_cast<const char*>(data), size);
    (void)smac::core::parse_command(text);
    (void)smac::core::parse_event(text);
    (void)smac::core::parse_replay(text);
    return 0;
}
