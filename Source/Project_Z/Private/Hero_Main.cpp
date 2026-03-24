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
#include "Components/BoxComponent.h" //武器碰撞体
#include "Components/CableComponent.h" //绳索组件
#include "Particles/ParticlesSystemComponent.h" //粒子系统
#include "Kismet/GameplayStatic.h" //游戏工具
#include "TimerManager.h" //定时器



// Sets default values
AHero_Main::AHero_Main()
{
 	// Set this pawn to call Tick() every frame.  You can turn this off to improve performance if you don't need it.

    PrimaryActorTick.bCanEverTick = true;

    
    
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
    
    // 5.武器碰撞体组件
    WeaponCollisionComp = CreateDefaultSubobject<UBoxComponent>(TEXT("WeaponCollision"));
    WeaponCollisionComp->SetupAttachment(MeshComp, TEXT(hand_r)); //附加到右手骨骼
    WeaponCollisionComp->SetBoxExtent(FVector(5.0f, 5.0f, 50.0f));
    WeaponCollisionComp->SetCollisionProfileName(TEXT("NoCollision")); //初始无碰撞
    WeaponCollisionComp->SetHiddenInGame(true); //游戏中隐藏
    WeaponCollisionComp->SetGenerateOverlapEvents(false); //初始不生成重叠事件

    //6.滑翔特效组件
    GlideParticleComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("GlideParticle"));
    GlideParticleComp->SetupAttachment(RootComponent);
    GlideParticleComp->bAutoActive = false;

    //7.摆荡绳索组件
    RopeCableComp = CreateDefaultSubobject<UCableComponent>(TEXT("RopeCable"));
    RopeCableComp->SetupAttachment(RootComponent);
    RopeCableComp->SetHiddenInGame(true);
    RopeCableComp->CableLength = 1000.0f;
    RopeCableComp->NumSegments = 10;
    RopeCableComp->CableWith = 5.0f;


    // --- 初始化默认参数值 （与头文件声明保持一致）---
    ParkourParams = FParkourParams();
    MovementParams = FBaseMovementParams();
    CombatParams =FCombatParams();
    HeroAttributes = FHeroAttributes();
    HeroPhysicsParams = FHeroPhysicsParams();
    CollisionParams = FCollisionResolutionParams();

    GlideParams = FGlideParams();
    SwingParams = FSwingParams();
    ClimbParams = FClimbParams();
    WeaponAttackParams = FWeaponAttackParams();
    
    // --- 初始化状态变量 ---
    CurrentHeroState = EHeroState::Stealth;
    CurrentSpeedState = ESpeedState::Jogging;
    CurrentTargetingState = ETargetingState::FreeCamera;
    
    CurrentAdvancedMoveMode = EAdvancedMoveState::None;
    CurrentAttackComboState = EAttackComboState::Ready;
    CurrentWeaponType = EWeaponType::Sword;

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
    //绑定武器碰撞体重叠事件
    // ====================

    if (WeaponCollision)
    {
        WeaponCollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AHero_Main::OnWeaponOverlapBegin);
    }

    //初始化绳索长度
    RopeLength = SwingParams.RopeLength;

    //调试信息
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Green, TEXT("英雄角色初始化完成-增强版本"))
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

    RecoverStaminaOverTime(DeltaTime)
    RecoverPostureOverTime(DeltaTime)

    //更新连段计时器
    if (CurrentAttackComboState != EAttackComboState::Ready)
    {
        float CurrentTime = GetWorld()->GetTimeSeconds();
        if (CurrentTime - LastAttackTime > WeaponAttackParams.ComboWindow)
        {
            ResetAttackCombo();
        }
    }

    //更新弹反窗口计时器
    if (bIsInParryWindow)
    {
        ParryWindowTimer += DeltaTime;
        if (ParryWindowTimer >= CombatParams.ParryWindowSeconds)
        {
            bIsInParryWindow = false;
            ParryWindowTimer = 0,0F
        }
    }

    //更新蓄力计时器
    if (CurrentAttackComboState == EAttackComboState::HeavyCharge)
    {
        HeavyCharge +=DeltaTime;
    }

    //更新滑翔特效
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Gliding && GlideParticleComp)
    {
        if (!GlideParticleComp->IsActive())
        {
            GlideParticleComp->IsActive();
        }
    }
    else if (GlideParticleComp && GlideParticleComp->IsActive())
    {
        GlideParticleComp->Deactivate();
    }

    //更新摆荡绳索
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Swinging && RopeCableComp)
    {
        RopeCableComp->SetHiddenInGame(false);
        RopeCableComp->SetAttachEndTo(this, Name_None, TEXT("spine_03")); //附加到脊椎
        RopeCableComp->EndLocation = SwingAnchorPoint - GetActorLocation();
    }
    else if (RopeCableComp && !RopeCableComp->bHiddenInGame)
    {
        RopeCableComp->SetHiddenInGame(true)
    }

    if (bShowDebugInfo)
    {
        DrawDebuginfo();
    }

    
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


// ==================================================
// --- 战斗系统 ---
// ==================================================


