// Fill out your copyright notice in the Description page of Project Settings.


#include "Hero_Main.h"
#include "GameFramework/SpringArmComponent.h"
#include "Camera/CameraComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Math/Vector.h"
#include "GameFramework/Controller.h"
#include "DrawDebugHelpers.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Components/BoxComponent.h"

// Sets default values
AHero_Main::AHero_Main()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    
    
    // --- 开始拼装主角肉体，基础四件套 ---
    
    //3.制造弹簧臂（类似于自拍杆），挂载到胶囊体上
    SpringArmComp = CreateDefaultSubobject<USpringArmComponent>(TEXT("HeroSpringArm"));
    SpringArmComp->SetupAttachment(RootComponent);
    SpringArmComp->TargetArmLength = 400.0f;
    SpringArmComp->bUsePawnControlRotation = true; // 关键机制：允许玩家用鼠标/右摇杆转动自拍杆
    SpringArmComp->bInheritPitch = true;
    SpringArmComp->bInheritYaw = true;
    SpringArmComp->bInheritRoll = false;
    SpringArmComp->bEnableCameraLag = true;
    SpringArmComp->CameraLagSpeed = 10.0f;
    
    // 4.制造摄像机，挂载到弹簧臂的末端（镜头端）
    CameraComp = CreateDefaultSubobject<UCameraComponent>(TEXT("HeroCamera"));
    CameraComp->SetupAttachment(SpringArmComp, USpringArmComponent::SocketName);
    CameraComp->bUsePawnControlRotation = false;
    
    // --- 初始化默认参数值 （与头文件声明保持一致）---
    ParkourParams = FParkourParams();
    MovementParams = FBaseMovementParams();
    CombatParams =FCombatParams();
    HeroAttributes = FHeroAttributes();
    HeroPhysicsParams = FHeroPhysicsParams();
    CollisionParams = FCollisionResolutionParams();
    
    // --- 初始化状态变量 ---
    CurrentHeroState = EHeroState::Stealth;
    CurrentSpeedState = ESpeedState::Jogging;
    CurrentTargetingState = ETargetingState::FreeCamera;
    

}

// Called when the game starts or when spawned
void AHero_Main::BeginPlay()
{
	Super::BeginPlay();
    
    //尝试获取本地玩家控制器并绑定增强输入映射
    if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
    {
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
        {
            //清楚可能存在的旧映射，添加我们定义的默认映射
            if (DefaultMappingContext)
            {
                Subsystem->AddMappingContext(DefaultMappingContext, 0);
            }
        }
    }
}

// Called every frame
void AHero_Main::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
    //-执行物理与重力推演-
    ApplyHeroPhysics(DeltaTime);
    
    //-如果在地面跑动，时刻侦测前方是否有墙壁可以攀爬-
    if (CurrentMoveState == EMoveState::Grounded)
    {
        CheckParkourFront();
    }
    
    UpdateSpeedState();
    CheckGroundStatus();
    
    // 此处可以放置每帧的状态更新逻辑，例如：
    // - 根据速度更新CurrentSpeedState (Walking, Jogging, Sprinting)
    // - 检测是否落地以更新CurrentMoveState (Grounded, InAir)
    // - 处理滑翔、攀爬等特殊移动的持续效果
    // - 更新角色属性（如耐力恢复、架势恢复等）

}

