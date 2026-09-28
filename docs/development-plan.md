# HID-Mouse v1 开发与验证计划

状态：`AWAITING_APPROVAL`  
风险：`HIGH`  
执行授权：`NO`

高风险触发因素是全局输入阻止、管理员权限、故障恢复，以及 RDP/触控/注入输入
来源信息不存在统一契约。用户确认本组文档后，授权按本计划实施；若技术探针不
满足继续条件，实施必须暂停，不能擅自扩展到新的驱动或系统修改。

## 1. 交付增量

### Increment A：工程骨架与 WinUHid 诊断

| ID | 对应需求 | 工作 | 产物 | 验证 |
| --- | --- | --- | --- | --- |
| TASK-A1 | REQ-01, REQ-02 | 建立 C++20/CMake/MSVC x64 工程和 CLI。 | CMake、`src/cli`、基本测试。 | VAL-A1：Debug/Release 构建和 CLI 单测。 |
| TASK-A2 | REQ-04, REQ-05 | 添加固定提交的官方 WinUHid submodule，只构建用户态 DLL。 | `third_party/WinUHid`、CMake/MSBuild 桥接。 | VAL-A2：干净克隆可初始化并构建。 |
| TASK-A3 | REQ-03, REQ-04, REQ-12 | 实现安全 DLL 加载、管理员检查、接口检查和 `doctor`。 | `winuhid_adapter`、诊断退出码。 | VAL-A3：缺 DLL、缺驱动、非管理员和成功路径。 |

### Increment B：只观察的输入探针

| ID | 对应需求 | 工作 | 产物 | 验证 |
| --- | --- | --- | --- | --- |
| TASK-B1 | REQ-07, REQ-08, REQ-12 | 实现 message-only window、Raw Input 和低级鼠标钩子的只观察模式。 | `input_bridge`、`probe`。 | VAL-B1：probe 不改变鼠标和触控行为。 |
| TASK-B2 | REQ-07 | 实现来源证据模型和结构化采样，不做猜测性分类。 | `source_classifier`、JSONL/文本日志。 | VAL-B2：单元测试覆盖所有已知/未知分支。 |
| TASK-B3 | REQ-06, REQ-13 | 采样物理鼠标、本地触控、RDP、Sunshine、软件注入及 WinUHid 回声。 | 经脱敏的实验记录。 | VAL-B3：每种来源都有可重复步骤和实际字段。 |

继续到 Increment C 的必要条件：

1. 可在钩子返回前识别计划转换的来源；
2. 可证明自身虚拟 HID 输出不会被再次转换；
3. 本地触控的原生 Pointer/Touch 行为在 probe 下保持不变；
4. 对无法识别的来源可以稳定归入 `unknown` 并放行。

任一条件不满足时，状态改为 `BLOCKED` 并提交证据。不得以时间/坐标猜测、全局
禁用输入、新增内核过滤驱动或目标应用特例绕过该 Gate。

### Increment C：安全转发核心

| ID | 对应需求 | 工作 | 产物 | 验证 |
| --- | --- | --- | --- | --- |
| TASK-C1 | REQ-05 | 实现规范化 MouseEvent、按钮状态和 WinUHid 报告映射。 | `mouse_state`、输出 worker。 | VAL-C1：边界值、分段、按钮和滚轮单测。 |
| TASK-C2 | REQ-07, REQ-09 | 实现严格策略和“成功入队才吞掉”的钩子决策。 | `forwarding_policy`、有界队列。 | VAL-C2：队列满/后端失败均 fail-open。 |
| TASK-C3 | REQ-10, REQ-11 | 实现紧急热键、Ctrl+C 和中立报告清理。 | `safety_controller`。 | VAL-C3：故障注入和人工恢复测试。 |
| TASK-C4 | REQ-12 | 组合 `run` 生命周期和稳定退出码。 | 可运行的 `hid-mouse.exe`。 | VAL-C4：重复启动/退出不遗留钩子或设备。 |

### Increment D：端到端验收与文档

