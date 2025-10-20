# Getting Started with KatanaCombat

Complete step-by-step setup guide for integrating the KatanaCombat system into your Unreal Engine 5.6 project.

---

## Prerequisites

### Required
- **Unreal Engine 5.6** or later
- **C++ project** with source code access
- **Motion Warping Plugin** (ships with UE5)
- **Enhanced Input Plugin** (ships with UE5)

### Recommended
- Basic understanding of Unreal C++ and Blueprint
- Familiarity with Animation Blueprints and Montages
- Third-person character template knowledge

---

## Step 1: Add KatanaCombat Module

### 1.1 Copy Source Files

Copy the `KatanaCombat` folder to your project's `Source` directory:

```
YourProject/
└── Source/
    ├── YourProject/
    └── KatanaCombat/          ← Add this
        ├── Public/
        ├── Private/
        └── KatanaCombat.Build.cs
```

### 1.2 Update .uproject File

Add KatanaCombat module to your `.uproject`:

```json
{
    "Modules": [
        {
            "Name": "YourProject",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        },
        {
            "Name": "KatanaCombat",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ],
    "Plugins": [
        {
            "Name": "MotionWarping",
            "Enabled": true
        },
        {
            "Name": "EnhancedInput",
            "Enabled": true
        }
    ]
}
```

### 1.3 Update Build Dependencies

Edit `YourProject.Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core",
    "CoreUObject",
    "Engine",
    "InputCore",
    "EnhancedInput",
    "MotionWarping",
    "KatanaCombat"  // Add this
});
```

### 1.4 Regenerate Project Files

Right-click `.uproject` → **Generate Visual Studio project files**

### 1.5 Compile

Open solution and build in Visual Studio (Ctrl+Shift+B)

---

## Step 2: Configure Character

### 2.1 Add Components to Character Header

Edit your character's `.h` file:

```cpp
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Core/HitReactionComponent.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"

UCLASS()
class YOURPROJECT_API AYourCharacter : public ACharacter,
                                       public ICombatInterface,
                                       public IDamageableInterface
{
    GENERATED_BODY()

public:
    // Components
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UCombatComponent> CombatComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UTargetingComponent> TargetingComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UWeaponComponent> WeaponComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UHitReactionComponent> HitReactionComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
    TObjectPtr<UMotionWarpingComponent> MotionWarpingComponent;

    // ICombatInterface Implementation
    virtual ECombatState GetCombatState_Implementation() const override;
    virtual EAttackPhase GetCurrentAttackPhase_Implementation() const override;
    virtual bool IsAttacking_Implementation() const override;
    virtual bool IsBlocking_Implementation() const override;
    virtual bool IsInCounterWindow_Implementation() const override;
    virtual bool IsInParryWindow_Implementation() const override;
    virtual float GetPosturePercent_Implementation() const override;
    virtual bool IsGuardBroken_Implementation() const override;

    // IDamageableInterface Implementation
    virtual void ApplyDamage_Implementation(const FDamageInfo& DamageInfo) override;
    virtual float GetCurrentHealth_Implementation() const override;
    virtual float GetMaxHealth_Implementation() const override;
    virtual bool IsDead_Implementation() const override;
    virtual bool IsVulnerableToFinisher_Implementation() const override;

private:
    UPROPERTY(EditAnywhere, Category = "Health")
    float MaxHealth = 100.0f;

    UPROPERTY()
    float CurrentHealth = 100.0f;
};
```

### 2.2 Initialize Components in Constructor

Edit your character's `.cpp` file constructor:

```cpp
AYourCharacter::AYourCharacter()
{
    // Create combat components
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));
    WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
    HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

    // Initialize health
    CurrentHealth = MaxHealth;
}
```

### 2.3 Implement Interface Methods

