# T21 README 与最终验收包

日期：2026-10-07。

用户授权停止新增功能，更新 README、推送远端并重新打包，供最终人工验收。本轮不增加枪口遮挡或其他玩法。

## 输入与范围

- 功能基线：T20 67fba19096b0d6fd8d378ae44be8ab4c7dc0fb01。
- README 更新到 T20：完整操作、现有功能、职责、构建与测试入口、技术边界、文档索引。
- 从推送后的 origin/develop 新建独立干净检出，使用仓库内的代码、配置和资源构建。
- 原工作区未提交的地图、蓝图、编辑器设置、插件启用和个人笔记保持原状；不把它们暗中混入可复现构建。
- 输出目标：原工作区 PackagedBuilds/T21_20261007/Windows，旧包保留；产物不上传 Git。
- 构建和运行日志目标：原工作区 Saved/T21_Final。
- 包类型：Windows x64 Development，保留显式自动检查入口用于最终验证。

## 当前状态

README、构建与自动运行验证已完成，最终包已交付准备；等待用户人工验收。最终包源码提交为 be77e209961261b251cecef9b3bed77302730db0，之后的提交仅补充文档和截图。

## 人工验收

双击 Windows/MyShoot.exe，不附带自动检查参数。

- 菜单进入游戏，WASD / 跳跃 / 鼠标手感。
- 双武器射击、后坐力、右键切换瞄准、换弹、切枪中断。
- AI 巡逻、视野交战、听觉／受击警觉、移动射击、死亡清理。
- F 拾取霰弹枪与弹药，补满 30/90、8/32；满弹保留物品。
- 地面／空中 Dash、墙体碰撞、无敌窗口、体力与冷却。
- 设置即时生效、恢复默认、退出重进保留。
- 白色命中线、金色击杀线、点射／连射确认音、血雾、击杀图标与多杀。
- 暂停、胜负、重开、返回菜单、退出；窗口适配、音效及光照。

首次远端干净构建：6fcb9bc，编辑器全量编译成功，19 项自动测试通过；Win64 打包成功且六组实际包检查全部 PASS。截图发现旧蓝图仍显示调试数值，增加 Engine 配置关闭屏幕调试文字，保留日志；该项属于展示清理，不改变玩法。随后更新包并复查。

## 最终构建与验证结果

- 干净检出目录：D:/UE5_My_Project/MyShoot_FinalVerify_20261007。来自已推送的 origin/develop；构建前后 git status 均为空。
- 编辑器全量编译 CleanEditorBuild.log 成功。CleanTests.log / AutomationReport：19 项通过，0 失败、0 未运行。
- 最终打包源码：be77e209961261b251cecef9b3bed77302730db0，仅比首轮增加关闭屏幕调试文字的配置和记录；未新增玩法。
- 最终 PackageFinal.log：BUILD SUCCESSFUL，UAT ExitCode=0，BuildCookRun 106.94 秒。
- 输出：PackagedBuilds/T21_20261007/Windows/MyShoot.exe。包体约 1.03 GiB（包括 Development 调试文件和先决条件安装器）。
- 随包 使用说明.txt 提供启动、全部操作及最终人工验收清单；BuildManifest.json 记录源码提交、验证范围、启动器/游戏二进制/容器哈希。
- 首轮和最终包证据分开保留：FirstRound / FirstPackageRuntimeSaved / FirstRunLogs 与最终 Round / PackageRuntimeSaved。
- 通过根目录正式启动器运行，最终六组检查均 PASS：

| 检查 | 结果与证据 |
| --- | --- |
| 对局 | PackageRound.log / Round/PackagedSmoke.txt：菜单、AI 伤害、射击、子弹、HUD、换弹、暂停、胜负、三次实际重开及返回菜单 |
| 拾取 | PackagePickup.log / PackageRuntimeSaved/T15_PickupRing/MapSmoke/Result.txt：触发/准星提示、重复领取、霰弹获得、实际步枪 0/0 补满 30/90、霰弹 8/32 |
| Dash | PackageDash.log / PackageRuntimeSaved/T17_AirDash/MapSmoke/Result.txt：地空位移、空中保持高度、结束下落、无敌窗口和暂停清理 |
| 打击感 | PackageFeedback.log / PackageRuntimeSaved/T20_Feedback/MapSmoke/Result.txt：命中、血雾、击杀、多杀及暂停清理；点射和连射累计 confirmations=10 / rendered=10 |
| 设置操作 | PackageSettingsWrite.log / PackageRuntimeSaved/T19_Mouse/Write/Result.txt：两菜单入口、滑条、默认值、Esc 返回和暂停保持 |
| 跨启动保存 | PackageSettingsVerify.log / PackageRuntimeSaved/T19_Mouse/Verify/Result.txt：第二个独立进程读取此前保存的设置 |

已查看最终独立包的开始菜单、默认设置和击杀截图，屏幕旧调试数字已消失；截图原样纳入 Docs/Images/T21 并用于 README，没有用编辑器截图冒充游戏包。

## 交付边界

- 原工作区 12 个既有修改文件逐一 SHA256 比较保持不变；最终包不混入这些未提交版本。
- 测试产生的整个 MyShoot/Saved 已移至 Saved/T21_Final/PackageRuntimeSaved，验收包不携带测试灵敏度，首次启动使用 0.80 / 0.75。
- 打包仍有既有动画蓝图线程安全调用和 GameplayCue 路径回退警告（最终 cook 汇总 7 条警告、0 错误）。没有据此宣称工程零警告。
- 最终六组实际运行日志未发现 Error 或 /Game 资源缺失；启动时缺少可选分析器 DLL 的提示不代表游戏资源缺失。
- 验证为本机离屏窗口和自动运行，声音听感、鼠标手感、窗口操作和另一台电脑兼容性仍需人工确认。
- 包为 Windows Development，并非 Shipping 发布版本。未上传 GitHub 安装包或建立 Release；源码、配置、文档和展示截图推送至 develop，包在本地单独交付。
- 本轮不增加新功能，下一步等待最终人工验收；不能将自动 PASS 写成用户已经验收。
