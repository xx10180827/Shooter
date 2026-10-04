#include "Tests/ShooterWeaponSmokeSubsystem.h"
#include "Characters/MyShooter.h"
#include "Movement/ShooterDashComponent.h"
#include "Player/ShooterPlayerController.h"
#include "AI/ShooterAIController.h"
#include "Game/ShooterGameMode.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/CameraComponent.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
bool UShooterWeaponSmokeSubsystem::StepDash(float)
{
    if(bFinished) { return false; } const double Now=FPlatformTime::Seconds();
    if(Now>Deadline) { Finish(false,FString::Printf(TEXT("Dash timeout phase %d"),Phase)); return false; }
    if(Now<NextTime) { return true; }
    auto* World=GetWorld(); auto* PC=World?Cast<AShooterPlayerController>(World->GetFirstPlayerController()):nullptr;
    auto* Player=PC?Cast<AMyShooter>(PC->GetPawn()):nullptr; auto* GM=World?Cast<AShooterGameMode>(World->GetAuthGameMode()):nullptr;
    if(!Player||!GM||!Player->IsGASInitialized()) { return true; }
    auto* Dash=Player->FindComponentByClass<UShooterDashComponent>(); auto* Camera=Player->FindComponentByClass<UCameraComponent>();
    auto Press=[PC](FKey Key){PC->InputKey(FInputKeyParams(Key,IE_Pressed,1.)); PC->InputKey(FInputKeyParams(Key,IE_Released,0.));};
    switch(Phase)
    {
    case 0:
        if(!Check(Dash&&Camera,TEXT("Existing Shooter blueprint inherits dash component"))||!Check(!Dash->TryDash(),TEXT("Menu blocks dash"))) { return false; }
        GM->StartRound();
        for(TActorIterator<AShooterAIController> It(World);It;++It) { It->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(*It); if(auto* C=Cast<ACharacter>(It->GetPawn())) { C->GetCharacterMovement()->DisableMovement(); } }
        Player->SetActorLocation(FVector(-3070,1450,820),false,nullptr,ETeleportType::TeleportPhysics); PC->SetControlRotation(FRotator::ZeroRotator);
        SavedCameraFOV=Camera->FieldOfView; Next(1,2.f); break;
    case 1:
        if(!Check(Dash->GetStamina()==100.f,TEXT("Real map initial stamina 100"))||!Check(Player->GetCharacterMovement()->IsMovingOnGround(),TEXT("Player grounded"))) { return false; }
        // 等待新导入的 UI 材质完成编译，防止截图只保留数字而漏掉体力框。
#if WITH_EDITOR
        if(GShaderCompilingManager && GShaderCompilingManager->IsCompiling()) { Next(1,.5f); break; }
#endif
        Next(10,1.f); break;
    case 10:
        Capture(TEXT("01-ReadyHUD.png")); Next(2,.5f); break;
    case 2:
        DashStart=Player->GetActorLocation(); Press(EKeys::LeftShift); Next(3,.09f); break;
    case 3:
        if(!Check(Dash->IsDashing()&&FMath::IsNearlyEqual(Dash->GetStamina(),75.f),TEXT("Actual Shift activates dash and spends 25"))
            ||!Check(!Player->GetShooterWeapon()->StartFiring(),TEXT("Actual dash blocks fire"))
            ||!Check(Camera->FieldOfView>SavedCameraFOV,TEXT("Dash camera feedback visible"))) { return false; }
        Capture(TEXT("02-DashingHUD.png")); Press(EKeys::LeftShift); Next(4,.4f); break;
    case 4:
    {
        const float Travel=FVector::Dist2D(DashStart,Player->GetActorLocation());
        UE_LOG(LogTemp,Display,TEXT("Actual dash travel %.2f cm; stamina %.1f; cooldown %.2f"),Travel,Dash->GetStamina(),Dash->GetCooldownRemaining());
        if(!Check(!Dash->IsDashing()&&FMath::Abs(Travel-Dash->DashDistance)<20.f,TEXT("Ground dash ends at configured distance"))
            ||!Check(Dash->GetStamina()==75.f,TEXT("Repeated Shift during cooldown has no extra cost"))
            ||!Check(FMath::IsNearlyEqual(Camera->FieldOfView,SavedCameraFOV),TEXT("FOV restored after dash"))) { return false; }
        Capture(TEXT("03-CooldownHUD.png")); Next(5,1.4f); break;
    }
    case 5:
        if(!Check(Dash->GetCooldownRemaining()==0.f&&Dash->GetStamina()>75,TEXT("Cooldown GE expires and stamina regenerates"))) { return false; }
        PC->SetControlRotation(FRotator(0,180,0)); Press(EKeys::LeftShift); Next(6,.07f); break;
    case 6:
        if(!Check(Dash->IsDashing(),TEXT("Second dash starts after cooldown"))) { return false; }
        Press(EKeys::P); Next(7,.12f); break;
    case 7:
        if(!Check(GM->GetRoundState()==EShooterRoundState::Paused&&!Dash->IsDashing(),TEXT("Actual P pause cancels dash"))
            ||!Check(FMath::IsNearlyEqual(Camera->FieldOfView,SavedCameraFOV),TEXT("Pause immediately restores camera"))) { return false; }
        DashStart=Player->GetActorLocation(); GM->TogglePause(); Next(8,.4f); break;
    case 8:
        if(!Check(FVector::Dist2D(DashStart,Player->GetActorLocation())<1.f,TEXT("Resume has no old root motion"))) { return false; }
        Capture(TEXT("04-ResumedHUD.png")); Next(9,.5f); break;
    case 9:
        Finish(true,TEXT("Actual Shift dash, stamina arc, cooldown icon, camera, pause cleanup verified")); return false;
    }
    return true;
}