#include "cli.hpp"
#include "event_logger.hpp"
#include "input_bridge.hpp"
#include "platform.hpp"
#include "winuhid_adapter.hpp"

#include <Windows.h>

#include <iostream>
#include <string>

namespace hid_mouse {
namespace {

enum ExitCode {
    success = 0,
    invalid_cli = 2,
    not_elevated = 3,
    dependency_error = 4,
    driver_error = 5,
    virtual_mouse_error = 6,
    input_bridge_error = 7,
    runtime_backend_error = 8,
    capture_timeout = 9,
    cancelled = 130,
};

std::string command_name(const Command command) {
    switch (command) {
        case Command::help: return "help";
        case Command::doctor: return "doctor";
        case Command::probe: return "probe";
        case Command::run: return "run";
    }
    return "unknown";
}

void log_session(EventLogger& logger, const CliOptions& options) {
    const SystemSnapshot snapshot = collect_system_snapshot();
    logger.write("session_start", {
        {"command", command_name(options.command)},
        {"once_kind", std::string(to_string(options.once))},
        {"replay_delay_seconds", static_cast<std::uint64_t>(options.replay_delay_seconds)},
        {"pid", static_cast<std::uint64_t>(snapshot.process_id)},
        {"session_id", static_cast<std::uint64_t>(snapshot.session_id)},
        {"elevated", snapshot.elevated},
        {"remote_session", snapshot.remote_session},
        {"windows_major", static_cast<std::uint64_t>(snapshot.windows_major)},
        {"windows_minor", static_cast<std::uint64_t>(snapshot.windows_minor)},
        {"windows_build", static_cast<std::uint64_t>(snapshot.windows_build)},
        {"architecture", snapshot.architecture},
        {"original_events_suppressed", false},
    });
}

int require_elevation(EventLogger& logger) {
    std::string error;
    if (is_process_elevated(error)) {
        logger.write("elevation_check", {{"ok", true}});
        return success;
    }
    logger.write("elevation_check", {
        {"ok", false},
        {"error", error},
    });
    std::cerr << "Administrator privileges are required: " << error << '\n';
    return not_elevated;
}

int load_and_probe_driver(WinUHidAdapter& adapter, EventLogger& logger) {
    std::string error;
    if (!adapter.load(executable_directory(), error)) {
        std::cerr << error << '\n';
        return dependency_error;
    }
    const DWORD version = adapter.driver_interface_version(error);
    if (version == 0) {
        std::cerr << error << '\n'
                  << "Install a compatible WinUHid driver before running this command.\n";
        return driver_error;
    }
    std::cout << "WinUHid driver interface version: " << version << '\n';
    logger.write("driver_ready", {{"interface_version", static_cast<std::uint64_t>(version)}});
    return success;
}

int run_doctor(EventLogger& logger) {
    const int elevation = require_elevation(logger);
    if (elevation != success) return elevation;

    WinUHidAdapter adapter(logger);
    const int driver = load_and_probe_driver(adapter, logger);
    if (driver != success) return driver;

    std::string error;
    if (!adapter.create_mouse(error)) {
        std::cerr << error << '\n';
        return virtual_mouse_error;
    }
    adapter.destroy_mouse();
    std::cout << "Virtual mouse create/destroy test: OK\n";
    return success;
}

int run_probe(const CliOptions& options, EventLogger& logger) {
    const int elevation = require_elevation(logger);
    if (elevation != success) return elevation;

    std::cout << "Probe is observing only. Original input is never suppressed.\n";
    if (options.duration_seconds == 0) {
        std::cout << "Press Ctrl+C to stop.\n";
    }

    InputBridge bridge(logger);
    const BridgeResult result = bridge.run({
        .mode = BridgeMode::probe,
        .once = OnceKind::none,
        .limit_seconds = options.duration_seconds,
    });
    if (!result.ok) {
        std::cerr << result.error << '\n';
        return input_bridge_error;
    }
    return result.cancelled ? cancelled : success;
}

int run_once(const CliOptions& options, EventLogger& logger) {
    const int elevation = require_elevation(logger);
    if (elevation != success) return elevation;

    WinUHidAdapter adapter(logger);
    const int driver = load_and_probe_driver(adapter, logger);
    if (driver != success) return driver;

    std::string error;
    if (!adapter.create_mouse(error)) {
        std::cerr << error << '\n';
        return virtual_mouse_error;
    }

    std::cout << "Waiting for one " << to_string(options.once)
              << " sample (original input remains enabled)...\n"
              << "After capture, observation stops. You then have "
              << options.replay_delay_seconds
              << " second(s) to focus and position the target before replay.\n";
    InputBridge bridge(logger);
    const BridgeResult result = bridge.run({
        .mode = BridgeMode::once,
        .once = options.once,
        .limit_seconds = options.timeout_seconds,
        .replay_delay_seconds = options.replay_delay_seconds,
    });
    if (!result.ok) {
        std::cerr << result.error << '\n';
        return input_bridge_error;
    }
    if (result.cancelled) {
        std::cerr << "Capture cancelled.\n";
        return cancelled;
    }
    if (!result.captured) {
        std::cerr << "No matching action was captured before timeout.\n";
        return capture_timeout;
    }

    // InputBridge::run removes the hook before returning. Replaying here cannot
    // be observed by this process's low-level hook, so C-Once cannot self-loop.
    if (!adapter.send(result.action, error)) {
        std::cerr << error << '\n';
        return runtime_backend_error;
    }
    std::cout << "Replayed one " << to_string(result.action.kind)
              << " action through WinUHid.\n";
    return success;
}

}  // namespace
}  // namespace hid_mouse

int wmain(const int argc, wchar_t* argv[]) {
    using namespace hid_mouse;

    const CliParseResult parsed = parse_cli(argc, argv);
    if (!parsed.ok) {
        std::cerr << "Error: " << parsed.error << "\n\n" << usage_text();
        return invalid_cli;
    }
    if (parsed.options.command == Command::help) {
        std::cout << usage_text();
        return success;
    }

    EventLogger logger;
    if (parsed.options.log_path.has_value()) {
        std::string error;
        if (!logger.open(*parsed.options.log_path, error)) {
            std::cerr << "Cannot enable logging at "
                      << wide_to_utf8(parsed.options.log_path->wstring())
                      << ": " << error << '\n';
            return invalid_cli;
        }
        std::cout << "Debug log: " << wide_to_utf8(logger.path().wstring()) << '\n';
    }
    log_session(logger, parsed.options);

    int result{};
    switch (parsed.options.command) {
        case Command::doctor:
            result = run_doctor(logger);
            break;
        case Command::probe:
            result = run_probe(parsed.options, logger);
            break;
        case Command::run:
            result = run_once(parsed.options, logger);
            break;
        case Command::help:
            result = success;
            break;
    }
    logger.write("session_end", {{"exit_code", static_cast<std::int64_t>(result)}});
    logger.close();
    return result;
}
