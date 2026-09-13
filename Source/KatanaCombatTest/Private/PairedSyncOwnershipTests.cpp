// Copyright Epic Games, Inc. All Rights Reserved.
#include "CombatTestHelpers.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Data/PairedAnimationData.h"

namespace
{
struct FSyncAlignmentFixture
{
	UWorld* World = nullptr;
	APlayerCharacter* Owner = nullptr;
	AEnemyCharacter* Victim = nullptr;
	UPairedAnimationData* Data = nullptr;
	UAnimNotifyState_PairedAnimationSync* Notify = nullptr;

	bool Initialize(bool bBegin = true)
	{
		World = FCombatTestHelpers::CreateTestWorld();
		Owner = FCombatTestHelpers::CreateTestPlayerCharacter(World);
		Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(140, 0, 0));
		if (!Owner || !Victim) { return false; }
		Data = NewObject<UPairedAnimationData>();
		Data->bApplySlowMotion = false;
		Data->AttackerMontage = NewObject<UAnimMontage>(Data);
		Data->VictimMontage = NewObject<UAnimMontage>(Data);
		Notify = NewObject<UAnimNotifyState_PairedAnimationSync>();
		Notify->bApplyDamage = false;
		Notify->bLogMisalignment = false;
		Owner->PairedAnimationComponent->AddPairedPartner(Victim);
		Victim->PairedAnimationComponent->AddPairedPartner(Owner);
		if (bBegin) { Owner->PairedAnimationComponent->BeginPairedAnimation(Data, EPairedReactionType::Finisher, false); }
		ResetPositions();
		return true;
	}

	void ResetPositions()
	{
		Owner->SetActorLocation(FVector::ZeroVector, false, nullptr, ETeleportType::TeleportPhysics);
		Victim->SetActorLocation(FVector(140, 0, 0), false, nullptr, ETeleportType::TeleportPhysics);
	}
	void Emit(bool bOwner)
	{
		Notify->NotifyBegin(bOwner ? Owner->GetMesh() : Victim->GetMesh(),
			bOwner ? Data->AttackerMontage : Data->VictimMontage, .08f, FAnimNotifyEventReference());
	}
	~FSyncAlignmentFixture()
	{
		if (Owner) { Owner->PairedAnimationComponent->CancelPairedAnimation(0); }
		FCombatTestHelpers::DestroyTestWorld(World);
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedSyncNotifyOrder,
	"KatanaCombat.PairedAnimation.SyncAlignment.NotifyOrder", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPairedSyncNotifyOrder::RunTest(const FString&)
{
	for (const bool bOwnerFirst : {true, false})
	{
		FSyncAlignmentFixture F;
		if (!TestTrue(TEXT("Pair initialized"), F.Initialize())) { return false; }
		F.Emit(bOwnerFirst); F.Emit(!bOwnerFirst);
		TestTrue(TEXT("Notify order cannot reposition the sequence owner"), F.Owner->GetActorLocation().Equals(FVector::ZeroVector));
		TestTrue(TEXT("Both orders apply the same correction to the accepted victim"), F.Victim->GetActorLocation().Equals(FVector(80, 0, 0)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedSyncAcceptedTarget,
	"KatanaCombat.PairedAnimation.SyncAlignment.AcceptedTarget", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPairedSyncAcceptedTarget::RunTest(const FString&)
{
	FSyncAlignmentFixture F;
	if (!TestTrue(TEXT("Pair initialized"), F.Initialize())) { return false; }
	const FVector BystanderPosition(0, 130, 0);
	auto* Bystander = FCombatTestHelpers::CreateTestEnemyCharacter(F.World, BystanderPosition);
	if (!TestNotNull(TEXT("Bystander exists"), Bystander)) { return false; }
	// Spawn collision adjustment is not part of the notify contract under test.
	Bystander->SetActorLocation(BystanderPosition, false, nullptr, ETeleportType::TeleportPhysics);
	auto* Paired = F.Owner->PairedAnimationComponent.Get();
	Paired->AddPairedPartner(Bystander);
	Paired->RemovePairedPartner(F.Victim); Paired->AddPairedPartner(F.Victim);
	F.Emit(true);
	TestTrue(TEXT("First tracked bystander is not the correction target"), Bystander->GetActorLocation().Equals(BystanderPosition));
	TestTrue(TEXT("Accepted target remains stable after partner-list reordering"), F.Victim->GetActorLocation().Equals(FVector(80, 0, 0)));
	F.ResetPositions(); Paired->RemovePairedPartner(F.Victim); F.Emit(true);
	TestTrue(TEXT("Removed target is not corrected"), F.Victim->GetActorLocation().Equals(FVector(140, 0, 0)));
	TestTrue(TEXT("Missing target cannot fall back to a bystander"), Bystander->GetActorLocation().Equals(BystanderPosition));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedSyncInactiveParticipation,
	"KatanaCombat.PairedAnimation.SyncAlignment.InactiveParticipation", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPairedSyncInactiveParticipation::RunTest(const FString&)
{
	FSyncAlignmentFixture F;
	if (!TestTrue(TEXT("Participants initialized"), F.Initialize(false))) { return false; }
	F.Emit(false); F.Emit(true);
	TestTrue(TEXT("Partner tracking alone cannot authorize owner movement"), F.Owner->GetActorLocation().Equals(FVector::ZeroVector));
	TestTrue(TEXT("Partner tracking alone cannot authorize victim movement"), F.Victim->GetActorLocation().Equals(FVector(140, 0, 0)));
	F.Owner->PairedAnimationComponent->BeginPairedAnimation(F.Data, EPairedReactionType::Counter, false);
	F.Owner->PairedAnimationComponent->EndPairedAnimation();
	F.ResetPositions(); F.Emit(true); F.Emit(false);
	TestTrue(TEXT("Ended generation cannot correct the victim"), F.Victim->GetActorLocation().Equals(FVector(140, 0, 0)));
	TestTrue(TEXT("Ended participation cannot correct the owner"), F.Owner->GetActorLocation().Equals(FVector::ZeroVector));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedSyncCorrectionGates,
	"KatanaCombat.PairedAnimation.SyncAlignment.AuthoredGates", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FPairedSyncCorrectionGates::RunTest(const FString&)
{
	FSyncAlignmentFixture F;
	if (!TestTrue(TEXT("Pair initialized"), F.Initialize())) { return false; }
	for (int32 Gate = 0; Gate < 5; ++Gate)
	{
		F.ResetPositions();
		F.Notify->bValidateAlignment = Gate != 0;
		F.Notify->bIsPrimarySyncPoint = Gate != 1;
		F.Notify->bNudgeOnMinorMisalignment = Gate != 2;
		const FVector Position(Gate == 3 ? 90 : Gate == 4 ? 160 : 140, 0, 0);
		F.Victim->SetActorLocation(Position);
		F.Emit(true);
		TestTrue(TEXT("Disabled validation/primary/nudge and out-of-band distances preserve position"), F.Victim->GetActorLocation().Equals(Position));
	}
	return true;
}
