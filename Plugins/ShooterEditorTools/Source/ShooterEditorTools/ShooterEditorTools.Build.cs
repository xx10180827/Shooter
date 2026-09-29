using UnrealBuildTool;

// 仅编辑器使用的蓝图迁移工具，不进入游戏运行时模块。
public class ShooterEditorTools : ModuleRules
{
    public ShooterEditorTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        // 各迁移文件有独立的匿名辅助函数，关闭 Unity 合并避免跨文件重名冲突。
        bUseUnity = false;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "UnrealEd", "BlueprintGraph", "KismetCompiler", "Json", "MyShoot", "InputCore", "UMG", "UMGEditor", "Slate", "SlateCore", "AssetRegistry", "RenderCore", "RHI", "AIModule", "SlateRHIRenderer", "MeshDescription", "StaticMeshDescription", "AnimGraph", "AnimGraphRuntime", "AudioEditor", "NavigationSystem"
        });
    }
}
