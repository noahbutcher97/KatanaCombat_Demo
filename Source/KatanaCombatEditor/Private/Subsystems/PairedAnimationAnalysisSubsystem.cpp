// Copyright Epic Games, Inc. All Rights Reserved.

#include "Subsystems/PairedAnimationAnalysisSubsystem.h"
#include "PairedAnimationAnalysisLibrary.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Editor.h"
#include "Misc/ScopedSlowTask.h"

#define LOCTEXT_NAMESPACE "PairedAnimationAnalysis"

void UPairedAnimationAnalysisSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// Context starts empty - user must call SetupContext

	// PT-21: Bind to editor world state changes to prevent stale pointer crashes
	if (GEditor)
	{
		// Clear context when map changes (prevents stale references to components in old world)
		FEditorDelegates::MapChange.AddUObject(this, &UPairedAnimationAnalysisSubsystem::OnMapChange);

		// Clear context before PIE starts (world is about to be replaced)
		FEditorDelegates::PreBeginPIE.AddUObject(this, &UPairedAnimationAnalysisSubsystem::OnPreBeginPIE);

		// Clear context when PIE ends (returning to editor world)
		FEditorDelegates::EndPIE.AddUObject(this, &UPairedAnimationAnalysisSubsystem::OnEndPIE);
	}
}

void UPairedAnimationAnalysisSubsystem::Deinitialize()
{
	// Unbind editor delegates
	if (GEditor)
	{
		FEditorDelegates::MapChange.RemoveAll(this);
		FEditorDelegates::PreBeginPIE.RemoveAll(this);
		FEditorDelegates::EndPIE.RemoveAll(this);
	}

	ClearContext();
	Super::Deinitialize();
}

// ============================================================================
// EDITOR STATE CHANGE HANDLERS (PT-21)
// ============================================================================

void UPairedAnimationAnalysisSubsystem::OnMapChange(uint32 MapChangeFlags)
{
	// Clear context when map changes to prevent stale references
	UE_LOG(LogTemp, Verbose, TEXT("PairedAnimationAnalysisSubsystem: Map change detected, clearing context"));
	ClearContext();
}

void UPairedAnimationAnalysisSubsystem::OnPreBeginPIE(bool bIsSimulating)
{
	// Clear context before PIE starts - the editor world components become invalid
	UE_LOG(LogTemp, Verbose, TEXT("PairedAnimationAnalysisSubsystem: PIE starting, clearing context"));
	ClearContext();
}

void UPairedAnimationAnalysisSubsystem::OnEndPIE(bool bIsSimulating)
{
	// Clear context when PIE ends - ensures clean state when returning to editor
	UE_LOG(LogTemp, Verbose, TEXT("PairedAnimationAnalysisSubsystem: PIE ended, clearing context"));
	ClearContext();
}

// ============================================================================
// CONTEXT SETUP
// ============================================================================

bool UPairedAnimationAnalysisSubsystem::SetupContext(
	UDebugSkelMeshComponent* AttackerMesh,
	UDebugSkelMeshComponent* VictimMesh,
	UAnimMontage* AttackerMontage,
	UAnimMontage* VictimMontage)
{
	if (!AttackerMesh || !VictimMesh)
	{
		UE_LOG(LogTemp, Warning, TEXT("PairedAnimationAnalysisSubsystem: Cannot setup context - null mesh component(s)"));
		return false;
	}

	// Use direct assignment for TWeakObjectPtr fields
	Context.AttackerMesh = AttackerMesh;
	Context.VictimMesh = VictimMesh;
	Context.AttackerMontage = AttackerMontage;
	Context.VictimMontage = VictimMontage;

	// Reset to defaults
	Context.SetDistance(150.0f);
	Context.SetAttackerRotation(FRotator::ZeroRotator);
	Context.SetVictimRotation(FRotator(0.0f, 180.0f, 0.0f)); // Facing attacker
	Context.SetVictimTimeOffset(0.0f);
	Context.SetContactThreshold(50.0f);

	InvalidateCache();

	return true;
}

void UPairedAnimationAnalysisSubsystem::UpdateContextConfiguration(
	float Distance,
	FRotator AttackerRotation,
	FRotator VictimRotation,
	float VictimTimeOffset,
	float ContactThreshold)
{
	// PT-15: Check for actual value changes before invalidating cache
	// This prevents redundant cache invalidation during slider drag operations
	const float Tolerance = 0.01f;
	const float RotationTolerance = 0.1f; // Degrees

	bool bValueChanged = false;

	if (!FMath::IsNearlyEqual(Context.GetDistance(), Distance, Tolerance))
	{
		Context.SetDistance(Distance);
		bValueChanged = true;
	}

	if (!Context.GetAttackerRotation().Equals(AttackerRotation, RotationTolerance))
	{
		Context.SetAttackerRotation(AttackerRotation);
		bValueChanged = true;
	}

	if (!Context.GetVictimRotation().Equals(VictimRotation, RotationTolerance))
	{
		Context.SetVictimRotation(VictimRotation);
		bValueChanged = true;
	}

	if (!FMath::IsNearlyEqual(Context.GetVictimTimeOffset(), VictimTimeOffset, Tolerance))
	{
		Context.SetVictimTimeOffset(VictimTimeOffset);
		bValueChanged = true;
	}

	if (!FMath::IsNearlyEqual(Context.GetContactThreshold(), ContactThreshold, Tolerance))
	{
		Context.SetContactThreshold(ContactThreshold);
		bValueChanged = true;
	}

	// Only invalidate cache if something actually changed
	if (bValueChanged)
	{
		InvalidateCache();
	}
}