// Called to bind functionality to input
void AHero_Main::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
    
    //尝试把基础输入组件“升级”成增强输入组件
    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {  //绑定移动动作：当触发（Triggered)时，调用主角自己的Moveinput函数
        if (MoveAction)
        {
            EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered,this, &AHero_Main::MoveInput);
        }
        
        //绑定视角动作：当触发（Triggered)时，调用主角自己的Look函数
        if (LookAction)
        {
            EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered,this, &AHero_Main::Look);
        }
        
        //状态切换类动作 （按下开始，松开结束）
        if (JumpAction)
        {
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started,this, &AHero_Main::JumpStart);
            EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed,this, &AHero_Main::JumpEnd);
        }
        
        if (SprintAction)
        {
            EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started,this, &AHero_Main::StartSprint);
            EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed,this, &AHero_Main::StopSprint);
        }
        
        if (CrouchAction)
        {
            EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Started,this, &AHero_Main::StartCrouch);
            EnhancedInputComponent->BindAction(CrouchAction, ETriggerEvent::Completed,this, &AHero_Main::StopCrouch);
        }
        
        
        //单次执行类动作（攻击，弹反，闪避）
        if (LightAttackAction)
        {
            EnhancedInputComponent->BindAction(LightAttackAction, ETriggerEvent::Started,this, &AHero_Main::PerformLightAttack);
        }
        
        if (HeavyAttackAction)
        {
            EnhancedInputComponent->BindAction(HeavyAttackAction, ETriggerEvent::Started,this, &AHero_Main::PerformHeavyAttack);
        }
        
        if (ParryAction)
        {
            EnhancedInputComponent->BindAction(ParryAction, ETriggerEvent::Started,this, &AHero_Main::PerformParry);
        }
        
        if (DodgeAction)
        {
            EnhancedInputComponent->BindAction(DodgeAction, ETriggerEvent::Started,this, &AHero_Main::PerformDodge);
        }
    
        if (LockOnAction)
        {
            EnhancedInputComponent->BindAction(LockOnAction, ETriggerEvent::Started,this, &AHero_Main::ToggleLockOn);
        }
        
    }
    else
    {
        //如果无法使用增强输入，输出警告
        UE_LOG(LogTemp,Warning, TEXT("无法转换为EnhancedInputComponent，请确保启用EnhancedInput插件"));
    }

}

// ==================================================
// --- 输入函数实体 （屏幕测试版） ---
// ==================================================

void AHero_Main::MoveInput(const FInputActionValue& Value)
{
    //获取摇杆/WASD传进来的二维向量（X和Y)
    FVector2D MovementVector = Value.Get<FVector2D>();
    
    //保存输入值用于其他计算
    CurrentMovementInput = MovementVector;
    
    //TODO:实际移动逻辑
    //获取控制器的向前和向右向量 （忽略俯仰）
    if (Controller != nullptr)
    {
        FRotator Rotation = Controller->GetControlRotation();
        FRotator YawRotation(0, Rotation.Yaw, 0);
        
        //获取前进方向
        const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
        const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
        
        //根据输入向量计算移动方向
        MoveDirection = (ForwardDirection * MovementVector.Y) + (RightDirection * MovementVector.X);
        MoveDirection.Normalize();
        
        
        //调用基类的移动函数
        Super::Move(MoveDirection, 1.0f);
        
        //更新水平速度（用于动画等）
        HorizontalSpeed = MoveDirection.Size() * GetCurrentSpeed();
    }
}



void AHero_Main::Look(const FInputActionValue& Value)
{
    //获取右摇杆/鼠标传进来的二维向量（X和Y)
    FVector2D LookAxisVector = Value.Get<FVector2D>();
    
    if (Controller != nullptr)
    {
        AddControllerYawInput(LookAxisVector.X);
        AddControllerPitchInput(LookAxisVector.Y);
    }
}


// JUMP
void AHero_Main::JumpStart(const FInputActionValue& Value)
{
    //调用基类的跳跃函数
    Super::Jump();
    
    //在此添加主角特有的跳跃逻辑
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("起跳！"));
    }
}

void AHero_Main::JumpEnd(const FInputActionValue& Value)
        
{
    //调用基类的跳跃函数
    Super::StopJump();
}
    
// SPRINT
void AHero_Main::StartSprint(const FInputActionValue& Value)
{
    //切换到冲刺状态
    SetSpeedState(ESpeedState::Sprinting);
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("开始冲刺"));
    }
}

void AHero_Main::StopSprint(const FInputActionValue& Value)
{
    //切换回慢跑状态
    SetSpeedState(ESpeedState::Jogging);
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("结束冲刺"));
    }
}

//CROUCH
void AHero_Main::StartCrouch(const FInputActionValue& Value)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, TEXT("下蹲"));
    }
}

void AHero_Main::StopCrouch(const FInputActionValue& Value)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Blue, TEXT("站起来"));
    }
}

