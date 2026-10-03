# T15 触发器拾取：武器与弹药

更新日期：2026-10-03。基于已推送的 T14 `24ce612` 开发。本轮包括用户描述的第一版武器拾取和第二版弹药拾取；尚未提交推送。

## 玩法和配置

- 玩家初始只拥有步枪。霰弹枪在场景中，领取前按 2 无效；领取后按 1/2 切枪，领取本身不强制打断当前动作或自动换枪。
- 范围内准星指向物品且视线无遮挡时，准星下方显示物品名和“按 F 拾取”。远离、转头、隔墙、暂停和死亡均不能领取。
- 默认 ProximityTrigger 半径 250 cm。以玩家根位置到触发器中心的实际距离做额外限制，避免胶囊边缘先进入时扩大交互距离。
- 按用户最新要求，一次弹药拾取同时补满对应武器：步枪 30/90、霰弹枪 8/32；备用上限分别为 90、32，保持原有初始数值。
- 弹药按 WeaponDefinition 匹配，当前未装备也能补满弹匣和备用。尚未拥有对应武器、弹匣与备用两项都满、重复武器均保留场景物品，提示原因。
- 例如步枪 0/0、0/90 或 30/20 均能领取，结果统一为 30/90；只有已经 30/90 时不能领取。当前武器补满前取消射击与旧换弹，避免补满后又被旧计时器扣除备用；非当前武器补满不打断正在使用的武器。
- 成功领取先锁定/标记已消耗，再关闭碰撞和显示、通知表现并销毁。连续 F 和事件重入不能重复领取。重开关卡恢复默认物品与初始所有权。

场景资产：

- `/Game/Pickups/BP_ShotgunPickup`
- `/Game/Pickups/BP_RifleAmmoPickup`
- `/Game/Pickups/BP_ShotgunAmmoPickup`

默认放在 bloodstrike 出生点前方附近，World Outliner 名称为 T15_Shotgun_Pickup、T15_Rifle_Ammo、T15_Shotgun_Ammo。弹药盒目前用引擎立方体做占位模型，可在蓝图里替换 DisplayMesh。

## 触发器与 Overlap 的关系

UE 触发器同样使用碰撞查询系统。这里没有物理阻挡，不会依赖撞到物品领取：

1. ProximityTrigger 是 SphereComponent，QueryOnly，Pawn 通道 Overlap，其余 Ignore。
2. OnComponentBeginOverlap / EndOverlap 只登记和移除玩家附近的候选物品；出生时已重叠也会登记。
3. 玩家交互组件每 0.05 秒从这些候选里找准星射线穿过 FocusBounds 的最近目标。
4. 使用 Visibility 射线检查摄像机到物品之间的墙体。范围触发器可以穿墙，视线检查不能省略。
5. F 按下时再次执行同一套检测，不直接相信上一帧提示。

FocusBounds 只参与数学射线/盒体相交测试，Collision 为 NoCollision；DisplayMesh 也不阻挡玩家和子弹。实际场景墙体需要阻挡 Visibility，与原射击遮挡规则一致。

如果想练习触发器事件，可打开 BP_ShotgunPickup，选中 ProximityTrigger，在 Details → Events 中为 On Component Begin Overlap / End Overlap 添加 Print String，观察进入与离开。原生 C++ 已绑定候选登记，不要在这些蓝图事件里再次授予武器或销毁物品。

在编辑器中选中拾取蓝图实例，查看 ProximityTrigger 球体与 FocusBounds 方框；调整 Sphere Radius 改附近范围，调整 Box Extent 改准星可瞄准区域。不要把展示模型缩放当成扩大触发器范围。

## 框架职责

| 类型 | 职责 |
| --- | --- |
| AShooterPickup | 触发器候选登记、领取事务、消耗生命周期、公共展示节点 |
| AShooterWeaponPickup | 武器授予规则，拒绝重复武器 |
| AShooterAmmoPickup | 对应武器弹匣与备用一起补满，拒绝未拥有与两项全满 |
| UShooterInteractionComponent | 本地候选管理、准星和遮挡检测、F 输入复检 |
| UShooterWeaponComponent / ShooterWeaponInventory.cpp | 所有权和各槽弹药状态；不修改共享 DataAsset |
| UShooterPickupWidget | 屏幕提示，只读取交互结果 |

