# T14 补充：霰弹枪独立动作与声音

日期：2026-10-02
状态：2026-10-02 用户确认开火声音和其余基础功能正常，反馈缺少真实后坐力、开火手部悬空和换弹音画不一致；本轮针对三项修正，待复验，尚未提交推送。

## 本轮范围

用户确认 T14 整体没有问题，随后要求先完善霰弹枪，再考虑拾取、GAS 闪避冲刺与简历。本轮只完善霰弹枪表现，不实现拾取或冲刺。

保留步枪原资源和当前 Shooter 蓝图节点。使用已有 HeroFPP 骨架动画生成独立副本，调整开火幅度与时间曲线，并生成独立枪声和可取消的换弹机械音。伤害、弹丸数量、弹匣、射速及整段换弹结算规则保持 T14 配置。

## 新增资源与配置

| 资源 | 来源 / 调整 | 时长 |
| --- | --- | --- |
| AS_ShotgunFire / AM_ShotgunFire | FPP_RifleFire；保留骨骼配合并调整时间曲线，当前绑定 Correction 副本 | 0.55 秒 |
| AS_ShotgunAimFire / AM_ShotgunAimFire | AS_PlayerAimFire；保留骨骼配合并调整瞄准开火节奏，当前绑定 Correction 副本 | 0.45 秒 |
| AS_ShotgunReload / AM_ShotgunReload | FPP_RifleReload；重新分配操作节奏并匹配换弹时间 | 2.4 秒 |
| SW_ShotgunShot | 现有原创 RifleShot 瞬态降频，叠加衰减低频和短促噪声 | 0.58 秒 |
| SW_ShotgunWholeReload | 一次取出、插入及锁定的整段动作声 | 2.4 秒 |

内容浏览器目录：

- /Game/Animations/Shotgun：保留首版动画及当前换弹动作；Correction 子目录保存当前开火和瞄准开火副本。
- /Game/Audio/Shotgun：当前使用 SW_ShotgunShot 和 SW_ShotgunWholeReload，原 SW_ShotgunReload 留作对比。
- /Game/Weapons/Data/DA_Shotgun：已绑定新资源，Fire Sound Volume 为 0.75。
- SourceArt/Audio/ShotgunShot.wav 与 ShotgunWholeReload.wav：48 kHz、单声道、16 位 WAV 源文件。

新动画不复制源动画中的音频/玩法 Notify，避免重复枪声和重复结算。射击声音仍由成功发射事件播放一次。普通持枪、开镜和 R 键入口沿用已有逻辑。

## 中断与职责

ShooterWeaponDefinition 新增可选 ReloadSound。ShooterWeaponPresentationComponent 监听现有换弹事件，创建游戏音频组件并保存弱引用：

- 换弹开始：播放当前武器配置的声音，按当前换弹时长调整播放速度。
- 换弹完成或取消：停止声音。
- 切枪、死亡、离开 Playing 状态、组件 EndPlay：停止声音并清理引用。
- 切枪/死亡/离开 Playing：只停止记录的开火蒙太奇，不调用 StopAllMontages，避免误停死亡等其他动作。

声音不负责补弹；补弹仍由原 GAS 换弹流程成功结束时完成。表现组件不另建换弹计时器。

.h 与 .cpp 分置 Public / Private，关键职责和资源生成流程有中文注释。生成工具为 ShooterShotgunPolishCommandlet；默认模式拒绝覆盖已经存在的新资源，已生成后应使用 -Verify 检查。它不是普通游戏运行时功能。

## 已知边界

- 当前枪模是一整件 StaticMesh，没有可独立驱动的护木部件。本轮没有实现真实的独立泵动。
- 换弹仍是完整计时结束后一次性补足弹匣，不是逐颗装填。
- 换弹是原步枪动作的节奏改编，保留了原来的手部路径，不宣称已经完成精确塞入霰弹的专用动作。
- 更精确的握持、装填和泵动需要后续手工动画/IK 调整，以及枪模部件拆分或配套骨骼。
- 音效为现有原创资源加工与程序合成；播放和数据检查不替代用户对音色及动作同步的实际试听。
- 本轮不更新 T08 打包产物。

## 2026-10-01 首轮验证记录（后续修正见文末）