void AHero_Main::StartAttack()
{
    //调用基类的StartAttack
    Super:StartAttack();

    //切换到战斗状态
    if (CurrentCharacterState != ECharacterState::Combat)
    {
        SetCharacterState(ECharacterState::Combat);
    }

    //激活武器碰撞体
    ActivateWeaponCollision();

    //更新连段状态
    UpdateAttackCombo(flase);

    //记录攻击时间
    LastAttackTime =GetWorld()->GetTimeSeconds();

    //清空已命中列表
    AlreadyHitActors.Empty();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor:Red,
            FString::Printf(TEXT("开始攻击-连段：%s"),
            *UEnum::GetValueAsString(CurrentAttackComboState)));
    }

}



void AHero_Main::PerformLightAttack(const FInputActionValue& Value)
{
    Super::StartAttack();
    
    StartAttack();
    
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("主角轻攻击"));
    }
}

void AHero_Main::PerformHeavyAttack(const FInputActionValue& Value)
{
    StartHeavyCharge();
    //开始蓄力

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("主角重攻击"));
    }
}

void AHero_Main::StartHeavyCharge()
{
    if (HasEnoughStamina(30.0F))
    {
        //设置蓄力状态
        CurrentAttackComboState = EAttackComboState::HeavyCharge;

        //重置蓄力时间
        HeavyChargeTime = 0.0f;

        //触发蓝图事件
        OnAttackComboChanged(CurrentAttackComboState);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Red, TEXT("开始主角蓄力重攻击"));
        }
    }
}

void AHero_Main::ReleaseHeavyAttack()
{
    if (CurrentAttackComboState == EAttackComboState::HeavyCharge)
    {
        //切换到释放状态
        CurrentAttackComboState = EAttackComboState::HeavyRelease;

        //触发蓝图事件
        OnAttackComboChanged(CurrentAttackComboState):

        //激活武器碰撞体
        ActivateWeaponCollision();

        //记录攻击时间
        LastAttackTime = GetWorld()->GetTimeSeconds();

        //清空已命中列表
        AlreadyHitActors.Empty();
        
        //消耗耐力
        ConsumeStamina(30.0f)

        if (GEngine0
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor:Red,
                    FString::Printf(TEXT("释放重攻击-蓄力时间：%.2f秒"), HeavyChargeTime));
            }

    }
}

void AHero_Main::ActivateWeaponCollision()
{
    if (WeaponCollision)
    {
        WeaponCollisionComp->SetCollisionProfileName(TEXT("Weapon"));
        WeaponCollisionComp->SetGenerateOverlapEvents(true);

        //设置定时器，0.3秒后自动停用
        FTimerHandle TimeHandle;
        GetWorld()->GetTimeManager().SetTimer(TimerHandle, [this]()
        {
            DeactivateWeaponCollision();
        }

    } 

}


void AHero_Main::DeactivateWeaponCollision()
{
    if (WeaponCollision)
    {
        WeaponCollisionComp->SetCollisionProfileName(TEXT("NoCollision"));
        WeaponCollisionComp->SetGenerateOverlapEvents(false);
    }

}

void AHero_Main::OnWeaponOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)

    {
        //忽略自己
        if (OtherActor == this)
        {
            return;
        }
        //检查是否已经命中过这个目标
        if (AlreadyHitActors.Contains(OtherActor))
        {
            return
        }

        //尝试转换为BaseCharacter
        ABaseCharacter* HitCharacter = Cast<ABaseCharacter>(OtherActor);
        if (HitCharacter && HitCharacter->IsAlive())
        {
            //计算伤害
            float Damage = CalculateAttackDamage(CurrentAttackComboState == EAttackComboState::HeavyRelease);

            //应用伤害
            ApplyAttackDamage(HitCharacter, Damage, CurrentAttackComboState == EAttackComboState::HeavyRelease);

            //添加到已命中列表
            AlreadyHitActors.Add(OtherActor);

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red,
                    FString::Printf(TEXT("命中敌人! 伤害: %.1f"), Damage));
            }
        }

    }

float AHero_Main::CalculateAttackDamage(bool bIsHeavyAttack) const
{
    float BaseDamage = bIsHeavyAttack ?
    WeaponAttackParams.HeavyAttackDamage :
    WeaponAttackParams.LightAttackDamage;

    //蓄力加成
    if (bIsHeavyAttack)
    {
        float ChargeMultiplier = 1.0f + FMath::Min(HeavyChargeTime, 3.0f) / 3.0f; //最大2倍
        BaseDamage *= ChargeMultiplier;
    }

    //连段加成
    float ComboMultiplier = 1.0f;
    switch (CurrentAttackComboState)
    {
        case EAttackComboState::Light2:
        ComboMultiplier = 1.2f;
        break;

        case EAttackComboState::Light3:
        ComboMultiplier = 1.5f;
        break;

        case EAttackComboState::Finisher:
        ComboMultiplier = 2.0f;
        break;
    }

    return BaseDamage * ComboMultiplier;

}

void AHero_Main::ApplyAttackDamage(ABaseCharacter* Target, float Damage, bool bIsHeavyAttack)
{
    if (Target && Target->IsAlive())
    {
        //调用目标的ApplyDamage函数
        Target->ApplyDamage(Damage);

        //触发蓝图事件
        OnSuccessfulHit(Target, Damage);

        //架势伤害（重攻击有额外架势伤害）
        if (bIsHeavyAttack)
        {
            float PostureDamage = Damage * CombatParams.PostureDamageMultiplier;
            //TODO:应用架势伤害到目标
        }

    }
}

