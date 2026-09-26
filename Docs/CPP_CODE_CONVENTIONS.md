# MyShoot C++ 文件与注释规范

2026-09-24 起按用户要求执行，适用于后续所有原生玩法代码。

## 1. 文件位置

模块根目录是 Source/MyShoot。头文件统一放 Public，实现文件统一放 Private；两个目录按相同职责建立子目录。

```text
Source/
├─ MyShoot.Target.cs
├─ MyShootEditor.Target.cs
└─ MyShoot/
   ├─ MyShoot.Build.cs
   ├─ Public/
   │  ├─ MyShoot.h
   │  ├─ Characters/       MyShooter.h、ShooterCharacterBase.h
   │  ├─ Combat/           ShooterDamageLibrary.h
   │  ├─ Weapons/          ShooterWeaponComponent.h
   │  └─ GAS/
   │     ├─ ShooterGameplayTags.h
   │     ├─ Attributes/   ShooterAttributeSet.h
   │     ├─ Effects/      ShooterInitialAttributesEffect.h、ShooterDamageEffect.h
   │     └─ Abilities/    ShooterGameplayAbility.h、ShooterFireAbility.h、ShooterReloadAbility.h
   └─ Private/
      ├─ MyShoot.cpp
      ├─ Characters/      对应角色实现
      ├─ Combat/          对应伤害入口实现
      ├─ Weapons/         对应武器实现
      ├─ GAS/             对应标签、属性、效果和能力实现
      └─ Tests/           所有自动化测试 .cpp
```

Build.cs 和 Target.cs 是构建规则，不是 C++ 文件，继续放在 UE 要求的位置。Public/Private 使用 UE 常见的首字母大写形式。

## 2. 头文件与实现分工

- .h 声明类型、对外接口、属性、委托及必要的内部成员。简单只读 getter 可以内联。
- .cpp 实现初始化、判断、状态推进、射线、伤害、定时器和资源清理。
- 一对同名 .h/.cpp 服务一个明确职责，避免把射击、死亡、AI 都塞进玩家类。
- 用 public / protected / private 控制类成员访问；文件夹名称不代替 C++ 访问修饰符。
- 公共头文件尽量前向声明；需要完整类型的依赖在对应实现文件中包含。
- 每个 .cpp 首先包含自己的头文件。项目头文件使用相对于 Public 的路径，例如：
  `#include "Characters/ShooterCharacterBase.h"`。
- 不写 `../` 穿越目录，也不在 include 中添加 `Public/` 或绝对磁盘路径。
- 反射类的 `XXX.generated.h` 仍使用原文件名，并保持为该头文件最后一条 include。
- 保持现有类名、模块名与原生默认组件名稳定，避免破坏蓝图的序列化引用。

## 3. 中文注释

注释重点解释用途、设计原因和边界：

1. 类：负责什么，与相邻模块如何分工。
2. 蓝图接口：何时调用、返回值含义、由谁维护数据。
3. 可配置参数：单位、含义及有效范围。
4. 关键流程：初始化先后关系、重复调用保护、状态互斥和回调重入。
5. 生命周期：委托、定时器和能力由谁创建、何时清理。
6. 测试：每组情景在验证哪个行为。

不逐行翻译显而易见的赋值语句。标识符仍用清晰的英文和 UE 命名规范，方便调用引擎接口。文件使用 UTF-8。

## 4. 修改后的验证

移动文件后检查 include、UHT 和完整 C++ 构建；涉及玩法变更时运行对应自动化测试。反射声明、继承或默认组件改变时关闭编辑器再编译，完成后重新生成 IDE 工程文件。蓝图表现仍需要 PIE 人工验收。

本次目录整理没有更改既有原生类名；因此无需重设 Shooter / Boot_Shooter_BP 父类。Shooter 应继续继承 MyShooter，敌人继续继承 ShooterCharacterBase。

官方目录规范参考：[Epic：Unreal Engine Modules](https://dev.epicgames.com/documentation/unreal-engine/unreal-engine-modules)。本项目 API 和编译验证以本机 UE 5.5 为准。
