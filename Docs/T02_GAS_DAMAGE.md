# T02 现有射击接入 GAS 伤害

日期：2026-09-22<br>
项目：`D:\UE5_My_Project\MyShoot 5.5`<br>
基线：T01 提交 `c7b258f` 已推送到 origin/develop。<br>
状态：**T02 完成。原生检查通过；用户已完成 GAS 伤害接线，并于 2026-09-24 确认修正 Get GAS Health 连接后死亡动画与尸体消失恢复。**

## 1. 本次实现

- ShooterDamageEffect：Instant GameplayEffect，以 Additive 方式改变 AttributeSet.Health。
- ShooterDamageLibrary：蓝图节点 Apply GAS Damage，将一次有效命中转为一个 GE。
- ShooterCharacterBase：On GAS Health Changed 通知，携带 Old Health / New Health；退出角色生命周期时解绑内部属性回调。
- 伤害入口接受正数 Damage，在内部转成负数 Data.HealthDelta。这与初始计划的 Data.Damage 命名不同：直接表达实际属性变化，避免引入仅为取负而编写的 ExecutionCalculation。
- 记录 SourceActor、DamageCauser 和可选 HitResult，便于后续击杀归属和命中表现。
- 拒绝非正数 / 非有限伤害、无效对象、没有所需 GAS 属性的目标、尚未初始化的公共角色、已零血或正在销毁的目标。
- 当前按单机 / Authority 路径实现，不包含网络预测和客户端伤害 RPC。
- 返回 true 表示目标血量实际降低；Instant GE 没有持久 ActiveEffectHandle，因此不以句柄有效性判断是否扣血。
- 如果射击来源仍是旧 Blueprint Character，入口允许它没有 ASC；若来源已有 ASC，则用来源 ASC 创建效果 Spec。两条路径均保存真实伤害来源。

这次没有修改射击蓝图、动画、地图，也没有实现完整死亡状态机。T03 再统一处理取消能力、停止 AI、死亡表现完成和清理。

## 2. 先确认接入前提

1. 敌人 Boot_Shooter_BP 父类为 ShooterCharacterBase，Is GAS Initialized 为 true。
2. Initial Max Health 先设为 100，便于验收。
3. 打开 Shooter 蓝图，定位现有 Shoot_Once 中的 Line Trace 与命中后的伤害调用。
4. 先辨认原来的 Take_Damage 路径是否同时包含扣血、受击表现及死亡判断，再替换。不能只添加新节点而保留旧扣血。

## 3. 在现有射击命中后调用新节点

保留原有输入、射速控制、Line Trace、枪口特效、声音。

在 Trace 命中为 true 的执行分支添加 Apply GAS Damage：

| 引脚 | 接法 |
| --- | --- |
| Source Actor | 玩家 Shooter 的 Self |
| Target Actor | 本次 Out Hit 中的 Hit Actor |
| Damage | 正数，例如 25 |
| Damage Causer | 展开高级引脚；有武器 Actor 则传武器，否则留空，内部使用 Source Actor |
| Hit Result | 本次 Trace 的完整 Out Hit |
| Return Value | 是否实际降低血量；需要按实际受伤触发的表现可以使用它 |

不要因为打到墙时返回 false 就再次调用旧 Take_Damage。无 ASC 的墙体被正常忽略。射线物理命中产生的墙面火花等表现可以继续根据 HitResult 触发，不要求造成血量伤害。

这一阶段的目标流程：

```text
Shoot_Once
  → 原有 Line Trace
  → 命中分支
  → Apply GAS Damage（Self、Hit Actor、25、Out Hit）
  → 目标 ASC 应用 Instant GE
  → AttributeSet.Health 变化
  → On GAS Health Changed 通知目标蓝图
```

## 4. 移除双重血量路径

- 当命中分支已经调用 Apply GAS Damage，断开原来的直接扣血调用。
- 旧蓝图 Health 不再参与战斗计算；读取血量改为 Get GAS Health。
- 不再执行 Health -= Damage，不再通过 Set Health 镜像 GAS 数值。
- 若旧 Take_Damage 同时执行声音、受击动画、血量和死亡，将表现及死亡判断移到下面的属性变化事件；不要在新旧入口各执行一次。
- 确认全部旧变量引用迁移后再删除旧 Health 变量。未确认前可保留无调用的旧节点作对照，但不能继续执行扣血。
- 本次不在 C++ 覆写引擎 TakeDamage，避免蓝图自定义 Take_Damage 和引擎伤害系统形成两个入口。

