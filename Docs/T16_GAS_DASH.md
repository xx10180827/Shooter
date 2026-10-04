# T16 GAS 单次地面闪避

日期：2026-10-03。T15 已提交推送 develop，提交 cc148fae6151a853f02fef19ea815839621c1e33。本阶段已于 2026-10-04 由用户完成 PIE 验收，并授权提交推送 develop。

## 已确认范围

- 单次按 Left Shift；有移动输入时沿输入方向，没有输入时沿视角的水平方向前冲。
- 仅地面短距离闪避，默认距离 480 cm、时长 0.22 秒；不能空中发动，不附带无敌帧。
- Stamina / MaxStamina 初始 100/100；一次消耗 25，冷却 1.2 秒。冷却结束后以每秒 10 点恢复至上限。
- 闪避中禁止开火、换弹、切枪和开镜；开始闪避会停止旧连射、退出开镜、清除剩余视角后坐力。换弹中暂不允许闪避，拒绝时不扣体力。
- 位移由 CharacterMovement 的 Root Motion Source / AbilityTask 执行，通过正常碰撞扫掠，不直接传送，不关闭胶囊碰撞。斜向撞墙可能沿墙滑动。
- 正常结束移除位移源并停止旧速度；死亡、暂停、进入菜单/结算、失去控制器和退出关卡均取消。取消不退回已消耗体力或已提交冷却；实际暂停时冷却/恢复随世界时间暂停。
- 体力为屏幕中央偏下的弧线；闪避图标在现有血条 HUD 右侧，显示 READY / 剩余秒数 / LOW / DASH。位置通过 HealthPanel 实际几何计算，随 DPI 缩放。
- 简单收枪使用原有手臂网格位置偏移，镜头增加少量 FOV 并平滑恢复；未新增专用闪避动画、无敌特效或镜头震动资产。

## 结构和 GAS 生命周期

Left Shift → AMyShooter::DashInput → UShooterDashComponent::TryDash → ASC 尝试激活 UShooterDashAbility → CanActivateAbility 检查体力、冷却、死亡/换弹和地面 → CommitAbility 提交消耗 GE 与冷却 GE → State.Dashing → ApplyRootMotionConstantForce AbilityTask → EndAbility 清理任务/标签/速度/表现。

- ShooterDashComponent：能力授予、输入入口、方向查询、冷却/体力读值、恢复调度和对局状态取消；不逐帧修改角色坐标。
- ShooterDashAbility：能力生命周期与 Root Motion 任务，包含作用域锁和回调取消后的再检查。
- ShooterDashCostEffect：Instant，Stamina -25；由标准 CommitAbility 的 Cost 检查与提交管理。
- ShooterDashCooldownEffect：HasDuration，1.2 秒；通过 UE5.5 TargetTagsGameplayEffectComponent 授予 Cooldown.Dash，交由 GAS 到期移除。
- ShooterStaminaRegenEffect：Instant，Stamina +2；组件每 0.2 秒检查存活、Playing、无闪避/冷却、未满后应用 GE。这个定时器仅调度恢复，不负责能力冷却。
- ShooterAttributeSet：Stamina / MaxStamina 和数值范围约束；初始属性 GE 初始化 100/100。
- ShooterAimComponent：统一合成开镜和闪避的 FOV/手臂位置，避免两个表现组件互相覆盖。默认增加 FOV 7 度，手臂相对偏移 (-8,0,-12) cm。
- ShooterDashWidget：原生绘制弧形和双箭头图标，只读玩法状态，没有第二套冷却计时器。

本项目当前为单机角色自持 ASC，能力沿用 ServerOnly 策略；此阶段不宣称实现多人移动预测或服务器校正。

## 可配置位置

Shooter 蓝图继承原生 ShooterDash 组件，无需人工添加重复组件。Class Defaults 中选择该组件可设置 Dash Distance（50–1000 cm）、Dash Duration（0.05–0.8 s）。数据会在使用时约束范围，防止无效时长。

体力消耗、冷却、恢复当前分别集中在 ShooterDashEffects.cpp 的三个原生 GE 构造函数，初始上限在 ShooterInitialAttributesEffect.cpp。修改数值后需重新编译；后续需要策划调参时可再做统一配置资产，避免本轮提前扩展范围。

所有 .h/.cpp 继续按 Public / Private 对应目录组织并保留中文注释；既有蓝图输入/表现节点未删除。原拾取、光圈和弹药配置无资产改写。

## 用户验收

1. Start Game 后体力满，中央偏下有弧形，血条右侧有 SHIFT / READY 图标。
2. 原地按 Shift 向前闪避；W/A/S/D 移动时按 Shift，分别沿对应输入方向移动。
3. 一次体力减少 25，图标显示约 1.2 秒冷却；连续快速按 Shift 不重复扣除。冷却后体力缓慢恢复。
4. 闪避中按左键、R、1/2、右键，不开火、不换弹、不切枪、不进入开镜；换弹中 Shift 无效且不扣体力。
5. 贴墙向墙冲刺不穿墙；在空中不能发动。
6. 闪避时手臂略收、视野略扩，结束回位；暂停/死亡中断无残留偏移，恢复游戏不继续旧冲刺。
7. 体力不足 25 时无法发动；重开 PIE 重新满体力、没有旧冷却或旧 UI。
8. 不同窗口尺寸下，体力弧线和冷却图标位置正常，不遮挡准星/血量数字。

## 验证与修复记录