void AHero_Main::UpdateAttackCombo(bool bIsHeavyAttack)
{
    if (bIsHeavyAttack)
    {
        CurrentAttackComboState = EAttackComboState::HeavyRelease;
    }
    else
    {
        //轻攻击连段
        switch (CurrentAttackComboState）
            {
                case EAttackComboState::Ready:
                CurrentAttackComboState = EAttackComboState::Light1;
                break; 

                case EAttackComboState::Light1:
                CurrentAttackComboState = EAttackComboState::Light2;
                break; 

                case EAttackComboState::Light2:
                CurrentAttackComboState = EAttackComboState::Light3;
                break; 

                case EAttackComboState::Light3:
                CurrentAttackComboState = EAttackComboState::Finisher;
                break; 

                default
                CurrentAttackComboState = EAttackComboState::Light1;
                break; 
            }
    }

    //触发蓝图事件
    OnAttackComboChanged(CurrentAttackComboState);

}

void AHero_Main::ResetAttackCombo()
{
    CurrentAttackComboState = EAttackComboState::Ready;
    HeavyChargeTime = 0.0f;

    //触发蓝图事件
    OnAttackComboChanged(CurrentAttackComboState);

     if(GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Yellow, TEXT("连段重置"));
    }
}

void AHero_Main::PerformParry(const FInputActionValue& Value)
{
    //调用基类的开始防御函数
    Super::StartDefend();
    
    //激活弹反窗口
    bIsInParryWindow = true;
    ParryWindowTimer = 0.0f;

    //处理弹反逻辑
    HandleParryWindow();


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

void AHero_Main::HandleParryWindow()
{
    // 在弹反窗口期间检测是否有敌人攻击
    // 这里可以扩展为检测特定范围内的敌人攻击
    // 暂时只设置状态，具体检测逻辑可以在Tick中实现

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 0.5f, FColor::Yellow, 
            FString::Printf(TEXT("弹反窗口: %.2f秒"), CombatParams.ParryWindowSeconds));
    }
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
// --- 耐力与属性系统 ---
// ==================================================

void AHero_Main::ConsumeStamina(float Amount)
{
    if (Amount > 0.0f)
    {
        Attributes.CurrentStamina = FMath::Math(0.0F, Attributes.CurrentStamina - Amount);

        //重置耐力恢复计时器
        StaminaRecoveryTimer = 0.0f;
       
        //耐力耗尽时的处理
        if (Attributes.CurrentStamina <= 0.0f)
        {
            //停止冲刺
            if (CurrentSpeedState == ESpeedState::Sprinting)
            {
                StopSprint(FInputActionValue());
            }

            //停止滑翔
            if (CurrentAdvancedMoveMode == EAdvancedMoveState::Gliding)
            {
                StopGliding();
            }

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("耐力耗尽!"));
            }
        }
    }
}

void AHero_Main::RecoveryStamina(float Amount)
{
    if (Amount > 0.0f)
    {
        Attributes.CurrentStamina = FMath::Min(Attributes.MaxStamina, Attributes.CurrentStamina + Amount);
    }
}

void AHero_Main::RecoverPosture(float Amount)
{
    if (Amount > 0.0f)
    {
        HeroAttributes.CurrentPosture = FMath::Min(HeroAttributes.MaxPosture, HeroAttributes.CurrentPosture + Amount);
    }
}


void AHero_Main::RecoverPostureOverTime(float DeltaTime)
{
    //只有在非战斗状态且一段时间没有消耗耐力才恢复
    if (CurrentCharacterState != ECharacterState::Combat &&
    CurrentSpeedState != ESpeedState::Sprinting &&
    CurrentAdvancedMoveMode != EAdvancedMoveState::Gliding &&
    CurrentAdvancedMoveMode != EAdvancedMoveState::Swinging &&
    CurrentAdvancedMoveMode != EAdvancedMoveState::Climbing)
    {
        StaminaRecoveryTimer += DeltaTime;

        if (StaminaRecoveryTimer >= StaminaRecoveryDelay)
        {
            //每秒恢复10点耐力
            float RecoveryAmount = 10.0f * DeltaTime;
            RecoveryStamina(RecoveryAmount);
        }
    }
    else
    {
        //在消耗耐力的状态下，重置计时器
        StaminaRecoveryTimer = 0.0f;
    }
}


void AHero_Main::RecoverPostureOverTime(float DeltaTime)
{
    //只有在非战斗状态时恢复架势
    if (CurrentCharacterState != ECharacterState::Combat)
    {
        //每秒恢复5点架势
        float RecoveryAmount = 5.0f * DeltaTime;
        RecoverPosture(RecoveryAmount);
    }
}

bool AHero_Main::HasEnoughStamina(float RequiredAmount)const
{
    return Attributes.CurrentStamina >= RequiredAmount;
}


// ==================================================
// --- 调试与可视化 ---
// ==================================================

