#include "Pickups/ShooterPickup.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

void AShooterPickup::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    RefreshGroundMarker();
}

void AShooterPickup::RefreshGroundMarker()
{
    if(!GroundMarker || !GetWorld() || IsTemplate()) { return; }
    // 物品悬浮展示，标记落在下方地面。放置/移动 Actor 与开始游戏时各刷新，无常驻 Tick。
    const FVector Origin=GetActorLocation();
    FCollisionQueryParams Query(SCENE_QUERY_STAT(PickupMarkerGround),true,this);
    FCollisionObjectQueryParams Objects(ECC_WorldStatic);
    FHitResult Ground;
    const bool bGround=GetWorld()->LineTraceSingleByObjectType(Ground,Origin+FVector(0,0,40),Origin-FVector(0,0,600),Objects,Query);
    GroundMarker->SetVisibility(bGround);
    if(!bGround) { return; }
    GroundMarker->SetWorldLocation(Ground.ImpactPoint+Ground.ImpactNormal*MarkerGroundOffset);
    GroundMarker->SetWorldRotation(FRotationMatrix::MakeFromZ(Ground.ImpactNormal).Rotator());
    // 引擎 Plane 边长 100 cm，使用世界尺寸防止模型自身缩放改变圆环大小。
    GroundMarker->SetWorldScale3D(FVector(FMath::Max(MarkerRadius,5.f)*2.f/100.f));
}