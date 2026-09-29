# T12 AI 随机巡逻与移动动画

日期：2026-09-29
当前状态：用户确认 AI 自主巡逻及巡逻速度完成，并授权提交推送；本次提交包含随机巡逻、速度与移动动画修正。

## 1. 有效基线与用户反馈

上一步已推送远端 develop：`c17e3d5001767fa87a5957c6e079e6f3c119999b`，提交说明为“完成 T10-T11：AI可见子弹、玩家开镜与视野远程交战”。本轮巡逻已获用户验收与提交推送授权，个人笔记 `Shooter项目中的关于宏问题.md` 不纳入任务。

用户反馈初版存在两个问题：多个 AI 像一起移动，巡逻时移动动画不明显或没有播放；其余行为用户确认没有问题。因此本轮只修正巡逻随机性、移动动画及相应验证，保留已有战斗、死亡、开镜与界面逻辑。

## 2. 当前巡逻行为

- 默认开启随机巡逻，不再按相同节奏顺序走三点路线。
- 每个敌人持有独立随机流，以自己的出生位置为中心，在半径 650 cm 内选择可到达的 NavMesh 目标。
- 启动时独立等待 0.2～2.5 秒；到点后独立停留 0.8～2.8 秒，再选择下一处目标。
- 目标与当前位置至少相隔 200 cm，避免不断选到脚下而看起来没有移动。
- 巡逻速度仍为 180 cm/s；发现玩家后中断巡逻并恢复原交战速度。
- 丢失目标后继续尚未完成的随机目标；暂停、死亡、菜单及对局结束时停止活动。
- 没有 NavMesh 时保持待机；有导航但暂时无法采样到可达位置时，等待 1 秒再尝试。
- 随机选择不保证每次起步时刻绝不重合，也不强制敌人永远互相远离；它们没有共享目的点或统一出发指令。

初版放置在地图中的 `Patrol_` 路线保留供对比，默认随机模式不使用它们。关闭 Random Patrol 后，仍可恢复固定路线的闭环或折返巡逻。

## 3. 移动动画修正

检查发现，原 `Boot_Shooter_BS` 只包含：

| 速度 | 原采样 |
| --- | --- |
| 0 | Idle |
| 600 | Run_Fwd |

180 cm/s 时只混入约 30% 跑步姿势，低速移动幅度较弱。现在新增 `/Game/Animations/BS_AI_Locomotion`，保留原混合动画资源：

| 速度 | 当前采样 |
| --- | --- |
| 0 | 原 Idle |
| 180 | 原 Run_Fwd 的完整姿势，采样播放倍率 0.45 |
| 600 | 原 Run_Fwd，原播放倍率 1.0 |

现有资源没有独立 Walk 动画，本轮使用现有跑步动作放慢播放；不会把它描述为新制作的走路动画。后续若有合适的持枪行走资源，可以替换 180 速度的采样。

`Boot_Shooter_AnimationBP` 的 Alive 分支改用新 BlendSpace，循环播放开启；原速度连接、攻击 Slot、死亡分支和旧节点保留。未覆盖源 Run_Fwd 动画。

## 4. 编辑器调整方法

打开 `Boot_Shooter_controller`，选择继承的 `ShooterPatrol` 组件，在默认值中调整：

| 参数 | 默认值 / 含义 |
| --- | --- |
| Patrol Enabled | 开启巡逻 |
| Random Patrol | 开启独立随机模式 |
| Patrol Radius | 650，单位 cm |
| Patrol Speed | 180，单位 cm/s |
| Min Wait Duration / Max Wait Duration | 0.8 / 2.8 秒 |
| Acceptance Radius | 60 cm，到达目标的容差 |

巡逻中心在控制器接管角色时记录，不随交战追踪持续移动。默认地图已具备 NavMesh，新增同类敌人无需额外设置路线即可随机巡逻。

需要对照固定路线时，关闭 Random Patrol，再在 World Outliner 中搜索 `Patrol_`。路线的 Assigned Enemy 指定所属敌人；PatrolSpline 点可移动；Closed Loop 控制循环或折返，Wait Duration 控制固定模式到点停留时间。

## 5. 代码与资源分工

