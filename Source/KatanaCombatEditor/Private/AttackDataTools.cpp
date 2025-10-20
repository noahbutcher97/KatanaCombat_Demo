// Copyright Epic Games, Inc. All Rights Reserved.

#include "AttackDataTools.h"
#include "Data/AttackData.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_AttackPhase.h"
#include "Animation/AnimNotifyState_ComboWindow.h"
#include "Animation/AnimNotifyState_HoldWindow.h"
#include "Animation/AnimNotify_ToggleHitDetection.h"
#include "CombatTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"

#define LOCTEXT_NAMESPACE "FAttackDataTools"

// ============================================================================
// TIMING CALCULATION
// ============================================================================

bool UAttackDataTools::AutoCalculateTiming(UAttackData* AttackData)
{
    if (!AttackData || !AttackData->AttackMontage)
    {
        LogToolMessage(TEXT("AutoCalculateTiming: Invalid AttackData or Montage"), true);
        return false;
    }

    const float SectionLength = AttackData->GetSectionLength();
    if (SectionLength <= 0.0f)
    {
        LogToolMessage(TEXT("AutoCalculateTiming: Invalid section length"), true);
        return false;
    }

    // Get default percentages based on attack type
    float WindupPercent, ActivePercent, RecoveryPercent;
    GetDefaultTimingPercentages(AttackData->AttackType, WindupPercent, ActivePercent, RecoveryPercent);

    // Calculate durations
    FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
    Timing.WindupDuration = SectionLength * WindupPercent;
    Timing.ActiveDuration = SectionLength * ActivePercent;
    Timing.RecoveryDuration = SectionLength * RecoveryPercent;

    // Configure hold window for light attacks
    if (AttackData->AttackType == EAttackType::Light && AttackData->bCanHold)
    {
        Timing.HoldWindowStart = Timing.WindupDuration + Timing.ActiveDuration;
        Timing.HoldWindowDuration = SectionLength * 0.1f; // 10% for hold
    }

    AttackData->MarkPackageDirty();
    LogToolMessage(FString::Printf(TEXT("AutoCalculateTiming: Success for %s"), *AttackData->GetName()));
    
    return true;
}