- 原生冷却 GE 的标签组件最初使用 FindOrAddComponent，触发构造期匿名 UObject 断言；已改为 CreateDefaultSubobject 的具名默认子对象并注册 GEComponents。Build4 后项目正常启动。
- 最初回归 15/17 成功，射击和换弹测试仍把角色总能力数应为 0 当作武器撤销条件。已改为分别验证 Fire / Reload 撤销、独立 Dash 保留。Build5 后 AutomationFinal/index.json：17 成功、0 失败、0 未运行。
- 实际地图首次验证 Shift、体力、互斥和暂停清理通过，并已查看体力弧线及血条右侧图标；但测得截图卡顿时距离 397 cm，偏离配置 320 cm。
- 已针对当前 DashMotion 清除引擎恒定力默认 DisablePartialEndTick 标记，使结束帧只按剩余时长积分；不会修改全局移动组件或引擎源码。Build6 编译通过，DashFrameVerified/index.json 专项通过，正常帧与 0.07 / 0.12 / 0.10 秒长帧组合均测得 320.15 cm，撞墙、体力/冷却、互斥、暂停和死亡检查也通过。
- 首轮启动错误、旧测试失败和距离偏差日志均保留于 Saved/T16_Dash，最终结果以相应修复后报告为准，不把首次运行表述为全部通过。

最终交付检查：

- Build6.log 编译成功；最终实际地图 MapVerified.log / MapSmoke/Result.txt 为 PASS，真实 Shift 测得位移 320.15 cm，一次消耗 25，冷却结束恢复体力；实际 P 暂停取消闪避，恢复后无旧位移，FOV 正常复位。
- 已查看最终 02-DashingHUD.png、03-CooldownHUD.png：中央偏下体力弧线为 75，闪避中手臂收回和视野反馈可见，左下血条右侧显示双箭头与 DASH / 0.7s。正常帧为蓝色 READY，冷却为橙色。UI 在 1280×720 验证；其他尺寸仍列入用户验收。
- 最终地图无 Error；旧动画蓝图线程安全、GameplayCue 路径回退及引擎平台图标缺失警告保留。没有改写动画蓝图或关卡资产来掩盖警告。
- git diff --check 通过。Saved 构建、测试报告和截图被忽略。T16 未提交推送、未重新打包；用户原有本地编辑和个人文档保留。

## 2026-10-04 验收反馈：更换 Energy UI 与加长距离

用户已确认原地/方向闪避、冷却、体力恢复、换弹互斥、开火/切枪互斥、不穿墙、收枪镜头回位正常。此次要求采用 SourceArt 新体力条，距离增加 1.5 或 2 倍。

- 先采用 1.5 倍：DashDistance 从 320 改为 480 cm，时长仍为 0.22 秒，因此速度也提升 1.5 倍。消耗 25、冷却 1.2 秒、恢复每秒 10 点等规则保持。若下一轮想尝试 2 倍，可在 Shooter 蓝图的 ShooterDash 组件把 Dash Distance 设为 640 cm。
- 用户原图 SourceArt/UI/Energy.jpg 是带有棋盘格底色的 JPEG。内置 image_gen 生成透明衍生图 Energy_Transparent.png，原文件保持不变。过程与完整提示词位于 SourceArt/UI/Energy_PROCESS.md。
- 新资源 /Game/UI/T_StaminaEnergy、/Game/UI/M_StaminaEnergy；材质参数 StaminaFraction 读取真实 Stamina/MaxStamina，仅降低已消耗区域的蓝色发光强度，金属边框持续显示。没有把固定体力数字绘入图片。
- 新体力框位于原来的中央偏下区域，显示尺寸 270×90 个 UI 逻辑单位，随 DPI 缩放；血条右侧冷却图标保留。原生绘制的旧弧线留作缺失资源兜底和对比。
- UShooterDashWidget 使用可编辑软引用加载 UI 材质，动态实例保存在 UPROPERTY，控件结束时释放；每帧只在体力比例变化时更新材质参数。
- ShooterStaminaUI Commandlet 用于导入/配置，加 -Verify 只读加载检查 UI 资源和玩家默认距离；无关卡资产覆盖。源图和当前地图哈希记录在 Saved/T16_EnergyUI/Protected.json。
- 本次暂不提交推送或打包。验收重点：新框外部透明、蓝色部分随体力变化、冷却图标不移位、480 cm 的距离和速度手感。
### 新体力 UI 的最终渲染验证

- 首次截图只有数字、没有体力框，截图左上角仍显示 Preparing Shaders (2)。此时只能确认资源导入，不能算 UI 渲染验收通过。
- 已修正实际地图验证流程：等待着色器编译结束，再留一秒给渲染线程后截图；不修改玩家输入或玩法规则。
- Build3.log 编译成功。MapVerified.log / MapSmoke/Result.txt 为 PASS；已人工查看 01-ReadyHUD.png 与 03-CooldownHUD.png，完整弧形透明边框可见，100 为满亮度，75 时右侧四分之一区域变暗。体力数值与材质均读取实际 GAS 状态；血条右侧冷却图标保持原位置。
- 新距离专项 DashReport/index.json 为 1 成功、0 失败；正常帧与长帧组合均测得 480.22 cm。实际地图 Shift 也测得 480.22 cm，并通过冷却、恢复、禁止开火、镜头复位以及 P 暂停/恢复清理检查。
- UE 编辑器初次打开新材质时需要等待着色器准备完成；截图验证现在包含这一步。验证截图、构建与测试报告在 Saved/T16_EnergyUI，被 Git 忽略。
- 用户尚未复验本轮新图案和加长后的距离手感；未提交、未推送、未更新打包产物。

## 2026-10-04 最终验收与提交授权

用户确认整体无问题，授权提交推送。本次提交完整 T16 GAS 地面闪避、体力消耗/恢复与冷却、互斥和取消清理、新 Energy 体力 UI，以及 480 cm 距离调整。此前的未验收/暂不推送条目保留为历史记录，以此处最新验收为准。构建及测试采用上一轮最终通过结果；本次仅整理文档与提交范围，不改运行逻辑。
