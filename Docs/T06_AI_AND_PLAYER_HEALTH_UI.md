# T06：基础 AI 战斗与屏幕玩家血条

日期：2026-09-26<br>
项目：`D:\UE5_My_Project\MyShoot 5.5`<br>
状态：完成；自动验证与用户 PIE 场景验收通过。<br>
Git：用户已授权将 T05/T06 作为本次功能提交保存并推送 origin/develop；提交号以 Git 历史为准。

## 1. 本次范围

根据用户要求推进 T06，并提前接入 T07 中的玩家血条部分。
形成“敌人发现玩家 → NavMesh 追踪 → 有效范围内攻击 → GAS 扣血 → 屏幕血条更新 → 玩家死亡中断”的链路。
对局胜负界面和重开按钮继续留在 T07。

C++ 头文件放 Public、实现放 Private，并按 AI、Player、UI、Tests 分目录；关键功能和生命周期写中文注释。
原有玩家射击、换弹、敌人移动动画及死亡动画继续复用。

## 2. AI 行为

`Boot_Shooter_controller` 保留为控制器蓝图，父类改成 `ShooterAIController`。
旧 BeginPlay → AI MoveTo → Delay 的执行入口断开，旧节点与内部连线保留并加中文说明。
现有敌人仍使用同一个控制器资产，无需重新指定地图里的敌人。

| 参数 | 默认值 | 含义 |
| --- | --- | --- |
| Detection Range | 2000 cm | 发现玩家的距离，必须有视线 |
| Lose Target Range | 3000 cm | 超出后放弃目标 |
| Attack Range | 220 cm | 当前为近距离攻击 |
| Attack Damage | 10 | 每次成功攻击的 GAS 伤害 |
| Attack Windup | 0.3 秒 | 攻击前摇，结束时才尝试结算 |
| Attack Interval | 1.25 秒 | 从开始攻击计的最短间隔 |
| Decision Interval | 0.2 秒 | 寻找、追踪和攻击条件检查周期 |

可在控制器蓝图 Class Defaults 的 Shooter / AI 中调整。状态为 Idle、Chasing、Attacking、Dead。

- 查找单机玩家控制器的 Pawn，不把其他敌人作为默认目标。
- 发现和攻击检查 Visibility 视线；场景墙体需阻挡该通道。
- 追踪使用现有 NavMesh。活跃 MoveToActor 自行跟踪目标，失败或结束后限频重试。
- 保留引擎控制器 Tick 更新朝向，玩法决策使用定时器。
- 攻击前摇结束再次检查距离、目标血量、攻击者血量和遮挡，躲开或进入墙后不会继续扣血。
- 伤害统一调用 ApplyGASDamage，更新已有 AttributeSet 的 Health。
- 死亡、失去目标、目标死亡/销毁或控制器退出时，取消攻击和相关订阅。
- 多个敌人各自保存目标、冷却与定时器。

本次采用近距离定时攻击。OnAttackStarted(Target, Windup) 是后续配置专用攻击动作的表现接口；
当前没有新增专用近战动画。现有追踪和死亡动画保留。

## 3. 玩家血条

新增资源：

- `/Game/UI/T_ShooterHealthFrame`：用户提供图片整理后的透明底框。
- `/Game/UI/WBP_ShooterHealthBar`：可在 UE UMG Designer 内编辑的屏幕血条。
- `/Game/Blueprints/BP_ShooterPlayerController`：继承 ShooterPlayerController，配置血条类。
- `Mygame_GM` 的 Player Controller Class 指向新控制器；Default Pawn 仍为 Shooter。

布局固定在玩家屏幕左下角，使用左下锚点、480×160 的设计尺寸和屏幕边距。
通过 AddToPlayerScreen 添加到玩家视口，未创建模型上方的 WidgetComponent。
血条不接收鼠标命中，保留原准星。

