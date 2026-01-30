// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "MontageAnalysisTypes.h"

class UAnimMontage;
class USkeletalMesh;
class UPairedAnimationData;
class UDebugSkelMeshComponent;
class FAdvancedPreviewScene;
class SSlider;

/**
 * Per-frame analysis data for display
 */
struct FFrameAnalysisData
{
	float Time = 0.0f;

	// Root Motion
	FVector RootMotionTranslation = FVector::ZeroVector;
	FRotator RootMotionRotation = FRotator::ZeroRotator;
	FVector RootMotionVelocity = FVector::ZeroVector;
	float RootMotionSpeed = 0.0f;

	// Bone Data (for selected bones)
	TMap<FName, FTransform> BoneTransforms;
	TMap<FName, FVector> BoneVelocities;

	// Active Notifies at this frame
	TArray<FString> ActiveNotifies;
	TArray<FString> ActiveNotifyStates;

	// Phase Information
	FString CurrentPhase;

	// Warp Data (if applicable)
	bool bHasWarpTarget = false;
	FVector WarpTargetLocation = FVector::ZeroVector;
	float WarpProgress = 0.0f;
};

/**
 * Comprehensive Montage Analysis Dashboard
 *
 * Features:
 * - 3D preview viewport with skeletal mesh
 * - Timeline scrubber with notify markers
 * - Per-frame analytics (root motion, bone trajectories, physics)
 * - Notify visualization
 * - Paired animation comparison
 *
 * Access: Window > Montage Analysis Dashboard
 */
class KATANACOMBATEDITOR_API SMontageAnalysisDashboard : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMontageAnalysisDashboard) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SMontageAnalysisDashboard();

	/** Register with editor menu */
	static void RegisterTabSpawner();
	static void UnregisterTabSpawner();
	static FName GetTabName() { return TEXT("MontageAnalysisDashboard"); }

private:
	// ========================================================================
	// PREVIEW SCENE
	// ========================================================================

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<SWidget> ViewportWidget;
	UDebugSkelMeshComponent* PreviewMeshComponent = nullptr;

	void SetupPreviewScene();
	void UpdatePreviewMesh(USkeletalMesh* Mesh);
	void UpdatePreviewAnimation(float Time);

	// ========================================================================
	// MONTAGE DATA
	// ========================================================================

	TWeakObjectPtr<UAnimMontage> CurrentMontage;
	TWeakObjectPtr<USkeletalMesh> CurrentSkeleton;
	TWeakObjectPtr<UPairedAnimationData> CurrentPairedData;

	float CurrentTime = 0.0f;
	float MontageDuration = 0.0f;
	bool bIsPlaying = false;
	float PlaybackSpeed = 1.0f;

	// Cached analysis data
	FMontageAnalysisResult CachedAnalysis;
	TArray<FFrameAnalysisData> FrameDataCache;
	int32 FrameCacheResolution = 60; // Samples per second

	void OnMontageChanged();
	void RebuildFrameCache();
	FFrameAnalysisData AnalyzeFrame(float Time);

	// ========================================================================
	// TIMELINE
	// ========================================================================

	TSharedPtr<SSlider> TimelineSlider;

	void OnTimelineValueChanged(float NewValue);
	void OnPlayPauseClicked();
	void OnStepForward();
	void OnStepBackward();
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// ========================================================================
	// UI BUILDERS
	// ========================================================================

	TSharedRef<SWidget> BuildToolbar();
	TSharedRef<SWidget> BuildViewport();
	TSharedRef<SWidget> BuildTimeline();
	TSharedRef<SWidget> BuildAnalyticsPanel();
	TSharedRef<SWidget> BuildNotifyPanel();
	TSharedRef<SWidget> BuildRootMotionPanel();
	TSharedRef<SWidget> BuildBoneDataPanel();
	TSharedRef<SWidget> BuildWarpingPanel();

	// ========================================================================
	// ANALYTICS DISPLAY
	// ========================================================================

	// Text blocks for live data
	TSharedPtr<STextBlock> TimeDisplayText;
	TSharedPtr<STextBlock> FrameDisplayText;
	TSharedPtr<STextBlock> PhaseDisplayText;
	TSharedPtr<STextBlock> RootMotionText;
	TSharedPtr<STextBlock> ActiveNotifiesText;
	TSharedPtr<STextBlock> BoneDataText;
	TSharedPtr<STextBlock> WarpDataText;

	void UpdateAnalyticsDisplay();
	FText GetCurrentTimeText() const;
	FText GetCurrentFrameText() const;
	FText GetRootMotionText() const;
	FText GetActiveNotifiesText() const;
	FText GetBoneDataText() const;

	// ========================================================================
	// ASSET SELECTION
	// ========================================================================

	FString GetMontagePath() const;
	FString GetSkeletonPath() const;
	FString GetPairedDataPath() const;

	void OnMontageSelected(const FAssetData& AssetData);
	void OnSkeletonSelected(const FAssetData& AssetData);
	void OnPairedDataSelected(const FAssetData& AssetData);

	// ========================================================================
	// VISUALIZATION OPTIONS
	// ========================================================================

	bool bShowRootMotionTrail = true;
	bool bShowBoneTrajectories = false;
	bool bShowNotifyMarkers = true;
	bool bShowWarpTargets = true;
	TArray<FName> SelectedBones;

	void DrawVisualizationOverlays();

	// ========================================================================
	// TAB SPAWNER
	// ========================================================================

	static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
};
