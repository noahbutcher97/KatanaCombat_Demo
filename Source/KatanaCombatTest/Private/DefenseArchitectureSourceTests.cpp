// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
FString StripCppComments(const FString& Source)
{
	FString Result;
	Result.Reserve(Source.Len());
	bool bLineComment = false;
	bool bBlockComment = false;
	bool bString = false;
	bool bCharacter = false;
	bool bEscaped = false;

	for (int32 Index = 0; Index < Source.Len(); ++Index)
	{
		const TCHAR Current = Source[Index];
		const TCHAR Next = Index + 1 < Source.Len() ? Source[Index + 1] : TEXT('\0');

		if (bLineComment)
		{
			if (Current == TEXT('\n'))
			{
				bLineComment = false;
				Result.AppendChar(Current);
			}
			continue;
		}
		if (bBlockComment)
		{
			if (Current == TEXT('*') && Next == TEXT('/'))
			{
				bBlockComment = false;
				++Index;
			}
			else if (Current == TEXT('\n'))
			{
				Result.AppendChar(Current);
			}
			continue;
		}

		if (!bString && !bCharacter && Current == TEXT('/') && Next == TEXT('/'))
		{
			bLineComment = true;
			++Index;
			continue;
		}
		if (!bString && !bCharacter && Current == TEXT('/') && Next == TEXT('*'))
		{
			bBlockComment = true;
			++Index;
			continue;
		}

		Result.AppendChar(Current);
		if (bEscaped)
		{
			bEscaped = false;
			continue;
		}
		if ((bString || bCharacter) && Current == TEXT('\\'))
		{
			bEscaped = true;
			continue;
		}
		if (!bCharacter && Current == TEXT('"'))
		{
			bString = !bString;
		}
		else if (!bString && Current == TEXT('\''))
		{
			bCharacter = !bCharacter;
		}
	}
	return Result;
}

bool ExtractFunctionBody(const FString& Source, const FString& FunctionName, FString& OutBody)
{
	const FString CleanSource = StripCppComments(Source);
	const int32 NameIndex = CleanSource.Find(FunctionName, ESearchCase::CaseSensitive);
	if (NameIndex == INDEX_NONE)
	{
		return false;
	}

	const int32 OpenBrace = CleanSource.Find(TEXT("{"), ESearchCase::CaseSensitive, ESearchDir::FromStart, NameIndex);
	if (OpenBrace == INDEX_NONE)
	{
		return false;
	}

	int32 Depth = 0;
	bool bString = false;
	bool bCharacter = false;
	bool bEscaped = false;
	for (int32 Index = OpenBrace; Index < CleanSource.Len(); ++Index)
	{
		const TCHAR Current = CleanSource[Index];
		if (bEscaped)
		{
			bEscaped = false;
			continue;
		}
		if ((bString || bCharacter) && Current == TEXT('\\'))
		{
			bEscaped = true;
			continue;
		}
		if (!bCharacter && Current == TEXT('"'))
		{
			bString = !bString;
			continue;
		}
		if (!bString && Current == TEXT('\''))
		{
			bCharacter = !bCharacter;
			continue;
		}
		if (bString || bCharacter)
		{
			continue;
		}

		if (Current == TEXT('{'))
		{
			++Depth;
		}
		else if (Current == TEXT('}'))
		{
			--Depth;
			if (Depth == 0)
			{
				OutBody = CleanSource.Mid(OpenBrace + 1, Index - OpenBrace - 1);
				return true;
			}
		}
	}
	return false;
}

