// Copyright Epic Games, Inc. All Rights Reserved.

#include "PairedMontageAnalyzer.h"
#include "Data/PairedAnimationData.h"
#include "Animation/AnimMontage.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"

// ============================================================================
// PAIRED ANIMATION ANALYSIS
// ============================================================================

FPairedMontageAnalysisResult UPairedMontageAnalyzer::AnalyzePairedAnimation(
	UPairedAnimationData* PairedData,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh)
{
	FPairedMontageAnalysisResult Result;
	Result.AnalyzedData = PairedData;

	if (!PairedData)
	{
		Result.CombinedMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("PairedAnimationData is null"))
		));
		return Result;
	}

	// Use same mesh for both if victim not specified
	if (!VictimMesh)
	{
		VictimMesh = AttackerMesh;
	}

	// Validate basic configuration
	ValidatePairedAnimationData(PairedData, Result.CombinedMessages);

	// Analyze attacker montage
	if (PairedData->AttackerMontage)
	{
		Result.AttackerAnalysis = AnalyzeMontage(
			PairedData->AttackerMontage,
			AttackerMesh,
			GetDefaultAttackerContactBones()
		);
		Result.CombinedMessages.Append(Result.AttackerAnalysis.Messages);
	}
	else
	{
		Result.CombinedMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("AttackerMontage is not set"))
		));
	}

	// Analyze victim montage
	if (PairedData->VictimMontage)
	{
		Result.VictimAnalysis = AnalyzeMontage(
			PairedData->VictimMontage,
			VictimMesh,
			GetDefaultVictimContactBones()
		);
		Result.CombinedMessages.Append(Result.VictimAnalysis.Messages);
	}
	else
	{
		Result.CombinedMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("VictimMontage is not set"))
		));
	}

	// Analyze contact points if we have both montages and meshes
	if (PairedData->AttackerMontage && PairedData->VictimMontage && AttackerMesh && VictimMesh)
	{
		// Predict contact points
		Result.ContactPoints = PredictContactPoints(
			PairedData->AttackerMontage,
			PairedData->VictimMontage,
			AttackerMesh,
			VictimMesh,
			GetDefaultAttackerContactBones(),
			GetDefaultVictimContactBones(),
			PairedData->VictimRelativePosition.Size()
		);

		// Analyze sync point
		FSyncPointAnalysis SyncAnalysis = AnalyzeSyncPoint(
			PairedData->AttackerMontage,
			PairedData->VictimMontage,
			PairedData->SyncPointTime,
			PairedData->VictimStartOffset
		);
		Result.SyncPointAnalyses.Add(SyncAnalysis);

		// Analyze reach requirements
		FName AttackerBone = TEXT("hand_r");
		FName VictimBone = TEXT("spine_03");

		FReachAnalysis ReachAnalysis = AnalyzeReachRequirement(
			AttackerMesh,
			VictimMesh,
			AttackerBone,
			VictimBone,
			PairedData->VictimRelativePosition.Size()
		);
		Result.ReachAnalyses.Add(ReachAnalysis);

		// Generate recommendations
		if (Result.ContactPoints.Num() > 0)
		{
			// Find primary contact
			for (const FContactPointAnalysis& Contact : Result.ContactPoints)
			{
				if (Contact.bIsPrimaryContact)
				{
					Result.RecommendedSyncPointTime = Contact.AttackerContactTime;
					break;
				}
			}
		}

		Result.RecommendedWarpDistance = CalculateRecommendedDistance(
			AttackerMesh,
			VictimMesh,
			AttackerBone,
			VictimBone
		);
	}

	// Determine overall validity
	Result.bAnalysisSuccessful = true;
	Result.bIsValid = true;

	for (const FAnalysisMessage& Msg : Result.CombinedMessages)
	{
		if (Msg.Severity == EAnalysisMessageSeverity::Error)
		{
			Result.bIsValid = false;
			break;
		}
	}

	return Result;
}