void AHero_Main::DrawDebuginfo()
{
    if (!GetWorld())
    {
        return;
    }

    FVector ActorLocation = GetActorLocation();\
    

    //绘制状态信息
    FString StateInfo = FString::Printf(TEXT(
        "状态：%\n"
        "移动模式：%\n"
        "速度：%.1f\n"
        "耐力：%.1f/%.1f\n"
        "架势：%.1f/%.1f\n"
        "生命：%.1f/%.1f\n"
        "连段: %s"
    ),
    *UEnum::GetValueAsString(CurrentCharacterState),
    *UEnum::GetValueAsString(CurrentAdvancedMoveMode),
    CurrentSpeedMagnitude,
    Attributes.CurrentStamina, Attributes.MaxStamina,
    HeroAttributes.CurrentPosture, HeroAttributes.MaxPosture,
    Attributes.CurrentHealth, Attributes.MaxHealth,
    *UEnum::GetValueAsString(CurrentAttackComboState)
);

GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::White, StateInfo);

//绘制速度向量
DrawDebugDirectionlArrow(GetWorld(), ActorLocation,
ActorLocation + CurrentVelocity.GetSafeNormal() * 100.0f,
50.0f, FColor::Cyan, false, 0.0f, 0, 2.0f);

//绘制武器碰撞体范围
if (WeaponCollisionComp && WeaponCollisionComp->GetGenerateOverlapEvents())
{
    FVector WeaponLoc = WeaponCollisionComp->GetComponentLocation();
    float Radius = WeaponCollisionComp->GetScaledBoxExtent().X;
    DrawDebugSphere(GetWorld(), WeaponLoc, Radius, 12, FColor::Red, flase, 0.0f, 0, 1.0f);
}


//绘制摆荡信息
if (CurrentAdvancedMoveMode == EAdvancedMoveState::Swinging)
{
    DrawDebugLine(GetWorld(), ActorLocation, SwingAnchorPoint, 
            FColor::Magenta, false, 0.0f, 0, 2.0f);

    DrawDebugSphere(GetWorld(), SwingAnchorPoint, 20.0f, 8, 
            FColor::Magenta, false, 0.0f, 0, 2.0f);
}


//绘制攀爬信息
if (CurrentAdvancedMoveMode == EAdvancedMoveState::Climbing)
{
    DrawDebugLine(GetWorld(), ActorLocation, CurrentClimbLocation, 
            FColor::Green, false, 0.0f, 0, 2.0f);
        
     DrawDebugSphere(GetWorld(), CurrentClimbLocation, 20.0f, 8, 
            FColor::Green, false, 0.0f, 0, 2.0f);
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
    Super::ApplyHeroPhysics(DeltaTime);

    //高级移动模式的特殊物理处理
    //================================================

    switch (CurrentAdvancedMoveMode)
    {
        case EAdvancedMoveState::Gliding:
        //滑翔物理：水平速度保持，垂直速度受限
        ApplyHeroPhysics(DeltaTime);
        break;

        case EAdvancedMoveState::Swinging:
        //摆荡物理，钟摆运动
        ApplyHeroPhysics(DeltaTime);
        break;

        case EAdvancedMoveState::Climbing:
        //攀爬物理：在墙壁上移动
        ApplyHeroPhysics(DeltaTime)
        break;

        case EAdvancedMoveState::WallRunning:
        //蹬墙跑物理：沿墙壁水平运动
        //TODO:蹬墙跑物理
        break;

        case EAdvancedMoveState::Sliding:
        //滑墙物理：地面滑动
        //TODO:实现滑铲物理
        break;

        default
        //普通移动物理处理
        break;
    }

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

    //滑翔自动检测
    if (CurrentMoveState == EMoveState::InAir &&
        CurrentAdvanceMoveMode == EAdvancedMoveState:: &&
        CanStartGliding())
        {
            //自动开始滑翔
            startGliding();
        }
    
 }


// ==================================================
// --- 新增滑翔物理计算 ---
// ==================================================

void AHero_Main::ApplyGlidePhysics(float DeltaTime)
{
    //保持水平速度
    FVector HorizontalVelocity = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f);

    //应用滑翔转向
    if (FMath::Abs(CurrentGlideTurn) > 0.1)
    {
        FRotator TurnRotation(0.0f, CurrentGlideTurn * GlideParams. TurnRate * DeltaTime, 0.0f);
        HorizontalVelocity = TurnRotation.RotateVector(HorizontalVelocity);
    }

    //限制水平速度
    float HorizontalSpeed = HorizontalVelocity.Size();
    if (HorizontalSpeed > GlideParams.MaxGlideSpeed)
    {
        HorizontalVelocity = HorizontalVelocity.GetSafeNormal() * GlideParams.MaxGlideSpeed;
    }

    //应用升力（抵消部分重力）
    float CurrentDescentRate = HeroPhysicsParam.GlideDescentRate;
    CurrentDescentRate += GlideParams.GlideliftForce * (HorizontalSpeed / GlideParams.MaxGlideSpeed);

    //应用空气阻力
    HorizontalVelocity *= (1.0f -GlideParams.GlideDragCofficient * DeltaTime);

    //更新速度
    CurrentVelocity.X = HorizontalVelocity.X;
    CurrentVelocity.Y = HorizontalVelocity.Y;
    CurrentVelocity.Z = CurrentDescentRate;

    //更新计时器
    GldieTimer += DeltaTime;

    //滑翔消耗耐力
    ConsumeStamina(5.0f * DeltaTime);

    //耐力耗尽时停止滑翔
    if (Attributes.CurrentStamina <= 0.0f)
    {
        StopGliding();
    }

}