C++ 声明和实现位于 Public / Private 对应子目录，职责和关键边界有中文注释。旧蓝图节点保留。

WeaponDefinitions 作为可装备配置目录继续保留两项；InitialWeaponCount=1 控制本局初始只拥有第一项。运行时槽保存 bOwned，EquipWeapon 拒绝未拥有槽。默认 InitialWeaponCount=0 保留旧演示/已有测试的全部初始拥有行为。

## 后续美术与功能扩展

拾取 Actor 结构：

```text
PickupRoot
├─ ProximityTrigger       范围检测
├─ FocusBounds            准星检测
└─ PresentationRoot       展示专用
   └─ DisplayMesh         模型
```

可以在 BP 的 PresentationRoot 下添加旋转/浮动组件、光源、Niagara、底座或额外网格；旋转缩放 PresentationRoot 不改变触发器与瞄准盒。当前使用独立 GroundMarker 地面光圈，自动旋转仍留待后续。触发器本身不渲染；DisplayMesh 使用网格原材质，金色发光只应用于 GroundMarker。光圈直接挂 PickupRoot，未来旋转 PresentationRoot 不会带动光圈。

OnPickupGranted 蓝图事件只在成功领取后触发，可播放世界音效或生成独立特效；Actor 随后销毁，持续特效需要独立生成。以后新增其他物品可继承 AShooterPickup，实现 CanReceive 与 Grant，不复制交互和消耗流程。

当前按原项目单机流程实现，授予要求 Authority；没有宣称实现多人拾取竞争或网络同步。

## 编辑器迁移和保护

ShooterPickupSetupCommandlet 配置角色初始所有权、备用上限，创建三份蓝图并在现有地图增量放置物品。`-Verify` 只读核对；重复执行按 T15_Pickup_0/1/2 标记识别已有 Actor，不重复放置。

修改前备份在 Saved/T15_Pickups/Backup，含 Shooter、bloodstrike 与两份武器配置。地图之前已有本地修改，本轮基于该版本增量添加，不用 Git 版本覆盖。菜单、子弹材质和个人配置不需改写。T08 包未更新。

## 用户验收清单

1. 新 PIE 只有步枪，按 2 不出现霰弹枪。
2. 靠近并瞄准霰弹枪：显示名称和 F 提示；转头、远离、隔墙不显示可领取提示，F 无效。
3. 按 F：物品消失，当前仍是步枪；按 2 能切出霰弹枪。连续 F 不重复增加武器或弹药。
4. 拾取对应弹药：同时补满主弹匣和备用到 30/90 或 8/32；未装备武器切回来仍保留补满结果。
5. 弹匣与备用都满、未拥有武器、重复武器：物品保留并显示原因；任意一项不足仍可领取。
6. 步枪打空弹匣甚至备用全部耗尽：F 领取后恢复 30/90，物品只消耗一次，HUD 同步。
7. 换弹期间拾取弹药：旧换弹取消，立即补满，原计时器到期后不再扣备用；切枪状态正常。
8. 暂停、死亡、重开和窗口尺寸变化：提示、输入和状态正常。

## 2026-10-02 初版验证记录（当时为定量补备用，已由最新规则替代）

- UE5.5 Editor 编译通过，最终构建 `Saved/T15_Pickups/Build4.log`。Build2 已包含完整运行时功能；Build3/4 只调整真实地图验证时序和编辑器展示方向。
- 独立资源重新加载验证通过：`Verify.log` 初始拥有数 1、备用上限 180/64、三处持久化拾取点；`SetupFinal.log` 重复运行 created=0，未重复放置。
- 全部 MyShoot 自动化：`AutomationReport/index.json` 为 16 成功、0 失败、0 未运行、0 测试警告，覆盖所有权、距离/遮挡、暂停/死亡、重复 F、已满/未拥有、超上限、非当前武器补弹、原射击/换弹/后坐力等。此后未修改运行时拾取逻辑。
- 实际 bloodstrike 地图：`MapFinal.log` 与 `MapSmoke/Result.txt` 为 PASS，真实 F 输入获得霰弹枪、重复 F 幂等、按 2 切换、霰弹备用 32→40、未装备步枪备用 90→120、弹药 HUD 同步均通过。已查看 01-ShotgunPrompt.png 和 02-AmmoPrompt.png，中文名称、F 提示、模型及原 HUD 正常。
- 初次地图验证过早读取相机更新和 InputKey 入队结果，导致提示/领取断言失败；改为等待后续游戏帧后通过，失败日志 MapGame.log 和 MapDiagnostic.log 保留。未通过修改运行时绕过检查。
- 资源工具 0 errors，但加载旧动画蓝图仍有线程安全警告，另有既有 GameplayCue 路径回退警告，不宣称项目全局无警告。
- `git diff --check` 通过；Saved 备份、日志和截图继续被忽略。地图与角色数据有本轮必要增量修改，未提交、未推送、未更新 T08 包。用户 PIE 尚未验收。

