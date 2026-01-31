// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

class UAnimMontage;
class USkeletalMesh;
class UDebugSkelMeshComponent;
class FAdvancedPreviewScene;
class SSlider;
class SCheckBox;
class SExpandableArea;
class SEditableTextBox;
template<typename OptionType> class SComboBox;

// ============================================================================
// ENUMS
// ============================================================================

/** Analysis mode for different insights */
UENUM()
enum class EAnalysisMode : uint8
{
	ContactPoints,		// Focus on contact detection
	Trajectories,		// Bone movement paths
	Timing,				// Sync and timing analysis
	Optimization		// Auto-optimization tools
};

/** Visualization layer toggles */
UENUM()
enum class EVisualizationLayer : uint32
{
	None				= 0,
	Skeletons			= 1 << 0,
	BoneNames			= 1 << 1,
	VelocityVectors		= 1 << 2,
	ContactPoints		= 1 << 3,
	ContactTrails		= 1 << 4,
	WeaponTrace			= 1 << 5,
	RootMotionPath		= 1 << 6,
	DistanceLines		= 1 << 7,
	ReachEnvelopes		= 1 << 8,
	CenterOfMass		= 1 << 9,
	CollisionBounds		= 1 << 10,
	MultiContact		= 1 << 11,  // Multi-contact point visualization
	All					= 0xFFFF
};
ENUM_CLASS_FLAGS(EVisualizationLayer);

/** Contact point types for multi-contact analysis */
UENUM()
enum class EContactPointType : uint8
{
	Head,
	LeftHand,
	RightHand,
	LeftFoot,
	RightFoot,
	Pelvis,
	COUNT
};

/**
 * Spatial relationship between attacker and victim.
 * Used to constrain optimization search space and provide context.
 */
UENUM()
enum class ESpatialRelationship : uint8
{
	Inferred,		// Auto-detect from animation (ContactNormal + victim bone)
	Facing,			// Victim faces attacker (front attack)
	Behind,			// Attacker behind victim (backstab)
	LeftSide,		// Attacker on victim's left
	RightSide,		// Attacker on victim's right
	Custom			// No constraints - full search space
};

/** Bone configuration for multi-contact tracking */
struct FMultiContactBoneConfig
{
	FName HeadBone = TEXT("head");
	FName LeftHandBone = TEXT("hand_l");
	FName RightHandBone = TEXT("hand_r");
	FName LeftFootBone = TEXT("foot_l");
	FName RightFootBone = TEXT("foot_r");
	FName PelvisBone = TEXT("pelvis");

	/** Get bone name for contact type */
	FName GetBoneForType(EContactPointType Type) const
	{
		switch (Type)
		{
			case EContactPointType::Head: return HeadBone;
			case EContactPointType::LeftHand: return LeftHandBone;
			case EContactPointType::RightHand: return RightHandBone;
			case EContactPointType::LeftFoot: return LeftFootBone;
			case EContactPointType::RightFoot: return RightFootBone;
			case EContactPointType::Pelvis: return PelvisBone;
			default: return NAME_None;
		}
	}

	/** Get display name for contact type */
	static FString GetTypeName(EContactPointType Type)
	{
		switch (Type)
		{
			case EContactPointType::Head: return TEXT("Head");
			case EContactPointType::LeftHand: return TEXT("L.Hand");
			case EContactPointType::RightHand: return TEXT("R.Hand");
			case EContactPointType::LeftFoot: return TEXT("L.Foot");
			case EContactPointType::RightFoot: return TEXT("R.Foot");
			case EContactPointType::Pelvis: return TEXT("Pelvis");
			default: return TEXT("Unknown");
		}
	}
};

/** Multi-contact analysis result for a single frame */
struct FMultiContactAnalysis
{
	float Time = 0.0f;

	// Per contact point positions and quality
	TMap<EContactPointType, FVector> AttackerContactPositions;
	TMap<EContactPointType, FVector> VictimContactPositions;

	// Pairwise distances between contact points
	// Key: (AttackerType, VictimType), Value: distance
	TMap<TPair<EContactPointType, EContactPointType>, float> PairwiseDistances;

