#include "Pickups/ShooterPickup.h"
#include "Characters/MyShooter.h"
#include "Weapons/ShooterWeaponComponent.h"
#include "Interaction/ShooterInteractionComponent.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Game/ShooterGameMode.h"
AShooterPickup::AShooterPickup()
{
    PrimaryActorTick.bCanEverTick=false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("PickupRoot")));
    ProximityTrigger=CreateDefaultSubobject<USphereComponent>(TEXT("ProximityTrigger"));
    ProximityTrigger->SetupAttachment(RootComponent); ProximityTrigger->InitSphereRadius(250.f);
    ProximityTrigger->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ProximityTrigger->SetCollisionResponseToAllChannels(ECR_Ignore);
    ProximityTrigger->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
    ProximityTrigger->SetGenerateOverlapEvents(true);
    FocusBounds=CreateDefaultSubobject<UBoxComponent>(TEXT("FocusBounds"));
    FocusBounds->SetupAttachment(RootComponent); FocusBounds->InitBoxExtent(FVector(55,25,25));
    FocusBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    PresentationRoot=CreateDefaultSubobject<USceneComponent>(TEXT("PresentationRoot")); PresentationRoot->SetupAttachment(RootComponent);
    DisplayMesh=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DisplayMesh")); DisplayMesh->SetupAttachment(PresentationRoot);
    DisplayMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision); DisplayMesh->SetGenerateOverlapEvents(false);
    GroundMarker=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundMarker"));
    GroundMarker->SetupAttachment(RootComponent);
    GroundMarker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    GroundMarker->SetGenerateOverlapEvents(false);
    GroundMarker->SetCanEverAffectNavigation(false);
    GroundMarker->SetCastShadow(false);
    DisplayMesh->SetCanEverAffectNavigation(false); ProximityTrigger->SetCanEverAffectNavigation(false);
}
void AShooterPickup::BeginPlay()
{
    Super::BeginPlay(); RefreshGroundMarker();
    ProximityTrigger->OnComponentBeginOverlap.AddUniqueDynamic(this,&AShooterPickup::OnEnter);
    ProximityTrigger->OnComponentEndOverlap.AddUniqueDynamic(this,&AShooterPickup::OnLeave);
    // 兼容出生时已经位于范围内的角色，不依赖双方 BeginPlay 的先后顺序。
    TArray<AActor*> Actors; ProximityTrigger->GetOverlappingActors(Actors,AMyShooter::StaticClass());
    for(auto* Actor:Actors) { if(auto* I=Actor->FindComponentByClass<UShooterInteractionComponent>()) { I->RegisterCandidate(this); } }
}
void AShooterPickup::OnEnter(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32,bool,const FHitResult&)
{
    if(auto* Player=Cast<AMyShooter>(Other)) { if(auto* I=Player->FindComponentByClass<UShooterInteractionComponent>()) { I->RegisterCandidate(this); } }
}
void AShooterPickup::OnLeave(UPrimitiveComponent*,AActor* Other,UPrimitiveComponent*,int32)
{
    // 一个角色可以有多个重叠组件，最后一个组件离开后才解除候选登记。
    if(Other&&!ProximityTrigger->IsOverlappingActor(Other))
    { if(auto* I=Other->FindComponentByClass<UShooterInteractionComponent>()) { I->UnregisterCandidate(this); } }
}
bool AShooterPickup::IsPlayerInRange(const AMyShooter* Player) const
{
    return IsValid(Player)&&!bConsumed&&!IsActorBeingDestroyed()&&ProximityTrigger->IsOverlappingActor(Player)
        &&FVector::DistSquared(Player->GetActorLocation(),ProximityTrigger->GetComponentLocation())<=FMath::Square(ProximityTrigger->GetScaledSphereRadius());
}
bool AShooterPickup::IntersectFocus(const FVector& Start,const FVector& End,FVector& HitPoint) const
{
    const auto Transform=FocusBounds->GetComponentTransform();
    const FVector Extent=FocusBounds->GetUnscaledBoxExtent(); FVector LocalHit,Normal; float Time=0;
    const bool bHit=FMath::LineExtentBoxIntersection(FBox(-Extent,Extent),Transform.InverseTransformPosition(Start),
        Transform.InverseTransformPosition(End),FVector::ZeroVector,LocalHit,Normal,Time);
    if(bHit) { HitPoint=Transform.TransformPosition(LocalHit); } return bHit;
}
bool AShooterPickup::CanReceive(const UShooterWeaponComponent*,FText& Reason) const { Reason=FText::FromString(TEXT("不可拾取")); return false; }
bool AShooterPickup::Grant(UShooterWeaponComponent*) { return false; }
bool AShooterPickup::TryCollect(AMyShooter* Player)
{
    // 必须经交互组件重新校验准星、范围和遮挡后调用；先锁定以防事件重入领取同一物品。
    if(!HasAuthority()||bConsumed||bGranting||!IsPlayerInRange(Player)||Player->HasGASDeathStarted()
        ||!AShooterGameMode::IsCombatAllowed(this)) { return false; }
    FText Reason; auto* Weapon=Player->GetShooterWeapon();
    if(!Weapon||!CanReceive(Weapon,Reason)) { return false; }
    TGuardValue<bool> Guard(bGranting,true);
    if(!Grant(Weapon)) { return false; }
    bConsumed=true; SetActorEnableCollision(false); SetActorHiddenInGame(true);
    OnPickupGranted(Player); Destroy(); return true;
}
void AShooterPickup::EndPlay(const EEndPlayReason::Type Reason)
{
    ProximityTrigger->OnComponentBeginOverlap.RemoveAll(this); ProximityTrigger->OnComponentEndOverlap.RemoveAll(this);
    Super::EndPlay(Reason);
}