## 2026-10-03 补满规则与金色材质修正

用户指出旧版弹药拾取不符合预期，明确要求同时恢复主弹匣和备用，并纠正最大值为步枪 30/90、霰弹枪 8/32。旧版只检查备用是否不足，在备用已满、弹匣空时会拒绝；新版 NeedsAmmoRefill 分别检查两项，TryRefillWeaponAmmo 完成取消旧状态、原子写入以及 HUD 双事件通知。

旧 TryAddReserveAmmo 接口保留供未来定量奖励使用，AShooterAmmoPickup 不再调用它；旧 Amount 序列化字段保留兼容历史资源，但不再用于计算，也不在编辑器暴露为有效配置。

新增材质 `/Game/Pickups/Materials/M_PickupGold` 与材质实例 `MI_PickupGold`，参数为：

- GoldColor：金黄色，默认线性 RGB (1, 0.48, 0.018)。
- GlowStrength：主体发光，默认 1.5。
- RimStrength：Fresnel 边缘发光，默认 3。

在 MI_PickupGold 中勾选参数覆盖即可调整。颜色/自发光绑定所有拾取展示模型的材质槽，玩家手持模型保持原材质。模型仍可被替换，PresentationRoot 和范围检测结构不变。

`ShooterPickupSetup -RefillGold` 更新材质、补满名称和两份备用上限；加 `-Verify` 只读核对。修正前资产备份为 Saved/T15_RefillGold/Backup，验证日志与图片位于同目录下。本次不自动提交推送，不更新打包版本。

本轮验证已完成，用户复验待进行：

- 最终编译 Build4.log 通过。Build2 包含全部运行时逻辑和材质工具；Build3/4 只调整测试等待与相机对准时序。
- Apply.log 生成并保存三个拾取物的材质/补满名称；Verify.log 独立重载通过，验证步枪 30/90 和霰弹枪 8/32 上限、三份拾取蓝图及场景实例材质引用。
- 首轮自动回归原有 15 项成功；拾取专项只因固定等待时弹匣尚剩 1 发产生断言失败。改为限时等待实际弹量归零后，PickupVerified/index.json 为 1 成功、0 失败、0 未运行。覆盖备用满但弹匣空、0/0、两项都满拒绝、非当前武器、换弹中领取取消旧流程、重复领取和原距离遮挡规则。失败历史保留，不宣称单份报告一次全绿。
- 实际地图 MapVerified.log / MapSmoke/Result.txt 为 PASS：真实开火/R 换弹耗尽步枪至 0/0，真实 F 恢复 30/90，HUD 同步；霰弹枪 7/32 拾取恢复 8/32。
- 首轮地图领取失败时 focus=None；自动改变视角后存在相机位置偏置更新，等待相机稳定并再次对准后才有 F 提示。MapVerified.log 明确记录 focus=BP_RifleAmmoPickup_C_0，领取后 ammo=30 reserve=90；未放宽运行时准星或遮挡检查。
- 已查看金色霰弹枪、金色弹药盒“步枪弹药（补满）/按 F 拾取”、领取后 30/90 的实际截图。
- Shooter、Shooter_idle、菜单、子弹材质及项目文件 SHA256 与本轮修改前一致；两份武器 DA 仅有本轮备用上限修改，场景与拾取蓝图更新材质/名称并保留位置。地图、拾取蓝图和武器 DA 备份已保留。
- 资源验证 0 errors，保留旧动画蓝图线程安全与 GameplayCue 路径警告。git diff --check 通过，Saved 日志/备份/截图被忽略。尚未提交推送或打包。


