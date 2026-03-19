// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "InputActionValue.h"
#include "BaseCharacter.h"
#include "Hero_Main.generated.h"



class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
class UBoxComponent; //武器碰撞体

//===================================
//核心数据契约一：枚举（Enums)
//===================================

//===================================
//武器攻击类型枚举（Enums)
//===================================

UENUM(BlueprintType)
enum class EWeaponType : uint8
{
    None             UMETA(DisplayName = "无"),
    Sword            UMETA(DisplayName = "剑")
};


//===================================
//攻击连段枚举（Enums)
//===================================


UENUM(BlueprintType)
enum class EAttackComboState : uint8
{
    Ready             UMETA(DisplayName = "准备"),
    Light1            UMETA(DisplayName = "轻攻击1段"),
    Light2            UMETA(DisplayName = "轻攻击2段"),
    Light3            UMETA(DisplayName = "轻攻击3段"),
    HeavyCharge       UMETA(DisplayName = "重攻击蓄力"),
    HeavyRelease      UMETA(DisplayName = "重攻击释放"),
    Finisher          UMETA(DisplayName = "处决招式"),
};


//===================================
//主角状态枚举
//===================================

UENUM(BlueprintType)
enum class EHeroState : uint8
{
    Stealth           UMETA(DisplayName = "潜行"),
    LockedOn          UMETA(DisplayName = "锁定目标中"),
};


UENUM(BlueprintType)
enum class ESpeedState : uint8
{
    Walking           UMETA(DisplayName = "行走"),
    Jogging           UMETA(DisplayName = "慢跑"),
    Sprinting         UMETA(DisplayName = "冲刺"),
};


UENUM(BlueprintType)
enum class EAdvancedMoveState : uint8
{
    None              UMETA(DisplayName = "无"),
    Climbing          UMETA(DisplayName = "攀爬中"),
    WallRunning       UMETA(DisplayName = "蹬墙跑"),
    Gliding           UMETA(DisplayName = "空中滑翔"),
    Swinging          UMETA(DisplayName = "绳索/蛛丝摆荡"),
    Perching          UMETA(DisplayName = "立柱蹲据"),
    Crouching         UMETA(DisplayName = "蹲下"),
    Sliding           UMETA(DisplayName = "滑铲"),
};

UENUM(BlueprintType)
enum class ETargetingState : uint8
{
    FreeCamera        UMETA(DisplayName = "自由视角"),
    LockedOn          UMETA(DisplayName = "锁定目标"), //缺：视角锁定
};


//===================================
//核心数据契约二：参数结构体（Structs)
//===================================

// 10.跑酷空间侦测参数
USTRUCT(BlueprintType)
struct FParkourParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    float ForwardTraceDistance = 150.0f;  //往前发射射线或者球体的探测距离
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    float TraceRadius = 30.0f; //球形判定的半径
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height")
    float MaxLedgeHeight = 250.0f; //角色最高能双手抓住的边缘高度
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Height")
    float MinVaultHeight = 50.0f; //至少多高的障碍物可以触发翻越，而不是直接跑过去
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
    float MaxApproachAngle = 45.0f; //扇形判定：面对墙壁的最大偏角，斜着跑不许上墙
};

//20. 基础移动参数
USTRUCT(BlueprintType)
struct FHeroMovementParams
{
    GENERATED_BODY()
    
    //继承基类的基础移动参数，这里为主角特有
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxSprintSpeed = 800.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ClimbSpeed = 300.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
    float SlideFriction = 0.1f; //滑铲时的地面摩擦力（越小滑得越远）
};

//30.硬核战斗参数
USTRUCT(BlueprintType)
struct FCombatParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ParryWindowSeconds = 0.2f;       //弹反判定帧
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float PostureDamageMultiplier = 1.5f;  //架势伤害倍率
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float DodgeDistance = 400.0f;          //垫步距离
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float ParryWindowDuration = 0.2f;      //完美格挡（弹反）的判定窗口（0.2秒硬核博弈）
};


