// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/AttackData.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogAttackData, Log, All);

UAttackData::UAttackData()
{
    // Default values are already set in header file via inline initialization
    // Only set complex defaults here if needed
}

// ============================================================================
// SECTION & TIMING QUERIES
// ============================================================================

void UAttackData::GetSectionTimeRange(float& OutStart, float& OutEnd) const
{
    OutStart = 0.0f;
    OutEnd = 0.0f;
    
    if (!AttackMontage)
    {
        return;
    }
    
    // If no section specified, use entire montage
    if (MontageSection == NAME_None)
    {
        OutStart = 0.0f;
        OutEnd = AttackMontage->CalculateSequenceLength();
        return;
    }
    
    // Find section index
    const int32 SectionIndex = AttackMontage->GetSectionIndex(MontageSection);
    if (SectionIndex == INDEX_NONE)
    {
        UE_LOG(LogAttackData, Warning, TEXT("%s: MontageSection '%s' not found in montage '%s'"), 
               *GetName(), *MontageSection.ToString(), *AttackMontage->GetName());
        return;
    }
    
    // Get section start time
    OutStart = AttackMontage->GetAnimCompositeSection(SectionIndex).GetTime();
    
    // Find next section or end of montage
    OutEnd = AttackMontage->CalculateSequenceLength();

    // PT-13 FIX: O(1) fast path - check if next section exists and is chronologically ordered
    const int32 NumSections = AttackMontage->CompositeSections.Num();
    if (SectionIndex + 1 < NumSections)
    {
        const float NextSectionStart = AttackMontage->CompositeSections[SectionIndex + 1].GetTime();
        if (NextSectionStart > OutStart)
        {
            // Sections are chronologically ordered - use O(1) lookup
            OutEnd = NextSectionStart;
            return;
        }
    }

    // Fallback: O(N) search for non-chronological section ordering (rare)
    for (int32 i = 0; i < NumSections; ++i)
    {
        if (i != SectionIndex)
        {
            const float OtherSectionStart = AttackMontage->CompositeSections[i].GetTime();
            if (OtherSectionStart > OutStart && OtherSectionStart < OutEnd)
            {
                OutEnd = OtherSectionStart;
            }
        }
    }
}

float UAttackData::GetSectionLength() const
{
    float Start, End;
    GetSectionTimeRange(Start, End);
    return FMath::Max(0.0f, End - Start);
}

// ============================================================================
// NOTIFY TIMING VALIDATION & EXTRACTION
// ============================================================================

bool UAttackData::HasValidNotifyTimingInSection() const
{
    if (!AttackMontage)
    {
        return false;
    }

    float SectionStart, SectionEnd;
    GetSectionTimeRange(SectionStart, SectionEnd);

    bool bHasActiveTransition = false;
    bool bHasRecoveryTransition = false;

    // NEW SYSTEM: Check for AnimNotify_AttackPhaseTransition events
    // We need exactly 2 transition events:
    // 1. Windup → Active transition
    // 2. Active → Recovery transition

    for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
    {
        const float NotifyTime = NotifyEvent.GetTriggerTime();

        // Check if notify is within our section
        if (NotifyTime >= SectionStart && NotifyTime < SectionEnd)
        {
            // Check for new event-based phase transitions
            if (const UAnimNotify_AttackPhaseTransition* TransitionNotify = Cast<UAnimNotify_AttackPhaseTransition>(NotifyEvent.Notify))
            {
                if (TransitionNotify->TransitionToPhase == EAttackPhase::Active)
                {
                    bHasActiveTransition = true;
                }
                else if (TransitionNotify->TransitionToPhase == EAttackPhase::Recovery)
                {
                    bHasRecoveryTransition = true;
                }
            }

            // DEPRECATED: Also check for old AnimNotifyState_AttackPhase for backward compatibility
            if (const UAnimNotifyState_AttackPhase* PhaseNotify = Cast<UAnimNotifyState_AttackPhase>(NotifyEvent.NotifyStateClass))
            {
                // Old system is deprecated but still functional
                // If found, consider it valid (but log deprecation warning)
                static bool bDeprecationWarningLogged = false;
                if (!bDeprecationWarningLogged)
                {
                    UE_LOG(LogTemp, Warning, TEXT("[AttackData] Found deprecated AnimNotifyState_AttackPhase in montage. Please migrate to AnimNotify_AttackPhaseTransition."));
                    bDeprecationWarningLogged = true;
                }
                return true; // Old system found - consider valid for now
            }
        }
    }

    // NEW SYSTEM: Valid if has both required transitions
    return bHasActiveTransition && bHasRecoveryTransition;
}