void UPairedAnimationAnalysisSubsystem::ClearContext()
{
	Context = FPairedAnimationAnalysisContext();
	SpatialConstraint = ESpatialRelationship::Inferred;
	CachedHolisticAnalysis.Reset();
	bAnalysisCacheDirty = true;
}

bool UPairedAnimationAnalysisSubsystem::IsContextValid() const
{
	return Context.IsValid();
}

// ============================================================================
// CONFIGURATION EVALUATION
// ============================================================================

FConfigurationEvaluationResult UPairedAnimationAnalysisSubsystem::EvaluateConfigurationAtFrame(
	float Distance,
	FRotator AttackerRotation,
	FRotator VictimRotation,
	float Time)
{
	if (!ValidateContextForAnalysis())
	{
		return FConfigurationEvaluationResult();
	}

	// Save original state
	const float OriginalDistance = Context.GetDistance();
	const FRotator OriginalAttackerRot = Context.GetAttackerRotation();
	const FRotator OriginalVictimRot = Context.GetVictimRotation();

	// Apply test configuration
	ApplyConfiguration(Distance, AttackerRotation, VictimRotation);
	UpdateAnimationTime(Time);

	// Analyze contact at this configuration
	FContactAnalysisSnapshot Snapshot = AnalyzeContactAtFrame(Time);

	// Build result
	FConfigurationEvaluationResult Result;
	Result.SetScore(Snapshot.GetContactQuality());
	Result.SetClosestBoneDistance(Snapshot.GetClosestDistance());
	Result.SetAttackerClosestBone(Snapshot.GetAttackerBone());
	Result.SetVictimClosestBone(Snapshot.GetVictimBone());
	Result.SetContactQuality(Snapshot.GetContactQuality());

	// Restore original state
	ApplyConfiguration(OriginalDistance, OriginalAttackerRot, OriginalVictimRot);

	return Result;
}

FHolisticEvaluationResult UPairedAnimationAnalysisSubsystem::EvaluateConfigurationHolistic(
	float Distance,
	FRotator AttackerRotation,
	FRotator VictimRotation,
	int32 NumSamples)
{
	if (!ValidateContextForAnalysis())
	{
		return FHolisticEvaluationResult();
	}

	// Save original state
	const float OriginalDistance = Context.GetDistance();
	const FRotator OriginalAttackerRot = Context.GetAttackerRotation();
	const FRotator OriginalVictimRot = Context.GetVictimRotation();

	// Apply test configuration
	ApplyConfiguration(Distance, AttackerRotation, VictimRotation);

	// Get animation duration
	float Duration = 1.0f;
	if (UAnimMontage* Montage = Context.GetAttackerMontage())
	{
		Duration = Montage->GetPlayLength();
	}

	// Sample across animation
	TArray<float> Scores;
	float MinScore = TNumericLimits<float>::Max();
	float MaxScore = TNumericLimits<float>::Lowest();
	float TotalScore = 0.0f;
	float BestTime = 0.0f;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float SampleTime = (static_cast<float>(i) / static_cast<float>(NumSamples - 1)) * Duration;
		UpdateAnimationTime(SampleTime);

		FContactAnalysisSnapshot Snapshot = AnalyzeContactAtFrame(SampleTime);
		float Score = Snapshot.GetContactQuality();

		Scores.Add(Score);
		TotalScore += Score;

		if (Score > MaxScore)
		{
			MaxScore = Score;
			BestTime = SampleTime;
		}
		MinScore = FMath::Min(MinScore, Score);
	}

	// Calculate statistics
	float AverageScore = NumSamples > 0 ? TotalScore / NumSamples : 0.0f;

	// Restore original state
	ApplyConfiguration(OriginalDistance, OriginalAttackerRot, OriginalVictimRot);

	// Build result
	FHolisticEvaluationResult Result;
	Result.SetOverallScore(AverageScore);
	Result.SetPeakContactQuality(MaxScore);
	Result.SetAverageContactQuality(AverageScore);
	Result.SetPeakContactTime(BestTime);
	Result.SetSampleCount(NumSamples);
	Result.GetPerFrameScoresMutable() = MoveTemp(Scores);

	return Result;
}

