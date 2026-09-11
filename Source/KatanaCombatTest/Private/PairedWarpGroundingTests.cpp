// Copyright Epic Games, Inc. All Rights Reserved.
#include "CombatTestHelpers.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Core/TargetingComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "MotionWarpingComponent.h"

namespace
{
struct FPairedGroundFixture
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Attacker = FCombatTestHelpers::CreateTestPlayerCharacter(World, FVector(0, 0, 90));
	AEnemyCharacter* Victim = FCombatTestHelpers::CreateTestEnemyCharacter(World, FVector(150, 0, 90));
	~FPairedGroundFixture() { FCombatTestHelpers::DestroyTestWorld(World); }

	UBoxComponent* Box(const FVector& Center, const FVector& Extent, ECollisionChannel ObjectType, AActor* Actor = nullptr)
	{
		if (!Actor) { Actor = World->SpawnActor<AActor>(); }
		auto* Component = NewObject<UBoxComponent>(Actor);
		Actor->AddInstanceComponent(Component);
		if (Actor->GetRootComponent()) { Component->SetupAttachment(Actor->GetRootComponent()); }
		else { Actor->SetRootComponent(Component); }
		Component->SetBoxExtent(Extent); Component->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		Component->SetCollisionObjectType(ObjectType); Component->SetCollisionResponseToAllChannels(ECR_Block);
		Component->CanCharacterStepUpOn = ECB_Yes;
		Component->RegisterComponent(); Component->SetWorldLocation(Center);
		return Component;
	}

