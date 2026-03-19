// Fill out your copyright notice in the Description page of Project Settings.


#include "AIController_NPC.h"

#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Hero_Main.h" //用于感知到玩家时进行特定的处理
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"


//定义Blackboard键的名称，避免编码字符串
const FName BBKey_TargetActor("TargetActor");
const FName BBKey_bHasLineOfSight("bHasLineOfSight");
const FName BBKey_bIsInCombat("bIsInCombat");
const FName BBKey_TargetLocation("TargetLocation");
const FName BBKey_DistanceToTarget("DistanceToTarget");
const FName BBKey_IsWithinAttackRange("IsWithinAttackRange");

AAIController_NPC::AAIController_NPC(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    //步骤1:创建并设置AI感知组件
    AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComponent"));
    if (AIPerceptionComp)
    {
        // --- 配置视觉感知 ---
        UAISenseConfig_Sight* SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("Sight Config"));
        if(SightConfig)
        {
            SightConfig->SightRadius = 2000.0f; //能看到的最大的距离
            SightConfig->LoseSightRadius = 2200.0f; //超出此距离才判定为丢失目标
            SightConfig->PeripheralVisionAngleDegrees = 80.0f; //视野锥形角度
            SightConfig->DetectionByAffiliation.bDetectEnemies = true;
            SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
            SightConfig->DetectionByAffiliation.bDetectFriendlies = true;
            SightConfig->SetMaxAge(5.0f); //记忆刺激的时间
            AIPerceptionComp->ConfigureSense(*SightConfig);
        }
        
        // --- 配置听觉感知 ---
        UAISenseConfig_Hearing* HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing Config"));
        if (HearingConfig)
        {
            HearingConfig->HearingRange = 1000.0f; // 能听到的最大距离
            HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
            HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
            HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
            HearingConfig->SetMaxAge(3.0f);
            AIPerceptionComp->ConfigureSense(*HearingConfig);
        }
        
        
        //设置主导感知（决定哪个刺激优先）
        AIPerceptionComp->SetDominantSense(SightConfig->GetSenseImplementation());
        
        //绑定感知更新委托
        AIPerceptionComp->OnPerceptionUpdated.AddDynamic(this, &AAIController_NPC::OnPerceptionUpdated);
    }
    
    //步骤2:初始化默认变量
    
    CurrentTargetActor = nullptr;
    bIsInCombat = false;
    bIsAttackOnCooldown = false;
    
    //步骤3:启用Tick(选用，可用于持续更新黑板等）
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = true;
}

void AAIController_NPC::OnPossess(APawn* InPawn)
{
    // 步骤1: 必须首先调用父类（AAIController）的OnPossess。
    // 这是Unreal AI控制器初始化的标准流程，它负责设置基本的控制器-受控Pawn关系。
    Super::OnPossess(InPawn);

    // 步骤2: （可选）这里可以进行一些获取Pawn后的立即初始化。
    // 例如，设置AI的焦点目标，或者立即运行行为树。
    // 注意：本类当前的 `InitializeBehaviorTree()` 调用在 `BeginPlay()` 中，
    // 这通常是安全的，但如果在 `OnPossess` 中需要依赖行为树，也可以将初始化逻辑移到这里。
    
    // 示例：立即设置焦点到受控的Pawn自身（防止AI无目标时乱转）
    // SetFocus(InPawn);
    
    UE_LOG(LogTemp, Log, TEXT("AAIController_NPC::OnPossess - 已控制Pawn: %s"), *InPawn->GetName());
}

void AAIController_NPC::BeginPlay()
{
    //步骤1:调用父类方法，确保基础设置正确
    Super::BeginPlay();
    
    //步骤2:安全检查，确保控制的Pawn有效
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn)
    {
        UE_LOG(LogTemp, Warning, TEXT("AIController_NPC试图控制一个无效的Pawn"));
        return;
    }
    
    //步骤3:尝试将控制的Pawn转换为ABaseCharacter
    ABaseCharacter* ControlledCharacter = Cast<ABaseCharacter>(ControlledPawn);
    if (ControlledCharacter)
    {
        //绑定角色死亡事件，以便AI做出反应
        //注意：需要在ABaseCharacter的OnDeath函数中触发一个多播委托，这里假设已经存在
        //ControlledCharacter->OnDeath.AddDynamic(this, &AAIController_NPC::OnControlledCharacterDeath);
        
        UE_LOG(LogTemp, Log, TEXT("AIController_NPC 已控制角色：%s"), *ControlledCharacter->GetName());
        //步骤4: 初始化行为树和黑板
        InitializeBehaviorTree();
    }
    else
    {
        UE_LOG(LogTemp, Error, TEXT("AIControlled_NPC 只能控制继承自ABaseCharacter的Pawn.当前Pawn: %s"), *ControlledPawn->GetName());
    }
}

