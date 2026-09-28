# ADR-0001：选择官方 WinUHid 作为输出依赖

- 状态：`PROPOSED`
- 日期：2026-09-28
- 决策范围：v1 虚拟鼠标用户态 SDK 来源

## 背景

候选仓库：

1. 官方仓库：<https://github.com/cgutman/WinUHid>
2. lurebat fork：<https://github.com/lurebat/WinUHid>

应用不会构建、安装或分发驱动，只使用预先安装的兼容驱动和用户态 SDK。

## 静态比较证据

比较时间为 2026-09-28：

| 项目 | 官方仓库 | lurebat fork |
| --- | --- | --- |
| 比较提交 | `d6cebbef5c7909168d1f881185be8f607d6aefd4` | `d880a9f3a42580ea7f6b96d37201f965feb4b310` |
| 分支关系 | 基线 | 比官方多 17 个提交，少 0 个提交 |
| fork 独有改动 | — | 31 个路径，约增加 9476 行、删除 34 行 |
| `WinUHid` 核心 API 差异 | — | 无 |
| `WinUHidMouse` 差异 | — | 无 |
| WinUHid 驱动核心/INF 差异 | — | 无 |
| 与设备 helper 相关的功能差异 | 基线 | 仅 PS5 helper/API 和测试 |
| 其他新增 | — | Web UI、Rust 依赖、Justfile、安装脚本、构建文档、嵌套 vcpkg |

lurebat fork 的增强能改善 WinUHid 自身的开发和演示，但不会减少 HID-Mouse
运行时的鼠标适配代码。它还扩大了供应链和仓库体积。其 README 要求启用测试
签名，而 BUILDING.md 又声称自签名 UMDF 可在 Secure Boot 下工作；由于本项目
明确不负责驱动安装，不能依赖这组相互不一致的安装说明。

## 决策

选择官方 `cgutman/WinUHid`，在实施获批后执行：

```powershell
git submodule add https://github.com/cgutman/WinUHid.git third_party/WinUHid
git -C third_party/WinUHid checkout d6cebbef5c7909168d1f881185be8f607d6aefd4
```

提交 `.gitmodules` 和固定的 gitlink。不得跟踪浮动 `main`，不得在 submodule
中维护本项目补丁。

只构建官方的用户态 SDK 与鼠标 helper；不初始化无关嵌套依赖，不构建驱动、
安装器和 WinUHid 单元测试。

## 影响

优点：

- 依赖来源和维护责任更清晰。
- 最小化无关 Web、Rust、vcpkg 和安装脚本表面积。
- 当前鼠标功能与 fork 完全相同。
- 将来能按明确提交审查上游更新。

代价：

- 本项目需要维护少量 CMake 到 MSBuild 的桥接代码。
- 不能直接复用 fork 的 Justfile 和构建说明。

## 重新评估条件

仅在以下任一条件成立时重新比较：

- fork 合入与鼠标 helper、核心 SDK、驱动接口兼容性直接相关的修复；
- 官方停止提供与已安装驱动兼容的用户态 SDK；
- 实际构建证据证明官方项目无法在目标 MSVC 工具链构建，而 fork 已修复相同
  问题。

活跃度、文档数量或 Web UI 本身不构成切换理由。