void UAttackData::GetEffectiveTiming(float& OutWindup, float& OutActive, float& OutRecovery) const
{
    OutWindup = 0.0f;
    OutActive = 0.0f;
    OutRecovery = 0.0f;
    
    if (!AttackMontage)
    {
        UE_LOG(LogAttackData, Warning, TEXT("%s: No AttackMontage assigned"), *GetName());
        return;
    }
    
    // Primary: Try to extract from AnimNotifyStates if enabled
    if (bUseAnimNotifyTiming && HasValidNotifyTimingInSection())
    {
        float SectionStart, SectionEnd;
        GetSectionTimeRange(SectionStart, SectionEnd);
        
        float WindupStart = -1.0f, WindupEnd = -1.0f;
        float ActiveStart = -1.0f, ActiveEnd = -1.0f;
        float RecoveryStart = -1.0f, RecoveryEnd = -1.0f;
        
        for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
        {
            const float NotifyTime = NotifyEvent.GetTriggerTime();
            
            if (NotifyTime >= SectionStart && NotifyTime < SectionEnd)
            {
                if (const UAnimNotifyState_AttackPhase* PhaseNotify = Cast<UAnimNotifyState_AttackPhase>(NotifyEvent.NotifyStateClass))
                {
                    const float StartTime = NotifyEvent.GetTriggerTime();
                    const float EndTime = NotifyEvent.GetEndTriggerTime();
                    
                    switch (PhaseNotify->Phase)
                    {
                        case EAttackPhase::Windup:
                            WindupStart = StartTime;
                            WindupEnd = EndTime;
                            break;
                        case EAttackPhase::Active:
                            ActiveStart = StartTime;
                            ActiveEnd = EndTime;
                            break;
                        case EAttackPhase::Recovery:
                            RecoveryStart = StartTime;
                            RecoveryEnd = EndTime;
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        
        if (WindupStart >= 0.0f && ActiveStart >= 0.0f && RecoveryStart >= 0.0f)
        {
            OutWindup = WindupEnd - WindupStart;
            OutActive = ActiveEnd - ActiveStart;
            OutRecovery = RecoveryEnd - RecoveryStart;
            return;
        }
        
        // If we reach here, bUseAnimNotifyTiming is true but notifies are invalid
        UE_LOG(LogAttackData, Warning, TEXT("%s: bUseAnimNotifyTiming=true but notifies incomplete. Check timing fallback mode."), *GetName());
    }
    
    // Fallback: Use manual timing
    OutWindup = ManualTiming.WindupDuration;
    OutActive = ManualTiming.ActiveDuration;
    OutRecovery = ManualTiming.RecoveryDuration;
}

#if WITH_EDITOR
// ============================================================================
// EDITOR-ONLY METHODS
// Note: Implementation is in KatanaCombatEditor module (AttackDataTools)
// These are just stubs that will be called by the editor customization
// ============================================================================

void UAttackData::AutoCalculateTimingFromSection()
{
    // Implementation in AttackDataTools (editor module)
    // Called by AttackDataCustomization UI
}

bool UAttackData::GenerateNotifiesInSection()
{
    // Implementation in AttackDataTools (editor module)
    // Called by AttackDataCustomization UI
    return false;
}

TArray<UAttackData*> UAttackData::FindOtherUsersOfSection() const
{
    // Implementation in AttackDataTools (editor module)
    // Called by AttackDataCustomization UI
    return TArray<UAttackData*>();
}

bool UAttackData::ValidateMontageSection(FText& OutErrorMessage) const
{
    // Implementation in AttackDataTools (editor module)
    // Called by AttackDataCustomization UI
    OutErrorMessage = FText::FromString(TEXT("Validation only available in editor"));
    return true;
}

FString UAttackData::GetTimingPreviewString() const
{
    // Implementation in AttackDataTools (editor module)
    // Called by AttackDataCustomization UI
    return TEXT("Preview only available in editor");
}

void UAttackData::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    
    const FName PropertyName = PropertyChangedEvent.GetPropertyName();
    
    // If montage changed, validate section still exists
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UAttackData, AttackMontage))
    {
        if (AttackMontage && MontageSection != NAME_None)
        {
            const int32 SectionIndex = AttackMontage->GetSectionIndex(MontageSection);
            if (SectionIndex == INDEX_NONE)
            {
                UE_LOG(LogAttackData, Warning, TEXT("%s: MontageSection '%s' no longer exists in new montage. Resetting to None."), 
                       *GetName(), *MontageSection.ToString());
                MontageSection = NAME_None;
            }
        }
    }
    
    // If timing mode changed to AnimNotify, validate notifies exist
    if (PropertyName == GET_MEMBER_NAME_CHECKED(UAttackData, bUseAnimNotifyTiming))
    {
        if (bUseAnimNotifyTiming && AttackMontage && !HasValidNotifyTimingInSection())
        {
            UE_LOG(LogAttackData, Warning, TEXT("%s: Enabled AnimNotify timing but section lacks required notifies. Use 'Generate Notifies' or set fallback mode."),
                   *GetName());
        }
    }
}

// ============================================================================
// CONTEXT SYSTEM VALIDATION (Phase 1)
// ============================================================================

EDataValidationResult UAttackData::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = EDataValidationResult::Valid;
    TArray<FText> ValidationErrors;

    // Run all validation checks
    TSet<const UAttackData*> Visited;
    const bool bHasCycles = DetectCycles(Visited, ValidationErrors);
    const bool bDirectionalValid = ValidateDirectionalFollowUps(ValidationErrors);
    const bool bTerminalValid = ValidateTerminalTag(ValidationErrors);

    // PT-17 NOTE: String-based error filtering
    // ==========================================
    // This pattern filters errors by checking if the message starts with the asset name.
    // It prevents cascading duplicate errors when validating combo chains (each asset in
    // the chain would otherwise report all errors from all downstream assets).
    //
    // This is a known architectural limitation. A cleaner solution would be to:
    // 1. Have each validation function take FDataValidationContext directly
    // 2. Use Context.AddError() with sub-object associations (Context.GetAssociatedObject())
    // 3. Or pass a struct that tracks which asset "owns" each error
    //
    // The current approach works correctly but relies on error message formatting conventions.
    // If refactoring, ensure error messages from DetectCycles, ValidateDirectionalFollowUps,
    // and ValidateTerminalTag maintain the "AssetName: description" format.
    const FString ThisAssetName = GetName();
    for (const FText& Error : ValidationErrors)
    {
        const FString ErrorString = Error.ToString();
        // Only report if this error is about THIS asset (error message starts with asset name)
        if (ErrorString.StartsWith(ThisAssetName + TEXT(":")))
        {
            Context.AddError(Error);
        }
    }

    // Determine result - but only fail if THIS asset has errors
    // Check if any error was actually reported for this asset
    if (ValidationErrors.Num() > 0)
    {
        // Count how many errors are actually about this asset
        int32 ThisAssetErrors = 0;
        for (const FText& Error : ValidationErrors)
        {
            if (Error.ToString().StartsWith(ThisAssetName + TEXT(":")))
            {
                ThisAssetErrors++;
            }
        }
        
        if (ThisAssetErrors > 0)
        {
            Result = EDataValidationResult::Invalid;
        }
    }

    return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}

