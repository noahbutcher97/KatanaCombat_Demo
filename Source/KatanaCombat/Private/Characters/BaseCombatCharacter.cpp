// Copyright Epic Games, Inc. All Rights Reserved.

#include "Characters/BaseCombatCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "MotionWarpingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ABaseCombatCharacter::ABaseCombatCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // Create combat components
    CombatComponent = CreateDefaultSubobject<UCombatComponent>(TEXT("CombatComponent"));
    TargetingComponent = CreateDefaultSubobject<UTargetingComponent>(TEXT("TargetingComponent"));
    WeaponComponent = CreateDefaultSubobject<UWeaponComponent>(TEXT("WeaponComponent"));
    HitReactionComponent = CreateDefaultSubobject<UHitReactionComponent>(TEXT("HitReactionComponent"));
    MotionWarpingComponent = CreateDefaultSubobject<UMotionWarpingComponent>(TEXT("MotionWarpingComponent"));

    // Initialize health
    CurrentHealth = MaxHealth;
}

void ABaseCombatCharacter::BeginPlay()
{
    Super::BeginPlay();

    // Initialize health to max
    CurrentHealth = MaxHealth;

    // Bind to weapon hit event for damage processing
    if (WeaponComponent)
    {
        WeaponComponent->OnWeaponHit.AddDynamic(this, &ABaseCombatCharacter::OnWeaponHitTarget);
    }
}

// ============================================================================
// HEALTH UTILITIES
// ============================================================================

float ABaseCombatCharacter::ModifyHealth(float Delta, AActor* DamageInstigator)
{
    const float OldHealth = CurrentHealth;
    CurrentHealth = FMath::Clamp(CurrentHealth + Delta, 0.0f, MaxHealth);
    const float ActualDelta = CurrentHealth - OldHealth;

    if (!FMath::IsNearlyZero(ActualDelta))
    {
        OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

        if (CurrentHealth <= 0.0f && OldHealth > 0.0f)
        {
            HandleDeath(DamageInstigator);
        }
    }

    return ActualDelta;
}

void ABaseCombatCharacter::SetHealth(float NewHealth, AActor* DamageInstigator)
{
    const float Delta = NewHealth - CurrentHealth;
    ModifyHealth(Delta, DamageInstigator);
}

void ABaseCombatCharacter::HandleDeath_Implementation(AActor* Killer)
{
    OnCharacterDeath.Broadcast(Killer);

    // Disable collision and movement
    if (GetCharacterMovement())
    {
        GetCharacterMovement()->DisableMovement();
    }

    // Disable combat
    if (CombatComponent)
    {
        CombatComponent->SetComponentTickEnabled(false);
    }
}

// ============================================================================
// ITeamMemberInterface IMPLEMENTATION
// ============================================================================

ETeamId ABaseCombatCharacter::GetTeamId_Implementation() const
{
    return TeamId;
}

bool ABaseCombatCharacter::IsHostileTo_Implementation(AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    // If other doesn't implement team interface, assume hostile
    if (!Other->Implements<UTeamMemberInterface>())
    {
        return true;
    }

    const ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(Other);

    // Neutrals are hostile to everyone except neutrals
    if (TeamId == ETeamId::Neutral)
    {
        return OtherTeam != ETeamId::Neutral;
    }

    // Player is hostile to enemies, enemies are hostile to player
    if (TeamId == ETeamId::Player)
    {
        return OtherTeam == ETeamId::Enemy;
    }

    if (TeamId == ETeamId::Enemy)
    {
        return OtherTeam == ETeamId::Player || OtherTeam == ETeamId::Ally;
    }

    // Allies are hostile to enemies
    if (TeamId == ETeamId::Ally)
    {
        return OtherTeam == ETeamId::Enemy;
    }

    return false;
}

bool ABaseCombatCharacter::IsFriendlyTo_Implementation(AActor* Other) const
{
    if (!Other)
    {
        return false;
    }

    // If other doesn't implement team interface, not friendly
    if (!Other->Implements<UTeamMemberInterface>())
    {
        return false;
    }

    const ETeamId OtherTeam = ITeamMemberInterface::Execute_GetTeamId(Other);

    // Same team is always friendly
    if (TeamId == OtherTeam)
    {
        return true;
    }

    // Player and ally are friendly to each other
    if ((TeamId == ETeamId::Player && OtherTeam == ETeamId::Ally) ||
        (TeamId == ETeamId::Ally && OtherTeam == ETeamId::Player))
    {
        return true;
    }

    return false;
}

// ============================================================================
// IDamageableInterface IMPLEMENTATION (Health Queries)
// ============================================================================

float ABaseCombatCharacter::GetCurrentHealth_Implementation() const
{
    return CurrentHealth;
}

float ABaseCombatCharacter::GetMaxHealth_Implementation() const
{
    return MaxHealth;
}

bool ABaseCombatCharacter::IsAlive_Implementation() const
{
    return CurrentHealth > 0.0f;
}

// ============================================================================
// IDamageableInterface IMPLEMENTATION (Combat)
// ============================================================================

float ABaseCombatCharacter::ApplyDamage_Implementation(const FHitReactionInfo& HitInfo)
{
    if (!HitReactionComponent)
    {
        return 0.0f;
    }

    // Not blocking - take full damage
    const float DamageDealt = HitReactionComponent->ApplyDamage(HitInfo);

    // Modify health by damage amount
    ModifyHealth(-DamageDealt, HitInfo.Attacker);

    return DamageDealt;
}