FContactAnalysisTimeline UPairedAnimationAnalysisSubsystem::BuildContactTimeline(int32 NumSamples)
{
	FContactAnalysisTimeline Timeline;

	if (!ValidateContextForAnalysis())
	{
		return Timeline;
	}

	// Get animation duration
	float Duration = 1.0f;
	if (UAnimMontage* Montage = Context.GetAttackerMontage())
	{
		Duration = Montage->GetPlayLength();
	}

	float BestQuality = 0.0f;
	float BestTime = 0.0f;
	float TotalQuality = 0.0f;
	float WorstQuality = FLT_MAX;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		float SampleTime = (static_cast<float>(i) / static_cast<float>(NumSamples - 1)) * Duration;
		FContactAnalysisSnapshot Snapshot = AnalyzeContactAtFrame(SampleTime);

		float Quality = Snapshot.GetContactQuality();
		TotalQuality += Quality;

		if (Quality > BestQuality)
		{
			BestQuality = Quality;
			BestTime = SampleTime;
		}
		if (Quality < WorstQuality)
		{
			WorstQuality = Quality;
		}

		Timeline.GetSnapshotsMutable().Add(MoveTemp(Snapshot));
	}

	// Set summary statistics
	Timeline.SetBestContactTime(BestTime);
	Timeline.SetBestContactQuality(BestQuality);
	Timeline.SetAverageContactQuality(NumSamples > 0 ? TotalQuality / NumSamples : 0.0f);
	Timeline.SetWorstContactQuality(WorstQuality);

	return Timeline;
}

// ============================================================================
// OPTIMIZATION
// ============================================================================

FDistanceOptimizationResult UPairedAnimationAnalysisSubsystem::FindOptimalDistance(
	float MinDistance,
	float MaxDistance,
	int32 Steps,
	float ReferenceTime)
{
	FDistanceOptimizationResult Result;

	if (!ValidateContextForAnalysis() || Steps <= 0)
	{
		return Result;
	}

	const FRotator AttackerRot = Context.GetAttackerRotation();
	const FRotator VictimRot = Context.GetVictimRotation();

	float BestDistance = MinDistance;
	float BestScore = TNumericLimits<float>::Lowest();

	for (int32 i = 0; i < Steps; ++i)
	{
		float TestDistance = MinDistance + (MaxDistance - MinDistance) * (static_cast<float>(i) / static_cast<float>(Steps - 1));

		FConfigurationEvaluationResult EvalResult = EvaluateConfigurationAtFrame(
			TestDistance, AttackerRot, VictimRot, ReferenceTime);

		float Score = EvalResult.GetScore();

		// Record in curve (TArray<TPair<float, float>>)
		Result.GetDistanceScoreCurveMutable().Add(TPair<float, float>(TestDistance, Score));

		if (Score > BestScore)
		{
			BestScore = Score;
			BestDistance = TestDistance;
		}
	}

	Result.SetOptimalDistance(BestDistance);
	Result.SetScore(BestScore);

	return Result;
}

// ============================================================================
// PT-14: GOLDEN SECTION SEARCH OPTIMIZATION
// ============================================================================
// Golden section search achieves O(log N) complexity vs O(N) for linear search.
// Uses the golden ratio φ = (1 + √5) / 2 ≈ 1.618 to efficiently narrow the search interval.
// Best for unimodal functions (single maximum), which is typical for paired animation optimization.

FDistanceOptimizationResult UPairedAnimationAnalysisSubsystem::FindOptimalDistanceFast(
	float MinDistance,
	float MaxDistance,
	float Tolerance,
	float ReferenceTime)
{
	FDistanceOptimizationResult Result;

	if (!ValidateContextForAnalysis() || MinDistance >= MaxDistance || Tolerance <= 0.0f)
	{
		return Result;
	}

	// Golden ratio constants
	constexpr float GoldenRatio = 1.6180339887498948482f;  // φ = (1 + √5) / 2
	constexpr float RespHi = 0.6180339887498948482f;       // φ - 1 = 1/φ

	const FRotator AttackerRot = Context.GetAttackerRotation();
	const FRotator VictimRot = Context.GetVictimRotation();

	float a = MinDistance;
	float b = MaxDistance;

	// Initial interior points
	float c = b - RespHi * (b - a);
	float d = a + RespHi * (b - a);

	// Evaluate at initial points
	FConfigurationEvaluationResult EvalC = EvaluateConfigurationAtFrame(c, AttackerRot, VictimRot, ReferenceTime);
	FConfigurationEvaluationResult EvalD = EvaluateConfigurationAtFrame(d, AttackerRot, VictimRot, ReferenceTime);

	float ScoreC = EvalC.GetScore();
	float ScoreD = EvalD.GetScore();

	int32 Iterations = 0;
	constexpr int32 MaxIterations = 100;  // Safety limit

	// Golden section search (maximizing score)
	while ((b - a) > Tolerance && Iterations < MaxIterations)
	{
		++Iterations;

		if (ScoreC > ScoreD)
		{
			// Maximum is in [a, d], narrow from right
			b = d;
			d = c;
			ScoreD = ScoreC;
			c = b - RespHi * (b - a);
			EvalC = EvaluateConfigurationAtFrame(c, AttackerRot, VictimRot, ReferenceTime);
			ScoreC = EvalC.GetScore();
		}
		else
		{
			// Maximum is in [c, b], narrow from left
			a = c;
			c = d;
			ScoreC = ScoreD;
			d = a + RespHi * (b - a);
			EvalD = EvaluateConfigurationAtFrame(d, AttackerRot, VictimRot, ReferenceTime);
			ScoreD = EvalD.GetScore();
		}
	}

	// Return midpoint of final interval
	float OptimalDistance = (a + b) / 2.0f;
	FConfigurationEvaluationResult FinalEval = EvaluateConfigurationAtFrame(
		OptimalDistance, AttackerRot, VictimRot, ReferenceTime);

	Result.SetOptimalDistance(OptimalDistance);
	Result.SetScore(FinalEval.GetScore());

	// Note: No curve data populated - use linear search if curve is needed
	return Result;
}

