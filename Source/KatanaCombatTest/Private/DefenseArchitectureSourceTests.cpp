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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseEnemyAttackFacingOwnershipSourceTest,
	"KatanaCombat.Defense.Alignment.Architecture.EnemyAttackUsesOwnedWarp",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseEnemyAttackFacingOwnershipSourceTest::RunTest(const FString& Parameters)
{
	FString Source;
	if (!TestTrue(TEXT("Enemy combat AI source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp"), Source)))
	{
		return false;
	}

	FString Body;
	if (!TestTrue(TEXT("Enemy attack execution has an extractable body"), ExtractFunctionBody(
		Source, TEXT("UEnemyCombatAIComponent::ExecuteAttack()"), Body)))
	{
		return false;
	}

	const int32 IntentIndex = Body.Find(TEXT("SetAttackIntentTarget"), ESearchCase::CaseSensitive);
	const int32 ExecuteIndex = Body.Find(TEXT("ExecuteAttackData"), ESearchCase::CaseSensitive);
	TestFalse(TEXT("Enemy attack execution performs no direct actor rotation"),
		Body.Contains(TEXT("SetActorRotation")));
	TestTrue(TEXT("Enemy attack publishes its explicit intent target"), IntentIndex != INDEX_NONE);
	TestTrue(TEXT("Enemy attack executes through CombatComponent"), ExecuteIndex != INDEX_NONE);
	TestTrue(TEXT("Enemy attack publishes intent before execution acquires its owned warp"),
		IntentIndex != INDEX_NONE && ExecuteIndex != INDEX_NONE && IntentIndex < ExecuteIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseNoDirectRotationSourceTest,
	"KatanaCombat.Defense.Alignment.Architecture.NoDirectRotation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseNoDirectRotationSourceTest::RunTest(const FString& Parameters)
{
	const TCHAR* DefenseOwnerSources[] =
	{
		TEXT("Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp"),
		TEXT("Source/KatanaCombat/Private/Characters/BaseCombatCharacter.cpp"),
		TEXT("Source/KatanaCombat/Private/Core/CombatComponent.cpp"),
		TEXT("Source/KatanaCombat/Private/Core/HitReactionComponent.cpp"),
		TEXT("Source/KatanaCombat/Private/Core/TargetingComponent.cpp")
	};

	for (const TCHAR* RelativePath : DefenseOwnerSources)
	{
		FString Source;
		if (!TestTrue(*FString::Printf(TEXT("Defense owner source loads: %s"), RelativePath),
			LoadProjectSource(RelativePath, Source)))
		{
			continue;
		}
		TestFalse(*FString::Printf(TEXT("Defense owner has no direct actor rotation: %s"), RelativePath),
			StripCppComments(Source).Contains(TEXT("SetActorRotation")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseCombatWarpCloneOwnershipSourceTest,
	"KatanaCombat.Defense.Alignment.Architecture.CombatWarpClonesBeforeConfiguration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseCombatWarpCloneOwnershipSourceTest::RunTest(const FString& Parameters)
{
	FString Source;
	if (!TestTrue(TEXT("Combat warp notify source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Animation/AnimNotifyState_CombatWarp.cpp"), Source)))
	{
		return false;
	}

	FString Body;
	if (!TestTrue(TEXT("Combat warp modifier factory has an extractable body"), ExtractFunctionBody(
		Source, TEXT("UAnimNotifyState_CombatWarp::AddRootMotionModifier_Implementation"), Body)))
	{
		return false;
	}

	const int32 CloneIndex = Body.Find(TEXT("AddModifierFromTemplate"), ESearchCase::CaseSensitive);
	const int32 RegistrationIndex = Body.Find(TEXT("RegisterAlignmentModifier"), ESearchCase::CaseSensitive);
	TestTrue(TEXT("Combat warp creates an engine-owned clone"), CloneIndex != INDEX_NONE);
	TestTrue(TEXT("Combat warp registers only the runtime clone"), RegistrationIndex != INDEX_NONE);
	TestTrue(TEXT("Clone creation precedes runtime configuration"),
		CloneIndex != INDEX_NONE && RegistrationIndex > CloneIndex);
	TestFalse(TEXT("Combat warp never mutates the shared notify template"),
		Body.Contains(TEXT("RootMotionModifier->")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAttackConsumptionOrderingSourceTest,
	"KatanaCombat.Defense.Architecture.AttackConsumptionOrdering",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAttackConsumptionOrderingSourceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Source;
	if (!TestTrue(TEXT("CombatComponent source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Core/CombatComponent.cpp"), Source)))
	{
		return false;
	}

	FString Body;
	if (!TestTrue(TEXT("Attack consumption body is extractable"), ExtractFunctionBody(
		Source, TEXT("UCombatComponent::ConsumeActiveAttackInternal"), Body)))
	{
		return false;
	}

	const int32 ConsumedIndex = Body.Find(TEXT("ConsumedAttackInstance = AttackId"));
	const int32 WindowCleanupIndex = Body.Find(TEXT("ClearPublishedAttackWindowsForAttack"));
	const int32 TraceCleanupIndex = Body.Find(TEXT("DisableHitDetectionForAttack"));
	const int32 ImmediateEventIndex = Body.Find(TEXT("OnAttackConsumedInternal.Broadcast"));
	TestTrue(TEXT("Consumed marker is installed"), ConsumedIndex != INDEX_NONE);
	TestTrue(TEXT("Canonical windows are cleaned"), WindowCleanupIndex != INDEX_NONE);
	TestTrue(TEXT("Exact trace generation is cleaned"), TraceCleanupIndex != INDEX_NONE);
	TestTrue(TEXT("Native termination event remains present"), ImmediateEventIndex != INDEX_NONE);
	TestTrue(TEXT("Consumed marker precedes cleanup and all callbacks"),
		ConsumedIndex < WindowCleanupIndex
			&& ConsumedIndex < TraceCleanupIndex
			&& ConsumedIndex < ImmediateEventIndex);
	TestFalse(TEXT("Public event is not broadcast synchronously"),
		Body.Contains(TEXT("OnAttackConsumed.Broadcast")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseEnemyTerminationOwnershipSourceTest,
	"KatanaCombat.Defense.Architecture.EnemyTerminationOwnsTokenRelease",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseEnemyTerminationOwnershipSourceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Source;
	if (!TestTrue(TEXT("Enemy combat AI source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/AI/EnemyCombatAIComponent.cpp"), Source)))
	{
		return false;
	}

	const TCHAR* CompatibilityHandlers[] =
	{
		TEXT("UEnemyCombatAIComponent::HandleAttackConsumedInternal"),
		TEXT("UEnemyCombatAIComponent::OnAttackMontageEnded"),
		TEXT("UEnemyCombatAIComponent::OnParried"),
		TEXT("UEnemyCombatAIComponent::OnCountered")
	};
	for (const TCHAR* Handler : CompatibilityHandlers)
	{
		FString Body;
		if (!TestTrue(*FString::Printf(TEXT("Termination handler is extractable: %s"), Handler),
			ExtractFunctionBody(Source, Handler, Body)))
		{
			continue;
		}
		TestTrue(*FString::Printf(TEXT("Handler delegates to centralized termination: %s"), Handler),
			Body.Contains(TEXT("TerminateActiveAttack")));
		TestFalse(*FString::Printf(TEXT("Handler cannot release a token directly: %s"), Handler),
			Body.Contains(TEXT("ReleaseTokenAndCleanup")));
	}

	FString TerminationBody;
	if (!TestTrue(TEXT("Central termination body is extractable"), ExtractFunctionBody(
		Source, TEXT("UEnemyCombatAIComponent::TerminateActiveAttack"), TerminationBody)))
	{
		return false;
	}
	TestTrue(TEXT("Central termination owns token cleanup"),
		TerminationBody.Contains(TEXT("ReleaseTokenAndCleanup")));
	const int32 CommitIndex = TerminationBody.Find(TEXT("bAttackTerminationCommitted = true"));
	const int32 ReleaseIndex = TerminationBody.Find(TEXT("ReleaseTokenAndCleanup"));
	TestTrue(TEXT("Central termination commits idempotence before cleanup"),
		CommitIndex != INDEX_NONE && ReleaseIndex != INDEX_NONE && CommitIndex < ReleaseIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseCanonicalNotifyIdentitySourceTest,
	"KatanaCombat.Defense.Architecture.CanonicalNotifyIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseCanonicalNotifyIdentitySourceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const TCHAR* NotifySources[] =
	{
		TEXT("Source/KatanaCombat/Private/Animation/AnimNotifyState_ParryWindow.cpp"),
		TEXT("Source/KatanaCombat/Private/Animation/AnimNotifyState_CounterWindow.cpp"),
		TEXT("Source/KatanaCombat/Private/Animation/AnimNotify_AttackPhaseTransition.cpp")
	};
	for (const TCHAR* RelativePath : NotifySources)
	{
		FString Source;
		if (!TestTrue(*FString::Printf(TEXT("Notify source loads: %s"), RelativePath),
			LoadProjectSource(RelativePath, Source)))
		{
			continue;
		}
		const FString Body = StripCppComments(Source);
		TestTrue(*FString::Printf(TEXT("Notify resolves exact source identity: %s"), RelativePath),
			Body.Contains(TEXT("ResolveRuntimeNotifySourceId")));
		TestTrue(*FString::Printf(TEXT("Notify resolves montage-instance identity: %s"), RelativePath),
			Body.Contains(TEXT("ResolveRuntimeMontageInstanceId")));
	}

	FString ParrySource;
	if (TestTrue(TEXT("Parry notify source loads for exact-instance timing"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Animation/AnimNotifyState_ParryWindow.cpp"),
		ParrySource)))
	{
		const FString ParryBody = StripCppComments(ParrySource);
		TestTrue(TEXT("Parry timing resolves the exact montage instance"),
			ParryBody.Contains(TEXT("GetMontageInstanceForID")));
		TestFalse(TEXT("Parry timing never reads position from the first asset-wide instance"),
			ParryBody.Contains(TEXT("Montage_GetPosition")));
		TestFalse(TEXT("Parry timing never reads play rate from the first asset-wide instance"),
			ParryBody.Contains(TEXT("Montage_GetEffectivePlayRate")));
		TestTrue(TEXT("Parry timing accounts for skeletal-mesh animation scaling"),
			ParryBody.Contains(TEXT("GlobalAnimRateScale")));
		TestTrue(TEXT("Parry timing accounts for actor-local simulation dilation"),
			ParryBody.Contains(TEXT("CustomTimeDilation")));

		FString NotifyBeginBody;
		if (TestTrue(TEXT("Parry NotifyBegin is extractable"), ExtractFunctionBody(
			ParrySource,
			TEXT("UAnimNotifyState_ParryWindow::NotifyBegin"),
			NotifyBeginBody)))
		{
			TestTrue(TEXT("Parry begin derives the remaining exact-instance duration"),
				NotifyBeginBody.Contains(TEXT("ResolveRemainingRuntimeWindowDuration")));
			TestFalse(TEXT("Parry begin never republishes the full authored duration after a montage jump"),
				NotifyBeginBody.Contains(TEXT("ResolveRuntimeWindowDuration")));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefensePerfectParryCommitAuthoritySourceTest,
	"KatanaCombat.Defense.Architecture.PerfectParryCommitAuthority",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefensePerfectParryCommitAuthoritySourceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Source;
	if (!TestTrue(TEXT("CombatComponent source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombat/Private/Core/CombatComponent.cpp"), Source)))
	{
		return false;
	}

	FString Body;
	if (!TestTrue(TEXT("Perfect-parry commit body is extractable"), ExtractFunctionBody(
		Source, TEXT("UCombatComponent::TryCommitPerfectParry"), Body)))
	{
		return false;
	}
	TestTrue(TEXT("Perfect parry resolves through the pure defense resolver"),
		Body.Contains(TEXT("FDefenseResolver::Resolve")));
	TestTrue(TEXT("Perfect parry consumes the selected source generation"),
		Body.Contains(TEXT("ConsumeActiveAttackInternal")));
	TestFalse(TEXT("Perfect-parry commit does not invoke the legacy counter search"),
		Body.Contains(TEXT("TryCounter")));
	TestFalse(TEXT("Perfect-parry commit does not mutate compatibility parry flags"),
		Body.Contains(TEXT("SetParryWindowActive")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseAuthoringDirtyPackageRefusalOrderSourceTest,
	"KatanaCombat.Defense.Architecture.AuthoringDirtyPackageRefusalOrder",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAuthoringDirtyPackageRefusalOrderSourceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FString Source;
	if (!TestTrue(TEXT("Defense authoring source loads"), LoadProjectSource(
		TEXT("Source/KatanaCombatEditor/Private/Commandlets/Operations/DefenseProofAuthoringOperation.cpp"),
		Source)))
	{
		return false;
	}

	FString Body;
	if (!TestTrue(TEXT("Package-state helper is extractable"), ExtractFunctionBody(
		Source, TEXT("AppendPackageStateFact"), Body)))
	{
		return false;
	}

	const int32 DirtyRefusalIndex = Body.Find(TEXT("if (bRejectDirty && bDirty)"));
	const int32 MissingReturnIndex = Body.Find(TEXT("if (!bExists)"));
	TestTrue(TEXT("Unsaved dirty packages are rejected before missing-package early returns"),
		DirtyRefusalIndex != INDEX_NONE
		&& MissingReturnIndex != INDEX_NONE
		&& DirtyRefusalIndex < MissingReturnIndex);
	return true;
}