// =================================================
// --- 摆荡物理计算 ---
// =================================================

void AHero_Main::ApplySwingPhysics(float DeltaTime)
{
    //计算绳索方向
    FVector RopeDirection = SwingAnchorPoint - GetActorLocation();
    float CurrentRopeLength = RopeDirection.size();

    //规范化方向
    RopeDirection.Normalize();

    //计算摆荡角度（相对于垂直向下方向）
    FVector DownVector = FVector(0.0f, 0.0f, -1,0f)
    CurrentSwingAngle = FMath::Acos(FVector::DotProduct(RopeDirection, DownVector));
    CurrentSwingAngle = FMath::RadiansToDegrees(CurrentSwingAngle);

    //限制最大的摆动角度
    if (CurrentSwingAngle > SwingParams.MaxSwingAngle)
    {
        CurrentSwingAngle = SwingParams.MaxSwingAngle;
    }

    //计算最大摆荡角度
    if (CurrentSwingAngle > SwingParams.MaxSwingAngle)
    {
        CurrentSwingAngle = SwingParams.MaxSwingAngle;
    }

    //计算摆荡物理（简化的单摆运动)
    float GravityAcceleration = 980.0f * PhysicsParams.GravityScale * SwingParams.SwingGravityScale;
    float AngularAcceleration = -(GravityAccelerationv / RopeLength) * FMath::sin(FMath::DegreesToRadians(CurrentSwingAngle));

    //角速度
    SwingAngularVelocity += AngularAcceleration * DeltaTime;

    //应用阻尼
    SwingAngularVelocity *= SwingParams.DampingFactor;

    //摆动角度
    CurrentSwingAngle += SwingAngularVelocity * DeltaTime;

    //计算切线角度方向
    FVector TagentDirection = FVector::CrossProduct(RopeDirection, FVector::UpVector);
    TangentDirection.Normalize();

    //计算线速度
    float LinearSpeed = SwingAngularVelocity * RopeLength;
    SwingVelocity = TangentDirection * LinearSpeed;

    //应用速度到角色
    CurrentVelocity.X = SwingVelocity.X;
    CurrentVelocity.Y = SwingVelocity.Y;
    
    //应用向下重力（保持绳索紧绷）
    CurrentVelocity.Z = -GravityAcceleration * DeltaTime;

    //保持与锚点的距离
    if (CurrentRopeLength > RopeLength)
    {
        FVector Correction = RopeDirection * (CurrentRopeLength - RopeLength) * 10.0f;
        CurrentVelocity += Correction;
    }

    //摆荡消耗耐力
    ConsumeStamina(3.0f * DeltaTime);

}

// =================================================
// --- 攀爬物理计算 ---
// =================================================

void AHero_Main::ApplyClimbPhysics(float DeltaTime)
{
    //攀爬状态下，覆盖默认重力
    CurrentVelocity.Z = 0.0f;

    //计算攀爬移动的方向
    FVector ClimbMoveDirection = FVector::ZeroVector;

    if (ClimbInput.SizeSquared() > 0.1f)
    {
        //将2D输入转换为墙壁3D方向
        FVector ClimbMoveDirection = FVector::ZeroVector(ClimbWallNormal, FVector::UpVector);
        RightDirection.Normalize();

        ClimbMoveDirection = (RightDirection * ClimbInput.X) + (FVector::UpVector * Climbinput.Y);
        ClimbMoveDirection.Normalize();

        //应用攀爬速度
        CurrentVelocity = ClimbMoveDirection * ClimbParams.ClimbSpeed;

        //攀爬消耗耐力
        ConsumeStamina(ClimbParams.ClimbStaminaCost * DeltaTime);
    }
    else
    {
        CurrentVelocity = FVector::ZeroVector;
    }

    //更新攀爬计时器
    ClimbTimer += DeltaTime;

    //耐力耗尽的时候停止攀爬
    if (Attributes.CurrentStamina <= 0.0f)
    {
        StopClimbing();
    }
}

        
        
// ==================================================
// --- 跑酷：前方的球形射线检测 /与攀爬检测系统 ---
// ==================================================

void AHero_Main::CheckParkourFront()
{
    FVector StartLoc = GetActorLocation() + FVector(0,0,50.0F); //从腰部高度发射
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
            //计算墙壁法线
            FVector WallNormal = HitResult.Normal;

            //检查玩家是否面向墙壁
            float DotProduct = FVector::DotProduct(GetActorForwardVector(), -WallNormal);
            if (DotProduct > 0.7f)//角度在45度以内
            {
                StartClimbing(ImpactPoint, WallNormal);

                //撞到墙了！这里以后写攀爬上墙的逻辑：SetHeroState(EHeroState::Climbing);
                if (GEngine)
                {
                    GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, FString::Printf(TEXT("侦测到前方有墙壁，可以攀爬！"), HeightDifference));
                }
            }
        }
    }
}


