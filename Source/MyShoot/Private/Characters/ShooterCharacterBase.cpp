#include "Characters/ShooterCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "Game/ShooterGameMode.h"
#include "GAS/Attributes/ShooterAttributeSet.h"
#include "GAS/Effects/ShooterInitialAttributesEffect.h"
#include "GAS/ShooterGameplayTags.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "Engine/LatentActionManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogShooterGAS, Log, All);

AShooterCharacterBase::AShooterCharacterBase()
{
    // 保留旧蓝图的 Event Tick，避免更换父类后原有逻辑停止。
    PrimaryActorTick.bCanEverTick = true;

    ShooterAbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(
        TEXT("ShooterAbilitySystemComponent"));
    ShooterAttributes = CreateDefaultSubobject<UShooterAttributeSet>(TEXT("ShooterAttributes"));
}

UAbilitySystemComponent* AShooterCharacterBase::GetAbilitySystemComponent() const
{
    return ShooterAbilitySystemComponent;
}

float AShooterCharacterBase::GetGASHealth() const
{
    return ShooterAttributes->GetHealth();
}

float AShooterCharacterBase::GetGASMaxHealth() const
{
    return ShooterAttributes->GetMaxHealth();
}

void AShooterCharacterBase::BeginPlay()
{
    // 父类 BeginPlay 会调用蓝图 BeginPlay，必须先初始化 GAS，保证蓝图读到有效血量。
    InitializeGAS();
    if (bGASInitialized && !HealthChangedHandle.IsValid())
    {
        HealthChangedHandle = ShooterAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            UShooterAttributeSet::GetHealthAttribute()).AddUObject(this, &AShooterCharacterBase::HandleHealthChanged);
    }
    Super::BeginPlay();
    if (AShooterGameMode* GM = Cast<AShooterGameMode>(GetWorld()->GetAuthGameMode())) { GM->RegisterCombatant(this); }
}

void AShooterCharacterBase::InitializeGAS()
{
    if (bGASInitialized)
    {
        return;
    }

    // 当前采用单机角色自持 ASC；多人同步和预测需要另行设计。
    ShooterAbilitySystemComponent->InitAbilityActorInfo(this, this);
    if (!HasAuthority())
    {
        return;
    }

    FGameplayEffectContextHandle Context = ShooterAbilitySystemComponent->MakeEffectContext();
    Context.AddSourceObject(this);
    FGameplayEffectSpecHandle Spec = ShooterAbilitySystemComponent->MakeOutgoingSpec(
        UShooterInitialAttributesEffect::StaticClass(), 1.0f, Context);
    if (!Spec.IsValid())
    {
        UE_LOG(LogShooterGAS, Error, TEXT("%s: could not create initial attributes effect."), *GetName());
        return;
    }

    const float SafeMaxHealth = FMath::IsFinite(InitialMaxHealth)
        ? FMath::Max(InitialMaxHealth, 1.0f) : 100.0f;
    Spec.Data->SetSetByCallerMagnitude(UShooterInitialAttributesEffect::GetInitialHealthTag(), SafeMaxHealth);
    ShooterAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

    // 瞬时效果不会留下有效的持续效果句柄，因此用实际属性值判断初始化是否成功。
    bGASInitialized = FMath::IsNearlyEqual(GetGASMaxHealth(), SafeMaxHealth)
        && FMath::IsNearlyEqual(GetGASHealth(), SafeMaxHealth);
    if (!bGASInitialized)
    {
        UE_LOG(LogShooterGAS, Error, TEXT("%s: initial attributes did not reach the configured values."), *GetName());
    }
}

void AShooterCharacterBase::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    if (bGASInitialized)
    {
        // 控制器接管时只刷新缓存，不能重复初始化而把残血角色回满。
        ShooterAbilitySystemComponent->RefreshAbilityActorInfo();
    }
}

// 生命周期结束时解除监听；退出 PIE 或销毁角色后不再处理旧属性回调。
void AShooterCharacterBase::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (HealthChangedHandle.IsValid())
    {
        ShooterAbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            UShooterAttributeSet::GetHealthAttribute()).Remove(HealthChangedHandle);
        HealthChangedHandle.Reset();
    }
    GetWorldTimerManager().ClearAllTimersForObject(this);
    Super::EndPlay(EndPlayReason);
    ShooterAbilitySystemComponent->ClearActorInfo();
    bGASInitialized = false;
}

void AShooterCharacterBase::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
    if (Data.OldValue != Data.NewValue)
    {
        if (bGASInitialized && Data.NewValue <= 0.0f)
        {
            BeginGASDeath();
        }
        // 死亡表现事件可能立即销毁角色；仅在角色仍有效时广播血量变化。
        if (!IsActorBeingDestroyed())
        {
            OnGASHealthChanged.Broadcast(Data.OldValue, Data.NewValue);
        }
    }
}

void AShooterCharacterBase::BeginGASDeath()
{
    if (!HasAuthority() || !bGASInitialized || bGASDeathStarted
        || IsActorBeingDestroyed() || GetGASHealth() > 0.0f)
    {
        return;
    }

    // 先设置死亡标记和标签，防止后续回调重入时再次扣血、回血或激活能力。
    bGASDeathStarted = true;
    ShooterAbilitySystemComponent->AddLooseGameplayTag(ShooterGameplayTags::State_Dead);
    if (IsActorBeingDestroyed()) { return; }
    OnGASDeathConfirmed.Broadcast(this);
    if (IsActorBeingDestroyed()) { return; }
    ShooterAbilitySystemComponent->CancelAllAbilities();
    if (IsActorBeingDestroyed())
    {
        return;
    }

    SetActorTickEnabled(false);
    GetCharacterMovement()->StopMovementImmediately();
    GetCharacterMovement()->DisableMovement();
    GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // AI 停止追踪与逻辑循环；玩家停止输入。保留网格自身动画更新。
    if (AAIController* AIController = Cast<AAIController>(GetController()))
    {
        AIController->StopMovement();
        if (UBrainComponent* Brain = AIController->GetBrainComponent())
        {
            Brain->StopLogic(TEXT("GAS character death"));
        }
        AIController->SetActorTickEnabled(false);
        GetWorldTimerManager().ClearAllTimersForObject(AIController);
        GetWorld()->GetLatentActionManager().RemoveActionsForObject(AIController);
    }
    else if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        DisableInput(PlayerController);
        PlayerController->SetIgnoreMoveInput(true);
        PlayerController->SetIgnoreLookInput(true);
    }

    // 先清除旧蓝图的射击、追踪计时器和延迟，再启动新的死亡表现。
    GetWorldTimerManager().ClearAllTimersForObject(this);
    GetWorld()->GetLatentActionManager().RemoveActionsForObject(this);
    if (IsActorBeingDestroyed())
    {
        return;
    }

    const float SafeDelay = FMath::IsFinite(DeathCleanupDelay)
        ? FMath::Max(0.1f, DeathCleanupDelay) : 5.0f;
    SetLifeSpan(SafeDelay);
    // 保留网格动画与物理供蓝图控制；当前死亡流程使用原地动画。
    OnGASDeathStarted();
}

void AShooterCharacterBase::FinishGASDeath()
{
    if (!HasAuthority() || !bGASDeathStarted || bGASDeathFinished || IsActorBeingDestroyed())
    {
        return;
    }
    bGASDeathFinished = true;
    SetLifeSpan(0.0f);
    Destroy();
}