| ID | 对应需求 | 工作 | 产物 | 验证 |
| --- | --- | --- | --- | --- |
| TASK-D1 | REQ-01..13 | 完成自动化回归和运行手册。 | README、故障排查、测试记录。 | VAL-D1：全新 checkout 构建验证。 |
| TASK-D2 | REQ-06, REQ-08 | 执行本地触控、物理鼠标和安全恢复测试。 | 人工验收记录。 | VAL-D2：所有必需场景通过。 |
| TASK-D3 | REQ-06 | 执行 zzz、Codex、RDP 和 Sunshine 测试。 | 人工验收记录。 | VAL-D3：支持项通过；不可识别项如实报告。 |

## 2. 验收与验证矩阵

| 验收 ID | 标准 | 验证 ID | 层级 | 必需 |
| --- | --- | --- | --- | --- |
| AC-01 | Windows 11 x64 可重复构建。 | VAL-A1, VAL-A2 | static/runtime | REQUIRED |
| AC-02 | 缺权限、DLL、驱动或接口不匹配时，在钩子安装前失败。 | VAL-A3 | integration | REQUIRED |
| AC-03 | probe 完全被动且能记录来源证据。 | VAL-B1..B3 | runtime/human | REQUIRED |
| AC-04 | 严格策略只吞可靠来源，未知来源放行。 | VAL-B2, VAL-C2 | unit/integration | REQUIRED |
| AC-05 | WinUHid 正确输出移动、按钮和滚轮。 | VAL-C1 | integration/human | REQUIRED |
| AC-06 | 触控滚动和多点触控未被破坏。 | VAL-D2 | human | REQUIRED |
| AC-07 | 任何可处理故障不留下按下按钮或持续输入阻断。 | VAL-C3, VAL-C4 | integration/human | REQUIRED |
| AC-08 | zzz 移动/左键和 Codex 关闭点击达到目标。 | VAL-D3 | end-to-end/human | REQUIRED |
| AC-09 | 运行时无目标应用特例。 | VAL-D1 | static | REQUIRED |

数据库验证为 `N/A`；项目无数据库。GUI 视觉验证为 `N/A`；项目不提供 GUI。

## 3. 退出码草案

| 代码 | 含义 |
| --- | --- |
| `0` | 命令成功或 `run` 正常停止。 |
| `2` | CLI 参数或配置错误。 |
| `3` | 未提升权限或交互会话不受支持。 |
| `4` | WinUHid 用户态 DLL 缺失、加载失败或导出不兼容。 |
| `5` | WinUHid 驱动缺失或接口版本不兼容。 |
| `6` | 虚拟鼠标创建或烟雾测试失败。 |
| `7` | 安全热键、消息循环或钩子初始化失败。 |
| `8` | 运行时后端故障，已切换 fail-open 并完成清理。 |
| `130` | 用户 Ctrl+C 或紧急旁路终止。 |

实现可在不改变错误语义的前提下细分内部错误，但 CLI 稳定退出码不得随意变化。

## 4. 风险控制与回退

- 所有实施改动都留在当前仓库；不安装驱动，不修改系统设置。
- submodule 固定提交，可通过普通 Git revert 移除。
- `probe` 永远不能吞输入，作为转发功能的前置技术 Gate。
- `run` 只有在后端、安全热键和队列均健康时进入 `FORWARDING`。
- 新发现的来源歧义首先回退为 `unknown/pass`。
- 任何需要内核过滤驱动、测试签名、关闭 Secure Boot 或后台服务的方案都超出已
  授权范围，必须重新进行需求和风险审批。

## 5. 当前 Gate 状态

| Gate | 状态 | 依据 |
| --- | --- | --- |
| Gate 0 Direction | READY | 用户明确要求通用转发兼容层并限定终端 v1。 |
| Gate 1 Requirement | READY | 本文档汇总当前已确认行为；文档整体仍待用户批准。 |
| Gate 2 Technical | BLOCKED（完整转发） | WinUHid 输出静态路径已确认；来源识别和自身回声必须由 Increment B 验证。 |
| Gate 3 Data/Contract | READY（A/B）/ BLOCKED（C/D） | 事件模型和失败语义已定义；转换来源映射需 probe 证据。 |
| Gate 4 Development Contract | READY（A/B）/ BLOCKED（C/D） | A/B 可执行；C/D 以四项继续条件为 Gate。 |

材料未知数不是产品选择，而是两个运行时事实：RDP/Sunshine 是否能即时可靠识别，
以及 WinUHid 回声在低级钩子中的表现。文档获批后先实现 A/B；证据满足继续条件
时按合同继续 C/D，否则暂停报告，不扩大范围。
