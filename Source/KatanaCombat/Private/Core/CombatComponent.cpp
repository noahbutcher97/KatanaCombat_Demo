// Copyright Epic Games, Inc. All Rights Reserved.

#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/DamageableInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimInstance.h"
#include "MotionWarpingComponent.h"
#include "Kismet/KismetMathLibrary.h"

UCombatComponent::UCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
}

void UCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    OwnerCharacter = Cast<ACharacter>(GetOwner());
    if (OwnerCharacter)
    {
        AnimInstance = OwnerCharacter->GetMesh()->GetAnimInstance();
        TargetingComponent = OwnerCharacter->FindComponentByClass<UTargetingComponent>();
        WeaponComponent = OwnerCharacter->FindComponentByClass<UWeaponComponent>();
        MotionWarpingComponent = OwnerCharacter->FindComponentByClass<UMotionWarpingComponent>();
    }

    if (CombatSettings)
    {
        CurrentPosture = CombatSettings->MaxPosture;
    }
}

void UCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    UpdatePosture(DeltaTime);

    // Update hold state (works for both light and heavy)
    if (bIsHolding)
    {
        UpdateHoldTime(DeltaTime);
    }

    // Update playback rate blending for holds
    // PlayRate calculation: 1.0 - Alpha
    // - Alpha 0.0 → PlayRate 1.0 (normal speed)
    // - Alpha 1.0 → PlayRate 0.0 (frozen)
    if (bIsBlendingToHold)
    {
        // Blend TO hold: increment alpha 0.0 → 1.0 (normal → frozen)
        HoldBlendAlpha = FMath::Min(1.0f, HoldBlendAlpha + DeltaTime * HoldBlendSpeed);
        const float PlayRate = 1.0f - HoldBlendAlpha;

        if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
        {
            AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, PlayRate);
        }

        // Check if we've finished blending to hold
        if (HoldBlendAlpha >= 1.0f)
        {
            bIsBlendingToHold = false;
            HoldBlendAlpha = 1.0f;  // Clamp to exactly 1.0

            // Ensure playback rate is exactly 0.0 (frozen)
            if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
            {
                AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, 0.0f);
            }
        }
    }
    else if (bIsBlendingFromHold)
    {
        // Blend FROM hold: decrement alpha 1.0 → 0.0 (frozen → normal)
        HoldBlendAlpha = FMath::Max(0.0f, HoldBlendAlpha - DeltaTime * HoldBlendSpeed);
        const float PlayRate = 1.0f - HoldBlendAlpha;

        if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
        {
            AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, PlayRate);
        }

        // Check if we've finished blending from hold
        if (HoldBlendAlpha <= 0.0f)
        {
            bIsBlendingFromHold = false;
            HoldBlendAlpha = 0.0f;  // Clamp to exactly 0.0

            // Ensure playback rate is exactly 1.0 (normal)
            if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
            {
                AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, 1.0f);
            }
        }
    }
}

// ============================================================================
// INPUT HANDLING - UPDATED FOR QUEUING
// ============================================================================

void UCombatComponent::OnLightAttackPressed()
{
    bLightAttackHeld = true;

    // Safety check to prevent input blocking
    if (!AnimInstance)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Light attack blocked - AnimInstance is null"));
        }
        return;
    }

    // If we can attack freely (idle state), execute default light attack
    if (CanAttack())
    {
        if (DefaultLightAttack)
        {
            CurrentAttackInputType = EInputType::LightAttack;
            ExecuteAttack(DefaultLightAttack);
        }
    }
    // Only buffer if currently attacking (not in hold or other states)
    else if (CurrentState == ECombatState::Attacking)
    {
        // ALWAYS buffer input (responsive path)
        bLightAttackBuffered = true;

        // Tag whether this input occurred during combo window
        if (bCanCombo)
        {
            bLightAttackInComboWindow = true;
            QueueComboInput(EInputType::LightAttack);

            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Light attack buffered DURING combo window"));
            }
        }
        else
        {
            bLightAttackInComboWindow = false;

            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Light attack buffered OUTSIDE combo window"));
            }
        }

        // FIX: ALWAYS interrupt during Recovery phase, regardless of combo window state
        // This ensures input never requires waiting for natural animation completion
        if (CurrentPhase == EAttackPhase::Recovery && CurrentAttackData)
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[RECOVERY INTERRUPT] Light attack during Recovery - interrupting immediately (combo window: %s)"),
                    bCanCombo ? TEXT("open") : TEXT("closed"));
            }

            // Determine which attack to execute based on combo availability
            UAttackData* NextAttack = CurrentAttackData->NextComboAttack;
            if (NextAttack)
            {
                // Combo available - execute combo
                bLightAttackBuffered = false;
                bLightAttackInComboWindow = false;
                ExecuteComboAttackWithHoldTracking(NextAttack, EInputType::LightAttack);
            }
            else if (DefaultLightAttack)
            {
                // No combo available - start fresh with default attack
                bLightAttackBuffered = false;
                bLightAttackInComboWindow = false;
                ResetCombo();
                CurrentAttackInputType = EInputType::LightAttack;
                ExecuteAttack(DefaultLightAttack);
            }
            return;
        }
    }
    // Ignore input if in hold state or blending to prevent buffering during holds
    else if (CurrentState == ECombatState::HoldingLightAttack || bIsHolding || bIsBlendingToHold)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Ignoring light attack input during hold/blend state"));
        }
    }
}

void UCombatComponent::OnLightAttackReleased()
{
    bLightAttackHeld = false;

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[LIGHT INPUT] Light attack RELEASED (Hold: %s, Current input type: %d)"),
            bIsHolding ? TEXT("yes") : TEXT("no"),
            static_cast<int32>(CurrentAttackInputType));
    }

    // FIX: Only release if we're holding a LIGHT attack
    // This prevents heavy attack releases from triggering light release logic
    if (bIsHolding && CurrentAttackInputType == EInputType::LightAttack)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Log, TEXT("[LIGHT INPUT] Releasing held LIGHT attack"));
        }

        // CRITICAL: Capture bHoldWindowExpired state AT MOMENT OF RELEASE
        // This prevents race condition where CloseHoldWindow() might run between
        // this check and ReleaseHeldLight() execution
        const bool bWasWindowExpired = bHoldWindowExpired;
        ReleaseHeldLight(bWasWindowExpired);
    }
    else if (bIsHolding && CurrentAttackInputType != EInputType::LightAttack)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Log, TEXT("[LIGHT INPUT] Ignoring light release - currently holding %s attack"),
                CurrentAttackInputType == EInputType::HeavyAttack ? TEXT("HEAVY") : TEXT("OTHER"));
        }
    }
}