	// Penetration detection (distance < threshold)
	TArray<TPair<EContactPointType, EContactPointType>> PenetrationPairs;
	float MaxPenetrationDepth = 0.0f;
	EContactPointType MostPenetratingAttacker = EContactPointType::RightHand;
	EContactPointType MostPenetratingVictim = EContactPointType::Head;

	// Contact quality per type (only if within threshold)
	TMap<EContactPointType, float> AttackerContactQualities;
	TMap<EContactPointType, float> VictimContactQualities;

	// Best contact pair found this frame
	EContactPointType BestAttackerContact = EContactPointType::RightHand;
	EContactPointType BestVictimContact = EContactPointType::Head;
	float BestContactDistance = FLT_MAX;
	float BestContactQuality = 0.0f;

	// Overall metrics
	int32 TotalActiveContacts = 0;
	float WeightedContactQuality = 0.0f;  // Quality weighted by contact type importance
	bool bHasPenetration = false;
};

// ============================================================================
// DATA STRUCTURES
// ============================================================================

/**
 * Configuration for a single character in the preview
 */
struct FCharacterPreviewConfig
{
	// Position offset from origin (or from partner if locked)
	FVector PositionOffset = FVector::ZeroVector;

	// Rotation offset (yaw primarily)
	FRotator RotationOffset = FRotator::ZeroRotator;

	// Scale multiplier
	float Scale = 1.0f;

	// Visualization color
	FLinearColor Color = FLinearColor::White;

	// Socket names for weapon (attacker only)
	FName WeaponStartSocket = TEXT("WeaponStart");
	FName WeaponEndSocket = TEXT("WeaponEnd");

	// Additional tracked bones
	TArray<FName> TrackedBones;
};

/**
 * Procedural contact point with full analysis data
 */
struct FProceduralContactPoint
{
	// Contact location in world space
	FVector WorldLocation = FVector::ZeroVector;

	// Normal direction (victim surface normal at contact)
	FVector ContactNormal = FVector::UpVector;

	// Impact direction (attacker approach vector)
	FVector ImpactDirection = FVector::ForwardVector;

	// Distance between attacker effector and victim surface
	float Distance = 0.0f;

	// Confidence score (0-1) based on approach velocity and alignment
	float Confidence = 0.0f;

	// Velocity of attacker bone at this point
	FVector AttackerVelocity = FVector::ZeroVector;

	// Speed magnitude
	float ImpactSpeed = 0.0f;

	// Bones/sockets involved
	FName AttackerBone = NAME_None;
	FName VictimBone = NAME_None;

	// Timing data
	float ContactTime = -1.0f;
	bool bIsActiveContact = false;
	bool bIsPredictedContact = false;  // Future contact based on trajectory

	// Quality metrics
	float AngleQuality = 0.0f;      // How perpendicular is the impact
	float PositionQuality = 0.0f;   // How centered on victim
};

/**
 * Bone trajectory data over time
 */
struct FBoneTrajectory
{
	FName BoneName;
	TArray<FVector> Positions;       // World positions per sample
	TArray<FVector> Velocities;      // Velocities per sample
	TArray<float> Speeds;            // Speed magnitudes
	float MaxSpeed = 0.0f;
	float MaxSpeedTime = 0.0f;
	int32 MaxSpeedSampleIndex = 0;
	FLinearColor TrajectoryColor = FLinearColor::White;
};

/**
 * Root motion analysis data
 */
struct FRootMotionAnalysis
{
	// Accumulated root motion translation
	FVector TotalTranslation = FVector::ZeroVector;

	// Accumulated root motion rotation
	FRotator TotalRotation = FRotator::ZeroRotator;

	// Per-frame deltas
	TArray<FVector> TranslationDeltas;
	TArray<FRotator> RotationDeltas;

	// Speed profile
	TArray<float> Speeds;
	float MaxSpeed = 0.0f;
	float AverageSpeed = 0.0f;
};

/**
 * Distance analysis between characters
 */
struct FDistanceAnalysis
{
	// Character center distances over time
	TArray<float> CenterDistances;

