// Fill out your copyright notice in the Description page of Project Settings.


#include "BaseCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

// Sets default values
ABaseCharacter::ABaseCharacter()
{
     // Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    
    PrimaryActorTick.bCanEverTick = true;
    
    // --- 开始拼装角色肉体，基础四件套 ---
    
    //1.制造胶囊体，设置胶囊体组件为根组件
    CapsuleComp = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BaseCapsule"));
    RootComponent = CapsuleComp;
    CapsuleComp->InitCapsuleSize(34.0f, 88.0f);
    CapsuleComp->SetCollisionProfileName(TEXT("Pawn"));
    
    //2.制造骨骼模型，并把它挂载（Attach)到胶囊体里面
    MeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BaseMesh"));
    MeshComp->SetupAttachment(RootComponent);
    MeshComp->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
    MeshComp->SetRelativeRotation(FRotator(0.0f, -90.0f, 0.0f));
    MeshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
    
    
    // --- 初始化默认参数值 （与头文件声明保持一致）---
    MovementParams = FBaseMovementParams();
    Attributes = FCharacterAttributes();
    PhysicsParams = FCustomPhysicsParams();
    
    
    // --- 初始化状态变量 ---
    CurrentCharacterState = ECharacterState::Idle;
    CurrentMoveState = EMoveState::Grounded;
    CurrentCombatAction = ECombatAction::None;
       

}

// Called when the game starts or when spawned
void ABaseCharacter::BeginPlay()
{
    Super::BeginPlay();
}

// Called every frame
void ABaseCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    //-执行物理与重力推演-
    ApplyBasePhysics(DeltaTime);
    
    //-持续检测地面状态-
    CheckGroundStatus();
    
    //-更新当前速度大小（用于动画蓝图等）
    CurrentSpeedMagnitude = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f).Size();
    
    // 此处可以放置每帧的状态更新逻辑，例如：
    // - 根据速度更新CurrentSpeedState (Walking, Jogging, Sprinting)
    // - 检测是否落地以更新CurrentMoveState (Grounded, InAir)
    // - 处理滑翔、攀爬等特殊移动的持续效果
    // - 更新角色属性（如耐力恢复、架势恢复等）

}


// ==================================================
// --- 输入函数实体 （屏幕测试版） ---
// ==================================================

void ABaseCharacter::SetCharacterState(ECharacterState NewState)
{
    // 以后可以在这里统一处理状态切换逻辑
    if (CurrentCharacterState != NewState && IsAlive()) //死亡后不可改变状态
    {
        ECharacterState PreviousState = CurrentCharacterState;
        CurrentCharacterState = NewState;
        
        //调试输出
        if (GEngine)
        {
            FString StateName = UEnum::GetValueAsString(NewState);
            GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::White, FString::Printf(TEXT("角色状态改变: %s"),*StateName));
        }
        //触发蓝图事件
        OnCharacterStateChanged(NewState, PreviousState);
        
        //状态特定逻辑
        switch (NewState)
        {
            case ECharacterState::Dead:
                OnDeath();
                break;
                //其他状态默认处理...
        }
    }
    
}

void ABaseCharacter::SetMoveState(EMoveState NewState)
{
    // 以后可以在这里统一处理状态切换逻辑
    if (CurrentMoveState !=NewState)
    {
        EMoveState PreviousState = CurrentMoveState;
        CurrentMoveState = NewState;
        
        OnMoveStateChanged(NewState, PreviousState);
    }
}

// ==================================================
// --- 移动控制接口 ---
// ==================================================


// ==================================================
// --- 核心手写物理推演 ---
// ==================================================

void ABaseCharacter::ApplyBasePhysics(float DeltaTime)
{
    //如果在空中，应用重力公式：V_old - g * Deltatime
    if (CurrentMoveState == EMoveState::InAir || CurrentMoveState == EMoveState::Falling)
    {
        float GravityAcceleration = 980.0f * PhysicsParams.GravityScale;
        CurrentVelocity.Z -= GravityAcceleration * DeltaTime;
        //限制最大下坠速度防止穿模
        CurrentVelocity.Z = FMath::Max(CurrentVelocity.Z, -PhysicsParams.TerminalVelocity);
    }
    else if (CurrentMoveState == EMoveState::Grounded)
    {
        //在地面时候没有下坠速度（不考虑下坡版本）
        CurrentVelocity.Z = 0.0f;
    }
    
    //根据计算出的速度，移动主角的肉体（Sweep=true, 表示会触发碰撞，撞墙会停下）
    FVector DeltaLocation = CurrentVelocity * DeltaTime;
    FHitResult HitResult;
    AddActorWorldOffset(DeltaLocation, true, &HitResult);
    
    //简单碰撞处理
    if (HitResult.bBlockingHit)
    {
        if (FMath::Abs(HitResult.Normal.Z)>0.7f)
        {
            if (HitResult.Normal.Z>0.7f && CurrentVelocity.Z < 0) //落地
            {
                SetMoveState(EMoveState::Grounded);
                CurrentVelocity.Z = 0.0f;
            }
            else if (HitResult.Normal.Z < -0.7f && CurrentVelocity.Z > 0) //撞到天花板
            {
                CurrentVelocity.Z = 0.0f;
            }
        }
        else // 撞到墙壁
        {
            //简单的墙壁滑动处理
            FVector Normal = HitResult.Normal;
            Normal.Z = 0;
            Normal.Normalize();
            
            FVector VelXY = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0);
            FVector ReflectedVel = VelXY - 2 * (VelXY | Normal) * Normal;
            ReflectedVel *= 0.7f;
            
            CurrentVelocity.X = ReflectedVel.X;
            CurrentVelocity.Y = ReflectedVel.Y;
            
        }
    }
}