```cpp
// ICombatInterface
ECombatState AYourCharacter::GetCombatState_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCombatState() : ECombatState::Idle;
}

EAttackPhase AYourCharacter::GetCurrentAttackPhase_Implementation() const
{
    return CombatComponent ? CombatComponent->GetCurrentPhase() : EAttackPhase::Recovery;
}

bool AYourCharacter::IsAttacking_Implementation() const
{
    return CombatComponent ? CombatComponent->IsAttacking() : false;
}

bool AYourCharacter::IsBlocking_Implementation() const
{
    return CombatComponent ? CombatComponent->IsBlocking() : false;
}

bool AYourCharacter::IsInCounterWindow_Implementation() const
{
    return CombatComponent ? CombatComponent->IsInCounterWindow() : false;
}

bool AYourCharacter::IsInParryWindow_Implementation() const
{
    return CombatComponent ? CombatComponent->IsInParryWindow() : false;
}

float AYourCharacter::GetPosturePercent_Implementation() const
{
    return CombatComponent ? CombatComponent->GetPosturePercent() : 1.0f;
}

bool AYourCharacter::IsGuardBroken_Implementation() const
{
    return CombatComponent ? CombatComponent->IsGuardBroken() : false;
}

// IDamageableInterface
void AYourCharacter::ApplyDamage_Implementation(const FDamageInfo& DamageInfo)
{
    if (HitReactionComponent)
    {
        HitReactionComponent->ApplyDamage(DamageInfo);
    }
}

float AYourCharacter::GetCurrentHealth_Implementation() const
{
    return CurrentHealth;
}

float AYourCharacter::GetMaxHealth_Implementation() const
{
    return MaxHealth;
}

bool AYourCharacter::IsDead_Implementation() const
{
    return CurrentHealth <= 0.0f;
}

bool AYourCharacter::IsVulnerableToFinisher_Implementation() const
{
    return CombatComponent && CombatComponent->IsGuardBroken();
}
```

### 2.4 Bind Weapon Hit Event

In `BeginPlay()`:

```cpp
void AYourCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (WeaponComponent)
    {
        WeaponComponent->OnWeaponHit.AddDynamic(this, &AYourCharacter::OnWeaponHitTarget);
    }
}

void AYourCharacter::OnWeaponHitTarget(AActor* HitActor, const FHitResult& HitResult,
                                        UAttackData* AttackData)
{
    if (!HitActor || !AttackData) return;

    // Build damage info
    FDamageInfo DamageInfo;
    DamageInfo.Amount = AttackData->BaseDamage;
    DamageInfo.PostureDamage = AttackData->PostureDamage;
    DamageInfo.Instigator = this;
    DamageInfo.HitLocation = HitResult.Location;
    DamageInfo.HitNormal = HitResult.Normal;
    DamageInfo.AttackType = AttackData->AttackType;
    DamageInfo.AttackPhase = CombatComponent->GetCurrentPhase();
    DamageInfo.HitstunDuration = AttackData->HitStunDuration;
    DamageInfo.bIsHeavyAttack = (AttackData->AttackType == EAttackType::Heavy);

    // Apply charge multiplier if charging
    if (CombatComponent->GetCombatState() == ECombatState::ChargingHeavyAttack)
    {
        DamageInfo.Amount *= CombatComponent->GetChargeDamageMultiplier();
    }

    // Apply counter multiplier
    if (CombatComponent->IsInCounterWindow())
    {
        DamageInfo.Amount *= CombatComponent->GetCombatSettings()->CounterDamageMultiplier;
    }

    // Apply damage
    if (HitActor->Implements<UDamageableInterface>())
    {
        IDamageableInterface::Execute_ApplyDamage(HitActor, DamageInfo);
    }
}
```

---

## Step 3: Setup Weapon Sockets

### 3.1 Add Sockets to Weapon Mesh

In your weapon skeletal mesh:

1. Open weapon skeletal mesh in editor
2. Add socket at weapon **base/hilt**: Name it `WeaponStart`
3. Add socket at weapon **tip/blade end**: Name it `WeaponEnd`

### 3.2 Configure WeaponComponent

In Character Blueprint or C++:

```cpp
WeaponComponent->WeaponStartSocket = "WeaponStart";
WeaponComponent->WeaponEndSocket = "WeaponEnd";
WeaponComponent->TraceRadius = 5.0f;
WeaponComponent->TraceChannel = ECC_Pawn;
```

---

## Step 4: Create Data Assets

### 4.1 Create CombatSettings Asset

1. Right-click in Content Browser → **Miscellaneous** → **Data Asset**
2. Choose `CombatSettings` as parent class
3. Name it `DA_CombatSettings`
4. Configure values (or use defaults)

### 4.2 Assign to CombatComponent