void UCombatComponent::OnHeavyAttackPressed()
{
    bHeavyAttackHeld = true;

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Heavy attack PRESSED (State: %d, Attacking: %s, Hold: %s)"),
            static_cast<int32>(CurrentState),
            CurrentState == ECombatState::Attacking ? TEXT("yes") : TEXT("no"),
            bIsHolding ? TEXT("yes") : TEXT("no"));
    }

    // Safety check to prevent input blocking
    if (!AnimInstance)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Heavy attack blocked - AnimInstance is null"));
        }
        return;
    }

    // Heavy attacks now work exactly like light attacks: immediate execution, no charging
    if (CanAttack())
    {
        if (DefaultHeavyAttack)
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Executing default heavy attack from Idle"));
            }
            CurrentAttackInputType = EInputType::HeavyAttack;
            ExecuteAttack(DefaultHeavyAttack);
        }
    }
    // Only buffer if currently attacking (not in hold or other states)
    else if (CurrentState == ECombatState::Attacking)
    {
        // ALWAYS buffer input (responsive path)
        bHeavyAttackBuffered = true;

        // Tag whether this input occurred during combo window
        if (bCanCombo)
        {
            bHeavyAttackInComboWindow = true;
            QueueComboInput(EInputType::HeavyAttack);

            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Heavy attack buffered DURING combo window"));
            }
        }
        else
        {
            bHeavyAttackInComboWindow = false;

            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Heavy attack buffered OUTSIDE combo window"));
            }
        }

        // FIX: ALWAYS interrupt during Recovery phase, regardless of combo window state
        // This ensures input never requires waiting for natural animation completion
        if (CurrentPhase == EAttackPhase::Recovery && CurrentAttackData)
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[RECOVERY INTERRUPT] Heavy attack during Recovery - interrupting immediately (combo window: %s)"),
                    bCanCombo ? TEXT("open") : TEXT("closed"));
            }

            // Determine which attack to execute based on heavy combo availability
            UAttackData* NextAttack = CurrentAttackData->HeavyComboAttack;
            if (NextAttack)
            {
                // Heavy combo available - execute it
                bHeavyAttackBuffered = false;
                bHeavyAttackInComboWindow = false;
                ExecuteComboAttackWithHoldTracking(NextAttack, EInputType::HeavyAttack);
            }
            else if (CurrentAttackData->NextComboAttack)
            {
                // No heavy-specific combo, use regular combo
                bHeavyAttackBuffered = false;
                bHeavyAttackInComboWindow = false;
                ExecuteComboAttackWithHoldTracking(CurrentAttackData->NextComboAttack, EInputType::HeavyAttack);
            }
            else if (DefaultHeavyAttack)
            {
                // No combo available - start fresh with default heavy attack
                bHeavyAttackBuffered = false;
                bHeavyAttackInComboWindow = false;
                ResetCombo();
                CurrentAttackInputType = EInputType::HeavyAttack;
                ExecuteAttack(DefaultHeavyAttack);
            }
            return;
        }
    }
    // Ignore input if in hold state or blending to prevent buffering during holds
    else if (CurrentState == ECombatState::HoldingLightAttack || bIsHolding || bIsBlendingToHold)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Ignoring heavy attack input during hold/blend state (Current input type: %d)"),
                static_cast<int32>(CurrentAttackInputType));
        }
    }
}

void UCombatComponent::OnHeavyAttackReleased()
{
    bHeavyAttackHeld = false;

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Heavy attack RELEASED (Hold: %s, Current input type: %d)"),
            bIsHolding ? TEXT("yes") : TEXT("no"),
            static_cast<int32>(CurrentAttackInputType));
    }

    // FIX: Only release if we're holding a HEAVY attack
    // This prevents light attack releases from triggering heavy release logic
    if (bIsHolding && CurrentAttackInputType == EInputType::HeavyAttack)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Releasing held HEAVY attack"));
        }

        // CRITICAL: Capture bHoldWindowExpired state AT MOMENT OF RELEASE
        // This prevents race condition where CloseHoldWindow() might run between
        // this check and ReleaseHeldHeavy() execution
        const bool bWasWindowExpired = bHoldWindowExpired;
        ReleaseHeldHeavy(bWasWindowExpired);
    }
    else if (bIsHolding && CurrentAttackInputType != EInputType::HeavyAttack)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HEAVY INPUT] Ignoring heavy release - currently holding %s attack"),
                CurrentAttackInputType == EInputType::LightAttack ? TEXT("LIGHT") : TEXT("OTHER"));
        }
    }
}

void UCombatComponent::OnEvadePressed()
{
    if (CurrentState == ECombatState::Idle || CurrentState == ECombatState::Blocking)
    {
        SetCombatState(ECombatState::Evading);

        // Reset after evade duration
        if (GetWorld())
        {
            FTimerHandle EvadeTimer;
            TWeakObjectPtr<UCombatComponent> WeakThis(this);
            GetWorld()->GetTimerManager().SetTimer(EvadeTimer, [WeakThis]()
            {
                if (WeakThis.IsValid())
                {
                    WeakThis->SetCombatState(ECombatState::Idle);
                }
            }, 0.5f, false);
        }
    }
    else
    {
        // Buffer evade if can't execute now
        bEvadeBuffered = true;
    }
}

void UCombatComponent::OnBlockPressed()
{
    if (CurrentState == ECombatState::Idle)
    {
        // Try to parry first - if no parry opportunity, will default to blocking
        TryParry();
    }
}

void UCombatComponent::OnBlockReleased()
{
    if (CurrentState == ECombatState::Blocking)
    {
        SetCombatState(ECombatState::Idle);
    }
}

void UCombatComponent::SetMovementInput(FVector2D Input)
{
    // CRITICAL: Ignore movement input entirely during hold state
    // Prevents conflicts with frozen montage and AnimInstance locomotion updates
    // Movement input during hold was causing animation state machine conflicts
    if (bIsHolding)
    {
        // Store zero input to ensure clean state
        StoredMovementInput = FVector2D::ZeroVector;
        return;
    }

    StoredMovementInput = Input;
}

// ============================================================================
// ATTACK PHASE CALLBACKS - UPDATED FOR QUEUING AND HOLD
// ============================================================================

void UCombatComponent::OnAttackPhaseBegin(EAttackPhase Phase)
{
    CurrentPhase = Phase;

    switch (Phase)
    {
        case EAttackPhase::Windup:
            // Motion warping could be setup here if needed
            break;

        case EAttackPhase::Active:
            // Enable hit detection during active phase
            if (WeaponComponent)
            {
                WeaponComponent->EnableHitDetection();
            }
            break;

        case EAttackPhase::Recovery:
            // Recovery phase started - combo window is controlled by AnimNotifyState_ComboWindow
            // Do NOT open combo window here - it's animation-driven
            break;

        default:
            break;
    }
}

void UCombatComponent::OnAttackPhaseEnd(EAttackPhase Phase)
{
    switch (Phase)
    {
        case EAttackPhase::Active:
            // Disable hit detection after active phase
            if (WeaponComponent)
            {
                WeaponComponent->DisableHitDetection();
            }

            // Check if we have combo-window-tagged inputs to execute at end of Active phase
            if ((bLightAttackInComboWindow && bLightAttackBuffered) ||
                (bHeavyAttackInComboWindow && bHeavyAttackBuffered))
            {
                if (bDebugDraw)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Executing combo input at Active phase end (snappy path)"));
                }

                // Process the combo immediately - this is the "snappy" path
                ProcessComboWindowInput();
            }
            break;

        case EAttackPhase::Recovery:
            // Recovery complete - process any remaining buffered inputs
            ProcessRecoveryComplete();
            break;

        default:
            break;
    }
}

// ============================================================================
// COMBO QUEUING SYSTEM - NEW
// ============================================================================

void UCombatComponent::QueueComboInput(EInputType InputType)
{
    if (!bCanCombo || !CurrentAttackData)
    {
        return;
    }

    // Add input to buffer for tracking purposes
    ComboInputBuffer.Add(InputType);
    bHasQueuedCombo = true;

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Marked input %d as combo window input"), static_cast<int32>(InputType));
    }
}

