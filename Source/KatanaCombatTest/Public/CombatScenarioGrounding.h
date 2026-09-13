#pragma once

#include "CombatScenarioPlacement.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

/** Fixture preparation only. Never called during entry or paired playback. */
namespace CombatScenarioGrounding
{
inline bool ReadBudget(const TSharedPtr<FJsonObject>& Definition, double& Budget)
{
	Budget = 0;
	if (!Definition->HasField(TEXT("placement_support"))) { return true; }
	const TSharedPtr<FJsonObject>* Support = nullptr;
	FString Mode;
	if (!Definition->TryGetObjectField(TEXT("placement_support"), Support) || (*Support)->Values.Num() != 2
		|| !(*Support)->TryGetStringField(TEXT("mode"), Mode) || Mode != TEXT("walking_floor")) { return false; }
	const auto Value = (*Support)->TryGetField(TEXT("max_adjustment_cm"));
	return Value && Value->Type == EJson::Number && Value->TryGetNumber(Budget)
		&& FMath::IsFinite(Budget) && Budget > 0 && Budget <= 50;
}

inline bool Prepare(ACharacter* Character, double Budget, TSharedRef<FJsonObject> Evidence)
{
	const auto Reject = [&](const TCHAR* Reason) { Evidence->SetStringField(TEXT("status"), Reason); return false; };
	UCharacterMovementComponent* Movement = Character ? Character->GetCharacterMovement() : nullptr;
	if (!Movement || !FMath::IsFinite(Budget) || Budget <= 0 || Budget > 50
		|| !Movement->IsMovingOnGround() || !Movement->GetGravityDirection().Equals(FVector(0,0,-1), 1.e-6)
		|| !Character->GetActorUpVector().Equals(FVector::UpVector, 1.e-6)) { return Reject(TEXT("unsupported_movement")); }
	const FVector Requested = Character->GetActorLocation();
	Evidence->SetArrayField(TEXT("requested_location_cm"), CombatScenarioPlacement::VectorJson(Requested));
	Evidence->SetNumberField(TEXT("capsule_radius_cm"), Character->GetCapsuleComponent()->GetScaledCapsuleRadius());
	Evidence->SetNumberField(TEXT("capsule_half_height_cm"), Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight());
	FFindFloorResult Floor;
	Movement->FindFloor(Requested, Floor, false);
	// Line-only support has different edge/perch semantics; this fixture contract
	// deliberately requires a capsule sweep against a walkable surface.
	if (!Floor.IsWalkableFloor() || Floor.HitResult.bStartPenetrating || Floor.bLineTrace
		|| !Floor.HitResult.GetComponent()) { return Reject(TEXT("unsupported_floor")); }
	const double Before = Floor.FloorDist;
	const double Target = (UCharacterMovementComponent::MIN_FLOOR_DIST + UCharacterMovementComponent::MAX_FLOOR_DIST) * .5;
	const double Predicted = Before < UCharacterMovementComponent::MIN_FLOOR_DIST || Before > UCharacterMovementComponent::MAX_FLOOR_DIST ? Target - Before : 0;
	Evidence->SetNumberField(TEXT("floor_distance_before_cm"), Before);
	if (!FMath::IsFinite(Predicted) || FMath::Abs(Predicted) > Budget) { return Reject(TEXT("adjustment_budget_exhausted")); }
	const FFindFloorResult PreviousFloor = Movement->CurrentFloor;
	Movement->CurrentFloor = Floor;
	Movement->AdjustFloorHeight();
	Movement->FindFloor(Character->GetActorLocation(), Floor, false);
	const FVector Delta = Character->GetActorLocation() - Requested;
	if (!Floor.IsWalkableFloor() || Floor.HitResult.bStartPenetrating || Floor.bLineTrace
		|| !Floor.HitResult.GetComponent() || Floor.FloorDist < UCharacterMovementComponent::MIN_FLOOR_DIST - .01
		|| Floor.FloorDist > UCharacterMovementComponent::MAX_FLOOR_DIST + .01
		|| FMath::Abs(Delta.Z) > Budget || FVector2D(Delta).Size() > .01)
	{
		// A sweep can discover an edge/obstruction that the initial query missed.
		// Reject the fixture atomically instead of retaining a partially settled pose.
		Character->SetActorLocation(Requested, false, nullptr, ETeleportType::TeleportPhysics);
		Movement->CurrentFloor = PreviousFloor;
		return Reject(TEXT("settling_verification_failed"));
	}
	Movement->CurrentFloor = Floor;
	Movement->SetBaseFromFloor(Floor);
	Evidence->SetStringField(TEXT("status"), TEXT("grounded"));
	Evidence->SetNumberField(TEXT("vertical_adjustment_cm"), Delta.Z);
	Evidence->SetNumberField(TEXT("floor_distance_after_cm"), Floor.FloorDist);
	Evidence->SetStringField(TEXT("floor_component"), Floor.HitResult.GetComponent()->GetPathName());
	Evidence->SetArrayField(TEXT("floor_impact_cm"), CombatScenarioPlacement::VectorJson(Floor.HitResult.ImpactPoint));
	return true;
}
}
