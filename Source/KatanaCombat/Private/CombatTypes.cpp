// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTypes.h"
#include "Animation/AnimMontage.h"

// ============================================================================
// FHitReactionEntry Implementation
// ============================================================================

void FHitReactionEntry::GetSectionTimeRange(float& OutStart, float& OutEnd) const
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
        // Section not found - fall back to entire montage
        OutStart = 0.0f;
        OutEnd = ReactionMontage->CalculateSequenceLength();
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

float FHitReactionEntry::GetSectionLength() const
{
    float Start, End;
    GetSectionTimeRange(Start, End);
    return FMath::Max(0.0f, End - Start);
}
