# 调试日志

HID-Mouse 默认不创建日志文件。只有传入 `--log <path>` 时才创建 UTF-8 JSON
Lines 文件；每行都是一个独立 JSON 对象。日志目录必须事先存在。

```powershell
hid-mouse probe --duration 20 --log probe.jsonl
hid-mouse run --once click --replay-delay 3 --log click.jsonl
```

## 时间字段

每条记录包含两套时间：

| 字段 | 来源 | 用途 |
| --- | --- | --- |
| `utc` | `GetSystemTimePreciseAsFileTime` | 与其他系统日志对齐的 UTC 墙钟时间。 |
| `utc_source` | 固定说明 | 明确墙钟 API。 |
| `qpc_ticks` | `QueryPerformanceCounter` | 单调、高分辨率事件排序。 |
| `qpc_frequency` | `QueryPerformanceFrequency` | 将 tick 换算成时间。 |
| `monotonic_us` | 从进程日志起点计算 | 便于查看相邻操作耗时。 |

不要使用墙钟差值分析短时延迟；系统时间可能被校时。短时顺序和间隔应使用 QPC
字段。

## 主要事件

| `event` | 内容 |
| --- | --- |
| `session_start` | 命令、PID、Session、管理员状态、远程会话标记、Windows 版本和架构。 |
| `raw_device_snapshot` | 启动时 Raw Input 设备数量。 |
| `raw_device` | 设备句柄、设备路径、类型、鼠标按钮数/采样率或 HID VID/PID/Usage。 |
| `raw_mouse` | 设备关联的 Raw Input 移动、按钮和滚轮数据。 |
| `low_level_mouse` | 钩子消息、坐标、flags、`dwExtraInfo`、即时来源分类和决策。 |
| `capture_completed` | 已冻结的单个测试样本及其动作数据。 |
| `replay_delay_started` | 捕获后观察已停止，开始为用户保留目标准备时间。 |
| `replay_delay_completed` | 准备时间结束，即将返回主流程发送样本。 |
| `winuhid_driver_probe` | 驱动接口版本和 Win32 结果。 |
| `virtual_mouse_create` | 虚拟鼠标创建结果。 |
| `virtual_mouse_send` | 重放动作、来源、数据和发送结果。 |
| `hook_removed` | 发送前/退出时卸载钩子的结果。 |
| `virtual_mouse_neutral` | 所有虚拟按钮复位结果。 |
| `session_end` | 程序退出码。 |

当前 `probe` 和 C-Once 的 `suppressed`/`original_events_suppressed` 永远是 `false`。
C-Once 的 `replay_delay_started.other_input_policy` 为 `not_observed`：等待期间输入
照常交给 Windows，但本程序已经停止 Hook/Raw Input 观察，不记录也不转发。

## 来源解释

`source_class` 只使用低级钩子当下提供的证据：

- `touch_promoted_mouse`：匹配 Windows 触控提升签名；
- `pen_promoted_mouse`：匹配笔提升签名；
- `software_injected`：包含 `LLMHF_INJECTED`；
- `lower_integrity_injected`：同时包含低完整性注入标志；
- `unknown`：没有足够证据，不能理解为“物理鼠标”。

`remote_session_candidate: true` 只说明程序运行在远程会话，不证明该条事件一定由
RDP 产生。Raw Input 和低级钩子没有公开的共同事件 ID，日志不会伪造两者的精确
关联。

## PowerShell 查看

```powershell
Get-Content .\probe.jsonl |
    ForEach-Object { $_ | ConvertFrom-Json } |
    Select-Object sequence, utc, event, source_class, message, device_path, ok
```

日志不记录目标进程、窗口标题、控件、键盘输入或文本内容。
