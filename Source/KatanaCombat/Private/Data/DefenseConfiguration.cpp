// Copyright Epic Games, Inc. All Rights Reserved.

#include "Data/DefenseConfiguration.h"

int32 FDefensePresentationRow::GetExactFieldCount() const
{
	return static_cast<int32>(!bMatchAnyHeight)
		+ static_cast<int32>(!bMatchAnyLane)
		+ static_cast<int32>(!bMatchAnySwingShape);
}

bool FDefensePresentationRow::IsGenericFallback() const
{
	return bMatchAnyHeight && bMatchAnyLane && bMatchAnySwingShape
		&& RequiredTags.IsEmpty() && ExcludedTags.IsEmpty();
}

int32 FAttackerResponsePresentationRow::GetExactFieldCount() const
{
	return static_cast<int32>(!bMatchAnyHeight)
		+ static_cast<int32>(!bMatchAnyLane)
		+ static_cast<int32>(!bMatchAnySwingShape);
}

bool FAttackerResponsePresentationRow::IsGenericFallback() const
{
	return bMatchAnyHeight && bMatchAnyLane && bMatchAnySwingShape
		&& RequiredTags.IsEmpty() && ExcludedTags.IsEmpty();
}

UDefenseConfiguration::UDefenseConfiguration()
{
}

FDefenseHeightResolution UDefenseConfiguration::ResolveHeight(
	const FName HitBone,
	const TArray<FName>& ParentBoneChain,
	const EAttackHeight AuthoredHeight) const
{
	for (const FDefenseBoneHeightRow& Row : BoneHeightRows)
	{
		if (!HitBone.IsNone() && Row.BoneName == HitBone)
		{
			return {Row.Height, EDefenseHeightProvenance::ExactBone, Row.BoneName};
		}
	}

	for (const FName ParentBone : ParentBoneChain)
	{
		for (const FDefenseBoneHeightRow& Row : BoneHeightRows)
		{
			if (!ParentBone.IsNone() && Row.BoneName == ParentBone)
			{
				return {Row.Height, EDefenseHeightProvenance::MappedParent, Row.BoneName};
			}
		}
	}

	return {AuthoredHeight, EDefenseHeightProvenance::Authored, NAME_None};
}