void UCombatComponent::ProcessQueuedCombo()
{
    // Check if we have any queued inputs
    if (ComboInputBuffer.Num() > 0 && CurrentAttackData)
    {
        // Get the LATEST valid input from the buffer (last one wins)
        for (int32 i = ComboInputBuffer.Num() - 1; i >= 0; i--)
        {
            EInputType QueuedInput = ComboInputBuffer[i];
            UAttackData* NextAttack = GetComboFromInput(QueuedInput);
            if (NextAttack)
            {
                // Execute the combo attack WITH the proper input type tracking
                ExecuteComboAttackWithHoldTracking(NextAttack, QueuedInput);
                ComboInputBuffer.Empty();
                bHasQueuedCombo = false;
                return;
            }
        }

        // No valid combos found in buffer
        ComboInputBuffer.Empty();
        bHasQueuedCombo = false;
    }

    // No combo queued - return to idle and process any buffered non-combo inputs
    SetCombatState(ECombatState::Idle);
    ProcessBufferedInputs();
}

void UCombatComponent::ProcessComboWindowInput()
{
    // This is called at the END of Active phase for combo-window-tagged inputs
    // Priority: Check which input was buffered during combo window

    if (bLightAttackInComboWindow && bLightAttackBuffered && CurrentAttackData)
    {
        // Clear the flags
        bLightAttackBuffered = false;
        bLightAttackInComboWindow = false;

        // Get the next combo attack
        UAttackData* NextAttack = CurrentAttackData->NextComboAttack;
        if (NextAttack)
        {
            ExecuteComboAttackWithHoldTracking(NextAttack, EInputType::LightAttack);
            return;
        }
    }

    if (bHeavyAttackInComboWindow && bHeavyAttackBuffered && CurrentAttackData)
    {
        // Clear the flags
        bHeavyAttackBuffered = false;
        bHeavyAttackInComboWindow = false;

        // Get the heavy branch combo
        UAttackData* NextAttack = CurrentAttackData->HeavyComboAttack;
        if (NextAttack)
        {
            ExecuteComboAttackWithHoldTracking(NextAttack, EInputType::HeavyAttack);
            return;
        }
    }
}

UAttackData* UCombatComponent::GetComboFromInput(EInputType InputType)
{
    if (!CurrentAttackData)
    {
        return nullptr;
    }

    switch (InputType)
    {
        case EInputType::LightAttack:
            return CurrentAttackData->NextComboAttack;

        case EInputType::HeavyAttack:
            return CurrentAttackData->HeavyComboAttack;

        default:
            return nullptr;
    }
}

void UCombatComponent::ExecuteComboAttackWithHoldTracking(UAttackData* NextAttack, EInputType InputType)
{
    if (!NextAttack)
    {
        return;
    }

    ComboCount++;
    bCanCombo = false;
    CurrentAttackData = NextAttack;

    // CRITICAL: Set the input type that triggered this combo
    // This ensures hold detection works correctly for combo attacks
    CurrentAttackInputType = InputType;

    // Clear combo input tracking since we're executing
    ComboInputBuffer.Empty();
    bHasQueuedCombo = false;

    // FIX: Clear hold state from previous attack to prevent hold interruption handler
    // from firing during combo transitions
    bIsHolding = false;
    bIsInHoldWindow = false;
    bIsBlendingToHold = false;
    bIsBlendingFromHold = false;
    bHoldWindowExpired = false;
    QueuedDirectionalInput = EAttackDirection::None;
    HoldBlendAlpha = 0.0f;
    CurrentHoldTime = 0.0f;

    // Ensure we're in attacking state for combo transitions
    SetCombatState(ECombatState::Attacking);

    // Reset combo timer
    if (GetWorld() && CombatSettings)
    {
        GetWorld()->GetTimerManager().SetTimer(
            ComboResetTimer,
            this,
            &UCombatComponent::ResetComboChain,
            3.0f, // TODO: Add ComboResetDelay to CombatSettings
            false
        );
    }

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatComponent: Executing combo attack %s (Count: %d, Input: %d)"),
            *NextAttack->GetName(), ComboCount, static_cast<int32>(InputType));
    }

    PlayAttackMontage(NextAttack);
}

void UCombatComponent::ExecuteComboAttack(UAttackData* NextAttack)
{
    // Wrapper for backward compatibility - tries to infer input type
    EInputType InferredType = EInputType::None;

    // Try to infer from current buffered states
    if (bLightAttackHeld || bLightAttackBuffered)
    {
        InferredType = EInputType::LightAttack;
    }
    else if (bHeavyAttackHeld || bHeavyAttackBuffered)
    {
        InferredType = EInputType::HeavyAttack;
    }
    else
    {
        // Default to light if we can't infer
        InferredType = EInputType::LightAttack;
    }

    ExecuteComboAttackWithHoldTracking(NextAttack, InferredType);
}

// ============================================================================
// HOLD SYSTEM - UPDATED
// ============================================================================

void UCombatComponent::UpdateHoldTime(float DeltaTime)
{
    if (!CurrentAttackData)
    {
        return;
    }

    CurrentHoldTime += DeltaTime;

    // Don't auto-release on timeout - let the hold window closure handle it
    // This prevents the animation from jumping to next combo on timeout
}

void UCombatComponent::ForceRestoreNormalPlayRate()
{
    if (!AnimInstance)
    {
        return;
    }

    // Restore our tracked montage if it exists
    if (CurrentAttackData && CurrentAttackData->AttackMontage)
    {
        AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, 1.0f);
    }

    // CRITICAL SAFETY: Also restore playrate of ANY currently active montage
    // This handles cases where:
    // - The montage changed during hold
    // - The montage was interrupted by movement input
    // - State machine tried to transition
    UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage();
    if (ActiveMontage)
    {
        AnimInstance->Montage_SetPlayRate(ActiveMontage, 1.0f);

        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Force restored playrate to 1.0 for active montage: %s"),
                *ActiveMontage->GetName());
        }
    }
}

void UCombatComponent::ReleaseHeldLight(bool bWasWindowExpired)
{
    if (!bIsHolding)
    {
        return;
    }

    bIsHolding = false;

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Released held light attack after %.2fs (window expired: %s)"),
            CurrentHoldTime, bWasWindowExpired ? TEXT("true") : TEXT("false"));
    }

    // Re-enable movement
    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    // CRITICAL: Force restore playrate IMMEDIATELY after re-enabling movement
    // This ensures we NEVER have movement enabled while animation is frozen
    // Fixes bug where movement input during hold causes frozen character to move around
    ForceRestoreNormalPlayRate();

    // Check if CurrentAttackData is valid (may be null if animation ended during hold)
    if (!CurrentAttackData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] ReleaseHeldLight: CurrentAttackData is null - attack likely completed during hold"));
        bHoldWindowExpired = false;
        QueuedDirectionalInput = EAttackDirection::None;
        SetCombatState(ECombatState::Idle);
        return;
    }

    // CRITICAL: Check if hold window has EXPIRED (held until timeout)
    // Use captured value to avoid race condition with CloseHoldWindow()
    if (bWasWindowExpired)
    {
        // SCENARIO B: Held until timeout → Try directional followups and combos
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Hold timeout release - trying directional followup (direction: %d)"),
                static_cast<int32>(QueuedDirectionalInput));
        }

        // Priority 1: Execute directional follow-up if it exists
        UAttackData* FollowUp = CurrentAttackData->DirectionalFollowUps.FindRef(QueuedDirectionalInput);
        if (FollowUp)
        {
            // Stop blending and execute directional attack as a combo
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            ExecuteComboAttack(FollowUp);
            return;
        }

        // Priority 2: If no directional follow-up, try to execute next combo attack
        if (CurrentAttackData->NextComboAttack)
        {
            // Stop blending and execute combo
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            ExecuteComboAttack(CurrentAttackData->NextComboAttack);
            return;
        }

        // No followups available, fall through to blend back
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Log, TEXT("[CombatComponent] No directional followups or combos available - blending back"));
        }
    }
    else
    {
        // SCENARIO A: Early release → Skip followups, just finish current attack
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Early release - blending back to finish current attack"));
        }
    }

    // Priority 3: Blend back to normal speed and complete current animation
    // This is reached for:
    // - Early releases (always)
    // - Timeout releases with no followup available (fallback)
    if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
    {
        // Check if montage is still playing
        if (AnimInstance->Montage_IsPlaying(CurrentAttackData->AttackMontage))
        {
            // Start blending back to normal speed (0.0 → 1.0)
            bIsBlendingToHold = false;
            bIsBlendingFromHold = true;
            // HoldBlendAlpha is already at 1.0 from the hold blend

            // Clear hold window flags
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;

            // Return to attacking state to complete current animation
            // After animation ends, ProcessRecoveryComplete will handle next input
            SetCombatState(ECombatState::Attacking);

            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Started blend back to normal speed"));
            }
        }
        else
        {
            // Montage ended during hold - clean up and return to idle
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Light attack montage ended during hold - returning to idle"));
            }

            CurrentAttackData = nullptr;
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            SetCombatState(ECombatState::Idle);
        }
    }
    else
    {
        // No montage to resume - clean up and return to idle
        CurrentAttackData = nullptr;
        bIsBlendingToHold = false;
        bIsBlendingFromHold = false;
        bHoldWindowExpired = false;
        QueuedDirectionalInput = EAttackDirection::None;
        SetCombatState(ECombatState::Idle);
    }
}