bool LoadProjectSource(const FString& RelativePath, FString& OutSource)
{
	return FFileHelper::LoadFileToString(
		OutSource,
		*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativePath));
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseRichContactAuthoritySourceTest,
	"KatanaCombat.Defense.Contact.Architecture.RichTargetAuthority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseRichContactAuthoritySourceTest::RunTest(const FString& Parameters)
{
	FString Source;
	if (!TestTrue(TEXT("BaseCombatCharacter source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp"), Source)))
	{
		return false;
	}

	FString Body;
	if (!TestTrue(TEXT("Rich target entry point has an extractable body"), ExtractFunctionBody(
		Source, TEXT("ABaseCombatCharacter::ResolveAndCommitCombatContact"), Body)))
	{
		return false;
	}

	TestFalse(TEXT("Rich target path does not recurse through legacy ApplyDamage"), Body.Contains(TEXT("ApplyDamage")));
	TestFalse(TEXT("Rich target path does not re-read legacy IsBlocking"), Body.Contains(TEXT("IsBlocking")));
	TestFalse(TEXT("Rich target path does not call legacy CanBlockHit"), Body.Contains(TEXT("CanBlockHit")));

	FString CommitBody;
	if (!TestTrue(TEXT("Silent rich damage helper has an extractable body"), ExtractFunctionBody(
		Source, TEXT("ABaseCombatCharacter::CommitResolvedDefenseDamage"), CommitBody)))
	{
		return false;
	}
	TestFalse(TEXT("Silent rich damage helper does not call legacy ApplyDamage"),
		CommitBody.Contains(TEXT("ApplyDamage")));
	TestFalse(TEXT("Silent rich damage helper does not call observable ModifyHealth"),
		CommitBody.Contains(TEXT("ModifyHealth")));
	TestFalse(TEXT("Silent rich damage helper does not call observable HandleDeath"),
		CommitBody.Contains(TEXT("HandleDeath")));
	TestFalse(TEXT("Silent rich damage helper does not reclassify CanBlockHit"),
		CommitBody.Contains(TEXT("CanBlockHit")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseWeaponReceiptOrderingSourceTest,
	"KatanaCombat.Defense.Contact.Architecture.WeaponReceiptBeforeAccounting",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseWeaponReceiptOrderingSourceTest::RunTest(const FString& Parameters)
{
	FString Source;
	if (!TestTrue(TEXT("WeaponComponent source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Core/WeaponComponent.cpp"), Source)))
	{
		return false;
	}

	FString WrapperBody;
	if (!TestTrue(TEXT("Weapon ProcessHit has an extractable body"), ExtractFunctionBody(
		Source, TEXT("UWeaponComponent::ProcessHit(const FHitResult& Hit)"), WrapperBody)))
	{
		return false;
	}
	TestTrue(TEXT("ProcessHit delegates to the attack-aware implementation"),
		WrapperBody.Contains(TEXT("ProcessHitWithAttackData")));
	TestFalse(TEXT("ProcessHit wrapper performs no accounting"),
		WrapperBody.Contains(TEXT("AddHitActor")));

	FString Body;
	if (!TestTrue(TEXT("Attack-aware ProcessHit has an extractable body"), ExtractFunctionBody(
		Source, TEXT("UWeaponComponent::ProcessHitWithAttackData"), Body)))
	{
		return false;
	}

	const int32 ReceiptIndex = Body.Find(TEXT("ResolveWeaponContactCandidate"), ESearchCase::CaseSensitive);
	const int32 FirstAccountingIndex = Body.Find(TEXT("AddHitActor"), ESearchCase::CaseSensitive);
	TestTrue(TEXT("Rich receipt is obtained in ProcessHit"), ReceiptIndex != INDEX_NONE);
	TestTrue(TEXT("Hit accounting remains present"), FirstAccountingIndex != INDEX_NONE);
	TestTrue(TEXT("No hit actor is added before the rich receipt"),
		ReceiptIndex != INDEX_NONE && FirstAccountingIndex > ReceiptIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefensePresentationBeforeCallbacksSourceTest,
	"KatanaCombat.Defense.Contact.Architecture.PresentationBeforeCallbacks",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefensePresentationBeforeCallbacksSourceTest::RunTest(const FString& Parameters)
{
	FString Source;
	if (!TestTrue(TEXT("BaseCombatCharacter source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp"), Source)))
	{
		return false;
	}

	FString Body;
	if (!TestTrue(TEXT("Committed contact flush has an extractable body"), ExtractFunctionBody(
		Source, TEXT("ABaseCombatCharacter::FlushCommittedDefenseContact"), Body)))
	{
		return false;
	}

	const int32 PresentationIndex = Body.Find(
		TEXT("PlayCommittedDamageReaction"), ESearchCase::CaseSensitive);
	const int32 CallbackIndex = Body.Find(
		TEXT("BroadcastCommittedDamage"), ESearchCase::CaseSensitive);
	TestTrue(TEXT("Rich flush owns hit presentation explicitly"), PresentationIndex != INDEX_NONE);
	TestTrue(TEXT("Rich flush broadcasts immutable damage explicitly"), CallbackIndex != INDEX_NONE);
	TestTrue(TEXT("Hit presentation precedes the public damage callback"),
		PresentationIndex != INDEX_NONE
			&& CallbackIndex != INDEX_NONE
			&& PresentationIndex < CallbackIndex);
	TestFalse(TEXT("Rich flush does not use compatibility dispatch ordering"),
		Body.Contains(TEXT("DispatchCommittedDamage")));
	return true;
}