图片中的固定 70%、18550 等内容已经清理。实际数字由 HealthValue 文本控件显示，
实际进度由 HealthFill 控件显示，均读取 GAS：

- 初次显示即读取当前 Health / MaxHealth。
- 受伤通过属性变化事件立即更新，不用 Tick 轮询。
- 血量比例不高于 30% 时进度条变红。
- 血量归零时显示 0 / MaxHealth。
- 玩家 Pawn 清理后保持空血条；重新接管角色会解绑旧角色并读取新初值。
- 控制器退出时移除控件和监听。

右侧蓝色图案沿用原图作为装饰，本次显示的是血量，不额外代表护盾数值。

## 4. 图片来源与处理

用户原图备份：`SourceArt/UI/ShooterBloodUI_reference.png`。<br>
用于导入的底框：`SourceArt/UI/ShooterHealthFrame.png`。<br>
使用内置 imagegen，未使用 Python；保留原图，不覆盖用户下载目录的文件。

实际生成提示词：

> Use the user-attached ShooterBloodUI image in this conversation as the edit target. Create a game-ready Unreal HUD frame PNG by closely preserving its horizontal silver metal capsule frame, red glowing heart left, cyan circular emblem right, and dark textured middle. Remove the surrounding tall dark poster backdrop, the BLOOD VOLUME headline, all text/numbers including 70%,18550,1280K and BLOOD, and the central glowing progress line. Leave the center panel blank dark charcoal for live health data drawn by the game. Transparent alpha background outside the horizontal metal HUD only. Tight wide 4:1 composition with small transparent padding; no new objects, no text, no numbers. Keep the original visual design. This is an image extraction and cleanup for a texture, not a new UI screenshot.

输出为 2172×724 PNG，已检查角落 Alpha 为 0；UMG 按实际 3:1 比例显示。

## 5. 代码入口

以下路径相对 Source/MyShoot：

| 文件 | 职责 |
| --- | --- |
| Public/AI/ShooterAIController.h | AI 参数、状态和表现接口 |
| Private/AI/ShooterAIController.cpp | 目标、寻路、前摇、GAS 攻击及取消 |
| Public/UI/ShooterHealthWidget.h | 血条控件绑定和只读显示数据 |
| Private/UI/ShooterHealthWidget.cpp | GAS 属性订阅、初值、文本和进度更新 |
| Public/Player/ShooterPlayerController.h | 玩家 HUD 类配置 |
| Private/Player/ShooterPlayerController.cpp | 创建屏幕控件、接管 Pawn 和退出清理 |
| Private/Tests/ShooterGASAITest.cpp | AI 遮挡/距离/冷却/死亡/独立状态与击杀中断换弹 |
| Private/Tests/ShooterHealthHUDTest.cpp | 血条初值、属性通知、最大血量和解绑 |
| Private/Tests/ShooterGASReloadTest.cpp | 增补弹匣 0 / 备用 90 时正常换成 30 / 60 |

编辑器接入工具位于 Plugins/ShooterEditorTools，负责迁移、UMG 资源创建和已保存蓝图运行验证。
迁移前的现有地图修改已备份，迁移不重新保存地图。

## 6. 验证记录

已完成以下检查：

- Development Editor / Win64 C++ 构建通过，VS2022 工程文件已重新生成。
- GAS 自动测试 7/7 通过，失败和未运行均为 0：Foundation、Damage、Death、Fire、Reload、AI、HealthHUD。
- 9 个项目蓝图编译通过：0 错误、0 加载失败；两个既有动画蓝图的 6 条编译警告保留。
- 独立加载保存后的资源验证通过：GameMode / PlayerController / Widget 引用正确；实际敌人蓝图造成 GAS 伤害；实际 UMG 控件显示初始 100、受伤 90、低血量 25、死亡 0；玩家死亡后 AI 清除目标。
- 实际 UMG 在 720p 和 1080p 下完成离屏渲染，检查图片框、真实数值、低血量红色进度和左下角锚点。预览图灰色背景仅用于布局检查，不是游戏内新增的背景遮罩。
- AI 原有 7 个节点全部保留，增加 1 个说明注释节点；旧执行入口断开，保留原循环内部连线供对比。
- 对照迁移前资源哈希，T06 仅修改原有 Boot_Shooter_controller 和 Mygame_GM 两个资产；原地图、玩家、敌人和准星资产没有因 T06 再次改写。
- 空弹匣且备用弹药为 90 时，正常换弹得到 30 / 60；AI 在换弹期间击杀玩家会取消换弹，不出现延迟补弹。这两项已有自动证据，场景操作仍需人工复核。

