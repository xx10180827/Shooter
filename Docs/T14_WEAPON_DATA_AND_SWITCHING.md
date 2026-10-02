# T14 武器配置数据化与步枪 / 霰弹枪切换

日期：2026-10-01
状态：双武器功能已获用户整体认可；本次补充独立霰弹枪动画与声音，表现待用户 PIE 验收。T14 尚未提交推送。

## 1. 本轮目标与基线

T13 已提交推送 develop，提交 a415d5b20d0ef0aabc3b168e9306f6592f0f563e（完成 T13：AI受击警觉、枪声调查与移动射击），已核实远端 SHA 一致。用户随后明确选择武器数据化与两把武器切换，并指定 SourceArt 中现有 FBX 为霰弹枪。

本轮使用 SourceArt/Weapons/Shotgun/Meshy_AI_Rustborn_Rifle_0929100508_texture_fbx 中的 FBX 及 BaseColor、Normal、Roughness、Metallic 四张贴图。虽然源文件名含 Rustborn_Rifle，玩法身份按用户本次选择作为霰弹枪；没有重新生成或替换为其他来源模型。

用户额外修改的 bloodstrike 地图、WBP_ShooterMenu 和个人笔记保持原状。本轮只接入 Shooter 蓝图、原生代码及新增武器资产，不保存地图。

## 2. 操作与默认配置

- 1：步枪；2：霰弹枪。重复选择当前槽不产生切换。
- 左键：步枪按住连射；霰弹枪按下一次发射一发，按住不自动连射。
- 右键：沿用已验收的切换瞄准，小红点准星。
- R：当前武器换弹。霰弹枪本轮按整段计时补充弹匣，不是逐颗装填。
- 切枪时退出瞄准、停止旧连射、取消旧换弹。继续开枪需重新按下左键。

| 配置 | 步枪 | 霰弹枪 |
| --- | --- | --- |
| 初始弹匣 / 备用 | 30 / 90 | 8 / 32 |
| 发射模式 | 自动 | 半自动 |
| 每发间隔 | 0.1 秒 | 0.85 秒 |
| 每发弹丸数 | 1 | 8 |
| 每颗弹丸伤害 | 25 | 12 |
| 散布半角 | 0 度 | 4 度 |
| 射线距离 | 100 米 | 35 米 |
| 换弹时长 | 1.5 秒 | 2.4 秒 |

霰弹枪全部弹丸命中同一目标时最多 96 点伤害；伤害随实际命中数量变化。本轮没有距离伤害衰减。一次射击仅扣一颗弹药、播放一次枪声和手臂开火动画；各弹丸具有独立射线及纯表现子弹，仍由 GAS 唯一入口扣血。

## 3. 在哪里调整

内容浏览器：

- /Game/Weapons/Data/DA_Rifle
- /Game/Weapons/Data/DA_Shotgun
- /Game/Assets/Weapons/Shotgun/SM_Shotgun 及 M_Shotgun、四张纹理

Shooter 蓝图的 ShooterWeapon 组件新增 Weapon Definitions，0 号配置为步枪、1 号为霰弹枪。配置包括伤害、弹丸数、散布、射速、弹药初值、换弹时长、枪声、可见子弹、动画和模型挂载变换。

组件原有单武器字段保留给旧蓝图对比；当前已配置 Weapon Definitions 时以 DataAsset 为准。修改数据资产后重新 PIE 生效。每名玩家独立保存弹药和冷却，任何运行时变化都不会写回共享 DataAsset。

## 4. 状态与取消规则

同一个 ShooterWeaponComponent 继续供蓝图、HUD、开镜和 GAS 能力使用，因此原 OnShotFired、OnReloadStarted、OnReloadFinished 绑定无需因切枪重新绑定。

切换顺序：检查是否可切换 → 暂时拒绝回调重入 → 停止射击并取消换弹 → 保存旧槽弹药 / 备用 / 下一次可射击时间 → 装入新配置与状态 → 切换外观并通知 HUD。

若 GAS 作用域锁导致旧能力未结束，会拒绝当前切换，防止旧回调落到新枪。发射结算和补弹通知期间同样拒绝切枪。暂停、菜单、死亡和非法槽位不能切枪。同一把枪切走再切回仍保留原冷却，不能借切枪提高它的射速。

关键场景：步枪剩 12 发，换弹开始后切枪，再切回来；等旧换弹原本完成的时刻过去，仍是 12 发，备用弹药保持 90。只有重新完成一次换弹才会变为 30 / 72。

## 5. 模型和动画边界

FBX 导入后的 LOD0 为 8,789 顶点、10,266 三角形，四张源纹理为 2048×2048。新模型作为静态网格挂在原 Weapon_mesh 节点下，跟随现有手臂动画；切枪仅隐藏 / 恢复原步枪网格，保留原资源及蓝图节点。

