# T04 蓝图接线与旧节点对比

项目：D:\UE5_My_Project\MyShoot 5.5<br>
操作日期：2026-09-25<br>
用户要求：直接接好蓝图，尽量保留原节点，用于比较普通蓝图与 GAS。

状态：接线已保存，C++ 工具及 Shooter 蓝图编译通过，实际输入与表现触发验证通过。2026-09-25 用户完成 PIE 验收，反馈各项正常、无报错。

## 1. 打开哪里检查

打开 Content/Blueprints/Shooter。

- EventGraph 的绿色注释框：当前使用的 GAS 输入。
- EventGraph 的蓝色注释框：BeginPlay 绑定、HandleWeaponShot 回调。
- EventGraph 的橙色注释框：已经断开输入的旧 Start_fire / End_fire 调用节点。
- 函数分类 GAS > Weapon：新函数 GAS_PlayFireEffects。
- 函数分类 Legacy > 保留对比：原 Start_fire、End_fire、Shoot_Once。

原函数保留的是你本次修改前保存的完整状态：既包括旧蓝图节点，也包括 T02 的 GAS 伤害过渡节点和你尝试连接 GAS 的节点。它们不是纯蓝图版的一键运行开关；当前输入只执行绿色区域。

## 2. 当前执行路径

```text
InputAction fire
├─ Pressed  → Get Shooter Weapon → Start Firing
└─ Released → Get Shooter Weapon → Stop Firing

BeginPlay
└─ Bind Event to On Shot Fired
   ├─ Target = Get Shooter Weapon
   └─ Event  = HandleWeaponShot

C++ 每发成功结算
└─ HandleWeaponShot(Blocking Hit, Hit Result)
   └─ GAS_PlayFireEffects(Blocking Hit, Hit Result)
      └─ Play Anim Montage（原 Shooter_fire_Montage）
         └─ Spawn Emitter Attached（GAS 单发枪口粒子，挂接原 Muzzle）
```

新表现函数不执行射线、不扣弹、不结算伤害、不创建连射计时器。实际命中与打空都会播放开火表现；没有弹药时 C++ 不广播新的发射事件。

Blocking Hit 和 Hit Result 已传入新函数。目前旧图中没有枪声、弹壳或命中特效节点，因此本次复用的是实际已有的蒙太奇和枪口粒子。以后增加命中特效可在此函数使用这两个参数。

## 3. 保留了什么

- 所有原节点保留，未删除旧函数。
- 原 Start_fire、End_fire、Shoot_Once 内部执行连线和数据连线保留。
- 原先 EventGraph 中调用 Start_fire / End_fire 的节点保留，只断开 InputAction fire 对它们的连接。
- 原移动、视角、跳跃、模型、AnimBP 和死亡蓝图不在本次接线范围。
- 原枪口粒子 P_AssaultRifle_MF 和原开火蒙太奇保持原样。

蓝图函数分类和中文注释用于说明哪些路径当前在执行，哪些仅供对比。

## 4. 为什么新增单发粒子副本

原枪口粒子的所有已检查发射器都设置为 Emitter Loops = 0，即无限循环。旧流程在开始按键时创建持续特效，松开时销毁。

GAS 的 OnShotFired 每发触发一次。如果照搬原粒子，就会每发创建一个无限循环组件。因此复制了一份：

`Content/Assets/Effects/ParticleSystems/Weapons/AssaultRifle/Muzzle/P_AssaultRifle_MF_GAS.uasset`

副本各发射器：
- Emitter Loops = 1。
- Emitter Duration = 0.05 秒，关闭时长随机范围。
- 蓝图 Spawn Emitter Attached 保持 Auto Destroy = true。

0.05 秒表示发射阶段时长，不代表所有烟雾、火花都在 0.05 秒内消失；粒子生命周期继续沿用原资源，结束后组件自动销毁。松开时停止产生新效果，已经产生的效果自然结束。

原蒙太奇长度约 0.167 秒，Default 段不循环，直接复用，无需修改资源。

## 5. 你需要检查的实际画面

1. 开火是否正常播放动作和枪口特效，位置是否仍在原 Muzzle。
2. 按住连射，松开后不再产生新射击。
3. 空弹匣后按键，不再播放新的开火动作或枪口特效。
4. 敌人每发掉血正确，没有重复伤害，死亡动画及尸体清理正常。
5. 快速点按不会额外生成多份持续枪口特效。
6. 重新 PIE，弹药和角色状态恢复正常。

自动验证能检查事件、组件与蒙太奇是否触发，最终枪口外观、动画融合和射击手感需要你在 PIE 中确认。

## 6. 恢复点与工具

修改前 Shooter 备份：
`Saved/T04/BlueprintWiring/Shooter-Before-20260925-094648.uasset`

修改前图结构与节点文本：
`Saved/T04/BlueprintWiring/Inspect/`

迁移后图结构：
`Saved/T04/BlueprintWiring/After/`

插件 `Plugins/ShooterEditorTools` 是仅编辑器使用的 UE C++ 工具，负责通过引擎 API 读取、连接、编译和保存蓝图。它不进入游戏运行时，不要求 Python。工具的头文件与实现也按 Public / Private 放置，并有中文注释。

迁移有目标蓝图、节点 GUID 和重复运行检查，不能对任意蓝图盲目套用。完成后只需正常打开项目，不要再次执行迁移。

## 7. 验证记录

- 原有 74 个节点全部保留；Start_fire / End_fire / Shoot_Once 内部引脚连线、默认值和资源引用全部与修改前一致。
- 修改前记录的 324 个 Content / Config 文件中，仅 Shooter.uasset 改变；另新增 P_AssaultRifle_MF_GAS.uasset，原粒子及蒙太奇未改变。
- 编辑器工具 C++ 构建通过；Shooter 蓝图编译通过。
- 重新加载 Shooter 后，实际执行编译后的 fire 输入委托：BeginPlay 绑定成功；Pressed 扣一发弹药；一个枪口组件挂在原 Muzzle；原蒙太奇开始播放；Released 停止；旧 Shoot_Once 定时器未启动；死亡取消成功。
- 最终验证命令退出码 0，汇总 0 错误、7 条既有警告（动画线程安全与 GameplayCue 默认搜索路径）。
- 初次无渲染验证因引擎跳过粒子创建而失败；改用 AllowCommandletRendering 后，粒子与蒙太奇验证均通过。没有把无渲染结果当成视觉验证。
- 中文注释框已重新加载核对，IDE 工程已刷新。实现阶段尚未提交；用户现已授权随 T02–T04 完整版本提交至 develop。

结果文件：Saved/T04/BlueprintWiring/FinalVerify.log、PreservationReport.json、Final/graphs.json。实际光照、枪口位置观感、动画融合和手感仍以你的 PIE 验收为准。

## 8. 用户验收完成（2026-09-25）

用户确认新 GAS 接点已启用；按下、按住、松开、快速点按、扣弹、每发 25 伤害、空弹匣限制、现有开火表现、死亡清理及重新 PIE 均正常，无报错。T04 完成，并授权提交 Git。完整反馈见 [T04 验收记录](T04_GAS_FIRE.md#9-t04-用户验收记录2026-09-25)。