void ABaseCharacter::CheckGroundStatus()
{
    //如果当前再空中且正在下落，则检测脚下
    if ((CurrentMoveState == EMoveState::InAir || CurrentMoveState == EMoveState::Falling) && CurrentVelocity.Z <= 0)
    {
        FVector Start = GetActorLocation();
        FVector End = Start - FVector(0, 0, 105.0f);// 检测距离略大于胶囊体一半高度
        
        
        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);
        
        bool bHit = GetWorld()->LineTraceSingleByChannel(
                                                         HitResult,
                                                         Start,
                                                         End,
                                                         ECC_WorldStatic,
                                                         QueryParams
                                                         );
        
        if (bHit && HitResult.Distance < 100.0f) //距离足够近认为落地
        {
            SetMoveState(EMoveState::Grounded);
            CurrentVelocity.Z = 0.0f;
        }
        else if (CurrentVelocity.Z < 0)
        {
            //未检测到地面且速度向下，切换为下落状态
            SetMoveState(EMoveState::Falling);
        }
    }
}

void ABaseCharacter::OnDeath()
{
    //基础死亡处理：禁用碰撞，停止移动，播放死亡动画（蓝图）
    if (CapsuleComp)
    {
        CapsuleComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
    CurrentVelocity = FVector::ZeroVector;
    MoveInputDirection = FVector::ZeroVector;
    //注意：不要在此处销毁Actor, 可以由蓝图或子类处理后续（如掉落物品）
}


void ABaseCharacter::ApplyDamage(float DamageAmount)
{
    if (!IsAlive() || CurrentCharacterState == ECharacterState::Dead)
    {
        return; // 已死亡，不再受到伤害
    }
    
    // 扣除生命值
    Attributes.CurrentHealth -= DamageAmount;
    
    // 调试输出
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, FString::Printf(TEXT("受到伤害: %.1f, 剩余生命: %.1f"), DamageAmount, Attributes.CurrentHealth));
    }
    
    // 检查是否死亡
    if (Attributes.CurrentHealth <= 0.0f)
    {
        Attributes.CurrentHealth = 0.0f;
        SetCharacterState(ECharacterState::Dead);
    }
    else
    {
        // 未死亡，可进入受击状态
        // SetCharacterState(ECharacterState::Stunned);
    }
}

void ABaseCharacter::StartAttack()
{
   //切换到攻击状态
    CurrentCombatAction = ECombatAction::Attacking;
    
    //调试输出
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, -1.5f, FColor::Yellow, TEXT("基类：开始攻击"));
    }
    //注意：实际攻击逻辑（如检测命中、播放动画）应在子类中重写此函数，或通过动画通知触发
}


void ABaseCharacter::StartDefend()
{
    // 切换到防御动作状态
    CurrentCombatAction = ECombatAction::Defending;
    
    // 调试输出
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.5f, FColor::Blue, TEXT("基类: 开始防御"));
    }
    
    // 注意：实际防御逻辑（如弹反窗口、减伤计算）应在子类中重写
}


void ABaseCharacter::Move(const FVector& Direction, float Scale)
{
    //保存输入方向，用于后续物理计算
    MoveInputDirection = Direction * Scale;
    
    //计算期望速度
    FVector DesiredVelocity = MoveInputDirection * MovementParams.MaxWalkSpeed;
    
    //当前速度向期望速度插值（模拟加速度）
    FVector VelXY = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0);
    FVector NewVelXY = FMath::VInterpConstantTo(VelXY, DesiredVelocity, GetWorld()->GetDeltaSeconds(), MovementParams.Acceleration);
    
    CurrentVelocity.X = NewVelXY.X;
    CurrentVelocity.Y = NewVelXY.Y;
    
    
    //如果需要立即停止移动（Scale为0或Direction为零向量），应用刹车
    if (Scale == 0.0f || Direction.IsNearlyZero())
    {
        FVector BrakeDecel = VelXY.GetSafeNormal() * PhysicsParams.BrakingDeceleration * GetWorld()->GetDeltaSeconds();
        if (BrakeDecel.SizeSquared() > VelXY.SizeSquared())
        {
            CurrentVelocity.X = 0;
            CurrentVelocity.Y = 0;
        }
        else
        {
            CurrentVelocity.X -= BrakeDecel.X;
            CurrentVelocity.Y -= BrakeDecel.Y;
        }
    }
}

void ABaseCharacter::Jump()
{
    //只有在地面状态时才允许跳跃
    if (CurrentMoveState == EMoveState::Grounded)
    {
        CurrentVelocity.Z = MovementParams.JumpVelocity;
        SetMoveState(EMoveState::InAir);
        
        
        //调试输出
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("基类：跳跃"));
        }
    }
}

void ABaseCharacter::StopJump()
{
    //如果还在上升过程中松开跳跃键，可以立即减小上升速度（短按小跳，长按高跳）
    if (CurrentVelocity.Z > 0)
    {
        CurrentVelocity.Z *= 0.5f; // 立即减半上升速度
    }
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Cyan, TEXT("基类：停止跳跃"));
    }
}



