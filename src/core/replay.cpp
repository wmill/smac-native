#include "smac/core/replay.hpp"

#include <charconv>
#include <iomanip>
#include <limits>
#include <sstream>

namespace smac::core {
namespace {
constexpr std::size_t max_replay_bytes = 16 * 1024 * 1024;
constexpr std::size_t max_replay_entries = 1'000'000;
constexpr std::size_t max_events_per_entry = 1024;

template <class T> bool parse_number(std::string_view text, T& value, int base = 10) {
    if (text.empty())
        return false;
    auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value, base);
    return error == std::errc{} && end == text.data() + text.size();
}

std::vector<std::string_view> fields(std::string_view text) {
    std::vector<std::string_view> result;
    while (!text.empty()) {
        const auto first = text.find_first_not_of(' ');
        if (first == text.npos)
            break;
        text.remove_prefix(first);
        const auto end = text.find(' ');
        result.push_back(text.substr(0, end));
        if (end == text.npos)
            break;
        text.remove_prefix(end + 1);
    }
    return result;
}

std::string hash_string(std::uint64_t hash) {
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << hash;
    return output.str();
}

std::string hex_string(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (char raw : value) {
        const auto byte = static_cast<unsigned char>(raw);
        result.push_back(digits[byte >> 4U]);
        result.push_back(digits[byte & 0x0FU]);
    }
    return result.empty() ? "-" : result;
}

bool parse_hex_string(std::string_view text, std::string& result) {
    if (text == "-") {
        result.clear();
        return true;
    }
    if ((text.size() & 1U) != 0U)
        return false;
    result.clear();
    result.reserve(text.size() / 2);
    for (std::size_t i = 0; i < text.size(); i += 2) {
        unsigned value{};
        if (!parse_number(text.substr(i, 2), value, 16))
            return false;
        result.push_back(static_cast<char>(value));
    }
    return true;
}
} // namespace

std::string serialize_command(const Command& command) {
    return std::visit(
        [](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, MoveUnit>)
                return std::string("MOVE ") + std::to_string(value.unit) + " " +
                       std::to_string(value.destination.x) + " " +
                       std::to_string(value.destination.y);
            else
                return std::string("END_TURN");
        },
        command);
}

std::string serialize_event(const Event& event) {
    return std::visit(
        [](const auto& value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, UnitMoved>) {
                return std::string("UNIT_MOVED ") + std::to_string(value.unit) + " " +
                       std::to_string(value.from.x) + " " + std::to_string(value.from.y) + " " +
                       std::to_string(value.to.x) + " " + std::to_string(value.to.y) + " " +
                       std::to_string(value.cost);
            } else if constexpr (std::is_same_v<T, TurnAdvanced>) {
                return std::string("TURN_ADVANCED ") + std::to_string(value.turn);
            } else {
                return std::string("COMMAND_REJECTED ") + hex_string(value.reason);
            }
        },
        event);
}

ReplayResult<Command> parse_command(std::string_view text) {
    const auto parts = fields(text);
    if (parts.size() == 1 && parts[0] == "END_TURN")
        return Command{EndTurn{}};
    MoveUnit move;
    if (parts.size() == 4 && parts[0] == "MOVE" && parse_number(parts[1], move.unit) &&
        parse_number(parts[2], move.destination.x) && parse_number(parts[3], move.destination.y))
        return Command{move};
    return ReplayError{"invalid command", 0};
}

ReplayResult<Event> parse_event(std::string_view text) {
    const auto parts = fields(text);
    UnitMoved moved;
    if (parts.size() == 7 && parts[0] == "UNIT_MOVED" && parse_number(parts[1], moved.unit) &&
        parse_number(parts[2], moved.from.x) && parse_number(parts[3], moved.from.y) &&
        parse_number(parts[4], moved.to.x) && parse_number(parts[5], moved.to.y) &&
        parse_number(parts[6], moved.cost))
        return Event{moved};
    TurnAdvanced advanced;
    if (parts.size() == 2 && parts[0] == "TURN_ADVANCED" &&
        parse_number(parts[1], advanced.turn))
        return Event{advanced};
    if (parts.size() == 2 && parts[0] == "COMMAND_REJECTED") {
        CommandRejected rejected;
        if (parse_hex_string(parts[1], rejected.reason))
            return Event{std::move(rejected)};
    }
    return ReplayError{"invalid event", 0};
}