- UE5.5 Editor 编译通过，Saved/T14_Polish/Build3.log。Build2 后仅修正编辑器资源验证的音频格式查询，运行时代码未再改变。
- 全部 MyShoot 自动化回归 14 项成功、0 失败、0 未运行；报告 Saved/T14_Polish/AutomationReport/index.json。
- 资源重新加载验证通过：Saved/T14_Polish/Verify2.log。三套动画的骨架、无重复通知及 DataAsset 引用正常；枪声与换弹 PCM 分别为 55,680 / 230,400 字节，Bink 压缩分别为 5,102 / 27,480 字节。
- bloodstrike 实际地图与音频中断专项通过：Saved/T14_Polish/MapSmoke/Result.txt、Game.log。覆盖真实按键、腰射 / ADS 蒙太奇、音频播放、切枪清理旧后坐力、换弹完成及切枪 / 暂停 / 死亡中断；中断不补弹。专项进程已正常退出。
- 本轮未保存地图、菜单、子弹材质、项目文件和 Shooter 蓝图；与 Saved/T14_Polish/UntouchedBefore.json 的 SHA256 全部一致。已查看 04-ShotgunFire.png 和 05-ShotgunReload.png，模型与 HUD 正常显示；实际手感、握持贴合和听感待用户确认。

初次资源验证失败是 NullRHI Commandlet 的音频播放代理尚未初始化，GetRuntimeFormat 返回 InvalidFormat。验证工具改为显式检查 Windows Bink 压缩数据；保留首轮失败日志，复验结果单独记录。

## 用户 PIE 验收

重新打开本次构建的 UE5.5 项目，开始游戏后按 2：

1. 普通腰射：开火动作和枪声明显区别于步枪；一次按下扣一发。
2. 右键瞄准后开火：小红点正常，播放独立瞄准开火动作。
3. R 换弹：观察 2.4 秒动作与机械声音，结束时补弹。
4. 换弹中按 1，再按 2：旧换弹声停止，未完成换弹不补弹。
5. 换弹中暂停/死亡：声音和动作停止，恢复后不出现旧流程补弹。
6. 开火动作尚未回位时切枪：旧后坐力不继续带动新武器。
7. 切回步枪、重新 PIE：原步枪表现正常，无额外报错。

请重点反馈后坐力幅度、回位速度、换弹手部动作和声音音量；这些属于表现验收，本轮自动检查不替用户作出主观判断。

## 2026-10-02 用户反馈修正

用户确认截图为实际游戏开火时的手部悬空，认可现有厚重开火枪声。本轮保留该枪声及整段补弹规则。

### 真实视角后坐力

新增 ShooterRecoilComponent，监听成功发射事件，一发霰弹只累计一次视角后坐力。默认腰射上抬 2.4 度，瞄准倍率 0.65（实际 1.56 度），在 0.06 秒内施加；配置位于 DA_Shotgun 的 Recoil 分类。

这里改变的是控制器俯仰角 Pitch，不移动角色的世界 Z 坐标。下一发射线继续使用玩家视角，所以实际瞄准方向会上抬。每帧增量叠加到当前 ControlRotation，不覆盖玩家向下压枪输入、不受鼠标灵敏度反转影响。本轮不自动拉回开火前的绝对方向，需要玩家主动压枪。旧步枪默认 RecoilPitch 为 0，保持此前行为。

切枪、换弹、暂停、死亡、失去控制权和退出关卡取消尚未施加的增量；取消不强制跳回旧视角。空枪、射速限制拒绝的发射不产生额外后坐力。

### 开火姿势与挂载检查

已检查 Shooter：Weapon_mesh 连接 CharacterMesh0 的 Right_Weapon 插槽，插槽父骨是 b_RightWeapon；挂载引用有效。原动画骨架为 HeroFPP，手部骨骼为 b_LeftHand / b_RightHand。

第一版将每条骨骼偏移分别放大 1.55 / 1.35，这会破坏本来配套的左右手及武器骨骼相对关系。修正版取消逐骨骼放大，保留原姿势配合并继续重采样时间曲线；实际后坐力交给独立视角组件。

修正资源放在 /Game/Animations/Shotgun/Correction。取消逐骨骼放大后，侧面截图仍暴露左手原有支撑位置与新枪护木不匹配；这一步单独不足以解决悬空，旧 MapSmoke 截图不作为通过证据。

