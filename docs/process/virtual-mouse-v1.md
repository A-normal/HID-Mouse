> This is a development workflow process record. It is not a customer
> requirement, product rule, acceptance authority, or source for deriving
> requirements. Follow the cited active requirement sources.

```yaml
artifact_type: dev-workflow-process-ledger
authority: process-record-only
is_requirement_source: false
project: HID-Mouse
increment_id: virtual-mouse-v1
workflow_run_id: planning-2026-09-28
risk_level: HIGH
status: VERIFICATION_PENDING
plan_version: 0.3
checkpoint_sequence: 5
created_at: 2026-09-28T15:57:08+08:00
updated_at: 2026-10-03T16:30:00+08:00
evidence_valid_as_of: 2026-10-03
```

# Virtual Mouse v1 流程台账

## 当前方向与基线

- 用户目标：建立与目标应用无关的 Windows 输入兼容层。
- 候选需求基线：[`../requirements.md`](../requirements.md)。
- 架构：[`../architecture.md`](../architecture.md)。
- 开发合同：[`../development-plan.md`](../development-plan.md)。
- 依赖决策：[`../decisions/0001-winuhid-source.md`](../decisions/0001-winuhid-source.md)。
- 当前范围：实施 Increment A、B 和 C-Once；持续吞掉/转发未授权。

## 风险与 Gate

- 风险：`HIGH`。
- 触发因素：管理员权限、全局输入阻止、故障恢复、来源识别不确定性。
- Gate 0：`READY`。
- Gate 1：`READY`，用户已确认原文档并补充日志/C-Once 需求。
- Gate 2：A/B/C-Once `READY`；持续转发 `BLOCKED`。
- Gate 3：A/B/C-Once `READY`；持续 C/D `BLOCKED`。
- Gate 4：A/B/C-Once `READY`；持续 C/D 为条件合同。

## 物料决策

1. 选择官方 `cgutman/WinUHid`，计划固定提交
   `d6cebbef5c7909168d1f881185be8f607d6aefd4`。
2. lurebat fork 相对官方多 17 个提交，但与核心 SDK、鼠标 helper 和驱动接口
   相关的比较文件无差异，因此不选作依赖。
3. 不打包、不安装、不更新驱动；只检测预装驱动。
4. 不启动后台进程、服务、GUI 或托盘。
5. 项目 README 面向使用者介绍问题、场景、使用方式、要求和边界；架构分析和
   开发 Gate 留在 `docs/`。
6. `--log <path>` 显式启用 JSONL 调试日志；没有该参数不创建日志文件。
7. 当前部分 C 仅为 `run --once click|move|wheel`：原事件放行、发送前卸载钩子、
   捕获一个样本后停止观察，经过可配置准备延时再重放并退出。
8. 用户 2026-10-03 明确纠正测试模型：准备延时只存在于 C-Once 的捕获与重放
   之间；延时期间本程序忽略其他输入。正式持续模式必须逐报告低延迟转发，不得
   等待移动手势结束或引入固定聚合延时。

## REQ -> TASK -> AC -> VAL

详细映射位于 [`../development-plan.md`](../development-plan.md)。当前可执行范围
为 TASK-A1..A3、TASK-B1..B4 和 TASK-C0，已经获得用户实施授权。

## 已变更工件

- `README.md`
- `docs/requirements.md`
- `docs/architecture.md`
- `docs/decisions/0001-winuhid-source.md`
- `docs/development-plan.md`
- `docs/process/README.md`
- `docs/process/virtual-mouse-v1.md`

## 外部副作用

- 无驱动、系统设置、服务或后台进程变更。
- 为源码比较创建的临时克隆已删除。
- 已添加固定 WinUHid submodule 并生成工作区内 Release 构建产物。
- 驱动由用户自行安装；程序和本开发过程均未安装或修改驱动、服务或系统设置。
- 用户已从提升终端运行 doctor、probe、once-click 和 once-move；日志保存在仓库
  根目录。没有后台进程。

## 开放技术事实

1. RDP 样本可被完整观察，但低级钩子 flags/extraInfo 均不足以即时分类；Raw
   Input 显示绝对虚拟桌面移动，仍需确定持续模式的可靠关联策略。
2. WinUHid 输出回声在低级钩子和 Raw Input 中的顺序及可识别性。
3. 修订后的捕获后延时能否让用户完成 zzz 移动及 Codex 点击端到端验证。

这些问题由被动 probe 解决，不需要当前用户提供新的产品决策。

## Resume Checkpoint

- Pause reason: 修订后的 C-Once 已构建，等待用户提升环境实测。
- User intent at pause: 完成任务 B 和 C-Once，交付可供实机测试的命令与日志。
- Risk level: HIGH。
- Last completed task: 根据用户反馈将 C-Once 改为捕获后停止观察、延时后重放。
- In-progress task: 无；等待人机端到端验证。
- Current artifact state: A/B/C-Once 已实现；Debug/Release 构建和自动测试通过。
- Changes already made: 见“已变更工件”。
- Verification completed: Debug/Release 构建均为 0 错误；两套 CTest 均为 2/2；doctor 驱动接口、虚拟
  鼠标创建/销毁通过；RDP probe 无丢事件；旧版 once 后端发送成功。
- Verification pending: 修订版 replay-delay 的 zzz/Codex 人工端到端验证、滚轮、
  触控保持和持续模式来源/回声策略。
- External side effects already executed: 用户预装驱动和运行前台测试；仓库构建产物。
- Open processes or temporary state: 无。
- Current gate states: A/B/C-Once 的 G0-G4 READY；持续 C/D 仍 BLOCKED。
- Invalidated gates: 无。
- New or unresolved questions: 见“开放技术事实”。
- Resume prerequisites: 用户从管理员 PowerShell 7 运行修订版 C-Once 并保留日志。
- Exact next safe action: 使用 `--replay-delay 3` 复测 move、click 和 wheel。

## 状态历史

- 2026-09-28：`PREPARING`。完成需求整理、架构草案、依赖选型和条件开发合同；
  等待用户审阅。
- 2026-09-28：完成文档链接和 Git diff 静态校验；无实现或系统副作用。
- 2026-09-28：按用户反馈将 README 从文档索引改为产品与使用说明；架构内容
  继续保留在 `docs/architecture.md`。
- 2026-09-28：用户授权按计划开发，新增可选 JSONL 调试日志和 C-Once 单动作
  实测要求；状态进入 `EXECUTING`。
- 2026-09-29：用户实机日志确认 WinUHid 驱动、RDP 被动观察和旧版单样本发送
  调用成功；旧版测试无法提供充分目标准备时间。
- 2026-10-03：用户明确 C-Once 应在捕获和重放之间提供准备延时，期间忽略其他
  输入；正式模式仍逐报告低延迟转发。实现 `--replay-delay`，Debug/Release
  构建和两套 CTest 2/2 通过，状态为 `VERIFICATION_PENDING`。
