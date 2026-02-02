// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Data/PairedAnimationEditorTypes.h"

class UAnimMontage;
class USkeletalMesh;
class UStaticMesh;
class UDebugSkelMeshComponent;
class UStaticMeshComponent;
class FAdvancedPreviewScene;
class SSlider;
class SCheckBox;
class SExpandableArea;
class SEditableTextBox;
class SObjectPropertyEntryBox;
template<typename OptionType> class SComboBox;

/**
 * Paired Animation Preview Tool - Industry-Grade Editor
 *
 * A comprehensive preview environment for analyzing paired animations with
 * full procedural spatial analysis, optimization tools, and visualization.
 *
 * Features:
 * - Single shared 3D viewport with both characters
 * - Configurable position/rotation offsets for each character
 * - Procedural contact point detection with confidence scoring
 * - Bone trajectory analysis with velocity tracking
 * - One-click optimization (find optimal distance, rotation, sync time)
 * - Configurable weapon sockets
 * - Toggleable visualization layers
 * - Real-time analysis graphs
 * - Export analysis data
 *
 * Access: Window > Paired Animation Preview
 */
class KATANACOMBATEDITOR_API SPairedAnimationPreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SPairedAnimationPreview) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SPairedAnimationPreview();

	/** Register with editor menu */
	static void RegisterTabSpawner();
	static void UnregisterTabSpawner();
	static FName GetTabName() { return TEXT("PairedAnimationPreview"); }