| 位置 | 功能 |
| --- | --- |
| Source/MyShoot/Public/AI/ShooterPatrolComponent.h、Private/AI/ShooterPatrolComponent.cpp | 独立随机流、区域采样、随机等待、移动请求与中断恢复 |
| Source/MyShoot/Public/AI/ShooterPatrolRoute.h、Private/AI/ShooterPatrolRoute.cpp | 保留的可编辑固定路线模式 |
| Source/MyShoot/Public/AI/ShooterAIController.h、Private/AI/ShooterAIController.cpp | 原战斗决策与巡逻切换 |
| Source/MyShoot/Public/Tests/ShooterPatrolSmokeSubsystem.h、Private/Tests/ShooterPatrolSmokeSubsystem.cpp | 实际地图三敌人随机巡逻、骨骼姿势及战斗切换验收 |
| ShooterEditorTools 的 ShooterPatrolSetupCommandlet | 路线配置、动画资源检查与一次性修复 |
| Content/Animations/BS_AI_Locomotion.uasset | 含低速巡逻采样的新混合动画 |
| Content/Blueprints/Boot_Shooter_AnimationBP.uasset | Alive 分支使用新混合动画 |

继续采用 UE C++，Public/Private 分离并保留中文注释。巡逻由原 AI 低频决策驱动，没有额外逐帧寻路；移动完成回调只处理自己的请求，战斗取消不会误记为到点。

## 6. 验证记录

初版固定路线检查记录仍在 `Saved/T12/MapSmoke`；这些是历史结果，不代替本次随机巡逻复验。

本轮真实 bloodstrike 地图专项通过，日志位于 `Saved/T12/RandomFix/MapSmoke/Game.log`，结果位于同目录 `Result.txt`：

- 同时运行地图中 3 个真实敌人，记录不同目标及不同起步节奏。
- 三个敌人的腿部骨骼在移动时分别记录到 692、769、583 次明显旋转变化，确认有实际姿势播放。
- 主测试敌人移动 1569.1 cm，检查 5 次随机等待。
- 暂停/恢复、自动发现玩家后造成 10 点伤害、恢复 600 cm/s 交战速度、丢失目标后恢复巡逻、死亡停止均通过。
- 骨骼验证在测试模式中强制离屏更新，防止测试摄像机不看敌人而跳过动画评估；普通游戏继续使用原有网格更新配置。

专项只在显式传入 `-ShooterPatrolSmoke` 时启动，临时移动玩家，先同时观察三个敌人，再隔离额外攻击者验证单次伤害。测试世界不保存回地图，普通游戏不启用测试 Tick。

动画检查/修复工具使用参数 `-run=ShooterPatrolSetup -AnimationInspect` 或 `-AnimationApply`。动画蓝图修复前的备份位于 `Saved/T12/RandomFix/Backup`。运行游戏不需要再次执行该工具。

最终 UE5.5 MyShootEditor Development 编译通过（Saved/T12/RandomFix/BuildFinal.log）；原有 13 项回归全部成功，失败 0、未运行 0（Saved/T12/RandomFix/AutomationReport/index.json），最终回归与地图专项日志未发现 Error 或 Ensure。测试进程均已退出。自动测试不代替动作观感的人工确认，旧 T08 打包程序也不包含这些改动。

## 7. 本轮人工复验

用 UE5.5 打开 MyShoot.uproject，在 bloodstrike 开始游戏。先躲在掩体后观察，避免敌人直接进入交战。

```text
三个 AI 各自选择随机目标：
起步 / 停留不再统一：
巡逻时腿部移动动画：
停下后恢复待机：
发现玩家后的追踪 / 射击：
死亡动画与清理：
重新 PIE：
报错：
```

用户此前确认其余功能没有问题。此次重点是随机巡逻与移动动画的观感，原战斗链只做回归确认。

## 8. 交接状态

初版曾评估为适合交接，随后用户给出上述两项修正并要求继续工作。当前先收拢修正与人工复验，同阶段不重复提出交接建议。仍未执行保存交接材料或新建对话。
## 9. 用户验收与提交授权（2026-09-29）

用户确认“完成了 AI 的自主巡逻还有自主巡逻的速度”，授权推送远端。此前反馈的独立随机巡逻、速度及移动动画修正纳入本次 T12 提交；不把这次整体确认扩写为未逐项回填的测试数据。

已有自动验证：UE5.5 编译通过，13 项回归全部成功，三敌人真实地图随机巡逻与腿部动画专项通过。开始菜单资产的额外本地修改及个人笔记不属于本轮巡逻提交，保留用户工作。生成文件仍由 .gitignore 忽略。