bool UAttackData::DetectCycles(TSet<const UAttackData*>& Visited, TArray<FText>& Errors) const
{
    // Check if we've already visited this attack (cycle detected!)
    if (Visited.Contains(this))
    {
        // Cycle detected - this attack references itself through its combo chain
        // Only report the error ONCE at the point where the cycle closes
        Errors.Add(FText::FromString(FString::Printf(
            TEXT("%s: Circular reference detected! This attack is part of a combo cycle. Review NextComboAttack, HeavyComboAttack, and DirectionalFollowUps to break the cycle."),
            *GetName()
        )));
        return true;
    }

    // Add this attack to visited set for this traversal path
    Visited.Add(this);

    bool bFoundCycle = false;

    // Check NextComboAttack
    if (NextComboAttack)
    {
        if (NextComboAttack->DetectCycles(Visited, Errors))
        {
            bFoundCycle = true;
        }
    }

    // Check HeavyComboAttack
    if (HeavyComboAttack)
    {
        if (HeavyComboAttack->DetectCycles(Visited, Errors))
        {
            bFoundCycle = true;
        }
    }

    // Check DirectionalFollowUps
    for (const auto& Pair : DirectionalFollowUps)
    {
        if (Pair.Value)
        {
            if (Pair.Value->DetectCycles(Visited, Errors))
            {
                bFoundCycle = true;
            }
        }
    }

    // Check HeavyDirectionalFollowUps
    for (const auto& Pair : HeavyDirectionalFollowUps)
    {
        if (Pair.Value)
        {
            if (Pair.Value->DetectCycles(Visited, Errors))
            {
                bFoundCycle = true;
            }
        }
    }

    // Remove from visited set to allow branching paths (DAG structure)
    // This allows: A→C, B→C (both A and B can reference C without false positive)
    Visited.Remove(this);

    return bFoundCycle;
}