FRotationOptimizationResult UPairedAnimationAnalysisSubsystem::FindOptimalRotationFast(
	EOptimizationTarget Target,
	float Tolerance,
	float ReferenceTime)
{
	FRotationOptimizationResult Result;

	if (!ValidateContextForAnalysis() || Tolerance <= 0.0f)
	{
		return Result;
	}

	// Golden ratio constants
	constexpr float GoldenRatio = 1.6180339887498948482f;
	constexpr float RespHi = 0.6180339887498948482f;

	const bool bOptimizeAttacker = (Target == EOptimizationTarget::Attacker);
	const float Distance = Context.GetDistance();
	const FRotator AttackerRot = Context.GetAttackerRotation();
	const FRotator VictimRot = Context.GetVictimRotation();

	// Get constraints based on spatial relationship
	float MinYaw, MaxYaw, ConstraintTolerance;
	GetCurrentRotationConstraints(MinYaw, MaxYaw, ConstraintTolerance);

	float a = MinYaw;
	float b = MaxYaw;

	// Helper lambda to evaluate at a given yaw
	auto EvaluateAtYaw = [&](float TestYaw) -> float
	{
		FRotator TestAttackerRot = AttackerRot;
		FRotator TestVictimRot = VictimRot;

		if (bOptimizeAttacker)
		{
			TestAttackerRot.Yaw = TestYaw;
		}
		else
		{
			TestVictimRot.Yaw = TestYaw;
		}

		FConfigurationEvaluationResult EvalResult = EvaluateConfigurationAtFrame(
			Distance, TestAttackerRot, TestVictimRot, ReferenceTime);
		return EvalResult.GetScore();
	};

	// Initial interior points
	float c = b - RespHi * (b - a);
	float d = a + RespHi * (b - a);

	float ScoreC = EvaluateAtYaw(c);
	float ScoreD = EvaluateAtYaw(d);

	int32 Iterations = 0;
	constexpr int32 MaxIterations = 100;

	// Golden section search (maximizing score)
	while ((b - a) > Tolerance && Iterations < MaxIterations)
	{
		++Iterations;

		if (ScoreC > ScoreD)
		{
			b = d;
			d = c;
			ScoreD = ScoreC;
			c = b - RespHi * (b - a);
			ScoreC = EvaluateAtYaw(c);
		}
		else
		{
			a = c;
			c = d;
			ScoreC = ScoreD;
			d = a + RespHi * (b - a);
			ScoreD = EvaluateAtYaw(d);
		}
	}

	// Return midpoint of final interval
	float OptimalYaw = (a + b) / 2.0f;
	float FinalScore = EvaluateAtYaw(OptimalYaw);

	// Store optimal rotation (only yaw is optimized, pitch/roll stay at 0)
	Result.SetOptimalRotation(FRotator(0.0f, OptimalYaw, 0.0f));
	Result.SetScore(FinalScore);

	// Note: No curve data populated - use linear search if curve is needed
	return Result;
}

FRotationOptimizationResult UPairedAnimationAnalysisSubsystem::FindOptimalRotation(
	EOptimizationTarget Target,
	int32 Steps,
	float ReferenceTime)
{
	FRotationOptimizationResult Result;

	if (!ValidateContextForAnalysis() || Steps <= 0)
	{
		return Result;
	}

	// PT-18: Use enum for clear semantics at call sites
	const bool bOptimizeAttacker = (Target == EOptimizationTarget::Attacker);

	const float Distance = Context.GetDistance();
	const FRotator AttackerRot = Context.GetAttackerRotation();
	const FRotator VictimRot = Context.GetVictimRotation();

	// Get constraints based on spatial relationship
	float MinYaw, MaxYaw, Tolerance;
	GetCurrentRotationConstraints(MinYaw, MaxYaw, Tolerance);

	float BestYaw = bOptimizeAttacker ? AttackerRot.Yaw : VictimRot.Yaw;
	float BestScore = TNumericLimits<float>::Lowest();

	for (int32 i = 0; i < Steps; ++i)
	{
		float TestYaw = MinYaw + (MaxYaw - MinYaw) * (static_cast<float>(i) / static_cast<float>(Steps - 1));

		FRotator TestAttackerRot = AttackerRot;
		FRotator TestVictimRot = VictimRot;

		if (bOptimizeAttacker)
		{
			TestAttackerRot.Yaw = TestYaw;
		}
		else
		{
			TestVictimRot.Yaw = TestYaw;
		}

		FConfigurationEvaluationResult EvalResult = EvaluateConfigurationAtFrame(
			Distance, TestAttackerRot, TestVictimRot, ReferenceTime);

		float Score = EvalResult.GetScore();

		// Record in curve (TArray<TPair<float, float>>)
		Result.GetYawScoreCurveMutable().Add(TPair<float, float>(TestYaw, Score));

		if (Score > BestScore)
		{
			BestScore = Score;
			BestYaw = TestYaw;
		}
	}

	// Set optimal rotation (FRotator, not just yaw)
	FRotator OptimalRot = bOptimizeAttacker ? AttackerRot : VictimRot;
	OptimalRot.Yaw = BestYaw;
	Result.SetOptimalRotation(OptimalRot);
	Result.SetScore(BestScore);

	return Result;
}

