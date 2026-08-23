#pragma once
#include "smac/core/game_state.hpp"
namespace smac::core { std::vector<MapPosition> find_path(const WorldMap&, MapPosition, MapPosition, std::int32_t budget); }

