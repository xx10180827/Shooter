#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "ShooterCharacterBase.generated.h"

class UAbilitySystemComponent;
class UShooterAttributeSet;
struct FOnAttributeChangeData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FShooterGASHealthChanged, float, OldHealth, float, NewHealth);

// 角色公共基类：每个角色独立持有 GAS 属性、能力系统和死亡状态；当前仅支持单机。
UCLASS()
class MYSHOOT_API AShooterCharacterBase : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    AShooterCharacterBase();

    // 对外通知：HUD 或受击表现订阅血量变化，不再维护第二份 Health。
    UPROPERTY(BlueprintAssignable, Category = "Shooter|GAS")
    FShooterGASHealthChanged OnGASHealthChanged;

    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

    /** 从唯一的 GAS 属性集中读取血量，供现有蓝图迁移使用。 */
    UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
    float GetGASHealth() const;

    UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
    float GetGASMaxHealth() const;

    UFUNCTION(BlueprintPure, Category = "Shooter|GAS")
    bool IsGASInitialized() const { return bGASInitialized; }

    UFUNCTION(BlueprintPure, Category = "Shooter|Death")
    bool HasGASDeathStarted() const { return bGASDeathStarted; }

    // 蓝图在死亡动画或延迟结束后调用；活着时和重复调用均不会触发额外清理。
    UFUNCTION(BlueprintCallable, Category = "Shooter|Death")
    void FinishGASDeath();

protected:
    // 死亡表现入口：进入事件前已停止移动、AI 和能力，蓝图只负责动画、音效与清理时机。
    UFUNCTION(BlueprintImplementableEvent, Category = "Shooter|Death")
    void OnGASDeathStarted();

    // 蓝图未主动结束死亡表现时的兜底清理时间，避免尸体永久残留。
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|Death",
        meta = (ClampMin = "0.1", UIMin = "0.1"))
    float DeathCleanupDelay = 5.0f;
    virtual void BeginPlay() override;
    virtual void PossessedBy(AController* NewController) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|GAS")
    TObjectPtr<UAbilitySystemComponent> ShooterAbilitySystemComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Shooter|GAS")
    TObjectPtr<UShooterAttributeSet> ShooterAttributes;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Shooter|GAS",
        meta = (ClampMin = "1.0", UIMin = "1.0"))
    float InitialMaxHealth = 100.0f;

private:
    // 初始化、属性监听、死亡状态推进只在本类内部执行，避免外部跳过规则。
    void InitializeGAS();
    void BeginGASDeath();
    void HandleHealthChanged(const FOnAttributeChangeData& Data);
    FDelegateHandle HealthChangedHandle;

    // 每个角色独立保存的一次性状态，重复伤害或回调不会再次启动死亡。
    bool bGASInitialized = false;
    bool bGASDeathStarted = false;
    bool bGASDeathFinished = false;
};
