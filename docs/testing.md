# 实机测试指南

当前测试目标是验证任务 B 的观察数据和 C-Once 的单动作 WinUHid 输出。所有命令
都在前台运行，不安装驱动，不启动服务或后台进程。

## 1. 准备

从管理员 PowerShell 7 进入仓库：

```powershell
git submodule update --init
cmake --preset vs2022-x64
cmake --build --preset release
ctest --preset release

$app = Resolve-Path .\build\vs2022-x64\Release\hid-mouse.exe
```

必须已经人工安装兼容的 WinUHid 驱动。

## 2. 驱动诊断

```powershell
& $app doctor --log .\doctor.jsonl
$LASTEXITCODE
```

预期退出码为 `0`，并输出驱动接口版本与虚拟鼠标创建/销毁成功。常见失败：

- `3`：终端未提升；
- `4`：用户态 DLL 缺失或导出不匹配；
- `5`：WinUHid 驱动缺失或不可访问；
- `6`：接口可访问，但虚拟鼠标创建失败。

## 3. 来源探针

```powershell
& $app probe --duration 30 --log .\probe.jsonl
```

在 30 秒内分别操作待测来源：物理鼠标、本地触控、RDP、Sunshine 或软件注入。
probe 不吞输入，也不创建虚拟鼠标。结束后检查：

```powershell
Get-Content .\probe.jsonl |
    ForEach-Object { $_ | ConvertFrom-Json } |
    Where-Object event -in raw_device, raw_mouse, low_level_mouse |
    Format-Table sequence, event, source_class, message, dx, dy, device_path
```

## 4. 单动作重放

C-Once 始终放行原事件。它先捕获一个样本，立即停止观察，然后默认等待 3 秒再
重放。请利用这段时间切换、聚焦和定位目标；期间其他输入不会被本程序捕获。

### 左键点击

```powershell
& $app run --once click --timeout 30 --replay-delay 3 --log .\once-click.jsonl
```

在 30 秒内先完成一次用于触发的左键按下与释放。捕获后程序立即停止观察；在随后
3 秒内把指针移到目标控件并聚焦目标。延时结束时，WinUHid 会在当时的指针位置
发送一次左键点击，然后程序退出。

### 相对移动

```powershell
& $app run --once move --timeout 30 --replay-delay 3 --log .\once-move.jsonl
```

轻微移动鼠标以捕获一个非零移动报告。捕获后有 3 秒可用键盘切换并聚焦 zzz；
期间的其他移动由 Windows 正常处理，但本程序忽略。延时结束后只重放最初捕获的
一个相对移动报告。长移动由多个报告组成，C-Once 不会聚合它们；正式持续模式将
逐个低延迟转发，而不是等待整个手势。

### 滚轮

```powershell
& $app run --once wheel --timeout 30 --replay-delay 3 --log .\once-wheel.jsonl
```

滚动一次垂直或水平滚轮以捕获样本，再利用 3 秒延时聚焦目标。原滚轮仍会放行，
延时结束后虚拟鼠标重放一次。

成功时日志中应按顺序出现：

1. `virtual_mouse_create`；
2. `input_bridge_started`；
3. 匹配的 `low_level_mouse`；
4. `hook_removed`、`capture_completed`；
5. `replay_delay_started`、`replay_delay_completed`；
6. `virtual_mouse_send`；
7. `virtual_mouse_neutral`、`virtual_mouse_destroy`；
8. `session_end`。

## 5. 目标场景

- Codex：使用 `--once click`，将指针放在关闭按钮上后完成一次触控点击。
- zzz：先使用 `--once move` 验证相对移动，再用 `--once click` 验证左键。
- RDP/Sunshine：先保存 probe 日志，再分别执行一次 move/click；不要把没有标志的
  事件仅凭远程会话环境认定为 RDP 来源。

请保留对应 JSONL。功能现象与日志字段结合后，才能决定持续转发阶段的来源策略。
