# T08：Windows 可运行打包基线

日期：2026-09-27<br>
项目：`D:\UE5_My_Project\MyShoot 5.5`<br>
功能基线：T07 / `43be8d0`（已推送并核对 origin/develop）<br>
状态：Windows 可运行打包基线已完成，实际包自动回归通过；音效/操作手感和演示视频待人工补充。

用户选择先完成可运行打包基线，再继续优化；本阶段不是最终定版。

## 本次范围

1. 检查 Windows Game / Editor 构建、默认地图和 GameMode。
2. 显式将 bloodstrike 加入打包地图列表，保留硬引用 UI、模型、动画和 GAS 资源。
3. 生成 Windows Development 包。
4. 从实际包启动，检查菜单、AI 扣血、真实射击、弹药、换弹、胜负、连续三次关卡重开和返回菜单。
5. 更新 README、结果记录与已知问题。

## 运行与验证路径

- 打包产物：`PackagedBuilds/T08/Windows`。
- 编辑器构建日志：`Saved/T08/BuildEditor.log`。
- UAT 日志：`Saved/T08/Package.log`。
- 实际运行日志、截图和报告：`Saved/T08/Smoke`。
- 生成文件均由 Git 忽略，不将打包程序、调试符号或测试截图上传源码仓库。

新增 ShooterPackagedSmokeSubsystem 只在非 Shipping 且显式传入 -ShooterSmoke 时注册验证 Tick。
它通过真实按钮委托和游戏入口驱动打包程序，检查新关卡中的初始属性与唯一 HUD。
测试会主动移动角色、施加用于胜负检查的伤害并退出；普通运行无这些操作。
真实攻击验证使用 AI 自身的伤害逻辑，射击验证使用玩家武器射线，不用测试代码直接模拟射击扣血。

## 检查结果

- UE5.5 Development Editor / Win64 构建通过。
- Windows Development BuildCookRun 成功，ExitCode=0；UAT 总耗时约 10 分钟。
- 实际包包含 59 个文件，总计 1,086,855,390 字节，约 1.01 GiB，包含 Development 调试文件和运行所需资源。
- 从打包目录启动实际游戏程序，自动回归 PASS，进程退出码 0。
- 已验证默认地图/菜单、初始 100 血量和 30 / 90 弹药、唯一 HUD、开始前射击禁用。
- 已验证实际 AI 扣血和血条同步；实际玩家射线每发造成 25 伤害、扣一发、生成一个子弹表现。
- 已验证换弹后弹匣/备用弹药为 30 / 89，暂停禁止战斗、继续按钮恢复。
- 已验证全敌人死亡显示胜利、玩家死亡显示失败和中断换弹。
- 已执行三次真实 OpenLevel 重开，每次新世界均恢复初值，HUD 没有重复。
- 已验证结算 MAIN MENU 和暂停 END GAME 均重新加载开始菜单。
- 实际游戏画面截图已检查，场景、角色、武器、血条与弹药正常显示。
- 实际运行日志没有 Error、Fatal 或断言；保留 GameplayCueNotifyPaths 未指定的既有警告。烘焙仍有原有 AnimBP 线程安全警告。

验证边界：运行在本机，采用离屏窗口、静音和显式自动验证参数；按钮通过真实 UMG 委托触发，并非人工鼠标操作。已完成实际游戏数据和跨关卡回归，不将它记为其他机器上的安装验证、音效试听或手感测试。QUIT GAME 人工操作和短演示视频尚未记录。

可直接双击 PackagedBuilds/T08/Windows/MyShoot.exe 正常游玩，不要添加 -ShooterSmoke 参数。分享时复制整个 Windows 文件夹，不要只复制 exe。

可执行文件 SHA256：77CC1771305C16F28A8D98CB6300661D3CF35729F0C1C002F487F7B2F5D1815A（内部 Windows/MyShoot/Binaries/Win64/MyShoot.exe）。完整记录见 Saved/T08/BuildManifest.json。

Git 状态：T07 的 43be8d0 已推送并核对远端；T08 打包配置、显式验证入口、README 和本记录暂存于本地工作树，尚未提交或推送。打包产物和日志被 .gitignore 忽略。

## 后续优化顺序

在可运行基线之后，可按体验问题逐项优化：

1. 战斗表现：AI 攻击动画与预警、命中反馈、子弹可见性、贴墙枪口与射线一致性。
2. 界面：暂停时的按钮语义、菜单风格与游戏场景一致性、血条与弹药布局。
3. 工程质量：处理原有 AnimBP 线程安全警告，检查资源和性能。
4. 最后重新打包、回归并补充演示视频。

本记录区分自动验证、用户反馈和仍待人工观察的手感，不能把一次打包成功等同于所有体验均已完成。
