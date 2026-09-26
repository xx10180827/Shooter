// 数据订阅测试：首次读取、GAS 扣血、最大血量、更换 Pawn 和死亡归零。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "Characters/MyShooter.h"
#include "Combat/ShooterDamageLibrary.h"
#include "UI/ShooterHealthWidget.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "AbilitySystemComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterHealthHUDTest, "MyShoot.GAS.HealthHUD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FShooterHealthHUDTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    if (!World) { AddError(TEXT("No HUD test world")); return false; }
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT
    {
        World->EndPlay(EEndPlayReason::Quit);
        GEngine->DestroyWorldContext(World);
        World->DestroyWorld(false);
    };
    World->InitializeActorsForPlay(FURL());
    World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    AMyShooter* First = World->SpawnActor<AMyShooter>();
    AMyShooter* Second = World->SpawnActor<AMyShooter>(FVector(0,1000,0), FRotator::ZeroRotator);
    AActor* Source = World->SpawnActor<AActor>();
    UShooterHealthWidget* Widget = NewObject<UShooterHealthWidget>(World);
    if (!First || !Second || !Source || !Widget) { AddError(TEXT("HUD fixture failed")); return false; }
    Widget->ObserveCharacter(First);
    TestEqual(TEXT("Initial health without taking damage"), Widget->GetDisplayedHealth(), 100.0f);
    UShooterDamageLibrary::ApplyGASDamage(Source, First, 25, Source, FHitResult());
    TestEqual(TEXT("Damage updates HUD immediately"), Widget->GetDisplayedHealth(), 75.0f);
    TestEqual(TEXT("Correct health fraction"), Widget->GetDisplayedFraction(), 0.75f);
    First->GetAbilitySystemComponent()->ApplyModToAttribute(UShooterAttributeSet::GetMaxHealthAttribute(), EGameplayModOp::Override, 175);
    TestEqual(TEXT("MaxHealth update observed"), Widget->GetDisplayedMaxHealth(), 175.0f);
    Widget->ObserveCharacter(Second);
    UShooterDamageLibrary::ApplyGASDamage(Source, First, 25, Source, FHitResult());
    TestEqual(TEXT("Old pawn cannot alter new HUD"), Widget->GetDisplayedHealth(), 100.0f);
    UShooterDamageLibrary::ApplyGASDamage(Source, Second, 100, Source, FHitResult());
    TestEqual(TEXT("Death displays zero"), Widget->GetDisplayedHealth(), 0.0f);
    Widget->ObserveCharacter(nullptr);
    TestEqual(TEXT("Unpossess leaves empty bar"), Widget->GetDisplayedFraction(), 0.0f);
    return true;
}
#endif