//50.主角进阶物理参数
USTRUCT(BlueprintType)
struct FHeroPhysicsParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glide")
    float GlideDescentRate = -200.0f; //滑翔时下坠的重力，会比自由落体小很多
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float MaxSwingCableLength = 3000.0f; //摆荡绳索的最大发射距离
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float SwingMomentumMultiplier = 1.2f; //摆荡谷底的动能放大器（实现越荡越快）
};

// 60. 主角特有属性 （扩展基类的FCharacterAttributes)
USTRUCT(BlueprintType)
struct FHeroAttributes
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxPosture = 100.0f;  // 架勢槽 （打滿被處決）
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentPosture = 100.0f;
    
    // 注意：生命值（Health)和耐力（Stamia)使用BaseCharacter的FCharacterAttributes
};

//70.环境碰撞与防卡死参数
USTRUCT(BlueprintType)
struct FCollisionResolutionParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float MaxStepHeight = 45.0f; //系统会把主角自动“抬”上去的最大阶梯高度
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float WalkableFloorAngle = 65.0f; //重力会强制把主角往下拉的极限站立坡度，超过这个坡度，会变成滑坡
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collision")
    float WallSlideFriction = 0.2f; //贴墙滑行的摩擦力 （玩家推着墙走保留多少动能）
    
};

//80.滑翔参数结构体

USTRUCT(BlueprintType)
struct FGlideParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glide")
    float MinHeightToStart = 500.0f; //开始滑翔的最低高度
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glide")
    float MaxGlideSpeed = 1200.0f; //最大滑翔速度
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glide")
    float GlideLifeForce = 300.0f; //升力系数
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glide")
    float GlideDragCoefficient = 0.1f; //空气阻力系数
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Glide")
    float TurnRate = 90.0f; //滑翔转向速率（度/秒）
    
};

//90.摆荡参数结构体

USTRUCT(BlueprintType)
struct FSwingParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float RopeLength = 1000.0f; //默认绳索长度
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float SwingGravityScale = 0.5f; //摆荡时的重力缩放
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float DampingFactor = 0.98f; //摆动阻尼
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float MaxSwingAngle = 60.0f; //最大摆动角度（度）
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Swing")
    float RopeAttachSpeed = 2000.0f; //绳索附着速度
};


//100.攀爬参数结构体


USTRUCT(BlueprintType)
struct FClimbParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    float ClimbSpeed = 300.0f; //攀爬速度
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    float LedgeGrabTime = 0.3f; //抓握边缘的时间
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    float MantleTime = 0.5f; //攀上平台的时间
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    float WallDetectionRange = 200.0f; //墙壁检测范围
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Climb")
    float ClimbStaminaCost = 10.0f; //每秒耐力消耗
    
};


//110.武器攻击参数


USTRUCT(BlueprintType)
struct FWeaponAttackParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float LightAttackDamage = 20.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float HeavyAttackDamage = 40.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float AttackDamage = 150.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float AttackRadius = 50.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attack")
    float ComboWindow = 0.5f; //连段时间窗口
};


UCLASS()
class PROJECT_Z_API AHero_Main : public ABaseCharacter
{
	GENERATED_BODY()

public:
	// Sets default values for this pawn's properties
	AHero_Main();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Called to bind functionality to input
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;
    
    //状态检查函数
    UFUNCTION(BlueprintCallable, Category = "Hero | State")
    bool IsInCombat() const { return CurrentCharacterState == ECharacterState::Combat; }
    
    //或取当前状态
    UFUNCTION(BlueprintCallable, Category = "Hero | State")
    EHeroState GetCurrentHeroState() const { return CurrentHeroState; }
    
    //设置状态函数
    UFUNCTION(BlueprintCallable, Category = "Hero | State")
    void SetHeroState(EHeroState NewState);
    
    
    //==============================================
    // --- 攀爬相关函数  ---
    //==============================================
    
