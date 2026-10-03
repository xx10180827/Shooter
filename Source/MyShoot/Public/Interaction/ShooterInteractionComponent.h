#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShooterInteractionComponent.generated.h"
class AShooterPickup;
/** 范围触发器登记候选；本地准星选中与 F 输入复检使用同一条检测路径。 */
UCLASS(ClassGroup=(Shooter),meta=(BlueprintSpawnableComponent))
class MYSHOOT_API UShooterInteractionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UShooterInteractionComponent();
    void RegisterCandidate(AShooterPickup* Pickup);
    void UnregisterCandidate(AShooterPickup* Pickup);
    UFUNCTION(BlueprintCallable, Category="Shooter|Interaction") bool TryInteract();
    UFUNCTION(BlueprintPure, Category="Shooter|Interaction") FText GetPrompt() const;
    AShooterPickup* GetFocusedPickup() const { return Focused.Get(); }
    void RefreshFocus();
protected:
    virtual void BeginPlay() override;
    virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* TickFunction) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TSet<TWeakObjectPtr<AShooterPickup>> Candidates;
    TWeakObjectPtr<AShooterPickup> Focused;
};
