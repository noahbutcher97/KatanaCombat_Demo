// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/HitReactionData.h"
#include "Animation/AnimMontage.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogHitReactionData, Log, All);

UHitReactionData::UHitReactionData()
{
    // Default values are set via inline initialization in header
}

// ============================================================================
// SECTION QUERIES
// ============================================================================

void UHitReactionData::GetSectionTimeRange(float& OutStart, float& OutEnd) const
{
    OutStart = 0.0f;
    OutEnd = 0.0f;

    if (!ReactionMontage)
    {
        return;
    }

    // If no section specified, use entire montage
    if (MontageSection == NAME_None)
    {
        OutStart = 0.0f;
        OutEnd = ReactionMontage->CalculateSequenceLength();
        return;
    }

    // Find section index
    const int32 SectionIndex = ReactionMontage->GetSectionIndex(MontageSection);
    if (SectionIndex == INDEX_NONE)
    {
        UE_LOG(LogHitReactionData, Warning, TEXT("%s: MontageSection '%s' not found in montage '%s'"),
            *GetName(), *MontageSection.ToString(), *ReactionMontage->GetName());
        return;
    }

    // Get section start time
    OutStart = ReactionMontage->GetAnimCompositeSection(SectionIndex).GetTime();

    // Find next section or end of montage
    OutEnd = ReactionMontage->CalculateSequenceLength();

    for (int32 i = 0; i < ReactionMontage->CompositeSections.Num(); ++i)
    {
        if (i != SectionIndex)
        {
            const float OtherSectionStart = ReactionMontage->CompositeSections[i].GetTime();
            if (OtherSectionStart > OutStart && OtherSectionStart < OutEnd)
            {
                OutEnd = OtherSectionStart;
            }
        }
    }
}

float UHitReactionData::GetSectionLength() const
{
    float Start, End;
    GetSectionTimeRange(Start, End);
    return FMath::Max(0.0f, End - Start);
}

// ============================================================================
// I-FRAME QUERIES
// ============================================================================

bool UHitReactionData::IsInIFrameWindow(float CurrentTime) const
{
    if (!bHasIFrames)
    {
        return false;
    }

    return CurrentTime >= IFrameStart && CurrentTime <= IFrameEnd;
}

// ============================================================================
// EDITOR VALIDATION
// ============================================================================

#if WITH_EDITOR
EDataValidationResult UHitReactionData::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = EDataValidationResult::Valid;

    // Validate montage exists
    if (!ReactionMontage)
    {
        Context.AddError(FText::FromString(FString::Printf(
            TEXT("%s: No ReactionMontage assigned"), *GetName())));
        Result = EDataValidationResult::Invalid;
    }

    // Validate section exists if specified
    if (ReactionMontage && MontageSection != NAME_None)
    {
        const int32 SectionIndex = ReactionMontage->GetSectionIndex(MontageSection);
        if (SectionIndex == INDEX_NONE)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("%s: MontageSection '%s' not found in montage '%s'"),
                *GetName(), *MontageSection.ToString(), *ReactionMontage->GetName())));
            Result = EDataValidationResult::Invalid;
        }
    }

    // Validate i-frame timing
    if (bHasIFrames)
    {
        if (IFrameEnd <= IFrameStart)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("%s: IFrameEnd (%.2f) must be greater than IFrameStart (%.2f)"),
                *GetName(), IFrameEnd, IFrameStart)));
            Result = EDataValidationResult::Invalid;
        }

        const float SectionLength = GetSectionLength();
        if (SectionLength > 0.0f && IFrameEnd > SectionLength)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("%s: IFrameEnd (%.2f) exceeds section length (%.2f)"),
                *GetName(), IFrameEnd, SectionLength)));
        }
    }

    // Validate paired reaction config
    if (bIsPairedReaction)
    {
        if (PairedType == EPairedReactionType::None)
        {
            Context.AddError(FText::FromString(FString::Printf(
                TEXT("%s: bIsPairedReaction is true but PairedType is None"),
                *GetName())));
            Result = EDataValidationResult::Invalid;
        }

        if (PairedReactionName == NAME_None)
        {
            Context.AddWarning(FText::FromString(FString::Printf(
                TEXT("%s: bIsPairedReaction is true but PairedReactionName is not set"),
                *GetName())));
        }
    }

    return CombineDataValidationResults(Result, Super::IsDataValid(Context));
}
#endif
