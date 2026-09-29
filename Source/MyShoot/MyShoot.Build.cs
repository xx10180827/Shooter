using UnrealBuildTool;

// 模块构建规则保留在根目录：Public 对外声明，Private 存放实现和测试。
public class MyShoot : ModuleRules
{
    public MyShoot(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // 公共头文件会使用角色、组件、GAS 和标签类型，依赖需要对使用本模块的代码可见。
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore",
            "GameplayAbilities", "GameplayTags", "GameplayTasks", "AIModule", "UMG"
        });

        // AIController 与 UserWidget 出现在公共头文件；Slate 仅供内部界面/测试使用。
        PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore", "NavigationSystem" });
    }
}