T14 初版复用步枪动画及枪声；2026-10-01 补充了由现有动作改编的独立腰射、瞄准开火、换弹动画及两段音效，并绑定到 DA_Shotgun。详见 [霰弹枪表现补充](T14_SHOTGUN_PRESENTATION.md)。2026-10-02 又补充真实视角后坐力和霰弹枪左手握持 IK，修正开火时的掌心悬空，并将换弹声统一为整段操作。当前仍没有独立可动泵、弹仓骨骼或逐颗装填；换弹保留原手部路径。

原换弹蓝图节点仍保留，只把三处固定蒙太奇输入改为 GetReloadMontage，时长继续由动画长度除以配置的换弹时间得出。多弹丸共享一次旧 OnShotFired 表现回调，选第一个命中点播放旧命中特效；本轮不为每颗弹丸额外生成命中特效。

## 6. 代码分工

- ShooterWeaponDefinition：只读共享配置。
- ShooterWeaponComponent / ShooterWeaponLoadout：当前武器执行、独立槽状态与切换互斥。
- ShooterWeaponPresentationComponent：装备模型、原步枪显示与枪口同步；管理可取消换弹音与旧武器开火动作清理。
- ShooterFireAbility：自动 / 半自动发射生命周期。
- MyShooter：1 / 2 输入。
- ShooterAimComponent、ShooterAmmoWidget：读当前配置、刷新动画与界面。
- ShooterWeaponSetupCommandlet：仅编辑器执行 FBX/PBR 导入、数据资产生成、蓝图接线与挂载校准。
- ShooterWeaponSwitchTest、ShooterWeaponSmokeSubsystem：规则回归与实际地图输入检查。

头文件放 Public、实现放 Private，关键流程有中文注释。模型源文件未改写；重复 ZIP 已加入忽略规则，必要 FBX / 贴图和 Content 资源保留，Saved、Binaries、Intermediate 与打包产物仍忽略。

## 7. 自动验证

- 全部 MyShoot 自动化测试 14 项成功，0 失败，0 未运行；其中新增 MyShoot.Weapons.SwitchAndShotgun。
- 覆盖两槽独立弹药、同帧快速切回的射速限制、取消连射、12 发换弹中断与旧回调失效、重新换弹守恒、共享数据资产的角色状态隔离、8 颗弹丸 96 点 GAS 伤害、墙体遮挡、死亡拒绝切换和取消换弹。
- 真实 bloodstrike 输入专项验证 1 / 2 切枪、FBX 显示、HUD、霰弹半自动、右键瞄准、配置动画播放、取消 / 完成换弹、暂停拒绝切换及连射中切枪。
- 自动报告：Saved/T14/AutomationReport/index.json；真实地图记录：Saved/T14/MapSmoke/Result.txt。

最终 UE5.5 Editor 构建成功：Saved/T14/Build8.log。最终真实地图复验 Game3.log 与 Result.txt 均为 PASS。运行时规则在 14 项回归后未再修改，之后仅调整 HUD 排版、检查器截图时序及模型数据；最终地图复验覆盖这些调整。

已实际查看最终普通持枪、开镜和换弹截图，材质已完成编译并正常显示，HUD 数字与武器名称没有溢出。源模型枪口朝负 X，挂载 Yaw=-90 度映射至现有正 Y 枪口方向；缩放 0.47346，挂载位置 (0,20,5) cm，枪口位置 (0,65,10.5) cm。腰射保持项目原来的低持枪姿态，开镜保持中心小红点；手部精细贴合仍依赖配套动画。

截图位于 Saved/T14/MapSmoke/01-Rifle.png、02-Shotgun.png、03-ShotgunADS.png、04-ShotgunFire.png、05-ShotgunReload.png、06-BackToRifle.png。最初的可见性断言误选了编辑器 CameraProxyMesh，专项现已按 EquippedWeaponMesh 名称精确检查。

本轮尚未更新 T08 打包产物，也未把自动验证写成用户人工验收完成。

## 8. 人工 PIE 验收

关闭旧编辑器，使用本次编译结果重新打开 UE5.5 项目后测试：

```text
初始步枪与 30 / 90：
1 / 2 切枪与 HUD：
霰弹枪模型、贴图、枪口方向：
步枪按住连射 / 霰弹枪单次按下：
霰弹每发扣 1 发、散布与伤害：
两把枪各自保留弹匣和备用弹药：
换弹中切枪，切回来不会延迟补弹：
连射中切枪，旧枪和新枪都不自动继续开火：
两把枪右键瞄准、小红点及切枪退出瞄准：
换弹动画与取消后的停止：
暂停恢复、死亡中断：
重新 PIE：
报错：
```

## 9. 官方资料

实现以本机 UE5.5 头文件和编译为准：

- [Epic Data Assets](https://dev.epicgames.com/documentation/unreal-engine/data-assets-in-unreal-engine)
- [Epic FBX Static Mesh Pipeline](https://dev.epicgames.com/documentation/unreal-engine/fbx-static-mesh-pipeline-in-unreal-engine)
