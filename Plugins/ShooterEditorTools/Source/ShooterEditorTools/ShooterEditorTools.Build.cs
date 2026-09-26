using UnrealBuildTool;

// 仅编辑器使用的蓝图迁移工具，不进入游戏运行时模块。
public class ShooterEditorTools : ModuleRules
{
    public ShooterEditorTools(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
        PrivateDependencyModuleNames.AddRange(new string[] {
            "UnrealEd", "BlueprintGraph", "KismetCompiler", "Json", "MyShoot", "InputCore", "UMG", "UMGEditor", "Slate", "SlateCore", "AssetRegistry", "RenderCore", "RHI", "AIModule", "SlateRHIRenderer"
        });
    }
}