void AHero_Main::StartClimbing(Const FVector& WallLocation, const FVector& WallNormal)
{
    //设置攀爬状态
    SetAdvancedMoveMode(EAdvancedMoveState:Climbing)

    //保存墙壁信息
    ClimbWallNormal = WallNormal;
    CurrentClimbLocation = WallLocation;

    //重置计时器
    ClimbTimer = 0.0f;
    bIsMantling = false;

    //禁用重力
    PhysicsParams.GravityScale = 0.0f;

    //设置角色位置和旋转
    FVector TargetLocation = WallLocation - (WallNormal * 50.0f); //距离墙壁50单位
    TargetLocation.Z = WallLocation.Z - 100.0f; //调整高度

    SetActorLocation(TargetLocation);

    //面向墙壁
    FRotator TargetRotation = (-WallNormal).Rotation(); 
    SetActorRotation(TargetRotation);

    //停止当前速度
    CurrentVelocity = FVector::ZeroVector;

    //触发蓝图事件
    OnStartClimbing();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("开始攀爬墙壁"))；
    }

}

void AHero_Main::StopClimbing = 1.0f;
{
    //恢复重力
    PhysicsParams.GravityScale = 1.0f;

    //退出攀爬状态
    SetAdvancedMoveMode(EAdvancedMoveState::None);

    //重置变量
    ClimbWallNormal = FVector::ZeroVector;
    CurrentClimbLocation = FVector::ZeroVector;
    ClimbInput = FVector2D::ZeroVector;

    //触发蓝图事件
    OnStopClimbing();

    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Green, TEXT("停止攀爬"));
    }
}

void AHero_Main::ClimbMoveInput(const FVector2D& Input)
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Climbing && !bIsMantling)
    {
        ClimbInput = Input;
    }
}

void AHero_Main::ClimbJump()
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Climbing)
    {
        //计算跳跃方向（远离墙壁）
        FVector JumpDirection = =ClimbWallNormal;
        JumpDirection.Z = 0.5f; // 向上成分

        JumpDirection.Normalize();

        //应用跳跃速度
        CurrentVelocity = JumpDirection * MovementParams.JumpVelocity;

        //停止攀爬
        StopClimbing();

        //设置为空中状态
        SetMoveState(EMoveState::InAir);

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("攀爬跳跃"));
        }

    }
}

void AHero_Main::MantleLedge()
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Climbing && !bIsMantling)
    {
        bIsMantling = true;

        //计算攀上的位置
        FVector MantleLocation == CurrentClimbLocation;
        MantleLocation += ClimbWallNormal * -100.0f; //向后移动
        MantleLocation.Z += 150.0f; //向上移动

        //设置目标位置
        SetActorLocation(MantleLocation);

        //停止攀爬
        StopClimbing();

        //设置为地面状态
        SetMoveState(EMoveState::Grounded);

        //触发蓝图事件
        OnMantleLedge();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Orange, TEXT("攀上平台"));
        }
    }
}



// ==================================================
// --- 滑翔系统 ---
// ==================================================

void AHero_Main::StartGliding()
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::None &&
        CurrentMoveState == EMoveState::InAir && 
        HasEnoughStamina(20.0f))
        {
            //设置滑翔状态
            SetAdvancedMoveMode(EAdvancedMoveState::Gliding);

            //初始化滑翔变量
            GlideVelocity = CurrentVelocity;
            CurrentGlideTurn = 0.0f;
            GldieTimer = 0.0f;

            //保持当前水平速度
            float InitialSpeed = FVector(CurrentVelocity.X, CurrentVelocity.Y, 0.0f).Size();
            if (InitialSpeed < 300.0f)
            {
                //如果速度太慢， 给予初始速度
                FVector ForwardDir = GetActorForwardVector();
                CurrentVelocity = ForwardDir * 300.0f
            }
            //触发蓝图事件
            OnStartClimbing();

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, text("开始滑翔"));
            }
        }
}

void AHero_Main::StopGliding()
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Gliding)
    {
        //退出滑翔状态
        SetAdvancedMoveMode(EAdvancedMoveState::None);

        //恢复垂直速度
        CurrentVelocity.Z = -500.0f; //给予下落速度

        //触发蓝图事件
        OnStopGliding();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Cyan, TEXT("停止滑翔"));
        }
    }
}


void AHero_Main::GlideTurnInput(float TurnValue)
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Gliding)
    {
        CurrentGlideTurn = TurnValue;
    }
}


bool AHero_Main::CanStartGliding() const
{
    //检查高度是否足够
    FVector Start = GetActorLocation();
    FVector End = Start - FVector(0, 0, GlideParams.MinHeightToStart)

    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(const_cast<AHero_Main*>(this));

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult, Start, End, ECC_WorldStatic, QueryParams
    );

    //如果下方有地面且距离足够近， 则不能滑翔
    if (bHit && HitResult.Distance < GlideParams.MinHeightToStart)
    {
        return false;
    }

    //检查耐力
    if (!HasEnoughStamina(20.0f))
    {
        return false;
    }

    return true;
}