bool UPairedMontageAnalyzer::ValidatePairedAnimationData(
	UPairedAnimationData* PairedData,
	TArray<FAnalysisMessage>& OutMessages)
{
	if (!PairedData)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("PairedAnimationData is null"))
		));
		return false;
	}

	bool bIsValid = true;

	// Check montage references
	if (!PairedData->AttackerMontage)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("AttackerMontage is not set"))
		));
		bIsValid = false;
	}

	if (!PairedData->VictimMontage)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("VictimMontage is not set"))
		));
		bIsValid = false;
	}

	// Check sync point time
	if (PairedData->AttackerMontage)
	{
		float AttackerLength = PairedData->AttackerMontage->GetPlayLength();
		if (PairedData->SyncPointTime > AttackerLength)
		{
			OutMessages.Add(FAnalysisMessage(
				EAnalysisMessageSeverity::Error,
				FText::Format(
					FText::FromString(TEXT("SyncPointTime ({0}s) exceeds AttackerMontage length ({1}s)")),
					FText::AsNumber(PairedData->SyncPointTime),
					FText::AsNumber(AttackerLength)
				),
				PairedData->SyncPointTime
			));
			bIsValid = false;
		}
	}

	// Check victim timing
	if (PairedData->VictimMontage && PairedData->AttackerMontage)
	{
		float VictimLength = PairedData->VictimMontage->GetPlayLength();
		float VictimSyncTime = PairedData->SyncPointTime - PairedData->VictimStartOffset;

		if (VictimSyncTime < 0.0f)
		{
			OutMessages.Add(FAnalysisMessage(
				EAnalysisMessageSeverity::Warning,
				FText::FromString(TEXT("VictimStartOffset makes sync point occur before victim animation starts"))
			));
		}

		if (VictimSyncTime > VictimLength)
		{
			OutMessages.Add(FAnalysisMessage(
				EAnalysisMessageSeverity::Error,
				FText::Format(
					FText::FromString(TEXT("Sync point in victim montage ({0}s) exceeds VictimMontage length ({1}s)")),
					FText::AsNumber(VictimSyncTime),
					FText::AsNumber(VictimLength)
				)
			));
			bIsValid = false;
		}
	}

	// Check distance settings
	if (PairedData->MinTriggerDistance >= PairedData->MaxTriggerDistance)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("MinTriggerDistance >= MaxTriggerDistance - paired animation may never trigger"))
		));
	}

	if (PairedData->MaxTriggerDistance > PairedData->MaxWarpDistance)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("MaxTriggerDistance > MaxWarpDistance - warp may fail at max trigger range"))
		));
	}

	// Check damage settings
	if (PairedData->ReactionType == EPairedReactionType::Finisher && !PairedData->bIsLethal)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("Finisher is not marked as lethal - victim may survive"))
		));
	}

	if (PairedData->BaseDamage <= 0.0f && !PairedData->bIsLethal)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("BaseDamage is 0 and not lethal - no damage will be dealt"))
		));
	}

	// Check animation name
	if (PairedData->AnimationName == NAME_None)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("AnimationName is not set - may cause lookup issues"))
		));
	}

	return bIsValid;
}

// ============================================================================
// CONTACT POINT ANALYSIS
// ============================================================================