	// Closest bone pair distances over time
	TArray<float> ClosestBoneDistances;
	TArray<TPair<FName, FName>> ClosestBonePairs;

	// Statistics
	float MinDistance = FLT_MAX;
	float MaxDistance = 0.0f;
	float MinDistanceTime = 0.0f;
	float MaxDistanceTime = 0.0f;

	// Optimal distance (where contact confidence is highest)
	float OptimalStartDistance = 0.0f;
	float OptimalDistanceConfidence = 0.0f;
};

/**
 * Timing analysis for sync points
 */
struct FTimingAnalysis
{
	// Detected natural sync points (where contact is best)
	TArray<float> NaturalSyncTimes;
	TArray<float> SyncConfidences;

	// Best sync point
	float BestSyncTime = 0.0f;
	float BestSyncConfidence = 0.0f;

	// Phase detection
	TArray<TPair<float, float>> HighActivityRanges;  // Start, End times of high movement
};

/**
 * Per-frame trajectory sample for holistic analysis
 */
struct FTrajectoryFrameSample
{
	float Time = 0.0f;

	// Activity metrics (velocity-based)
	float AttackerVelocityMagnitude = 0.0f;
	float VictimVelocityMagnitude = 0.0f;
	float CombinedActivity = 0.0f;  // Normalized 0-1

	// Contact metrics
	float ClosestDistance = FLT_MAX;
	float ContactQuality = 0.0f;
	float AngleQuality = 0.0f;

	// Weight for optimization (higher during high-activity phases)
	float OptimizationWeight = 1.0f;

	// Approach trajectory
	FVector ApproachDirection = FVector::ForwardVector;
	float ApproachSpeed = 0.0f;

	// Phase detection
	bool bIsHighActivityPhase = false;
	bool bIsContactPhase = false;
	bool bIsPeakVelocityFrame = false;
};

/**
 * Holistic timeline analysis for intelligent optimization
 */
struct FHolisticTimelineAnalysis
{
	TArray<FTrajectoryFrameSample> FrameSamples;

	// Peak detection
	float PeakVelocityTime = 0.0f;
	float PeakVelocityMagnitude = 0.0f;

	// Phase boundaries
	float HighActivityStartTime = 0.0f;
	float HighActivityEndTime = 0.0f;
	float ContactPhaseStartTime = 0.0f;
	float ContactPhaseEndTime = 0.0f;

	// Optimization weights (pre-computed)
	float TotalWeight = 0.0f;

	// Statistics
	float AverageActivity = 0.0f;
	float AverageContactQuality = 0.0f;
	int32 HighActivityFrameCount = 0;
	int32 ContactPhaseFrameCount = 0;

	// Quality metrics (weighted by activity)
	float WeightedContactScore = 0.0f;
	float WeightedAlignmentScore = 0.0f;
	float WeightedOverallScore = 0.0f;
};

/**
 * Optimization result from auto-analysis
 */
struct FOptimizationResult
{
	bool bSuccess = false;

	// Recommended settings
	float RecommendedDistance = 150.0f;
	FRotator RecommendedAttackerRotation = FRotator::ZeroRotator;
	FRotator RecommendedVictimRotation = FRotator(0.0f, 180.0f, 0.0f);
	float RecommendedSyncTime = 0.0f;

	// Quality scores
	float ContactQuality = 0.0f;      // 0-1, how good the contact is
	float AlignmentQuality = 0.0f;    // 0-1, how well aligned
	float TimingQuality = 0.0f;       // 0-1, how good the sync timing
	float OverallScore = 0.0f;        // Combined score

	// Warnings/suggestions
	TArray<FString> Warnings;
	TArray<FString> Suggestions;
};

/**
 * Complete per-frame analysis
 */
struct FPairedFrameAnalysis
{
	float Time = 0.0f;

	// Character Positioning
	FVector AttackerLocation = FVector::ZeroVector;
	FVector VictimLocation = FVector::ZeroVector;
	FRotator AttackerRotation = FRotator::ZeroRotator;
	FRotator VictimRotation = FRotator::ZeroRotator;
	float CharacterDistance = 0.0f;
	float FacingAngle = 0.0f;

