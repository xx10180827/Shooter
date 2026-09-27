# T09 第一项：AI 攻击动画与武器枪声

本项只补充攻击表现与枪声，沿用既有 GAS 伤害、射速、弹药、换弹和死亡规则。用户已整体确认本项完成，并授权提交 Git。T08 旧运行包不包含本项，先在 UE 编辑器中验收。

## 已有动画核查

2026-09-27 使用 UE 5.5 资源注册表实际加载了 Content/Assets/Characters/TTP_Animations 中的 22 个动画，骨架均为 HeroTPP_Skeleton。

- 9 个 AimOffset：前、上、下、左、右与四个斜向瞄准姿势，时长约 0.033 秒。
- Idle、Equip、Death、Reload、Launcher_Reload。
- JumpStart、JumpLoop、JumpEnd。
- Run_Fwd、Run_Bwd、Run_Lt、Run_Rt、RoadieRun_Fwd。

目录中没有独立的 Fire/Recoil 攻击序列。现有 FPP_RifleFire 和 Shooter_fire_Montage 使用 HeroFPP_Skeleton，继续用于玩家。只读检查记录位于 Saved/T09/InspectAssets.log。

本次从原 AimOffsetFwd 与 AimOffsetUp 的骨骼数据制作 0.3 秒的单发抬枪回位动画：只混合上半身，峰值取向上瞄准姿势的 12%，以原前向姿势为叠加基线。它是基于原资产补出的基础攻击表现，不把原瞄准姿势说成现成的开火动画。原 TTP 动画不修改。

## 动画与伤害如何配合

1. AI 进入现有 0.3 秒前摇。
2. 前摇结束重新检查自身/玩家死亡、攻击范围、墙体和对局状态。
3. 有效时播放一次 AttackMontage 与 AttackSound，再执行原来的 10 点 GAS 伤害。
4. 躲开前摇不播放开火表现，也不扣血；死亡、结算或取消时只停止本次攻击蒙太奇。

Boot_Shooter_AnimationBP 的 Alive 状态中增加 DefaultSlot，原移动节点保留；死亡分支保留。蒙太奇不通过 AnimNotify 再施加伤害。

## 资源与配置位置

| 用途 | 内容浏览器位置 / 配置 |
|---|---|
| AI 后坐力序列 | /Game/Animations/AS_AI_RifleRecoil |
| AI 单发攻击蒙太奇 | /Game/Animations/AM_AI_RifleFire |
| 基础枪声 | /Game/Audio/SW_RifleShot |
| AI 枪声距离衰减 | /Game/Audio/SA_RifleShot |
| AI 表现参数 | Boot_Shooter_controller → Class Defaults → Shooter / AI / Presentation |
| 玩家枪声参数 | Shooter → ShooterWeapon 组件 → Shooter / Weapon / Audio |

枪声是本次原创合成的基础音效，0.24 秒、48 kHz、单声道、非循环，源文件为 SourceArt/Audio/RifleShot.wav；可直接更换 FireSound / AttackSound，不必改伤害代码。玩家本地使用 2D 枪声，AI 使用空间声与距离衰减。默认音量倍率均为 0.65。

玩家枪声只在 TryFireOneShot 成功发射后播放。旧 GAS_PlayFireEffects 继续负责玩家动画和枪口粒子；不要在那里再加同一份枪声，以免重叠。空弹、换弹、射速冷却和菜单状态不会播放新枪声。

## 验收

- 玩家点射：每发有一声枪响，原动画/粒子正常；按住连射与射速一致。
- 空弹、换弹期间：按开火没有枪声。
- AI 靠近攻击：能看到上半身抬枪回位，枪声与实际扣血发生在同一击。
- 前摇中离开范围或躲到墙后：该次攻击不扣血、不播放枪响。
- AI 攻击中死亡：停止攻击，原死亡动画和尸体清理正常。
- 暂停、继续、重开：无额外枪声循环或残留攻击。

## 实现与保留

.h 与 .cpp 分别置于 Public / Private，新增接口和关键流程有中文注释。表现资源由编辑器专用 ShooterCombatPresentationCommandlet 创建和校验；不会进入正常游戏运行路径。迁移前蓝图副本保存在 Saved/T09/Backup。

旧玩家/AI 事件图节点没有删除；敌人动画图只在 Alive 姿势链中插入 Slot。无需重新设置角色父类。

引擎机制参考：[Epic Animation Slots](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-slots-in-unreal-engine)。本项目编译与验证以本机 UE 5.5 为准。

## 自动验证结果

- UE 5.5 Development Editor 编译通过：Saved/T09/BuildEditor.log。
- 保存后资源校验通过：敌人骨架、叠加动画、Alive Slot、蒙太奇、玩家/AI 枪声引用均正确。
- 启用 -AllowCommandletAudio 后，音效原始 PCM（23040 字节，48 kHz，单声道）与运行时压缩数据验证通过：Saved/T09/VerifyAudio.log。WAV 峰值 23647/32767，非静音且未削波。
- 原有 9 项 MyShoot.GAS 回归全部通过：Saved/T09/AutomationReport/index.json。
- 新增 MyShoot.Presentation.AIAttack 专项复验通过：Saved/T09/PresentationReport/index.json。实际使用保存后的 Boot_Shooter_BP、控制器与 AnimInstance，验证前摇不播放、伤害发生时蒙太奇实际播放、取消和死亡停止、死亡后无延迟伤害。
- 首次专项测试把结算时刻固定为 0.52 秒，受到 TimerManager 帧边界影响而失败；修正为等待实际攻击结算后复验通过。首次报告保留，不伪装为首轮全部通过。
- 导入阶段发现无声命令行不自动注册音频解码器，编辑器工具已显式注册 BinkAudioDecoder；声音校验必须启用 -AllowCommandletAudio，不使用 -nosound。保存后复验无该错误。
- 原 TTP 动画无 Git 差异。原地图、项目文件和此前 T08 本地改动未由本项覆盖。

人工边界：自动测试确认动画播放/中断与音效数据及引用，未代替玩家对动画幅度、音色、空间声定位和操作手感的 PIE 验收。原 AnimBP 的线程安全警告、预览场景无 Pawn Owner 警告仍保留，未扩展到本项之外进行重构。

## 用户确认与提交（2026-09-27）

用户确认“完成了 AI 的攻击动画以及开火声音”，授权提交本项。提交包含本项 C++、蓝图、动画、声音、编辑器工具、专项测试和文档；T08 打包改动及其他本地修改另行保留。