TArray<FContactPointAnalysis> UPairedMontageAnalyzer::PredictContactPoints(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	const TArray<FName>& AttackerBones,
	const TArray<FName>& VictimBones,
	float CharacterDistance)
{
	TArray<FContactPointAnalysis> Results;

	if (!AttackerMontage || !VictimMontage || !AttackerMesh || !VictimMesh)
	{
		return Results;
	}

	TArray<FName> AttBones = AttackerBones.Num() > 0 ? AttackerBones : GetDefaultAttackerContactBones();
	TArray<FName> VicBones = VictimBones.Num() > 0 ? VictimBones : GetDefaultVictimContactBones();

	float MinDistance = MAX_FLT;
	FContactPointAnalysis PrimaryContact;

	for (const FName& AttackerBone : AttBones)
	{
		if (!DoesBoneExist(AttackerMesh, AttackerBone))
		{
			continue;
		}

		for (const FName& VictimBone : VicBones)
		{
			if (!DoesBoneExist(VictimMesh, VictimBone))
			{
				continue;
			}

			float ContactDistance = 0.0f;
			float ContactTime = FindClosestApproachTime(
				AttackerMontage,
				VictimMontage,
				AttackerMesh,
				VictimMesh,
				AttackerBone,
				VictimBone,
				CharacterDistance,
				ContactDistance
			);

			if (ContactTime >= 0.0f)
			{
				FContactPointAnalysis Contact;
				Contact.AttackerBone = AttackerBone;
				Contact.VictimBone = VictimBone;
				Contact.AttackerContactTime = ContactTime;
				Contact.VictimContactTime = ContactTime; // Simplified - same time in both
				Contact.DistanceAtContact = ContactDistance;
				Contact.Confidence = 1.0f - FMath::Clamp(ContactDistance / 100.0f, 0.0f, 1.0f);

				// Get actual contact location
				FTransform AttackerTransform = GetBoneTransformAtTime(
					AttackerMontage, AttackerMesh, AttackerBone, ContactTime);
				FTransform VictimTransform = GetBoneTransformAtTime(
					VictimMontage, VictimMesh, VictimBone, ContactTime);

				// Adjust victim position for character distance
				FVector VictimPos = VictimTransform.GetLocation();
				VictimPos.X += CharacterDistance;

				Contact.ContactLocation = (AttackerTransform.GetLocation() + VictimPos) * 0.5f;
				Contact.ContactNormal = (VictimPos - AttackerTransform.GetLocation()).GetSafeNormal();

				Results.Add(Contact);

				if (ContactDistance < MinDistance)
				{
					MinDistance = ContactDistance;
					PrimaryContact = Contact;
					PrimaryContact.bIsPrimaryContact = true;
				}
			}
		}
	}

	// Mark primary contact
	for (FContactPointAnalysis& Contact : Results)
	{
		if (Contact.AttackerBone == PrimaryContact.AttackerBone &&
			Contact.VictimBone == PrimaryContact.VictimBone)
		{
			Contact.bIsPrimaryContact = true;
			break;
		}
	}

	return Results;
}

FContactPointAnalysis UPairedMontageAnalyzer::FindPrimaryContactPoint(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	float CharacterDistance)
{
	TArray<FContactPointAnalysis> AllContacts = PredictContactPoints(
		AttackerMontage,
		VictimMontage,
		AttackerMesh,
		VictimMesh,
		TArray<FName>(),
		TArray<FName>(),
		CharacterDistance
	);

	for (const FContactPointAnalysis& Contact : AllContacts)
	{
		if (Contact.bIsPrimaryContact)
		{
			return Contact;
		}
	}

	return FContactPointAnalysis();
}

FContactPointAnalysis UPairedMontageAnalyzer::AnalyzeContactAtSyncPoint(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	float SyncPointTime,
	FName AttackerBone,
	FName VictimBone,
	float CharacterDistance)
{
	FContactPointAnalysis Result;
	Result.AttackerBone = AttackerBone;
	Result.VictimBone = VictimBone;
	Result.AttackerContactTime = SyncPointTime;
	Result.VictimContactTime = SyncPointTime;

	if (!AttackerMontage || !VictimMontage || !AttackerMesh || !VictimMesh)
	{
		return Result;
	}

	Result.DistanceAtContact = GetBoneDistanceAtTime(
		AttackerMontage,
		VictimMontage,
		AttackerMesh,
		VictimMesh,
		AttackerBone,
		VictimBone,
		SyncPointTime,
		CharacterDistance
	);

	Result.Confidence = 1.0f - FMath::Clamp(Result.DistanceAtContact / 100.0f, 0.0f, 1.0f);

	FTransform AttackerTransform = GetBoneTransformAtTime(
		AttackerMontage, AttackerMesh, AttackerBone, SyncPointTime);
	FTransform VictimTransform = GetBoneTransformAtTime(
		VictimMontage, VictimMesh, VictimBone, SyncPointTime);

	FVector VictimPos = VictimTransform.GetLocation();
	VictimPos.X += CharacterDistance;

	Result.ContactLocation = (AttackerTransform.GetLocation() + VictimPos) * 0.5f;
	Result.ContactNormal = (VictimPos - AttackerTransform.GetLocation()).GetSafeNormal();

	return Result;
}

// ============================================================================
// SYNC POINT ANALYSIS
// ============================================================================

