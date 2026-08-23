#include "smac/formats/text.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    const auto text = std::string_view(reinterpret_cast<const char*>(data), size);
    (void)smac::formats::cp1252_to_utf8(text);
    (void)smac::formats::normalize_text(text);
    return 0;
}