	// Root Motion
	FVector AttackerRootMotionDelta = FVector::ZeroVector;
	FVector VictimRootMotionDelta = FVector::ZeroVector;
	float AttackerRootMotionSpeed = 0.0f;
	float VictimRootMotionSpeed = 0.0f;

	// Procedural Contact Points
	TArray<FProceduralContactPoint> ContactPoints;
	FProceduralContactPoint PrimaryContact;

	// Skeleton Analysis
	float ClosestBoneDistance = FLT_MAX;
	FName AttackerClosestBone = NAME_None;
	FName VictimClosestBone = NAME_None;

	// Weapon state
	FVector WeaponStartPos = FVector::ZeroVector;
	FVector WeaponEndPos = FVector::ZeroVector;
	FVector WeaponVelocity = FVector::ZeroVector;
	float WeaponSpeed = 0.0f;

	// Center of mass
	FVector AttackerCOM = FVector::ZeroVector;
	FVector VictimCOM = FVector::ZeroVector;

	// Active Notifies
	TArray<FString> AttackerActiveNotifies;
	TArray<FString> VictimActiveNotifies;
};

/**
 * Result of inferring spatial relationship from animation data
 */
struct FSpatialRelationshipInference
{
	ESpatialRelationship InferredRelationship = ESpatialRelationship::Facing;
	float Confidence = 0.0f;  // 0-1 confidence in the inference

	// Evidence that led to this inference
	FVector PrimaryContactNormal = FVector::ForwardVector;  // Contact normal at sync point
	FName VictimContactBone = NAME_None;  // Which bone receives the contact
	float VictimFacingAngle = 0.0f;  // Angle between victim forward and attacker position

	// Reasoning (for display)
	FString ReasoningText;
};

/**
 * Rotation constraints based on spatial relationship
 */
struct FSpatialRotationConstraint
{
	float TargetYaw = 0.0f;  // Target victim rotation relative to attacker forward
	float Tolerance = 30.0f;  // Degrees of allowed deviation
	bool bConstrained = false;  // Whether this relationship constrains the search