void UCombatComponent::ReleaseHeldHeavy(bool bWasWindowExpired)
{
    if (!bIsHolding)
    {
        return;
    }

    bIsHolding = false;

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Released held heavy attack after %.2fs (window expired: %s)"),
            CurrentHoldTime, bWasWindowExpired ? TEXT("true") : TEXT("false"));
    }

    // Re-enable movement
    if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
    {
        OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
    }

    // CRITICAL: Force restore playrate IMMEDIATELY after re-enabling movement
    // This ensures we NEVER have movement enabled while animation is frozen
    // Fixes bug where movement input during hold causes frozen character to move around
    ForceRestoreNormalPlayRate();

    // Check if CurrentAttackData is valid (may be null if animation ended during hold)
    if (!CurrentAttackData)
    {
        UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] ReleaseHeldHeavy: CurrentAttackData is null - attack likely completed during hold"));
        bHoldWindowExpired = false;
        QueuedDirectionalInput = EAttackDirection::None;
        SetCombatState(ECombatState::Idle);
        return;
    }

    // CRITICAL: Check if hold window has EXPIRED (held until timeout)
    // Use captured value to avoid race condition with CloseHoldWindow()
    if (bWasWindowExpired)
    {
        // SCENARIO B: Held until timeout → Try combo attack
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HEAVY RELEASE] Hold timeout release - trying heavy combo"));
        }

        // Try to execute heavy combo attack if available
        if (CurrentAttackData->HeavyComboAttack)
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HEAVY RELEASE] Executing heavy combo attack: %s"),
                    *CurrentAttackData->HeavyComboAttack->GetName());
            }

            // Stop blending and execute combo WITH EXPLICIT HEAVY INPUT TYPE
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            // FIX: Use explicit EInputType::HeavyAttack instead of inference
            ExecuteComboAttackWithHoldTracking(CurrentAttackData->HeavyComboAttack, EInputType::HeavyAttack);
            return;
        }

        // Try regular next combo if no heavy-specific combo
        if (CurrentAttackData->NextComboAttack)
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[HEAVY RELEASE] No heavy-specific combo, executing next combo: %s"),
                    *CurrentAttackData->NextComboAttack->GetName());
            }

            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            // FIX: Use explicit EInputType::HeavyAttack instead of inference
            ExecuteComboAttackWithHoldTracking(CurrentAttackData->NextComboAttack, EInputType::HeavyAttack);
            return;
        }

        // No combos available, fall through to blend back
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[HEAVY RELEASE] No heavy combos available - blending back"));
        }
    }
    else
    {
        // SCENARIO A: Early release → Skip combos, just finish current attack
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Early release - blending back to finish current attack"));
        }
    }

    // Blend back to normal speed and complete current animation
    // This is reached for:
    // - Early releases (always)
    // - Timeout releases with no combo available (fallback)
    if (AnimInstance && CurrentAttackData->AttackMontage)
    {
        if (AnimInstance->Montage_IsPlaying(CurrentAttackData->AttackMontage))
        {
            // Start blending back to normal speed (0.0 → 1.0)
            bIsBlendingToHold = false;
            bIsBlendingFromHold = true;
            // HoldBlendAlpha is already at 1.0 from the hold blend

            // Clear hold window flags
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;

            // Return to attacking state to complete current animation
            SetCombatState(ECombatState::Attacking);

            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Blending heavy attack back to normal speed"));
            }
        }
        else
        {
            // Montage has ended while we were holding - clean up and return to idle
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Heavy attack montage ended during hold - returning to idle"));
            }

            CurrentAttackData = nullptr;
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            SetCombatState(ECombatState::Idle);
        }
    }
    else
    {
        // No montage to resume - clean up and return to idle
        CurrentAttackData = nullptr;
        bIsBlendingToHold = false;
        bIsBlendingFromHold = false;
        bHoldWindowExpired = false;
        QueuedDirectionalInput = EAttackDirection::None;
        SetCombatState(ECombatState::Idle);
    }
}

// ============================================================================
// HEAVY CHARGE SYSTEM
// ============================================================================

void UCombatComponent::UpdateHeavyCharging(float DeltaTime)
{
    if (!CurrentAttackData)
    {
        return;
    }

    CurrentChargeTime += DeltaTime;

    // Auto-release at max charge
    if (CurrentAttackData && CurrentChargeTime >= CurrentAttackData->MaxChargeTime)
    {
        ReleaseChargedHeavy();
    }
}

void UCombatComponent::ReleaseChargedHeavy()
{
    if (!bIsCharging)
    {
        return;
    }

    bIsCharging = false;
    SetCombatState(ECombatState::Attacking);

    // Resume normal playback speed
    if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
    {
        AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, 1.0f);
    }

    if (bDebugDraw && CurrentAttackData)
    {
        const float ChargePercent = FMath::Clamp(CurrentChargeTime / CurrentAttackData->MaxChargeTime, 0.0f, 1.0f);
        UE_LOG(LogTemp, Warning, TEXT("CombatComponent: Released heavy charge at %.1f%% (%.2fs)"), ChargePercent * 100.0f, CurrentChargeTime);
    }
}

// ============================================================================
// DIRECTIONAL FOLLOW-UPS
// ============================================================================

void UCombatComponent::ExecuteDirectionalFollowUp(EAttackDirection Direction)
{
    if (!CurrentAttackData)
    {
        return;
    }

    if (UAttackData* FollowUp = CurrentAttackData->DirectionalFollowUps.FindRef(Direction))
    {
        CurrentAttackData = FollowUp;
        PlayAttackMontage(FollowUp);
    }
}

FVector UCombatComponent::GetWorldSpaceMovementInput() const
{
    if (!OwnerCharacter || StoredMovementInput.IsNearlyZero())
    {
        return FVector::ZeroVector;
    }

    // Convert 2D input to world space direction
    const FRotator ControlRotation = OwnerCharacter->GetControlRotation();
    const FRotator YawRotation(0, ControlRotation.Yaw, 0);

    const FVector ForwardDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
    const FVector RightDir = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

    const FVector WorldDir = (ForwardDir * StoredMovementInput.Y) + (RightDir * StoredMovementInput.X);
    return WorldDir.GetSafeNormal();
}