## 5. 敌人订阅 GAS 血量变化

在 Boot_Shooter_BP 现有 BeginPlay 链末尾：

1. Bind Event to On GAS Health Changed，Target 为 Self，只绑定一次。
2. 创建参数匹配的自定义事件，例如 HandleGASHealthChanged，参数为 Old Health、New Health。
3. 先在事件中打印 New Health，确认每次有效命中只有一条数值变化。
4. 如果保留原来的死亡表现，在该事件里检查 New Health <= 0 且尚未死亡，再调用原 Dead 路径。
5. 死亡标记只在一个位置设置。若原 Dead 内部会检查或设置 IsDead，先检查原图，避免外部提前设置导致原 Dead 被自身分支挡住。
6. 受击表现按实际需求在 New Health < Old Health 且 New Health > 0 时执行；不再放回旧扣血逻辑。

这只是通往 T03 的临时表现接线，不代表完整死亡系统已经完成。T02 原生入口会忽略零血目标的继续伤害，但不会自动播放死亡动画、销毁角色或停止 AI。如果暂时只接打印，不做第 4 项，敌人零血后继续活动属于尚未接死亡流程。

初始血量在蓝图 BeginPlay 前已经设置；变化事件不会补发初始化通知。要显示出生血量，在 BeginPlay 读取 Get GAS Health / Get GAS Max Health。

## 6. 验收步骤

| 场景 | 预期 |
| --- | --- |
| 100 HP，连续四次各 25 伤害 | 75 → 50 → 25 → 0 |
| 100 HP，单次 150 伤害 | 0，无负数 |
| 未命中 | 不应用伤害 |
| 打墙等没有 GAS 的 Actor | 无报错、不扣角色血量 |
| 重复命中零血目标 | 不再应用新的伤害效果，不重复触发血量变化 |
| 每次命中 | 只有一次实际扣血，旧 Take_Damage 不再叠加扣血 |
| 原射击表现 | 输入、枪口效果和声音正常，受击表现没有重复 |
| 停止再进入 PIE | 恢复配置的满血，无重复绑定 |
| 临时死亡表现接线（若已做） | 归零后旧死亡表现仍能触发一次；完整清理留 T03 |

## 7. 自动验证与日志

- Build.log：Development Editor 构建日志。
- Automation.log：MyShoot.GAS.Foundation 和 MyShoot.GAS.Damage 测试日志。
- AutomationReport/index.json：真实结构化测试结果，应有 2 个成功测试，failed=0、notRun=0。
- asset-baseline.csv：T02 操作前资产 / 配置文件 SHA256。

上述文件均位于 Saved/T02，由 .gitignore 排除。测试里的伤害调用直接使用生产 ApplyGASDamage 入口，覆盖四次命中、过量伤害、非法输入、无 ASC 目标、零血 / 销毁 / 未初始化目标、来源 Actor 与武器及命中信息。它不代替蓝图实际接线验收。

## 8. 当前状态和提交范围

T01 已按用户指定标题推送到远程 develop。T02 改动单独保留在工作区，等待蓝图接入验收后再提交，未混入 T01 提交。

用户接线反馈模板：

```text
Apply GAS Damage 节点接入：完成 / 卡在哪一步
原 Take_Damage 扣血路径：已断开 / 尚未处理
四发 25 的血量打印：
150 伤害：
打墙 / 未命中：
零血后重复射击：
原射击表现：
旧死亡表现是否已接到血量变化事件：
停止后重新 PIE：
报错：
```


## 9. 本轮最终检查结果

- Development Editor 编译和链接通过。
- MyShoot.GAS.Damage 与 MyShoot.GAS.Foundation：2 成功、0 失败、0 未运行、测试内 0 警告。
- 现有 7 个蓝图：0 编译错误、6 条既有动画编译警告、0 加载失败；此结果不代表射击蓝图已接新入口。
- T02 之前记录的 325 个资产及配置文件复核无变化；用户现有资源改动已保存在 T01 提交中。
- T02 源码和文档暂未提交；先完成本阶段蓝图接入验收。


2026-09-24 用户确认 T02 接线已实现，Get GAS Health 连接问题修正后死亡动画和清理正常；现进入 T03，使用新的原生死亡状态和表现事件替换临时死亡判断。
