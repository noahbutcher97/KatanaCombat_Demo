#include "Animation/RootMotionSource_ProceduralDisplacement.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Utilities/DisplacementMath.h"

FRootMotionSource* FRootMotionSource_ProceduralDisplacement::Clone() const
{
	return new FRootMotionSource_ProceduralDisplacement(*this);
}

bool FRootMotionSource_ProceduralDisplacement::Matches(const FRootMotionSource* Other) const
{
	if (!FRootMotionSource::Matches(Other))
	{
		return false;
	}

	// Safe: FRootMotionSource::Matches() checked that the script structs are equal.
	const FRootMotionSource_ProceduralDisplacement* OtherCast = static_cast<const FRootMotionSource_ProceduralDisplacement*>(Other);

	return Direction.Equals(OtherCast->Direction, UE_KINDA_SMALL_NUMBER)
		&& FMath::IsNearlyEqual(Distance, OtherCast->Distance, UE_KINDA_SMALL_NUMBER)
		&& FMath::IsNearlyEqual(CurveDuration, OtherCast->CurveDuration, UE_KINDA_SMALL_NUMBER)
		&& SpeedProfile == OtherCast->SpeedProfile
		&& FMath::IsNearlyEqual(StartElapsed, OtherCast->StartElapsed, UE_KINDA_SMALL_NUMBER);
}

bool FRootMotionSource_ProceduralDisplacement::MatchesAndHasSameState(const FRootMotionSource* Other) const
{
	// The curve fields are configuration, compared by Matches(); the only state is the base time.
	return FRootMotionSource::MatchesAndHasSameState(Other);
}

bool FRootMotionSource_ProceduralDisplacement::UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, const bool bMarkForSimulatedCatchup)
{
	// No state beyond the base time, which FRootMotionSource takes over.
	return FRootMotionSource::UpdateStateFrom(SourceToTakeStateFrom, bMarkForSimulatedCatchup);
}

void FRootMotionSource_ProceduralDisplacement::PrepareRootMotion(
	const float SimulationTime,
	const float MovementTickTime,
	const ACharacter& Character,
	const UCharacterMovementComponent& MoveComponent)
{
	RootMotionParams.Clear();

	// Animation root motion overrides every root-motion source on this step (ApplyRootMotionToVelocity),
	// so none of the curve would be applied. PrepareRootMotion runs after TickCharacterPose has gathered
	// the step's animation root motion, so this is exactly that case: leave the clock alone, because the
	// source's time is the record of push time actually applied.
	if (MoveComponent.HasAnimRootMotion())
	{
		return;
	}

	// Evaluate the curve over this step's own simulation time, so every step lands on the curve
	// whatever the previous step lasted. Past the curve's end the step is zero.
	const double T0 = static_cast<double>(StartElapsed) + GetTime();
	const double Step = DisplacementMath::DistanceBetween(SpeedProfile, Distance, CurveDuration, T0, T0 + SimulationTime);

	// Root-motion translation is a velocity applied over the movement tick, as in
	// FRootMotionSource_ConstantForce: velocity * MovementTickTime is the step's distance.
	const FVector Velocity = Direction * (MovementTickTime > UE_SMALL_NUMBER ? Step / MovementTickTime : 0.0);
	RootMotionParams.Set(FTransform(Velocity));

	SetTime(GetTime() + SimulationTime);
}

bool FRootMotionSource_ProceduralDisplacement::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
	if (!FRootMotionSource::NetSerialize(Ar, Map, bOutSuccess))
	{
		return false;
	}

	Ar << Direction;
	Ar << Distance;
	Ar << CurveDuration;
	uint8 SpeedProfileSerialize = static_cast<uint8>(SpeedProfile);
	Ar << SpeedProfileSerialize;
	SpeedProfile = static_cast<EDisplacementSpeedProfile>(SpeedProfileSerialize);
	Ar << StartElapsed;

	bOutSuccess = true;
	return true;
}

UScriptStruct* FRootMotionSource_ProceduralDisplacement::GetScriptStruct() const
{
	return FRootMotionSource_ProceduralDisplacement::StaticStruct();
}

FString FRootMotionSource_ProceduralDisplacement::ToSimpleString() const
{
	return FString::Printf(TEXT("[ID:%u]FRootMotionSource_ProceduralDisplacement %s"), LocalID, *InstanceName.GetPlainNameString());
}