std::string serialize_replay(const ReplayLog& replay) {
    std::ostringstream output;
    output << "SMAC_REPLAY " << replay.version << '\n';
    output << "INITIAL " << hash_string(replay.initial_state_hash) << '\n';
    for (const auto& entry : replay.entries) {
        output << "COMMAND " << serialize_command(entry.command) << '\n';
        for (const auto& event : entry.events)
            output << "EVENT " << serialize_event(event) << '\n';
        output << "STATE " << hash_string(entry.state_hash) << '\n';
    }
    output << "END\n";
    return output.str();
}

ReplayResult<ReplayLog> parse_replay(std::string_view text) {
    if (text.size() > max_replay_bytes)
        return ReplayError{"replay exceeds size limit", 0};
    ReplayLog result;
    ReplayEntry* entry = nullptr;
    bool saw_header = false;
    bool saw_initial = false;
    bool saw_end = false;
    bool entry_has_state = false;
    std::size_t line_number = 0;
    while (!text.empty()) {
        ++line_number;
        const auto newline = text.find('\n');
        auto line = text.substr(0, newline);
        if (!line.empty() && line.back() == '\r')
            line.remove_suffix(1);
        text = newline == text.npos ? std::string_view{} : text.substr(newline + 1);
        if (line.empty())
            continue;
        if (!saw_header) {
            const auto parts = fields(line);
            if (parts.size() != 2 || parts[0] != "SMAC_REPLAY" ||
                !parse_number(parts[1], result.version) || result.version != replay_format_version)
                return ReplayError{"unsupported or missing replay version", line_number};
            saw_header = true;
            continue;
        }
        if (line.starts_with("INITIAL ") && !saw_initial && result.entries.empty()) {
            if (!parse_number(line.substr(8), result.initial_state_hash, 16))
                return ReplayError{"invalid initial state hash", line_number};
            saw_initial = true;
            continue;
        }
        if (line.starts_with("COMMAND ") && saw_initial) {
            if (entry && !entry_has_state)
                return ReplayError{"command is missing its state hash", line_number};
            if (result.entries.size() >= max_replay_entries)
                return ReplayError{"too many replay entries", line_number};
            auto command = parse_command(line.substr(8));
            if (auto* error = std::get_if<ReplayError>(&command))
                return ReplayError{error->message, line_number};
            result.entries.push_back({std::get<Command>(std::move(command)), {}, 0});
            entry = &result.entries.back();
            entry_has_state = false;
            continue;
        }
        if (line.starts_with("EVENT ") && entry && !entry_has_state) {
            if (entry->events.size() >= max_events_per_entry)
                return ReplayError{"too many events for command", line_number};
            auto event = parse_event(line.substr(6));
            if (auto* error = std::get_if<ReplayError>(&event))
                return ReplayError{error->message, line_number};
            entry->events.push_back(std::get<Event>(std::move(event)));
            continue;
        }
        if (line.starts_with("STATE ") && entry && !entry_has_state) {
            if (!parse_number(line.substr(6), entry->state_hash, 16))
                return ReplayError{"invalid state hash", line_number};
            entry_has_state = true;
            continue;
        }
        if (line == "END" && saw_initial && (!entry || entry_has_state)) {
            saw_end = true;
            if (!text.empty())
                return ReplayError{"content follows replay end", line_number};
            break;
        }
        return ReplayError{"unexpected replay line", line_number};
    }
    if (!saw_header || !saw_initial || !saw_end)
        return ReplayError{"truncated replay", line_number};
    return result;
}

ReplayLog record_replay(GameState state, std::span<const Command> commands) {
    ReplayLog replay;
    replay.initial_state_hash = state.stable_hash();
    replay.entries.reserve(commands.size());
    for (const auto& command : commands) {
        auto events = state.apply(command);
        replay.entries.push_back({command, std::move(events), state.stable_hash()});
    }
    return replay;
}

ReplayResult<std::uint64_t> replay_commands(GameState& state, const ReplayLog& replay) {
    if (replay.version != replay_format_version)
        return ReplayError{"unsupported replay version", 0};
    if (state.stable_hash() != replay.initial_state_hash)
        return ReplayError{"initial state hash mismatch", 0};
    for (std::size_t i = 0; i < replay.entries.size(); ++i) {
        const auto& expected = replay.entries[i];
        const auto events = state.apply(expected.command);
        if (events != expected.events)
            return ReplayError{"event mismatch", i + 1};
        if (state.stable_hash() != expected.state_hash)
            return ReplayError{"state hash mismatch", i + 1};
    }
    return state.stable_hash();
}
} // namespace smac::core
