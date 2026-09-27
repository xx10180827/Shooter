# T07：对局流程、弹药 HUD、开始菜单与子弹表现

日期：2026-09-26<br>
项目：`D:\UE5_My_Project\MyShoot 5.5`<br>
基线：develop / `6a957dd`<br>
状态：2026-09-27 用户确认没有问题，授权提交并推送 develop。实现、构建与自动检查已完成。

## 1. 本次范围

接续 T07：弹药 HUD、胜负判定、关卡重开；增加用户要求的开始菜单和飞行子弹模型。
用户确认子弹只做飞行表现，继续沿用当前射线伤害。原有血条、准星、射击音效/特效、换弹及死亡动画保留。

头文件位于 Public，实现位于 Private，按 Game / UI / Player / Weapons / Tests 分目录，关键规则添加中文注释。

## 2. 操作与界面

原地图 bloodstrike 使用 Mygame_GM，后者继承新增 ShooterGameMode，默认玩家与控制器蓝图引用保留。

| 状态 / 操作 | 行为 |
| --- | --- |
| 进入关卡 | 显示开始菜单，禁止射击/换弹，AI 不攻击 |
| START GAME | 开始本局；暂停时继续本局 |
| P | 打开或关闭暂停菜单；PIE 优先用此键 |
| Esc | 独立运行时也可开关暂停；编辑器默认可能先截获 Esc 结束 PIE |
| END GAME | 暂停时结束本局并重新载入开始菜单；尚未开始时禁用 |
| QUIT GAME | PIE 中退出预览，独立运行时退出游戏 |
| 玩家死亡 | 显示 DEFEAT，停止战斗，取消换弹 |
| 已登记敌人全部死亡 | 显示 VICTORY，停止战斗 |
| RESTART | 重载当前关卡并自动开始新一局 |
| MAIN MENU | 重载当前关卡并显示开始菜单 |

菜单使用玩家控制器创建的屏幕 UMG，鼠标可操作；回到游戏后恢复游戏输入和鼠标捕获。菜单期间隐藏原准星、血条和弹药 HUD，继续游戏后恢复。开始/暂停期间暂停世界时间；结算后停止战斗，保留死亡表现和清理。

开始菜单直接使用用户提供的 BeginPlay.jpg，保留原样，未重绘。原图中心三处按钮上叠加透明的实际 UMG Button，提供悬停与点击反馈；竖版图片等比居中，宽屏两侧由深色底覆盖。图片按钮文字是原图内容，暂停时底部说明 START GAME 表示继续。

原图副本：`SourceArt/UI/BeginPlay_reference.jpg`。引擎纹理：`/Game/UI/T_ShooterStartMenu`。原下载文件没有修改。

## 3. 弹药 HUD

右下角显示弹匣与备用弹药，例如 `30 / 090`，并显示换弹、空弹和操作提示。
初次读取武器实际值，后续订阅 OnAmmoChanged、OnReserveAmmoChanged、OnReloadStarted、OnReloadFinished。
不新增另一套弹药变量来控制射击，也不使用每帧轮询。
Pawn 变更和退出时解除旧武器订阅。

资源：`/Game/UI/WBP_ShooterAmmo`。原屏幕血条仍为 `WBP_ShooterHealthBar`。

## 4. 对局规则

ShooterGameMode 保存 Menu / Playing / Paused / Won / Lost。
角色 BeginPlay 幂等登记，GameMode 启动时再补扫一次：

- AMyShooter 作为当前单机玩家。
- 其余使用 ShooterAIController 或其子类的公共 GAS 角色登记为敌人。
- 没有登记过敌人的空关卡不会立即胜利。
- 新增的死亡确认事件在死亡状态和标签建立后、蓝图死亡表现前广播，避免表现立即销毁角色时漏记死亡。
- 本帧的死亡在世界 Actor Tick 结束后统一判定；同帧玩家与最后一个敌人死亡，失败优先。
- Won / Lost 一经确定不再被后续死亡覆盖。
- 直接 Destroy 活敌人不算击杀；本阶段不包含中途移除活敌人的刷怪/撤退规则。
- 结算与菜单禁止武器能力请求，撤销 AI 前摇和移动，取消玩家射击/换弹。
- 重开使用 OpenLevel 重建角色、ASC、能力、定时器与界面，不做原地复活。

仍是单机实现，不代表已实现多人 GameState 复制或网络预测。

## 5. 飞行子弹模型

使用 UE C++ 生成简单的金色低面数弹头网格，沿本地 X 轴朝前，约 8 cm 长、1.3 cm 直径；无需外部 3D 服务。

资源：

- `/Game/Weapons/SM_ShooterBullet`
- `/Game/Weapons/M_ShooterBullet`
- `/Game/Weapons/BP_ShooterBulletVisual`

ShooterWeapon 组件新增 Bullet Visual Class / Bullet Visual Speed / Muzzle Component Name，蓝图已配置子弹类，默认寻找名为 Muzzle 的现有枪口组件。

一发成功射击的顺序：

1. 原射线结算一次伤害、扣一发弹药。
2. 从枪口向射线命中点或射程终点生成一个飞行模型。
3. 表现路径额外检查枪口遮挡，模型不穿过路径上的墙。
4. 到达端点后销毁，无碰撞、无重叠、无伤害回调，不重复播放命中特效。

