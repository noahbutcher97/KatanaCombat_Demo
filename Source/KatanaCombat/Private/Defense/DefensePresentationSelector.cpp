// Copyright Epic Games, Inc. All Rights Reserved.

#include "Defense/DefensePresentationSelector.h"

#include "Data/DefenseConfiguration.h"

namespace
{
struct FPresentationRank
{
	int32 ExactFieldCount = 0;
	int32 RequiredTagCount = 0;
	int32 Priority = 0;
};

bool HasSameRank(const FPresentationRank& Left, const FPresentationRank& Right)
{
	return Left.ExactFieldCount == Right.ExactFieldCount
		&& Left.RequiredTagCount == Right.RequiredTagCount
		&& Left.Priority == Right.Priority;
}

bool IsBetterRank(const FPresentationRank& Left, const FPresentationRank& Right)
{
	if (Left.ExactFieldCount != Right.ExactFieldCount)
	{
		return Left.ExactFieldCount > Right.ExactFieldCount;
	}
	if (Left.RequiredTagCount != Right.RequiredTagCount)
	{
		return Left.RequiredTagCount > Right.RequiredTagCount;
	}
	return Left.Priority > Right.Priority;
}

bool MatchesSharedFields(
	const FDefensePresentationSelectionContext& Context,
	const bool bMatchAnyHeight,
	const EAttackHeight Height,
	const bool bMatchAnyLane,
	const EIncomingAttackLane Lane,
	const bool bMatchAnySwingShape,
	const ESwingDirection SwingShape,
	const FGameplayTagContainer& RequiredTags,
	const FGameplayTagContainer& ExcludedTags,
	const FDefensePresentationPayload& Payload)
{
	if ((!bMatchAnyHeight && Height != Context.Height)
		|| (!bMatchAnyLane && Lane != Context.Lane)
		|| (!bMatchAnySwingShape && SwingShape != Context.SwingShape)
		|| !Context.AttackTags.HasAll(RequiredTags)
		|| Context.AttackTags.HasAny(ExcludedTags))
	{
		return false;
	}

	const bool bNeedsBridgePreflight = Payload.bRequiresBridgePreflight || Payload.PairedBridgeData != nullptr;
	return !bNeedsBridgePreflight || Context.bPairedBridgeUsable;
}

bool LexicallyBefore(const FName Left, const FName Right)
{
	return Left.ToString().Compare(Right.ToString()) < 0;
}
}

FDefensePresentationSelectionResult FTableDefensePresentationSelector::SelectDefender(
	const FDefensePresentationSelectionContext& Context,
	const UDefenseConfiguration* Configuration) const
{
	FDefensePresentationSelectionResult Result;
	Result.Outcome = Context.Outcome;
	Result.AttackerResponse = Context.AttackerResponse;
	if (!Configuration)
	{
		return Result;
	}

	const FDefensePresentationRow* BestRow = nullptr;
	FPresentationRank BestRank;
	for (const FDefensePresentationRow& Row : Configuration->DefenderPresentationRows)
	{
		if (Row.Outcome != Context.Outcome
			|| !MatchesSharedFields(
				Context,
				Row.bMatchAnyHeight,
				Row.Height,
				Row.bMatchAnyLane,
				Row.Lane,
				Row.bMatchAnySwingShape,
				Row.SwingShape,
				Row.RequiredTags,
				Row.ExcludedTags,
				Row.Payload))
		{
			continue;
		}

		const FPresentationRank Rank{Row.GetExactFieldCount(), Row.RequiredTags.Num(), Row.Priority};
		if (!BestRow || IsBetterRank(Rank, BestRank))
		{
			BestRow = &Row;
			BestRank = Rank;
			Result.bAmbiguous = false;
		}
		else if (HasSameRank(Rank, BestRank))
		{
			Result.bAmbiguous = true;
			if (LexicallyBefore(Row.RowName, BestRow->RowName))
			{
				BestRow = &Row;
			}
		}
	}

	if (BestRow)
	{
		Result.bFound = true;
		Result.RowName = BestRow->RowName;
		Result.Payload = BestRow->Payload;
		Result.FallbackLevel = BestRow->IsGenericFallback()
			? EDefensePresentationFallbackLevel::Generic
			: EDefensePresentationFallbackLevel::Exact;
	}
	return Result;
}

