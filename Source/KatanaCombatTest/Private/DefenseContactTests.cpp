// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatEventRecorder.h"
#include "CombatTestHelpers.h"
#include "Characters/BaseCombatCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
#include "Animation/AnimMontage.h"
#include "NiagaraSystem.h"
#include "Sound/SoundWave.h"
#include "Utilities/CombatGameplayTags.h"
#include "Containers/Ticker.h"
#include "HAL/PlatformTime.h"
#include "UObject/GarbageCollection.h"

namespace
{
FDefenseContactRequest MakeContactRequest(
	ABaseCombatCharacter* Source,
	ABaseCombatCharacter* Target,
	UAttackData* AttackData,
	const int32 TraceGeneration)
{
	if (Source && Source->WeaponComponent)
	{
		Source->WeaponComponent->SetCompatibilityTraceGenerationForTesting(TraceGeneration);
	}

	FDefenseContactRequest Request;
	FWeaponTraceInstanceId TraceId;
	TraceId.WeaponComponent = Source ? Source->WeaponComponent.Get() : nullptr;
	TraceId.TraceGeneration = TraceGeneration;
	Request.ContactId = FContactInstanceId::FromCompatibilityTrace(TraceId);

	Request.Query.Stage = EDefenseQueryStage::Contact;
	Request.Query.Attack.AttackData = AttackData;
	Request.Query.Attack.AttackType = AttackData ? AttackData->AttackType : EAttackType::None;
	Request.Query.Attack.AttackTags = AttackData ? AttackData->AttackTags : FGameplayTagContainer();
	Request.Query.Attack.AuthoredHeight = AttackData
		? AttackData->DefenseProfile.Height
		: EAttackHeight::Middle;
	Request.Query.Attack.NominalLane = AttackData
		? AttackData->DefenseProfile.NominalLane
		: EIncomingAttackLane::Center;
	Request.Query.Attack.SwingShape = AttackData
		? AttackData->DefenseProfile.SwingShape
		: ESwingDirection::Horizontal;
	Request.Query.Attack.AttackerTransform = Source ? Source->GetActorTransform() : FTransform::Identity;
	Request.Query.Attack.bAttackerAlive = Source && !Source->IsDeadOrDying();
	Request.Query.Attack.bAttackActive = true;

	Request.HitInfo = FCombatTestHelpers::CreateTestHitInfo(Source, AttackData ? AttackData->BaseDamage : 15.0f,
		Source && Target
			? (Source->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal()
			: FVector::ForwardVector,
		AttackData);
	Request.HitInfo.ImpactPoint = Target ? Target->GetActorLocation() : FVector::ZeroVector;
	Request.HitInfo.ImpactNormal = FVector::BackwardVector;
	Request.HitInfo.BoneName = TEXT("spine_03");
	Request.HitInfo.WeaponVelocity = Target && Source
		? (Target->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal() * 1000.0f
		: FVector::ForwardVector * 1000.0f;
	Request.TraceStart = Source ? Source->GetActorLocation() : FVector::ZeroVector;
	Request.TraceEnd = Target ? Target->GetActorLocation() : FVector::ForwardVector;
	Request.ActiveSourceSocket = TEXT("weapon_end");
	return Request;
}

FDefenseContactReceipt ResolveAndFinalize(
	ABaseCombatCharacter* Source,
	ABaseCombatCharacter* Target,
	const FDefenseContactRequest& Request)
{
	FDefenseContactReceipt Receipt = Source->ResolveWeaponContactCandidate(Target, Request);
	Source->FinalizeResolvedWeaponContact(Target, Receipt);
	return Receipt;
}

ABaseCombatCharacter* CreateDefenseTestCharacter(
	UWorld* World,
	const ETeamId Team,
	const FVector& Location)
{
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestPlayerCharacter(World, Location);
	Character->TeamId = Team;
	return Character;
}

FHitResult MakeWeaponContactHit(AActor* Target, const FVector& SourceLocation)
{
	FHitResult Hit;
	Hit.HitObjectHandle = FActorInstanceHandle(Target);
	Hit.TraceStart = SourceLocation;
	Hit.TraceEnd = Target ? Target->GetActorLocation() : SourceLocation + FVector::ForwardVector;
	Hit.ImpactPoint = Hit.TraceEnd;
	Hit.ImpactNormal = FVector::BackwardVector;
	Hit.BoneName = TEXT("spine_03");
	return Hit;
}

void BindRecorder(
	ABaseCombatCharacter* Source,
	ABaseCombatCharacter* Target,
	UCombatEventRecorder* Recorder)
{
	Target->HitReactionComponent->OnDamageReceived.AddDynamic(
		Recorder, &UCombatEventRecorder::HandleDamageReceived);
	Target->OnHealthChanged.AddDynamic(Recorder, &UCombatEventRecorder::HandleHealthChanged);
	Target->OnCharacterDying.AddDynamic(Recorder, &UCombatEventRecorder::HandleCharacterDying);
	Source->CombatComponent->OnAttackHit.AddDynamic(Recorder, &UCombatEventRecorder::HandleAttackHit);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactCacheLifecycleTest,
	"KatanaCombat.Defense.Contact.CacheLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactCacheLifecycleTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, nullptr, 1);
	FDefenseInteractionKey Key;
	Key.Stage = EDefenseQueryStage::Contact;
	Key.ContactInstance = Request.ContactId;
	Key.Defender = Target;

	FDefenseInteractionId FirstId;
	FDefenseContactReceipt Existing;
	TestEqual(TEXT("First registration is new"),
		Target->CombatComponent->BeginDefenseInteraction(Key, FirstId, Existing),
		EDefenseCommitStatus::NewCommit);
	TestTrue(TEXT("First interaction ID is valid"), FirstId.IsValid());
	TestFalse(TEXT("In-progress interaction is not finalized"),
		Target->CombatComponent->IsDefenseInteractionFinalized(FirstId));

	FDefenseInteractionId ReentrantId;
	TestEqual(TEXT("Synchronous duplicate sees in-progress state"),
		Target->CombatComponent->BeginDefenseInteraction(Key, ReentrantId, Existing),
		EDefenseCommitStatus::InProgress);
	TestTrue(TEXT("In-progress lookup reuses interaction ID"), ReentrantId == FirstId);

	FDefenseContactReceipt Committed;
	Committed.CommitStatus = EDefenseCommitStatus::NewCommit;
	Committed.Resolution.InteractionId = FirstId;
	Committed.AppliedDamage = 12.0f;
	Target->CombatComponent->FinalizeDefenseInteraction(FirstId, Committed);
	TestTrue(TEXT("Committed interaction generation is finalized"),
		Target->CombatComponent->IsDefenseInteractionFinalized(FirstId));

	FDefenseInteractionId CachedId;
	TestEqual(TEXT("Finalized duplicate returns cached state"),
		Target->CombatComponent->BeginDefenseInteraction(Key, CachedId, Existing),
		EDefenseCommitStatus::Cached);
	TestEqual(TEXT("Cached damage is immutable"), Existing.AppliedDamage, 12.0f);
	TestTrue(TEXT("Cached lookup reuses interaction ID"), CachedId == FirstId);

	const double TerminalNow = FPlatformTime::Seconds();
	Target->CombatComponent->MarkDefenseContactSourceTerminal(Request.ContactId, TerminalNow);
	Target->CombatComponent->SweepDefenseInteractionCache(TerminalNow + 2.0);
	FDefenseInteractionId ReplacementId;
	TestEqual(TEXT("Expired tombstone permits a new registration"),
		Target->CombatComponent->BeginDefenseInteraction(Key, ReplacementId, Existing),
		EDefenseCommitStatus::NewCommit);
	TestTrue(TEXT("Replacement receives a later epoch"), ReplacementId.Epoch > FirstId.Epoch);
	TestFalse(TEXT("Superseded interaction generation is no longer finalized"),
		Target->CombatComponent->IsDefenseInteractionFinalized(FirstId));

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactCacheSourceInvalidationTest,
	"KatanaCombat.Defense.Contact.CacheSurvivesSourceInvalidation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactCacheSourceInvalidationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, nullptr, 1);

	const FDefenseContactReceipt First = Target->ResolveAndCommitCombatContact(Request);
	TestEqual(TEXT("Initial contact commits"), First.CommitStatus, EDefenseCommitStatus::NewCommit);
	World->DestroyActor(Source);

	FDefenseInteractionKey Key;
	Key.Stage = EDefenseQueryStage::Contact;
	Key.ContactInstance = Request.ContactId;
	Key.Defender = Target;
	FDefenseInteractionId CachedId;
	FDefenseContactReceipt Cached;
	TestEqual(TEXT("Destroyed source does not erase the committed receipt"),
		Target->CombatComponent->BeginDefenseInteraction(Key, CachedId, Cached),
		EDefenseCommitStatus::Cached);
	TestEqual(TEXT("Cached receipt retains applied damage"), Cached.AppliedDamage, First.AppliedDamage);
	TestTrue(TEXT("Cache generation remains finalized after source invalidation"),
		Target->CombatComponent->IsDefenseInteractionFinalized(First.Resolution.InteractionId));

	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactCacheRetainsObjectReferencesTest,
	"KatanaCombat.Defense.Contact.CacheRetainsObjectReferences",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactCacheRetainsObjectReferencesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = NewObject<UAttackData>();
	TWeakObjectPtr<UAttackData> WeakAttack = Attack;

	FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 1);
	FDefenseInteractionKey Key;
	Key.Stage = EDefenseQueryStage::Contact;
	Key.ContactInstance = Request.ContactId;
	Key.Defender = Target;
	FDefenseContactReceipt Receipt = Target->ResolveAndCommitCombatContact(Request);
	TestEqual(TEXT("Contact commits before forced collection"),
		Receipt.CommitStatus, EDefenseCommitStatus::NewCommit);
	Source->FinalizeResolvedWeaponContact(Target, Receipt);

	Request = FDefenseContactRequest();
	Receipt = FDefenseContactReceipt();
	Attack = nullptr;
	CollectGarbage(RF_NoFlags);
	TestTrue(TEXT("Finalized receipt retains referenced attack data"), WeakAttack.IsValid());

	FDefenseInteractionId CachedId;
	FDefenseContactReceipt Cached;
	TestEqual(TEXT("Receipt remains cached after collection"),
		Target->CombatComponent->BeginDefenseInteraction(Key, CachedId, Cached),
		EDefenseCommitStatus::Cached);
	TestTrue(TEXT("Cached receipt returns the retained attack data"),
		Cached.Resolution.ActualContact.HitInfo.AttackData == WeakAttack.Get());

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactLazyGenerationSweepTest,
	"KatanaCombat.Defense.Contact.CacheLazySweepDetectsStaleTrace",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactLazyGenerationSweepTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, nullptr, 1);
	const FDefenseContactReceipt Receipt = ResolveAndFinalize(Source, Target, Request);
	TestTrue(TEXT("Current trace interaction is finalized"),
		Target->CombatComponent->IsDefenseInteractionFinalized(Receipt.Resolution.InteractionId));

	Source->WeaponComponent->SetCompatibilityTraceGenerationForTesting(2);
	const double TerminalNow = FPlatformTime::Seconds();
	Target->CombatComponent->SweepDefenseInteractionCache(TerminalNow);
	Target->CombatComponent->SweepDefenseInteractionCache(TerminalNow + 2.0);
	TestFalse(TEXT("Lazy sweep expires a superseded trace generation"),
		Target->CombatComponent->IsDefenseInteractionFinalized(Receipt.Resolution.InteractionId));

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactCacheBoundedTest,
	"KatanaCombat.Defense.Contact.CacheBoundedTerminalRecords",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactCacheBoundedTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	APlayerCharacter* TerminalSource = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);