//主角攻击
void AHero_Main::PerformLightAttack(const FInputActionValue& Value)
{
    Super::StartAttack();
    
    if (CurrentCharacterState != ECharacterState::Combat)
    {
        SetCharacterState(ECharacterState::Combat);
    }
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("主角轻攻击"));
    }
}

void AHero_Main::PerformHeavyAttack(const FInputActionValue& Value)
{
    //重攻击逻辑（可扩展）
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("主角重攻击"));
    }
}

void AHero_Main::PerformParry(const FInputActionValue& Value)
{
    //调用基类的开始防御函数
    Super::StartDefend();
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("主角完美格挡"));
    }
}

void AHero_Main::PerformDodge(const FInputActionValue& Value)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("主角闪避"));
    }
    
    FVector DodgeDirection;
    if (MoveDirection.SizeSquared() > 0.1f)
    {
        DodgeDirection = MoveDirection;
    }
    else
    {
        DodgeDirection = GetActorForwardVector();
    }
    
    CurrentVelocity = DodgeDirection * CombatParams.DodgeDistance;
}

void AHero_Main::ToggleLockOn(const FInputActionValue& Value)
{
    if (CurrentTargetingState == ETargetingState::FreeCamera)
    {
        SetTargetingState(ETargetingState::LockedOn);
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, TEXT("切换视角锁定"));
        }
    }
    else
    {
        CurrentTargetingState = ETargetingState::FreeCamera;
        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, TEXT("取消锁定"));
        }
    }
}


// ==================================================
// --- 主角特有输入函数实体 （屏幕测试版） ---
// ==================================================

void AHero_Main::SetHeroState(EHeroState NewState)
{
    // 以后可以在这里统一处理状态切换逻辑
    if (CurrentHeroState != NewState)
    {
        CurrentHeroState = NewState;
        
        if (GEngine)
        {
            FString StateName = UEnum::GetValueAsString(NewState);
            GEngine->AddOnScreenDebugMessage(-1,3.0f,FColor::White, FString::Printf(TEXT("英雄状态改变: %s"),*StateName));
        }
        
        //状态特定逻辑
        switch (NewState)
        {
            case EHeroState::Stealth:
                //潜行逻辑
                break;
            case EHeroState::LockedOn:
                //锁定目标逻辑
                break;
        }
    }
    //TODO:触发状态改变事件/广播，用于动画蓝图更新或者UI更新
    //比如 if (NewState == EHeroState::Combat) { 拔刀(); }
}

void AHero_Main::SetSpeedState(ESpeedState NewState)
{
    // 以后可以在这里统一处理状态切换逻辑
    if (CurrentSpeedState !=NewState)
    {
        CurrentSpeedState = NewState;
        
        switch (NewState)
        {
            case ESpeedState::Walking:
                MovementParams.MaxWalkSpeed = 200.0f; // 行走速度
                break;
            case ESpeedState::Jogging:
                MovementParams.MaxWalkSpeed = 500.0f; //慢跑速度
                break;
            case ESpeedState::Sprinting:
                MovementParams.MaxWalkSpeed = HeroMovementParams.MaxSprintSpeed; //冲刺速度
                break;
        }
        
        if (GEngine)
        {
            FString StateName = UEnum::GetValueAsString(NewState);
            GEngine->AddOnScreenDebugMessage(-1,3.0f, FColor::White, FString::Printf(TEXT("移动状态改变: %s"), *StateName));
        }
    }
    //TODO:触发状态改变事件/广播
    // 以后可以在这里统一切换移动速度， 比如 if (NewState == EMoveState::Sprinting) { Speed = MovementParams, MaxSprintSpeed;}
}

void AHero_Main::SetTargetingState(ETargetingState NewState)
{
    if (CurrentTargetingState != NewState)
    {
        CurrentTargetingState = NewState;
    }
}

void AHero_Main::SetAdvancedMoveMode(EAdvancedMoveState NewMode)
{
    if (CurrentAdvancedMoveMode != NewMode)
    {
        CurrentAdvancedMoveMode = NewMode;
    }
}




// ==================================================
// --- 核心手写物理推演 ---
// ==================================================