## 2026-10-03 用户验收通过，展示改为金色地面标记圈

用户已确认：步枪恢复 30/90、霰弹枪恢复 8/32、备用满但弹匣不足可领取、两项满保留、换弹中补满不重复扣备用全部正常。金色模型亮度可以，但需求明确为类似 Lyra 拾取点下方的发光坐标圈。本轮不修改上述玩法与弹药配置。

- 新增 GroundMarker（引擎 Plane）与程序化材质 M_PickupMarker / MI_PickupMarker，显示双圆环、分段外圈、四向刻度和柔光；透明背景，不是实体圆盘。自制效果，不依赖 Lyra 资产。
- DisplayMesh 清除上轮金色覆盖，恢复网格默认材质；旧 M_PickupGold / MI_PickupGold 保留作为对比，当前拾取物不再引用它们。
- GroundMarker 直接挂在 PickupRoot 下，独立于 PresentationRoot。NoCollision、不产生 Overlap、不影响导航、不投影；深度检测保留，不隔墙显示。
- RefreshGroundMarker 在 OnConstruction 和 BeginPlay 投射下方静态地面，贴合法线并上移 2 cm 避免闪烁。无地面时隐藏，无常驻 Tick；未来若做移动拾取台，需要主动刷新位置。
- 当前投射区间为物品中心上方 40 cm 到下方 600 cm，适合本场景悬浮拾取物；跨楼层或动态平台需另行配置投射策略。
- Actor 成功领取后整体隐藏并销毁，地面标记随同消失；无法领取则连同物品保留。

可调参数：选中拾取 Actor 的 Pickup → Marker：Marker Radius 默认 48 cm，Marker Ground Offset 默认 2 cm。材质实例 MI_PickupMarker 中 GoldColor 控制金黄色、GlowStrength 默认 5 控制自发光、LineWidth 默认 0.015 控制圆环宽度。半径仅是视觉大小，与 250 cm 拾取触发器不同。

资源工具 `ShooterPickupSetup -Ring` 只更新圆环材质、三份拾取蓝图和地图实例；`-Ring -Verify` 独立重载核对。不要用历史 `-RefillGold` 恢复旧全模型发光效果。修改前备份 Saved/T15_PickupRing/Backup；本轮构建与关卡验证输出放在 Saved/T15_PickupRing。

本轮最终验证：

- Build.log 与仅调整截图等待的 BuildCapture.log 均编译通过；运行时光圈代码无后续修改。
- Apply.log 和独立 Verify.log 通过：3 个拾取物均使用新光圈材质、展示网格原材质、无标记碰撞；原位置未变，物品 Z=810，光圈 Z=712，地面 Z=710。
- MapVerified.log 与 MapSmoke/Result.txt 为 PASS。实际 F 领取/重复领取、拾取后切枪、步枪实际射击与换弹耗尽到 0/0 后领取至 30/90、霰弹 7/32 至 8/32、HUD 同步均通过。
- 已查看最终 MapSmoke/00-GroundRings.png：三个金色圆环、外圈刻度、透明中心与背景正常，网格原材质恢复；03-Collected.png 确认所有领取后物品及光圈消失。首次着色器未完成的截图保留为 FirstCapture-ShadersPending.png，仅用于排查记录。
- 最终地图日志无 Error 或材质编译失败；资源加载保留旧动画蓝图线程安全与 GameplayCue 路径警告。
- SHA256 核对：Shooter 蓝图、DA_Rifle、DA_Shotgun、ShooterWeaponInventory.cpp 和用户修改的 ShooterDamageLibrary.cpp 与本轮开始前一致。Saved 下备份、日志、截图被 Git 忽略。
- 本轮只复用必要的实际地图回归与资源检查，没有重跑全套自动化或重新打包；未提交、未推送。新的光圈样式待用户验收。

用户已授权本次 T15 拾取功能、补满规则与金色地面光圈提交并推送 develop。下一阶段 T16 采用单次短距离闪避：Shift 触发，沿移动方向，静止时向前，接入 GAS 体力与冷却。