// ==================================================
// --- 摆荡系统 ---
// ==================================================

void AHero_Main::StartSwinging(const FVector& AnchorPoint)
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::None &&
        CurrentMovementState == EMoveState::InAir &&
        HasEnoughStamina(30.0f))
        {
            //设置摆荡状态
            SetAdvancedMoveMode(EAdvancedMoveState::Swinging);

            //保存锚点
            SwingAnchorPoint = AnchorPoint;

            //初始化摆荡变量
            CurrentSwingAngle = 0.0f;
            SwingAngularVelocity = 0.0f;
            SwingVelocity = FVector::ZeroVector;
            RopeLength = SwingParams.RopeLength;

            //计算初始角度
            FVector RopeDirection = SwingAnchorPoint - GetActorLocation();
            RopeDirection.Normalize();

            FVector DownVector = FVector(0.0f, 0.0f, -1.0f);
            CurrentSwingAngle = FMath::AcoS(FVector::DotProduct(RopeDirection, DownVector));
            CurrentSwingAngle = FMath::DegreesToRadians(CurrentSwingAngle);

            //触发蓝图事件
            OnStartSwinging();

            if (GEngine)
            {
                GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, TEXT("开始摆荡"));
            }
        }
}

void AHero_Main::StopSwinging()
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Swinging)
    {
        //退出摆荡状态
        SetAdvancedMoveMode(EAdvancedMoveState::Mone);

        //给予脱离速度(当前摆动方向）
        CurrentVelocity = SwingVelocity * 1.5f;

        //重置变量
        SwingAnchorPoint = FVector::ZeroVector;
        CurrentSwingAngle = 0.0f;
        SwingAngularVelocity = 0.0f;
        SwingVelocity = FVector::ZeroVector;

        //触发蓝图事件
        OnStopSwinging();

        if (GEngine)
        {
            GEngine->AddOnScreenDebugMessage(-1, 2.0f, FColor::Magenta, TEXT("停止摆荡"));
        }
    }
}


void AHero_Main::SwingInput(const FVector2D& Input)
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Swinging)
    {
        //根据输入增加角速度（玩家可以控制摆动）
        SwingAngularVelocity += Input.X * 100.0f * GetWorld()->GetDeltaSeconds();

        //限制最大角速度
        SwingAngularVelocity = FMath::Clamp(SwingAngularVelocity, -200.0f, 200.0f);
    }
}

bool AHero_Main::FindSwingAnchor(FVector& OutAnchorPoint)
{
    FVector Start = GetActorLocation();
    FVector End = Start + GetActorForwardVector() * SwingParams.RopeLength;


    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    bool bHit = GetWorld()->LineTraceSingleByChannel(
        HitResult, Start, End, ECC_WorldStatic, QueryParams
    );

    if (bHit)
    {
        OutAnchorPoint = HitResult.ImpactPoint;
        return true;
    }

    return false;
}

