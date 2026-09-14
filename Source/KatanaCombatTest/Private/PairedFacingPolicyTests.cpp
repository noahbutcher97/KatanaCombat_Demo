// Copyright Epic Games, Inc. All Rights Reserved.
#include "CombatTestHelpers.h"
#include "Core/TargetingComponent.h"
#include "MotionWarpingComponent.h"

namespace
{
struct FPairedFacingFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Owner = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 90));
	AEnemyCharacter* Partner = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(200, 0, 200));
	~FPairedFacingFixture() { FCombatTestHelpers::DestroyTestWorld(World); }
	UMotionWarpingComponent* Warp() const { return Owner->FindComponentByClass<UMotionWarpingComponent>(); }
	bool Start(bool bVictim, const FPairedWarpConfig& Config, AActor* Target)
	{
		return bVictim ? Owner->TargetingComponent->SetupVictimWarp(Target, Config)
			: Owner->TargetingComponent->SetupAttackerPairedWarp(Target, Config);
	}
	void Clear() { Owner->TargetingComponent->ClearVictimWarp(); Owner->TargetingComponent->ClearAttackerPairedWarp(); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedFacingRolePolicies,
	"KatanaCombat.Targeting.PairedFacing.RolePoliciesAndRefresh", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedFacingRolePolicies::RunTest(const FString&)
{
	FPairedFacingFixture F;
	FPairedWarpConfig Config; Config.RelativeOffset = FVector(40, 30, 0); Config.bAdjustToTerrain = false; Config.MaxWarpDistance = 0;
	TestEqual(TEXT("Old assets retain positional facing"), Config.FacingPolicy, EPairedFacingPolicy::FacePartner);
	for (bool bVictim : {false, true})
	{
		for (EPairedFacingPolicy Policy : {EPairedFacingPolicy::FacePartner, EPairedFacingPolicy::FaceAwayFromPartner, EPairedFacingPolicy::MatchPartnerHeading})
		{
			F.Owner->SetActorRotation(FRotator(0, 37, 0));
			F.Partner->SetActorLocation(FVector(200, 0, 200)); F.Partner->SetActorRotation(FRotator(25, 90, 15));
			Config.FacingPolicy = Policy;
			TestTrue(TEXT("Valid paired policy starts"), F.Start(bVictim, Config, F.Partner));
			for (int32 Refresh = 0; Refresh < 2; ++Refresh)
			{
				if (Refresh)
				{
					F.Partner->SetActorLocation(FVector(80, 70, 350)); F.Partner->SetActorRotation(FRotator(-20, -45, 10));
					F.Warp()->OnPreUpdate.Broadcast(F.Warp());
				}
				const auto* Target = F.Warp()->FindWarpTarget(Config.WarpTargetName);
				if (!TestNotNull(TEXT("Published role target"), Target)) { continue; }
				const double PositionalYaw = bVictim ? (Refresh ? 171.86989765 : -53.13010235) : (Refresh ? 41.18592517 : 0.0);
				const double ExpectedYaw = Policy == EPairedFacingPolicy::MatchPartnerHeading ? (Refresh ? -45.0 : 90.0)
					: PositionalYaw + (Policy == EPairedFacingPolicy::FaceAwayFromPartner ? 180.0 : 0.0);
				TestTrue(TEXT("Setup and refresh honor role policy without pitch or roll"), Target->Rotator().Equals(FRotator(0, ExpectedYaw, 0), .01));
			}
			F.Clear();
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedFacingCoincidenceAndDisabled,
	"KatanaCombat.Targeting.PairedFacing.CoincidenceAndDisabledRotation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedFacingCoincidenceAndDisabled::RunTest(const FString&)
{
	FPairedFacingFixture F;
	FPairedWarpConfig Config; Config.RelativeOffset = FVector::ZeroVector; Config.bAdjustToTerrain = false;
	F.Partner->SetActorLocation(FVector(0, 0, 350)); F.Partner->SetActorRotation(FRotator(-20, -63, 10));
	for (bool bVictim : {false, true})
	{
		for (EPairedFacingPolicy Policy : {EPairedFacingPolicy::FacePartner, EPairedFacingPolicy::FaceAwayFromPartner, EPairedFacingPolicy::MatchPartnerHeading})
		{
			F.Owner->SetActorRotation(FRotator(12, 37, 9)); Config.FacingPolicy = Policy;
			for (bool bEnabled : {true, false})
			{
				Config.bWarpRotation = bEnabled;
				TestTrue(TEXT("Coincident role policy starts"), F.Start(bVictim, Config, F.Partner));
				F.Warp()->OnPreUpdate.Broadcast(F.Warp());
				const auto* Target = F.Warp()->FindWarpTarget(Config.WarpTargetName);
				const FRotator Expected = bEnabled ? FRotator(0, Policy == EPairedFacingPolicy::MatchPartnerHeading ? -63 : 37, 0) : F.Owner->GetActorRotation();
				TestTrue(TEXT("Coincidence preserves fallback; disabled rotation preserves full owner rotation"), Target && Target->Rotator().Equals(Expected, .01));
				F.Clear();
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedFacingInvalidInputs,
	"KatanaCombat.Targeting.PairedFacing.InvalidInputsPreserveOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedFacingInvalidInputs::RunTest(const FString&)
{
	for (bool bVictim : {false, true})
	{
		FPairedFacingFixture F;
		FPairedWarpConfig Config; Config.bAdjustToTerrain = false; Config.FacingPolicy = EPairedFacingPolicy::MatchPartnerHeading;
		F.Partner->SetActorRotation(FRotator(0, 61, 0));
		TestTrue(TEXT("Valid owner starts"), F.Start(bVictim, Config, F.Partner));
		FPairedWarpConfig Invalid = Config; Invalid.FacingPolicy = static_cast<EPairedFacingPolicy>(255);
		TestFalse(TEXT("Unknown policy rejected"), F.Start(bVictim, Invalid, F.Partner));
		TestFalse(TEXT("Missing partner rejected"), F.Start(bVictim, Config, nullptr));
		F.Partner->SetActorRotation(FRotator(0, 84, 0)); F.Warp()->OnPreUpdate.Broadcast(F.Warp());
		const auto* Target = F.Warp()->FindWarpTarget(Config.WarpTargetName);
		TestTrue(TEXT("Rejected replacement preserves original policy ownership"), Target && Target->Rotator().Equals(FRotator(0, 84, 0), .01));
		F.Partner->Destroy(); F.Warp()->OnPreUpdate.Broadcast(F.Warp());
		// Direct paired tracking stops on destruction; its published endpoint lives
		// until the owning paired sequence explicitly clears that named target.
		Target = F.Warp()->FindWarpTarget(Config.WarpTargetName);
		TestTrue(TEXT("Destroyed partner stops updates at the last endpoint"), Target && Target->Rotator().Equals(FRotator(0, 84, 0), .01));
		F.Owner->TargetingComponent->ClearMotionWarp(Config.WarpTargetName);
		TestNull(TEXT("Explicit owner cleanup removes the target"), F.Warp()->FindWarpTarget(Config.WarpTargetName));
		TestFalse(TEXT("Destroyed partner cannot acquire a new target"), F.Start(bVictim, Config, F.Partner));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedFacingAlignmentPolicy,
	"KatanaCombat.Targeting.PairedFacing.RetainedAlignmentTracksPolicy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedFacingAlignmentPolicy::RunTest(const FString&)
{
	FPairedFacingFixture F;
	FAlignmentRequestSpec Spec;
	Spec.OwnerId = TEXT("PairedFacingObservation"); Spec.OwnerGeneration = 1;
	Spec.Executor = EAlignmentExecutor::MotionWarping; Spec.Priority = EDefenseAlignmentPriority::PairedOrParryBridge;
	Spec.Target = F.Partner; Spec.WarpTargetName = TEXT("PairedFacingTarget");
	Spec.MaximumTurnRate = 360; Spec.RemainingTurnBudget = 180; Spec.bTrackTargetRotation = true;
	Spec.FacingPolicy = EPairedFacingPolicy::MatchPartnerHeading;
	F.Partner->SetActorRotation(FRotator(20, 61, 10));
	const auto Handle = F.Owner->TargetingComponent->AcquireAlignmentRequest(Spec);
	TestTrue(TEXT("Retained alignment acquired"), Handle.IsValid());
	const auto* Target = F.Warp()->FindWarpTarget(Spec.WarpTargetName);
	TestTrue(TEXT("Retained target matches partner heading at acquisition"), Target && Target->Rotator().Equals(FRotator(0, 61, 0), .01));
	F.Partner->SetActorRotation(FRotator(-15, 84, 25));
	TestTrue(TEXT("Retained owner can refresh its request"), F.Owner->TargetingComponent->UpdateAlignmentRequest(Handle, Spec));
	Target = F.Warp()->FindWarpTarget(Spec.WarpTargetName);
	TestTrue(TEXT("Retained target refresh follows heading rather than bearing"), Target && Target->Rotator().Equals(FRotator(0, 84, 0), .01));
	FAlignmentRequestSpec Invalid = Spec; Invalid.FacingPolicy = static_cast<EPairedFacingPolicy>(255);
	TestFalse(TEXT("Invalid retained update rejected"), F.Owner->TargetingComponent->UpdateAlignmentRequest(Handle, Invalid));
	FAlignmentRequestSpec Current;
	TestTrue(TEXT("Existing request survives rejection"), F.Owner->TargetingComponent->GetAlignmentRequestSpec(Handle, Current));
	TestEqual(TEXT("Existing policy survives rejection"), Current.FacingPolicy, EPairedFacingPolicy::MatchPartnerHeading);
	F.Owner->TargetingComponent->ReleaseAlignmentRequest(Handle);
	TestNull(TEXT("Release removes the paired target"), F.Warp()->FindWarpTarget(Spec.WarpTargetName));
	return true;
}