FDefensePresentationSelectionResult FTableDefensePresentationSelector::SelectAttacker(
	const FDefensePresentationSelectionContext& Context,
	const UDefenseConfiguration* AttackerConfiguration) const
{
	FDefensePresentationSelectionResult Result;
	Result.Outcome = Context.Outcome;
	Result.AttackerResponse = Context.AttackerResponse;
	if (!AttackerConfiguration)
	{
		return Result;
	}

	const FAttackerResponsePresentationRow* BestRow = nullptr;
	FPresentationRank BestRank;
	for (const FAttackerResponsePresentationRow& Row : AttackerConfiguration->AttackerResponseRows)
	{
		if (Row.Response != Context.AttackerResponse
			|| !MatchesSharedFields(
				Context,
				Row.bMatchAnyHeight,
				Row.Height,
				Row.bMatchAnyLane,
				Row.Lane,
				Row.bMatchAnySwingShape,
				Row.SwingShape,
				Row.RequiredTags,
				Row.ExcludedTags,
				Row.Payload))
		{
			continue;
		}

		const FPresentationRank Rank{Row.GetExactFieldCount(), Row.RequiredTags.Num(), Row.Priority};
		if (!BestRow || IsBetterRank(Rank, BestRank))
		{
			BestRow = &Row;
			BestRank = Rank;
			Result.bAmbiguous = false;
		}
		else if (HasSameRank(Rank, BestRank))
		{
			Result.bAmbiguous = true;
			if (LexicallyBefore(Row.RowName, BestRow->RowName))
			{
				BestRow = &Row;
			}
		}
	}

	if (BestRow)
	{
		Result.bFound = true;
		Result.RowName = BestRow->RowName;
		Result.Payload = BestRow->Payload;
		Result.FallbackLevel = BestRow->IsGenericFallback()
			? EDefensePresentationFallbackLevel::Generic
			: EDefensePresentationFallbackLevel::Exact;
	}
	return Result;
}

FDefensePresentationSelectionResult FTableDefensePresentationSelector::SelectGenericDefender(
	const FDefensePresentationSelectionContext& Context,
	const UDefenseConfiguration* Configuration) const
{
	FDefensePresentationSelectionResult Result;
	Result.Outcome = Context.Outcome;
	Result.AttackerResponse = Context.AttackerResponse;
	if (!Configuration)
	{
		return Result;
	}

	const FDefensePresentationRow* BestRow = nullptr;
	for (const FDefensePresentationRow& Row : Configuration->DefenderPresentationRows)
	{
		if (Row.Outcome != Context.Outcome
			|| !Row.IsGenericFallback()
			|| !MatchesSharedFields(
				Context,
				Row.bMatchAnyHeight,
				Row.Height,
				Row.bMatchAnyLane,
				Row.Lane,
				Row.bMatchAnySwingShape,
				Row.SwingShape,
				Row.RequiredTags,
				Row.ExcludedTags,
				Row.Payload))
		{
			continue;
		}
		if (!BestRow || Row.Priority > BestRow->Priority)
		{
			BestRow = &Row;
			Result.bAmbiguous = false;
		}
		else if (Row.Priority == BestRow->Priority)
		{
			Result.bAmbiguous = true;
			if (LexicallyBefore(Row.RowName, BestRow->RowName))
			{
				BestRow = &Row;
			}
		}
	}

	if (BestRow)
	{
		Result.bFound = true;
		Result.RowName = BestRow->RowName;
		Result.Payload = BestRow->Payload;
		Result.FallbackLevel = EDefensePresentationFallbackLevel::Generic;
	}
	return Result;
}

FDefensePresentationSelectionResult FTableDefensePresentationSelector::SelectGenericAttacker(
	const FDefensePresentationSelectionContext& Context,
	const UDefenseConfiguration* AttackerConfiguration) const
{
	FDefensePresentationSelectionResult Result;
	Result.Outcome = Context.Outcome;
	Result.AttackerResponse = Context.AttackerResponse;
	if (!AttackerConfiguration)
	{
		return Result;
	}

	const FAttackerResponsePresentationRow* BestRow = nullptr;
	for (const FAttackerResponsePresentationRow& Row : AttackerConfiguration->AttackerResponseRows)
	{
		if (Row.Response != Context.AttackerResponse
			|| !Row.IsGenericFallback()
			|| !MatchesSharedFields(
				Context,
				Row.bMatchAnyHeight,
				Row.Height,
				Row.bMatchAnyLane,
				Row.Lane,
				Row.bMatchAnySwingShape,
				Row.SwingShape,
				Row.RequiredTags,
				Row.ExcludedTags,
				Row.Payload))
		{
			continue;
		}
		if (!BestRow || Row.Priority > BestRow->Priority)
		{
			BestRow = &Row;
			Result.bAmbiguous = false;
		}
		else if (Row.Priority == BestRow->Priority)
		{
			Result.bAmbiguous = true;
			if (LexicallyBefore(Row.RowName, BestRow->RowName))
			{
				BestRow = &Row;
			}
		}
	}

	if (BestRow)
	{
		Result.bFound = true;
		Result.RowName = BestRow->RowName;
		Result.Payload = BestRow->Payload;
		Result.FallbackLevel = EDefensePresentationFallbackLevel::Generic;
	}
	return Result;
}
