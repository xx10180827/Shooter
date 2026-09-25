# T04 单武器状态与 GA_Fire

项目：D:\UE5_My_Project\MyShoot 5.5<br>
日期：2026-09-24<br>
前置：用户已确认 T03 验收正常。<br>
状态：C++ 构建通过，4 个 GAS 自动测试成功、0 失败、0 测试警告；现有 7 个蓝图编译通过。2026-09-25 已按用户要求直接完成 Shooter 蓝图接线，并通过真实输入和表现触发验证。2026-09-25 用户确认全部验收项目正常、无报错，T04 完成。

## 1. 本次职责划分

```text
Shooter 蓝图输入
    ├─ 按下 → ShooterWeapon.StartFiring
    └─ 松开 → ShooterWeapon.StopFiring
                    ↓
        UShooterFireAbility（GA_Fire 原生实现）
        激活判断 / 连射定时器 / EndAbility 清理
                    ↓
        UShooterWeaponComponent
        武器配置 / 当前弹药 / 射速时间戳 / 单次射线
                    ↓
        ApplyGASDamage → 瞬时 GE → GAS Health → T03 死亡
                    ↓
        OnShotFired → 蓝图开火动画、音效、特效
```

- 当前只有一把武器，使用组件承载配置和状态，不另建一套武器 Actor。
- MyShooter 默认创建 ShooterWeapon，组件 BeginPlay 自动向 ASC 授予 UShooterFireAbility。
- 不需要另建 GA_Fire 蓝图或手动 GiveAbility，也不要重复添加第二个武器组件。
- 敌人当前仍继承 ShooterCharacterBase，既有 T03 死亡表现连接继续保留。
- 当前只支持单机权威执行；未实现多人预测、武器切换、后坐力、霰弹枪或换弹补弹。

## 2. 配置与规则

打开 Shooter 蓝图，确认父类仍为 MyShooter，选中继承的 ShooterWeapon 组件，在 Shooter > Weapon 分类修改：

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| Damage | 25 | 每次命中的伤害 |
| Fire Interval | 0.1 | 两发之间的最短时间，单位秒 |
| Range | 10000 | 视线射程，单位厘米 |
| Magazine Capacity | 30 | 弹匣容量，新角色以满弹匣开始 |
| Trace Channel | Visibility | 命中检测通道，目标需阻挡该通道 |

运行时使用 Get Current Ammo / Get Magazine Capacity 查询，不继续读写旧蓝图弹药变量。

射速时间戳保存在武器组件中。反复松开、按下不能重置它；高帧间隔时不会在一帧补发多发子弹。打空也消耗一枚弹药。空弹匣立即结束能力，不继续播放开火表现或造成伤害。

State.Dead 阻止激活；死亡时取消能力。State.Reloading 阻止激活并立即取消正在进行的连射，移除标签不会自动恢复射击。T04 只提供互斥规则，真正的 GA_Reload 和备用弹药属于 T05。测试弹匣打空后，停止并重新 PIE 即可恢复。

## 3. 编辑器操作：先接输入

**2026-09-25：以下输入和表现接线已由工具完成，无需重复绑定。用户已完成验收，结果见第 9 节；新旧节点位置见 [蓝图接线与旧节点对比](T04_BLUEPRINT_WIRING.md)。**

1. 用本次编译后的项目打开编辑器，打开 Shooter，检查继承的 ShooterWeapon 已出现。
2. 找到原来鼠标左键或开火输入事件，记录并断开旧射击入口。
3. 从 Get Shooter Weapon 的返回值拉线，调用组件方法：

```text
原开火输入 Pressed  → Get Shooter Weapon → Start Firing
原开火输入 Released → Get Shooter Weapon → Stop Firing
```

保留项目现有输入系统即可。如果你已使用 Enhanced Input，则 Started 对应 Start Firing，Completed 和 Canceled 都对应 Stop Firing；不要在每帧 Triggered 中重复接原射击循环。

4. 原蓝图中的连射 Timer、Delay 循环、射线、Apply GAS Damage 和扣弹逻辑都退出玩家这条开火调用链，避免与 C++ 重复运行。
5. 模型、摄像机、移动、跳跃和 AnimBP 配置继续沿用。

## 4. 编辑器操作：接表现事件

在 Shooter 的 BeginPlay 中，从 Get Shooter Weapon 拉出：

```text
Bind Event to On Shot Fired
    Event → 自定义事件 HandleWeaponShot
            参数：Blocking Hit（bool）、Hit Result
```

创建签名匹配的事件可使用 Bind 节点 Event 引脚的“添加自定义事件”。只在 BeginPlay 绑定一次，连接到现有 BeginPlay 执行链，不丢失原初始化逻辑。

在 HandleWeaponShot 中连接原有的：
- 枪口火焰、枪声、弹壳。
- 武器或手臂开火动画。
- Blocking Hit 为 true 时的命中特效；位置、法线取 Hit Result。
- 弹道终点：命中取 Impact Point，未命中取 Trace End。

这个事件表示一发已经完成结算，不能再连接射线伤害、Apply GAS Damage 或扣弹逻辑。如果旧 Fire 函数混合了这些内容，先把表现节点拆成独立函数，再从 HandleWeaponShot 调用。

当前使用玩家视角射线；没有玩家控制器时使用角色眼睛朝向。没有单独的枪口遮挡检测，摄像机射线和枪口效果可能在贴墙时存在差别，后续再完善。

## 5. 弹药显示与碰撞检查

