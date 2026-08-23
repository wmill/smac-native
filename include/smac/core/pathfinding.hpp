#pragma once
#include "smac/core/game_state.hpp"
namespace smac::core {
std::vector<MapPosition> find_path(const GameState& state, UnitId unit, MapPosition goal,
                                   std::int32_t budget);
}
