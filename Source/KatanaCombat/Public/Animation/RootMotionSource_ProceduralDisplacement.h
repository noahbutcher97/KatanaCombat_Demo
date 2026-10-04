#pragma once

#include "CoreMinimal.h"
#include "GameFramework/RootMotionSource.h"
#include "CombatTypes.h"
#include "RootMotionSource_ProceduralDisplacement.generated.h"

/**
 * Character-movement root-motion source used by UTargetingComponent's ProceduralDisplacement
 * executor when no root-motion animation plays. PrepareRootMotion evaluates the displacement
 * curve over each movement step's own simulation time, so the push lands on the curve at any
 * frame rate and contributes nothing past the curve's end. The executor owns termination, so
 * the base Duration stays negative (no timeout) and the curve's length is CurveDuration.
 */
USTRUCT()
struct KATANACOMBAT_API FRootMotionSource_ProceduralDisplacement : public FRootMotionSource
{
	GENERATED_BODY()

	virtual ~FRootMotionSource_ProceduralDisplacement() override {}

	/** Horizontal unit direction in world space. */
	UPROPERTY()
	FVector Direction = FVector::ZeroVector;

	/** Total curve distance in centimeters. */
	UPROPERTY()
	float Distance = 0.0f;

	/** Curve duration in seconds; distinct from the base Duration, which is the source's timeout. */
	UPROPERTY()
	float CurveDuration = 0.0f;

	UPROPERTY()
	EDisplacementSpeedProfile SpeedProfile = EDisplacementSpeedProfile::Linear;

	/** Curve time at which this source starts, so a resumed push continues the curve. */
	UPROPERTY()
	float StartElapsed = 0.0f;

	/**
	 * Simulation time of the steps animation root motion overrode since this source's clock last advanced; a step
	 * that applies the curve zeroes it. Character movement applies only the animation on such a step, so the curve
	 * does not advance: the executor ends a push whose override outlasts the push it has left, and leaves the step's
	 * movement (the animation's) out of the push's own travel. The executor's local bookkeeping, not replicated
	 * state: NetSerialize, MatchesAndHasSameState and UpdateStateFrom leave it out.
	 */
	float OverriddenTime = 0.0f;

	virtual FRootMotionSource* Clone() const override;

	virtual bool Matches(const FRootMotionSource* Other) const override;

	virtual bool MatchesAndHasSameState(const FRootMotionSource* Other) const override;

	virtual bool UpdateStateFrom(const FRootMotionSource* SourceToTakeStateFrom, bool bMarkForSimulatedCatchup = false) override;

	virtual void PrepareRootMotion(
		float SimulationTime,
		float MovementTickTime,
		const ACharacter& Character,
		const UCharacterMovementComponent& MoveComponent
		) override;

	virtual bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess) override;

	virtual UScriptStruct* GetScriptStruct() const override;

	virtual FString ToSimpleString() const override;
};

template<>
struct TStructOpsTypeTraits<FRootMotionSource_ProceduralDisplacement> : public TStructOpsTypeTraitsBase2<FRootMotionSource_ProceduralDisplacement>
{
	enum
	{
		WithNetSerializer = true,
		WithCopy = true
	};
};