进一步在 Shooter_idle 的最终输出前加入 Local To Component → Two Bone IK → Component To Local，原 9 个动画图节点全部保留，新增 6 个节点（含缓存属性读取）。IK 约束 b_LeftHand，目标相对 b_RightWeapon；实际支撑点配置在 DA_Shotgun 的 Grip 分类。动画实例在游戏线程把 Right_Weapon 插槽配置转换成目标骨骼局部坐标，图中只读缓存，避免追随上一帧手位置产生累积漂移。

仅霰弹枪存活且非换弹时启用，切回步枪、换弹与死亡时关闭。右手和整枪挂载没有移动。最终目标由原姿势的支撑点沿插槽坐标后移 6 cm、上移 5 cm；通过 -Grip -DZ=5 校准，数据资产是运行时配置来源。原版本动画与节点保留供对比。

Blueprint 变更仅为本次必需的 Shooter_idle；备份位于 Saved/T14_Revision/Backup/Shooter_idle.uasset。Shooter 角色蓝图、地图、菜单以及已认可的开火音效不需改写。

### 整段换弹声音

DA_Shotgun 的 ReloadSound 改为 SW_ShotgunWholeReload，源文件 SourceArt/Audio/ShotgunWholeReload.wav。新声音按一次取出摩擦、一次插入闷响、结束锁定编排，移除原先五段类似逐颗装填的短促点击。时长仍为 2.4 秒，换弹中断时同样停止。

没有把本轮改成逐颗装填；原换弹动画、弹药计时和结算规则保持一致。SW_ShotgunShot 和源 ShotgunShot.wav 保持用户已验收版本。

### 修正验证

- 构建：Saved/T14_Revision/Build7.log 成功；Build5 已包含完整运行时改动，Build6/7 仅修正独立测试的本地玩家连接及对象归属。
- 自动回归：原有 14 项在 AutomationFinal 报告中全部通过；新增 MyShoot.Weapons.RealViewRecoil 在修正测试本地玩家初始化后，RecoilVerified/index.json 为 1 成功、0 失败、0 未运行。控制器与实际相机均读到 2 度（3 度后坐力减去玩家 1 度压枪），并覆盖瞄准倍率、八颗弹丸只触发一次、空枪、中断与死亡。运行时代码未因测试初始化修正而改变。首轮失败报告保留，不冒充一次全绿报告。
- 真实地图：Saved/T14_Revision/AfterIK/GameFinal.log 与 Result.txt 为 PASS。新支撑点下的持枪、开火和回位骨骼误差小于 0.01 cm，换弹释放 IK、步枪关闭 IK、瞄准真实上抬 1.56 度，以及声音中断和弹药守恒均通过。已查看最终侧面图，原明显悬空消除；手指精细贴合仍由用户视觉验收。最新图片为 Saved/T14_Revision/FinalGrip/Idle.png、Fire.png、Recovery.png。
- 哈希：Shooter、地图、菜单、子弹材质、项目文件、SW_ShotgunShot 和源 ShotgunShot.wav 与本轮开始前相同。Shooter_idle 是本轮必要变更，保留原 9 个图节点并新增 6 个 IK 接线节点，有单独备份。

### 本次复验操作

重新打开项目后开始游戏，按 2 切换霰弹枪：

- 腰射 / 开镜开火：观察实际视角上抬，鼠标向下可压枪；手臂动作回位不强行重置瞄准方向。
- 左手握持：普通持枪、开火和回位不再出现原来的明显悬空；按 R 时左手释放，换弹结束重新握持。
- 音画：保留原厚重开火枪声，换弹改为整段操作声，不模拟逐颗入仓。
- 切枪 / 暂停 / 死亡：没有剩余后坐力继续施加，没有残留换弹声和旧流程补弹。
- 重新 PIE：模型、准星、弹药 UI 和原步枪正常。

本轮只完成本地修复与验证，未提交、未推送、未更新可运行包。截图与测试日志位于已忽略的 Saved 目录。

### 用户验收回填及步枪后续（2026-10-02）

用户确认霰弹枪腰射/开镜上抬和压枪、左手持枪/开火/回位贴合、整段换弹及声音停止、切回步枪与重新 PIE 正常。换弹仍为动画结束统一补弹。随后单独授权开启步枪真实后坐力，因此上文“步枪为 0”描述的是霰弹枪修正阶段的历史状态；当前步枪参数与验证见 [步枪后坐力](T14_RIFLE_RECOIL.md)。