EAttackDirection UCombatComponent::GetAttackDirectionFromWorldDirection(const FVector& WorldDir) const
{
    if (!OwnerCharacter || WorldDir.IsNearlyZero())
    {
        return EAttackDirection::Forward;
    }

    // Get angle relative to character's forward
    const FVector CharacterForward = OwnerCharacter->GetActorForwardVector();
    const float DotProduct = FVector::DotProduct(CharacterForward, WorldDir);
    const FVector CrossProduct = FVector::CrossProduct(CharacterForward, WorldDir);

    // Determine quadrant
    const float Angle = FMath::RadiansToDegrees(FMath::Acos(DotProduct));
    const bool bIsRight = CrossProduct.Z > 0.0f;

    // 8-directional to 4-directional mapping
    if (Angle < 45.0f)
    {
        return EAttackDirection::Forward;
    }
    else if (Angle > 135.0f)
    {
        return EAttackDirection::Backward;
    }
    else
    {
        return bIsRight ? EAttackDirection::Right : EAttackDirection::Left;
    }
}

// ============================================================================
// COMBO SYSTEM - EXISTING FUNCTIONS
// ============================================================================

void UCombatComponent::OpenComboWindow(float Duration)
{
    // Prevent opening multiple combo windows
    if (bCanCombo)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("CombatComponent: Combo window already open - ignoring duplicate"));
        }
        return;
    }

    bCanCombo = true;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(ComboWindowTimer, this, &UCombatComponent::CloseComboWindow, Duration, false);
    }

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatComponent: Combo window OPENED (%.2fs)"), Duration);
    }
}

void UCombatComponent::CloseComboWindow()
{
    bCanCombo = false;

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatComponent: Combo window CLOSED"));
    }
}

void UCombatComponent::ResetCombo()
{
    ComboCount = 0;
    bCanCombo = false;
    CurrentAttackData = nullptr;
    ComboInputBuffer.Empty();

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ComboResetTimer);
    }
}

void UCombatComponent::ResetComboChain()
{
    ResetCombo();
}

// ============================================================================
// ATTACK EXECUTION
// ============================================================================

bool UCombatComponent::CanAttack() const
{
    // Can only start fresh attacks from Idle
    // Combos are handled through buffering system, NOT through CanAttack
    return CurrentState == ECombatState::Idle;
}

bool UCombatComponent::ExecuteAttack(UAttackData* AttackData)
{
    if (!AttackData)
    {
        return false;
    }

    // ExecuteAttack is ONLY for fresh attacks from Idle
    // Combos use ExecuteComboAttack instead
    if (CurrentState != ECombatState::Idle)
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] ExecuteAttack blocked - not in Idle state (State: %d)"), static_cast<int32>(CurrentState));
        }
        return false;
    }

    CurrentAttackData = AttackData;

    // Infer input type from default attack if not already set
    // (This handles cases where ExecuteAttack is called directly, not from input)
    if (CurrentAttackInputType == EInputType::None)
    {
        if (AttackData == DefaultLightAttack)
        {
            CurrentAttackInputType = EInputType::LightAttack;
        }
        else if (AttackData == DefaultHeavyAttack)
        {
            CurrentAttackInputType = EInputType::HeavyAttack;
        }
    }

    SetCombatState(ECombatState::Attacking);

    // Setup motion warping if needed
    if (AttackData->MotionWarpingConfig.bUseMotionWarping)
    {
        SetupMotionWarping(AttackData);
    }

    // Play montage
    return PlayAttackMontage(AttackData);
}

void UCombatComponent::SetupMotionWarping(UAttackData* AttackData)
{
    if (!MotionWarpingComponent || !TargetingComponent || !AttackData)
    {
        return;
    }

    // Find target in default direction (forward/none)
    AActor* Target = TargetingComponent->FindTarget(EAttackDirection::None);

    if (Target)
    {
        const FMotionWarpingTarget WarpTarget(
            AttackData->MotionWarpingConfig.MotionWarpingTargetName,
            Target->GetActorTransform()
        );
        MotionWarpingComponent->AddOrUpdateWarpTarget(WarpTarget);
    }
}

bool UCombatComponent::PlayAttackMontage(UAttackData* AttackData)
{
    if (!AnimInstance || !AttackData || !AttackData->AttackMontage)
    {
        return false;
    }

    const float PlayRate = 1.0f;
    AnimInstance->Montage_Play(AttackData->AttackMontage, PlayRate);

    if (!AttackData->MontageSection.IsNone())
    {
        AnimInstance->Montage_JumpToSection(AttackData->MontageSection, AttackData->AttackMontage);
    }

    // Set up montage end delegate for cleanup when animation completes or is interrupted
    FOnMontageEnded OnMontageEndDelegate;
    OnMontageEndDelegate.BindLambda([this, AttackDataPtr = AttackData](UAnimMontage* Montage, bool bInterrupted)
    {
        // FIX: Detect intentional interruptions (combo transitions)
        // If the montage that ended is NOT the current attack, this was a combo transition
        // In that case, don't reset to Idle - the new attack is already active
        if (bInterrupted && CurrentAttackData != nullptr && CurrentAttackData != AttackDataPtr)
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Old montage interrupted by new combo - ignoring (intentional)"));
            }
            return; // Combo transition - do nothing
        }

        // Handle natural completion
        if (!bInterrupted && CurrentState == ECombatState::Attacking)
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Attack montage completed naturally - cleaning up"));
            }

            // Natural completion - clean up and return to idle
            CurrentAttackData = nullptr;
            CurrentAttackInputType = EInputType::None;
            CurrentPhase = EAttackPhase::None;
            bLightAttackBuffered = false;
            bHeavyAttackBuffered = false;
            bLightAttackInComboWindow = false;
            bHeavyAttackInComboWindow = false;
            ComboInputBuffer.Empty();
            bHasQueuedCombo = false;

            // Clear hold and blend states if montage ends during hold
            bIsHolding = false;
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            HoldBlendAlpha = 0.0f;

            SetCombatState(ECombatState::Idle);
        }
        // CRITICAL SAFETY: Handle montage interruption during hold state
        else if (bInterrupted && (bIsHolding || bIsBlendingToHold || bIsBlendingFromHold))
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Attack montage interrupted during hold - force cleanup"));
            }

            // Force restore normal montage playback rate for any active montage
            if (AnimInstance)
            {
                UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage();
                if (ActiveMontage)
                {
                    AnimInstance->Montage_SetPlayRate(ActiveMontage, 1.0f);
                }
            }

            // Re-enable movement (in case it was disabled during hold)
            if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
            {
                OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            }

            // Clear all hold-related state
            bIsHolding = false;
            bIsInHoldWindow = false;
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            HoldBlendAlpha = 0.0f;
            CurrentHoldTime = 0.0f;

            // FIX: Clear attack state and return to Idle to prevent lockout
            CurrentAttackData = nullptr;
            CurrentAttackInputType = EInputType::None;
            CurrentPhase = EAttackPhase::None;
            bLightAttackBuffered = false;
            bHeavyAttackBuffered = false;
            bLightAttackInComboWindow = false;
            bHeavyAttackInComboWindow = false;
            ComboInputBuffer.Empty();
            bHasQueuedCombo = false;

            SetCombatState(ECombatState::Idle);
        }
        // FIX: SAFETY - Handle ANY other interruption during Attacking/Holding state
        else if (bInterrupted && (CurrentState == ECombatState::Attacking || CurrentState == ECombatState::HoldingLightAttack))
        {
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Attack montage interrupted (non-hold) - force return to Idle"));
            }

            // Full state cleanup
            CurrentAttackData = nullptr;
            CurrentAttackInputType = EInputType::None;
            CurrentPhase = EAttackPhase::None;
            bLightAttackBuffered = false;
            bHeavyAttackBuffered = false;
            bLightAttackInComboWindow = false;
            bHeavyAttackInComboWindow = false;
            ComboInputBuffer.Empty();
            bHasQueuedCombo = false;

            // Clear hold state (redundant safety)
            bIsHolding = false;
            bIsInHoldWindow = false;
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            HoldBlendAlpha = 0.0f;
            CurrentHoldTime = 0.0f;

            SetCombatState(ECombatState::Idle);
        }
    });
    AnimInstance->Montage_SetEndDelegate(OnMontageEndDelegate, AttackData->AttackMontage);

    return true;
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

