# T18 Windows 验收包

日期：2026-10-05。

## 打包范围

用户要求先生成一次最新可运行包供人工验收。本包以 UE5.5 项目 D:/UE5_My_Project/MyShoot 5.5 的当前已保存工作区为输入；Git 基线 721224308b1596466aecb1a934ada6401dbf2344，包含用户尚未提交的蓝图、地图、菜单及子弹材质等本地编辑，不宣称等同于远端干净检出的构建。

输入状态和已修改文件的 SHA256 保存在 Saved/T18_Package/InputManifest.json；本轮不自动提交或推送这些本地修改。

- 目标：Windows Win64 Development，默认 bloodstrike 地图。
- 输出：PackagedBuilds/T18_20261005/Windows。
- 构建日志：Saved/T18_Package/Package.log。
- 旧 PackagedBuilds/T08 保留；新包、日志、测试截图仍由 Git 忽略。
- 复用 UAT BuildCookRun，启用 build/cook/stage/pak/iostore/archive/prereqs，使用现有已编译编辑器执行 cook。

## 用户启动与验收

打包完成后双击 Windows/MyShoot.exe，普通游玩不添加任何 Smoke 测试参数。若需要复制给另一台机器，请复制整个 Windows 文件夹；不能只复制 exe。

建议依次检查：

1. 开始菜单、开始游戏、鼠标锁定和准星显示。
2. 玩家移动/跳跃，步枪开火、连射、后坐力、右键切换瞄准与红点、R 换弹。
3. AI 巡逻、发现/追踪/攻击、受击和死亡；攻击动作、枪声和飞行子弹。
4. 近距离瞄准物品按 F，霰弹枪获得后按 1/2 切枪，弹药拾取恢复步枪 30/90、霰弹枪 8/32；满弹时物品保留。
5. 地空 Shift 闪避 480 cm，空中活动期间保持高度，结束后下落，无敌只覆盖 Dash 活动窗口。
6. 血量、体力框、冷却和弹药 HUD，窗口尺寸适配。
7. 暂停/继续、胜利/失败、重新开始、返回菜单和退出游戏。

人工音效、手感及窗口行为由用户验收；自动运行结果不替代这些结论。最终构建与运行结果在完成后追加。
## 最终结果

- 最终 UAT 日志 PackageFinal.log：BUILD SUCCESSFUL，ExitCode=0。首轮 204.62 秒，修复后重打包 93.16 秒。
- 最终包为 58 个文件、1,102,095,526 字节（约 1.03 GiB），含 Development 调试符号与依赖；已附中文 使用说明.txt。
- 首轮发现原生 Dash HUD 的软引用材质未被烘焙，回退显示旧弧线。已在 DefaultGame.ini 显式加入 /Game/UI 烘焙目录；最终实际游戏截图确认 Energy 体力框正常显示，没有相应缺包警告。
- 旧 T08 流程测试按近战 AI 假设摆放玩家，当前视野 AI 未必看得见，首轮该断言失败。测试已改为开阔区域正面交战，按玩家 Owner 统计子弹，换弹等待读取实际配置时长；仅显式 -ShooterSmoke 使用，不改普通游戏逻辑。
- 用户实际双击的根目录启动器已运行验证，RoundFinal/PackagedSmoke.txt 为 PASS：开始菜单、AI 真实扣血、射线 25 伤害、弹药/血条、子弹表现、换弹、暂停、胜负、三次重开及两种返回菜单路径通过。
- 最终 DashFinal.log / RuntimeSaved/T17_AirDash/MapSmoke/Result.txt 为 PASS：新体力框可见，地空位移、保持高度、恢复下落、无敌窗口及暂停清理通过。
- 最终 PickupFinal.log / RuntimeSaved/T15_PickupRing/MapSmoke/Result.txt 为 PASS：F 领取霰弹枪、切枪、金色圈、步枪真实打空后补满 30/90、霰弹枪 8/32 和 HUD 同步通过。
- 运行验证在本机离屏窗口进行；不替代人工音效/手感和另一台机器测试。打包截图背光区域较暗，未修改用户关卡光照，列入人工画面验收。
- 烘焙保留原有动画蓝图线程安全及 GameplayCue 路径回退警告；最终三组运行日志未发现 Error 或资源缺包警告。
- 测试产生的游戏 Saved 目录已移动到项目 Saved/T18_Package/RuntimeSaved，保留证据并避免给验收包附带测试设置。首轮失败日志单独保留。
- 输入清单中的用户原有本地编辑逐一 SHA256 比较均未变化。最终 BuildManifest.json 记录启动器与游戏二进制哈希。
- git diff --check 通过；此次打包配置、测试适配与文档尚未提交推送。等待用户人工验收。
## 人工验收更新（2026-10-05）
用户确认独立包整体功能、音效、动画、光照和菜单正常；仅反馈鼠标偏快。该反馈已在 T19 鼠标自定义设置中实现并由用户验收通过。T19 提交一并收录本阶段的 UI 烘焙修复、测试适配和文档。
