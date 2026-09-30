#include "Animation/RootMotionModifier_ProceduralDisplacement.h"
#include "Utilities/DisplacementMath.h"
#include "MotionWarpingAdapter.h"
#include "Components/SkeletalMeshComponent.h"

void URootMotionModifier_ProceduralDisplacement::Configure(const FProceduralDisplacement& InDisplacement, const double StartElapsed)
{
	Displacement = InDisplacement;
	Elapsed = FMath::Clamp(StartElapsed, 0.0, static_cast<double>(InDisplacement.Duration));
}

FTransform URootMotionModifier_ProceduralDisplacement::ProcessRootMotion(const FTransform& InRootMotion, const float DeltaSeconds)
{
	if (IsComplete() || !FMath::IsFinite(DeltaSeconds) || DeltaSeconds < 0.0f)
	{
		return InRootMotion;
	}

	const double T0 = Elapsed;
	Elapsed = FMath::Min(static_cast<double>(Displacement.Duration), Elapsed + DeltaSeconds);
	const double Step = DisplacementMath::DistanceBetween(
		Displacement.SpeedProfile, Displacement.Distance, Displacement.Duration, T0, Elapsed);

	FVector LocalDelta = FVector::ZeroVector;
	if (const UMotionWarpingBaseAdapter* Adapter = GetOwnerAdapter())
	{
		if (const USkeletalMeshComponent* Mesh = Adapter->GetMesh())
		{
			const FTransform& ComponentTransform = Mesh->GetComponentTransform();
			// Inverse of USkeletalMeshComponent::ConvertLocalRootMotionToWorld for a translation.
			LocalDelta = ComponentTransform.InverseTransformVector(Displacement.Direction * Step);
			if (Displacement.AnimationBlend == EDisplacementAnimationBlend::AddToAnimation)
			{
				// The executor's blocked check expects this motion too.
				AnimationTravel += FVector::DotProduct(
					ComponentTransform.TransformVector(InRootMotion.GetTranslation()), Displacement.Direction);
			}
		}
	}
	return ApplyDisplacement(InRootMotion, LocalDelta, Displacement.AnimationBlend);
}

FTransform URootMotionModifier_ProceduralDisplacement::ApplyDisplacement(
	const FTransform& InRootMotion, const FVector& LocalDelta, const EDisplacementAnimationBlend Blend)
{
	FTransform Result = InRootMotion;
	Result.SetTranslation(Blend == EDisplacementAnimationBlend::AddToAnimation
		? InRootMotion.GetTranslation() + LocalDelta
		: LocalDelta);
	return Result;
}
