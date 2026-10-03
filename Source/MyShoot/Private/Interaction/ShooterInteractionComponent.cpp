#include "Interaction/ShooterInteractionComponent.h"
#include "Pickups/ShooterPickup.h"
#include "Characters/MyShooter.h"
#include "GameFramework/PlayerController.h"
#include "Game/ShooterGameMode.h"
#include "Engine/World.h"
UShooterInteractionComponent::UShooterInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick=true; PrimaryComponentTick.TickInterval=.05f;
}
void UShooterInteractionComponent::BeginPlay()
{
    Super::BeginPlay();
    TArray<AActor*> Nearby; GetOwner()->GetOverlappingActors(Nearby,AShooterPickup::StaticClass());
    for(auto* Actor:Nearby) { RegisterCandidate(Cast<AShooterPickup>(Actor)); }
}
void UShooterInteractionComponent::RegisterCandidate(AShooterPickup* Pickup) { if(IsValid(Pickup)) { Candidates.Add(Pickup); } }
void UShooterInteractionComponent::UnregisterCandidate(AShooterPickup* Pickup)
{
    Candidates.Remove(Pickup); if(Focused.Get()==Pickup) { Focused.Reset(); }
}
void UShooterInteractionComponent::RefreshFocus()
{
    Focused.Reset(); auto* Player=Cast<AMyShooter>(GetOwner());
    auto* PC=Player?Cast<APlayerController>(Player->GetController()):nullptr;
    if(!Player||!PC||!PC->IsLocalController()||Player->HasGASDeathStarted()||!AShooterGameMode::IsCombatAllowed(this)) { return; }
    FVector Eye; FRotator Direction; PC->GetPlayerViewPoint(Eye,Direction);
    const FVector End=Eye+Direction.Vector()*1000.f; float Nearest=BIG_NUMBER;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(ShooterPickupVisibility),true,Player);
    for(auto It=Candidates.CreateIterator();It;++It)
    {
        auto* Pickup=It->Get(); if(!IsValid(Pickup)||Pickup->IsConsumed()) { It.RemoveCurrent(); continue; }
        FVector HitPoint;
        if(!Pickup->IsPlayerInRange(Player)||!Pickup->IntersectFocus(Eye,End,HitPoint)) { continue; }
        const float Distance=FVector::DistSquared(Eye,HitPoint); if(Distance>=Nearest) { continue; }
        // 触发区域可以穿墙，所以必须单独验证从摄像机到准星命中点之间没有 Visibility 阻挡。
        FHitResult Obstacle; if(GetWorld()->LineTraceSingleByChannel(Obstacle,Eye,HitPoint,ECC_Visibility,Query)) { continue; }
        Nearest=Distance; Focused=Pickup;
    }
}
void UShooterInteractionComponent::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction)
{ Super::TickComponent(DeltaTime,TickType,TickFunction); RefreshFocus(); }
bool UShooterInteractionComponent::TryInteract()
{
    // 不相信上一帧提示；按键时重新检查，防止离开范围/转向/隔墙后领取旧目标。
    RefreshFocus(); auto* Pickup=Focused.Get(); auto* Player=Cast<AMyShooter>(GetOwner());
    const bool bSuccess=Pickup&&Player&&Pickup->TryCollect(Player); RefreshFocus(); return bSuccess;
}
FText UShooterInteractionComponent::GetPrompt() const
{
    const auto* Player=Cast<AMyShooter>(GetOwner()); const auto* Pickup=Focused.Get();
    if(!Player||!IsValid(Pickup)||Pickup->IsConsumed()||Player->HasGASDeathStarted()||!AShooterGameMode::IsCombatAllowed(this)) { return FText::GetEmpty(); }
    FText Reason; const bool bCan=Pickup->CanReceive(Player->GetShooterWeapon(),Reason);
    return FText::Format(NSLOCTEXT("Shooter","PickupPrompt","{0}\n{1}"),Pickup->ItemName,bCan?FText::FromString(TEXT("按 F 拾取")):Reason);
}
void UShooterInteractionComponent::EndPlay(const EEndPlayReason::Type Reason)
{ Candidates.Empty(); Focused.Reset(); Super::EndPlay(Reason); }
