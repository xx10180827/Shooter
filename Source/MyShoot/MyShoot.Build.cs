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
            "GameplayAbilities", "GameplayTags", "GameplayTasks"
        });

        // AI 控制器仅在角色死亡实现和测试中使用，无需成为公共接口依赖。
        PrivateDependencyModuleNames.AddRange(new string[] { "AIModule" });
    }
}
