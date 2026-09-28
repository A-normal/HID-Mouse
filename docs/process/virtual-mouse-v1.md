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
status: PREPARING
plan_version: 0.1
checkpoint_sequence: 3
created_at: 2026-09-28T15:57:08+08:00
updated_at: 2026-09-28T16:07:40+08:00
evidence_valid_as_of: 2026-09-28
```

# Virtual Mouse v1 流程台账

## 当前方向与基线

- 用户目标：建立与目标应用无关的 Windows 输入兼容层。
- 候选需求基线：[`../requirements.md`](../requirements.md)。
- 架构：[`../architecture.md`](../architecture.md)。
- 开发合同：[`../development-plan.md`](../development-plan.md)。
- 依赖决策：[`../decisions/0001-winuhid-source.md`](../decisions/0001-winuhid-source.md)。
- 当前范围：只准备文档；没有实施授权。

## 风险与 Gate

- 风险：`HIGH`。
- 触发因素：管理员权限、全局输入阻止、故障恢复、来源识别不确定性。
- Gate 0：`READY`。
- Gate 1：`READY`，等待用户对整理文档的整体确认。
- Gate 2：完整转发 `BLOCKED`，等待被动 probe 运行证据。
- Gate 3：Increment A/B `READY`；C/D `BLOCKED`。
- Gate 4：Increment A/B `READY`；C/D 为条件合同。

## 物料决策

1. 选择官方 `cgutman/WinUHid`，计划固定提交
   `d6cebbef5c7909168d1f881185be8f607d6aefd4`。
2. lurebat fork 相对官方多 17 个提交，但与核心 SDK、鼠标 helper 和驱动接口
   相关的比较文件无差异，因此不选作依赖。
3. 不打包、不安装、不更新驱动；只检测预装驱动。
4. 不启动后台进程、服务、GUI 或托盘。
5. 项目 README 面向使用者介绍问题、场景、使用方式、要求和边界；架构分析和
   开发 Gate 留在 `docs/`。

## REQ -> TASK -> AC -> VAL

详细映射位于 [`../development-plan.md`](../development-plan.md)。当前可执行范围
仅为 TASK-A1..A3 和 TASK-B1..B3，且仍需用户确认文档后才获得实施授权。

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
- 未添加 Git submodule，未开始应用实现。

## 开放技术事实

1. RDP 和 Sunshine 输入能否在低级钩子返回前可靠分类。
2. WinUHid 输出回声在低级钩子和 Raw Input 中的顺序及可识别性。

这些问题由被动 probe 解决，不需要当前用户提供新的产品决策。

## Resume Checkpoint

- Pause reason: 等待用户审阅并确认开发文档。
- User intent at pause: 文档确认后开始实施。
- Risk level: HIGH。
- Last completed task: WinUHid fork 对比和候选开发合同落盘。
- In-progress task: 无。
- Current artifact state: 仅文档变更，未实现代码。
- Changes already made: 见“已变更工件”。
- Verification completed: fork/upstream 静态差异、6 份 Markdown 文档内部链接、
  `git diff --check`。
- Verification pending: 用户审批、实现构建和所有运行时验证。
- External side effects already executed: 无持久外部副作用。
- Open processes or temporary state: 无。
- Current gate states: G0 READY；G1 READY 待整体确认；G2-G4 见上文。
- Invalidated gates: 无。
- New or unresolved questions: 两项运行时事实，交由 probe。
- Resume prerequisites: 用户明确确认本组文档并授权实施。
- Exact next safe action: 复核实际仓库 diff，随后等待用户确认；获批后从 TASK-A1 开始。

## 状态历史

- 2026-09-28：`PREPARING`。完成需求整理、架构草案、依赖选型和条件开发合同；
  等待用户审阅。
- 2026-09-28：完成文档链接和 Git diff 静态校验；无实现或系统副作用。
- 2026-09-28：按用户反馈将 README 从文档索引改为产品与使用说明；架构内容
  继续保留在 `docs/architecture.md`。