	UMotionWarpingComponent* Warp(ACharacter* Character) const { return Character->FindComponentByClass<UMotionWarpingComponent>(); }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedWarpGroundIgnoresCharacterGeometry,
	"KatanaCombat.Targeting.PairedGrounding.IgnoresCharacterGeometry", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedWarpGroundIgnoresCharacterGeometry::RunTest(const FString& Parameters)
{
	FPairedGroundFixture F;
	F.Box(FVector(0, 0, -10), FVector(1000, 1000, 10), ECC_WorldStatic);
	// A character mesh can advertise an environment object type; owner identity
	// must exclude it even when its top normal looks like a walkable surface.
	F.Box(FVector(50, 0, 155), FVector(20, 20, 10), ECC_WorldDynamic, F.Attacker);
	AActor* Weapon = F.World->SpawnActor<AActor>(); Weapon->SetOwner(F.Attacker);
	F.Box(FVector(50, 0, 170), FVector(15, 15, 5), ECC_WorldStatic, Weapon);
	FPairedWarpConfig Config; Config.RelativeOffset = FVector(50, 0, 0); Config.bAdjustToTerrain = true;
	TestTrue(TEXT("Victim tracking starts"), F.Victim->TargetingComponent->SetupVictimWarp(F.Attacker, Config));
	const auto* Target = F.Warp(F.Victim)->FindWarpTarget(Config.WarpTargetName);
	if (TestNotNull(TEXT("Victim target"), Target))
	{
		TestTrue(TEXT("Ground is the floor below character and weapon geometry"),
			FMath::IsNearlyEqual(Target->GetLocation().Z, F.Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), .1));
	}
	F.Attacker->SetActorLocation(FVector(0, 0, 110));
	F.Warp(F.Victim)->OnPreUpdate.Broadcast(F.Warp(F.Victim));
	Target = F.Warp(F.Victim)->FindWarpTarget(Config.WarpTargetName);
	TestTrue(TEXT("Continuous refresh still selects the environment floor"), Target &&
		FMath::IsNearlyEqual(Target->GetLocation().Z, F.Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(), .1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedWarpGroundWalkability,
	"KatanaCombat.Targeting.PairedGrounding.WalkabilityAndMovingSupport", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedWarpGroundWalkability::RunTest(const FString& Parameters)
{
	FPairedGroundFixture F;
	F.Box(FVector(0, 0, -10), FVector(1000, 1000, 10), ECC_WorldStatic);
	UBoxComponent* Ramp = F.Box(FVector(50, 0, 90), FVector(150, 150, 5), ECC_WorldStatic);
	Ramp->SetWorldRotation(FRotator(60, 0, 0));
	FPairedWarpConfig Config; Config.RelativeOffset = FVector(50, 0, 0); Config.bAdjustToTerrain = true;
	F.Victim->GetCharacterMovement()->SetWalkableFloorAngle(40);
	F.Victim->TargetingComponent->SetupVictimWarp(F.Attacker, Config);
	const auto* Target = F.Warp(F.Victim)->FindWarpTarget(Config.WarpTargetName);
	const float HalfHeight = F.Victim->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	TestTrue(TEXT("Unwalkable ramp cannot lift the target"), Target && FMath::IsNearlyEqual(Target->GetLocation().Z, HalfHeight, .1));
	Ramp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	UBoxComponent* Platform = F.Box(FVector(50, 0, 15), FVector(50, 50, 5), ECC_WorldDynamic);
	F.Warp(F.Victim)->OnPreUpdate.Broadcast(F.Warp(F.Victim));
	Target = F.Warp(F.Victim)->FindWarpTarget(Config.WarpTargetName);
	TestTrue(TEXT("Walkable dynamic platform supports the target"), Target && FMath::IsNearlyEqual(Target->GetLocation().Z, 20 + HalfHeight, .1));
	Platform->SetWorldLocation(FVector(50, 0, 25));
	F.Warp(F.Victim)->OnPreUpdate.Broadcast(F.Warp(F.Victim));
	Target = F.Warp(F.Victim)->FindWarpTarget(Config.WarpTargetName);
	TestTrue(TEXT("Ground refresh follows platform height"), Target && FMath::IsNearlyEqual(Target->GetLocation().Z, 30 + HalfHeight, .1));
	// No surface in range: preserve the requested location instead of inventing ground.
	F.Attacker->SetActorLocation(FVector(5000, 0, 123));
	F.Warp(F.Victim)->OnPreUpdate.Broadcast(F.Warp(F.Victim));
	Target = F.Warp(F.Victim)->FindWarpTarget(Config.WarpTargetName);
	TestTrue(TEXT("Missing ground preserves requested height"), Target && FMath::IsNearlyEqual(Target->GetLocation().Z, 123., .1));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedWarpUprightFacing,
	"KatanaCombat.Targeting.PairedGrounding.UprightFacingAndOffsets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedWarpUprightFacing::RunTest(const FString& Parameters)
{
	FPairedGroundFixture F;
	for (bool bVictim : {true, false})
	{
		ACharacter* Owner = bVictim ? static_cast<ACharacter*>(F.Victim) : F.Attacker;
		ACharacter* Partner = bVictim ? static_cast<ACharacter*>(F.Attacker) : F.Victim;
		auto* Targeting = Owner->FindComponentByClass<UTargetingComponent>();
		Owner->SetActorLocation(FVector(0, 0, 90)); Owner->SetActorRotation(FRotator(0, 37, 0));
		Partner->SetActorLocation(FVector(100, 0, 200)); Partner->SetActorRotation(FRotator(35, 90, 20));
		FPairedWarpConfig Config; Config.RelativeOffset = FVector(50, 0, 0); Config.bAdjustToTerrain = false; Config.MaxWarpDistance = 0;
		TestTrue(TEXT("Paired tracking starts"), bVictim ? Targeting->SetupVictimWarp(Partner, Config) : Targeting->SetupAttackerPairedWarp(Partner, Config));
		for (int32 Refresh = 0; Refresh < 2; ++Refresh)
		{
			if (Refresh)
			{
				Partner->SetActorLocation(FVector(80, 70, 300)); Partner->SetActorRotation(FRotator(-30, -45, 25));
				F.Warp(Owner)->OnPreUpdate.Broadcast(F.Warp(Owner));
			}
			const auto* Target = F.Warp(Owner)->FindWarpTarget(Config.WarpTargetName);
			if (!TestNotNull(TEXT("Paired warp target"), Target)) { continue; }
			const FVector Expected = Partner->GetActorLocation() + FRotator(0, Partner->GetActorRotation().Yaw, 0).RotateVector(Config.RelativeOffset);
			TestTrue(TEXT("Relative XY offset uses partner yaw only"), Target->GetLocation().Equals(Expected, .01));
			const FRotator Rotation = Target->Rotator();
			TestTrue(TEXT("Facing remains upright across height changes"), FMath::IsNearlyZero(Rotation.Pitch, .01) && FMath::IsNearlyZero(Rotation.Roll, .01));
			FVector TowardPartner = Partner->GetActorLocation() - (bVictim ? Expected : Owner->GetActorLocation()); TowardPartner.Z = 0;
			TestTrue(TEXT("Yaw still faces the partner"), FVector::DotProduct(Rotation.Vector(), TowardPartner.GetSafeNormal()) > .999);
		}
		Targeting->ClearVictimWarp(); Targeting->ClearAttackerPairedWarp();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPairedWarpCoincidentFacing,
	"KatanaCombat.Targeting.PairedGrounding.CoincidentHeading", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FPairedWarpCoincidentFacing::RunTest(const FString& Parameters)
{
	FPairedGroundFixture F;
	F.Attacker->SetActorLocation(FVector(0, 0, 90)); F.Attacker->SetActorRotation(FRotator(0, 37, 0));
	F.Victim->SetActorLocation(FVector(0, 0, 300)); F.Victim->SetActorRotation(FRotator(0, -63, 0));
	FPairedWarpConfig Config; Config.RelativeOffset = FVector::ZeroVector; Config.bAdjustToTerrain = false;
	F.Attacker->TargetingComponent->SetupAttackerPairedWarp(F.Victim, Config);
	F.Victim->TargetingComponent->SetupVictimWarp(F.Attacker, Config);
	for (ACharacter* Character : {static_cast<ACharacter*>(F.Attacker), static_cast<ACharacter*>(F.Victim)})
	{
		const auto* Target = F.Warp(Character)->FindWarpTarget(Config.WarpTargetName);
		TestTrue(TEXT("Coincident XY preserves owner heading without pitch"), Target && Target->Rotator().Equals(FRotator(0, Character->GetActorRotation().Yaw, 0), .01));
	}
	return true;
}