void UCombatComponent::SetCombatState(ECombatState NewState)
{
    if (CurrentState == NewState)
    {
        return;
    }

    // Validate the state transition
    if (!CanTransitionTo(NewState))
    {
        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Error, TEXT("CombatComponent: Invalid state transition %d -> %d blocked!"),
                static_cast<int32>(CurrentState), static_cast<int32>(NewState));
        }
        return;
    }

    const ECombatState OldState = CurrentState;
    CurrentState = NewState;

    // CRITICAL SAFETY: Force restore playrate when transitioning OUT of hold state
    // Ensures animation is never frozen when entering any non-hold state
    // Catches edge cases where hold state exits via unexpected paths
    if (OldState == ECombatState::HoldingLightAttack && NewState != ECombatState::HoldingLightAttack)
    {
        ForceRestoreNormalPlayRate();

        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Exited hold state via state transition - forced playrate restore"));
        }
    }

    // CRITICAL SAFETY: Force exit hold state if entering Dead state
    // Prevents frozen corpses and stuck blend states
    if (NewState == ECombatState::Dead)
    {
        if (bIsHolding || bIsBlendingToHold || bIsBlendingFromHold)
        {
            // Force restore normal montage playback rate
            if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
            {
                AnimInstance->Montage_SetPlayRate(CurrentAttackData->AttackMontage, 1.0f);
            }

            // Re-enable movement (in case it was disabled during hold)
            if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
            {
                OwnerCharacter->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
            }

            // Clear all hold-related state
            bIsHolding = false;
            bIsInHoldWindow = false;
            bIsBlendingToHold = false;
            bIsBlendingFromHold = false;
            bHoldWindowExpired = false;
            QueuedDirectionalInput = EAttackDirection::None;
            HoldBlendAlpha = 0.0f;
            CurrentHoldTime = 0.0f;

            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Force exited hold state on death"));
            }
        }
    }

    // Clear all combat flags when entering idle to prevent input blocking
    if (NewState == ECombatState::Idle)
    {
        // Clear all attack-related state
        CurrentAttackData = nullptr;
        CurrentAttackInputType = EInputType::None;
        CurrentPhase = EAttackPhase::None;

        // Clear buffering flags
        bLightAttackBuffered = false;
        bHeavyAttackBuffered = false;
        bLightAttackInComboWindow = false;
        bHeavyAttackInComboWindow = false;
        bEvadeBuffered = false;

        // Clear combo state
        ComboInputBuffer.Empty();
        bHasQueuedCombo = false;
        bCanCombo = false;

        // Clear hold state
        bIsHolding = false;
        bIsInHoldWindow = false;
        CurrentHoldTime = 0.0f;
        bHoldWindowExpired = false;
        QueuedDirectionalInput = EAttackDirection::None;

        // Clear blend states
        bIsBlendingToHold = false;
        bIsBlendingFromHold = false;
        HoldBlendAlpha = 0.0f;

        // Clear other window states
        bIsInParryWindow = false;
        bIsInCounterWindow = false;

        // Clear any pending timers
        if (GetWorld())
        {
            GetWorld()->GetTimerManager().ClearTimer(ComboWindowTimer);
            GetWorld()->GetTimerManager().ClearTimer(HoldWindowTimer);
            GetWorld()->GetTimerManager().ClearTimer(ParryWindowTimer);
            GetWorld()->GetTimerManager().ClearTimer(CounterWindowTimer);
        }

        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Entering Idle - cleared all combat flags"));
        }
    }

    OnCombatStateChanged.Broadcast(NewState);

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Warning, TEXT("CombatComponent: State %d -> %d"), static_cast<int32>(OldState), static_cast<int32>(NewState));
    }
}

bool UCombatComponent::CanTransitionTo(ECombatState NewState) const
{
    // Dead state is terminal - no transitions allowed
    if (CurrentState == ECombatState::Dead)
    {
        return false;
    }

    // Can't transition to the same state
    if (CurrentState == NewState)
    {
        return false;
    }

    // Define valid state transitions
    switch (CurrentState)
    {
        case ECombatState::Idle:
            // From Idle, can transition to any action state
            return NewState == ECombatState::Attacking ||
                   NewState == ECombatState::Blocking ||
                   NewState == ECombatState::Evading ||
                   NewState == ECombatState::HoldingLightAttack ||
                   NewState == ECombatState::ChargingHeavyAttack ||
                   NewState == ECombatState::HitStunned ||
                   NewState == ECombatState::GuardBroken ||
                   NewState == ECombatState::Dead;

        case ECombatState::Attacking:
            // From Attacking, can transition to hold states, hit reactions, or idle
            return NewState == ECombatState::Idle ||
                   NewState == ECombatState::HoldingLightAttack ||
                   NewState == ECombatState::ChargingHeavyAttack ||
                   NewState == ECombatState::HitStunned ||
                   NewState == ECombatState::GuardBroken ||
                   NewState == ECombatState::Dead;

        case ECombatState::HoldingLightAttack:
            // From holding, can return to attacking or idle, or be interrupted
            return NewState == ECombatState::Attacking ||
                   NewState == ECombatState::Idle ||
                   NewState == ECombatState::HitStunned ||
                   NewState == ECombatState::Dead;

        case ECombatState::ChargingHeavyAttack:
            // From charging, can release to attacking or be interrupted
            return NewState == ECombatState::Attacking ||
                   NewState == ECombatState::Idle ||
                   NewState == ECombatState::HitStunned ||
                   NewState == ECombatState::Dead;

        case ECombatState::Blocking:
            // From blocking, can parry, be guard broken, or return to idle
            return NewState == ECombatState::Idle ||
                   NewState == ECombatState::Parrying ||
                   NewState == ECombatState::GuardBroken ||
                   NewState == ECombatState::HitStunned ||
                   NewState == ECombatState::Evading ||
                   NewState == ECombatState::Dead;

        case ECombatState::Parrying:
            // From parrying, return to idle or counter attack
            return NewState == ECombatState::Idle ||
                   NewState == ECombatState::Attacking ||
                   NewState == ECombatState::Dead;

        case ECombatState::GuardBroken:
            // From guard broken, can only recover to idle or die
            return NewState == ECombatState::Idle ||
                   NewState == ECombatState::Finishing ||  // Can be finished during guard break
                   NewState == ECombatState::Dead;

        case ECombatState::Finishing:
            // From finishing, return to idle or die
            return NewState == ECombatState::Idle ||
                   NewState == ECombatState::Dead;

        case ECombatState::HitStunned:
            // From hit stun, recover to idle or be hit again, or die
            return NewState == ECombatState::Idle ||
                   NewState == ECombatState::HitStunned ||  // Can be hit again while stunned
                   NewState == ECombatState::GuardBroken ||
                   NewState == ECombatState::Dead;

        case ECombatState::Evading:
            // From evading, return to idle or attack
            return NewState == ECombatState::Idle ||
                   NewState == ECombatState::Attacking ||
                   NewState == ECombatState::Dead;

        case ECombatState::Dead:
            // Dead is terminal state - no transitions allowed
            return false;

        default:
            // Unknown state - be conservative and deny transition
            return false;
    }
}