In Character Blueprint:
- Select CombatComponent
- Set `CombatSettings` to `DA_CombatSettings`

---

## Step 5: Create Your First Attack

### 5.1 Prepare Animation Montage

1. Create animation montage from attack animation
2. Name it `AM_LightAttack1`
3. Create section: `LightAttack1`

### 5.2 Add AnimNotifyStates

**Add these in order**:

1. **Windup Phase** (0.0s - 0.3s)
   - Add `AnimNotifyState_AttackPhase`
   - Set `Phase = Windup`

2. **Active Phase** (0.3s - 0.5s)
   - Add `AnimNotifyState_AttackPhase`
   - Set `Phase = Active`
   - Add `AnimNotify_ToggleHitDetection` at 0.3s, `bEnable = true`
   - Add `AnimNotify_ToggleHitDetection` at 0.5s, `bEnable = false`

3. **Recovery Phase** (0.5s - 1.0s)
   - Add `AnimNotifyState_AttackPhase`
   - Set `Phase = Recovery`

4. **Combo Window** (0.4s - 0.9s, overlaps Active+Recovery)
   - Add `AnimNotifyState_ComboWindow`

### 5.3 Create AttackData Asset

1. Right-click → **Miscellaneous** → **Data Asset**
2. Choose `AttackData` as parent class
3. Name it `DA_LightAttack1`
4. Configure:

```
Basic:
- AttackType: Light
- Direction: None
- AttackMontage: AM_LightAttack1
- MontageSection: LightAttack1

Damage:
- BaseDamage: 25.0
- PostureDamage: 10.0
- HitStunDuration: 0.0

Timing:
- bUseAnimNotifyTiming: true
- TimingFallbackMode: AutoCalculate

Motion Warping:
- bUseMotionWarping: true
- MaxWarpDistance: 400.0
- MinWarpDistance: 50.0
```

### 5.4 Assign as Default Attack

In Character Blueprint:
- Select CombatComponent
- Set `DefaultLightAttack` to `DA_LightAttack1`

---

## Step 6: Setup Input

### 6.1 Create Input Actions

Create these Input Actions in Content Browser:

- `IA_Move` (Value, Vector2D)
- `IA_Look` (Value, Vector2D)
- `IA_LightAttack` (Button)
- `IA_HeavyAttack` (Button)
- `IA_Block` (Button)
- `IA_Evade` (Button)

### 6.2 Create Input Mapping Context

Create `IMC_Combat` with mappings:

```
IA_Move:
- W/S: Forward/Backward axis
- A/D: Left/Right axis

IA_Look:
- Mouse X/Y

IA_LightAttack:
- Left Mouse Button

IA_HeavyAttack:
- Right Mouse Button

IA_Block:
- Middle Mouse Button

IA_Evade:
- Spacebar
```

### 6.3 Bind Input in Character

In `SetupPlayerInputComponent()`:

```cpp
void AYourCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
    {
        // Movement
        EnhancedInput->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AYourCharacter::Move);
        EnhancedInput->BindAction(LookAction, ETriggerEvent::Triggered, this, &AYourCharacter::Look);

        // Combat
        EnhancedInput->BindAction(LightAttackAction, ETriggerEvent::Started,
            this, &AYourCharacter::OnLightAttackStarted);
        EnhancedInput->BindAction(LightAttackAction, ETriggerEvent::Completed,
            this, &AYourCharacter::OnLightAttackCompleted);

        EnhancedInput->BindAction(HeavyAttackAction, ETriggerEvent::Started,
            this, &AYourCharacter::OnHeavyAttackStarted);
        EnhancedInput->BindAction(HeavyAttackAction, ETriggerEvent::Completed,
            this, &AYourCharacter::OnHeavyAttackCompleted);

        EnhancedInput->BindAction(BlockAction, ETriggerEvent::Started,
            this, &AYourCharacter::OnBlockStarted);
        EnhancedInput->BindAction(BlockAction, ETriggerEvent::Completed,
            this, &AYourCharacter::OnBlockCompleted);

        EnhancedInput->BindAction(EvadeAction, ETriggerEvent::Started,
            this, &AYourCharacter::OnEvadeStarted);
    }
}

void AYourCharacter::OnLightAttackStarted(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnLightAttackPressed();
    }
}

void AYourCharacter::OnLightAttackCompleted(const FInputActionValue& Value)
{
    if (CombatComponent)
    {
        CombatComponent->OnLightAttackReleased();
    }
}

// ... (similar for other inputs)
```

