# T05：GAS 换弹、备用弹药与中断

日期：2026-09-25<br>
项目：`D:\UE5_My_Project\MyShoot 5.5`<br>
状态：完成；常规换弹与 T06 玩家死亡中断已获用户验收，空弹匣且有备用弹药的规则由自动测试验证。<br>
基线：develop / `9a9874b`；用户已授权与 T06 一同提交并推送，提交号以 Git 历史为准。

## 1. 本次行为

- R 键请求换弹，松开 R 不取消。
- 默认弹匣容量 30、备用弹药 90、换弹时间 1.5 秒；在 Shooter 的 ShooterWeapon 组件默认值中调整。
- 满弹匣、无备用弹药、死亡、正在换弹时拒绝请求，不播放新的换弹动画。
- 换弹开始即取消连射，换弹期间拒绝射击；完成后需要重新按下开火键。
- 只有正常计时完成才转移弹药：`转移量 = Min(弹匣容量 - 当前弹药, 备用弹药)`。
- 主动取消、死亡、组件移除、退出关卡均清理定时器；未完成时不补弹、不扣备用弹药。
- 动画只负责表现，没有使用 AnimNotify 再补一次弹药。

本阶段仍是单机玩法。ServerOnly 延续 T04 的执行策略，本次不宣称已完成多人复制或客户端预测。备用弹药没有拾取/补给入口，留给后续需求。

## 2. C++ 职责

| 文件 | 职责 |
| --- | --- |
| Public/GAS/Abilities/ShooterReloadAbility.h | 声明换弹能力和能力内部状态 |
| Private/GAS/Abilities/ShooterReloadAbility.cpp | 持有 State.Reloading、启动计时、统一完成/取消 |
| Public/Weapons/ShooterWeaponComponent.h | 换弹接口、配置、备用弹药和表现事件 |
| Private/Weapons/ShooterWeaponComponent.cpp | 授予能力、判断条件、原子转移弹药、发布通知 |
| Private/Tests/ShooterGASReloadTest.cpp | 真实射击消耗后的换弹与中断测试 |

以上路径相对 `Source/MyShoot`。继续遵守 .h 放 Public、.cpp 放 Private、关键流程写中文注释。

### 可调用接口

- `StartReloading()`：请求能力激活，bool 表示请求是否接受。
- `CancelReloading()`：取消当前换弹，后续切枪等行为可复用。
- `CanReload()` / `IsReloading()`：读取是否可换弹 / 当前标签状态。
- `GetCurrentAmmo()` / `GetMagazineCapacity()` / `GetReserveAmmo()` / `GetReloadDuration()`：只读数据。
- `OnReloadStarted(Duration)`：成功开始一次换弹时通知表现。
- `OnReloadFinished(bSucceeded)`：本次完成或取消后的通知；true 代表已完成补弹。
- `OnAmmoChanged` / `OnReserveAmmoChanged`：弹药实际变化后通知，供后续 HUD 订阅。

组件退出时停止广播表现事件，销毁路径无需再播放收尾动画。正常完成时先同时更新两个弹药值，再移除能力状态并发布数据通知，避免监听者看到只更新一半的数据。状态清理和数据通知期间暂时拒绝新输入，最终结束通知后允许下一次操作。

## 3. Shooter 蓝图连接

在 EventGraph 右侧新增带中文注释的 T05 区域。现有射击、移动、死亡和旧蓝图对比函数保留。你的弹药打印节点也保留，换弹绑定只插入初始化分支，不会每发重新绑定。

```text
InputAction reload.Pressed
    → GetShooterWeapon → StartReloading

BeginPlay
    → 原 Bind OnShotFired
    → Bind OnReloadStarted
    → Bind OnReloadFinished
    → 原子弹剩余数打印

HandleReloadStarted(Duration)
    → Play Anim Montage：Shooter_reload_GAS_Montage
       InPlayRate = 蒙太奇 GetPlayLength / Duration

HandleReloadFinished(bSucceeded)
    → Stop Anim Montage：仅指定 Shooter_reload_GAS_Montage
```

结束事件不需要根据 bSucceeded 再计算弹药；C++ 已完成结算或取消。指定具体换弹蒙太奇停止，可以避免误停其他蒙太奇，尤其是死亡表现。

使用已有 `/Game/Assets/Characters/FPP_Animations/FPP_RifleReload`，创建 `/Game/Blueprints/Shooter_reload_GAS_Montage`。原动画资产不修改；迁移工具已核实骨架和 Slot 匹配现有 Shooter / 开火蒙太奇。

不要把旧开火循环、扣弹、Apply GAS Damage 或动画通知补弹接到换弹事件中。

## 4. 自动检查与证据

已验证：

- Development Editor / Win64 编译通过；VS2022 工程文件已重新生成。
- 7 个项目蓝图编译通过：0 错误、0 加载失败；6 条既有动画蓝图警告保留。
- GAS 自动测试 5/5 通过：Foundation、Damage、Death、Fire、Reload。
- 保存后真实 Shooter 输入检查通过：R 键、满弹拒绝、换弹动画和速率、重复 R 不重启、开火互斥、到期补弹、取消及死亡清理。
- T04 实际蓝图射击回归通过：一发扣一弹、一次粒子/蒙太奇、松开停止、旧循环未激活、死亡取消。
- 原有 92 个节点全部保留，新增 15 个节点（包含注释框）。旧节点坐标、注释、默认值及旧函数内部连接均未修改；仅三个原引脚因插入 BeginPlay 绑定和复用组件引用而调整连线。
- 换弹序列、手臂和开火蒙太奇骨架均为 HeroFPP_Skeleton，使用 DefaultSlot。换弹蒙太奇原始时长约 1.667 秒，按 ReloadDuration 自动调整播放速度。
- 计时测试按小帧推进并初始化首帧时钟，覆盖到期前不补弹、重复按键不重置、取消后无延迟补弹。

