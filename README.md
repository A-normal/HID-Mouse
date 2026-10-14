# HID-Mouse

HID-Mouse 是面向 Windows 11 的虚拟 HID 鼠标兼容层，用来解决一类特殊的
输入兼容问题：远程桌面、触控提升或软件注入产生的鼠标事件能够驱动 Windows
光标，却可能不被只接受真实 HID 鼠标的程序识别。

程序在当前桌面会话中观察鼠标兼容输入。正常输入保持原样；需要转换且来源明确
的事件会被替换为一只 WinUHid 虚拟鼠标发出的移动、按键或滚轮报告。最终由
Windows 自己决定事件交给哪个窗口，HID-Mouse 不识别或适配具体应用。

> 当前状态：被动输入探针和单动作 WinUHid 重放已经实现；持续识别、吞掉并转发
> 输入仍在验证阶段。

## 适用场景

- RDP 或远控软件产生的鼠标输入无法操作某些程序。
- 触控已经移动光标，但提升出来的点击不被目标程序接受。
- 软件注入输入需要转换成系统可枚举的 HID 鼠标输入。
- 希望使用统一兼容层，而不是为每个目标程序维护坐标、窗口或控件规则。

## 设计目标

- **通用转发**：只关心输入来源与鼠标事件，不关心目标进程。
- **保留触控体验**：保留原生 Pointer/Touch 流，避免破坏滚动和多点触控。
- **安全放行**：来源不明、后端不可用或队列异常时不吞输入。
- **避免回环**：虚拟鼠标产生的输入不会再次进入转发链。
- **前台运行**：提供纯终端命令，不安装服务、不驻留托盘、不启动后台进程。

## 构建

需要 Visual Studio 2022（Desktop development with C++）和 CMake。应用构建只会
编译 WinUHid 用户态 DLL，不会构建或安装驱动：

```powershell
git submodule update --init
cmake --preset vs2022-x64
cmake --build --preset release
ctest --preset release
```

输出位于 `build/vs2022-x64/Release/`。

## 当前使用方式

程序需要从管理员 PowerShell 7 或其他管理员终端运行：

```powershell
# 检查权限、用户态依赖、驱动接口和虚拟鼠标创建能力
hid-mouse doctor

# 只观察输入来源，不吞输入、不转发
hid-mouse probe

# 捕获一个样本，停止观察，等待 3 秒后重放
hid-mouse run --once click --replay-delay 3 --log click.jsonl
hid-mouse run --once move --replay-delay 3 --log move.jsonl
hid-mouse run --once wheel --replay-delay 3 --log wheel.jsonl
```

`probe` 和 `run --once` 始终放行原输入。`run --once` 捕获一个测试样本后立即停止
Hook 和 Raw Input 观察，等待 `--replay-delay` 指定的时间，让用户切换、聚焦和
定位目标，再通过虚拟鼠标重放并退出。等待期间的输入仍由 Windows 正常处理，
但不会被本程序记录或转发。它是受控测试模式，不是最终的持续兼容转发。

正式持续模式不会采用该测试延时，也不会等待一个移动手势结束；长移动应作为
连续的小型输入报告低延迟转发。

只有指定 `--log <path>` 才创建 UTF-8 JSONL 调试日志。日志目录必须已经存在。
字段说明和 PowerShell 查看方式见[调试日志](docs/logging.md)，完整实测步骤见
[测试指南](docs/testing.md)。

## 运行要求

- Windows 11 x64。
- 管理员权限。
- 用户已经自行安装兼容的 WinUHid 驱动。
- 程序目录包含项目构建的 WinUHid 用户态 DLL。

HID-Mouse **不会**安装或更新驱动，也不会修改测试签名、Secure Boot、启动
配置或驱动证书。驱动缺失或接口不兼容时，程序会在安装输入钩子前报错退出。

## 支持范围

当前可测试：

- Raw Input 鼠标设备枚举与事件记录；
- 低级鼠标事件、注入标志和触控/笔提升签名记录；
- 单次左键点击、相对移动、垂直/水平滚轮的 WinUHid 重放；
- 明确的权限、DLL、驱动和虚拟设备诊断。

首个完整版本计划支持：

- 相对鼠标移动；
- 左键、右键、中键、X1 和 X2；
- 垂直与水平滚轮；
- 可识别的软件注入、触控提升和 RDP 鼠标来源；
- 故障时自动放行、按钮复位和紧急退出。

以下场景不在首个版本范围内：

- UAC 安全桌面、登录界面、锁屏和其他用户会话；
- 键盘转发；
- 目标应用专用规则；
- 自动安装驱动；
- GUI、Web 页面、系统服务或后台常驻。

如果 Windows 无法可靠区分某种远控输入，HID-Mouse 会保留原输入并通过
`probe` 报告，而不是猜测来源后全局吞掉鼠标。

## 开发文档

- [需求基线](docs/requirements.md)
- [系统架构](docs/architecture.md)
- [WinUHid 选型记录](docs/decisions/0001-winuhid-source.md)
- [开发与验证计划](docs/development-plan.md)
- [调试日志](docs/logging.md)
- [测试指南](docs/testing.md)

持续转发只会在 probe 证明来源识别和自身回声处理可靠后实现。
