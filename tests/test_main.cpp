#include "cli.hpp"
#include "event_logger.hpp"
#include "source_classifier.hpp"

#include <Windows.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures{};

void expect(const bool condition, const char* message) {
    if (!condition) {
        ++failures;
        std::cerr << "FAIL: " << message << '\n';
    }
}

hid_mouse::CliParseResult parse(std::vector<std::wstring> arguments) {
    std::vector<wchar_t*> pointers;
    pointers.reserve(arguments.size());
    for (auto& argument : arguments) pointers.push_back(argument.data());
    return hid_mouse::parse_cli(static_cast<int>(pointers.size()), pointers.data());
}

void test_classifier() {
    using hid_mouse::SourceClass;
    using hid_mouse::classify_hook_source;
    expect(classify_hook_source(0, 0) == SourceClass::unknown, "plain hook is unknown");
    expect(classify_hook_source(LLMHF_INJECTED, 0) == SourceClass::software_injected,
           "injected flag is classified");
    expect(classify_hook_source(LLMHF_INJECTED | LLMHF_LOWER_IL_INJECTED, 0) ==
               SourceClass::lower_integrity_injected,
           "lower integrity injected flag wins");
    expect(classify_hook_source(0, 0xFF515780) == SourceClass::touch_promoted_mouse,
           "touch signature is classified");
    expect(classify_hook_source(0, 0xFF515700) == SourceClass::pen_promoted_mouse,
           "pen signature is classified");
}

void test_cli() {
    using hid_mouse::Command;
    using hid_mouse::OnceKind;
    auto result = parse({L"hid-mouse", L"probe", L"--duration", L"5", L"--log", L"x.jsonl"});
    expect(result.ok, "probe CLI parses");
    expect(result.options.command == Command::probe, "probe command selected");
    expect(result.options.duration_seconds == 5, "probe duration parsed");
    expect(result.options.log_path.has_value(), "log path parsed");

    result = parse({L"hid-mouse", L"run", L"--once", L"click", L"--timeout", L"10",
                    L"--replay-delay", L"5"});
    expect(result.ok, "run once CLI parses");
    expect(result.options.once == OnceKind::click, "click mode parsed");
    expect(result.options.timeout_seconds == 10, "timeout parsed");
    expect(result.options.replay_delay_seconds == 5, "replay delay parsed");

    result = parse({L"hid-mouse", L"run"});
    expect(!result.ok, "run without --once is rejected");

    result = parse({L"hid-mouse", L"doctor", L"--duration", L"1"});
    expect(!result.ok, "doctor rejects probe-only duration");

    result = parse({L"hid-mouse", L"run", L"--once", L"move", L"--replay-delay", L"31"});
    expect(!result.ok, "run rejects excessive replay delay");

    result = parse({L"hid-mouse", L"probe", L"--replay-delay", L"3"});
    expect(!result.ok, "probe rejects run-only replay delay");
}

void test_logger() {
    const auto path = std::filesystem::temp_directory_path() /
        (L"hid-mouse-test-" + std::to_wstring(GetCurrentProcessId()) + L".jsonl");
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    hid_mouse::EventLogger logger;
    expect(!logger.enabled(), "logger is disabled by default");
    logger.write("disabled_event");
    expect(!std::filesystem::exists(path), "disabled logger creates no file");

    std::string error;
    expect(logger.open(path, error), "logger opens explicit path");
    logger.write("test_event", {
        {"device", std::string("mouse\\device")},
        {"ok", true},
        {"value", static_cast<std::int64_t>(42)},
    });
    logger.close();

    std::ifstream input(path, std::ios::binary);
    const std::string line((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    expect(line.find("\"event\":\"test_event\"") != std::string::npos, "event is logged");
    expect(line.find("\"utc_source\":\"GetSystemTimePreciseAsFileTime\"") != std::string::npos,
           "UTC source is logged");
    expect(line.find("\"qpc_frequency\":") != std::string::npos, "QPC source is logged");
    expect(line.find("mouse\\\\device") != std::string::npos, "JSON string is escaped");
    std::filesystem::remove(path, ignored);
}

}  // namespace

int main() {
    test_classifier();
    test_cli();
    test_logger();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "All tests passed\n";
    return 0;
}