FSyncPointAnalysis UPairedMontageAnalyzer::AnalyzeSyncPoint(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	float SyncPointTime,
	float VictimStartOffset)
{
	FSyncPointAnalysis Result;
	Result.SyncPointName = TEXT("Impact");
	Result.AttackerTime = SyncPointTime;
	Result.VictimTime = SyncPointTime - VictimStartOffset;

	if (!AttackerMontage || !VictimMontage)
	{
		Result.bIsAligned = false;
		return Result;
	}

	float AttackerLength = AttackerMontage->GetPlayLength();
	float VictimLength = VictimMontage->GetPlayLength();

	// Check attacker sync point validity
	if (SyncPointTime < 0.0f || SyncPointTime > AttackerLength)
	{
		Result.bIsAligned = false;
		Result.AlignmentError = (SyncPointTime > AttackerLength) ?
			SyncPointTime - AttackerLength : -SyncPointTime;
	}

	// Check victim sync point validity
	float VictimSyncTime = SyncPointTime - VictimStartOffset;
	if (VictimSyncTime < 0.0f || VictimSyncTime > VictimLength)
	{
		Result.bIsAligned = false;
		if (VictimSyncTime < 0.0f)
		{
			Result.AlignmentError = FMath::Max(Result.AlignmentError, -VictimSyncTime);
		}
		else
		{
			Result.AlignmentError = FMath::Max(Result.AlignmentError, VictimSyncTime - VictimLength);
		}
	}

	Result.TimingDifference = VictimStartOffset;

	return Result;
}

float UPairedMontageAnalyzer::FindOptimalSyncPointTime(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	FName AttackerContactBone,
	FName VictimContactBone,
	float CharacterDistance)
{
	if (!AttackerMontage || !VictimMontage || !AttackerMesh || !VictimMesh)
	{
		return 0.0f;
	}

	float MinDistance = 0.0f;
	return FindClosestApproachTime(
		AttackerMontage,
		VictimMontage,
		AttackerMesh,
		VictimMesh,
		AttackerContactBone,
		VictimContactBone,
		CharacterDistance,
		MinDistance
	);
}

bool UPairedMontageAnalyzer::ValidateSyncPointTime(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	float SyncPointTime,
	float VictimStartOffset,
	TArray<FAnalysisMessage>& OutMessages)
{
	bool bIsValid = true;

	if (!AttackerMontage)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("AttackerMontage is null"))
		));
		return false;
	}

	if (!VictimMontage)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("VictimMontage is null"))
		));
		return false;
	}

	float AttackerLength = AttackerMontage->GetPlayLength();
	float VictimLength = VictimMontage->GetPlayLength();

	if (SyncPointTime < 0.0f)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("SyncPointTime is negative")),
			SyncPointTime
		));
		bIsValid = false;
	}

	if (SyncPointTime > AttackerLength)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::Format(
				FText::FromString(TEXT("SyncPointTime ({0}s) exceeds AttackerMontage length ({1}s)")),
				FText::AsNumber(SyncPointTime),
				FText::AsNumber(AttackerLength)
			),
			SyncPointTime
		));
		bIsValid = false;
	}

	float VictimSyncTime = SyncPointTime - VictimStartOffset;
	if (VictimSyncTime < 0.0f)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::FromString(TEXT("VictimStartOffset causes sync point before victim starts"))
		));
	}

	if (VictimSyncTime > VictimLength)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::Format(
				FText::FromString(TEXT("Victim sync time ({0}s) exceeds VictimMontage length ({1}s)")),
				FText::AsNumber(VictimSyncTime),
				FText::AsNumber(VictimLength)
			)
		));
		bIsValid = false;
	}

	return bIsValid;
}

// ============================================================================
// REACH ANALYSIS
// ============================================================================