	// Returns true if a rotation is within the constraint
	bool IsWithinConstraint(float VictimYaw) const
	{
		if (!bConstrained) return true;
		float Diff = FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, VictimYaw));
		return Diff <= Tolerance;
	}
};

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
	// SHARED PREVIEW SCENE
	// ========================================================================

	TSharedPtr<FAdvancedPreviewScene> SharedPreviewScene;
	TSharedPtr<SWidget> ViewportWidget;

	UDebugSkelMeshComponent* AttackerMeshComponent = nullptr;
	UDebugSkelMeshComponent* VictimMeshComponent = nullptr;

	void SetupSharedPreviewScene();
	void UpdateAttackerMesh(USkeletalMesh* Mesh);
	void UpdateVictimMesh(USkeletalMesh* Mesh);
	void UpdateCharacterPositions();
	void UpdateAnimations(float Time);

	// ========================================================================
	// CHARACTER CONFIGURATION
	// ========================================================================

	FCharacterPreviewConfig AttackerConfig;
	FCharacterPreviewConfig VictimConfig;

	// Lock victim position relative to attacker
	bool bLockVictimToAttacker = true;
	float LockedDistance = 150.0f;

	void ApplyCharacterConfigs();
	void OnAttackerPositionChanged(FVector NewPosition);
	void OnVictimPositionChanged(FVector NewPosition);
	void OnAttackerRotationChanged(FRotator NewRotation);
	void OnVictimRotationChanged(FRotator NewRotation);
	void OnLockedDistanceChanged(float NewDistance);

	// ========================================================================
	// ANIMATION DATA
	// ========================================================================

	TWeakObjectPtr<UAnimMontage> AttackerMontage;
	TWeakObjectPtr<UAnimMontage> VictimMontage;
	TWeakObjectPtr<USkeletalMesh> AttackerSkeleton;
	TWeakObjectPtr<USkeletalMesh> VictimSkeleton;

	// Montage section selection (NAME_None = entire montage)
	FName AttackerMontageSection = NAME_None;
	FName VictimMontageSection = NAME_None;
	TArray<TSharedPtr<FName>> AttackerSectionOptions;
	TArray<TSharedPtr<FName>> VictimSectionOptions;

	void RefreshAttackerSectionOptions();
	void RefreshVictimSectionOptions();
	void OnAttackerSectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectType);
	void OnVictimSectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectType);
	float GetSectionStartTime(UAnimMontage* Montage, FName SectionName) const;
	float GetSectionDuration(UAnimMontage* Montage, FName SectionName) const;
	void GetSectionTimeRange(UAnimMontage* Montage, FName SectionName, float& OutStart, float& OutEnd) const;

	// Playback
	float CurrentTime = 0.0f;
	float MaxDuration = 0.0f;
	bool bIsPlaying = false;
	float PlaybackSpeed = 1.0f;
	bool bLoopPlayback = true;
	bool bPingPongPlayback = false;
	int32 PingPongDirection = 1;

	// Victim timing offset (positive = victim starts later)
	float VictimTimeOffset = 0.0f;

	void RecalculateMaxDuration();
	float GetAttackerTime() const { return CurrentTime; }
	float GetVictimTime() const { return FMath::Max(0.0f, CurrentTime - VictimTimeOffset); }

	// ========================================================================
	// SPATIAL RELATIONSHIP (PT-2)
	// ========================================================================

	// User-selected spatial relationship (Inferred = auto-detect)
	ESpatialRelationship CurrentSpatialRelationship = ESpatialRelationship::Inferred;

	// Cached inference result (updated when montages change or on demand)
	FSpatialRelationshipInference InferredRelationship;
	bool bSpatialInferenceCacheDirty = true;

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
	// PROCEDURAL ANALYSIS ENGINE
	// ========================================================================

	// Analysis settings
	float ContactThreshold = 50.0f;
	float ContactPredictionWindow = 0.2f;
	int32 AnalysisSampleRate = 60;
	int32 TrajectorySampleCount = 120;

	// Analysis cache
	TArray<FPairedFrameAnalysis> FrameAnalysisCache;
	TArray<FBoneTrajectory> AttackerTrajectories;
	TArray<FBoneTrajectory> VictimTrajectories;
	FRootMotionAnalysis AttackerRootMotion;
	FRootMotionAnalysis VictimRootMotion;
	FDistanceAnalysis DistanceAnalysis;
	FTimingAnalysis TimingAnalysis;
	FOptimizationResult LastOptimizationResult;
	FHolisticTimelineAnalysis HolisticAnalysis;

	// Cache validity
	bool bAnalysisCacheDirty = true;
	bool bHolisticCacheDirty = true;

	void RebuildAnalysisCache();
	void RebuildTrajectoryCache();
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
	// MULTI-CONTACT POINT ANALYSIS
	// ========================================================================

	// Bone configurations for multi-contact tracking
	FMultiContactBoneConfig AttackerBoneConfig;
	FMultiContactBoneConfig VictimBoneConfig;

	// Contact type importance weights for optimization scoring
	TMap<EContactPointType, float> ContactTypeWeights;
	void InitializeContactTypeWeights();

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
	float EvaluateConfiguration(float Distance, FRotator AttackerRot, FRotator VictimRot);

	void ApplyOptimizationResult(const FOptimizationResult& Result);
	void OnOptimizeClicked();
	void OnFindOptimalDistanceClicked();
	void OnFindOptimalRotationClicked();
	void OnFindOptimalSyncClicked();

	// ========================================================================
	// VISUALIZATION
	// ========================================================================

	EVisualizationLayer ActiveVisualizationLayers = EVisualizationLayer::ContactPoints |
													EVisualizationLayer::WeaponTrace |
													EVisualizationLayer::DistanceLines |
													EVisualizationLayer::MultiContact;

	// Colors
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
	// PRESETS
	// ========================================================================

	void ApplyPreset_Finisher();
	void ApplyPreset_Counter();
	void ApplyPreset_Parry();
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