    /** 开始攀爬墙壁 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Parkour")
    void StartClimbing(const FVector& WallLocation, const FVector& WallNormal);
    
    /**停止攀爬 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Parkour")
    void StopClimbing();
    
    /**攀爬移动输入 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Parkour")
    void ClimbMoveInput(const FVector2D& Input);
    
    /**从攀爬状态跳跃 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Parkour")
    void ClimbJump();
    
    /**攀上平台（边缘 ）*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Parkour")
    void MantleLedge();
    
    //==============================================
    // --- 滑翔相关函数  ---
    //==============================================
    
    
    /** 开始滑翔*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    void StartGliding();
    
    /** 停止滑翔*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    void StopGliding();
    
    /** 滑翔转向输入*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    void GlidingTurnInput(float TurnValue);
    
    //==============================================
    // --- 摆荡相关函数  ---
    //==============================================
    
    /** 开始摆荡*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    void StartSwinging(const FVector& AnchorPoint);
    
    /** 停止摆荡*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    void StopSwinging();
    
    /** 摆荡输入*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    void SwingingTurnInput(float TurnValue);
    
    
    //==============================================
    // --- 战斗系统增强函数  ---
    //==============================================
    
    /** 激活武器碰撞体*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Combat")
    void ActivateWeaponCollision();
    
    /** 停用武器碰撞体*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Combat")
    void DeactivateWeaponCollision();
    
    /** 检测攻击命中*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Combat")
    void CheckAttackHit();
    
    /** 应用攻击伤害*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Combat")
    void ApplyAttackDamage(ABaseCharacter* Target, float Damage, bool bIsHeavyAttack = false);
    
    /** 重置连段 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Combat")
    void ResetAttackCombo();
    
    /**开始蓄力重攻击 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Combat")
    void StartHeavyCharge();
    
    /** 释放蓄力重攻击*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Combat")
    void ReleaseHeavyAttack();
    
    
    //==============================================
    // --- 耐力系统函数  ---
    //==============================================
    
    /** 消耗耐力 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Attributes")
    void ConsumeStamina(float Amount);
    
    /** 恢复耐力*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Attributes")
    void RecoverStamina(float Amount);
    
    /** 恢复架势*/
    UFUNCTION(BlueprintCallable, Category = "Hero|Attributes")
    void RecoverPosture(float Amount);
    
    
    //==============================================
    // --- 状态查询接口  ---
    //==============================================
    
    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    bool IsCombat() const { return CurrentCharacterState == ECharacterState::Combat; }
    

    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    bool IsClimbing() const { return CurrentAdvancedMoveMode == EAdvancedMoveState::Climbing; }
    
    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    EAttackComboState GetAttackComboState() const { return CurrentAttackComboState; }
    
    
    //==============================================
    // --- 辅助函数  ---
    //==============================================
    
    float GetCurrentSpeed() const;
    
    /** 获取摆荡锚点位置 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    FVector GetSwingAnchorPoint() const { return SwingAnchorPoint; }
    
    /** 获取当前摆荡角度 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    float GetSwingAngle() const { return CurrentSwingAngle; }
    
    /** 获取绳索方向 */
    UFUNCTION(BlueprintCallable, Category = "Hero|Movement")
    FVector GetRopeDirection() const;
    
    
    
    
protected:
    //==============================================
    // --- 核心组件 （Core Component) ---
    //==============================================
    
    /** 武器碰撞体 （用于攻击检测）*/
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UBoxComponent* WeaponCollisionComp;
    
