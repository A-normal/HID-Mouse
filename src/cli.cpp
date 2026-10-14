#include "cli.hpp"

#include <limits>
#include <string_view>

namespace hid_mouse {
namespace {

std::optional<unsigned> parse_unsigned(const std::wstring_view value) {
    if (value.empty()) {
        return std::nullopt;
    }
    unsigned result{};
    for (const wchar_t character : value) {
        if (character < L'0' || character > L'9') {
            return std::nullopt;
        }
        const unsigned digit = static_cast<unsigned>(character - L'0');
        if (result > (std::numeric_limits<unsigned>::max() - digit) / 10U) {
            return std::nullopt;
        }
        result = result * 10U + digit;
    }
    return result;
}

std::optional<OnceKind> parse_once_kind(const std::wstring_view value) {
    if (value == L"click") return OnceKind::click;
    if (value == L"move") return OnceKind::move;
    if (value == L"wheel") return OnceKind::wheel;
    return std::nullopt;
}

}  // namespace

CliParseResult parse_cli(const int argc, wchar_t* argv[]) {
    CliParseResult result;
    bool replay_delay_specified{};
    if (argc <= 1) {
        result.ok = true;
        result.options.command = Command::help;
        return result;
    }

    const std::wstring_view command = argv[1];
    if (command == L"--help" || command == L"-h" || command == L"help") {
        result.ok = true;
        result.options.command = Command::help;
        return result;
    }
    if (command == L"doctor") result.options.command = Command::doctor;
    else if (command == L"probe") result.options.command = Command::probe;
    else if (command == L"run") result.options.command = Command::run;
    else {
        result.error = "unknown command";
        return result;
    }

    for (int index = 2; index < argc; ++index) {
        const std::wstring_view argument = argv[index];
        if (argument == L"--log") {
            if (++index >= argc || std::wstring_view(argv[index]).empty()) {
                result.error = "--log requires a path";
                return result;
            }
            if (result.options.log_path.has_value()) {
                result.error = "--log may only be specified once";
                return result;
            }
            result.options.log_path = std::filesystem::path(argv[index]);
        } else if (argument == L"--duration") {
            if (++index >= argc) {
                result.error = "--duration requires seconds";
                return result;
            }
            const auto value = parse_unsigned(argv[index]);
            if (!value.has_value() || *value > 86400) {
                result.error = "--duration must be between 0 and 86400 seconds";
                return result;
            }
            result.options.duration_seconds = *value;
        } else if (argument == L"--timeout") {
            if (++index >= argc) {
                result.error = "--timeout requires seconds";
                return result;
            }
            const auto value = parse_unsigned(argv[index]);
            if (!value.has_value() || *value == 0 || *value > 300) {
                result.error = "--timeout must be between 1 and 300 seconds";
                return result;
            }
            result.options.timeout_seconds = *value;
        } else if (argument == L"--replay-delay") {
            if (replay_delay_specified) {
                result.error = "--replay-delay may only be specified once";
                return result;
            }
            if (++index >= argc) {
                result.error = "--replay-delay requires seconds";
                return result;
            }
            const auto value = parse_unsigned(argv[index]);
            if (!value.has_value() || *value > 30) {
                result.error = "--replay-delay must be between 0 and 30 seconds";
                return result;
            }
            result.options.replay_delay_seconds = *value;
            replay_delay_specified = true;
        } else if (argument == L"--once") {
            if (++index >= argc) {
                result.error = "--once requires click, move, or wheel";
                return result;
            }
            const auto value = parse_once_kind(argv[index]);
            if (!value.has_value()) {
                result.error = "--once requires click, move, or wheel";
                return result;
            }
            result.options.once = *value;
        } else if (argument == L"--help" || argument == L"-h") {
            result.options.command = Command::help;
        } else {
            result.error = "unknown option";
            return result;
        }
    }

    if (result.options.command == Command::run && result.options.once == OnceKind::none) {
        result.error = "run currently requires --once click|move|wheel";
        return result;
    }
    if (result.options.command != Command::run && result.options.once != OnceKind::none) {
        result.error = "--once is only valid with run";
        return result;
    }
    if (result.options.command != Command::run && result.options.timeout_seconds != 30) {
        result.error = "--timeout is only valid with run";
        return result;
    }
    if (result.options.command != Command::run && replay_delay_specified) {
        result.error = "--replay-delay is only valid with run";
        return result;
    }
    if (result.options.command != Command::probe && result.options.duration_seconds != 0) {
        result.error = "--duration is only valid with probe";
        return result;
    }

    result.ok = true;
    return result;
}

std::string usage_text() {
    return
        "HID-Mouse - Windows virtual HID mouse compatibility probe\n\n"
        "Usage:\n"
        "  hid-mouse doctor [--log <path>]\n"
        "  hid-mouse probe [--duration <seconds>] [--log <path>]\n"
        "  hid-mouse run --once click|move|wheel [--timeout <seconds>]\n"
        "                [--replay-delay <seconds>] [--log <path>]\n"
        "\n"
        "Logging is disabled unless --log is provided. run --once never suppresses\n"
        "the original event. After capture it stops observing, waits 3 seconds by\n"
        "default, then replays the captured sample through WinUHid.\n";
}

}  // namespace hid_mouse
