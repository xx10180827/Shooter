# T01 GAS 基础实施与编辑器接入

日期：2026-09-22<br>
项目：`D:\UE5_My_Project\MyShoot 5.5`<br>
状态：**T01 完成。原生构建和自动验证通过；用户已确认角色蓝图接入、血量初始化和运行表现正常。**

## 1. 本次范围与实际改动

- MyShoot.uproject 启用 GameplayAbilities。
- MyShoot.Build.cs 加入 GameplayAbilities / GameplayTags / GameplayTasks。
- 新增 AShooterCharacterBase：实现 IAbilitySystemInterface，持有 ASC 和 AttributeSet。
- 新增 UShooterAttributeSet：Health / MaxHealth 和数值边界。
- 新增 UShooterInitialAttributesEffect：Instant GameplayEffect，先设置 MaxHealth，再设置 Health。
- 初始数值使用原生 GameplayTag Data.InitialHealth 作为 SetByCaller 键；无需在编辑器重复添加该标签。
- AMyShooter 改为继承 AShooterCharacterBase；删除空的 BeginPlay / Tick / 输入绑定覆写。
- 公共角色仍允许 Tick，保留已有蓝图 Event Tick 的运行条件。
- 新增并执行 MyShoot.GAS.Foundation 自动化测试：成功，0 错误，1 条 GameplayCue 搜索路径警告。

类结构：

```text
ACharacter
└─ AShooterCharacterBase
   ├─ AMyShooter
   │  └─ Shooter（用户稍后重设父类）
   └─ Boot_Shooter_BP（用户稍后重设父类）
```

这次没有改动现有蓝图资产、地图和输入。此代码按单机设计，尚未实现网络属性复制、伤害入口、死亡响应或能力授予。

## 2. 生命周期和数据约定

- 每个角色独立持有 ASC 和 AttributeSet，Owner / Avatar 均为角色自身。
- 在原生 BeginPlay 中先初始化 GAS，再调用父类 BeginPlay，使原有 Blueprint BeginPlay 执行时可查询初始化结果。
- 初始属性只在该角色实例的初始化路径中应用一次；重新被控制器接管时只刷新 ActorInfo，不恢复血量。
- EndPlay 清理 ActorInfo 和初始化标记。
- Initial Max Health 默认 100，可在角色蓝图 Class Defaults → Shooter / GAS 下配置。
- 无需先创建 GE 蓝图；本轮使用 C++ 定义的初始效果。初始化后应满足 Health = MaxHealth，MaxHealth >= 1。
- 无效的初始值在运行时有兜底处理；运行时动态 MaxHealth 增益和比例调整不在本轮范围。
- 通过 Get GAS Health / Get GAS Max Health / Is GAS Initialized 查询数据，避免与旧角色蓝图中的 Health 命名冲突。

**阶段性迁移说明：T01 的 GAS 属性用于初始化验收，现有射击仍执行旧蓝图逻辑。旧 Health 暂时保留以避免中断现有玩法；T02 必须将伤害切换到 GAS 并移除旧扣血路径，之后以 GAS 为唯一战斗血量来源。不要在两者之间增加双向同步，也不要用当前旧射击能否降低 GAS Health 来判断 T01 是否通过。**

## 3. 当前可以开始蓝图接入

已在编辑器关闭后执行 -Rebuild。首轮发现测试文件头文件路径错误，修正后续编和链接成功；所有新增原生类均已编译。没有使用 Live Coding 加载继承或反射结构变化。Visual Studio 工程文件也已重新生成。

自动化测试初次运行时，隔离 World 没有 GameMode，未派发 BeginPlay；修正为调用 WorldSettings 的正式 NotifyBeginPlay 入口，并增加生命周期断言后，测试通过。未降低预期或删除行为断言。最终以 AutomationReport-Final/index.json 中的测试结果为准，不能只根据 UE 进程退出码判断测试成功。

现在可重新打开 MyShoot.uproject，按下一节先接敌人蓝图，再接玩家。助手没有修改资产父类，蓝图接入与视觉表现仍需人工验收。

## 4. 编译通过后的敌人接入步骤

1. 打开正确的 MyShoot 5.5 项目。
2. 打开 Boot_Shooter_BP，先记录原模型、Anim Class、AI Controller Class 和 Auto Possess AI。
3. 在 Blueprint 编辑器 File → Reparent Blueprint 中选择 ShooterCharacterBase。
4. 编译蓝图，确认没有同名成员冲突、组件丢失或断开的节点。
5. 在 Class Defaults 搜索 Initial Max Health，先设为 100。
6. 检查 AI Controller Class 仍为 Boot_Shooter_controller，Auto Possess AI 仍为 PlacedInWorld。
7. 保留原 BeginPlay 逻辑。原生 GAS 初始化不要求在蓝图里再手动调用一次；若原有父类事件调用节点因更换父类失效，检查并更新该节点。
8. 在现有 BeginPlay 执行链末尾临时连接 Print String，输出 Is GAS Initialized、Get GAS Health、Get GAS Max Health。不要新建第二条独立 BeginPlay 事件。
9. 进入 PIE，应得到 true / 100 / 100；退出后将 Initial Max Health 改为 175，再进入应得到 true / 175 / 175。
10. 最后把初始值恢复为希望使用的数值，清理临时打印或注明调试用途。