void AHero_Main::ApplyHeroPhysics(float DeltaTime)
{
    //首先调用基类的物理逻辑
    //高级移动模式的特殊处理
    /*
     
     switch (CurrentAdvanceMoveMode)
     {
     case EAdvanceMoveState::Gliding:
     //滑翔物理：水平速度保持，垂直速度受限
     if (CurrentVelocity.Z < HeroPhysicParams.GlideDescentRate)
     {
     CurrentVelocity.Z = HeroPhysicsParams.GlideDescentRate;
     }
     break;
     
     case EAdvancedMoveState::Swinging:
     //摆荡物理
     //摆荡圆周运动物理
     break;
     
     }
     */
    
    //冲刺状态的特殊处理
    if (CurrentSpeedState == ESpeedState::Sprinting && IsGrounded())
    {
        //冲刺时增加加速度
        MovementParams.Acceleration = 4096.0f;
    }
    else
    {
        //恢复默认加速度
        MovementParams.Acceleration = 2048.0f;
    }
    
 }
        
        
// ==================================================
// --- 跑酷：前方的球形射线检测 ---
// ==================================================

void AHero_Main::CheckParkourFront()
{
    FVector StartLoc = GetActorLocation();
    FVector EndLoc = StartLoc + GetActorForwardVector() * ParkourParams.ForwardTraceDistance;
    
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);
    
    
    //发射一个球体，检测前房是否有墙壁（WorldStatic 类型的物体）
    bool bHit = UKismetSystemLibrary::SphereTraceSingle(
                                                        GetWorld(),StartLoc, EndLoc, ParkourParams.TraceRadius,UEngineTypes::ConvertToTraceType(ECC_WorldStatic), false, TArray<AActor*>(),EDrawDebugTrace::ForOneFrame, HitResult, true
                                                        );
    
    if (bHit)
    {
        FVector ImpactPoint = HitResult.ImpactPoint;
        FVector CharacterLocation = GetActorLocation();
        float HeightDifference = ImpactPoint.Z - CharacterLocation.Z;
        
        if (HeightDifference <= ParkourParams.MaxLedgeHeight && HeightDifference >= ParkourParams.MinVaultHeight)
        {
            //撞到墙了！这里以后写攀爬上墙的逻辑：SetHeroState(EHeroState::Climbing);
            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, TEXT("侦测到前方有墙壁，可以攀爬！"));
            }
        }
    }
}


void AHero_Main::UpdateSpeedState()
{
    //获取当前水平速度
    float Speed = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f).Size();
    
    //根据速度确定状态
    if (CurrentSpeedState != ESpeedState::Sprinting) // 冲刺状态由输入控制，不自动切换
    {
        if (Speed < 100.0f)
        {
            SetSpeedState(ESpeedState::Walking);
        }
        else
        {
            SetSpeedState(ESpeedState::Jogging);
        }
    }
}

void AHero_Main::CheckHeroGroundStatus()
{
    if (CurrentMoveState == EMoveState::InAir && CurrentVelocity.Z <= 0)
    {
        
        Super::CheckGroundStatus();
        
        
        FVector Start = GetActorLocation();
        FVector End = Start - FVector(0, 0, 100.0f);
        
        
        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);
        
        
        bool bHit = (GetWorld()->LineTraceSingleByChannel(
                                                          HitResult,
                                                          Start,
                                                          End,
                                                          ECC_WorldStatic,
                                                          QueryParams
                                                          )
                     );
        {
            float GroundAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(HitResult.Normal, FVector::UpVector)));
            
            if (GroundAngle > CollisionParams.WalkableFloorAngle)
            {
                //陡坡处理逻辑
                //比如 应用向下推力防止卡住
            }
        }
    }
}

//辅助函数：获取当前速度
float AHero_Main::GetCurrentSpeed() const
{
    switch(CurrentSpeedState)
    {
        case ESpeedState::Walking: return 200.0f;
        case ESpeedState::Jogging: return 500.0f;
        case ESpeedState::Sprinting: return HeroMovementParams.MaxSprintSpeed;
        default: return 500.0f;
    }
}

