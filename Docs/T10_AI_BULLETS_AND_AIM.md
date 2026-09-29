# T10：AI 可见子弹与玩家切换瞄准

日期：2026-09-28。状态：用户于 2026-09-28 确认没有问题，人工验收通过；未提交 Git。

## 1. 本次行为

- 玩家按一次鼠标右键进入瞄准，松开保持，再按一次退出。
- 普通视角沿用原十字准星；开镜后隐藏十字，在屏幕中心显示直径 6 个 UI 单位的小红点，退出恢复十字。暂停、菜单和死亡时隐藏准星。
- 使用项目已有 FPP_RifleAim 姿势，动画权重与摄像机 FOV 在 0.18 秒内同步过渡；瞄准 FOV 默认 65，退出时恢复启动时的原始值。
- 瞄准开火使用 FPP_RifleAimFire 的副本及 AM_PlayerAimFire 蒙太奇。副本去掉原有 PlaySound 通知，枪声仍由武器成功发射入口播放一次；原资源不修改。
- 换弹、死亡、暂停、返回菜单和失去控制时退出瞄准；恢复游戏或换弹完成不会自动重新开镜。
- AI 每次有效攻击从 Rifle 下的 Muzzle 生成现有 BP_ShooterBulletVisual。与攻击动画、枪声使用同一触发点。
- 子弹只表现飞行，无碰撞、无伤害。原有 AI GAS 伤害仍在有效攻击时结算一次；这不是按弹丸到达时间结算的物理弹道。

## 2. C++ 与蓝图分工

| 文件/资源 | 职责 |
| --- | --- |
| Public/Weapons/ShooterAimComponent.h、Private/Weapons/ShooterAimComponent.cpp | 开镜状态、State.Aiming 标签、FOV/模型过渡、换弹/死亡/菜单复位 |
| Public/Animation/ShooterPlayerAnimInstance.h、Private/Animation/ShooterPlayerAnimInstance.cpp | 给动画图提供 ShooterAimAlpha |
| Public/Characters/MyShooter.h、Private/Characters/MyShooter.cpp | 创建 ShooterAim，右键 Pressed 调用 ToggleAiming；没有 Released 退出绑定 |
| Private/AI/ShooterAIController.cpp | 成功攻击时生成纯表现子弹；枪口到目标的可见路径遇墙截断 |
| Public/UI/ShooterAimReticleWidget.h、Private/UI/ShooterAimReticleWidget.cpp | 绘制屏幕中心小红点；玩家控制器订阅开镜状态，统一切换原十字与红点 |
| Content/Blueprints/Shooter_idle | 父类改为 ShooterPlayerAnimInstance，原移动姿势与 FPP_RifleAim 混合后进入原 DefaultSlot |
| Content/Blueprints/Shooter | 保留原开火执行链，蒙太奇参数通过 Get Shooter Aim → Get Fire Montage 选择 |
| Content/Blueprints/Boot_Shooter_BP | 在原 Rifle 下增加 Muzzle，复用同一 Rifle 模型上玩家已校准的枪口局部位置 |
| Content/Blueprints/Boot_Shooter_controller | 指定现有 BP_ShooterBulletVisual；保留用户设置的攻击参数 |

所有新增头文件和实现分置 Public/Private，相同职责使用对应子目录，关键流程附中文注释。

## 3. 编辑器中可以调整的位置

打开 Shooter，选中继承的 **ShooterAim** 组件：

| 参数 | 当前用途 |
| --- | --- |
| Aim Field Of View | 开镜 FOV，默认 65 |
| Aim Transition Duration | 切换过渡时间，默认 0.18 秒 |
| Aim Mesh Offset | 当前已校准为 X=25、Y=-0.2、Z=20 cm，适配现有 Camera 与 FPP 动画；只在开镜时混合应用，退出恢复原值 |
| Hip Fire Montage / Aim Fire Montage | 普通和瞄准时的开火动画 |

打开 Boot_Shooter_controller 的 Class Defaults，在 AI 表现参数中调整 Bullet Visual Class / Speed / Scale。当前速度 6000 cm/s、整体缩放 1.5；近距离弹道很短，表现子弹保留既有最短飞行时间规则。