产物位于 Saved/T06，由 .gitignore 忽略：

- BeforeAIHUD-20260926-104714.zip：含已有未提交修改的基线备份。
- AssetBaseline.csv：资源哈希基线。
- BuildFinal.log：C++ 构建。
- AutomationReport/index.json：GAS 自动测试。
- Migrate.log：资源接入记录。
- Verify.log：实际敌人蓝图、GAS 和实际 UMG 控件验证。
- HUD-100.png / HUD-25.png / HUD-0.png：UMG 离屏布局检查图。
- CompileBlueprints.log：项目蓝图编译。
- GenerateProjectFiles.log：VS2022 工程文件生成。
- Before/：迁移前的原蓝图节点与连线。

自动测试使用独立世界验证规则；原地图 NavMesh 的实际追踪、场景遮挡以及 PIE 操作体验由下一节人工验收。

## 7. 你需要做的 PIE 验收

打开原地图进入 PIE：

1. 左下角出现图片样式血条，开始即显示正确血量；准星、移动和射击正常。
2. 靠近敌人，使敌人看到玩家。敌人追近后开始扣血，每次默认 10，血条同步下降。
3. 与敌人拉开到攻击范围外，或躲到阻挡 Visibility 的墙后，确认不再受伤。
4. 打死敌人，确认它不再追踪或造成伤害，旧死亡动画和尸体清理仍正常。
5. 让玩家剩余少量血，开始换弹并让敌人击杀，确认换弹取消、血条归零、输入停止。
6. 停止 PIE 后再开始，确认只有一个玩家血条，恢复初始血量，没有旧目标或定时器残留。
7. 如场景有多个敌人，确认它们的活动独立。可调整窗口尺寸检查血条锚点与边距。

玩家死亡后的失败界面/重开按钮尚属 T07；本阶段可停止 PIE 后重新运行。

反馈模板：

```text
屏幕左下角血条 / 原准星：
初始血量：
敌人追踪与攻击：
每次扣血 / 血条同步：
离开范围 / 墙体阻挡：
敌人死亡停止攻击：
玩家死亡 / 换弹中断：
重新 PIE / 窗口尺寸：
报错：
```

## 8. 用户 PIE 验收与提交授权（2026-09-26）

用户确认本次验收结果：

| 项目 | 结果 |
| --- | --- |
| 屏幕左下角血条 / 原准星 | 正常 |
| 初始血量 | 100 |
| 敌人追踪与攻击 | 正常 |
| 每次扣血 / 血条同步 | 正常 |
| 离开范围 / 墙体阻挡 | 用户反馈停止 AI 行为 |
| 敌人死亡停止攻击 | 正常 |
| 玩家死亡 / 换弹中断 | 正常 |
| 重新 PIE / 窗口尺寸 | 正常 |
| 报错 | 无 |

T06 验收完成，T07 的屏幕血条部分也已通过验收。用户授权提交并推送到远端仓库 develop 分支，本次包含此前尚未提交的 T05 换弹实现及相关测试、蓝图、资源与文档。

提交说明按实际代码记录为 AI 追踪、近距离攻击与 GAS 扣血；当前没有新增独立的 AI 枪械射击能力或专用攻击动画。