FFullOptimizationResult UPairedAnimationAnalysisSubsystem::RunFullOptimization(
	float MinDistance,
	float MaxDistance,
	int32 DistanceSteps,
	int32 RotationSteps,
	float ReferenceTime)
{
	// Call the internal implementation without progress UI
	return RunFullOptimizationInternal(MinDistance, MaxDistance, DistanceSteps, RotationSteps, ReferenceTime, nullptr);
}

FFullOptimizationResult UPairedAnimationAnalysisSubsystem::RunFullOptimizationWithProgress(
	bool& bOutWasCancelled,
	float MinDistance,
	float MaxDistance,
	int32 DistanceSteps,
	int32 RotationSteps,
	float ReferenceTime)
{
	bOutWasCancelled = false;

	if (!ValidateContextForAnalysis())
	{
		return FFullOptimizationResult();
	}

	// Calculate total steps: distance + attacker rotation + victim rotation + overhead
	const int32 TotalSteps = DistanceSteps + RotationSteps + RotationSteps + 3;

	// Create progress dialog (PT-10: Visual feedback for long operations)
	FScopedSlowTask SlowTask(TotalSteps, LOCTEXT("OptimizingPairedAnimation", "Optimizing Paired Animation..."));
	SlowTask.MakeDialog(true); // true = allow cancel

	int32 CurrentStep = 0;

	// Create TFunction from the progress callback lambda
	TFunction<bool(const FText&)> ProgressCallback = [&SlowTask, &CurrentStep, &bOutWasCancelled](const FText& StepDescription) -> bool
	{
		if (SlowTask.ShouldCancel())
		{
			bOutWasCancelled = true;
			return false; // Signal to stop
		}
		SlowTask.EnterProgressFrame(1.0f, StepDescription);
		++CurrentStep;
		return true; // Continue
	};

	FFullOptimizationResult Result = RunFullOptimizationInternal(
		MinDistance, MaxDistance, DistanceSteps, RotationSteps, ReferenceTime, &ProgressCallback);

	if (bOutWasCancelled)
	{
		Result.SetSuccess(false);
		Result.GetWarningsMutable().Add(TEXT("Optimization was cancelled by user"));
	}

	return Result;
}

FFullOptimizationResult UPairedAnimationAnalysisSubsystem::RunFullOptimizationInternal(
	float MinDistance,
	float MaxDistance,
	int32 DistanceSteps,
	int32 RotationSteps,
	float ReferenceTime,
	TFunction<bool(const FText&)>* ProgressCallback)
{
	FFullOptimizationResult Result;

	if (!ValidateContextForAnalysis())
	{
		return Result;
	}

	// Helper to report progress and check for cancellation
	auto ReportProgress = [&ProgressCallback](const FText& Description) -> bool
	{
		if (ProgressCallback && *ProgressCallback)
		{
			return (*ProgressCallback)(Description);
		}
		return true;
	};

	// Step 1: Infer spatial relationship if set to Inferred
	if (!ReportProgress(LOCTEXT("InferringSpatialRelationship", "Inferring spatial relationship...")))
	{
		return Result;
	}

	if (SpatialConstraint == ESpatialRelationship::Inferred)
	{
		FSpatialRelationshipInference Inference = InferSpatialRelationship(ReferenceTime);
		SpatialConstraint = Inference.GetInferredRelationship();
		Result.GetSpatialRelationshipMutable() = Inference;
	}

	// Step 2: Find optimal distance
	if (!ReportProgress(LOCTEXT("OptimizingDistance", "Finding optimal distance...")))
	{
		return Result;
	}

	FDistanceOptimizationResult DistResult = FindOptimalDistanceWithProgress(
		MinDistance, MaxDistance, DistanceSteps, ReferenceTime, ProgressCallback);
	Result.GetDistanceResultMutable() = DistResult;

	// Apply optimal distance for rotation optimization
	Context.SetDistance(DistResult.GetOptimalDistance());

	// Step 3: Find optimal attacker rotation
	if (!ReportProgress(LOCTEXT("OptimizingAttackerRotation", "Finding optimal attacker rotation...")))
	{
		return Result;
	}

	FRotationOptimizationResult AttackerRotResult = FindOptimalRotationWithProgress(
		EOptimizationTarget::Attacker, RotationSteps, ReferenceTime, ProgressCallback);
	Result.GetAttackerRotationResultMutable() = AttackerRotResult;

	// Apply optimal attacker rotation
	Context.SetAttackerRotation(AttackerRotResult.GetOptimalRotation());

	// Step 4: Find optimal victim rotation
	if (!ReportProgress(LOCTEXT("OptimizingVictimRotation", "Finding optimal victim rotation...")))
	{
		return Result;
	}

	FRotationOptimizationResult VictimRotResult = FindOptimalRotationWithProgress(
		EOptimizationTarget::Victim, RotationSteps, ReferenceTime, ProgressCallback);
	Result.GetVictimRotationResultMutable() = VictimRotResult;

	// Compute final score with optimal values
	if (!ReportProgress(LOCTEXT("ComputingFinalScore", "Computing final score...")))
	{
		return Result;
	}

	FConfigurationEvaluationResult FinalEval = EvaluateConfigurationAtFrame(
		DistResult.GetOptimalDistance(),
		AttackerRotResult.GetOptimalRotation(),
		VictimRotResult.GetOptimalRotation(),
		ReferenceTime);

	Result.SetOverallScore(FinalEval.GetScore());
	Result.SetSuccess(true);

	return Result;
}