输入配置：Project Settings → Input → Action Mappings → aim → Right Mouse Button。开镜状态由 C++ 维护；无需在蓝图再增加一套右键翻转逻辑。

## 4. 保留与对比

- 改动前资源备份：Saved/T10/Backup；SHA256 清单：Saved/T10/Before/Hashes.csv。
- 蓝图节点快照：Saved/T10/Before；完成后快照：Saved/T10/After。
- 原 FPP_RifleAim、FPP_RifleAimFire、移动节点、旧普通蓝图射击函数、敌人死亡动画分支保留。
- 节点 GUID 对比：Shooter 107 → 109、Shooter_idle 7 → 10、Boot_Shooter_BP 37 → 37，原节点删除数量均为 0。Start_fire / End_fire / Shoot_Once 的节点与连接内容完全一致。
- Boot_Shooter_AnimationBP 与改动前备份的 SHA256 一致，本次未修改敌人动画图。
- Shooter_idle 仅在原移动结果与 DefaultSlot 之间增加姿势混合；Shooter 的 GAS_PlayFireEffects 仅更换蒙太奇选择参数。
- 资源接线工具为 C++ 编辑器命令 ShooterAimWiring。默认迁移遇到已有 T10 资源会拒绝覆盖；-Verify 只读验证，-Calibrate 仅用于明确的瞄准偏移校准。
- 项目原有用户修改、地图、个人学习笔记和 T08 未提交工作保留。本次不重建 T08 旧打包产物。

## 5. 验证与边界

- UE5.5 MyShootEditor Development 构建通过。
- 保存后的蓝图配置验证通过；最终 12 项 MyShoot 自动测试全部通过（0 失败、0 未运行）。报告：Saved/T10/FinalAutomationReport/index.json。
- 实际 bloodstrike 关卡通过真实右键 Pressed/Released 验证切换开镜、开火选用瞄准蒙太奇、扣一发弹药、换弹/暂停/死亡退出、抬头跟随、AI 子弹生成与单次伤害。
- 已查看普通、瞄准、AI 子弹和俯仰截图，瞄具与画面中央准星对齐。实际记录：Saved/T10/FinalSmoke2/AimSmoke.txt，截图在同目录。
- 实际验证发现并修复了 HUD 初始化先后顺序问题：武器初始化弹药后广播现有弹药事件，使提前订阅的 HUD 正确显示 30 / 090；实际启动检查已覆盖。

实际关卡验证使用显式参数 -ShooterAimSmoke；普通启动不会注册测试 Tick，也不会暂停 AI 或改动玩家位置。测试截图和日志在 Saved/T10 下，不纳入 Git。

既有动画图线程安全警告、Rifle 资源切线提示不属于本次新增问题。当前使用原始射线命中与 GAS 伤害规则；开镜不改变伤害值，也不增加屏幕瞄准镜遮罩。

## 6. 用户 PIE 验收

请从 MyShoot.uproject 打开当前编辑器版本，点击开始游戏后检查：

```text
AI 枪口发出可见子弹：
AI 每次扣血 / 子弹消失：
右键一次进入 / 松开保持 / 再按退出：
开镜小红点 / 退出恢复十字准星：
瞄准枪械位置 / 抬头低头跟随：
瞄准中单发 / 连射 / 枪声：
换弹退出瞄准 / 换弹完成保持普通视角：
暂停与继续 / 玩家死亡恢复视角：
初始弹药 HUD / 扣弹与换弹同步：
重新 PIE：
报错：
```

后续提交应在用户验收确认后执行；Saved、Intermediate、Binaries、截图和打包目录继续由 .gitignore 忽略。

## 开镜红点补充（2026-09-28）

按用户要求，新增原生 ShooterAimReticleWidget，并由开镜状态事件驱动原十字/小红点切换。保留原 Shooter_UI 资产与节点。编译通过，复用实际关卡检查通过；已查看普通、开镜和抬头截图，确认普通视角仍显示十字，瞄准中心仅显示小红点。记录与截图：Saved/T10/RedDotSmoke。
