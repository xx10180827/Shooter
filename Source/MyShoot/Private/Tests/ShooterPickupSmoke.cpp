#include "Tests/ShooterWeaponSmokeSubsystem.h"
#include "Pickups/ShooterPickup.h"
#include "Pickups/ShooterWeaponPickup.h"
#include "Pickups/ShooterAmmoPickup.h"
#include "Interaction/ShooterInteractionComponent.h"
#include "Characters/MyShooter.h"
#include "Player/ShooterPlayerController.h"
#include "AI/ShooterAIController.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Weapons/ShooterWeaponDefinition.h"
#include "UI/ShooterAmmoWidget.h"
#include "Game/ShooterGameMode.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerInput.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "EngineUtils.h"
#include "Engine/StaticMesh.h"
#include "TimerManager.h"
#include "HAL/PlatformTime.h"
#if WITH_EDITOR
#include "ShaderCompiler.h"
#endif
bool UShooterWeaponSmokeSubsystem::StepPickup(float DeltaTime)
{
    if(bFinished) { return false; }
    const double Now=FPlatformTime::Seconds();
    if(Now>Deadline) { Finish(false,FString::Printf(TEXT("Pickup timeout phase %d"),Phase)); return false; }
    if(Now<NextTime) { return true; }
    auto* World=GetWorld(); auto* PC=World?Cast<AShooterPlayerController>(World->GetFirstPlayerController()):nullptr;
    auto* Player=PC?Cast<AMyShooter>(PC->GetPawn()):nullptr; auto* GM=World?Cast<AShooterGameMode>(World->GetAuthGameMode()):nullptr;
    if(!Player||!GM||!Player->IsGASInitialized()) { return true; }
    auto* W=Player->GetShooterWeapon(); auto* I=Player->FindComponentByClass<UShooterInteractionComponent>();
    auto Find=[World](int32 Index)->AShooterPickup*
    {
        const FName Tag(*FString::Printf(TEXT("T15_Pickup_%d"),Index));
        for(TActorIterator<AShooterPickup> It(World);It;++It) { if(It->ActorHasTag(Tag)&&!It->IsConsumed()) { return *It; } } return nullptr;
    };
    auto Position=[&](AShooterPickup* Pickup)
    {
        Player->SetActorLocation(Pickup->GetActorLocation()-FVector(140,0,12),false,nullptr,ETeleportType::TeleportPhysics);
        Pickup->ProximityTrigger->UpdateOverlaps();
    };
    auto Aim=[&](AShooterPickup* Pickup)
    {
        const auto* Camera=Player->FindComponentByClass<UCameraComponent>();
        const FVector Eye=Camera?Camera->GetComponentLocation():Player->GetPawnViewLocation();
        PC->SetControlRotation((Pickup->GetActorLocation()-Eye).Rotation()); PC->PlayerCameraManager->UpdateCamera(.016f); I->RefreshFocus();
    };
    auto Press=[PC](FKey Key){PC->InputKey(FInputKeyParams(Key,IE_Pressed,1.));PC->InputKey(FInputKeyParams(Key,IE_Released,0.));};
    switch(Phase)
    {
    case 0:
        if(!Check(I&&W->GetWeaponCount()==1&&W->GetEquippedSlot()==0,TEXT("Saved Shooter starts with rifle only"))||!Check(Find(0)&&Find(1)&&Find(2),TEXT("Three saved pickups exist"))) { return false; }
        for(int32 Index=0;Index<3;++Index)
        {
            auto* Item=Find(Index); auto* Material=Item->GroundMarker->GetMaterial(0);
            if(!Check(Material&&Material->GetPathName().Contains(TEXT("MI_PickupMarker")),TEXT("Pickup uses separate gold ground ring"))
                ||!Check(Item->GroundMarker->IsVisible()&&Item->GroundMarker->GetComponentLocation().Z<Item->GetActorLocation().Z-50,TEXT("Ring is projected below floating item"))
                ||!Check(Item->GroundMarker->GetCollisionEnabled()==ECollisionEnabled::NoCollision,TEXT("Ring does not block collision"))
                ||!Check(Item->DisplayMesh->GetMaterial(0)==Item->DisplayMesh->GetStaticMesh()->GetMaterial(0),TEXT("Pickup model retains original material"))) { return false; }
        }
        if(!Check(GM->StartRound(),TEXT("Start round"))) { return false; }
        for(TActorIterator<AShooterAIController> It(World);It;++It) { It->SuspendCombat(); World->GetTimerManager().ClearAllTimersForObject(*It); if(auto* P=Cast<ACharacter>(It->GetPawn())) { P->GetCharacterMovement()->DisableMovement(); } }
        Player->GetCharacterMovement()->DisableMovement(); Press(EKeys::Two);
        if(!Check(W->GetEquippedSlot()==0,TEXT("Actual key 2 cannot equip unowned shotgun"))) { return false; }
                // 先从斜上方记录真实地图中的完整光圈，再继续原本的 F 拾取回归。
        Player->SetActorLocation(Find(1)->GetActorLocation()+FVector(-320,0,230),false,nullptr,ETeleportType::TeleportPhysics);
        PC->SetControlRotation(FRotator(-48,0,0)); Next(30,3.f); break;
    case 30:
#if WITH_EDITOR
        // 新材质首次编译时暂不拍照，避免把暂时缺失的光圈当作最终画面。
        if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()) { Next(30,1.f); break; }
#endif
        Next(32,1.f); break;
    case 32:
        Capture(TEXT("00-GroundRings.png")); Next(31,.5f); break;
    case 31:
        Position(Find(0)); Next(1,2.f); break;
    case 1:
        Aim(Find(0)); Next(11,.5f); break;
    case 11:
    {
        auto* Item=Find(0); FVector Eye; FRotator Rotation; PC->GetPlayerViewPoint(Eye,Rotation); FVector Hit;
        const bool bHit=Item->IntersectFocus(Eye,Eye+Rotation.Vector()*1000.f,Hit);
        FHitResult Obstacle; FCollisionQueryParams Query(SCENE_QUERY_STAT(PickupSmoke),true,Player);
        const bool bWall=bHit&&World->LineTraceSingleByChannel(Obstacle,Eye,Hit,ECC_Visibility,Query);
        UE_LOG(LogTemp,Display,TEXT("Pickup diagnostic range=%d overlap=%d hit=%d wall=%s player=%s item=%s eye=%s view=%s focused=%s prompt=%s"),
            Item->IsPlayerInRange(Player),Item->ProximityTrigger->IsOverlappingActor(Player),bHit,*GetNameSafe(Obstacle.GetActor()),*Player->GetActorLocation().ToString(),
            *Item->GetActorLocation().ToString(),*Eye.ToString(),*Rotation.ToString(),*GetNameSafe(I->GetFocusedPickup()),*I->GetPrompt().ToString());
        Capture(TEXT("00-Diagnostic.png")); Next(12,.6f); break;
    }
    case 12:
        if(!Check(I->GetFocusedPickup()==Find(0)&&I->GetPrompt().ToString().Contains(TEXT("F")),TEXT("Actual trigger and aim produce F prompt"))) { return false; }
        Next(2,.6f); break;
    case 2:
        Capture(TEXT("01-ShotgunPrompt.png")); Next(3,.4f); break;
    case 3:
        Press(EKeys::F); Next(13,.25f); break;
    case 13:
        if(!Check(W->GetWeaponCount()==2&&!Find(0),TEXT("Actual F grants and consumes shotgun"))) { return false; }
        Press(EKeys::F); Next(14,.25f); break;
    case 14:
        if(!Check(W->GetWeaponCount()==2,TEXT("Repeated F grants nothing twice"))) { return false; }
        Press(EKeys::Two); Next(15,.25f); break;
    case 15:
        if(!Check(W->GetEquippedSlot()==1&&W->GetCurrentAmmo()==8,TEXT("Key 2 works after pickup"))) { return false; }
        Press(EKeys::LeftMouseButton); Next(25,.4f); break;
    case 25:
        if(!Check(W->GetCurrentAmmo()==7,TEXT("Shotgun has missing magazine ammo but full reserve"))) { return false; }
        Position(Find(2)); Next(4,.5f); break;
    case 4:
        Aim(Find(2)); Next(20,.4f); break;
    case 20:
        if(!Check(I->GetFocusedPickup()==Find(2),TEXT("Shotgun ammo focused"))) { return false; }
        Next(5,.6f); break;
    case 5:
        Capture(TEXT("02-AmmoPrompt.png")); Next(6,.4f); break;
    case 6:
        Press(EKeys::F); Next(16,.25f); break;
    case 16:
        if(!Check(W->GetCurrentAmmo()==8&&W->GetReserveAmmo()==32&&!Find(2),TEXT("Shotgun pickup restores 8/32 even with already full reserve"))
            ||!Check(PC->GetAmmoWidget()&&PC->GetAmmoWidget()->GetDisplayedReserve()==32&&PC->GetAmmoWidget()->GetDisplayedAmmo()==8,TEXT("HUD updates both magazine and reserve"))) { return false; }
        Press(EKeys::One); Next(21,.4f); break;
    case 21:
        // 实际按住开火、换弹，直到步枪 0/0；不直接改内部弹药变量。
        PC->SetControlRotation(FRotator(60,90,0));
        PC->InputKey(FInputKeyParams(EKeys::LeftMouseButton,IE_Pressed,1.)); Next(22,4.f); break;
    case 22:
        // 等待实际弹匣耗尽，不把墙钟等待当成固定射击次数；低帧率不应误判失败。
        if(W->GetCurrentAmmo()>0) { Next(22,.25f); break; }
        PC->InputKey(FInputKeyParams(EKeys::LeftMouseButton,IE_Released,0.));
        if(!Check(W->GetCurrentAmmo()==0,TEXT("Real rifle fire empties magazine"))) { return false; }
        if(W->GetReserveAmmo()>0) { Press(EKeys::R); Next(23,W->GetReloadDuration()+.4f); }
        else { Capture(TEXT("04-RifleEmpty.png")); Next(24,.4f); }
        break;
    case 23:
        if(!Check(W->GetCurrentAmmo()>0,TEXT("Real R reload transfers reserve"))) { return false; }
        Next(21,.2f); break;
    case 24:
        if(!Check(W->GetReserveAmmo()==0,TEXT("Rifle fully depleted to 0/0"))) { return false; }
        Position(Find(1)); Next(7,.5f); break;
    case 7:
        Aim(Find(1)); Next(17,.4f); break;
    case 17:
    {
        UE_LOG(LogTemp,Display,TEXT("Rifle refill before F: ammo=%d reserve=%d slot=%d focus=%s prompt=%s"),W->GetCurrentAmmo(),W->GetReserveAmmo(),W->GetEquippedSlot(),*GetNameSafe(I->GetFocusedPickup()),*I->GetPrompt().ToString());
        // 改变相机方向也会更新角色旋转和偏置；等待相机稳定后再精确对准物品。
        if(I->GetFocusedPickup()!=Find(1)) { Aim(Find(1)); Next(17,.4f); break; }
        Capture(TEXT("05-RifleRefillPrompt.png")); Next(26,.4f); break;
    }
    case 26:
        Press(EKeys::F); Next(18,.25f); break;
    case 18:
        Press(EKeys::One); Next(19,.25f); break;
    case 19:
        UE_LOG(LogTemp,Display,TEXT("Rifle refill after F: ammo=%d reserve=%d slot=%d focus=%s prompt=%s"),W->GetCurrentAmmo(),W->GetReserveAmmo(),W->GetEquippedSlot(),*GetNameSafe(I->GetFocusedPickup()),*I->GetPrompt().ToString());
        if(!Check(W->GetCurrentAmmo()==30&&W->GetReserveAmmo()==90&&!Find(1),TEXT("Actual F restores fully depleted rifle to 30/90"))) { return false; }
        if(!Check(PC->GetAmmoWidget()->GetDisplayedAmmo()==30&&PC->GetAmmoWidget()->GetDisplayedReserve()==90,TEXT("HUD shows 30/90 after depleted pickup"))) { return false; }
        Next(8,.5f); break;
    case 8:
        Capture(TEXT("03-Collected.png")); Next(9,.5f); break;
    case 9:
        Finish(true,TEXT("Ground rings and original model materials, real empty-rifle 0/0 to 30/90, shotgun 8/32 and HUD passed")); return false;
    }
    return true;
}
