#pragma once

#include "smac/core/game_state.hpp"

#include <span>
#include <string_view>

namespace smac::core {
inline constexpr std::uint32_t replay_format_version = 2;

struct ReplayEntry {
    Command command;
    std::vector<Event> events;
    std::uint64_t state_hash{};
    friend bool operator==(const ReplayEntry&, const ReplayEntry&) = default;
};

struct ReplayLog {
    std::uint32_t version{replay_format_version};
    std::uint64_t initial_state_hash{};
    std::vector<ReplayEntry> entries;
    friend bool operator==(const ReplayLog&, const ReplayLog&) = default;
};

struct ReplayError {
    std::string message;
    std::size_t line{};
};

template <class T> using ReplayResult = std::variant<T, ReplayError>;

[[nodiscard]] std::string serialize_command(const Command& command);
[[nodiscard]] std::string serialize_event(const Event& event);
[[nodiscard]] ReplayResult<Command> parse_command(std::string_view text);
[[nodiscard]] ReplayResult<Event> parse_event(std::string_view text);
[[nodiscard]] std::string serialize_replay(const ReplayLog& replay);
[[nodiscard]] ReplayResult<ReplayLog> parse_replay(std::string_view text);
[[nodiscard]] ReplayLog record_replay(GameState state, std::span<const Command> commands);
[[nodiscard]] ReplayResult<std::uint64_t> replay_commands(GameState& state,
                                                          const ReplayLog& replay);
} // namespace smac::core
