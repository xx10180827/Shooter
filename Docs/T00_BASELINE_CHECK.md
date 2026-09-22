# T00 基线检查与人工验收记录

自动检查日期：2026-09-21；人工验收日期：2026-09-22  
项目：`D:\UE5_My_Project\MyShoot 5.5`  
状态：**T00 完成。自动检查通过，用户已确认编辑器配置与其余运行验收正常；既有动画编译警告保留为后续待处理项。**

## 1. 已完成的检查

| 检查项 | 结果 |
| --- | --- |
| 本地分支 | develop，跟踪 origin/develop；本次未联网刷新远程 |
| HEAD | a808d95eb27bed190534e229e8cc96ff8c9c0a2a |
| UE | 本机 UE5.5.4 |
| 构建目标 | MyShootEditor / Win64 / Development |
| 构建方式 | 编辑器关闭后 -Rebuild 完整重编译，不依赖 Live Coding |
| C++ 结果 | 退出码 0，10 个构建动作完成，生成 UnrealEditor-MyShoot.dll |
| 实际编译工具链 | VS2022 MSVC 14.38.33143，安装目录 14.38.33130 |
| Windows SDK | 10.0.22621.0 |
| 项目蓝图编译 | 退出码 0；7 个蓝图，0 错误、6 条编译警告、0 加载失败 |
| 原始文件保护 | 对 335 个源码、配置和资产等文件记录 SHA256；构建后检查无变化 |
| 本地备份 | 336 个文件的 ZIP；其中 335 个基线文件逐一与构建前 SHA256 一致 |
| Git 提交 / 推送 | 未执行 |

C++ 构建命令：

```powershell
& 'D:\UE_Engine\UE_5.5\Engine\Build\BatchFiles\Build.bat' MyShootEditor Win64 Development '-Project=D:\UE5_My_Project\MyShoot 5.5\MyShoot.uproject' -WaitMutex -NoHotReloadFromIDE -Rebuild
```

项目蓝图检查使用本机 CompileAllBlueprints commandlet 和项目资源白名单，在内存中编译，不保存 .uasset。检查包含 Shooter、Mygame_GM、Boot_Shooter_controller、Boot_Shooter_BP、Shooter_UI、Shooter_idle、Boot_Shooter_AnimationBP。

**该结果证明现有 C++ 可以构建、上述蓝图可以加载和编译；不证明地图已进入 PIE，也不代表实际玩法或动画线程问题已解决。地图内 Level Blueprint 不计入这 7 个独立蓝图资源。**

## 2. 日志和备份位置

项目内相对路径：

- C++ 日志：`Saved/T00/Build-DevelopmentEditor.log`
- 最终有效的蓝图日志：`Saved/T00/BlueprintCompile-Project.log`
- 文件校验清单：`Saved/T00/baseline-hashes.csv`
- 基线备份：`Saved/T00/Baseline-20260921-210649.zip`
- 备份大小：389,547,720 字节，约 371.5 MiB。

备份包含当前已保存的源码、配置、资产、.uproject、.vsconfig 和当时版本的工作文档；不包含 .git 历史、引擎及被忽略的构建缓存。它是本地恢复副本，不是远程备份。清理 Saved 目录前应先保留这个 ZIP。

需要恢复时，先解压到新的空目录并核对，再决定恢复哪些文件，不直接覆盖后续新增工作。

首轮蓝图检查因 UE 命令参数解析截掉白名单文件扩展名而失败，范围也包含了引擎 / 插件资源；该日志保留为 BlueprintCompile.log。随后使用无扩展名白名单重跑成功，**验收以 BlueprintCompile-Project.log 为准**。

## 3. 现有代码和配置结论

- MyShoot Runtime 模块已存在，Game / Editor Target 已存在。
- AMyShooter 继承 ACharacter，当前为模板代码；BeginPlay、Tick、SetupPlayerInputComponent 调用父类，没有新增玩法实现。
- 已具备继续接入 GAS 的 C++ 基础，不需要重新创建工程。
- 目前 Build.cs 尚未加入 GAS 模块，T00 不修改这一点。
- EditorStartupMap 与 GameDefaultMap 均为 /Game/Maps/bloodstrike.bloodstrike。
- 未在所检查配置项中发现 GlobalDefaultGameMode；地图可能使用 World Settings 覆盖，不能由此判定 GameMode 缺失。
- 输入配置：WASD 移动、鼠标控制视角、鼠标左键 fire。
- Jump 当前是 Axis Mapping，绑定 BackSpace。它只证明输入配置存在，用户于 2026-09-22 以“其余正常”确认运行验收；未单独描述跳跃节点连接，本轮不更改按键。
- 已有修改保留：DefaultEngine.ini、Mygame_GM、Shooter、bloodstrike、MyShoot.uproject，以及未跟踪的 Source、.vsconfig 和 Docs。