bool UAttackDataTools::ExtractTimingFromNotifies(UAttackData* AttackData, float& OutWindup, float& OutActive, float& OutRecovery)
{
    OutWindup = OutActive = OutRecovery = 0.0f;

    if (!AttackData || !AttackData->AttackMontage)
    {
        return false;
    }

    const UAnimMontage* Montage = AttackData->AttackMontage;
    float SectionStart, SectionEnd;
    AttackData->GetSectionTimeRange(SectionStart, SectionEnd);

    float WindupStart = -1.0f, WindupEnd = -1.0f;
    float ActiveStart = -1.0f, ActiveEnd = -1.0f;
    float RecoveryStart = -1.0f, RecoveryEnd = -1.0f;

    // Find phase notifies in section
    for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
    {
        const float NotifyTime = NotifyEvent.GetTriggerTime();
        if (NotifyTime < SectionStart || NotifyTime >= SectionEnd)
        {
            continue;
        }

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

    // Validate we found required phases
    if (WindupStart < 0.0f || ActiveStart < 0.0f || RecoveryStart < 0.0f)
    {
        return false;
    }

    OutWindup = WindupEnd - WindupStart;
    OutActive = ActiveEnd - ActiveStart;
    OutRecovery = RecoveryEnd - RecoveryStart;

    return true;
}

bool UAttackDataTools::GetTimingPercentages(UAttackData* AttackData, float& OutWindupPercent, float& OutActivePercent, float& OutRecoveryPercent)
{
    OutWindupPercent = OutActivePercent = OutRecoveryPercent = 0.0f;

    if (!AttackData)
    {
        return false;
    }

    const float SectionLength = AttackData->GetSectionLength();
    if (SectionLength <= 0.0f)
    {
        return false;
    }

    const FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
    OutWindupPercent = Timing.WindupDuration / SectionLength;
    OutActivePercent = Timing.ActiveDuration / SectionLength;
    OutRecoveryPercent = Timing.RecoveryDuration / SectionLength;

    return true;
}

// ============================================================================
// ANIMNOTIFY GENERATION
// ============================================================================

bool UAttackDataTools::GenerateAttackPhaseNotifies(UAttackData* AttackData)
{
    if (!AttackData || !AttackData->AttackMontage)
    {
        LogToolMessage(TEXT("GenerateAttackPhaseNotifies: Invalid AttackData or Montage"), true);
        return false;
    }

    // Ensure timing is calculated
    if (AttackData->ManualTiming.WindupDuration <= 0.0f)
    {
        if (!AutoCalculateTiming(AttackData))
        {
            return false;
        }
    }

    UAnimMontage* Montage = AttackData->AttackMontage;
    const FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
    float SectionStart, SectionEnd;
    AttackData->GetSectionTimeRange(SectionStart, SectionEnd);

    // Remove existing phase notifies
    RemoveNotifiesOfType(Montage, AttackData->MontageSection, UAnimNotifyState_AttackPhase::StaticClass());

    // Add Windup
    UAnimNotifyState_AttackPhase* WindupNotify = NewObject<UAnimNotifyState_AttackPhase>(Montage);
    WindupNotify->Phase = EAttackPhase::Windup;
    if (!AddNotifyStateToMontage(Montage, SectionStart, Timing.WindupDuration, WindupNotify, AttackData->MontageSection))
    {
        return false;
    }

    // Add Active
    UAnimNotifyState_AttackPhase* ActiveNotify = NewObject<UAnimNotifyState_AttackPhase>(Montage);
    ActiveNotify->Phase = EAttackPhase::Active;
    if (!AddNotifyStateToMontage(Montage, SectionStart + Timing.WindupDuration, Timing.ActiveDuration, ActiveNotify, AttackData->MontageSection))
    {
        return false;
    }

    // Add Recovery
    UAnimNotifyState_AttackPhase* RecoveryNotify = NewObject<UAnimNotifyState_AttackPhase>(Montage);
    RecoveryNotify->Phase = EAttackPhase::Recovery;
    if (!AddNotifyStateToMontage(Montage, SectionStart + Timing.WindupDuration + Timing.ActiveDuration, Timing.RecoveryDuration, RecoveryNotify, AttackData->MontageSection))
    {
        return false;
    }

    // Add Hold Window if applicable (using separate AnimNotifyState_HoldWindow)
    if (AttackData->bCanHold && Timing.HoldWindowDuration > 0.0f)
    {
        UAnimNotifyState_HoldWindow* HoldNotify = NewObject<UAnimNotifyState_HoldWindow>(Montage);
        AddNotifyStateToMontage(Montage, SectionStart + Timing.HoldWindowStart, Timing.HoldWindowDuration, HoldNotify, AttackData->MontageSection);
    }

    MarkMontageModified(Montage);
    LogToolMessage(FString::Printf(TEXT("GenerateAttackPhaseNotifies: Success for %s"), *AttackData->GetName()));
    
    return true;
}

bool UAttackDataTools::GenerateHitDetectionNotifies(UAttackData* AttackData)
{
    if (!AttackData || !AttackData->AttackMontage)
    {
        return false;
    }

    UAnimMontage* Montage = AttackData->AttackMontage;
    const FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
    float SectionStart, SectionEnd;
    AttackData->GetSectionTimeRange(SectionStart, SectionEnd);

    // Remove existing hit detection notifies
    RemoveNotifiesOfType(Montage, AttackData->MontageSection, UAnimNotify_ToggleHitDetection::StaticClass());

    // Add Enable notify at start of Active phase
    UAnimNotify_ToggleHitDetection* EnableNotify = NewObject<UAnimNotify_ToggleHitDetection>(Montage);
    EnableNotify->bEnable = true;
    const float ActiveStart = SectionStart + Timing.WindupDuration;
    if (!AddNotifyToMontage(Montage, ActiveStart, EnableNotify, AttackData->MontageSection))
    {
        return false;
    }

    // Add Disable notify at end of Active phase
    UAnimNotify_ToggleHitDetection* DisableNotify = NewObject<UAnimNotify_ToggleHitDetection>(Montage);
    DisableNotify->bEnable = false;
    const float ActiveEnd = ActiveStart + Timing.ActiveDuration;
    if (!AddNotifyToMontage(Montage, ActiveEnd, DisableNotify, AttackData->MontageSection))
    {
        return false;
    }

    MarkMontageModified(Montage);
    LogToolMessage(FString::Printf(TEXT("GenerateHitDetectionNotifies: Success for %s"), *AttackData->GetName()));
    
    return true;
}

bool UAttackDataTools::GenerateComboWindowNotify(UAttackData* AttackData)
{
    if (!AttackData || !AttackData->AttackMontage)
    {
        return false;
    }

    UAnimMontage* Montage = AttackData->AttackMontage;
    const FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
    float SectionStart, SectionEnd;
    AttackData->GetSectionTimeRange(SectionStart, SectionEnd);

    // Remove existing combo window notifies
    RemoveNotifiesOfType(Montage, AttackData->MontageSection, UAnimNotifyState_ComboWindow::StaticClass());

    // Add combo window during recovery phase
    UAnimNotifyState_ComboWindow* ComboWindowNotify = NewObject<UAnimNotifyState_ComboWindow>(Montage);
    const float RecoveryStart = SectionStart + Timing.WindupDuration + Timing.ActiveDuration;
    const float ComboWindowDuration = FMath::Min(AttackData->ComboInputWindow, Timing.RecoveryDuration * 0.6f); // First 60% of recovery
    
    if (!AddNotifyStateToMontage(Montage, RecoveryStart, ComboWindowDuration, ComboWindowNotify, AttackData->MontageSection))
    {
        return false;
    }

    MarkMontageModified(Montage);
    LogToolMessage(FString::Printf(TEXT("GenerateComboWindowNotify: Success for %s"), *AttackData->GetName()));
    
    return true;
}

bool UAttackDataTools::GenerateAllNotifies(UAttackData* AttackData)
{
    if (!AttackData)
    {
        return false;
    }

    bool bSuccess = true;
    bSuccess &= GenerateAttackPhaseNotifies(AttackData);
    bSuccess &= GenerateHitDetectionNotifies(AttackData);
    bSuccess &= GenerateComboWindowNotify(AttackData);

    return bSuccess;
}

// ============================================================================
// VALIDATION
// ============================================================================

bool UAttackDataTools::ValidateMontageSection(UAttackData* AttackData, FText& OutErrorMessage)
{
    if (!AttackData)
    {
        OutErrorMessage = LOCTEXT("ValidateNull", "AttackData is null");
        return false;
    }

    if (!AttackData->AttackMontage)
    {
        OutErrorMessage = LOCTEXT("ValidateNoMontage", "No montage assigned");
        return false;
    }

    // Check section exists if specified
    if (AttackData->MontageSection != NAME_None)
    {
        const int32 SectionIndex = AttackData->AttackMontage->GetSectionIndex(AttackData->MontageSection);
        if (SectionIndex == INDEX_NONE)
        {
            OutErrorMessage = FText::Format(
                LOCTEXT("ValidateSectionNotFound", "Section '{0}' not found in montage"),
                FText::FromName(AttackData->MontageSection)
            );
            return false;
        }
    }

    // Check section length
    const float SectionLength = AttackData->GetSectionLength();
    if (SectionLength <= 0.0f)
    {
        OutErrorMessage = LOCTEXT("ValidateInvalidLength", "Section has invalid length");
        return false;
    }

    OutErrorMessage = LOCTEXT("ValidateSuccess", "Montage section is valid");
    return true;
}

TArray<UAttackData*> UAttackDataTools::FindSectionConflicts(UAttackData* AttackData)
{
    TArray<UAttackData*> Conflicts;

    if (!AttackData || !AttackData->AttackMontage || AttackData->MontageSection == NAME_None)
    {
        return Conflicts;
    }

    const TArray<UAttackData*> AllAttacks = FindAllAttackDataAssets();
    
    for (UAttackData* OtherAttack : AllAttacks)
    {
        if (OtherAttack == AttackData || !OtherAttack)
        {
            continue;
        }

        if (OtherAttack->AttackMontage == AttackData->AttackMontage &&
            OtherAttack->MontageSection == AttackData->MontageSection)
        {
            Conflicts.Add(OtherAttack);
        }
    }

    return Conflicts;
}

bool UAttackDataTools::HasValidNotifyTiming(UAttackData* AttackData)
{
    if (!AttackData || !AttackData->AttackMontage)
    {
        return false;
    }

    float Windup, Active, Recovery;
    return ExtractTimingFromNotifies(AttackData, Windup, Active, Recovery);
}

bool UAttackDataTools::ValidateAttackData(UAttackData* AttackData, TArray<FText>& OutWarnings, TArray<FText>& OutErrors)
{
    OutWarnings.Empty();
    OutErrors.Empty();

    if (!AttackData)
    {
        OutErrors.Add(LOCTEXT("ValidateNullData", "AttackData is null"));
        return false;
    }

    // Check montage section
    FText SectionError;
    if (!ValidateMontageSection(AttackData, SectionError))
    {
        OutErrors.Add(SectionError);
    }

    // Check for conflicts
    const TArray<UAttackData*> Conflicts = FindSectionConflicts(AttackData);
    if (Conflicts.Num() > 0)
    {
        OutWarnings.Add(FText::Format(
            LOCTEXT("ValidateConflicts", "Section is shared with {0} other attack(s)"),
            FText::AsNumber(Conflicts.Num())
        ));
    }

    // Check timing
    if (AttackData->bUseAnimNotifyTiming && !HasValidNotifyTiming(AttackData))
    {
        OutWarnings.Add(LOCTEXT("ValidateNoNotifies", "No AnimNotifyState timing found in section"));
    }

    // Check combos
    if (AttackData->NextComboAttack && !AttackData->NextComboAttack->AttackMontage)
    {
        OutErrors.Add(LOCTEXT("ValidateInvalidCombo", "NextComboAttack has no montage assigned"));
    }

    return OutErrors.Num() == 0;
}

// ============================================================================
// VISUALIZATION
// ============================================================================

FString UAttackDataTools::GetTimingPreview(UAttackData* AttackData)
{
    if (!AttackData)
    {
        return TEXT("No AttackData");
    }

    const float SectionLength = AttackData->GetSectionLength();
    if (SectionLength <= 0.0f)
    {
        return TEXT("Invalid section length");
    }

    float WindupPercent, ActivePercent, RecoveryPercent;
    if (!GetTimingPercentages(AttackData, WindupPercent, ActivePercent, RecoveryPercent))
    {
        return TEXT("Unable to calculate timing");
    }

    const FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
    
    return FString::Printf(
        TEXT("[Windup %.2fs (%.0f%%)] [Active %.2fs (%.0f%%)] [Recovery %.2fs (%.0f%%)]"),
        Timing.WindupDuration, WindupPercent * 100.0f,
        Timing.ActiveDuration, ActivePercent * 100.0f,
        Timing.RecoveryDuration, RecoveryPercent * 100.0f
    );
}

bool UAttackDataTools::GetDetailedTiming(UAttackData* AttackData, FAttackPhaseTiming& OutPhaseTiming)
{
    if (!AttackData)
    {
        return false;
    }

    const FAttackPhaseTimingOverride& Timing = AttackData->ManualTiming;
    float SectionStart, SectionEnd;
    AttackData->GetSectionTimeRange(SectionStart, SectionEnd);

    OutPhaseTiming.WindupStart = SectionStart;
    OutPhaseTiming.WindupEnd = SectionStart + Timing.WindupDuration;
    OutPhaseTiming.ActiveStart = OutPhaseTiming.WindupEnd;
    OutPhaseTiming.ActiveEnd = OutPhaseTiming.ActiveStart + Timing.ActiveDuration;
    OutPhaseTiming.RecoveryStart = OutPhaseTiming.ActiveEnd;
    OutPhaseTiming.RecoveryEnd = SectionEnd;

    if (AttackData->bCanHold && Timing.HoldWindowDuration > 0.0f)
    {
        OutPhaseTiming.bHasHoldWindow = true;
        OutPhaseTiming.HoldWindowStart = SectionStart + Timing.HoldWindowStart;
        OutPhaseTiming.HoldWindowEnd = OutPhaseTiming.HoldWindowStart + Timing.HoldWindowDuration;
    }

    return true;
}

// ============================================================================
// MONTAGE UTILITIES
// ============================================================================

TArray<FName> UAttackDataTools::GetMontageSections(UAnimMontage* Montage)
{
    TArray<FName> Sections;
    
    if (Montage)
    {
        for (const FCompositeSection& Section : Montage->CompositeSections)
        {
            Sections.Add(Section.SectionName);
        }
    }

    return Sections;
}

float UAttackDataTools::GetSectionLength(UAnimMontage* Montage, FName SectionName)
{
    if (!Montage)
    {
        return 0.0f;
    }

    if (SectionName == NAME_None)
    {
        return Montage->CalculateSequenceLength();
    }

    const int32 SectionIndex = Montage->GetSectionIndex(SectionName);
    if (SectionIndex == INDEX_NONE)
    {
        return 0.0f;
    }

    const float SectionStart = Montage->GetAnimCompositeSection(SectionIndex).GetTime();
    
    // Find next section or end of montage
    float SectionEnd = Montage->CalculateSequenceLength();
    for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
    {
        if (i != SectionIndex)
        {
            const float OtherStart = Montage->CompositeSections[i].GetTime();
            if (OtherStart > SectionStart && OtherStart < SectionEnd)
            {
                SectionEnd = OtherStart;
            }
        }
    }

    return SectionEnd - SectionStart;
}

float UAttackDataTools::GetSectionStartTime(UAnimMontage* Montage, FName SectionName)
{
    if (!Montage || SectionName == NAME_None)
    {
        return 0.0f;
    }

    const int32 SectionIndex = Montage->GetSectionIndex(SectionName);
    if (SectionIndex == INDEX_NONE)
    {
        return 0.0f;
    }

    return Montage->GetAnimCompositeSection(SectionIndex).GetTime();
}

bool UAttackDataTools::DoesSectionExist(UAnimMontage* Montage, FName SectionName)
{
    if (!Montage || SectionName == NAME_None)
    {
        return false;
    }

    return Montage->GetSectionIndex(SectionName) != INDEX_NONE;
}

// ============================================================================
// ASSET DISCOVERY
// ============================================================================

TArray<UAttackData*> UAttackDataTools::FindAllAttackDataAssets()
{
    TArray<UAttackData*> FoundAssets;

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> AssetDataList;
    FTopLevelAssetPath ClassPath(TEXT("/Script/KatanaCombat"), TEXT("AttackData"));
    AssetRegistry.GetAssetsByClass(ClassPath, AssetDataList, true);

    for (const FAssetData& AssetData : AssetDataList)
    {
        if (UAttackData* AttackData = Cast<UAttackData>(AssetData.GetAsset()))
        {
            FoundAssets.Add(AttackData);
        }
    }

    return FoundAssets;
}

TArray<UAttackData*> UAttackDataTools::FindAttackDataUsingMontage(UAnimMontage* Montage)
{
    TArray<UAttackData*> FoundAssets;
    
    if (!Montage)
    {
        return FoundAssets;
    }

    const TArray<UAttackData*> AllAttacks = FindAllAttackDataAssets();
    for (UAttackData* AttackData : AllAttacks)
    {
        if (AttackData && AttackData->AttackMontage == Montage)
        {
            FoundAssets.Add(AttackData);
        }
    }

    return FoundAssets;
}

TArray<UAttackData*> UAttackDataTools::FindAttackDataByType(EAttackType AttackType)
{
    TArray<UAttackData*> FoundAssets;

    const TArray<UAttackData*> AllAttacks = FindAllAttackDataAssets();
    for (UAttackData* AttackData : AllAttacks)
    {
        if (AttackData && AttackData->AttackType == AttackType)
        {
            FoundAssets.Add(AttackData);
        }
    }

    return FoundAssets;
}

// ============================================================================
// BATCH OPERATIONS
// ============================================================================

bool UAttackDataTools::BatchGenerateNotifies(const TArray<UAttackData*>& AttackDataArray, int32& OutSuccessCount, int32& OutFailureCount)
{
    OutSuccessCount = 0;
    OutFailureCount = 0;

    for (UAttackData* AttackData : AttackDataArray)
    {
        if (GenerateAllNotifies(AttackData))
        {
            OutSuccessCount++;
        }
        else
        {
            OutFailureCount++;
        }
    }

    return OutSuccessCount > 0;
}

void UAttackDataTools::BatchValidate(const TArray<UAttackData*>& AttackDataArray, TArray<UAttackData*>& OutValidAssets, TArray<UAttackData*>& OutInvalidAssets)
{
    OutValidAssets.Empty();
    OutInvalidAssets.Empty();

    for (UAttackData* AttackData : AttackDataArray)
    {
        TArray<FText> Warnings, Errors;
        if (ValidateAttackData(AttackData, Warnings, Errors))
        {
            OutValidAssets.Add(AttackData);
        }
        else
        {
            OutInvalidAssets.Add(AttackData);
        }
    }
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

bool UAttackDataTools::AddNotifyStateToMontage(UAnimMontage* Montage, float StartTime, float Duration, UAnimNotifyState* NotifyState, FName SectionName)
{
    if (!Montage || !NotifyState || Duration <= 0.0f)
    {
        return false;
    }

    FAnimNotifyEvent NewNotify;
    NewNotify.NotifyStateClass = NotifyState;
    NewNotify.SetTime(StartTime);
    NewNotify.SetDuration(Duration);
    NewNotify.TriggerTimeOffset = EAnimEventTriggerOffsets::OffsetBefore;
    NewNotify.TrackIndex = 0;

    Montage->Notifies.Add(NewNotify);
    return true;
}

bool UAttackDataTools::AddNotifyToMontage(UAnimMontage* Montage, float Time, UAnimNotify* Notify, FName SectionName)
{
    if (!Montage || !Notify)
    {
        return false;
    }

    FAnimNotifyEvent NewNotify;
    NewNotify.Notify = Notify;
    NewNotify.SetTime(Time);
    NewNotify.TriggerTimeOffset = EAnimEventTriggerOffsets::OffsetBefore;
    NewNotify.TrackIndex = 0;

    Montage->Notifies.Add(NewNotify);
    return true;
}

void UAttackDataTools::RemoveNotifiesOfType(UAnimMontage* Montage, FName SectionName, UClass* NotifyClass)
{
    if (!Montage || !NotifyClass)
    {
        return;
    }

    float SectionStart = 0.0f;
    float SectionEnd = Montage->CalculateSequenceLength();

    if (SectionName != NAME_None)
    {
        SectionStart = GetSectionStartTime(Montage, SectionName);
        SectionEnd = SectionStart + GetSectionLength(Montage, SectionName);
    }

    for (int32 i = Montage->Notifies.Num() - 1; i >= 0; --i)
    {
        const FAnimNotifyEvent& NotifyEvent = Montage->Notifies[i];
        const float NotifyTime = NotifyEvent.GetTriggerTime();

        if (NotifyTime >= SectionStart && NotifyTime < SectionEnd)
        {
            bool bShouldRemove = false;

            if (NotifyEvent.NotifyStateClass && NotifyEvent.NotifyStateClass->IsA(NotifyClass))
            {
                bShouldRemove = true;
            }
            else if (NotifyEvent.Notify && NotifyEvent.Notify->IsA(NotifyClass))
            {
                bShouldRemove = true;
            }

            if (bShouldRemove)
            {
                Montage->Notifies.RemoveAt(i);
            }
        }
    }
}

float UAttackDataTools::SectionTimeToMontageTime(UAnimMontage* Montage, FName SectionName, float SectionRelativeTime)
{
    if (!Montage)
    {
        return SectionRelativeTime;
    }

    return GetSectionStartTime(Montage, SectionName) + SectionRelativeTime;
}

void UAttackDataTools::GetDefaultTimingPercentages(EAttackType AttackType, float& OutWindupPercent, float& OutActivePercent, float& OutRecoveryPercent)
{
    switch (AttackType)
    {
        case EAttackType::Light:
            OutWindupPercent = 0.30f;
            OutActivePercent = 0.20f;
            OutRecoveryPercent = 0.50f;
            break;

        case EAttackType::Heavy:
            OutWindupPercent = 0.40f;
            OutActivePercent = 0.30f;
            OutRecoveryPercent = 0.30f;
            break;

        case EAttackType::Special:
            OutWindupPercent = 0.35f;
            OutActivePercent = 0.25f;
            OutRecoveryPercent = 0.40f;
            break;

        default:
            OutWindupPercent = 0.33f;
            OutActivePercent = 0.33f;
            OutRecoveryPercent = 0.34f;
            break;
    }
}

void UAttackDataTools::MarkMontageModified(UAnimMontage* Montage)
{
    if (Montage)
    {
        Montage->MarkPackageDirty();
    }
}

void UAttackDataTools::LogToolMessage(const FString& Message, bool bIsWarning)
{
    if (bIsWarning)
    {
        UE_LOG(LogTemp, Warning, TEXT("AttackDataTools: %s"), *Message);
    }
    else
    {
        UE_LOG(LogTemp, Log, TEXT("AttackDataTools: %s"), *Message);
    }
}

#undef LOCTEXT_NAMESPACE