可从组件 Bind Event to On Ammo Changed，把 New Ammo 打印到屏幕，或更新现有 HUD。绑定后用 Get Current Ammo 主动读取初始值；初始化不会补发历史事件。

第一轮建议 Magazine Capacity 设置 3、Damage 设置 25、Fire Interval 设置 0.2。测试清楚后恢复期望配置。敌人初始 100 血时，一发应减少 25；换回足够弹量时第四发进入一次死亡流程。

如果播放了开火表现但没有掉血：
1. Blocking Hit 是否为 true，Hit Actor 是否为目标敌人。
2. 目标胶囊或网格是否阻挡组件配置的 Trace Channel。
3. 是否命中墙或其他遮挡物，是否在 Range 内。
4. 敌人是否已初始化 GAS、是否仍存活。
5. 旧蓝图射击伤害入口是否已退出，避免重复伤害干扰判断。

目标死亡后胶囊会关闭碰撞，这是 T03 的既定行为。

## 6. 人工验收清单（验收通过，用户反馈见第 9 节）

- [x] Shooter 的父类仍为 MyShooter，继承的 ShooterWeapon 可见。
- [x] 单次按下立即发一发，按住按配置间隔连射。
- [x] 松开停止，重复快速点按不会突破射速或叠加循环。
- [x] 每发只扣一枚弹药；打空同样扣弹。
- [x] 命中 100 血敌人，每发 25，第 4 发只触发一次死亡。
- [x] 弹药为 0 后不再产生开火表现或伤害。
- [x] 角色死亡时射击停止，没有延迟补发。
- [x] 旧玩家蓝图射击循环、扣弹和伤害入口已退出调用链。
- [x] 项目现有开火动画 / 音效 / 特效正常（按用户反馈；本次未新增命中特效）。
- [x] 原有死亡动画、尸体清理和 AI 行为没有退化。
- [x] 停止重进 PIE，满弹匣、正常血量，没有残留回调。

换弹互斥由自动测试验证；真正的换弹输入和动画在 T05 接入后再进行人工验收。

## 7. 文件与自动验证

阅读顺序：
1. Public/Characters/MyShooter.h：玩家如何提供武器组件。
2. Public/Weapons/ShooterWeaponComponent.h：配置、输入接口和表现事件。
3. Private/Weapons/ShooterWeaponComponent.cpp：能力授予、单发射线、扣弹与伤害。
4. Public/GAS/Abilities/ShooterFireAbility.h 及对应 Private 实现：连射和取消生命周期。
5. Private/Tests/ShooterGASFireTest.cpp：行为验证情景。

以上路径均相对于 Source/MyShoot。完整规范见 [C++ 文件与注释规范](CPP_CODE_CONVENTIONS.md)。

自动测试 MyShoot.GAS.Fire 覆盖真实射线伤害、快速点按射速、松开停止、空弹匣、打空扣弹、换弹标签取消、死亡取消以及组件移除撤销能力；同时回归 Foundation / Damage / Death。

最终验证：UE 5.5 Development Editor 构建通过；MyShoot.GAS.Foundation / Damage / Death / Fire 共 4 个测试成功，0 失败、0 测试警告；项目现有 7 个蓝图编译为 0 错误、6 条既有动画编译警告、0 加载失败。命令日志包含重复汇总及既有骨架警告，整体汇总为 16 条警告，不代表新增 16 个问题。自动测试不能代替本项目摄像机、动画和输入的实际 PIE 验收。

本次修改前恢复点：Saved/T04/BeforeLayout-20260924-114323.zip。目录整理回归报告：Saved/T04/LayoutReport/index.json。最终日志与报告统一放 Saved/T04，已被 .gitignore 忽略。2026-09-25 用户验收通过后授权统一提交 T02–T04 至 develop。

## 8. 反馈模板

```text
ShooterWeapon 组件可见：
按下 / 按住 / 松开：
快速点按射速：
一发扣弹数量：
每发伤害：
空弹匣后表现：
开火动画 / 音效 / 特效：
旧玩家射击循环已断开：
死亡和尸体清理：
重新 PIE：
报错：
```

验证文件：Saved/T04/Build.log、AutomationReport/index.json、ProjectBlueprintCompile.log、ProjectFiles.log。Visual Studio 工程已刷新；325 个 Content / Config 文件校验值与开始前一致。

蓝图编译参数说明：AllowListFile 必须使用项目相对路径 Saved/T01/BlueprintAllowList；UE 5.5 会在读取前自动拼接项目目录。第一次使用绝对路径导致筛选文件读取失败，已按相对路径重跑成功，最终依据 ProjectBlueprintCompile.log。

## 9. T04 用户验收记录（2026-09-25）

用户确认以下结果，并授权提交 Git；T04 标记完成。

| 项目 | 用户反馈 |
| --- | --- |
| ShooterWeapon 组件可见 | 可以 |
| 按下 / 按住 / 松开 | 完整 |
| 快速点按射速 | 可以 |
| 一发扣弹数量 | 正常 |
| 每发伤害 | 正常，25 |
| 空弹匣后表现 | 无法射击，没有特效动作 |
| 开火动画 / 音效 / 特效 | 正常 |
| 旧玩家射击循环 | 已断开，使用新的 GAS 接点 |
| 死亡和尸体清理 | 正常 |
| 重新 PIE | 正常 |
| 报错 | 无 |

上述为用户实际运行反馈；结合已完成的 C++、自动测试和蓝图输入验证，T04 验收完成。下一项为 T05：GA_Reload、备用弹药与换弹中断，本次提交不实施 T05。