bool UCombatComponent::IsAttacking() const
{
    return CurrentState == ECombatState::Attacking ||
           CurrentState == ECombatState::ChargingHeavyAttack ||
           CurrentState == ECombatState::HoldingLightAttack;
}

void UCombatComponent::StopCurrentAttack()
{
    if (AnimInstance && CurrentAttackData && CurrentAttackData->AttackMontage)
    {
        AnimInstance->Montage_Stop(0.2f, CurrentAttackData->AttackMontage);
    }

    CurrentAttackData = nullptr;
    SetCombatState(ECombatState::Idle);
}

void UCombatComponent::StartBlocking()
{
    if (CanBlock())
    {
        SetCombatState(ECombatState::Blocking);
    }
}

void UCombatComponent::StopBlocking()
{
    if (CurrentState == ECombatState::Blocking)
    {
        SetCombatState(ECombatState::Idle);
    }
}

bool UCombatComponent::CanBlock() const
{
    return CurrentState == ECombatState::Idle;
}

void UCombatComponent::TriggerGuardBreak()
{
    HandleGuardBreak();
}

float UCombatComponent::GetPosturePercent() const
{
    const float MaxPosture = GetMaxPosture();
    return MaxPosture > 0.0f ? (CurrentPosture / MaxPosture) : 0.0f;
}

float UCombatComponent::GetCurrentPostureRegenRate() const
{
    if (!CombatSettings)
    {
        return 0.0f;
    }

    switch (CurrentState)
    {
        case ECombatState::Attacking:
        case ECombatState::ChargingHeavyAttack:
        case ECombatState::HoldingLightAttack:
            return CombatSettings->PostureRegenRate_Attacking;

        case ECombatState::Blocking:
            return 0.0f;

        case ECombatState::Idle:
        case ECombatState::Evading:
        default:
            return CombatSettings->PostureRegenRate_Idle;
    }
}

void UCombatComponent::ClearInputBuffers()
{
    ComboInputBuffer.Empty();
    bEvadeBuffered = false;
}

void UCombatComponent::ProcessBufferedInputs()
{
    // Process non-combo buffered inputs
    if (bEvadeBuffered)
    {
        bEvadeBuffered = false;
        OnEvadePressed();
    }
}

void UCombatComponent::ProcessRecoveryComplete()
{
    // This is called at END of Recovery phase
    // Only process NON-combo-window inputs (combo window inputs were handled at Active phase end)

    // Check buffered inputs that were NOT during combo window (RESPONSIVE PATH)
    if (bLightAttackBuffered && !bLightAttackInComboWindow)
    {
        bLightAttackBuffered = false;
        bLightAttackInComboWindow = false;

        // Get next combo attack if available
        if (CurrentAttackData && CurrentAttackData->NextComboAttack)
        {
            ExecuteComboAttackWithHoldTracking(CurrentAttackData->NextComboAttack, EInputType::LightAttack);
        }
        else if (DefaultLightAttack)
        {
            // No combo available, start fresh with default attack
            ResetCombo();
            CurrentAttackInputType = EInputType::LightAttack;
            ExecuteAttack(DefaultLightAttack);
        }
        return;
    }

    if (bHeavyAttackBuffered && !bHeavyAttackInComboWindow)
    {
        bHeavyAttackBuffered = false;
        bHeavyAttackInComboWindow = false;

        // Get heavy branch if available
        if (CurrentAttackData && CurrentAttackData->HeavyComboAttack)
        {
            ExecuteComboAttackWithHoldTracking(CurrentAttackData->HeavyComboAttack, EInputType::HeavyAttack);
        }
        else if (DefaultHeavyAttack)
        {
            // No heavy combo, start fresh with default heavy
            ResetCombo();
            CurrentAttackInputType = EInputType::HeavyAttack;
            ExecuteAttack(DefaultHeavyAttack);
        }
        return;
    }

    // Check other buffered inputs
    if (bEvadeBuffered)
    {
        bEvadeBuffered = false;
        OnEvadePressed();
        return;
    }

    // No buffered input or all inputs were already processed - continue current animation
    // Animation might not be finished yet (could have hold windows after recovery)
    // Clear only the input buffers, not CurrentAttackInputType
    bLightAttackBuffered = false;
    bHeavyAttackBuffered = false;
    bLightAttackInComboWindow = false;
    bHeavyAttackInComboWindow = false;
    ComboInputBuffer.Empty();
    bHasQueuedCombo = false;
    // Don't set to idle - let the animation finish naturally
}

// ============================================================================
// POSTURE SYSTEM
// ============================================================================

void

UCombatComponent::UpdatePosture(float DeltaTime)
{
    if (!CombatSettings || CurrentState == ECombatState::GuardBroken)
    {
        return;
    }

    float RegenRate = 0.0f;

    switch (CurrentState)
    {
        case ECombatState::Attacking:
        case ECombatState::ChargingHeavyAttack:
        case ECombatState::HoldingLightAttack:
            RegenRate = CombatSettings->PostureRegenRate_Attacking;
            break;

        case ECombatState::Blocking:
            RegenRate = 0.0f;
            break;

        case ECombatState::Idle:
        case ECombatState::Evading:
        default:
            RegenRate = CombatSettings->PostureRegenRate_Idle;
            break;
    }

    if (RegenRate > 0.0f)
    {
        RestorePosture(RegenRate * DeltaTime);
    }
}

float UCombatComponent::GetMaxPosture() const
{
    return CombatSettings ? CombatSettings->MaxPosture : 100.0f;
}

bool UCombatComponent::ApplyPostureDamage(float Amount)
{
    CurrentPosture = FMath::Max(0.0f, CurrentPosture - Amount);

    if (CurrentPosture <= 0.0f)
    {
        HandleGuardBreak();
        return true;
    }

    return false;
}

void UCombatComponent::RestorePosture(float Amount)
{
    const float MaxPosture = GetMaxPosture();
    CurrentPosture = FMath::Min(MaxPosture, CurrentPosture + Amount);
}

void UCombatComponent::HandleGuardBreak()
{
    SetCombatState(ECombatState::GuardBroken);
    OnGuardBroken.Broadcast();

    if (GetWorld() && CombatSettings)
    {
        GetWorld()->GetTimerManager().SetTimer(
            GuardBreakRecoveryTimer,
            this,
            &UCombatComponent::RecoverFromGuardBreak,
            CombatSettings->GuardBreakStunDuration,
            false
        );
    }
}

void UCombatComponent::RecoverFromGuardBreak()
{
    if (CombatSettings)
    {
        RestorePosture(CombatSettings->MaxPosture * CombatSettings->GuardBreakRecoveryPercent);
    }

    SetCombatState(ECombatState::Idle);
}

// ============================================================================
// COUNTER SYSTEM
// ============================================================================

void UCombatComponent::OpenCounterWindow(float Duration)
{
    bIsInCounterWindow = true;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(CounterWindowTimer, this, &UCombatComponent::CloseCounterWindow, Duration, false);
    }
}

void UCombatComponent::CloseCounterWindow()
{
    bIsInCounterWindow = false;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(CounterWindowTimer);
    }
}

// ============================================================================
// PARRY WINDOW SYSTEM
// ============================================================================

void UCombatComponent::OpenParryWindow(float Duration)
{
    bIsInParryWindow = true;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(ParryWindowTimer, this, &UCombatComponent::CloseParryWindow, Duration, false);
    }

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Opened parry window for %f seconds"), Duration);
    }
}

void UCombatComponent::CloseParryWindow()
{
    bIsInParryWindow = false;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(ParryWindowTimer);
    }

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Closed parry window"));
    }
}