void AAIController_NPC::OnUnPossess()
{
    //步骤1:清楚当前目标
    CurrentTargetActor = nullptr;
    bIsInCombat = false;
    
    //步骤2:如果有行为树在运行，则停止它
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent);
    if (BTComp)
    {
        BTComp->StopTree(EBTStopMode::Safe);
    }
    
    //步骤3: 清楚攻击冷却定时器
    GetWorld()->GetTimerManager().ClearTimer(AttackCooldownTimerHandle);
    
    // 步骤4: 调用父类方法
    Super::OnUnPossess();
}

void AAIController_NPC::OnControlledCharacterDeath()
{
    //步骤1:AI控制的角色已经死亡，停止所有的AI行为
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent);
    if (BTComp)
    {
        BTComp->StopTree(EBTStopMode::Forced);
    }
    
    //步骤2:禁用感知插件
    if (AIPerceptionComp)
    {
        AIPerceptionComp->Deactivate();
    }
    
    // 步骤3: 播放死亡反应，或在一段时间后销毁控制器/角色
    UE_LOG(LogTemp, Warning, TEXT("AI控制的角色已死亡，AI控制器停止运行。"));
    
}


// ===============================================================================
// --- 公共接口函数 ---
// ===============================================================================

ABaseCharacter* AAIController_NPC::GetControlledBaseCharacter() const
{
    //返回当前控制的Pawn, 并转换为ABaseCharacter类型
    return Cast<ABaseCharacter>(GetPawn());
}


AActor* AAIController_NPC::GetTargetActor() const
{
    return CurrentTargetActor;
}


void AAIController_NPC::SetTargetActor(AActor* NewTarget)
{
    //步骤1: 更新内部变量
    CurrentTargetActor = NewTarget;
    
    //步骤2:更新黑板值，以便行为树能立即作出反应
    UBlackboardComponent* BBComp = UAIBlueprintHelperLibrary::GetBlackboard(this);
    if (BBComp)
    {
        BBComp->SetValueAsObject(BBKey_TargetActor, NewTarget);
        
        //步骤3:如果设置了新的目标，通常意味着进入战斗状态
        if (NewTarget)
        {
            SetAICombatState(true);
        }
        else
        {
            //目标丢失，可能需要退出战斗（由行为树决定）
            //SetAICombatState(false)
        }
        
    }
        
}

void AAIController_NPC::SetAICombatState(bool bInCombat)
{
    //步骤1:更新内部状态
    bIsInCombat = bInCombat;
    
    //步骤2:更新黑板值
    UBlackboardComponent* BBComp = UAIBlueprintHelperLibrary::GetBlackboard(this);
    if (BBComp)
    {
        BBComp->SetValueAsBool(BBKey_bIsInCombat, bInCombat);
    }
    //步骤3:如果进入战斗，确保角色状态也切换到战斗
    ABaseCharacter* MyChar = GetControlledBaseCharacter();
    if (bInCombat && MyChar && MyChar->IsAlive())
    {
        MyChar->SetCharacterState(ECharacterState::Combat);
    }
    //注意：退出战斗时，将角色状态改回Idle,通常由行为树在安全时处理
}


// ===============================================================================
// --- 感知系统回调 ---
// ===============================================================================