注意：修改 Boot_Shooter_BP 父类会影响所有使用该蓝图的敌人实例；这里的“先接一个敌人”指先接敌人这一种蓝图，不代表只修改地图中的单独实例。

## 5. 敌人通过后的玩家接入步骤

1. 打开 Shooter，通过 File → Reparent Blueprint 选择 MyShooter。
2. 编译，检查 Mesh、Anim Class、相机、弹簧臂等原组件与相对变换。
3. Class Defaults 中确认 Initial Max Health。
4. GameMode 保持 Mygame_GM，Default Pawn Class 保持 Shooter。
5. 保留现有输入和射击节点，通过上述 GAS 查询节点确认初始化。
6. 运行移动、视角、跳跃和原有射击，确认更换父类没有改变原行为。
7. 连续进入 / 退出 PIE 三次，确认没有重复初始化错误、无效引用或组件丢失。

不要在蓝图组件面板额外手动添加第二个 ASC；公共 C++ 父类已经创建组件。

## 6. 自动验证结果

MyShoot.GAS.Foundation 已通过以下行为检查：

- 在实际测试 World 中生成玩家 C++ 子类，ASC 与 AttributeSet 可用，Owner / Avatar 正确。
- 模拟蓝图初始值 175，初始效果得到 175 / 175。
- 测试专用 Instant GE 能作用于实际注册的属性。
- 首次及重复控制器接管不会再次应用初始属性、导致回血。
- 血量不会低于 0 或超过 MaxHealth。
- EndPlay 清理 ActorInfo；新角色独立初始化为默认满血。
- 初始最大血量为 0 的异常配置能被限制为正数。

这些是原生 GAS 基础测试，不替代对现有 Blueprint 资产、模型和动画的人工验收。测试里的临时伤害效果仅用于验证属性，不是 T02 的生产伤害入口。

## 7. 结果记录

| 检查项 | 状态 |
| --- | --- |
| 代码实现 | 已构建并通过原生行为测试 |
| Development Editor 完整编译 | 已执行全量构建；修复测试包含路径后续编成功，退出码 0 |
| MyShoot.GAS.Foundation | Success，0 错误 / 1 警告；全部预期断言通过 |
| 项目蓝图编译检查 | 7 个蓝图，0 错误 / 6 编译警告 / 0 加载失败，退出码 0 |
| 敌人重设父类与属性验收 | 用户确认通过；175→178 为输入笔误 |
| 玩家重设父类与玩法回归 | 用户在补充确认中反馈结果正常 |
| Git 提交 / 推送 | c7b258f 已推送到 origin/develop |

修改前的 Source 与 .uproject 已保存在 Saved/T01/Before-20260922-103810。当前蓝图 / 配置哈希清单位于 Saved/T01/asset-baseline.csv；现有资源的完整历史备份见 T00 文档。

## 8. 用户验收回复模板

```text
敌人父类：ShooterCharacterBase
敌人 Is GAS Initialized：
敌人 100 配置下 Health / MaxHealth：
敌人 175 配置下 Health / MaxHealth：
玩家父类：MyShooter
玩家 Is GAS Initialized：
玩家 Health / MaxHealth：
模型 / 动画 / 相机：
移动 / 射击 / 敌人追踪：
连续三次 PIE：
报错：
```

## 9. 日志与已知限制

- Saved/T01/Build-DevelopmentEditor.log：最初完整构建日志（含已解决的测试包含路径错误）。
- Saved/T01/Build-DevelopmentEditor-Final.log：最终续编与链接成功日志。
- Saved/T01/Automation-Final.log：最终自动测试日志。
- Saved/T01/AutomationReport-Final/index.json：结构化报告，succeededWithWarnings=1、failed=0、notRun=0，测试 state=Success。
- Saved/T01/BlueprintCompile.log：项目蓝图检查结果。
- Saved/T01/GenerateProjectFiles.log：Visual Studio 工程文件生成结果。
- Saved/T01/pre-build-assets.csv：本次构建前的 325 个资产 / 配置文件校验值；构建及测试后复核无变化。

GAS 测试中的一条警告：没有配置 GameplayCueNotifyPaths，引擎回退为扫描 /Game。当前尚无 GameplayCue 资源，本轮保留默认行为；后续接入 GameplayCue 时指定实际资源目录。

原有 Shooter_idle / Boot_Shooter_AnimationBP 的 6 条线程安全编译警告仍待处理。日志还显示 HeroFPP_Skeleton / HeroTPP_Skeleton 引用旧 /Engine/EngineMeshes/Humanoid；该引用在 T00 日志中也已存在，本轮未改动骨骼资源。它不阻塞当前蓝图编译；原动画表现仍按人工验收确认。

首轮失败的自动化日志与报告保留排查用途，最终结论以带 Final 的报告为准。本阶段已在用户验收后以 c7b258f 提交并推送至 origin/develop。

## 10. 用户验收（2026-09-22）

用户确认敌人父类为 ShooterCharacterBase，蓝图编译成功，模型动画完整、追踪正常、无其他报错。经追问更正：175 配置得到 178 是输入笔误，结果均正确；据此记录 Is GAS Initialized 为 true，100 / 175 配置满血初始化正确，并记录用户确认玩家 MyShooter 接入结果正常。上述为用户人工反馈，不等同于助手操作编辑器。

T01 完成。后续 T02 接通生产伤害入口。生成文件、测试报告和本地备份继续通过 .gitignore 排除。