测试日志、蓝图导出和备份位于 `Saved/T05`，由 .gitignore 忽略。

- 备份：`Saved/T05/BeforeReload-20260925-182408.zip`。
- C++ 构建：`Saved/T05/BuildFinal.log`。
- GAS 自动测试：`Saved/T05/AutomationReport/index.json`。
- 蓝图迁移：`Saved/T05/Migrate.log`。
- 保存并独立重新加载后的验证：`Saved/T05/VerifySaved.log`。
- 真实蓝图运行验证与蒙太奇时长同步：`Saved/T05/Verify.log`。
- 蓝图编译：`Saved/T05/CompileBlueprints.log`。
- 接线导出：`Saved/T05/BlueprintWiring/After/graphs.json`。

编辑器工具只在 Editor 模块中，代码位于 `Plugins/ShooterEditorTools/Source/ShooterEditorTools/Public` 和 `Private`。迁移只允许指定的 Shooter 资产，并拒绝重复生成节点或覆盖已有换弹蒙太奇。

## 5. 你需要完成的 PIE 验收

打开 UE5.5 项目进入原地图，先确认没有蓝图编译错误。需要读取弹药时，在 PIE 中按 F8，选中运行时玩家的 ShooterWeapon 组件查看 Current Ammo / Reserve Ammo；不要只查看蓝图类默认对象。HUD 显示属于 T07。

| 操作 | 预期 |
| --- | --- |
| 满弹匣时按 R | 不换弹、不播换弹动画 |
| 开几枪，再按 R | 换弹动画播放一次，到期补弹，备用弹药扣相同数量 |
| 换弹期间反复按 R | 动画不重启，换弹不延长或重复补弹 |
| 换弹过程中点击/按住开火 | 不射击、不扣弹、不播开火特效 |
| 连射过程中按 R | 立即停止连射，开始换弹 |
| 完成后重新按开火 | 正常射击；不会自动续上之前按住的连射 |
| 换弹中死亡 | 动画/规则取消，死亡流程正常，不延迟补弹 |
| 打空后按 R | 有备用弹药时能换弹，完成后可重新射击 |
| 退出并重新 PIE | 初始弹匣/备用弹药恢复，无旧计时器或重复绑定 |

数值验收可在组件默认值分别设 `Initial Reserve Ammo = 50` 和 `5`，每次重新 PIE，打掉 20 发后换弹：

- 容量 30，弹匣 10，备用 50 → 完成后弹匣 30，备用 30。
- 容量 30，弹匣 10，备用 5 → 完成后弹匣 15，备用 0；再次按 R 无效。

验收后把默认备用弹药恢复为 90 并保存。取消接口、死亡中断和弹药数值会先由自动测试覆盖，场景中的动画观感仍需你确认。

反馈模板：

```text
R 键换弹：
动画播放和时长：
完成后弹药 / 备用弹药：
重复按 R：
换弹期间开火：
打空后换弹：
取消 / 死亡中断（已测试则填写）：
重新 PIE：
报错：
```

T05 通过后再进入 T06 基础 AI 战斗；本次不提前标记 T05 人工验收完成。


## 6. 用户 PIE 反馈（2026-09-25）

用户已确认：

- R 键换弹、动画播放与时长、完成后的弹匣与备用弹药正常。
- 重复按 R 不会重复换弹，换弹期间不能开火。
- 重新 PIE 正常，无报错。

待澄清：“打空后换弹：无法换弹，正常”没有说明备用弹药是否也为零。
正确规则是：弹匣为 0 且备用弹药大于 0 时允许换弹；备用弹药为 0 时拒绝换弹。
确认之前不把这一项记为通过，也不改动现有规则。

玩家死亡验收说明：用户反馈当前游玩中尚未看到 Shooter 死亡，因此没有人工验收死亡中断。
AMyShooter 已继承 ShooterCharacterBase 的公共 GAS 死亡流程；血量归零会取消所有能力、
停止玩家输入并清理角色。已有原生测试和保存后的真实 Shooter 蓝图测试均验证了换弹中受到
致命 GAS 伤害后取消、不延迟补弹。场景内“敌人攻击 → 玩家扣血 → 死亡中断”的人工验收
随 T06 攻击入口接通后补验；失败界面与重开属于 T07。本项不记作用户已人工通过。

本次仅记录反馈与核对代码、已有测试报告，未提交或推送 Git。
## 7. T06 联动补验（2026-09-26）

用户授权进入 T06。已在自动测试中通过真实射击将弹匣打空，验证弹匣 0 / 备用 90 时按规则换成 30 / 60；新增 AI 测试验证敌人在换弹期间击杀玩家后，换弹取消且后续不会延迟补弹。当前整套 GAS 测试 7/7 通过，报告见 Saved/T06/AutomationReport/index.json。

这补充了空弹条件和 AI 伤害入口的自动证据，不代表此前用户反馈中的备用弹药条件已经得到澄清。场景动画、玩家死亡输入停止与换弹中断请随 [T06 人工验收](T06_AI_AND_PLAYER_HEALTH_UI.md) 复核。
## 8. 用户死亡中断补验与阶段结论（2026-09-26）

用户在 T06 原地图 PIE 中确认玩家死亡 / 换弹中断正常，重新 PIE 正常，无报错，并授权提交远端。结合此前常规换弹人工验收和现有自动测试，T05 记为完成。

证据边界：弹匣 0 / 备用 90 → 30 / 60 已通过自动测试；此前“打空后无法换弹”的备用弹药条件没有新增人工说明，不追溯记为该条件的人工验收通过。