	const FDefenseContactRequest ActiveRequest = MakeContactRequest(Source, Target, nullptr, 1);
	FDefenseInteractionKey ActiveKey;
	ActiveKey.Stage = EDefenseQueryStage::Contact;
	ActiveKey.ContactInstance = ActiveRequest.ContactId;
	ActiveKey.Defender = Target;
	FDefenseInteractionId ActiveId;
	FDefenseContactReceipt Existing;
	Target->CombatComponent->BeginDefenseInteraction(ActiveKey, ActiveId, Existing);
	FDefenseContactReceipt ActiveReceipt;
	ActiveReceipt.CommitStatus = EDefenseCommitStatus::NewCommit;
	ActiveReceipt.Resolution.InteractionId = ActiveId;
	Target->CombatComponent->FinalizeDefenseInteraction(ActiveId, ActiveReceipt);

	const double TerminalNow = FPlatformTime::Seconds();
	for (int32 Index = 2; Index <= 141; ++Index)
	{
		const FDefenseContactRequest Request = MakeContactRequest(
			TerminalSource, Target, nullptr, Index);
		FDefenseInteractionKey Key;
		Key.Stage = EDefenseQueryStage::Contact;
		Key.ContactInstance = Request.ContactId;
		Key.Defender = Target;
		FDefenseInteractionId Id;
		Target->CombatComponent->BeginDefenseInteraction(Key, Id, Existing);
		FDefenseContactReceipt Receipt;
		Receipt.CommitStatus = EDefenseCommitStatus::NewCommit;
		Receipt.Resolution.InteractionId = Id;
		Target->CombatComponent->FinalizeDefenseInteraction(Id, Receipt);
		Target->CombatComponent->MarkDefenseContactSourceTerminal(Request.ContactId, TerminalNow);
	}

	TestTrue(TEXT("Terminal records are capped while active records remain"),
		Target->CombatComponent->GetDefenseInteractionCacheSizeForTesting() <= 129);
	FDefenseInteractionId FoundActiveId;
	TestEqual(TEXT("Unmarked active record is never evicted"),
		Target->CombatComponent->BeginDefenseInteraction(ActiveKey, FoundActiveId, Existing),
		EDefenseCommitStatus::Cached);
	TestTrue(TEXT("Active interaction ID is retained"), FoundActiveId == ActiveId);

