// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "BaseCharacter.generated.h"


//===================================
//核心数据契约一：通用状态枚举（Enums)
//===================================

UENUM(BlueprintType)
enum class ECharacterState : uint8
{
    Idle              UMETA(DisplayName = "空闲"),
    Moving            UMETA(DisplayName = "移动中"),
    Combat            UMETA(DisplayName = "战斗警戒"),
    Interacting       UMETA(DisplayName = "交互无敌状态"),
    Stunned           UMETA(DisplayName = "受击硬直"), //缺：击打
    Dead              UMETA(DisplayName = "死亡"), //缺：倒地
};

UENUM(BlueprintType)
enum class EMoveState : uint8
{
    Grounded          UMETA(DisplayName = "地面常态"),
    InAir             UMETA(DisplayName = "腾空/下落"),
    Falling           UMETA(DisplayName = "下落"),
    CustomLocomotion  UMETA(DisplayName = "自定义移动"),
};

UENUM(BlueprintType)
enum class ECombatAction : uint8
{
    None,
    Attacking,        //通用攻击状态，子类可分为轻重攻击
    Defending,        //通用防御姿势，子类可详细分为格挡或者弹反
};


//===================================
//核心数据契约二：通用参数结构体（Structs)
//===================================


//10. 基础移动参数
USTRUCT(BlueprintType)
struct FBaseMovementParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float MaxWalkSpeed = 500.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float Acceleration = 2048.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float JumpVelocity = 600.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float GroundFriction = 8.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
    float AirControl = 0.3f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Angle")
    float MaxWalkableSlope = 60.0f; //斜坡检测：最大可移动坡度角度
};


//20.角色基础生存属性
USTRUCT(BlueprintType)
struct FCharacterAttributes
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxHealth = 100.0f;   //主角生命值
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentHealth = 100.0f;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float MaxStamina = 100.0f;  //能量绿条 （限制无限瞎滚）
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CurrentStamia = 100.0f;
    
    // 可扩展至 护甲， 能量，经验值等
};


//30.核心通用物理法则参数（替代引擎自带的重力与摩擦力）
USTRUCT(BlueprintType)
struct FCustomPhysicsParams
{
    GENERATED_BODY()
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Gravity")
    float GravityScale = 1.0f; // 重力倍率，比如主角开重攻击的时候可以变成0.5，实现滞空
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Gravity")
    float TerminalVelocity = 4000.0f; //终端速度 （最高下落速度，防止从极高处掉落时穿模）
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Physics|Friction")
    float BrakingDeceleration = 2048.0f; //刹车减速度 （急停或者急转弯时候的制动力）
};


UCLASS()
class ABaseCharacter : public APawn
{
    GENERATED_BODY()

public:
    // Sets default values for this pawn's properties
    ABaseCharacter();

protected:
    // Called when the game starts or when spawned
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaTime) override;



protected:
    //==============================================
    // --- 通用核心组件 （Core Component) ---
    //==============================================
    
    // 1. 胶囊体（Capusule):主角的物理外壳，用来挡子弹，算碰撞体积
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class UCapsuleComponent* CapsuleComp;
    
    // 2. 骨骼模型（Skeletal Mesh): 主角真正的肉体，用来播放跑酷和攻击动画
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    class USkeletalMeshComponent* MeshComp;
    

public:
    //=============================================
    // --- 通用状态变量实例（State Variables）---
    //=============================================
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|State")
    ECharacterState CurrentCharacterState = ECharacterState::Idle;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|State")
    EMoveState CurrentMoveState = EMoveState::Grounded;
    
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Character|State")
    ECombatAction CurrentCombatAction = ECombatAction::None;

    
    // ==========================================
    // ---  参数包实例 (策划的调音台面板) ---
    // ==========================================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Parameters")
    FBaseMovementParams MovementParams;
       
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Parameters")
    FCharacterAttributes Attributes;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Parameters")
    FCustomPhysicsParams PhysicsParams;
    
    
    // ==========================================
    // ---  核心物理与动量（内部使用） ---
    // ==========================================
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Physics")
    FVector CurrentVelocity = FVector::ZeroVector;
    
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Movement")
    float CurrentSpeedMagnitude = 0.0f;
    
    //输入方向（对于ai控制的角色，此变量将由ai逻辑设置
    FVector MoveInputDirection = FVector::ZeroVector;
    
public:
    // ==========================================
    // ---  核心物功能函数（公共接口） ---
    // ==========================================
    
    UFUNCTION(BlueprintCallable, Category = "Character|State")
    virtual void SetCharacterState(ECharacterState NewState);
    
    UFUNCTION(BlueprintCallable, Category = "Character|State")
    virtual void SetMoveState(EMoveState NewState);
    
    UFUNCTION(BlueprintCallable, Category = "Character|State")
    bool IsAlive() const { return Attributes.CurrentHealth > 0.0f; }
    
    UFUNCTION(BlueprintCallable, Category = "Character|State")
    bool IsGrounded() const {return CurrentMoveState == EMoveState::Grounded; }
    
    //移动控制接口 （子类或控制器调用）
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void Move(const FVector& Direction, float Scale = 1.0f);
    
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void Jump();
    
    UFUNCTION(BlueprintCallable, Category = "Character|Movement")
    virtual void StopJump();
    
    //战斗系统接口 （子类或控制器调用）
    UFUNCTION(BlueprintCallable, Category = "Character|Combat")
    virtual void ApplyDamage(float DamageAmount);
    
    UFUNCTION(BlueprintCallable, Category = "Character|Combat")
    virtual void StartAttack();
    
    UFUNCTION(BlueprintCallable, Category = "Character|Combat")
    virtual void StartDefend();
    
    //组件获取器 （子类或控制器调用）
    UFUNCTION(BlueprintCallable, Category = "Character|Components")
    class UCapsuleComponent* GetCapsuleComponent() const { return CapsuleComp; }
    
    UFUNCTION(BlueprintCallable, Category = "Character|Components")
    class USkeletalMeshComponent* GetMeshComponent() const { return MeshComp; }
    
protected:
    // ==========================================
    // ---  内部逻辑函数（可以被子类重写） ---
    // ==========================================
    
    //每一帧物理更新 （核心逻辑）
    virtual void ApplyBasePhysics(float DeltaTime);
    
    //地面检测
    virtual void CheckGroundStatus();
    
    //死亡处理
    virtual void OnDeath();
    
    //状态改变事件 （蓝图可实现）
    UFUNCTION(BlueprintImplementableEvent, Category = "Character|Events")
    void OnCharacterStateChanged(ECharacterState NewState, ECharacterState PreviousState);
    
    UFUNCTION(BlueprintImplementableEvent, Category = "Character|Events")
    void OnMoveStateChanged(EMoveState NewState, EMoveState PreviousState);
    
    
    
    
    
    
    
    
    

    
};