FDistanceOptimizationResult UPairedAnimationAnalysisSubsystem::FindOptimalDistanceWithProgress(
	float MinDistance,
	float MaxDistance,
	int32 Steps,
	float ReferenceTime,
	TFunction<bool(const FText&)>* ProgressCallback)
{
	FDistanceOptimizationResult Result;

	if (!ValidateContextForAnalysis() || Steps <= 0)
	{
		return Result;
	}

	const FRotator AttackerRot = Context.GetAttackerRotation();
	const FRotator VictimRot = Context.GetVictimRotation();

	float BestDistance = MinDistance;
	float BestScore = TNumericLimits<float>::Lowest();

	for (int32 i = 0; i < Steps; ++i)
	{
		// Check for cancellation via progress callback
		if (ProgressCallback && *ProgressCallback)
		{
			FText StepText = FText::Format(
				LOCTEXT("DistanceStepProgress", "Testing distance {0}/{1}"),
				FText::AsNumber(i + 1),
				FText::AsNumber(Steps));

			if (!(*ProgressCallback)(StepText))
			{
				// Cancelled - return current best
				break;
			}
		}

		float TestDistance = MinDistance + (MaxDistance - MinDistance) * (static_cast<float>(i) / static_cast<float>(Steps - 1));

		FConfigurationEvaluationResult EvalResult = EvaluateConfigurationAtFrame(
			TestDistance, AttackerRot, VictimRot, ReferenceTime);

		float Score = EvalResult.GetScore();

		// Record in curve (TArray<TPair<float, float>>)
		Result.GetDistanceScoreCurveMutable().Add(TPair<float, float>(TestDistance, Score));

		if (Score > BestScore)
		{
			BestScore = Score;
			BestDistance = TestDistance;
		}
	}

	Result.SetOptimalDistance(BestDistance);
	Result.SetScore(BestScore);

	return Result;
}

FRotationOptimizationResult UPairedAnimationAnalysisSubsystem::FindOptimalRotationWithProgress(
	EOptimizationTarget Target,
	int32 Steps,
	float ReferenceTime,
	TFunction<bool(const FText&)>* ProgressCallback)
{
	FRotationOptimizationResult Result;

	if (!ValidateContextForAnalysis() || Steps <= 0)
	{
		return Result;
	}

	// PT-18: Use enum for clear semantics at call sites
	const bool bOptimizeAttacker = (Target == EOptimizationTarget::Attacker);

	const float Distance = Context.GetDistance();
	const FRotator AttackerRot = Context.GetAttackerRotation();
	const FRotator VictimRot = Context.GetVictimRotation();

	// Get constraints based on spatial relationship
	float MinYaw, MaxYaw, Tolerance;
	GetCurrentRotationConstraints(MinYaw, MaxYaw, Tolerance);

	float BestYaw = bOptimizeAttacker ? AttackerRot.Yaw : VictimRot.Yaw;
	float BestScore = TNumericLimits<float>::Lowest();

	FText CharacterName = bOptimizeAttacker
		? LOCTEXT("Attacker", "attacker")
		: LOCTEXT("Victim", "victim");

	for (int32 i = 0; i < Steps; ++i)
	{
		// Check for cancellation via progress callback
		if (ProgressCallback && *ProgressCallback)
		{
			FText StepText = FText::Format(
				LOCTEXT("RotationStepProgress", "Testing {0} rotation {1}/{2}"),
				CharacterName,
				FText::AsNumber(i + 1),
				FText::AsNumber(Steps));

			if (!(*ProgressCallback)(StepText))
			{
				// Cancelled - return current best
				break;
			}
		}

		float TestYaw = MinYaw + (MaxYaw - MinYaw) * (static_cast<float>(i) / static_cast<float>(Steps - 1));

		FRotator TestAttackerRot = AttackerRot;
		FRotator TestVictimRot = VictimRot;

		if (bOptimizeAttacker)
		{
			TestAttackerRot.Yaw = TestYaw;
		}
		else
		{
			TestVictimRot.Yaw = TestYaw;
		}

		FConfigurationEvaluationResult EvalResult = EvaluateConfigurationAtFrame(
			Distance, TestAttackerRot, TestVictimRot, ReferenceTime);

		float Score = EvalResult.GetScore();

		// Record in curve (TArray<TPair<float, float>>)
		Result.GetYawScoreCurveMutable().Add(TPair<float, float>(TestYaw, Score));

		if (Score > BestScore)
		{
			BestScore = Score;
			BestYaw = TestYaw;
		}
	}

	// Set optimal rotation (FRotator, not just yaw)
	FRotator OptimalRot = bOptimizeAttacker ? AttackerRot : VictimRot;
	OptimalRot.Yaw = BestYaw;
	Result.SetOptimalRotation(OptimalRot);
	Result.SetScore(BestScore);

	return Result;
}