    /** 滑翔特效组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
    class UParticleSystemComponent* GlidePartieComp;
    
    /** 摆荡绳索组件 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Component")
    class UCableComponent* RopeCableComp;
    
    
    
    
    //==============================================
    // --- 主角核心组件 （Core Component) ---
    //==============================================
    
    
    //3. 弹簧臂 （Spring Arm): 像一根自拍杆，把摄像机挑在主角身后，防止穿墙
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    USpringArmComponent* SpringArmComp;
    
    //4. 摄像机（Camera):玩家的眼睛
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    UCameraComponent* CameraComp;
    
    
    //===============================================
    // --- 增强输入资产 （Enhanced Input Assets) ---
    //===============================================
    
    
    // 输入映射上下文 （相当于一套按键方案，比如：战斗模式按键 或 跑酷模式按键）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputMappingContext* DefaultMappingContext;
    
    //移动动作 （推左摇杆/按WASD)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* MoveAction;
    
    // 视角动作 （推右摇杆/动鼠标）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LookAction;
    
    
    //--- 基础机动动作 ---
    
    //跳跃
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* JumpAction;
    
    //冲刺（快速奔跑）
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* SprintAction;
    
    //蹲下
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* CrouchAction;
    
    //--- 硬核战斗动作 ---
    
    //轻攻击
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LightAttackAction;
    
    //重攻击
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* HeavyAttackAction;
    
    //完美格挡
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* ParryAction;
    
    //闪避
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* DodgeAction;
    
    //锁定
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
    UInputAction* LockOnAction;
    
    //===============================================
    // --- 输入相应函数 （Input Callbacks) ---
    //===============================================
    
    // 当玩家推左动摇杆时，引擎会调用这个函数
    void MoveInput(const FInputActionValue& Value);
    
    // 当玩家推动右摇杆转动视角时，引擎会调用这个函数
    void Look(const FInputActionValue& Value);
    
    //当玩家蹲下，跳跃或者冲刺的时候，引擎会选择调用以下函数
    void JumpStart(const FInputActionValue& Value);
    void JumpEnd(const FInputActionValue& Value);
    void StartSprint(const FInputActionValue& Value);
    void StopSprint(const FInputActionValue& Value);
    void StartCrouch(const FInputActionValue& Value);
    void StopCrouch(const FInputActionValue& Value);
    
    //当玩家攻击的时候，引擎会选择调用以下函数
    void PerformLightAttack(const FInputActionValue& Value);
    void PerformHeavyAttack(const FInputActionValue& Value);
    void PerformParry(const FInputActionValue& Value);
    void PerformDodge(const FInputActionValue& Value);
    void ToggleLockOn(const FInputActionValue& Value);
    
    //物理推演与跑酷侦测函数
    virtual void ApplyHeroPhysics(float DeltaTime);
    
    void ApplyGlidePhysics(float DeltaTime);
    
    void ApplyClimbPhysics(float DeltaTime);
    
    void ApplySwingPhysics(float DeltaTime);
    
    void CheckParkourFront(); //球形射线检测前方墙壁
    
    void UpdateSpeedState();
    
    void CheckHeroGroundStatus();
    
    bool CanStateGliding() const;
    
    bool FindSwingAnchor(FVector& OutAnchorPoint);
    
    //=============================================
    // --- 战斗系统核心参数---
    //=============================================
    
    /** 增强的攻击开始函数（激活武器碰撞体） */
    virtual void StartAttack() override;
    
    /** 处理攻击命中结果 */
    UFUNCTION()
    void OnWeaponOverlapBegin(UPrimitiveComponent* OverlappedComp, AActor* OtherActor,
                              UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                              bool bFromSweep, const FHitResult& SweepResult);
    
    /** 计算攻击伤害 */
    float CalculateAttackDamage(bool bIsHeavyAttack) const;
    
    /** 更新连段状态 */
    void UpdateAttackCombo(bool bIsHeavyAttack = false);
    
    /** 处理弹反窗口 */
    void HandleParryWindow();
    
    /** 每帧恢复耐力 */
    void RecoverStaminaOverTime(float DeltaTime);
    
    /** 每帧恢复架势 */
    void RecoverPostureOverTime(float DeltaTime);
    
    /** 检查耐力是否足够 */
    bool HasEnoughStamina(float RequiredAmount) const;
    
    /** 绘制调试信息 */
    void DrawDebugInfo();
    
    /** 切换调试显示 */
    void TaggleDebugDisplay();
    
    
private:
    //=============================================
    // --- 核心物理与动量---
    //=============================================
    
    FVector2D CurrentMovementInput = FVector2D::ZeroVector;
    FVector MoveDirection = FVector::ZeroVector;
    
    float HorizontalSpeed = 0.0f;
    
    /** 滑翔相关变量 */
    FVector GlideVelocity = FVector::ZeroVector;
    float CurrentGlideTurn = 0.0f;
    float GlideTimer = 0.0f;
    
    /** 摆荡相关变量 */
    FVector SwingAnchorPoint = FVector::ZeroVector;
    float CurrentSwingAngle = 0.0f;
    float SwingAngularVelocity = 0.0f;
    FVector SwingVelocity = FVector::ZeroVector;
    float RopeLength = 1000.0f;
    