FReachAnalysis UPairedMontageAnalyzer::AnalyzeReachRequirement(
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	FName AttackerBone,
	FName VictimBone,
	float CharacterDistance)
{
	FReachAnalysis Result;
	Result.EffectorBone = AttackerBone;
	Result.RequiredReach = CharacterDistance;

	if (!AttackerMesh)
	{
		return Result;
	}

	USkeleton* Skeleton = AttackerMesh->GetSkeleton();
	if (!Skeleton)
	{
		return Result;
	}

	// Estimate available reach from reference pose
	// This is a simplified calculation - actual reach depends on arm chain length
	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();

	int32 HandIndex = RefSkeleton.FindBoneIndex(AttackerBone);
	if (HandIndex == INDEX_NONE)
	{
		return Result;
	}

	// Walk up the hierarchy to find shoulder
	float ChainLength = 0.0f;
	int32 CurrentIndex = HandIndex;

	while (CurrentIndex != INDEX_NONE)
	{
		int32 ParentIndex = RefSkeleton.GetParentIndex(CurrentIndex);
		if (ParentIndex != INDEX_NONE)
		{
			FVector BonePos = RefSkeleton.GetRefBonePose()[CurrentIndex].GetLocation();
			ChainLength += BonePos.Size();
		}

		// Stop at clavicle or shoulder
		FName BoneName = RefSkeleton.GetBoneName(CurrentIndex);
		if (BoneName.ToString().Contains(TEXT("clavicle")) ||
			BoneName.ToString().Contains(TEXT("shoulder")))
		{
			break;
		}

		CurrentIndex = ParentIndex;
	}

	Result.ChainRoot = RefSkeleton.GetBoneName(CurrentIndex);
	Result.AvailableReach = ChainLength;
	Result.ExtensionRatio = (ChainLength > 0.0f) ? CharacterDistance / ChainLength : 0.0f;
	Result.bIsReachable = Result.ExtensionRatio <= 1.0f;

	if (!Result.bIsReachable)
	{
		// Suggest how much closer characters need to be
		float NeededDistance = ChainLength * 0.85f; // 85% extension is comfortable
		Result.SuggestedAdjustment = FVector(NeededDistance - CharacterDistance, 0.0f, 0.0f);
	}

	return Result;
}

float UPairedMontageAnalyzer::CalculateRecommendedDistance(
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	FName AttackerContactBone,
	FName VictimContactBone,
	float DesiredExtensionRatio)
{
	FReachAnalysis Analysis = AnalyzeReachRequirement(
		AttackerMesh,
		VictimMesh,
		AttackerContactBone,
		VictimContactBone,
		100.0f // Dummy distance
	);

	if (Analysis.AvailableReach <= 0.0f)
	{
		return 100.0f; // Default
	}

	return Analysis.AvailableReach * FMath::Clamp(DesiredExtensionRatio, 0.3f, 0.9f);
}

// ============================================================================
// TIMING SYNCHRONIZATION
// ============================================================================

float UPairedMontageAnalyzer::CalculateVictimStartOffset(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	float AttackerSyncTime,
	float VictimSyncTime)
{
	// Victim start offset = attacker sync time - victim sync time
	// This makes both animations reach their sync points at the same time
	return AttackerSyncTime - VictimSyncTime;
}

bool UPairedMontageAnalyzer::CheckMontageCompatibility(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	float SyncPointTime,
	TArray<FAnalysisMessage>& OutMessages)
{
	if (!AttackerMontage || !VictimMontage)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::FromString(TEXT("One or both montages are null"))
		));
		return false;
	}

	float AttackerLength = AttackerMontage->GetPlayLength();
	float VictimLength = VictimMontage->GetPlayLength();

	// Check if victim needs to be longer than attacker to reach sync point
	if (VictimLength < SyncPointTime)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Error,
			FText::Format(
				FText::FromString(TEXT("VictimMontage ({0}s) is too short to reach sync point ({1}s)")),
				FText::AsNumber(VictimLength),
				FText::AsNumber(SyncPointTime)
			)
		));
		return false;
	}

	// Warn if lengths are very different
	float LengthRatio = AttackerLength / VictimLength;
	if (LengthRatio < 0.5f || LengthRatio > 2.0f)
	{
		OutMessages.Add(FAnalysisMessage(
			EAnalysisMessageSeverity::Warning,
			FText::Format(
				FText::FromString(TEXT("Montage lengths differ significantly (Attacker: {0}s, Victim: {1}s)")),
				FText::AsNumber(AttackerLength),
				FText::AsNumber(VictimLength)
			)
		));
	}

	return true;
}

// ============================================================================
// AUTO-FILL RECOMMENDATIONS
// ============================================================================