private:
	// ========================================================================
	// MODEL (PT-11: Centralized State)
	// ========================================================================
	// All preview state is managed through this Model struct.
	// UI widgets observe/modify Model state, analysis reads from it.
	// This enables: testable state management, potential undo system, serialization.

	FPairedAnimationPreviewModel Model;

	// ========================================================================
	// SHARED PREVIEW SCENE (Runtime UObjects - NOT in Model)
	// ========================================================================

	TSharedPtr<FAdvancedPreviewScene> SharedPreviewScene;
	TSharedPtr<SWidget> ViewportWidget;

	UDebugSkelMeshComponent* AttackerMeshComponent = nullptr;
	UDebugSkelMeshComponent* VictimMeshComponent = nullptr;

	// Weapon mesh components (attached to character skeletons)
	UStaticMeshComponent* AttackerWeaponMeshComponent = nullptr;
	UStaticMeshComponent* VictimWeaponMeshComponent = nullptr;

	void SetupSharedPreviewScene();
	void UpdateAttackerMesh(USkeletalMesh* Mesh);
	void UpdateVictimMesh(USkeletalMesh* Mesh);
	void UpdateCharacterPositions();
	void UpdateAnimations(float Time);

	/** Update attacker weapon mesh and attach to skeleton */
	void UpdateAttackerWeaponMesh(UStaticMesh* Mesh);

	/** Update victim weapon mesh and attach to skeleton */
	void UpdateVictimWeaponMesh(UStaticMesh* Mesh);

	/** Re-attach weapons to their current sockets (call after skeleton update) */
	void ReattachWeapons();

	/** Get world position of weapon contact point */
	FVector GetWeaponContactPosition(UStaticMeshComponent* WeaponMesh, const FWeaponMeshConfig& Config, EContactPointType ContactType) const;

	/** Check if weapon meshes are available for contact detection */
	bool HasAttackerWeapon() const { return AttackerWeaponMeshComponent != nullptr && Model.AttackerWeaponConfig.IsValid(); }
	bool HasVictimWeapon() const { return VictimWeaponMeshComponent != nullptr && Model.VictimWeaponConfig.IsValid(); }

	// --- Weapon Socket Configuration ---

	/** Get all socket names from a skeletal mesh */
	TArray<FName> GetSkeletalMeshSockets(UDebugSkelMeshComponent* MeshComp) const;

	/** Get all socket names from a static mesh */
	TArray<FName> GetStaticMeshSockets(UStaticMesh* StaticMesh) const;

	/** PT-24: Consolidated helper for refreshing socket options */
	void RefreshWeaponSocketOptionsForCharacter(bool bIsAttacker);

	/** Refresh socket options for weapon configuration dropdowns */
	void RefreshAttackerWeaponSocketOptions() { RefreshWeaponSocketOptionsForCharacter(true); }
	void RefreshVictimWeaponSocketOptions() { RefreshWeaponSocketOptionsForCharacter(false); }

	// Socket option arrays for dropdowns
	TArray<TSharedPtr<FName>> AttackerCharacterSocketOptions;  // Sockets on attacker skeleton
	TArray<TSharedPtr<FName>> AttackerWeaponSocketOptions;     // Sockets on attacker's weapon mesh
	TArray<TSharedPtr<FName>> VictimCharacterSocketOptions;    // Sockets on victim skeleton
	TArray<TSharedPtr<FName>> VictimWeaponSocketOptions;       // Sockets on victim's weapon mesh

	// Socket combo boxes
	TSharedPtr<SComboBox<TSharedPtr<FName>>> AttackerCharacterSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> AttackerWeaponGripSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> AttackerWeaponTipSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> AttackerWeaponMidSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> AttackerWeaponBaseSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> VictimCharacterSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> VictimWeaponGripSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> VictimWeaponTipSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> VictimWeaponMidSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> VictimWeaponBaseSocketCombo;

	// Socket change handlers
	void OnAttackerCharacterSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnAttackerWeaponGripSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnAttackerWeaponTipSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnAttackerWeaponMidSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnAttackerWeaponBaseSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnVictimCharacterSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnVictimWeaponGripSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnVictimWeaponTipSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnVictimWeaponMidSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);
	void OnVictimWeaponBaseSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType);

	// Offset change handlers
	void OnAttackerWeaponOffsetChanged(FVector NewOffset);
	void OnAttackerWeaponRotationChanged(FRotator NewRotation);
	void OnVictimWeaponOffsetChanged(FVector NewOffset);
	void OnVictimWeaponRotationChanged(FRotator NewRotation);

	/** Build weapon configuration sub-panel (expandable) */
	TSharedRef<SWidget> BuildWeaponConfigPanel();

	// ========================================================================
	// CHARACTER CONFIGURATION (State in Model)
	// ========================================================================

	void ApplyCharacterConfigs();
	void OnAttackerPositionChanged(FVector NewPosition);
	void OnVictimPositionChanged(FVector NewPosition);
	void OnAttackerRotationChanged(FRotator NewRotation);
	void OnVictimRotationChanged(FRotator NewRotation);
	void OnLockedDistanceChanged(float NewDistance);

	// ========================================================================
	// ANIMATION DATA (State in Model, UI arrays here)
	// ========================================================================

	// Section options for dropdown UI (populated from montages)
	TArray<TSharedPtr<FName>> AttackerSectionOptions;
	TArray<TSharedPtr<FName>> VictimSectionOptions;

	void RefreshAttackerSectionOptions();
	void RefreshVictimSectionOptions();
	void OnAttackerSectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectType);
	void OnVictimSectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectType);
	float GetSectionStartTime(UAnimMontage* Montage, FName SectionName) const;
	float GetSectionDuration(UAnimMontage* Montage, FName SectionName) const;
	void GetSectionTimeRange(UAnimMontage* Montage, FName SectionName, float& OutStart, float& OutEnd) const;

	void RecalculateMaxDuration();

	// Playback direction for ping-pong mode (not in Model - runtime playback state)
	int32 PingPongDirection = 1;

	// Time accessors delegating to Model
	float GetAttackerTime() const { return Model.GetAttackerTime(); }
	float GetVictimTime() const { return Model.GetVictimTime(); }

	// ========================================================================
	// SPATIAL RELATIONSHIP (PT-2, State in Model)
	// ========================================================================

	// Infer spatial relationship from animation data at peak contact time
	FSpatialRelationshipInference InferSpatialRelationship();

	// Get rotation constraint for current relationship (used by optimization)
	FSpatialRotationConstraint GetRotationConstraintForRelationship() const;

	// Get the effective relationship (either user-selected or inferred)
	ESpatialRelationship GetEffectiveSpatialRelationship() const;

	// UI callback for relationship dropdown
	void OnSpatialRelationshipChanged(ESpatialRelationship NewRelationship);

	// Get display name for relationship type
	static FString GetRelationshipDisplayName(ESpatialRelationship Relationship);

	// ========================================================================
	// PROCEDURAL ANALYSIS ENGINE (Caches in Model, settings here for now)
	// ========================================================================

	// Analysis settings not yet in Model
	float ContactPredictionWindow = 0.2f;
	int32 AnalysisSampleRate = 60;
	int32 TrajectorySampleCount = 120;

	// Analysis results not yet in Model (consider migrating later)
	FRootMotionAnalysis AttackerRootMotion;
	FRootMotionAnalysis VictimRootMotion;
	FDistanceAnalysis DistanceAnalysis;
	FTimingAnalysis TimingAnalysis;

	// Undo/Redo (PT-19) - delegating to Model
	void PushStateToHistory(const FString& Description = TEXT(""));
	bool CanUndo() const { return Model.CanUndo(); }
	bool CanRedo() const { return Model.CanRedo(); }
	void UndoOptimization();
	void RedoOptimization();
	void ApplyHistoryState(const FPreviewOptimizationState& State);

	void RebuildAnalysisCache();
	void RebuildTrajectoryCache();
	/** PT-22: Progress-enabled trajectory cache rebuild with cancellation support */
	bool RebuildTrajectoryCacheWithProgress();
	void RebuildDistanceAnalysis();
	void RebuildTimingAnalysis();
	void RebuildHolisticAnalysis();
	FPairedFrameAnalysis AnalyzeFrame(float Time);
	FPairedFrameAnalysis GetAnalysisAtTime(float Time) const;

	// Holistic timeline analysis for intelligent optimization
	FTrajectoryFrameSample SampleTrajectoryFrame(float Time);
	void DetectActivityPhases();
	void DetectContactPhases();
	void ComputeOptimizationWeights();
	float GetActivityWeightAtTime(float Time) const;
	float EvaluateConfigurationHolistic(float Distance, FRotator AttackerRot, FRotator VictimRot);

	// Core procedural analysis functions
	TArray<FProceduralContactPoint> ComputeContactPoints(float Time);
	TArray<FProceduralContactPoint> PredictFutureContacts(float CurrentTime, float LookAheadTime);
	FVector GetBoneWorldLocation(UDebugSkelMeshComponent* Mesh, FName BoneName) const;
	FVector GetSocketWorldLocation(UDebugSkelMeshComponent* Mesh, FName SocketName) const;
	FVector ComputeBoneVelocity(UDebugSkelMeshComponent* Mesh, FName BoneName, float Time, float DeltaTime = 0.016f);
	FName FindClosestBone(UDebugSkelMeshComponent* Mesh, const FVector& WorldLocation, float& OutDistance) const;
	float ComputeClosestSkeletonDistance(FName& OutAttackerBone, FName& OutVictimBone) const;
	FVector ComputeCenterOfMass(UDebugSkelMeshComponent* Mesh) const;
	TArray<FName> GetAllBoneNames(UDebugSkelMeshComponent* Mesh) const;
	TArray<FString> GetActiveNotifies(UAnimMontage* Montage, float Time) const;

	// ========================================================================
	// MULTI-CONTACT POINT ANALYSIS (Weights in Model)
	// ========================================================================

	// Bone configurations for multi-contact tracking (not yet unified in Model)
	FMultiContactBoneConfig AttackerBoneConfig;
	FMultiContactBoneConfig VictimBoneConfig;

	// InitializeContactTypeWeights delegates to Model
	void InitializeContactTypeWeights() { Model.InitializeContactTypeWeights(); }

	// Multi-contact analysis functions
	FMultiContactAnalysis ComputeMultiContactPoints(float Time);
	float ComputePairwiseDistance(EContactPointType AttackerContact, EContactPointType VictimContact) const;
	bool DetectPenetration(float Distance, EContactPointType AttackerType, EContactPointType VictimType, float& OutPenetrationDepth) const;
	float EvaluateMultiContactQuality(const FMultiContactAnalysis& Analysis) const;
	float GetPenetrationThreshold(EContactPointType Type) const;

	// Multi-contact aware optimization
	float EvaluateConfigurationWithMultiContact(float Distance, FRotator AttackerRot, FRotator VictimRot);

	// Multi-contact visualization
	void DrawMultiContactPoints();

	// ========================================================================
	// OPTIMIZATION ENGINE
	// ========================================================================

	FOptimizationResult RunFullOptimization();
	float FindOptimalDistance(float MinDist = 50.0f, float MaxDist = 400.0f, int32 Steps = 50);
	FRotator FindOptimalAttackerRotation(int32 Steps = 36);
	FRotator FindOptimalVictimRotation(int32 Steps = 36);
	float FindOptimalSyncTime();

	// Evaluates configuration at a specific frame - used for Global Paired Orientation (t=0)
	float EvaluateConfigurationAtFrame(float Distance, FRotator AttackerRot, FRotator VictimRot, float Time);
	// Legacy wrapper - evaluates at reference frame (t=0)
	float EvaluateConfiguration(float Distance, FRotator AttackerRot, FRotator VictimRot);

	void ApplyOptimizationResult(const FOptimizationResult& Result);
	void OnOptimizeClicked();
	void OnFindOptimalDistanceClicked();
	void OnFindOptimalRotationClicked();
	void OnFindOptimalSyncClicked();

	// ========================================================================
	// VISUALIZATION (Layers in Model, colors here for UI)
	// ========================================================================

	// Colors for visualization (UI-specific, not in Model)
	FLinearColor AttackerColor = FLinearColor(0.2f, 0.6f, 1.0f);      // Blue
	FLinearColor VictimColor = FLinearColor(1.0f, 0.4f, 0.2f);        // Orange
	FLinearColor ContactColor = FLinearColor(1.0f, 1.0f, 0.0f);       // Yellow
	FLinearColor WeaponTraceColor = FLinearColor(1.0f, 0.0f, 0.0f);   // Red
	FLinearColor PredictedContactColor = FLinearColor(0.5f, 1.0f, 0.5f); // Light Green
	FLinearColor TrajectoryColor = FLinearColor(0.8f, 0.2f, 0.8f);    // Magenta
	FLinearColor COMColor = FLinearColor(1.0f, 1.0f, 1.0f);           // White

	bool IsVisualizationActive(EVisualizationLayer Layer) const;
	void SetVisualizationActive(EVisualizationLayer Layer, bool bActive);
	void DrawDebugVisualization();
	void DrawSkeletons();
	void DrawBoneNames();
	void DrawVelocityVectors();
	void DrawContactPoints();
	void DrawContactTrails();
	void DrawWeaponTrace();
	void DrawRootMotionPath();
	void DrawDistanceLines();
	void DrawReachEnvelopes();
	void DrawCenterOfMass();
	void DrawCollisionBounds();
	void DrawAxisGrid();

	// ========================================================================
	// TIMELINE & PLAYBACK
	// ========================================================================

	TSharedPtr<SSlider> TimelineSlider;

	void OnTimelineValueChanged(float NewValue);
	void OnPlayPauseClicked();
	void OnStepForward();
	void OnStepBackward();
	void OnStepForwardLarge();
	void OnStepBackwardLarge();
	void OnResetClicked();
	void OnGoToMaxContactClicked();
	void OnGoToMaxSpeedClicked();
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// ========================================================================
	// UI CONSTRUCTION
	// ========================================================================

	TSharedRef<SWidget> BuildMainLayout();
	TSharedRef<SWidget> BuildAssetSelectionPanel();
	TSharedRef<SWidget> BuildPositioningPanel();
	TSharedRef<SWidget> BuildTimelineControls();
	TSharedRef<SWidget> BuildAnalysisPanel();
	TSharedRef<SWidget> BuildOptimizationPanel();
	TSharedRef<SWidget> BuildVisualizationPanel();
	TSharedRef<SWidget> BuildSettingsPanel();
	TSharedRef<SWidget> BuildQuickActionsBar();
	TSharedRef<SWidget> BuildGraphsPanel();

	// ========================================================================
	// TEXT DISPLAYS
	// ========================================================================

	TSharedPtr<STextBlock> TimeDisplayText;
	TSharedPtr<STextBlock> ContactInfoText;
	TSharedPtr<STextBlock> DistanceInfoText;
	TSharedPtr<STextBlock> VelocityInfoText;
	TSharedPtr<STextBlock> OptimizationInfoText;
	TSharedPtr<STextBlock> StatusText;

	void UpdateAnalyticsDisplay();
	FText GetTimeDisplayText() const;
	FText GetContactInfoText() const;
	FText GetDistanceInfoText() const;
	FText GetVelocityInfoText() const;
	FText GetOptimizationInfoText() const;
	FText GetStatusText() const;

	// ========================================================================
	// ASSET SELECTION
	// ========================================================================

	FString GetAttackerMontagePath() const;
	FString GetVictimMontagePath() const;
	FString GetAttackerSkeletonPath() const;
	FString GetVictimSkeletonPath() const;

	void OnAttackerMontageSelected(const FAssetData& AssetData);
	void OnVictimMontageSelected(const FAssetData& AssetData);
	void OnAttackerSkeletonSelected(const FAssetData& AssetData);
	void OnVictimSkeletonSelected(const FAssetData& AssetData);
	void OnAttackerWeaponMeshSelected(const FAssetData& AssetData);
	void OnVictimWeaponMeshSelected(const FAssetData& AssetData);

	FString GetAttackerWeaponMeshPath() const;
	FString GetVictimWeaponMeshPath() const;

	// Section Selection Widgets
	TSharedPtr<SComboBox<TSharedPtr<FName>>> AttackerSectionCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>> VictimSectionCombo;

	// ========================================================================
	// SOCKET CONFIGURATION
	// ========================================================================

	TSharedPtr<SEditableTextBox> WeaponStartSocketInput;
	TSharedPtr<SEditableTextBox> WeaponEndSocketInput;

	void OnWeaponStartSocketChanged(const FText& NewText, ETextCommit::Type CommitType);
	void OnWeaponEndSocketChanged(const FText& NewText, ETextCommit::Type CommitType);
	TArray<FName> GetAvailableSockets(UDebugSkelMeshComponent* Mesh) const;

	// ========================================================================
	// ORIENTATION PRESETS
	// ========================================================================

	/** Apply orientation preset: Characters facing each other (180° victim yaw) */
	void ApplyOrientationPreset_Facing();

	/** Apply orientation preset: Attacker behind victim (0° victim yaw) */
	void ApplyOrientationPreset_Behind();

	/** Apply orientation preset: Attacker on victim's left side (90° victim yaw) */
	void ApplyOrientationPreset_LeftSide();

	/** Apply orientation preset: Attacker on victim's right side (-90° victim yaw) */
	void ApplyOrientationPreset_RightSide();

	void SaveCurrentAsPreset(const FString& PresetName);
	void LoadPreset(const FString& PresetName);

	// ========================================================================
	// EXPORT
	// ========================================================================

	void ExportAnalysisToCSV();
	void ExportAnalysisToJSON();
	void CopyAnalysisToClipboard();

	// ========================================================================
	// TAB SPAWNER
	// ========================================================================

	static TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);
};