蓝图父类、GameMode 默认 Pawn 和 AIController 的有效配置，必须以编辑器面板为准。本次未将二进制字符串中的类引用视为父类确认结果。

## 4. 发现的警告

### 4.1 C++ 包含顺序升级提示

当前构建采用兼容的 Unreal5_3 include order；引擎提示可迁移到 Unreal5_5。此次构建成功，不是阻塞错误。后续有意调整 Target 设置时再单独验证，本次不修改。

构建还因可用内存较低自动限制为单个并行编译任务，最终正常完成。

### 4.2 动画蓝图线程安全编译警告

| 资产 | 日志指出的调用 | 本轮处理 |
| --- | --- | --- |
| Shooter_idle | Move_BS 混合空间相关路径读取 Try Get Pawn Owner | 记录，待编辑器定位节点 |
| Boot_Shooter_AnimationBP | Get Boot Shooter / Get_Boot_Shooter，以及 Boot_Shooter_BS 相关路径读取 Try Get Pawn Owner | 记录，待编辑器定位节点 |

蓝图编译器汇总为 6 条警告；整个命令行进程汇总为 16 条警告，包含加载期间的警告和重复报告，二者统计范围不同。本次检查无编译错误，不等于警告已修复。

后续修复方向：先确认实际节点链；在合适的游戏线程更新路径读取并校验角色，缓存速度、空中状态等数据，AnimGraph 使用缓存值；或采用符合 UE5.5 要求的线程安全数据访问方案。不要仅给访问 Actor 的函数添加 ThreadSafe 标记来掩盖问题。

### 4.3 历史运行日志

检查现有 MyShoot.log 的指定错误模式，未找到匹配的 Log...: Error、Accessed None 或 Fatal error 行；该历史日志中存在 EOS / HTTP 的 SSL 连接警告。这里只是历史日志筛查，不是本轮 PIE 通过的证据。用户于 2026-09-22 确认项目打开及其余运行验收正常，未报告对应玩法故障。

## 5. 用户人工验收结果（2026-09-22）

证据来源：用户在当前任务中反馈，不是助手远程操作编辑器的结果。

| 检查项 | 用户确认 |
| --- | --- |
| 项目打开 | 正常 |
| Shooter 父类 | Character |
| Boot_Shooter_BP 父类 | Character |
| GameMode Override | Mygame_GM |
| Default Pawn Class | Shooter |
| 敌人 AI Controller Class | Boot_Shooter_controller |
| Auto Possess AI | PlacedInWorld |
| 其余运行验收 | 用户反馈“其余正常”，按前述验收清单整体通过记录 |

“其余正常”作为移动、视角、跳跃、射击、追踪、死亡、再次运行及报错检查的整体反馈；没有额外录制的逐项证据，也不代表既有动画编译警告已消失。

## 6. 对后续改造的影响

- 两个现有角色蓝图均直接继承 ACharacter；现有 AMyShooter 尚未成为这两个蓝图的父类。
- T01 需要完成公共 C++ 角色基础和蓝图接入，不能只创建 C++ 类而不改蓝图父类。
- 建议新增公共战斗基类 AShooterCharacterBase，让 AMyShooter 继承它作为玩家扩展点；敌人可以直接继承公共类，确有敌人专用 C++ 逻辑时再增加敌人子类。此为下一阶段建议，尚未实施。
- 当前 PlacedInWorld 配置配合现有放置敌人的玩法已由用户验收。本轮保持原样；T06 若引入动态生成敌人，再核对生成后的 AI 接管方式。
- 先前动画线程安全警告已记录为后续事项，不作为本次基础玩法验收的阻塞项；没有声称已修复。

## 7. T00 结束条件

- [x] C++ 完整重编译成功。
- [x] 项目独立蓝图资源编译检查通过，现有警告已记录。
- [x] 当前已保存文件存在经校验的本地备份。
- [x] 用户确认编辑器可以正常打开项目。
- [x] 用户确认父类、GameMode、Pawn 和 AI 配置。
- [x] 用户完成 PIE 验收，现有缺陷有明确记录和处理决定。

T00 已完成，下一项为 T01；本次仅更新验收文档，未开始 GAS 实现。暂不提交或推送；2026-09-21 的已校验备份作为本阶段恢复副本。