bool UPairedMontageAnalyzer::GenerateRecommendations(
	UPairedAnimationData* PairedData,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	float& OutRecommendedSyncTime,
	float& OutRecommendedDistance,
	float& OutRecommendedVictimOffset)
{
	if (!PairedData || !PairedData->AttackerMontage || !PairedData->VictimMontage)
	{
		return false;
	}

	if (!AttackerMesh || !VictimMesh)
	{
		return false;
	}

	// Find optimal sync point
	FName AttackerBone = TEXT("hand_r");
	FName VictimBone = TEXT("spine_03");

	OutRecommendedSyncTime = FindOptimalSyncPointTime(
		PairedData->AttackerMontage,
		PairedData->VictimMontage,
		AttackerMesh,
		VictimMesh,
		AttackerBone,
		VictimBone,
		100.0f // Initial guess
	);

	// Calculate recommended distance
	OutRecommendedDistance = CalculateRecommendedDistance(
		AttackerMesh,
		VictimMesh,
		AttackerBone,
		VictimBone,
		0.7f
	);

	// Find sync point in victim montage
	float VictimSyncTime = FindSyncPointTime(PairedData->VictimMontage);
	if (VictimSyncTime < 0.0f)
	{
		VictimSyncTime = OutRecommendedSyncTime; // Use same time as fallback
	}

	OutRecommendedVictimOffset = CalculateVictimStartOffset(
		PairedData->AttackerMontage,
		PairedData->VictimMontage,
		OutRecommendedSyncTime,
		VictimSyncTime
	);

	return true;
}

bool UPairedMontageAnalyzer::AutoFillPairedAnimationData(
	UPairedAnimationData* PairedData,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh)
{
	float SyncTime, Distance, VictimOffset;

	if (!GenerateRecommendations(PairedData, AttackerMesh, VictimMesh, SyncTime, Distance, VictimOffset))
	{
		return false;
	}

	// Apply recommendations
	PairedData->SyncPointTime = SyncTime;
	PairedData->VictimRelativePosition = FVector(Distance, 0.0f, 0.0f);
	PairedData->VictimStartOffset = VictimOffset;
	PairedData->MaxWarpDistance = Distance * 1.5f;
	PairedData->MaxTriggerDistance = Distance * 2.0f;

	PairedData->MarkPackageDirty();

	return true;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

TArray<FName> UPairedMontageAnalyzer::GetDefaultAttackerContactBones()
{
	return TArray<FName>{
		FName(TEXT("hand_r")),
		FName(TEXT("hand_l")),
		FName(TEXT("WeaponEnd")),
		FName(TEXT("weapon_r"))
	};
}

TArray<FName> UPairedMontageAnalyzer::GetDefaultVictimContactBones()
{
	return TArray<FName>{
		FName(TEXT("head")),
		FName(TEXT("neck_01")),
		FName(TEXT("spine_03")),
		FName(TEXT("spine_02")),
		FName(TEXT("pelvis"))
	};
}

float UPairedMontageAnalyzer::GetBoneDistanceAtTime(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	FName AttackerBone,
	FName VictimBone,
	float Time,
	float CharacterDistance)
{
	FTransform AttackerTransform = GetBoneTransformAtTime(
		AttackerMontage, AttackerMesh, AttackerBone, Time);
	FTransform VictimTransform = GetBoneTransformAtTime(
		VictimMontage, VictimMesh, VictimBone, Time);

	// Adjust victim position for character separation
	FVector AttackerPos = AttackerTransform.GetLocation();
	FVector VictimPos = VictimTransform.GetLocation();
	VictimPos.X += CharacterDistance;

	return FVector::Dist(AttackerPos, VictimPos);
}

float UPairedMontageAnalyzer::FindClosestApproachTime(
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage,
	USkeletalMesh* AttackerMesh,
	USkeletalMesh* VictimMesh,
	FName AttackerBone,
	FName VictimBone,
	float CharacterDistance,
	float& OutMinDistance)
{
	if (!AttackerMontage || !VictimMontage || !AttackerMesh || !VictimMesh)
	{
		OutMinDistance = MAX_FLT;
		return -1.0f;
	}

	float Duration = FMath::Min(
		AttackerMontage->GetPlayLength(),
		VictimMontage->GetPlayLength()
	);

	const int32 SampleCount = 30;
	float TimeStep = Duration / SampleCount;

	float MinDistance = MAX_FLT;
	float BestTime = 0.0f;

	for (int32 i = 0; i <= SampleCount; ++i)
	{
		float Time = i * TimeStep;
		float Distance = GetBoneDistanceAtTime(
			AttackerMontage,
			VictimMontage,
			AttackerMesh,
			VictimMesh,
			AttackerBone,
			VictimBone,
			Time,
			CharacterDistance
		);

		if (Distance < MinDistance)
		{
			MinDistance = Distance;
			BestTime = Time;
		}
	}

	OutMinDistance = MinDistance;
	return BestTime;
}