默认视觉速度 18000 cm/s；近距离至少保留约 0.06 秒飞行便于观察，伤害仍然即时结算。表现速度可以在 Shooter 的武器组件默认值中调整。摄像机射线和枪口遮挡仍可能在贴墙时不同：这里只裁剪可见子弹，不改变原射线伤害规则。

## 6. 主要代码

路径均相对 Source/MyShoot：

| 代码 | 职责 |
| --- | --- |
| Public/Private/Game/ShooterGameMode | 对局状态、敌人登记、帧末胜负和关卡重载 |
| Public/Private/UI/ShooterAmmoWidget | 事件驱动弹药显示 |
| Public/Private/UI/ShooterMenuWidget | 原图按钮、结算按钮和菜单按键 |
| Public/Private/Player/ShooterPlayerController | 创建屏幕 UI、输入模式、暂停和解绑 |
| Public/Private/Weapons/ShooterBulletVisual | 无伤害飞行模型和自动清理 |
| Weapons/ShooterWeaponComponent | 对局门禁与单发子弹表现生成 |
| Characters/ShooterCharacterBase | 可靠的一次性死亡确认通知 |
| AI/ShooterAIController | 菜单与结算时停止战斗 |
| Private/Tests/ShooterRoundTest.cpp | 对局、弹药及子弹表现测试 |

编辑器接入/验证工具为 Plugins/ShooterEditorTools 中的 ShooterRoundCommandlet、ShooterRoundWiring、ShooterRoundVerification。
工具只更换 GameMode 父类和资源引用，不删除原蓝图节点。编辑器工具模块关闭 Unity 合并编译，避免此前多个独立迁移文件中的匿名辅助函数在合并编译时重名。

## 7. 验证与备份

- C++ Development Editor / Win64 构建通过。
- GAS 自动测试 9/9 通过，0 失败、0 未运行，包含原有战斗回归、对局规则、弹药和子弹表现。
- 12 个项目蓝图编译通过，0 错误、0 加载失败；原有两个 AnimBP 的 6 条警告保留。
- 保存后独立重新加载验证通过：GameMode / 默认玩家 / UI / 子弹引用正确，真实 START GAME 与继续按钮可切换对局状态，一发生成一个无碰撞子弹并扣一发，换弹后实际文本为 30 / 089。
- 开始菜单、胜利和失败页完成 UMG 离屏渲染，菜单在 720p / 1080p 保持原图比例；实际菜单图已检查。
- 子弹网格缩略图在 commandlet 中未取得可用外观图，不把该缩略图作为外观通过证据；资产引用、无碰撞、飞行和清理已有自动验证。

- 修改前备份：`Saved/T07/BeforeT07.zip`。
- 构建：`Saved/T07/BuildFinal.log`。
- 自动测试：`Saved/T07/AutomationReport/index.json`。
- 资源迁移：`Saved/T07/Migrate.log`。
- 实际蓝图/UMG 验证：`Saved/T07/Verify.log`。
- 菜单、结果、弹药和子弹模型预览：`Saved/T07/*.png`。
- 以上生成内容由 .gitignore 忽略；运行必需的 Content 与源图保留。

自动测试与离屏渲染不替代真实地图中鼠标操作、子弹观感和连续三次重开的人工验收。

## 8. PIE 验收

1. 打开原地图进入 PIE，开始菜单显示原图，尚未点击 START GAME 时敌人不攻击，玩家不能射击。
2. 点击 START GAME，检查鼠标回到游戏、原准星/血条/弹药出现，初始 30 / 090。
3. 开一枪，确认只扣一发，原有每发伤害仍为 25；朝较远处开枪观察金色飞行弹头，到墙面或终点消失。
4. 按 R，检查换弹提示及最终弹匣/备用弹药；无弹时没有飞行模型和开火特效。
5. 按 P 暂停，检查敌人和玩家战斗停止；点 START GAME 继续，鼠标和移动恢复。
6. 暂停后点 END GAME，回到开始菜单；再次开始时血量/弹药恢复初值。
7. 击杀全部敌人，出现 VICTORY；点击 RESTART 可从完整初值开始新一局。
8. 让玩家被击杀，出现 DEFEAT，换弹中断；MAIN MENU 回到开始菜单。
9. 连续重开三次，检查 HUD 没有重复、旧子弹和计时器没有残留；调整窗口尺寸检查菜单比例。
10. QUIT GAME 在 PIE 中退出预览；独立运行时退出游戏。

反馈模板：

```text
开始菜单 / 原图比例：
开始前战斗禁用：
START GAME / 鼠标恢复：
弹药初值 / 射击 / 换弹同步：
飞行子弹 / 一发伤害 / 清理：
P 暂停 / 继续：
END GAME 返回菜单：
清空敌人 VICTORY：
玩家死亡 DEFEAT / 换弹中断：
RESTART 连续三次：
MAIN MENU / QUIT GAME：
重新 PIE / 窗口尺寸：
报错：
```

## 9. 用户验收与后续安排（2026-09-27）

用户反馈“没有问题，可以推送”，据此记录 T07 整体验收通过并提交远端 develop；用户未逐项填写模板，不补造逐项测试记录。

用户选择先完成 T08 可运行打包基线，再继续优化。当前版本作为可回退的功能基线，后续优化仍可继续，不将首次打包当作最终定版。
