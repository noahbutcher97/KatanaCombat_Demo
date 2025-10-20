// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/AttackData.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_AttackPhase.h"

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
    
    for (int32 i = 0; i < AttackMontage->CompositeSections.Num(); ++i)
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
    
    bool bHasWindup = false;
    bool bHasActive = false;
    bool bHasRecovery = false;
    
    // Check for required phase notifies in section
    for (const FAnimNotifyEvent& NotifyEvent : AttackMontage->Notifies)
    {
        const float NotifyTime = NotifyEvent.GetTriggerTime();
        
        // Check if notify is within our section
        if (NotifyTime >= SectionStart && NotifyTime < SectionEnd)
        {
            if (const UAnimNotifyState_AttackPhase* PhaseNotify = Cast<UAnimNotifyState_AttackPhase>(NotifyEvent.NotifyStateClass))
            {
                switch (PhaseNotify->Phase)
                {
                    case EAttackPhase::Windup:
                        bHasWindup = true;
                        break;
                    case EAttackPhase::Active:
                        bHasActive = true;
                        break;
                    case EAttackPhase::Recovery:
                        bHasRecovery = true;
                        break;
                    default:
                        break;
                }
            }
        }
    }
    
    return bHasWindup && bHasActive && bHasRecovery;
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

#endif // WITH_EDITOR