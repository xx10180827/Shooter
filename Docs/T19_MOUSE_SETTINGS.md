# T19 鼠标灵敏度设置

日期：2026-10-05。基于 UE5.5 项目当前已保存工作区实现；T18 用户验收中提出鼠标整体偏快，本阶段只处理鼠标设置。命中反馈与血雾没有纳入本轮。

## 玩家操作与默认值

- 开始菜单和暂停菜单右上角 SETTINGS 进入设置，游戏进行时入口隐藏。
- Mouse sensitivity：默认 0.80，范围 0.10–3.00，首版统一调整 X/Y。
- Aim multiplier：默认 0.75，范围 0.10–2.00，乘在腰射灵敏度之上；默认完全开镜有效倍率为原基础鼠标输入的 0.80 × 0.75 = 0.60。
- RESTORE DEFAULTS：仅重置鼠标配置，立即保存；BACK / Esc / P 返回所在菜单。
- 暂停设置关闭后仍暂停，再按 Esc / P 或菜单继续才恢复游戏。
- 滑条立即生效并调用 SaveSettings。退出游戏后再次启动读取 GameUserSettings.ini。
- 编辑器 / PIE 与独立包使用各自的平台配置目录，不承诺两者互相同步。设置是本机用户偏好，不随角色重生或武器拾取重置。

## 架构与扩展

`.h` 位于 Public，`.cpp` 位于 Private；关键职责配有中文注释。

| 层 | 文件/类型 | 职责 |
| --- | --- | --- |
| 持久化 | Settings/ShooterUserSettings | UGameUserSettings 子类；保存 HipSensitivity(X/Y)、AimSensitivityProfiles(FName → X/Y)，校验范围和非法值 |
| 输入 | Player/ShooterPlayerInput | UPlayerInput 子类，只缩放 MouseX / MouseY，保留既有蓝图轴事件 |
| 配置选择 | Weapons/ShooterWeaponDefinition | AimSensitivityProfile 默认 Default，选取当前武器/瞄具的设置档案 |
| UI | UI/ShooterMouseSettingsWidget | 独立 UMG 面板，读取/写入用户配置、保存、恢复与焦点处理 |
| 生命周期 | Player/ShooterPlayerController | 设置面板创建、菜单状态切换和销毁；使用自定义 PlayerInput |

以后拆分水平、垂直滑条，分别更新 FVector2D 对应轴即可，不需要迁移配置结构。增加 2x、4x 等瞄具时，使用稳定配置名，例如 Scope2x / Scope4x，调用 SetAimSensitivity 添加对应设置；缺失配置自动回退 Default。当前只在 UI 暴露通用 ADS 倍率，尚未添加多倍率瞄具或其专用页面。

配置与运行时状态分离：鼠标偏好不放在 WeaponComponent、Character 或 GameplayAbility 中。实际缩放为：HipAxis × Lerp(1, AimProfileAxis, AimAlpha)，开镜/退出开镜平滑过渡。

## 后坐力和 Dash 边界

- 只在 UPlayerInput 的 MouseX / MouseY 处理层追加倍率。键盘和手柄输入不缩放。
- 保留原鼠标基础 AxisConfig 灵敏度与平滑行为；鼠标帧内位移不重复乘 DeltaSeconds。
- DefaultInput.ini 关闭 bEnableFOVScaling，避免 Dash / 其他镜头 FOV 变化额外改变转向速度。
- 开镜变慢由显式 ADS 设置控制，不再叠加旧引擎 FOV 缩放。0.80 指腰射相对于原手感约 80%，并非原 ADS 整体恒定乘 80%。
- RecoilComponent 直接修改 ControlRotation，绕过鼠标输入倍率，武器原有后坐力数值保持作用。
- UI 在暂停状态运行，禁用下层菜单按钮；滑条有焦点时用预览按键事件接收 Esc/P，防止意外恢复战斗。

## 验证记录

- 编辑器最终构建：Saved/T19_Mouse/BuildFinal.log。
- 18 项 MyShoot 自动测试全部通过：Saved/T19_Mouse/Tests.log、Report/index.json。
- 新增 MyShoot.Settings.MouseSensitivity：独立 X/Y 与命名瞄具保存读取、缺省回退、越界值限制、恢复默认、鼠标轴缩放、键盘不变、FOV 不变性、ADS 混合、低/高灵敏度下真实射击后坐力相同。
- 真实 bloodstrike 地图：Write/Result.txt PASS。开始/暂停入口、两条滑条、恢复默认、Esc 焦点返回、暂停保持与恢复通过。
- 完全退出第一个游戏进程再启动第二个：Verify/Result.txt PASS，读取腰射 0.63 与 ADS 0.52。测试后已恢复原 WindowsEditor 用户配置，哈希一致。
- 运行截图已人工查看；随后改善按钮文字对比度。自动运行不替代玩家鼠标手感验收。
- 测试入口仅在显式 -ShooterMouseSettingsSmoke 参数时运行；配合 -ShooterMouseSettingsVerify 验证第二个进程的保存值，普通游戏无测试逻辑。

## 人工验收

```text
开始菜单 / 暂停菜单 SETTINGS：
默认 0.80 腰射快慢：
腰射滑条同时改变水平 / 垂直：
开镜倍率调整，腰射速度不变：
恢复默认：0.80 / 0.75：
退出设置后仍处于暂停菜单：
完全退出游戏再启动，数值保留：
连射后坐力力度，压枪是否正常：
Dash 期间转向没有额外加速 / 减速：
重新 PIE / 其他问题：
```

本轮未提交或推送；新设置手感等待用户验收。T18 的整体功能、音效、画面已由用户确认正常，鼠标偏快由本阶段解决。

## 最终独立包验证

- 新包：PackagedBuilds/T19_20261005/Windows/MyShoot.exe。旧 T18 包保留，随包附中文使用说明。
- Package.log：BUILD SUCCESSFUL，ExitCode=0；BuildCookRun 154.47 秒。
- 通过根目录正式启动器启动两个独立进程，PackageWrite.log 与 PackageVerify.log 均正常退出。
- PackageRuntimeSaved/T19_Mouse/Write/Result.txt 和 Verify/Result.txt 均 PASS，确认实际包的两菜单入口、滑条回调、恢复默认、暂停返回和重启保存。
- 已查看最终包的设置截图，确认默认值 0.80 / 0.75、滑条和按钮可见，改善后的按钮对比度正常。
- 包内实际保存位置：Windows/MyShoot/Saved/Config/Windows/GameUserSettings.ini。测试 Saved 已移至项目 Saved/T19_Mouse/PackageRuntimeSaved，保留验证证据，验收包首次启动使用默认值。
- 最终包基于当前已保存工作区，包含用户原有未提交资产；不宣称等同于远端干净检出构建。本轮没有提交或推送。

## 2026-10-05 用户验收通过与提交授权

用户确认：开始/暂停菜单右上角 SETTINGS 正常，默认 0.80 腰射合适，水平/垂直同步调整与独立开镜倍率正常，恢复默认有效；设置返回后仍保持暂停，完全退出后重启仍保留设置；步枪/霰弹枪后坐力与压枪正常，Dash 转向稳定。上述结果替代前文的待人工验收状态。

用户授权提交推送 origin/develop，提交主题为“完成了鼠标灵敏度的自定义调整”。提交包含 T19 实现、测试和文档，以及此前已验收独立包的 UI 软引用烘焙修复、打包测试适配与 T18 记录。用户其他本地资产/编辑器设置/插件配置/笔记不纳入提交，打包产物与个人设置继续忽略。