    /** 攀爬相关变量 */
    FVector ClimbWallNormal = FVector::ZeroVector;
    FVector CurrentClimbLocation = FVector::ZeroVector;
    FVector2D ClimbInput = FVector2D::ZeroVector;
    float ClimbTimer = 0.0f;
    bool bIsMantling = false;
    
    /** 战斗相关变量 */
    TArray<AActor*> AlreadyHitActors; //已命中的敌人（防止重复命中）
    float LastAttackTime = 0.0f;
    float HeavyChargeTime = 0.0f;
    float ParryWindowTimer = 0.0f;
    bool bIsInParryWindow = false;
    
    /** 耐力恢复计时器 */
    float StaminaRecoveryDelay = 1.0f;
    float StaminaRecoveryTimer = 0.0f;
    
    /** 调试相关 */
    bool bShowDebugInfo = false;
    
    
    
public:
    //=============================================
    // --- 状态变量实例（State Variables）---
    //=============================================
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero|State")
    EHeroState CurrentHeroState = EHeroState::Stealth;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero|State")
    ESpeedState CurrentSpeedState = ESpeedState::Jogging; // 速度状态（行走/慢跑/冲刺）
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero|State")
    EAdvancedMoveState CurrentAdvancedMoveMode = EAdvancedMoveState::None;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero|State")
    ETargetingState CurrentTargetingState = ETargetingState::FreeCamera;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero|State")
    EAttackComboState CurrentAttackComboState = EAttackComboState::Ready;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Hero|State")
    EWeaponType CurrentWeaponType = EWeaponType::Sword;
    
    
    //=============================================
    // --- 状态查询接口（State Queries）---
    //=============================================
    
    bool IsGrounded() const {return CurrentMoveState == EMoveState::Grounded; }
    
    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    bool IsGliding() const { return CurrentAdvancedMoveMode == EAdvancedMoveState::Gliding; }
    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    bool IsSwinging() const { return CurrentAdvancedMoveMode == EAdvancedMoveState::Swinging; }
    
    //=============================================
    // --- 状态设置接口（State Setters）---
    //=============================================
    
    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    void SetSpeedState(ESpeedState NewState);
    
    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    void SetAdvancedMoveMode(EAdvancedMoveState NewMode);
    
    UFUNCTION(BlueprintCallable, Category = "Hero|State")
    void SetTargetingState(ETargetingState NewState);
    
    
    // ==========================================
    // ---  参数包实例 (策划的调音台面板) ---
    // ==========================================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters")
    FParkourParams ParkourParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters")
    FHeroMovementParams HeroMovementParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters")
    FCombatParams CombatParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters")
    FHeroAttributes HeroAttributes;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters")
    FHeroPhysicsParams HeroPhysicsParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters")
    FCollisionResolutionParams CollisionParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters|Glide")
    FGlideParams GlideParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters|Swing")
    FSwingParams SwingParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters|Climb")
    FClimbParams ClimbParams;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hero|Parameters|Weapon")
    FWeaponAttackParams WeaponAttackParams;
    
    //=============================================
    // --- 蓝图事件---
    //=============================================
    
    /** 当开始滑翔时触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnStartGliding();
    
    /** 当停止滑翔时触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnStopGliding();
    
    /** 当开始摆荡时触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnStartSwinging();
    
    /** 当停止摆荡时触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnStopSwinging();
    
    /** 当开始攀爬时触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnStartClimbing();
    
    /** 当停止攀爬时触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnStopClimbing();
    
    /** 当攀爬上平台时触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnMantleLedge();
    
    /** 当连段状态改变的时候触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnAttackComboChanged(EAttackComboState NewComboState);
    
    /** 当成功命中敌人的时候触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnSuccessfulHit(ABaseCharacter* HitEnemy, float DamageDealt);
    
    /** 当成功弹反的时候触发 */
    UFUNCTION(BlueprintImplementableEvent, Category = "Hero|Events")
    void OnSuccessfulParry(AActor* ParriedEnemy);
    

};




