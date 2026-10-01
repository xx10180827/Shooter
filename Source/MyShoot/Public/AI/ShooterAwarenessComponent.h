#pragma once
#include "CoreMinimal.h"
#include "Perception/AIPerceptionComponent.h"
#include "ShooterAwarenessComponent.generated.h"
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;

/** 原生听觉/受击感知：只把事件发生时的位置交给调查状态，不提供墙后实时追踪。 */
UCLASS(ClassGroup=(Shooter), meta=(BlueprintSpawnableComponent))
class MYSHOOT_API UShooterAwarenessComponent : public UAIPerceptionComponent
{
    GENERATED_BODY()
public:
    UShooterAwarenessComponent();
    void RefreshListener();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    /** 厘米；枪声范围与音效音量独立，当前采用距离听觉，不模拟墙体声学衰减。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Awareness", meta=(ClampMin="100"))
    float HearingRange = 2400.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shooter|Awareness", meta=(ClampMin="0.5", ClampMax="20"))
    float InvestigationDuration = 4.f;
private:
    UFUNCTION()
    void HandleStimulus(AActor* Actor, FAIStimulus Stimulus);
    UPROPERTY()
    TObjectPtr<UAISenseConfig_Hearing> HearingConfig;
    UPROPERTY()
    TObjectPtr<UAISenseConfig_Damage> DamageConfig;
};