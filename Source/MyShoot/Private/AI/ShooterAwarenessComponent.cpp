#include "AI/ShooterAwarenessComponent.h"
#include "AI/ShooterAIController.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "GameFramework/Pawn.h"

UShooterAwarenessComponent::UShooterAwarenessComponent()
{
    HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
    HearingConfig->HearingRange = 2400.f;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    // 项目尚未配置队伍 ID，原生过滤可能把同为 NoTeam 的玩家判为友方；
    // 先接收所有阵营，再由 HandleStimulus 的玩家身份过滤敌人及自身枪声。
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->SetMaxAge(4.f);
    DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("Damage"));
    DamageConfig->SetMaxAge(4.f);
    ConfigureSense(*HearingConfig);
    ConfigureSense(*DamageConfig);
}
void UShooterAwarenessComponent::BeginPlay()
{
    Super::BeginPlay();
    OnTargetPerceptionUpdated.AddUniqueDynamic(this, &UShooterAwarenessComponent::HandleStimulus);
    RefreshListener();
}
void UShooterAwarenessComponent::RefreshListener()
{
    HearingRange = FMath::IsFinite(HearingRange) ? FMath::Max(100.f, HearingRange) : 2400.f;
    InvestigationDuration = FMath::IsFinite(InvestigationDuration) ? FMath::Clamp(InvestigationDuration, 0.5f, 20.f) : 4.f;
    HearingConfig->HearingRange = HearingRange;
    ConfigureSense(*HearingConfig);
    RequestStimuliListenerUpdate();
}
void UShooterAwarenessComponent::HandleStimulus(AActor* Actor, FAIStimulus Stimulus)
{
    const APawn* Source = Cast<APawn>(Actor);
    AShooterAIController* Controller = Cast<AShooterAIController>(GetOwner());
    // 当前单机规则只对玩家产生敌意；队友枪声、感知过期、销毁后的事件都不触发警觉。
    if (!Controller || !IsValid(Source) || !Source->IsPlayerControlled() || !Stimulus.WasSuccessfullySensed()) { return; }
    if (Stimulus.Type != UAISense::GetSenseID<UAISense_Hearing>()
        && Stimulus.Type != UAISense::GetSenseID<UAISense_Damage>()) { return; }
    if (!Stimulus.StimulusLocation.ContainsNaN())
    {
        Controller->InvestigateLocation(Stimulus.StimulusLocation, InvestigationDuration);
    }
}
void UShooterAwarenessComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    OnTargetPerceptionUpdated.RemoveDynamic(this, &UShooterAwarenessComponent::HandleStimulus);
    Super::EndPlay(Reason);
}