void AAIController_NPC::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
    //此函数在感知到任何刺激时被调用。我们遍历所有刺激，并分发给更具体的处理函数
    //注意：这是一个简化的实现，对于更复杂的AI,可能需要处理刺激的年龄，强度等。
    
    for (AActor* Actor : UpdatedActors)
    {
        if (!Actor) continue;
        
        
        //获取关于此Actor的所有最新刺激
        FActorPerceptionBlueprintInfo Info;
        if (AIPerceptionComp->GetActorsPerception(Actor, Info))
        {
            for(const FAIStimulus& Stimulus : Info.LastSensedStimuli)
            {
                //根据刺激类型分发处理
                if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
                {
                    OnSeeTarget(Actor, Stimulus);
                }
                else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
                {
                    OnHearSound(Actor, Stimulus);
                }
            }
        }
    }
}


void AAIController_NPC::OnSeeTarget(AActor* Actor, FAIStimulus Stimulus)
{
    //步骤1: 检查看到的Actor是否是玩家（AHero_Main)
    AHero_Main* SeenPlayer = Cast<AHero_Main>(Actor);
    if (!SeenPlayer)
    {
        return; //忽略非玩家目标
    }
    
    //步骤2:检查刺激是否成功（WasSuccessfullSensed)
    if(Stimulus.WasSuccessfullySensed())
    {
        //步骤3:发现玩家！
        UE_LOG(LogTemp, Log, TEXT("AI 发现了玩家：%s"), *SeenPlayer->GetName());
        
        //步骤4:设置当前目标
        SetTargetActor(SeenPlayer);
        
        //步骤5:更新黑板中的“视线”状态
        UBlackboardComponent* BBComp = UAIBlueprintHelperLibrary::GetBlackboard(this);
        if (BBComp)
        {
            BBComp->SetValueAsBool(BBKey_bHasLineOfSight, true);
        }
        
        //步骤6:调试绘制
        #if ENABLE_DRAW_DEBUG
        if (GetControlledBaseCharacter())
        {
            DrawDebugLine(GetWorld(),GetControlledBaseCharacter()->GetActorLocation(),
                          SeenPlayer->GetActorLocation(), FColor::Red, false, 2.0f, 0, 2.0f);
        }
        #endif // ENABLE_DRAW_DEBUG
        
    }
    else
    {
        //步骤7:丢失对目标的视线
        UE_LOG(LogTemp, Log, TEXT("AI 丢失了玩家的视线"));
        
        UBlackboardComponent* BBComp = UAIBlueprintHelperLibrary::GetBlackboard(this);
        if (BBComp)
        {
            BBComp->SetValueAsBool(BBKey_bHasLineOfSight, false);
            // 注意：这里不清除TargetActor，AI会记住最后已知位置（由刺激位置提供）
            BBComp->SetValueAsVector(BBKey_TargetLocation, Stimulus.StimulusLocation);
        }
    }
    //步骤8:无论是否看到，都更新一次黑板（例如距离）
    UpdateBlackboardKeys();
}

void AAIController_NPC::OnHearSound(AActor* Actor, FAIStimulus Stimulus)
{
    //步骤1:处理听到的声音（例如：玩家脚步声，攻击声）
    if (Stimulus.WasSuccessfullySensed())
    {
        UE_LOG(LogTemp, Log, TEXT("AI 听到了来自 %s 的声音"), Actor ? *Actor->GetName() : TEXT("未知来源"));
        
        //步骤2: 如果AI当前没有目标，但听到了声音，可以将声音来源设为可疑点进行调查
        UBlackboardComponent* BBComp = UAIBlueprintHelperLibrary::GetBlackboard(this);
        if (BBComp && !CurrentTargetActor)
        {
            // 设置最后已知位置为声音源头，但目标Actor仍为空
            BBComp->SetValueAsVector(BBKey_TargetLocation, Stimulus.StimulusLocation);
            // 行为树可以有一个“调查”任务，移动到该位置
        }
    }
}


// ==================================================
// --- 核心内部逻辑 ---
// ==================================================


void AAIController_NPC::InitializeBehaviorTree()
{
    //步骤1:安全检查
    if (!BehaviorTreeAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("AIController_NPC 没有分配行为树"));
        return;
    }
    
    //步骤2:运行行为树
    UBehaviorTreeComponent* BTComp = Cast<UBehaviorTreeComponent>(BrainComponent);
    if (!BTComp)
    {
        //如果还没有行为树组件，则创建一个
        BTComp = NewObject<UBehaviorTreeComponent>(this);
        BrainComponent = BTComp;
    }
    
    //步骤3:初始化并启动行为树
    if(BTComp)
    {
        //注意，RunBehaviorTree会自动处理黑板的初始化（如果行为树指定了黑板资产）
        BTComp->StartTree(*BehaviorTreeAsset,EBTExecutionMode::Looped);
        UE_LOG(LogTemp, Log, TEXT("AIController_NPC 已启动行为树: %s"), *GetNameSafe(BehaviorTreeAsset));
    }
}