### 6.4 Add Mapping Context

In `BeginPlay()`:

```cpp
if (APlayerController* PC = Cast<APlayerController>(GetController()))
{
    if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
        ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
    {
        Subsystem->AddMappingContext(DefaultMappingContext, 0);
    }
}
```

---

## Step 7: Create Animation Blueprint

### 7.1 Create AnimInstance

Create Blueprint class inheriting from `SamuraiAnimInstance`:

1. Right-click → **Animation** → **Animation Blueprint**
2. Choose your character's skeleton
3. Set Parent Class to `SamuraiAnimInstance`
4. Name it `ABP_YourCharacter`

### 7.2 Setup State Machine

In Animation Blueprint Event Graph:

**Variables are automatically updated by `SamuraiAnimInstance`**:
- `CombatState`
- `bIsAttacking`
- `bIsBlocking`
- `Speed`
- `bIsInAir`
- `PosturePercent`

Create state machine with states:
- Idle/Move
- Attacking
- Blocking
- Hit Stunned

**Transitions**:
- Idle → Attacking (when `bIsAttacking == true`)
- Attacking → Idle (when montage completes)
- Any → Blocking (when `bIsBlocking == true`)

### 7.3 Assign to Character

Set character's `Anim Class` to `ABP_YourCharacter`

---

## Step 8: Test!

### 8.1 Enable Debug Visualization

In Character Blueprint or C++:

```cpp
CombatComponent->bDebugDraw = true;
TargetingComponent->bDebugDraw = true;
WeaponComponent->bDebugDraw = true;
```

### 8.2 Place Enemy

1. Duplicate your character BP
2. Add AI Controller
3. Place in level near player

### 8.3 Test Attack Flow

1. Play in editor
2. Press light attack button
3. Verify:
   - Attack animation plays
   - Weapon trace visualized (red lines)
   - Enemy takes damage
   - Posture bar updates

### 8.4 Verify Systems

- **Motion Warping**: Character smoothly rotates toward enemy
- **Hit Detection**: Red sphere traces from weapon, hits register once
- **Combo**: Tap light attack during combo window → chains to next attack
- **Posture**: Hold block while enemy attacks → posture depletes

---

## Step 9: Create Combo Chain

See [ATTACK_CREATION.md](ATTACK_CREATION.md) for detailed combo creation workflow.

**Quick steps**:

1. Create `DA_LightAttack2`
2. In `DA_LightAttack1`:
   - Set `NextComboAttack = DA_LightAttack2`
3. In `DA_LightAttack2`:
   - Set `NextComboAttack = DA_LightAttack3` (or null to end chain)

---

## Step 10: Add Enemy AI (Optional)

See full documentation for AI integration, but quick setup:

```cpp
// In AI Controller
void AYourAIController::AttackPlayer()
{
    if (ACombatCharacter* Character = Cast<ACombatCharacter>(GetPawn()))
    {
        if (Character->CombatComponent)
        {
            Character->CombatComponent->OnLightAttackPressed();
        }
    }
}
```

---

## Next Steps

- **Create more attacks**: See [ATTACK_CREATION.md](ATTACK_CREATION.md)
- **Setup parry system**: Add `AnimNotifyState_ParryWindow` to enemy attacks
- **Add heavy attacks**: Configure charge mechanics
- **Implement finishers**: Check `IsVulnerableToFinisher()` interface method
- **Tune values**: Adjust `CombatSettings` for your game feel

---

## Troubleshooting

See [TROUBLESHOOTING.md](TROUBLESHOOTING.md) for common issues.

**Quick fixes**:

- **Attacks not executing**: Check `CanAttack()` returns true, verify state is Idle
- **Combos not chaining**: Ensure `AnimNotifyState_ComboWindow` is placed, check `NextComboAttack` is set
- **Hits not detecting**: Verify weapon sockets exist, check hit detection enabled during Active phase
- **Parry not working**: Ensure `AnimNotifyState_ParryWindow` on attacker's montage, check defender is blocking

---

**You're now ready to build Ghost of Tsushima-style combat!** 🗡️