#pragma once

#include "app_types.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace hid_mouse {

enum class Command {
    help,
    doctor,
    probe,
    run,
};

struct CliOptions {
    Command command{Command::help};
    std::optional<std::filesystem::path> log_path;
    OnceKind once{OnceKind::none};
    unsigned duration_seconds{};
    unsigned timeout_seconds{30};
    unsigned replay_delay_seconds{3};
};

struct CliParseResult {
    bool ok{};
    CliOptions options;
    std::string error;
};

CliParseResult parse_cli(int argc, wchar_t* argv[]);
std::string usage_text();

}  // namespace hid_mouse
