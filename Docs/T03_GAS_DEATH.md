# T03 GAS 死亡状态与表现接入

日期：2026-09-24<br>
项目：`D:\UE5_My_Project\MyShoot 5.5`<br>
状态：**原生构建通过；3 个 GAS 测试成功、0 失败、测试内无警告；现有 7 个蓝图编译通过。2026-09-24 用户确认验收结束、已能正常执行，T03 完成。**

## 1. 本阶段目标

用户已修正 T02 的 Get GAS Health 接线，并确认死亡动画和尸体消失恢复。T03 把死亡规则收进公共 C++ 角色，继续使用现有死亡表现。

本次不重做模型、动画或地图，不实现原地复活。已有 T02 未提交内容和用户新增的 Markdown 文件全部保留，不自动提交或推送。

## 2. 原生死亡流程

```text
Health 归零
  → BeginGASDeath（内部入口）
  → 标记 bGASDeathStarted，添加 State.Dead
  → 取消当前能力
  → 停止角色移动，关闭角色 Tick 和胶囊碰撞
  → AI StopMovement / Brain StopLogic / 停止 AI Actor Tick
  → 清理绑定到角色及 AIController 的旧定时器与旧延迟动作
  → 安排死亡清理超时
  → On GAS Death Started（蓝图表现事件）
  → 表现完成：Finish GAS Death
      或：超时兜底销毁
```

关键约定：

- 状态先于可重入回调设置，同一个角色仅进入死亡一次。
- State.Dead 是原生 GameplayTag，无需在编辑器手动添加。
- bGASDeathStarted 不与旧 Blueprint IsDead 同名，不自动覆盖旧变量。
- Get GAS Health / Max Health 继续用于血量查询；Has GAS Death Started 查询新死亡状态。
- 已死亡角色不能通过 Apply GAS Damage 继续造成伤害；已死亡目标忽略后续伤害。
- 普通回血不能复活已死亡角色；新生成的角色重新初始化为满血。
- 玩家死亡时也停止输入与视角输入；本轮重开关卡恢复状态，玩家结算 UI 在 T07 实现。
- 胶囊碰撞关闭；Mesh 动画、Mesh 碰撞及布娃娃由蓝图按原表现配置。
- 默认死亡动画按原地播放处理；使用 Root Motion 位移死亡时，需要另行设计移动策略。

## 3. Blueprint 接口

| 接口 | 类型 | 用途 |
| --- | --- | --- |
| Event On GAS Death Started | 可实现事件 | 死亡规则已执行，用于启动旧死亡表现 |
| Finish GAS Death | 可调用函数 | 表现结束后销毁；活着时误调用会被忽略 |
| Has GAS Death Started | 纯查询 | 读取原生死亡状态 |
| Death Cleanup Delay | Class Defaults 配置 | 从进入死亡起计时的最长保留时长，默认 5 秒，最低 0.1 秒 |

如果死亡动画和尸体展示总计超过 5 秒，请增加 Death Cleanup Delay。该值是兜底截止时间，可能先于较长的蓝图 Delay / 蒙太奇结束回调生效。

## 4. 敌人蓝图接线：复用旧 Dead，替换入口

自动验证已完成，现在可以打开编辑器按下面步骤接入：

1. 打开 Boot_Shooter_BP，父类保持 ShooterCharacterBase。
2. 在 Event Graph 右键搜索 On GAS Death Started，添加对应事件；也可在可覆写事件列表中查找。
3. 将这个事件连接到原来已经能正常播放死亡动画的 Dead 自定义事件 / 函数。
4. 断开 T02 中 On GAS Health Changed → 血量 <= 0 → Dead 的死亡分支，只留下血量打印、UI 或普通受击处理。
5. 若原 Dead 内部用旧 IsDead 防重入并设置动画状态，可暂时保留；不要在调用 Dead 之前又手动把旧 IsDead 提前设置为 true，否则可能被原有分支挡住。
6. 不要在新死亡事件前加 Has GAS Death Started == false 的条件；该事件到达时新状态已经是 true。
7. 旧 Clear_Death / 动画结束 / 延迟结束中最后的 Destroy Actor，改为调用 Finish GAS Death，统一主动结束入口。蓝图保留一条主动结束路径即可。
8. 在 Class Defaults 搜索 Death Cleanup Delay，确保大于期望的动画与尸体停留总时长。
9. 编译、保存并运行。

推荐的最终连接：

```text
On GAS Health Changed
  → 打印 / UI 更新 / 非致死受击表现
  （不再从这里调用 Dead）

Event On GAS Death Started
  → 原 Dead 的死亡表现
  → 动画结束或原有延迟结束
  → Finish GAS Death
```

原 Dead 中已有的 StopMovement、DetachFromController、停止攻击等节点，应逐项核对后精简；不要为简化节点而删除死亡动画、IsDead 动画驱动状态或必要的 Mesh 配置。原生代码当前不会主动 UnPossess，所以保留旧死亡表现中的 DetachFromControllerPendingDestroy 可以继续使用。

如果原 Dead 会在死亡时重新开启移动、重新调用 MoveTo 或恢复胶囊碰撞，需要去掉这些与死亡规则冲突的操作。