bool UAttackData::ValidateDirectionalFollowUps(TArray<FText>& Errors) const
{
    bool bIsValid = true;

    // Check if has CanDirectional tag
    const bool bHasCanDirectional = AttackTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Attack.Capability.CanDirectional")));

    // If has CanDirectional tag, must have at least one directional follow-up
    if (bHasCanDirectional)
    {
        const int32 TotalDirectionals = DirectionalFollowUps.Num() + HeavyDirectionalFollowUps.Num();
        if (TotalDirectionals == 0)
        {
            Errors.Add(FText::FromString(FString::Printf(
                TEXT("%s: Has 'Attack.Capability.CanDirectional' tag but no DirectionalFollowUps configured. Either remove tag or add directional attacks."),
                *GetName()
            )));
            bIsValid = false;
        }
    }

    // If has directional follow-ups but NO CanDirectional tag, warn
    if (!bHasCanDirectional)
    {
        const int32 TotalDirectionals = DirectionalFollowUps.Num() + HeavyDirectionalFollowUps.Num();
        if (TotalDirectionals > 0)
        {
            Errors.Add(FText::FromString(FString::Printf(
                TEXT("%s: Has DirectionalFollowUps configured but missing 'Attack.Capability.CanDirectional' tag. Add tag for proper resolution."),
                *GetName()
            )));
            bIsValid = false;
        }
    }

    return bIsValid;
}

bool UAttackData::ValidateTerminalTag(TArray<FText>& Errors) const
{
    bool bIsValid = true;

    // Check if has Terminal tag
    const bool bHasTerminal = AttackTags.HasTag(FGameplayTag::RequestGameplayTag(FName("Attack.Capability.Terminal")));

    if (bHasTerminal)
    {
        // Terminal attacks must NOT have any follow-ups
        if (NextComboAttack)
        {
            Errors.Add(FText::FromString(FString::Printf(
                TEXT("%s: Has 'Attack.Capability.Terminal' tag but NextComboAttack is set. Terminal attacks cannot have follow-ups."),
                *GetName()
            )));
            bIsValid = false;
        }

        if (HeavyComboAttack)
        {
            Errors.Add(FText::FromString(FString::Printf(
                TEXT("%s: Has 'Attack.Capability.Terminal' tag but HeavyComboAttack is set. Terminal attacks cannot have follow-ups."),
                *GetName()
            )));
            bIsValid = false;
        }

        if (DirectionalFollowUps.Num() > 0)
        {
            Errors.Add(FText::FromString(FString::Printf(
                TEXT("%s: Has 'Attack.Capability.Terminal' tag but DirectionalFollowUps are set. Terminal attacks cannot have follow-ups."),
                *GetName()
            )));
            bIsValid = false;
        }

        if (HeavyDirectionalFollowUps.Num() > 0)
        {
            Errors.Add(FText::FromString(FString::Printf(
                TEXT("%s: Has 'Attack.Capability.Terminal' tag but HeavyDirectionalFollowUps are set. Terminal attacks cannot have follow-ups."),
                *GetName()
            )));
            bIsValid = false;
        }
    }

    return bIsValid;
}

#endif // WITH_EDITOR