// ============================================================================
// HOLD WINDOW SYSTEM
// ============================================================================

void UCombatComponent::OpenHoldWindow(float Duration)
{
    bIsInHoldWindow = true;

    // Initialize hold window state
    bHoldWindowExpired = false;
    QueuedDirectionalInput = EAttackDirection::None;

    // Check if the SAME input type that queued this attack is STILL HELD
    bool bInputStillHeld = false;

    if (CurrentAttackInputType == EInputType::LightAttack && bLightAttackHeld)
    {
        bInputStillHeld = true;
    }
    else if (CurrentAttackInputType == EInputType::HeavyAttack && bHeavyAttackHeld)
    {
        bInputStillHeld = true;
    }

    // If the correct input is still held, enter hold state
    if (bInputStillHeld && CurrentAttackData && CurrentAttackData->bCanHold)
    {
        // Enter hold state
        bIsHolding = true;
        CurrentHoldTime = 0.0f;

        // CRITICAL: Clear buffered inputs to prevent them from triggering after hold release
        // This prevents the combo from cycling when holding the button continuously
        bLightAttackBuffered = false;
        bHeavyAttackBuffered = false;
        bHasQueuedCombo = false;
        ComboInputBuffer.Empty();

        // Set state based on input type (both use same hold state for simplicity)
        if (CurrentAttackInputType == EInputType::LightAttack)
        {
            SetCombatState(ECombatState::HoldingLightAttack);
        }
        else if (CurrentAttackInputType == EInputType::HeavyAttack)
        {
            SetCombatState(ECombatState::HoldingLightAttack); // Use same hold state for both
        }

        // Start smooth blend to hold (1.0 → 0.0 playback rate)
        bIsBlendingToHold = true;
        bIsBlendingFromHold = false;
        HoldBlendAlpha = 0.0f;

        // Lock movement during hold
        if (OwnerCharacter && OwnerCharacter->GetCharacterMovement())
        {
            OwnerCharacter->GetCharacterMovement()->DisableMovement();
        }

        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Entered hold state (input type %d still held during hold window)"),
                static_cast<int32>(CurrentAttackInputType));
        }
    }
    else if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Hold window opened but input not held - continuing normal combo (Input type: %d, Still held: %s)"),
            static_cast<int32>(CurrentAttackInputType), bInputStillHeld ? TEXT("true") : TEXT("false"));
    }

    // Set timer to close hold window
    if (GetWorld())
    {
        GetWorld()->GetTimerManager().SetTimer(
            HoldWindowTimer,
            this,
            &UCombatComponent::CloseHoldWindow,
            Duration,
            false
        );
    }

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Opened hold window for %.2fs"), Duration);
    }
}

void UCombatComponent::CloseHoldWindow()
{
    bIsInHoldWindow = false;

    if (GetWorld())
    {
        GetWorld()->GetTimerManager().ClearTimer(HoldWindowTimer);
    }

    // If button is still held when window closes, mark as "held until timeout"
    // This enables directional followups when button is eventually released
    if (bIsHolding)
    {
        bHoldWindowExpired = true;

        // Sample directional input NOW at moment of timeout
        const FVector WorldInput = GetWorldSpaceMovementInput();
        if (!WorldInput.IsNearlyZero())
        {
            QueuedDirectionalInput = GetAttackDirectionFromWorldDirection(WorldInput);
        }
        else
        {
            QueuedDirectionalInput = EAttackDirection::Forward; // Default forward
        }

        if (bDebugDraw)
        {
            UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] Hold window expired - queued direction: %d"),
                static_cast<int32>(QueuedDirectionalInput));
        }
    }

    // IMPORTANT: Do NOT auto-resume when window closes
    // The hold should remain frozen indefinitely until button is released
    // This prevents the animation from jumping to next combo on window timeout

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatComponent] Closed hold window (hold state: %s, expired: %s)"),
            bIsHolding ? TEXT("still holding") : TEXT("not holding"),
            bHoldWindowExpired ? TEXT("true") : TEXT("false"));
    }
}

// ============================================================================
// PARRY SYSTEM (Defender-Side Detection)
// ============================================================================

bool UCombatComponent::TryParry()
{
    if (!TargetingComponent || !OwnerCharacter)
    {
        return false;
    }

    // Get default parry window duration from settings
    const float ParryWindowDuration = CombatSettings ? CombatSettings->ParryWindow : 0.3f;

    // Find all nearby enemies in range
    TArray<AActor*> NearbyEnemies;
    TargetingComponent->GetAllTargetsInRange(NearbyEnemies);

    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatComponent] TryParry: Found %d nearby enemies"), NearbyEnemies.Num());
    }

    // Check each enemy to see if they're in parry window
    for (AActor* Enemy : NearbyEnemies)
    {
        if (!Enemy || Enemy == OwnerCharacter)
        {
            continue;
        }

        // Check if enemy implements combat interface
        ICombatInterface* EnemyCombat = Cast<ICombatInterface>(Enemy);
        if (!EnemyCombat)
        {
            continue;
        }

        // Check if enemy is in parry window (CRITICAL: Attacker-side state)
        if (ICombatInterface::Execute_IsInParryWindow(Enemy))
        {
            // SUCCESS: Perfect parry!
            if (bDebugDraw)
            {
                UE_LOG(LogTemp, Warning, TEXT("[CombatComponent] PARRY SUCCESS on %s!"), *Enemy->GetName());
            }

            // Transition to parrying state
            SetCombatState(ECombatState::Parrying);

            // Fully restore parry executor's posture (reward for successful parry)
            CurrentPosture = GetMaxPosture();
            OnPostureChanged.Broadcast(CurrentPosture);

            // Apply posture damage to attacker (punish failed attack)
            const float ParryPostureDamage = CombatSettings ? CombatSettings->ParryPostureDamage : 40.0f;
            if (IDamageableInterface* EnemyDamageable = Cast<IDamageableInterface>(Enemy))
            {
                IDamageableInterface::Execute_ApplyPostureDamage(Enemy, ParryPostureDamage, OwnerCharacter);
            }

            // Open counter window on attacker (they're vulnerable now)
            const float CounterDuration = CombatSettings ? CombatSettings->CounterWindowDuration : 1.5f;
            if (IDamageableInterface* EnemyDamageable = Cast<IDamageableInterface>(Enemy))
            {
                IDamageableInterface::Execute_OpenCounterWindow(Enemy, CounterDuration);
            }

            // Notify attacker they were parried (for animation/feedback)
            if (IDamageableInterface* EnemyDamageable = Cast<IDamageableInterface>(Enemy))
            {
                IDamageableInterface::Execute_OnAttackParried(Enemy, OwnerCharacter);
            }

            // Broadcast parry success event
            OnPerfectParry.Broadcast(Enemy);

            // Return to idle after brief parry animation
            if (GetWorld())
            {
                FTimerHandle ParryRecoveryTimer;
                TWeakObjectPtr<UCombatComponent> WeakThis(this);
                GetWorld()->GetTimerManager().SetTimer(ParryRecoveryTimer, [WeakThis]()
                {
                    if (WeakThis.IsValid() && WeakThis->CurrentState == ECombatState::Parrying)
                    {
                        WeakThis->SetCombatState(ECombatState::Idle);
                    }
                }, 0.3f, false); // Brief parry recovery window
            }

            return true;
        }
    }

    // FAIL: No enemy in parry window - this is just a normal block
    if (bDebugDraw)
    {
        UE_LOG(LogTemp, Log, TEXT("[CombatComponent] TryParry: No enemies in parry window - defaulting to block"));
    }

    StartBlocking();
    return false;
}