	World->DestroyActor(Source);
	World->DestroyActor(TerminalSource);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactHitIdempotenceTest,
	"KatanaCombat.Defense.Contact.HitIdempotenceAndReentry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactHitIdempotenceTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 40.0f;
	Target->HitReactionComponent->DamageResistance = 0.5f;

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	int32 ResolutionCount = 0;
	Target->CombatComponent->OnDefenseResolvedNative.AddLambda(
		[&ResolutionCount](const FDefenseResolution&) { ++ResolutionCount; });

	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 1);
	Recorder->bReenterOnDamage = true;
	Recorder->ReentryTarget = Target;
	Recorder->ReentryRequest = Request;

	const float InitialHealth = Target->CurrentHealth;
	const FDefenseContactReceipt First = ResolveAndFinalize(Source, Target, Request);
	TestEqual(TEXT("First contact commits once"), First.CommitStatus, EDefenseCommitStatus::NewCommit);
	TestEqual(TEXT("Unguarded contact resolves as hit"), First.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestEqual(TEXT("Receipt reports post-resistance damage"), First.AppliedDamage, 20.0f);
	TestEqual(TEXT("Health changes by receipt damage"), Target->CurrentHealth, InitialHealth - 20.0f);
	TestEqual(TEXT("Damage callback runs once"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Health callback runs once"), Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Source hit event runs once"), Recorder->AttackHitCount, 1);
	TestEqual(TEXT("Resolution event runs once"), ResolutionCount, 1);
	TestEqual(TEXT("Listener reentry observes finalized cache"),
		Recorder->DamageReentryReceipt.CommitStatus, EDefenseCommitStatus::Cached);
	TestEqual(TEXT("Listener reentry sees original applied damage"), Recorder->DamageReentryReceipt.AppliedDamage, 20.0f);
	TestEqual(TEXT("Impact presentation is attempted once"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 1);

	Source->FinalizeResolvedWeaponContact(Target, First);
	TestEqual(TEXT("Retained new receipt cannot replay source hit event"), Recorder->AttackHitCount, 1);
	TestEqual(TEXT("Retained new receipt cannot replay impact presentation"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 1);

	const FDefenseContactReceipt Duplicate = ResolveAndFinalize(Source, Target, Request);
	TestEqual(TEXT("Repeated trace returns cached receipt"), Duplicate.CommitStatus, EDefenseCommitStatus::Cached);
	TestEqual(TEXT("Duplicate preserves health"), Target->CurrentHealth, InitialHealth - 20.0f);
	TestEqual(TEXT("Duplicate does not replay damage callback"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Duplicate does not replay health callback"), Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Duplicate does not replay source hit event"), Recorder->AttackHitCount, 1);
	TestEqual(TEXT("Duplicate does not replay resolution event"), ResolutionCount, 1);
	TestEqual(TEXT("Duplicate does not replay impact presentation"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 1);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactCanonicalSourceReceiptTest,
	"KatanaCombat.Defense.Contact.SourceFinalizationUsesCanonicalReceipt",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactCanonicalSourceReceiptTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 70);
	FDefenseContactReceipt Receipt = Source->ResolveWeaponContactCandidate(Target, Request);
	TestEqual(TEXT("Target returns a new canonical receipt"),
		Receipt.CommitStatus, EDefenseCommitStatus::NewCommit);

	Receipt.bAcceptsWeaponHit = false;
	Receipt.bConsumesHitBudget = false;
	Receipt.Resolution.Decision.Outcome = EDefenseOutcome::IgnoredFriendly;
	Receipt.Resolution.ActualContact.HitInfo.AttackData = nullptr;
	Source->FinalizeResolvedWeaponContact(Target, Receipt);

	TestEqual(TEXT("Canonical receipt still drives source presentation"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 1);
	TestEqual(TEXT("Canonical receipt still drives source attack event"), Recorder->AttackHitCount, 1);
	TestEqual(TEXT("Canonical target gameplay still dispatches once"), Recorder->DamageReceivedCount, 1);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactContextRetentionTest,
	"KatanaCombat.Defense.Contact.ActualContextAndPredictionRetention",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactContextRetentionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->DefenseProfile.Height = EAttackHeight::Low;
	Attack->DefenseProfile.NominalLane = EIncomingAttackLane::Right;

	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>();
	Configuration->BoneHeightRows.Add({TEXT("spine_03"), EAttackHeight::High});
	Target->CombatComponent->DefenseConfigurationOverride = Configuration;

	FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 1);
	Request.HitInfo.AnimationTime = 0.42f;
	Request.HitInfo.PhaseWhenHit = EAttackPhase::Active;
	Request.HitInfo.SurfaceType = ECombatSurfaceType::Metal;
	Request.HitInfo.HitConfidence = 0.73f;
	Request.Query.Attack.PredictedContact.bIsValid = true;
	Request.Query.Attack.PredictedContact.IntendedTarget = Target;
	Request.Query.Attack.PredictedContact.ContactPoint = FVector(10.0f, 20.0f, 30.0f);
	Request.Query.Attack.PredictedContact.SourceSocket = TEXT("predicted_socket");
	Request.Query.Attack.PredictedContact.DefenderTargetBone = TEXT("predicted_bone");
	Request.Query.Attack.PredictedContact.Lane = EIncomingAttackLane::Left;
	Request.Query.Attack.PredictedContact.Height = EAttackHeight::Middle;
	Request.Query.Attack.PredictedContact.Confidence = EDefensePredictionConfidence::High;

	const FDefenseContactReceipt Receipt = ResolveAndFinalize(Source, Target, Request);
	const FActualDefenseContact& Actual = Receipt.Resolution.ActualContact;
	const FPredictedDefenseContact& Predicted = Receipt.Resolution.PredictedContact;
	TestTrue(TEXT("Actual contact is retained"), Receipt.Resolution.bHasActualContact);
	TestEqual(TEXT("Accepted trace start is retained"), Actual.TraceStart, Request.TraceStart);
	TestEqual(TEXT("Accepted trace end is retained"), Actual.TraceEnd, Request.TraceEnd);
	TestEqual(TEXT("Actual source socket comes from accepted trace"), Actual.SourceSocket, FName(TEXT("weapon_end")));
	TestEqual(TEXT("Actual hit bone is retained"), Actual.ResolvedTargetBone, FName(TEXT("spine_03")));
	TestEqual(TEXT("Exact defender bone mapping controls actual height"), Actual.Height, EAttackHeight::High);
	TestEqual(TEXT("Exact bone provenance is retained"),
		Actual.HeightProvenance, EDefenseHeightProvenance::ExactBone);
	TestEqual(TEXT("Actual trajectory resolves independently of authored lane"),
		Actual.Lane, EIncomingAttackLane::Center);
	TestEqual(TEXT("Surface metadata is retained"), Actual.HitInfo.SurfaceType, ECombatSurfaceType::Metal);
	TestEqual(TEXT("Animation time is retained"), Actual.HitInfo.AnimationTime, 0.42f);
	TestEqual(TEXT("Attack phase is retained"), Actual.HitInfo.PhaseWhenHit, EAttackPhase::Active);
	TestEqual(TEXT("Hit confidence is retained"), Actual.HitInfo.HitConfidence, 0.73f);
	TestTrue(TEXT("Prediction remains present"), Predicted.bIsValid);
	TestEqual(TEXT("Prediction contact is not overwritten by actual contact"),
		Predicted.ContactPoint, FVector(10.0f, 20.0f, 30.0f));
	TestEqual(TEXT("Prediction socket is not overwritten by actual socket"),
		Predicted.SourceSocket, FName(TEXT("predicted_socket")));
	TestEqual(TEXT("Prediction lane is not overwritten"), Predicted.Lane, EIncomingAttackLane::Left);
	TestEqual(TEXT("Prediction height is not overwritten"), Predicted.Height, EAttackHeight::Middle);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactTargetBoneFallbackTest,
	"KatanaCombat.Defense.Contact.TargetBoneFallbackPrecedence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactTargetBoneFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->DefenseProfile.DefenderTargetBoneFallback = TEXT("authored_bone");

	FDefenseContactRequest PredictedRequest = MakeContactRequest(Source, Target, Attack, 1);
	PredictedRequest.HitInfo.BoneName = NAME_None;
	PredictedRequest.Query.Attack.PredictedContact.bIsValid = true;
	PredictedRequest.Query.Attack.PredictedContact.DefenderTargetBone = TEXT("predicted_bone");
	PredictedRequest.Query.Attack.DefenderTargetBone = TEXT("snapshot_bone");
	const FDefenseContactReceipt Predicted = ResolveAndFinalize(Source, Target, PredictedRequest);
	TestEqual(TEXT("Valid prediction supplies missing actual target bone"),
		Predicted.Resolution.ActualContact.ResolvedTargetBone, FName(TEXT("predicted_bone")));

	FDefenseContactRequest SnapshotRequest = MakeContactRequest(Source, Target, Attack, 2);
	SnapshotRequest.HitInfo.BoneName = NAME_None;
	SnapshotRequest.Query.Attack.DefenderTargetBone = TEXT("snapshot_bone");
	const FDefenseContactReceipt Snapshot = ResolveAndFinalize(Source, Target, SnapshotRequest);
	TestEqual(TEXT("Attack snapshot supplies target bone when prediction has none"),
		Snapshot.Resolution.ActualContact.ResolvedTargetBone, FName(TEXT("snapshot_bone")));

	FDefenseContactRequest AuthoredRequest = MakeContactRequest(Source, Target, Attack, 3);
	AuthoredRequest.HitInfo.BoneName = NAME_None;
	const FDefenseContactReceipt Authored = ResolveAndFinalize(Source, Target, AuthoredRequest);
	TestEqual(TEXT("Attack data supplies final target bone fallback"),
		Authored.Resolution.ActualContact.ResolvedTargetBone, FName(TEXT("authored_bone")));

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactHealthReentryTest,
	"KatanaCombat.Defense.Contact.HealthListenerReentryAndAction",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactHealthReentryTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Source = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	APlayerCharacter* Target = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 1);

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	Recorder->bReenterOnHealth = true;
	Recorder->bBeginBlockOnHealth = true;
	Recorder->ReentryTarget = Target;
	Recorder->ReentryRequest = Request;

	ResolveAndFinalize(Source, Target, Request);
	TestEqual(TEXT("Health listener runs once"), Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Health listener sees finalized cached receipt"),
		Recorder->HealthReentryReceipt.CommitStatus, EDefenseCommitStatus::Cached);
	TestTrue(TEXT("Health listener can begin a later action"), Recorder->bBeginBlockResult);
	TestTrue(TEXT("Later action remains active after callback"), Target->CombatComponent->IsBlocking());

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactIgnoredOutcomesTest,
	"KatanaCombat.Defense.Contact.IgnoredOutcomes",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactIgnoredOutcomesTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Enemy, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 25.0f;
	const float InitialHealth = Target->CurrentHealth;
	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);

	const FDefenseContactReceipt Friendly = ResolveAndFinalize(
		Source, Target, MakeContactRequest(Source, Target, Attack, 1));
	TestEqual(TEXT("Enemy versus enemy is intentionally friendly"),
		Friendly.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredFriendly);
	TestFalse(TEXT("Friendly contact is not accepted"), Friendly.bAcceptsWeaponHit);
	TestEqual(TEXT("Friendly contact causes no damage"), Target->CurrentHealth, InitialHealth);
	TestEqual(TEXT("Friendly contact attempts no impact presentation"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 0);

	Target->TeamId = ETeamId::Player;
	Target->HitReactionComponent->bIsInvulnerable = true;
	const FDefenseContactReceipt Invulnerable = ResolveAndFinalize(
		Source, Target, MakeContactRequest(Source, Target, Attack, 2));
	TestEqual(TEXT("Invulnerable target is ignored canonically"),
		Invulnerable.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredInvulnerable);
	TestFalse(TEXT("Invulnerable contact does not consume budget"), Invulnerable.bConsumesHitBudget);
	TestEqual(TEXT("Invulnerable contact attempts no impact presentation"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 0);

	Target->HitReactionComponent->bIsInvulnerable = false;
	Target->HitReactionComponent->SetIFrameStateForTesting(true);
	const FDefenseContactReceipt IFrames = ResolveAndFinalize(
		Source, Target, MakeContactRequest(Source, Target, Attack, 3));
	TestEqual(TEXT("I-frame target is ignored canonically"),
		IFrames.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredInvulnerable);
	Target->HitReactionComponent->SetIFrameStateForTesting(false);

	Target->bIsDying = true;
	const FDefenseContactReceipt Dead = ResolveAndFinalize(
		Source, Target, MakeContactRequest(Source, Target, Attack, 4));
	TestEqual(TEXT("Dead or dying target is invalid"),
		Dead.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredInvalid);
	TestEqual(TEXT("Ignored contacts preserve health"), Target->CurrentHealth, InitialHealth);
	TestEqual(TEXT("Ignored contacts never reach the presentation hook"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 0);
	TestEqual(TEXT("Ignored contacts emit no damage callbacks"), Recorder->DamageReceivedCount, 0);
	TestEqual(TEXT("Ignored contacts emit no health callbacks"), Recorder->HealthChangedCount, 0);
	TestEqual(TEXT("Ignored contacts emit no source attack callbacks"), Recorder->AttackHitCount, 0);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactNeutralCompatibilityTest,
	"KatanaCombat.Defense.Contact.NeutralCompatibilityDamagePolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactNeutralCompatibilityTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Neutral, FVector(100.0f, 0.0f, 0.0f));
	ABaseCombatCharacter* Target = CreateDefenseTestCharacter(
		World, ETeamId::Neutral, FVector::ZeroVector);
	FDefenseContactRequest Request = MakeContactRequest(Source, Target, nullptr, 60);
	Request.HitInfo.Damage = 15.0f;
	const float InitialHealth = Target->CurrentHealth;

	const FDefenseContactReceipt Receipt = ResolveAndFinalize(Source, Target, Request);
	TestEqual(TEXT("Neutral compatibility contact remains a physical hit"),
		Receipt.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestEqual(TEXT("Neutral compatibility contact applies requested damage"),
		Receipt.AppliedDamage, 15.0f);
	TestEqual(TEXT("Neutral compatibility contact changes health"),
		Target->CurrentHealth, InitialHealth - 15.0f);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactBlockMatrixTest,
	"KatanaCombat.Defense.Contact.BlockAndUnblockable",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactBlockMatrixTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Source = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	APlayerCharacter* Target = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 30.0f;
	Attack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	TestTrue(TEXT("Defender enters held guard"), Target->CombatComponent->BeginBlock(Source));

	const float InitialHealth = Target->CurrentHealth;
	const FDefenseContactRequest BlockRequest = MakeContactRequest(Source, Target, Attack, 1);
	const FDefenseContactReceipt Block = ResolveAndFinalize(Source, Target, BlockRequest);
	TestEqual(TEXT("Aligned guard resolves normal block"),
		Block.Resolution.Decision.Outcome, EDefenseOutcome::NormalBlock);
	TestEqual(TEXT("Normal block suppresses damage"), Block.AppliedDamage, 0.0f);
	TestTrue(TEXT("Normal block accepts the weapon hit"), Block.bAcceptsWeaponHit);
	TestTrue(TEXT("Normal block consumes one hit budget"), Block.bConsumesHitBudget);
	TestEqual(TEXT("Normal block preserves health"), Target->CurrentHealth, InitialHealth);
	TestEqual(TEXT("Normal block presents one accepted impact"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 1);
	TestEqual(TEXT("Normal block dispatches defender presentation once"),
		Target->HitReactionComponent->GetDefensePresentationAttemptCountForTesting(), 1);
	TestEqual(TEXT("Normal block dispatches attacker response once"),
		Source->HitReactionComponent->GetAttackerResponseAttemptCountForTesting(), 1);
	TestEqual(TEXT("Normal block emits no damage callback"), Recorder->DamageReceivedCount, 0);
	TestEqual(TEXT("Normal block emits no health callback"), Recorder->HealthChangedCount, 0);
	TestFalse(TEXT("Committed defender presentation rejects the source component"),
		Source->HitReactionComponent->PlayDefensePresentation(Block.Resolution));
	TestFalse(TEXT("Committed attacker response rejects the defender component"),
		Target->HitReactionComponent->PlayAttackerResponse(Block.Resolution));
	TestEqual(TEXT("Wrong owner cannot claim a defender presentation attempt"),
		Source->HitReactionComponent->GetDefensePresentationAttemptCountForTesting(), 0);
	TestEqual(TEXT("Wrong owner cannot claim an attacker response attempt"),
		Target->HitReactionComponent->GetAttackerResponseAttemptCountForTesting(), 0);

	Source->FinalizeResolvedWeaponContact(Target, Block);
	ResolveAndFinalize(Source, Target, BlockRequest);
	TestEqual(TEXT("Retained and cached receipts cannot replay defender presentation"),
		Target->HitReactionComponent->GetDefensePresentationAttemptCountForTesting(), 1);
	TestEqual(TEXT("Retained and cached receipts cannot replay attacker response"),
		Source->HitReactionComponent->GetAttackerResponseAttemptCountForTesting(), 1);

	Attack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	const FDefenseContactReceipt Unblockable = ResolveAndFinalize(
		Source, Target, MakeContactRequest(Source, Target, Attack, 2));
	TestEqual(TEXT("Unblockable semantics override held guard"),
		Unblockable.Resolution.Decision.Outcome, EDefenseOutcome::UnblockableHit);
	TestEqual(TEXT("Unblockable hit applies authored damage"), Unblockable.AppliedDamage, 30.0f);
	TestEqual(TEXT("Unblockable hit changes health"), Target->CurrentHealth, InitialHealth - 30.0f);
	TestEqual(TEXT("Unblockable hit presents one additional impact"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 2);
	TestEqual(TEXT("Unblockable hit emits damage exactly once"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Unblockable hit emits health exactly once"), Recorder->HealthChangedCount, 1);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactConfigurationPolicyTest,
	"KatanaCombat.Defense.Contact.ConfigurationPolicy",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactConfigurationPolicyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Source = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 17.63f, 0.0f));
	APlayerCharacter* Target = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>();
	Target->CombatComponent->DefenseConfigurationOverride = Configuration;
	TestTrue(TEXT("Defender enters held guard"), Target->CombatComponent->BeginBlock(Source));

	Configuration->NormalBlockFinalTolerance = 5.0f;
	const FDefenseContactReceipt Narrow = ResolveAndFinalize(
		Source, Target, MakeContactRequest(Source, Target, Attack, 80));
	TestEqual(TEXT("Authored narrow tolerance rejects a ten-degree contact"),
		Narrow.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestEqual(TEXT("Committed decision records authored narrow tolerance"),
		Narrow.Resolution.Decision.RequiredFinalTolerance, 5.0f);

	Configuration->NormalBlockFinalTolerance = 15.0f;
	const FDefenseContactReceipt Wide = ResolveAndFinalize(
		Source, Target, MakeContactRequest(Source, Target, Attack, 81));
	TestEqual(TEXT("Authored wide tolerance accepts the same contact"),
		Wide.Resolution.Decision.Outcome, EDefenseOutcome::NormalBlock);
	TestEqual(TEXT("Committed decision records authored wide tolerance"),
		Wide.Resolution.Decision.RequiredFinalTolerance, 15.0f);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseBlockedPresentationPrecedenceTest,
	"KatanaCombat.Defense.Presentation.BlockedImpactPrecedence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseBlockedPresentationPrecedenceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	AEnemyCharacter* Source = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	APlayerCharacter* Target = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());

	UDefenseConfiguration* DefenderConfiguration = NewObject<UDefenseConfiguration>();
	UDefenseConfiguration* AttackerConfiguration = NewObject<UDefenseConfiguration>();
	Target->CombatComponent->DefenseConfigurationOverride = DefenderConfiguration;
	Source->CombatComponent->DefenseConfigurationOverride = AttackerConfiguration;

	USoundWave* ExactSound = NewObject<USoundWave>(DefenderConfiguration);
	USoundWave* AttackSound = NewObject<USoundWave>(Attack);
	USoundWave* GenericSound = NewObject<USoundWave>(DefenderConfiguration);
	USoundWave* DefaultSound = NewObject<USoundWave>(DefenderConfiguration);
	UNiagaraSystem* ExactVFX = NewObject<UNiagaraSystem>(DefenderConfiguration);
	UNiagaraSystem* AttackVFX = NewObject<UNiagaraSystem>(Attack);
	UNiagaraSystem* GenericVFX = NewObject<UNiagaraSystem>(DefenderConfiguration);
	UNiagaraSystem* DefaultVFX = NewObject<UNiagaraSystem>(DefenderConfiguration);

	FDefensePresentationRow GenericRow;
	GenericRow.RowName = TEXT("GenericBlock");
	GenericRow.Outcome = EDefenseOutcome::NormalBlock;
	GenericRow.Payload.bOverrideImpactAudio = true;
	GenericRow.Payload.ImpactAudio.ImpactSound = GenericSound;
	GenericRow.Payload.bOverrideImpactVFX = true;
	GenericRow.Payload.ImpactVFX.ImpactVFX = GenericVFX;

	FDefensePresentationRow ExactRow = GenericRow;
	ExactRow.RowName = TEXT("ExactBlock");
	ExactRow.bMatchAnyHeight = false;
	ExactRow.Height = EAttackHeight::Middle;
	ExactRow.bMatchAnyLane = false;
	ExactRow.Lane = EIncomingAttackLane::Center;
	ExactRow.bMatchAnySwingShape = false;
	ExactRow.SwingShape = ESwingDirection::Horizontal;
	ExactRow.Payload.ImpactAudio.ImpactSound = ExactSound;
	ExactRow.Payload.ImpactVFX.ImpactVFX = ExactVFX;
	DefenderConfiguration->DefenderPresentationRows = {GenericRow, ExactRow};

	FAttackerResponsePresentationRow RecoilRow;
	RecoilRow.RowName = TEXT("AttackerOwnedRecoil");
	RecoilRow.Response = EAttackerResponse::Recoil;
	RecoilRow.Payload.Montage = NewObject<UAnimMontage>(AttackerConfiguration);
	FAttackerResponsePresentationRow ExactRecoilWithoutMontage = RecoilRow;
	ExactRecoilWithoutMontage.RowName = TEXT("ExactRecoilWithoutMontage");
	ExactRecoilWithoutMontage.bMatchAnyHeight = false;
	ExactRecoilWithoutMontage.Height = EAttackHeight::Middle;
	ExactRecoilWithoutMontage.bMatchAnyLane = false;
	ExactRecoilWithoutMontage.Lane = EIncomingAttackLane::Center;
	ExactRecoilWithoutMontage.bMatchAnySwingShape = false;
	ExactRecoilWithoutMontage.SwingShape = ESwingDirection::Horizontal;
	ExactRecoilWithoutMontage.Payload.Montage = nullptr;
	AttackerConfiguration->AttackerResponseRows = {RecoilRow, ExactRecoilWithoutMontage};

	TestTrue(TEXT("Defender enters held guard"), Target->CombatComponent->BeginBlock(Source));
	FDefenseContactReceipt Exact = Source->ResolveWeaponContactCandidate(
		Target, MakeContactRequest(Source, Target, Attack, 90));
	TestEqual(TEXT("Exact defense row is committed"), Exact.Resolution.PresentationRow,
		FName(TEXT("ExactBlock")));
	TestEqual(TEXT("Exact row retains exact fallback provenance"),
		Exact.Resolution.PresentationFallback, EDefensePresentationFallbackLevel::Exact);
	TestTrue(TEXT("Exact defense-row audio outranks every fallback"),
		Exact.Resolution.Presentation.ImpactAudio.ImpactSound == ExactSound);
	TestTrue(TEXT("Exact defense-row VFX outranks every fallback"),
		Exact.Resolution.Presentation.ImpactVFX.ImpactVFX == ExactVFX);
	TestEqual(TEXT("Attacker response is selected from attacker configuration"),
		Exact.Resolution.AttackerPresentationRow, FName(TEXT("AttackerOwnedRecoil")));
	TestTrue(TEXT("Committed attacker payload retains attacker montage"),
		Exact.Resolution.AttackerPresentation.Montage == RecoilRow.Payload.Montage);
	TestEqual(TEXT("Missing exact recoil montage commits the authored generic recoil"),
		Exact.Resolution.AttackerPresentationFallback,
		EDefensePresentationFallbackLevel::Generic);
	Source->FinalizeResolvedWeaponContact(Target, Exact);

	DefenderConfiguration->DefenderPresentationRows[1].Payload.bOverrideImpactAudio = false;
	DefenderConfiguration->DefenderPresentationRows[1].Payload.bOverrideImpactVFX = false;
	Attack->DefenseProfile.bOverrideBlockedImpactAudio = true;
	Attack->DefenseProfile.BlockedImpactAudio.ImpactSound = AttackSound;
	Attack->DefenseProfile.bOverrideBlockedImpactVFX = true;
	Attack->DefenseProfile.BlockedImpactVFX.ImpactVFX = AttackVFX;
	FDefenseContactReceipt AttackOverride = Source->ResolveWeaponContactCandidate(
		Target, MakeContactRequest(Source, Target, Attack, 91));
	TestTrue(TEXT("Attack-profile audio wins when exact row has no audio override"),
		AttackOverride.Resolution.Presentation.ImpactAudio.ImpactSound == AttackSound);
	TestTrue(TEXT("Attack-profile VFX wins when exact row has no VFX override"),
		AttackOverride.Resolution.Presentation.ImpactVFX.ImpactVFX == AttackVFX);
	Source->FinalizeResolvedWeaponContact(Target, AttackOverride);

	Attack->DefenseProfile.bOverrideBlockedImpactAudio = false;
	Attack->DefenseProfile.bOverrideBlockedImpactVFX = false;
	FDefenseContactReceipt GenericImpactFallback = Source->ResolveWeaponContactCandidate(
		Target, MakeContactRequest(Source, Target, Attack, 94));
	TestEqual(TEXT("Specialized row remains the committed animation selection"),
		GenericImpactFallback.Resolution.PresentationRow, FName(TEXT("ExactBlock")));
	TestTrue(TEXT("Generic row audio fills a missing specialized impact override"),
		GenericImpactFallback.Resolution.Presentation.ImpactAudio.ImpactSound == GenericSound);
	TestTrue(TEXT("Generic row VFX fills a missing specialized impact override"),
		GenericImpactFallback.Resolution.Presentation.ImpactVFX.ImpactVFX == GenericVFX);
	Source->FinalizeResolvedWeaponContact(Target, GenericImpactFallback);

	DefenderConfiguration->DefenderPresentationRows = {GenericRow};
	FDefenseContactReceipt Generic = Source->ResolveWeaponContactCandidate(
		Target, MakeContactRequest(Source, Target, Attack, 92));
	TestEqual(TEXT("Wildcard row is committed as generic provenance"),
		Generic.Resolution.PresentationFallback, EDefensePresentationFallbackLevel::Generic);
	TestTrue(TEXT("Generic defense-row audio follows attack fallback"),
		Generic.Resolution.Presentation.ImpactAudio.ImpactSound == GenericSound);
	TestTrue(TEXT("Generic defense-row VFX follows attack fallback"),
		Generic.Resolution.Presentation.ImpactVFX.ImpactVFX == GenericVFX);
	Source->FinalizeResolvedWeaponContact(Target, Generic);

	DefenderConfiguration->DefenderPresentationRows[0].Payload.bOverrideImpactAudio = false;
	DefenderConfiguration->DefenderPresentationRows[0].Payload.bOverrideImpactVFX = false;
	DefenderConfiguration->DefaultBlockImpactAudio.ImpactSound = DefaultSound;
	DefenderConfiguration->DefaultBlockImpactVFX.ImpactVFX = DefaultVFX;
	FDefenseContactReceipt Default = Source->ResolveWeaponContactCandidate(
		Target, MakeContactRequest(Source, Target, Attack, 93));
	TestTrue(TEXT("Defense default audio follows generic row fallback"),
		Default.Resolution.Presentation.ImpactAudio.ImpactSound == DefaultSound);
	TestTrue(TEXT("Defense default VFX follows generic row fallback"),
		Default.Resolution.Presentation.ImpactVFX.ImpactVFX == DefaultVFX);
	TestEqual(TEXT("Actual target bone remains authoritative across asset fallback"),
		Default.Resolution.ActualContact.HitInfo.BoneName, FName(TEXT("spine_03")));
	Source->FinalizeResolvedWeaponContact(Target, Default);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactCompatibilityAndOrderingTest,
	"KatanaCombat.Defense.Contact.CompatibilityAndSameFrameOrdering",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactCompatibilityAndOrderingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	const float InitialHealth = Target->CurrentHealth;

	FDefenseContactRequest Compatibility = MakeContactRequest(Source, Target, nullptr, 1);
	Compatibility.HitInfo.Damage = 15.0f;
	const FDefenseContactReceipt First = ResolveAndFinalize(Source, Target, Compatibility);
	TestEqual(TEXT("Null AttackData compatibility contact can still hit"),
		First.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestEqual(TEXT("Compatibility contact reports applied damage"), First.AppliedDamage, 15.0f);

	Target->CombatComponent->BeginBlock(Source);
	const FDefenseContactReceipt Retroactive = ResolveAndFinalize(Source, Target, Compatibility);
	TestEqual(TEXT("Later guard cannot rewrite an already committed contact"),
		Retroactive.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestEqual(TEXT("Later guard receives cached contact"),
		Retroactive.CommitStatus, EDefenseCommitStatus::Cached);

	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	FDefenseContactRequest Consumed = MakeContactRequest(Source, Target, Attack, 2);
	Consumed.Query.Attack.bAttackConsumed = true;
	const FDefenseContactReceipt IgnoredConsumed = ResolveAndFinalize(Source, Target, Consumed);
	TestEqual(TEXT("Prior source consumption wins over later physical contact"),
		IgnoredConsumed.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredConsumed);
	TestFalse(TEXT("Consumed attack contact is not accepted"), IgnoredConsumed.bAcceptsWeaponHit);
	TestEqual(TEXT("Only first compatibility hit changes health"), Target->CurrentHealth, InitialHealth - 15.0f);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactIdentityValidationTest,
	"KatanaCombat.Defense.Contact.IdentitySourceAndGenerationValidation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactIdentityValidationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	ABaseCombatCharacter* ForeignSource = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 100.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	const float InitialHealth = Target->CurrentHealth;

	FDefenseContactRequest Mismatched = MakeContactRequest(Source, Target, nullptr, 40);
	ForeignSource->WeaponComponent->SetCompatibilityTraceGenerationForTesting(41);
	FWeaponTraceInstanceId ForeignTrace;
	ForeignTrace.WeaponComponent = ForeignSource->WeaponComponent;
	ForeignTrace.TraceGeneration = 41;
	Mismatched.ContactId = FContactInstanceId::FromCompatibilityTrace(ForeignTrace);
	const FDefenseContactReceipt MismatchedReceipt = ResolveAndFinalize(Source, Target, Mismatched);
	TestEqual(TEXT("Foreign weapon identity is rejected before registration"),
		MismatchedReceipt.CommitStatus, EDefenseCommitStatus::RejectedBeforeRegistration);
	TestEqual(TEXT("Rejected foreign identity preserves health"), Target->CurrentHealth, InitialHealth);

	const FDefenseContactRequest Legitimate = MakeContactRequest(
		ForeignSource, Target, nullptr, 41);
	const FDefenseContactReceipt LegitimateReceipt = ResolveAndFinalize(
		ForeignSource, Target, Legitimate);
	TestEqual(TEXT("Rejected foreign identity does not poison the legitimate key"),
		LegitimateReceipt.CommitStatus, EDefenseCommitStatus::NewCommit);
	TestEqual(TEXT("Legitimate compatibility contact remains a hit"),
		LegitimateReceipt.Resolution.Decision.Outcome, EDefenseOutcome::Hit);

	const FDefenseContactRequest Stale = MakeContactRequest(Source, Target, nullptr, 42);
	Source->WeaponComponent->SetCompatibilityTraceGenerationForTesting(43);
	const FDefenseContactReceipt StaleReceipt = ResolveAndFinalize(Source, Target, Stale);
	TestEqual(TEXT("Superseded compatibility generation is consumed"),
		StaleReceipt.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredConsumed);
	TestEqual(TEXT("Only the legitimate contact changes health"),
		Target->CurrentHealth, InitialHealth - 15.0f);

	World->DestroyActor(Source);
	World->DestroyActor(ForeignSource);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactSnapshotParticipantRevalidationTest,
	"KatanaCombat.Defense.Contact.SnapshotParticipantRevalidation",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactSnapshotParticipantRevalidationTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ADefenseDestructiveTeamQueryCharacter* Source =
		World->SpawnActor<ADefenseDestructiveTeamQueryCharacter>(
			ADefenseDestructiveTeamQueryCharacter::StaticClass(),
			FVector(100.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator);
	Source->CombatSettings = FCombatTestHelpers::CreateTestCombatSettings();
	Source->TeamId = ETeamId::Player;
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 25.0f;
	const float InitialHealth = Target->CurrentHealth;
	Source->ActorToDestroyOnHostilityQuery = Target;

	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 50);
	const FDefenseContactReceipt Receipt = Source->ResolveWeaponContactCandidate(Target, Request);
	TestFalse(TEXT("Team query invalidates the target"), IsValid(Target));
	TestEqual(TEXT("Invalidated snapshot resolves without contact side effects"),
		Receipt.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredInvalid);
	TestEqual(TEXT("Invalidated snapshot applies no damage"), Receipt.AppliedDamage, 0.0f);
	TestEqual(TEXT("Invalidated snapshot preserves committed health"), Target->CurrentHealth, InitialHealth);

	World->DestroyActor(Source);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactLethalEventTest,
	"KatanaCombat.Defense.Contact.LethalEventIdempotence",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactLethalEventTest::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("[HEALTH] EnemyCharacter_0 DIED!"),
		EAutomationExpectedErrorFlags::Contains, 1);
	AddExpectedErrorPlain(TEXT("[HitReaction] Unknown PlayDeathReaction"),
		EAutomationExpectedErrorFlags::Contains, 1);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	Target->SetHealth(10.0f);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 30.0f;

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 1);
	Recorder->bReenterOnDying = true;
	Recorder->ReentryTarget = Target;
	Recorder->ReentryRequest = Request;
	const FDefenseContactReceipt First = ResolveAndFinalize(Source, Target, Request);
	TestEqual(TEXT("Lethal receipt reports health actually removed"), First.AppliedDamage, 10.0f);
	TestEqual(TEXT("Lethal contact commits zero health"), Target->CurrentHealth, 0.0f);
	TestTrue(TEXT("Lethal contact commits dying state before return"), Target->IsDying());
	TestEqual(TEXT("Lethal contact emits damage once"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Lethal contact emits health once"), Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Lethal contact emits dying once"), Recorder->CharacterDyingCount, 1);
	TestEqual(TEXT("Dying listener sees finalized cached receipt"),
		Recorder->DyingReentryReceipt.CommitStatus, EDefenseCommitStatus::Cached);

	ResolveAndFinalize(Source, Target, Request);
	TestEqual(TEXT("Duplicate lethal contact does not replay damage"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Duplicate lethal contact does not replay health"), Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Duplicate lethal contact does not replay dying"), Recorder->CharacterDyingCount, 1);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactSourceInvalidFallbackTest,
	"KatanaCombat.Defense.Contact.SourceInvalidUsesUnscaledFallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactSourceInvalidFallbackTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;
	Attack->MaxHitCount = 1;
	Source->ConfigureResolvedWeaponImpactForTesting(true, false);

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	int32 ResolutionCount = 0;
	Target->CombatComponent->OnDefenseResolvedNative.AddLambda(
		[&ResolutionCount](const FDefenseResolution&) { ++ResolutionCount; });
	const float InitialHealth = Target->CurrentHealth;

	Source->WeaponComponent->ProcessHitForTesting(
		MakeWeaponContactHit(Target, Source->GetActorLocation()), Attack);
	TestFalse(TEXT("Source is invalidated during presentation"), IsValid(Source));
	TestEqual(TEXT("Weapon accounting precedes presentation"),
		Source->GetAcceptedHitCountObservedDuringImpactForTesting(), 1);
	TestEqual(TEXT("Gameplay mutation is committed before fallback"),
		Target->CurrentHealth, InitialHealth - 20.0f);
	TestEqual(TEXT("Source invalidation defers damage event"), Recorder->DamageReceivedCount, 0);
	TestEqual(TEXT("Source invalidation defers health event"), Recorder->HealthChangedCount, 0);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Fallback emits damage exactly once"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Fallback emits health exactly once"), Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Fallback emits resolution exactly once"), ResolutionCount, 1);
	TestEqual(TEXT("Destroyed source emits no attack event"), Recorder->AttackHitCount, 0);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Fallback is one-shot"), ResolutionCount, 1);

	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactTargetInvalidBeforeAccountingTest,
	"KatanaCombat.Defense.Contact.TargetInvalidBeforeSourceAccounting",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactTargetInvalidBeforeAccountingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;
	Attack->MaxHitCount = 1;
	Target->SetDestroyAfterDefenseCommitForTesting(true);

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	Source->WeaponComponent->ProcessHitForTesting(
		MakeWeaponContactHit(Target, Source->GetActorLocation()), Attack);

	TestFalse(TEXT("Target is invalid before source accounting"), IsValid(Target));
	TestEqual(TEXT("Invalidated target does not consume hit budget"),
		Source->WeaponComponent->GetAcceptedHitCountForTesting(), 0);
	TestEqual(TEXT("Invalidated target does not enter weapon dedupe"),
		Source->WeaponComponent->GetHitActorCount(), 0);
	TestEqual(TEXT("Invalidated target emits no source attack event"), Recorder->AttackHitCount, 0);
	TestEqual(TEXT("Invalidated target attempts no source presentation"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 0);

	World->DestroyActor(Source);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactSourceInvalidBeforeAccountingTest,
	"KatanaCombat.Defense.Contact.SourceInvalidBeforeWeaponAccountingUsesFallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactSourceInvalidBeforeAccountingTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UWeaponComponent* Weapon = Source->WeaponComponent;
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;
	Attack->MaxHitCount = 1;
	Target->SetActorToDestroyAfterDefenseCommitForTesting(Source);

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	int32 ResolutionCount = 0;
	Target->CombatComponent->OnDefenseResolvedNative.AddLambda(
		[&ResolutionCount](const FDefenseResolution&) { ++ResolutionCount; });
	const float InitialHealth = Target->CurrentHealth;
	Weapon->ProcessHitForTesting(MakeWeaponContactHit(Target, Source->GetActorLocation()), Attack);

	TestFalse(TEXT("Source is invalid before weapon accounting"), IsValid(Source));
	TestEqual(TEXT("Invalid source does not consume hit budget"),
		Weapon->GetAcceptedHitCountForTesting(), 0);
	TestEqual(TEXT("Invalid source does not update weapon dedupe"), Weapon->GetHitActorCount(), 0);
	TestEqual(TEXT("Target gameplay remains committed"),
		Target->CurrentHealth, InitialHealth - 20.0f);
	TestEqual(TEXT("Fallback has not fired synchronously"), Recorder->DamageReceivedCount, 0);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Fallback emits target damage once"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Fallback emits target health once"), Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Fallback emits target resolution once"), ResolutionCount, 1);
	TestEqual(TEXT("Invalid source emits no source attack event"), Recorder->AttackHitCount, 0);

	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactTargetDestroyedDuringDamageTest,
	"KatanaCombat.Defense.Contact.TargetDestroyedSuppressesRemainingDispatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactTargetDestroyedDuringDamageTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;
	Source->ConfigureResolvedWeaponImpactForTesting(false, false);

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	Recorder->bDestroyOnDamage = true;
	Recorder->ActorToDestroy = Target;
	int32 ResolutionCount = 0;
	Target->CombatComponent->OnDefenseResolvedNative.AddLambda(
		[&ResolutionCount](const FDefenseResolution&) { ++ResolutionCount; });

	Source->WeaponComponent->ProcessHitForTesting(
		MakeWeaponContactHit(Target, Source->GetActorLocation()), Attack);
	TestEqual(TEXT("Presentation is attempted once before target dispatch"),
		Source->GetResolvedWeaponImpactAttemptCountForTesting(), 1);
	TestEqual(TEXT("Damage listener destroys target once"), Recorder->DamageReceivedCount, 1);
	TestFalse(TEXT("Target is invalid after listener"), IsValid(Target));
	TestEqual(TEXT("Destroyed target suppresses health event"), Recorder->HealthChangedCount, 0);
	TestEqual(TEXT("Destroyed target suppresses resolution event"), ResolutionCount, 0);
	TestEqual(TEXT("Destroyed target suppresses source attack event"), Recorder->AttackHitCount, 0);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Fallback does not replay destroyed-target work"), Recorder->DamageReceivedCount, 1);

	World->DestroyActor(Source);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactTargetDestroyedDuringHealthTest,
	"KatanaCombat.Defense.Contact.HealthListenerDestructionSuppressesRemainingDispatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactTargetDestroyedDuringHealthTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	Recorder->bDestroyOnHealth = true;
	Recorder->ActorToDestroy = Target;
	int32 ResolutionCount = 0;
	Target->CombatComponent->OnDefenseResolvedNative.AddLambda(
		[&ResolutionCount](const FDefenseResolution&) { ++ResolutionCount; });

	Source->WeaponComponent->ProcessHitForTesting(
		MakeWeaponContactHit(Target, Source->GetActorLocation()), Attack);
	TestEqual(TEXT("Damage event precedes health destruction"), Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Health listener destroys target once"), Recorder->HealthChangedCount, 1);
	TestFalse(TEXT("Target is invalid after health listener"), IsValid(Target));
	TestEqual(TEXT("Health destruction suppresses resolution event"), ResolutionCount, 0);
	TestEqual(TEXT("Health destruction suppresses source attack event"), Recorder->AttackHitCount, 0);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Fallback does not replay health-destruction work"), Recorder->HealthChangedCount, 1);

	World->DestroyActor(Source);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactTargetDestroyedDuringDyingTest,
	"KatanaCombat.Defense.Contact.DyingListenerDestructionSuppressesRemainingDispatch",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactTargetDestroyedDuringDyingTest::RunTest(const FString& Parameters)
{
	AddExpectedErrorPlain(TEXT("[HEALTH] EnemyCharacter_0 DIED!"),
		EAutomationExpectedErrorFlags::Contains, 1);
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	ABaseCombatCharacter* Source = CreateDefenseTestCharacter(
		World, ETeamId::Player, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	Target->SetHealth(10.0f);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	Attack->BaseDamage = 20.0f;

	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	Recorder->bDestroyOnDying = true;
	Recorder->ActorToDestroy = Target;
	int32 ResolutionCount = 0;
	Target->CombatComponent->OnDefenseResolvedNative.AddLambda(
		[&ResolutionCount](const FDefenseResolution&) { ++ResolutionCount; });

	Source->WeaponComponent->ProcessHitForTesting(
		MakeWeaponContactHit(Target, Source->GetActorLocation()), Attack);
	TestEqual(TEXT("Lethal contact emits damage before dying destruction"),
		Recorder->DamageReceivedCount, 1);
	TestEqual(TEXT("Lethal contact emits health before dying destruction"),
		Recorder->HealthChangedCount, 1);
	TestEqual(TEXT("Dying listener destroys target once"), Recorder->CharacterDyingCount, 1);
	TestFalse(TEXT("Target is invalid after dying listener"), IsValid(Target));
	TestEqual(TEXT("Dying destruction suppresses resolution event"), ResolutionCount, 0);
	TestEqual(TEXT("Dying destruction suppresses source attack event"), Recorder->AttackHitCount, 0);

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Fallback does not replay dying-destruction work"),
		Recorder->CharacterDyingCount, 1);

	World->DestroyActor(Source);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseContactExpiredFallbackSuppressionTest,
	"KatanaCombat.Defense.Contact.ExpiredGenerationSuppressesFallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseContactExpiredFallbackSuppressionTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Source = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	AEnemyCharacter* Target = FCombatTestHelpers::CreateTestEnemyCharacter(World);
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack();
	UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
	BindRecorder(Source, Target, Recorder);
	const FDefenseContactRequest Request = MakeContactRequest(Source, Target, Attack, 1);

	const FDefenseContactReceipt Receipt = Target->ResolveAndCommitCombatContact(Request);
	const double TerminalNow = FPlatformTime::Seconds();
	Target->CombatComponent->MarkDefenseContactSourceTerminal(Request.ContactId, TerminalNow);
	Target->CombatComponent->SweepDefenseInteractionCache(TerminalNow + 2.0);
	TestFalse(TEXT("Expired interaction generation is no longer dispatchable"),
		Target->CombatComponent->IsDefenseInteractionFinalized(Receipt.Resolution.InteractionId));

	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Expired fallback does not emit damage"), Recorder->DamageReceivedCount, 0);
	TestEqual(TEXT("Expired fallback does not emit health"), Recorder->HealthChangedCount, 0);

	World->DestroyActor(Source);
	World->DestroyActor(Target);
	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
