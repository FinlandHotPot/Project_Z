// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "BaseCharacter.h" //基类定义
#include "AIController_NPC.generated.h"

/**
 * 用于控制NPC敌人（继承自ABaseCharacter) 的AI控制器
 * 此控制器负责：
 * 1.通过AIPerception(视觉，听觉）感知环境（特别是玩家）。
 * 2.管理行为树（Behavior Tree)和黑板（Blackblard)资产。
 * 3.协调AI的移动，战斗和状态切换，调用ABaseCharacter提供的公共接口。
 */
UCLASS()
class PROJECT_Z_API AAIController_NPC : public AAIController
{
	GENERATED_BODY()
    
public:
    //构造函数
    AAIController_NPC(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
    
protected:
    //游戏开始或控制器生成时调用
    virtual void BeginPlay() override;
    
    //当此控制器获得一个Pawn（即NPC角色）的控制权时调用
    virtual void OnPossess(APawn* InPawn) override;
    
    //当此控制器失去对Pawn的控制权的时候的时候调用
    virtual void OnUnPossess() override;
    
    //当此控制器控制的角色（受控Pawn)死亡时的回调
    UFUNCTION()
    void OnControlledCharacterDeath();
    
public:
    //获取当前控制的NPC角色（ABaseCharacter)的快捷方式
    UFUNCTION(BlueprintCallable, Category = "AI")
    ABaseCharacter* GetControlledBaseCharacter() const;
    
    //获取或者设置当前AI的当前目标（通常是玩家）
    UFUNCTION(BlueprintCallable, Category = "AI")
    AActor* GetTargetActor() const;
    
    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetTargetActor(AActor* NewTarget);
    
    //强制AI进入或者推出战斗状态（例如：当被玩家发现的时候）
    UFUNCTION(BlueprintCallable, Category = "AI")
    void SetAICombatState(bool bInCombat);
    
protected:
    //================================================
    //--- AI核心组件 ---
    //================================================
    
    /**AI感知组件，用于配置和接受视觉，听觉等刺激**/
    UPROPERTY(VisibleAnyWhere, BlueprintReadOnly, Category = "Conponents")
    class UAIPerceptionComponent* AIPerceptionComp;
    
    /**行为树运行的逻辑大脑**/
    UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "AI")
    class UBehaviorTree* BehaviorTreeAsset;
    
    /**存储AI状态和目标的键值对容器，供行为树和任务节点读取或者写入**/
    UPROPERTY(EditAnyWhere, BlueprintReadWrite, Category = "AI")
    class UBlackboardData* BlackboardAsset;
    
    //================================================
    //--- 感知回调 ---
    //================================================
    
    /**当AI感知组件检测到任何刺激，（如看到玩家，听到声音）时候调用**/
    UFUNCTION()
    void OnPerceptionUpdated(const TArray<AActor*>& UpdateActors);
    
    /**专门处理“看到”某个Actor的刺激**/
    UFUNCTION()
    void OnSeeTarget(AActor* Actor, FAIStimulus Stimulus);
    
    /**专门处理“听到”声音的刺激**/
    UFUNCTION()
    void OnHearSound(AActor* Actor, FAIStimulus Stimulus);
    
    //================================================
    //---  内部辅助函数 ---
    //================================================
    
    /**初始化行为树和黑板**/
    void InitializeBehaviorTree();
    
    /**根据当前与目标的距离，状态等，更新行为树黑板中的关键值**/
    void UpdateBlackboardKeys();
    
    /**协调AI的攻击行为，由行为树中的任务调用或定时器驱动**/
    void ExecuteAttackAction();
   
    /**检查与当前目标（玩家）的距离，判断是否进入近战攻击范围**/
    bool IsWithinAttackRange(float Range) const;
    
    /**检查与当前目标（玩家）的相对方位，判断是否在背后（用于刺杀判定**/
    bool IsBehindTarget() const;
    
    //================================================
    //---  内部状态 ---
    //================================================
	
    /**当前预订的目标Actor, 通常是玩家控制的AHero_Main **/
    UPROPERTY(BlueprintReadOnly, Category = "AI | State")
    AActor* CurrentTargetActor;
    
    /**AI自身是否处于战斗状态（非巡逻/闲逛）**/
    UPROPERTY(BlueprintReadOnly, Category = "AI | State")
    bool bIsInCombat;
    
    /**攻击冷却计时器句柄，防止AI无限连击**/
    FTimerHandle AttackCooldownTimerHandle;
    
    /**是否处于攻击冷却状态**/
    bool bIsAttackOnCooldown;
    
    /**攻击冷却时间（秒）**/
    UPROPERTY(EditDefaultsOnly, Category = "AI | Combat")
    float AttackCooldownDuration = 1.5f;
};