FVector AHero_Main::GetRopeDirection() const
{
    if (CurrentAdvancedMoveMode == EAdvancedMoveState::Swinging)
    {
        return (SwingAnchorPoint - GetActorLocation()).GetSafeNormal();
    }

    return FVector::ZeroVector;
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
    Super::CheckGroundStatus();

    if (CurrentMoveState == EMoveState::Grounded)
    {
        // 获取胶囊体底部中心位置（稍微提高一点避免自相交）
        FVector CapsuleBottom = GetActorLocation();
        CapsuleBottom.Z -= (CapsuleComp->GetScaledCapsuleHalfHeight() - 10.0f);

        // 射线检测距离：略大于胶囊体半径，用于检测前方地面
        float TraceDistance = CapsuleComp->GetScaledCapsuleRadius() + 20.0f;


        // 使用角色向前的方向进行检测（考虑移动方向）
        FVector TraceDirection = FVector::ZeroVector;
        if (MoveDirection.SizeSquared() > 0.1f)
        {
            TraceDirection = MoveDirection;
        }
        else
        {
            TraceDirection = GetActorForwardVector();
        }
        TraceDirection.Z = 0.0f;
        TraceDirection.Normalize();

        FVector TraceStart = CapsuleBottom;
        FVector TraceEnd = TraceStart + (TraceDirection * TraceDistance);
        TraceEnd.Z -= 50.0f; //向下倾斜，检测前方地面高度变化

        FHitResult HitResult;
        FCollisionQueryParams QueryParams;
        QueryParams.AddIgnoredActor(this);

        if (bShowDebugInfo)
        {
            DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Yellow, false, 0.0f, 0, 1.0f);
        }

         bool bHit = (GetWorld()->LineTraceSingleByChannel(
                                                          HitResult,
                                                          Start,
                                                          End,
                                                          ECC_WorldStatic,
                                                          QueryParams
                                                          )
                     );
                     if (bHit)
                     {
                        //计算地面坡度角度
                        FVector GroundNormal = HitResult.Normal;
                        float GroundSlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(GroundNormal, FVector::UpVector)));

                        //根据坡度采取不同处理
                        if (GroundSlopeAngle > CollisionParams.WalkableFloorAngle)
                        {
                            // 【情况A】坡度过陡，触发滑落
                            // 计算滑落方向（沿斜坡的法向投影到水平面）
                            FVector SlideDirection = FVector::CrossProduct)FVector::CrossProduct(GroundNormal, FVector::UpVector), GroundNormal
                            SlideDirection.Z = 0.0f; // 确保水平方向
                            SlideDirection.Normalize();

                            // 应用滑落速度（基于坡度角度和重力）
                            float SlideSeverity = (GroundSlopeAngle - CollisionParams.WalkableFloorAngle) / 30.0f; // 0-1的严重度
                            SlideSeverity = FMath::Clamp(SlideSeverity, 0.1f, 1.0f);

                            FVector SlideVelocity = SlideDirection * 200.0f * SlideSeverity;

                            //只在角色没有主动输入时应用滑落，避免与玩家控制冲突
                            if (MoveDirection.SizeSquared() < 0.1F)
                            {
                                CurrentVelocity.X = SlideVelocity.X;
                                CurrentVelocity.Y = SlideVelocity.Y;
                            }

                            if (GEngine && bShowDebugInfo)
                            {
                                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Orange, 
                                    FString::Printf(TEXT("坡度过陡！角度: %.1f° (最大: %.1f°)"), 
                                    GroundSlopeAngle, CollisionParams.WalkableFloorAngle));

                            }
                        }
                         else if(GroundSlopeAngle > 30.0f)
                         {
                            // 【情况B】坡度较陡但可站立，调整移动参数
                            // 在陡坡上行走时减速
                            float SlopeFactor = 1.0f - ((GroundSlopeAngle - 30.0f) / 30.0f) * 0.5f;
                            SlopeFactor = FMath::Clamp(SlopeFactor, 0.5f, 1.0f);

                            //临时调整移动速度（可通过蓝图事件通知动画系统）
                            if (GEngine && bShowDebugInfo)
                               {
                                GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Yellow, 
                                       FString::Printf(TEXT("陡坡行走，速度系数: %.2f"), SlopeFactor));
                                }
                         }
                    }

                    // 检测脚下地面的平整度（防止卡在微小凸起上）
                    FVector DownTraceStart = CapsuleBottom;
                    FVector DownTraceEnd = DownTraceStart - FVector(0, 0, 120.0F);

                    FHitResult DownHitResult;
                    bool bDownHit = GetWorld()->LineTraceSingleByChannel(
                        DownHitResult,
                        DownTraceStart,
                        DownTraceEnd,
                        ECC_WorldStatic,
                        QueryParams
                    );

                    if (bDownHit)
                    {
                        // 计算脚下的坡度
                        FVector FootGroundNormal = DownHitResult.Normal;
                        float FootSlopeAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(FootGroundNormal, FVector::UpVector)));

                        // 如果脚下是陡坡但角色没有移动，给予微小推力防止卡住
                        if (FootSlopeAngle > CollisionParams.WalkableFloorAngle &&
                            MoveDirection.SizeSquared() < 0.1f &&
                            CurrentVelocity.SizeSquared() < 100.0f)
                            {
                                // 给予一个向下的微小速度，帮助角色滑下陡坡
                                CurrentVelocity.Z = -50.0f;

                                if (GEngine && bShowDebugInfo)
                                {
                                    GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Cyan, 
                                        FString::Printf(TEXT("脚下陡坡，应用防卡住推力")));
                                }
                            }
                    }
    };

                    
                    

    //空中状态的特殊检测：检查是否即将着陆
    else if (CurrentMoveState == EMoveState::InAir || CurrentMoveState == EMoveState::Falling)
    {
        // 只在有明显下落速度时检测
        if (CurrentVelocity.Z < -100.0f)
        {
            FVector Start = GetActorLocation();
            FVector End = Start - FVector(0, 0, 200.0f); // 向下检测200单位

            FHitResult HitResult;
            FCollisionQueryParams QueryParams;
            QueryParams.AddIgnoredActor(this);

            bool bHit = GetWorld()->LineTraceSingleByChannel(
                HitResult, Start, End, ECC_WorldStatic, QueryParams
            );

            if(bHit);
            {
                float DistanceToGround = HitResult.dISTANCE;
                float TimeToLand = DistanceToGround / FMath::Abs(CurrentVelocity.Z);

                // 如果即将在0.2秒内着陆，可以触发预着陆状态（用于动画混合等）
                if (TimeToLand < 0.2f)
                {
                    //【新增】触发蓝图事件 - 预着陆
                    //在此处添加蓝图可实现的预着陆事件，用于动画混合


                     if (GEngine && bShowDebugInfo)
                    {
                        GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Green, 
                            FString::Printf(TEXT("即将着陆！距离: %.1f, 时间: %.2fs"),
                            DistanceToGround, TimeToLand));
                    }
                }
            }
        }
    }
     //更新角色的接地状态标志（用于其他系统查询）
     //可以根据检测结果设置它

     bool bIsFirmlyGrounded = (CurrentMoveState == EMoveState::Grounded);

     //如果在地面但速度很小，认为是稳定站立
     if (bIsFirmlyGrounded && CurrentVelocity.SizeSquared() < 25.0f)
    {
        // 可以触发"完全静止"状态，用于动画或音效
    }


}







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

