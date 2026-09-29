// 验证真实敌人蓝图从枪口生成单个纯表现子弹，飞行/销毁不追加伤害。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "AI/ShooterAIController.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterBulletVisual.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterAIBulletTest,"MyShoot.Presentation.AIBullet",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FShooterAIBulletTest::RunTest(const FString& Parameters)
{
    UClass* EnemyClass=LoadClass<AShooterCharacterBase>(nullptr,TEXT("/Game/Blueprints/Boot_Shooter_BP.Boot_Shooter_BP_C"));
    UClass* AIClass=LoadClass<AShooterAIController>(nullptr,TEXT("/Game/Blueprints/Boot_Shooter_controller.Boot_Shooter_controller_C"));
    if(!EnemyClass||!AIClass) { AddError(TEXT("Saved AI classes missing")); return false; }
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false); GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    const uint64 SavedFrame=GFrameCounter;
    ON_SCOPE_EXIT { World->EndPlay(EEndPlayReason::Quit); GEngine->DestroyWorldContext(World); World->DestroyWorld(false); GFrameCounter=SavedFrame; };
    World->InitializeActorsForPlay(FURL()); World->BeginPlay(); World->GetWorldSettings()->NotifyBeginPlay();
    FActorSpawnParameters P; P.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AShooterCharacterBase* Enemy=World->SpawnActor<AShooterCharacterBase>(EnemyClass,FVector::ZeroVector,FRotator::ZeroRotator,P);
    AMyShooter* Target=World->SpawnActor<AMyShooter>(FVector(180,0,0),FRotator::ZeroRotator,P);
    AShooterAIController* AI=Cast<AShooterAIController>(Enemy->GetController()); if(!AI) { AI=World->SpawnActor<AShooterAIController>(AIClass); AI->Possess(Enemy); }
    Enemy->GetCharacterMovement()->DisableMovement(); Target->GetCharacterMovement()->DisableMovement();
    Enemy->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    AI->SetCombatTarget(Target);
    auto Advance=[World](float Seconds) { while(Seconds>KINDA_SMALL_NUMBER) { const float Step=FMath::Min(Seconds,0.005f); ++GFrameCounter; World->Tick(LEVELTICK_All,Step); Seconds-=Step; } };
    for(int32 Step=0;Step<600 && Target->GetGASHealth()==100.f;++Step) { Advance(0.005f); }
    int32 Count=0; AShooterBulletVisual* Bullet=nullptr;
    for(TActorIterator<AShooterBulletVisual> It(World);It;++It) { if(It->GetOwner()==Enemy) { ++Count; Bullet=*It; } }
    TestEqual(TEXT("One effective AI attack creates one bullet"),Count,1);
    FFloatProperty* DamageProperty=FindFProperty<FFloatProperty>(AShooterAIController::StaticClass(),TEXT("AttackDamage"));
    if(!TestNotNull(TEXT("AI damage configuration"),DamageProperty)) { return false; }
    const float Damage=DamageProperty->GetPropertyValue_InContainer(AI);
    TestEqual(TEXT("Damage remains configured single GAS hit"),Target->GetGASHealth(),FMath::Max(0.f,100.f-Damage));
    if(Bullet)
    {
        TestEqual(TEXT("Cosmetic bullet has no collision"),Bullet->GetBulletMesh()->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
        const FVector Initial=Bullet->GetActorLocation();
        USceneComponent* Muzzle=nullptr; TInlineComponentArray<USceneComponent*> Components(Enemy);
        for(USceneComponent* C:Components) { if(C->GetFName()==TEXT("Muzzle")) { Muzzle=C; break; } }
        TestNotNull(TEXT("Saved AI muzzle component"),Muzzle);
        if(Muzzle) { TestTrue(TEXT("Bullet starts from gun muzzle"),FVector::Dist(Initial,Muzzle->GetComponentLocation())<8.f); }
        AI->SuspendCombat(); Advance(0.015f);
        TestTrue(TEXT("Cosmetic bullet advances"),Bullet->IsActorBeingDestroyed() || FVector::Dist(Initial,Bullet->GetActorLocation())>0.1f);
    }
    const float HealthAfterShot=Target->GetGASHealth(); AI->SuspendCombat(); Advance(0.8f);
    TestEqual(TEXT("Flight and cleanup do not add damage"),Target->GetGASHealth(),HealthAfterShot);
    int32 Remaining=0; for(TActorIterator<AShooterBulletVisual> It(World);It;++It) { if(It->GetOwner()==Enemy) { ++Remaining; } }
    TestEqual(TEXT("Cosmetic bullet cleans itself up"),Remaining,0);
    return true;
}
#endif