// ============================================================================
// SPATIAL RELATIONSHIP
// ============================================================================

FSpatialRelationshipInference UPairedAnimationAnalysisSubsystem::InferSpatialRelationship(float AnalysisTime)
{
	FSpatialRelationshipInference Result;

	if (!ValidateContextForAnalysis())
	{
		return Result;
	}

	UpdateAnimationTime(AnalysisTime);

	// Get relevant bone positions
	FVector AttackerPelvis = GetAttackerBoneWorldLocation(TEXT("pelvis"));
	FVector VictimPelvis = GetVictimBoneWorldLocation(TEXT("pelvis"));

	// Calculate direction from attacker to victim
	FVector AttackDirection = (VictimPelvis - AttackerPelvis).GetSafeNormal2D();

	// Get victim's forward vector
	UDebugSkelMeshComponent* VictimMesh = Context.GetVictimMesh();
	FVector VictimForward = VictimMesh ? VictimMesh->GetForwardVector() : FVector::ForwardVector;

	// Use the pure math function from the utility library
	ESpatialRelationship InferredRelationship = UPairedAnimationAnalysisLibrary::InferRelationshipFromAngle(
		0.0f, // Not used in the function
		AttackDirection,
		VictimForward);

	// Calculate angle and confidence using pure math functions
	float DotProduct = FVector::DotProduct(AttackDirection, VictimForward);
	float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(DotProduct, -1.0f, 1.0f)));
	float Confidence = UPairedAnimationAnalysisLibrary::CalculateSpatialInferenceConfidence(AngleDegrees);

	Result.SetInferredRelationship(InferredRelationship);
	Result.SetConfidence(Confidence);
	Result.SetVictimFacingAngle(AngleDegrees);

	return Result;
}

void UPairedAnimationAnalysisSubsystem::SetSpatialRelationshipConstraint(ESpatialRelationship Relationship)
{
	if (SpatialConstraint != Relationship)
	{
		SpatialConstraint = Relationship;
		InvalidateCache();
	}
}

ESpatialRelationship UPairedAnimationAnalysisSubsystem::GetSpatialRelationshipConstraint() const
{
	return SpatialConstraint;
}

// ============================================================================
// CONTACT ANALYSIS
// ============================================================================

float UPairedAnimationAnalysisSubsystem::ComputeClosestBoneDistance(FName& OutAttackerBone, FName& OutVictimBone)
{
	if (!ValidateContextForAnalysis())
	{
		return TNumericLimits<float>::Max();
	}

	TArray<FName> AttackerBones = GetAttackerBoneNames();
	TArray<FName> VictimBones = GetVictimBoneNames();

	float ClosestDistance = TNumericLimits<float>::Max();
	OutAttackerBone = NAME_None;
	OutVictimBone = NAME_None;

	for (const FName& ABone : AttackerBones)
	{
		FVector APos = GetAttackerBoneWorldLocation(ABone);

		for (const FName& VBone : VictimBones)
		{
			FVector VPos = GetVictimBoneWorldLocation(VBone);
			float Distance = FVector::Dist(APos, VPos);

			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				OutAttackerBone = ABone;
				OutVictimBone = VBone;
			}
		}
	}

	return ClosestDistance;
}

FContactAnalysisSnapshot UPairedAnimationAnalysisSubsystem::AnalyzeContactAtFrame(float Time)
{
	FContactAnalysisSnapshot Snapshot;

	if (!ValidateContextForAnalysis())
	{
		return Snapshot;
	}

	UpdateAnimationTime(Time);

	FName AttackerBone, VictimBone;
	float ClosestDistance = ComputeClosestBoneDistance(AttackerBone, VictimBone);

	Snapshot.SetTime(Time);
	Snapshot.SetClosestDistance(ClosestDistance);
	Snapshot.SetAttackerBone(AttackerBone);
	Snapshot.SetVictimBone(VictimBone);

	// Calculate score based on distance and threshold using pure math function
	float Threshold = Context.GetContactThreshold();
	float Quality = UPairedAnimationAnalysisLibrary::CalculateContactScore(ClosestDistance, Threshold);

	Snapshot.SetContactQuality(Quality);
	Snapshot.SetHasPenetration(ClosestDistance <= Context.GetPenetrationThreshold());

	// Get contact point (midpoint between closest bones)
	FVector AttackerPos = GetAttackerBoneWorldLocation(AttackerBone);
	FVector VictimPos = GetVictimBoneWorldLocation(VictimBone);
	Snapshot.SetContactPoint(UPairedAnimationAnalysisLibrary::CalculateMidpoint(AttackerPos, VictimPos));

	return Snapshot;
}