## 5. 玩家与后续能力

玩家 Shooter 应继承 MyShooter，间接获得同一死亡流程。需要玩家死亡表现时，实现同名事件；没有表现回调时会按 Death Cleanup Delay 清理。胜负 / 重开界面后续由 GameMode 和 PlayerController 处理。

新增 UShooterGameplayAbility，配置 State.Dead 为 ActivationBlockedTags。后续 GA_Fire、GA_Reload、AI 攻击能力应继承这个类，并保持该阻止标签。

- 当前正在运行的可取消能力在死亡时被 CancelAllAbilities 取消。
- 继承这个基类的能力在死亡后不能再次激活。
- 外部能力如果直接继承 UGameplayAbility，必须显式采用相同死亡阻止规则；不能假定所有第三方能力自动获得本项目基类规则。
- 不要把本项目战斗能力设为不可取消；以后若确需不可取消阶段，要单独定义死亡时的结束方式。
- 角色及 AIController 上的旧定时器与延迟动作会清理。独立 Weapon Actor / 组件自行持有的循环，以及无对象绑定的 lambda 计时器，应由对应能力 EndAbility / 对象清理负责，不能依赖 ClearAllTimersForObject(this) 处理所有对象。

## 6. 验收清单

- [ ] 100 血量、每发 25，仍按 75 / 50 / 25 / 0 变化。
- [ ] 在新死亡事件中临时打印角色名，单个角色只打印一次。
- [ ] 归零立即停止追踪、移动与攻击。
- [ ] 死亡动画正常播放，模型不会因关闭胶囊碰撞而出现不期望的表现。
- [ ] 动画 / 延迟结束后 Finish GAS Death 正常清理。
- [ ] 尸体阶段反复射击，不再次播放死亡、不重置消失时间。
- [ ] 暂时断开主动 Finish GAS Death 的连接，仍在配置时长后自动消失。
- [ ] 没有死亡动画的角色也能超时清理。
- [ ] 同时击杀多个敌人，各自独立进入死亡与清理。
- [ ] 停止并重新 PIE，角色恢复初始状态，无旧回调报错。

临时打印可用 Has GAS Death Started 检查状态，它在新死亡事件中应为 true。验证结束后删除不需要的屏幕打印。

## 7. 自动验证范围

新增 MyShoot.GAS.Death；同时回归 MyShoot.GAS.Foundation 与 MyShoot.GAS.Damage：

- 死亡标签只添加一次，回调重入不能再次应用伤害。
- 移动、速度、角色与 AI Tick、胶囊碰撞按预期关闭。
- 绑定到角色和 AIController 的定时器被清除。
- 已激活的测试能力被取消，死亡后不能再次激活。
- 死亡角色不能继续伤害其他角色。
- 活着时调用 Finish GAS Death 不会销毁。
- 正常 Finish 清理、无动画回调的超时清理均有效。
- 尸体再次受击不延长兜底时间。
- 新角色没有遗留死亡标签、正常满血。
- 原属性测试保留活着时的上限检查，并增加死亡后回血不能复活的断言。

这些测试不替代现有蓝图死亡动画、蒙太奇通知、AI 蓝图自定义回路与实际画面的人工验收。本项目目前没有用于测试的完整行为树资产，Brain StopLogic 分支仍需未来接入行为树时结合资产验收。

## 8. 文件与恢复点

- 原生修改：ShooterCharacterBase、ShooterAttributeSet、ShooterDamageLibrary、MyShoot.Build.cs。
- 新增：ShooterGameplayTags、ShooterGameplayAbility、ShooterGASDeathTest。
- 更新：ShooterGASFoundationTest 的死亡后回血规则。
- 日志与结构化报告：Saved/T03/Build.log、Automation.log、AutomationReport/index.json。
- 修改前备份：Saved/T03/BeforeDeath-20260924-093408.zip。
- 资产 / 配置校验清单：Saved/T03/asset-baseline.csv。

备份、日志和报告均在已忽略的 Saved 目录中。备份包含本次修改前用户已保存的 T02 接线和未提交文件，不等同于远程 Git 提交。

## 9. 当前状态

C++ 编译通过；MyShoot.GAS.Foundation / Damage / Death 共 3 个测试成功，0 失败、0 警告。现有 7 个蓝图编译为 0 错误、6 条既有动画编译警告、0 加载失败。本次未修改 Blueprint 事件连接，也没有提交 / 推送。

反馈模板：

```text
已改为 On GAS Death Started 调用旧 Dead：
旧 Health Changed 死亡分支已断开：
归零后 AI 是否立即停止：
每个角色死亡事件触发次数：
死亡动画：
Finish GAS Death 主动清理：
不调用 Finish 时的超时清理：
Death Cleanup Delay 数值：
重新 PIE：
报错：
```


最终文件保护检查：T03 开始前记录的 325 个 Content / Config 文件校验值全部一致。用户已修正的 T02 资产和新增说明文档均已保留。生成的 Visual Studio 工程文件已刷新。

2026-09-24 用户确认：验收结束，已可以正常执行，要求进入下一步。以上人工验收以用户整体反馈为依据，并非本次自动化逐项观测。T03 标记完成。