void AAIController_NPC::UpdateBlackboardKeys()
{
    //此函数应该在Tick或事件驱动下调用， 以更新行为树所需要的关键信息
    UBlackboardComponent* BBComp = UAIBlueprintHelperLibrary::GetBlackboard(this);
    if (!BBComp || !CurrentTargetActor)
    {
        return;
    }
    
    ABaseCharacter* MyChar = GetControlledBaseCharacter();
    if (!MyChar)
    {
        return;
    }
    
    //步骤1: 更新目标位置
    BBComp->SetValueAsVector(BBKey_TargetLocation, CurrentTargetActor->GetActorLocation());
    
    //步骤2:计算并更新与目标的距离
    float Distance = FVector::Distance(MyChar->GetActorLocation(), CurrentTargetActor->GetActorLocation());
    BBComp->SetValueAsFloat(BBKey_DistanceToTarget, Distance);
    
    //步骤3:判断是否在攻击范围内（这里假设近战攻击范围为200单位）
    bool bInRange = IsWithinAttackRange(200.0f);
    BBComp->SetValueAsBool(BBKey_IsWithinAttackRange, bInRange);
}

void AAIController_NPC::ExecuteAttackAction()
{
    //步骤1:安全检查
    if (bIsAttackOnCooldown)
    {
        return; //攻击冷却中，忽略此次执行
    }
    
    ABaseCharacter* MyChar = GetControlledBaseCharacter();
    if (!MyChar || !MyChar->IsAlive())
    {
        return;
    }
    
    //步骤2:检查目标是否有效且在攻击范围内
    if (!CurrentTargetActor || !IsWithinAttackRange(250.0f))
    {
        return;
    }
    
    //步骤3:调用角色基类的攻击接口卡
    //这里可以根据行为树的决策（黑板值）选择轻重攻击
    MyChar->StartAttack(); //触发攻击状态和动画
    
    
    //步骤4:设置攻击冷却
    bIsAttackOnCooldown = true;
    GetWorld()->GetTimerManager().SetTimer(AttackCooldownTimerHandle,
        [this]()
        {
        bIsAttackOnCooldown = false;
        
        },
        AttackCooldownDuration,
        false
                                           );


       //步骤5， 调试
       UE_LOG(LogTemp, Log, TEXT("AI 执行攻击"));
}


bool AAIController_NPC::IsWithinAttackRange(float Range) const
{
    if (!CurrentTargetActor || !GetControlledBaseCharacter())
    {
        return false;
    }
    
    float Distance = FVector::Distance(GetControlledBaseCharacter()->GetActorLocation(),
                                       CurrentTargetActor->GetActorLocation());
    return Distance <= Range;
}

bool AAIController_NPC::IsBehindTarget() const
{
    //判断AI是否在目标身后（用于刺杀或者背刺判定）
    if (!CurrentTargetActor || !GetControlledBaseCharacter())
    {
        return false;
    }
    
    FVector ToTarget = CurrentTargetActor->GetActorLocation() - GetControlledBaseCharacter()->GetActorLocation();
    ToTarget.Normalize();
    
    FVector TargetForward = CurrentTargetActor->GetActorForwardVector();
    
    
    //计算点积， 如果小于0（角度大于90度）， 则在身后
    float DotProduct = FVector::DotProduct(ToTarget, TargetForward);
    return DotProduct < 0;
}


// ==================================================
// --- 可选的Tick函数，用于持续更新 ---
// ==================================================
// 可以在头文件中将PrimaryActorTick.bCanEverTick设为true，并在此实现
/*
void AAIController_NPC::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    
    // 持续更新黑板键值，确保行为树有最新信息
    UpdateBlackboardKeys();
    
    // 其他每帧需要执行的AI逻辑...
}
*/