// ============================================================================
// MESH UTILITIES
// ============================================================================

void UPairedAnimationAnalysisSubsystem::ApplyConfiguration(
	float Distance,
	FRotator AttackerRotation,
	FRotator VictimRotation)
{
	UDebugSkelMeshComponent* AttackerMesh = Context.GetAttackerMesh();
	UDebugSkelMeshComponent* VictimMesh = Context.GetVictimMesh();

	if (!AttackerMesh || !VictimMesh)
	{
		return;
	}

	// Attacker at origin with specified rotation
	AttackerMesh->SetWorldLocation(FVector::ZeroVector);
	AttackerMesh->SetWorldRotation(AttackerRotation);

	// Victim at distance along attacker's forward vector
	FVector AttackerForward = AttackerRotation.Vector();
	FVector VictimLocation = AttackerForward * Distance;
	VictimMesh->SetWorldLocation(VictimLocation);
	VictimMesh->SetWorldRotation(VictimRotation);

	// Update context
	Context.SetDistance(Distance);
	Context.SetAttackerRotation(AttackerRotation);
	Context.SetVictimRotation(VictimRotation);
}

void UPairedAnimationAnalysisSubsystem::UpdateAnimationTime(float Time)
{
	UDebugSkelMeshComponent* AttackerMesh = Context.GetAttackerMesh();
	UDebugSkelMeshComponent* VictimMesh = Context.GetVictimMesh();

	if (AttackerMesh)
	{
		AttackerMesh->SetPosition(Time);
		AttackerMesh->RefreshBoneTransforms();
	}

	if (VictimMesh)
	{
		float VictimTime = Time + Context.GetVictimTimeOffset();
		VictimMesh->SetPosition(FMath::Max(0.0f, VictimTime));
		VictimMesh->RefreshBoneTransforms();
	}
}

FVector UPairedAnimationAnalysisSubsystem::GetAttackerBoneWorldLocation(FName BoneName) const
{
	if (UDebugSkelMeshComponent* Mesh = Context.GetAttackerMesh())
	{
		return Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
	}
	return FVector::ZeroVector;
}

FVector UPairedAnimationAnalysisSubsystem::GetVictimBoneWorldLocation(FName BoneName) const
{
	if (UDebugSkelMeshComponent* Mesh = Context.GetVictimMesh())
	{
		return Mesh->GetBoneLocation(BoneName, EBoneSpaces::WorldSpace);
	}
	return FVector::ZeroVector;
}

TArray<FName> UPairedAnimationAnalysisSubsystem::GetAttackerBoneNames() const
{
	TArray<FName> BoneNames;

	if (UDebugSkelMeshComponent* Mesh = Context.GetAttackerMesh())
	{
		if (const USkeletalMesh* SkelMesh = Mesh->GetSkeletalMeshAsset())
		{
			const FReferenceSkeleton& RefSkeleton = SkelMesh->GetRefSkeleton();
			for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
			{
				BoneNames.Add(RefSkeleton.GetBoneName(i));
			}
		}
	}

	return BoneNames;
}

TArray<FName> UPairedAnimationAnalysisSubsystem::GetVictimBoneNames() const
{
	TArray<FName> BoneNames;

	if (UDebugSkelMeshComponent* Mesh = Context.GetVictimMesh())
	{
		if (const USkeletalMesh* SkelMesh = Mesh->GetSkeletalMeshAsset())
		{
			const FReferenceSkeleton& RefSkeleton = SkelMesh->GetRefSkeleton();
			for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
			{
				BoneNames.Add(RefSkeleton.GetBoneName(i));
			}
		}
	}

	return BoneNames;
}

// ============================================================================
// INTERNAL HELPERS
// ============================================================================

bool UPairedAnimationAnalysisSubsystem::ValidateContextForAnalysis() const
{
	if (!Context.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("PairedAnimationAnalysisSubsystem: Context not valid for analysis. Call SetupContext first."));
		return false;
	}
	return true;
}

void UPairedAnimationAnalysisSubsystem::GetCurrentRotationConstraints(float& OutMinYaw, float& OutMaxYaw, float& OutTolerance) const
{
	// Use the pure math function from the utility library
	UPairedAnimationAnalysisLibrary::GetRelationshipConstraints(
		SpatialConstraint,
		OutMinYaw,
		OutMaxYaw,
		OutTolerance);
}

void UPairedAnimationAnalysisSubsystem::InvalidateCache()
{
	CachedHolisticAnalysis.Reset();
	bAnalysisCacheDirty = true;
}

#undef LOCTEXT_NAMESPACE