bool ABaseCombatCharacter::ApplyPostureDamage_Implementation(float PostureDamage, AActor* Attacker)
{
    // TODO: Migrate posture system
    return false;
}

bool ABaseCombatCharacter::CanBeDamaged_Implementation() const
{
    return HitReactionComponent ? HitReactionComponent->CanBeDamaged() : (CurrentHealth > 0.0f);
}

bool ABaseCombatCharacter::IsBlocking_Implementation() const
{
    // TODO: Migrate blocking system
    return false;
}

bool ABaseCombatCharacter::IsGuardBroken_Implementation() const
{
    // TODO: Migrate guard break system
    return false;
}

bool ABaseCombatCharacter::ExecuteFinisher_Implementation(AActor* Attacker, UAttackData* FinisherData)
{
    if (!HitReactionComponent || !FinisherData)
    {
        return false;
    }

    // Check if we're in a finishable state (guard broken or stunned)
    if (!IsGuardBroken_Implementation() && (!HitReactionComponent || !HitReactionComponent->IsStunned()))
    {
        return false;
    }

    // Play victim animation
    const FName FinisherName = FinisherData->MontageSection;
    return HitReactionComponent->PlayFinisherVictimAnimation(FinisherName);
}

void ABaseCombatCharacter::OnAttackParried_Implementation(AActor* Parrier)
{
    // Play parried reaction animation
    if (HitReactionComponent)
    {
        HitReactionComponent->PlayGuardBrokenReaction();
    }
}

void ABaseCombatCharacter::OpenCounterWindow_Implementation(float Duration)
{
    // TODO: Migrate counter window system
}

float ABaseCombatCharacter::GetCurrentPosture_Implementation() const
{
    // TODO: Migrate posture system
    return 0.0f;
}

float ABaseCombatCharacter::GetMaxPosture_Implementation() const
{
    // TODO: Migrate posture system
    return 100.0f;
}

bool ABaseCombatCharacter::IsInCounterWindow_Implementation() const
{
    // TODO: Migrate counter window system
    return false;
}

// ============================================================================
// ICombatInterface IMPLEMENTATION
// ============================================================================

bool ABaseCombatCharacter::CanPerformAttack_Implementation() const
{
    // TODO: Delegate to CombatComponent when migrated
    return CombatComponent != nullptr && IsAlive_Implementation();
}

ECombatState ABaseCombatCharacter::GetCombatState_Implementation() const
{
    // TODO: Delegate to CombatComponent when migrated
    return ECombatState::Idle;
}

bool ABaseCombatCharacter::IsAttacking_Implementation() const
{
    // TODO: Delegate to CombatComponent when migrated
    return false;
}

UAttackData* ABaseCombatCharacter::GetCurrentAttack_Implementation() const
{
    // TODO: Delegate to CombatComponent when migrated
    return nullptr;
}

EAttackPhase ABaseCombatCharacter::GetCurrentPhase_Implementation() const
{
    // TODO: Delegate to CombatComponent when migrated
    return EAttackPhase::None;
}

void ABaseCombatCharacter::OnEnableHitDetection_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->EnableHitDetection();
    }
}

void ABaseCombatCharacter::OnDisableHitDetection_Implementation()
{
    if (WeaponComponent)
    {
        WeaponComponent->DisableHitDetection();
    }
}

void ABaseCombatCharacter::OnAttackPhaseBegin_Implementation(EAttackPhase Phase)
{
    // Legacy notify state - phase transitions now use OnAttackPhaseTransition
}

void ABaseCombatCharacter::OnAttackPhaseEnd_Implementation(EAttackPhase Phase)
{
    // Legacy notify state - phase transitions now use OnAttackPhaseTransition
}

void ABaseCombatCharacter::OnAttackPhaseTransition_Implementation(EAttackPhase NewPhase)
{
    if (CombatComponent)
    {
        CombatComponent->OnPhaseTransition(NewPhase);
    }
}

bool ABaseCombatCharacter::IsInParryWindow_Implementation() const
{
    // TODO: Migrate parry window system
    return false;
}

void ABaseCombatCharacter::OnHoldWindowStart_Implementation(EInputType InputType)
{
    if (CombatComponent)
    {
        CombatComponent->OnHoldWindowStart(InputType);
    }
}

// ============================================================================
// WEAPON HIT PROCESSING
// ============================================================================

void ABaseCombatCharacter::OnWeaponHitTarget(AActor* HitActor, const FHitResult& HitResult, UAttackData* AttackData)
{
    if (!HitActor || !AttackData)
    {
        return;
    }

    // Check if target implements IDamageableInterface
    if (HitActor->Implements<UDamageableInterface>())
    {
        // Build hit reaction info
        FHitReactionInfo HitInfo;
        HitInfo.Attacker = this;
        HitInfo.HitDirection = (HitActor->GetActorLocation() - GetActorLocation()).GetSafeNormal();
        HitInfo.AttackData = AttackData;
        HitInfo.Damage = AttackData->BaseDamage;
        HitInfo.StunDuration = AttackData->HitStunDuration;
        HitInfo.bWasCounter = false; // TODO: Counter window not migrated yet
        HitInfo.ImpactPoint = HitResult.ImpactPoint;

        // Apply damage via interface
        IDamageableInterface::Execute_ApplyDamage(HitActor, HitInfo);
    }
}
