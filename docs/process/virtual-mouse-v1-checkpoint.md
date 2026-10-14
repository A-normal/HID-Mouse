# Virtual Mouse v1 - Task B / C-Once checkpoint

Date: 2026-10-03

## Delivery state

- Status: `VERIFICATION_PENDING`
- Authorized increment: Task A, Task B, and the controllable `C-Once` path only.
- Continuous interception/suppression remains outside this increment.
- No driver installation, service mutation, or background process was performed.

## Implemented scope

- `doctor`: validates elevation, user-mode WinUHid DLL loading, driver interface compatibility, and virtual mouse creation.
- `probe`: records Raw Input device inventory/change notifications and low-level mouse events without suppressing input.
- `run --once click|move|wheel`: captures one test sample, stops Hook/Raw Input observation, waits for a configurable foreground replay delay, replays the frozen sample through WinUHid, logs cleanup, and exits.
- Logging is disabled by default and enabled only with `--log <path>`.
- JSONL diagnostics include precise UTC, QPC monotonic timing, session/elevation state, device information, observed operation, source-classification evidence, forwarding result, and cleanup result.
- Hook callbacks only enqueue bounded event data and post a window message; file I/O and WinUHid calls occur outside the hook.

## Requirement trace

| Requirement | State | Evidence / remaining work |
| --- | --- | --- |
| REQ-01..REQ-07 | implemented | CLI, platform checks, WinUHid adapter, deterministic exit codes |
| REQ-08..REQ-13 | implemented for passive observation | Raw Input inventory plus `WH_MOUSE_LL`; source classification is evidence-based and does not guess RDP from session state alone |
| REQ-14 | implemented | opt-in UTF-8 JSONL logger and logger tests |
| REQ-15..REQ-16 | implemented, runtime verification pending | one-sample capture, stopped observation during test delay, then replay; production direction remains per-report without delay |

## Validation evidence

- Visual Studio 2022 x64 Debug build completed successfully.
- Visual Studio 2022 x64 Release build completed successfully.
- Debug CTest: 2/2 passed (`unit`, `cli-help`).
- Release CTest: 2/2 passed (`unit`, `cli-help`).
- WinUHid exports and runtime DLL dependencies were inspected.
- A non-elevated `doctor --log` run exited with code 3 and produced valid timing, environment, decision, and final-result records.
- The repository execution helper later failed to create any new foreground PowerShell process (`helper_unknown_error: setup refresh had errors`), so the final documentation-only/source-adapter delta could not be followed by another redundant build in this session.
- User runtime logs later verified elevated driver probing, virtual mouse create/destroy, lossless RDP observation, and successful old C-Once backend calls.
- The old C-Once timing was not human-usable for target preparation. On 2026-10-03 it was replaced with capture, immediate observation stop, configurable replay delay, and replay. The revised Debug and Release builds passed with zero errors and both CTest presets passed 2/2; human validation is pending.

## Required human verification

Run from an elevated foreground PowerShell 7 session:

1. `doctor --log .\doctor.jsonl`
2. `probe --duration 30 --log .\probe.jsonl`
3. `run --once click --replay-delay 3 --log .\once-click-v2.jsonl`
4. `run --once move --replay-delay 3 --log .\once-move-v2.jsonl`
5. `run --once wheel --replay-delay 3 --log .\once-wheel-v2.jsonl`

Success criteria:

- `doctor` exits 0 and reports a compatible driver and created virtual mouse.
- `probe` records physical, touch/pen-signature, injected, remote-session-candidate, and device-change evidence without altering input.
- Each `run --once` command captures exactly one matching sample, stops observation, logs the replay delay, reports one successful virtual-HID send, performs cleanup, and exits 0.
- The target applications accept the replayed action.

## Known boundary

`C-Once` deliberately passes the original sample through. After capture, the program stops observing; input used to focus or position the target during the replay delay is handled normally by Windows but is neither logged nor forwarded by HID-Mouse. The test delay is not a production forwarding policy. Suppression remains deferred until real probe samples establish a safe discrimination rule and a recovery path.
