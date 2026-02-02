// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PairedAnimationPreviewConfig.h"
#include "PairedAnimationEditorTypes.generated.h"

class UDebugSkelMeshComponent;
class UAnimMontage;

// ============================================================================
// ENUMS
// ============================================================================

/** Analysis mode for different insights */
UENUM(BlueprintType)
enum class EAnalysisMode : uint8
{
	ContactPoints,		// Focus on contact detection
	Trajectories,		// Bone movement paths
	Timing,				// Sync and timing analysis
	Optimization		// Auto-optimization tools
};

/** Visualization layer toggles (internal editor use - not Blueprint exposed) */
UENUM(Meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
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
	MultiContact		= 1 << 11,
	AxisGrid			= 1 << 12,		// World axis reference grid with labels
	All					= 0xFFFF
};
ENUM_CLASS_FLAGS(EVisualizationLayer);

/** Contact point types for multi-contact analysis */
UENUM(BlueprintType)
enum class EContactPointType : uint8
{
	// Body contact points
	Head,
	LeftHand,
	RightHand,
	LeftFoot,
	RightFoot,
	Pelvis,

	// Weapon contact points (requires weapon mesh attached)
	WeaponTip,		// Tip/end of weapon (blade tip, spear point)
	WeaponMid,		// Middle of weapon (blade center)
	WeaponBase,		// Base of weapon (hilt, handle)

	COUNT,

	// Aliases for categorization
	BODY_START = Head,
	BODY_END = Pelvis,
	WEAPON_START = WeaponTip,
	WEAPON_END = WeaponBase
};

/**
 * Spatial relationship between attacker and victim.
 * Used to constrain optimization search space and provide context.
 */
UENUM(BlueprintType)
enum class ESpatialRelationship : uint8
{
	Inferred,		// Auto-detect from animation (ContactNormal + victim bone)
	Facing,			// Victim faces attacker (front attack)
	Behind,			// Attacker behind victim (backstab)
	LeftSide,		// Attacker on victim's left
	RightSide,		// Attacker on victim's right
	Custom			// No constraints - full search space
};

/**
 * PT-18: Target character for optimization operations.
 * Replaces boolean trap pattern (bOptimizeAttacker) with explicit enum.
 */
UENUM(BlueprintType)
enum class EOptimizationTarget : uint8
{
	Attacker,		// Optimize attacker rotation/position
	Victim			// Optimize victim rotation/position
};

// ============================================================================
// PREVIEW CONFIGURATION STRUCTS
// ============================================================================

/**
 * Bone configuration for multi-contact tracking.
 * Maps contact point types to skeleton bone names.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FMultiContactBoneConfig
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(EditAnywhere, Category = "Bones")
	FName HeadBone = TEXT("head");

	UPROPERTY(EditAnywhere, Category = "Bones")
	FName LeftHandBone = TEXT("hand_l");

	UPROPERTY(EditAnywhere, Category = "Bones")
	FName RightHandBone = TEXT("hand_r");

	UPROPERTY(EditAnywhere, Category = "Bones")
	FName LeftFootBone = TEXT("foot_l");

	UPROPERTY(EditAnywhere, Category = "Bones")
	FName RightFootBone = TEXT("foot_r");

	UPROPERTY(EditAnywhere, Category = "Bones")
	FName PelvisBone = TEXT("pelvis");

public:
	// === Getters ===
	FName GetHeadBone() const { return HeadBone; }
	FName GetLeftHandBone() const { return LeftHandBone; }
	FName GetRightHandBone() const { return RightHandBone; }
	FName GetLeftFootBone() const { return LeftFootBone; }
	FName GetRightFootBone() const { return RightFootBone; }
	FName GetPelvisBone() const { return PelvisBone; }

	// === Setters ===
	void SetHeadBone(FName InBone) { HeadBone = InBone; }
	void SetLeftHandBone(FName InBone) { LeftHandBone = InBone; }
	void SetRightHandBone(FName InBone) { RightHandBone = InBone; }
	void SetLeftFootBone(FName InBone) { LeftFootBone = InBone; }
	void SetRightFootBone(FName InBone) { RightFootBone = InBone; }
	void SetPelvisBone(FName InBone) { PelvisBone = InBone; }

	/** Get bone name for contact type (only valid for body contact types) */
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
			// Weapon types return NAME_None - use weapon mesh instead
			case EContactPointType::WeaponTip:
			case EContactPointType::WeaponMid:
			case EContactPointType::WeaponBase:
			default: return NAME_None;
		}
	}

	/** Check if this is a body contact type (uses skeleton bones) */
	static bool IsBodyContactType(EContactPointType Type)
	{
		return Type >= EContactPointType::Head && Type <= EContactPointType::Pelvis;
	}

	/** Check if this is a weapon contact type (uses weapon mesh) */
	static bool IsWeaponContactType(EContactPointType Type)
	{
		return Type >= EContactPointType::WeaponTip && Type <= EContactPointType::WeaponBase;
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
			case EContactPointType::WeaponTip: return TEXT("Wpn.Tip");
			case EContactPointType::WeaponMid: return TEXT("Wpn.Mid");
			case EContactPointType::WeaponBase: return TEXT("Wpn.Base");
			default: return TEXT("Unknown");
		}
	}

	// === Factory ===
	static FMultiContactBoneConfig CreateDefault() { return FMultiContactBoneConfig(); }

	static FMultiContactBoneConfig CreateWithBones(
		FName InHead, FName InLeftHand, FName InRightHand,
		FName InLeftFoot, FName InRightFoot, FName InPelvis)
	{
		FMultiContactBoneConfig Config;
		Config.HeadBone = InHead;
		Config.LeftHandBone = InLeftHand;
		Config.RightHandBone = InRightHand;
		Config.LeftFootBone = InLeftFoot;
		Config.RightFootBone = InRightFoot;
		Config.PelvisBone = InPelvis;
		return Config;
	}
};

/**
 * Configuration for weapon mesh attachment and contact detection.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FWeaponMeshConfig
{
	GENERATED_BODY()

public:
	/** The static mesh representing the weapon */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	TSoftObjectPtr<UStaticMesh> WeaponMesh;

	/** Socket on the skeleton to attach the weapon to (e.g., "hand_r_weapon") */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName AttachmentSocket = TEXT("hand_r");

	/** Relative transform offset from the attachment socket */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FTransform AttachmentOffset = FTransform::Identity;

	/** Socket name on the weapon mesh to use as the grip/attachment point. If NAME_None, uses mesh origin. */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName WeaponGripSocket = NAME_None;

	/** Socket name on the weapon mesh for the tip (blade end). If NAME_None, uses mesh bounds. */
	UPROPERTY(EditAnywhere, Category = "Weapon|Sockets")
	FName WeaponTipSocket = NAME_None;

	/** Socket name on the weapon mesh for the middle (blade center). Optional - if only Tip and Base are set, they define weapon length. */
	UPROPERTY(EditAnywhere, Category = "Weapon|Sockets")
	FName WeaponMidSocket = NAME_None;

	/** Socket name on the weapon mesh for the base (hilt). If NAME_None, uses mesh bounds. */
	UPROPERTY(EditAnywhere, Category = "Weapon|Sockets")
	FName WeaponBaseSocket = NAME_None;

	/** Whether to use this weapon mesh for contact detection */
	UPROPERTY(EditAnywhere, Category = "Weapon")
	bool bUseForContactDetection = true;

	/** Whether this weapon config is valid (has a mesh assigned) */
	UPROPERTY()
	bool bIsValid = false;

public:
	// === Getters ===
	UStaticMesh* GetWeaponMesh() const { return WeaponMesh.Get(); }
	FName GetAttachmentSocket() const { return AttachmentSocket; }
	const FTransform& GetAttachmentOffset() const { return AttachmentOffset; }
	FName GetWeaponGripSocket() const { return WeaponGripSocket; }
	FName GetWeaponTipSocket() const { return WeaponTipSocket; }
	FName GetWeaponMidSocket() const { return WeaponMidSocket; }
	FName GetWeaponBaseSocket() const { return WeaponBaseSocket; }
	bool UseForContactDetection() const { return bUseForContactDetection && bIsValid; }
	bool IsValid() const { return bIsValid; }

	/** Returns true if all 3 weapon sockets (Tip, Mid, Base) are set for full contact detection */
	bool HasAllWeaponSockets() const { return !WeaponTipSocket.IsNone() && !WeaponMidSocket.IsNone() && !WeaponBaseSocket.IsNone(); }

	/** Returns true if at least Tip and Base are set (defines weapon length) */
	bool HasWeaponLengthSockets() const { return !WeaponTipSocket.IsNone() && !WeaponBaseSocket.IsNone(); }

	/** Returns the number of valid weapon sockets configured (0-3) */
	int32 GetValidSocketCount() const
	{
		int32 Count = 0;
		if (!WeaponTipSocket.IsNone()) Count++;
		if (!WeaponMidSocket.IsNone()) Count++;
		if (!WeaponBaseSocket.IsNone()) Count++;
		return Count;
	}

	// === Setters ===
	void SetWeaponMesh(UStaticMesh* InMesh)
	{
		WeaponMesh = InMesh;
		bIsValid = (InMesh != nullptr);
	}
	void SetAttachmentSocket(FName InSocket) { AttachmentSocket = InSocket; }
	void SetAttachmentOffset(const FTransform& InOffset) { AttachmentOffset = InOffset; }
	void SetWeaponGripSocket(FName InSocket) { WeaponGripSocket = InSocket; }
	void SetWeaponTipSocket(FName InSocket) { WeaponTipSocket = InSocket; }
	void SetWeaponMidSocket(FName InSocket) { WeaponMidSocket = InSocket; }
	void SetWeaponBaseSocket(FName InSocket) { WeaponBaseSocket = InSocket; }
	void SetUseForContactDetection(bool bUse) { bUseForContactDetection = bUse; }

	// === Factory ===
	static FWeaponMeshConfig CreateDefault() { return FWeaponMeshConfig(); }

	static FWeaponMeshConfig CreateWithMesh(UStaticMesh* InMesh, FName InSocket = TEXT("hand_r"))
	{
		FWeaponMeshConfig Config;
		Config.SetWeaponMesh(InMesh);
		Config.AttachmentSocket = InSocket;
		return Config;
	}
};

/**
 * Configuration for a single character in the preview.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FCharacterPreviewConfig
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(EditAnywhere, Category = "Transform")
	FVector PositionOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Transform")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, Category = "Transform")
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Visualization")
	FLinearColor Color = FLinearColor::White;

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName WeaponStartSocket = TEXT("WeaponStart");

	UPROPERTY(EditAnywhere, Category = "Weapon")
	FName WeaponEndSocket = TEXT("WeaponEnd");

	UPROPERTY(EditAnywhere, Category = "Analysis")
	TArray<FName> TrackedBones;

public:
	// === Getters ===
	FVector GetPositionOffset() const { return PositionOffset; }
	FRotator GetRotationOffset() const { return RotationOffset; }
	float GetScale() const { return Scale; }
	FLinearColor GetColor() const { return Color; }
	FName GetWeaponStartSocket() const { return WeaponStartSocket; }
	FName GetWeaponEndSocket() const { return WeaponEndSocket; }
	const TArray<FName>& GetTrackedBones() const { return TrackedBones; }

	// === Setters ===
	void SetPositionOffset(const FVector& InOffset) { PositionOffset = InOffset; }
	void SetRotationOffset(const FRotator& InRotation) { RotationOffset = InRotation; }
	void SetScale(float InScale) { Scale = InScale; }
	void SetColor(const FLinearColor& InColor) { Color = InColor; }
	void SetWeaponStartSocket(FName InSocket) { WeaponStartSocket = InSocket; }
	void SetWeaponEndSocket(FName InSocket) { WeaponEndSocket = InSocket; }
	TArray<FName>& GetTrackedBonesMutable() { return TrackedBones; }
	void SetTrackedBones(const TArray<FName>& InBones) { TrackedBones = InBones; }

	// === Factory ===
	static FCharacterPreviewConfig CreateDefault() { return FCharacterPreviewConfig(); }

	static FCharacterPreviewConfig CreateAttacker(const FLinearColor& InColor = FLinearColor(0.2f, 0.6f, 1.0f))
	{
		FCharacterPreviewConfig Config;
		Config.Color = InColor;
		return Config;
	}

	static FCharacterPreviewConfig CreateVictim(const FLinearColor& InColor = FLinearColor(1.0f, 0.4f, 0.2f))
	{
		FCharacterPreviewConfig Config;
		Config.Color = InColor;
		Config.RotationOffset = FRotator(0.0f, 180.0f, 0.0f);
		return Config;
	}
};

// ============================================================================
// CONTACT POINT STRUCTS
// ============================================================================

/**
 * Procedural contact point with full analysis data.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FProceduralContactPoint
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector ContactNormal = FVector::UpVector;

	UPROPERTY()
	FVector ImpactDirection = FVector::ForwardVector;

	UPROPERTY()
	float Distance = 0.0f;

	UPROPERTY()
	float Confidence = 0.0f;

	UPROPERTY()
	FVector AttackerVelocity = FVector::ZeroVector;

	UPROPERTY()
	float ImpactSpeed = 0.0f;

	UPROPERTY()
	FName AttackerBone = NAME_None;

	UPROPERTY()
	FName VictimBone = NAME_None;

	UPROPERTY()
	float ContactTime = -1.0f;

	UPROPERTY()
	bool bIsActiveContact = false;

	UPROPERTY()
	bool bIsPredictedContact = false;

	UPROPERTY()
	float AngleQuality = 0.0f;

	UPROPERTY()
	float PositionQuality = 0.0f;

public:
	// === Getters ===
	FVector GetWorldLocation() const { return WorldLocation; }
	FVector GetContactNormal() const { return ContactNormal; }
	FVector GetImpactDirection() const { return ImpactDirection; }
	float GetDistance() const { return Distance; }
	float GetConfidence() const { return Confidence; }
	FVector GetAttackerVelocity() const { return AttackerVelocity; }
	float GetImpactSpeed() const { return ImpactSpeed; }
	FName GetAttackerBone() const { return AttackerBone; }
	FName GetVictimBone() const { return VictimBone; }
	float GetContactTime() const { return ContactTime; }
	bool IsActiveContact() const { return bIsActiveContact; }
	bool IsPredictedContact() const { return bIsPredictedContact; }
	float GetAngleQuality() const { return AngleQuality; }
	float GetPositionQuality() const { return PositionQuality; }

	// === Setters ===
	void SetWorldLocation(const FVector& InLocation) { WorldLocation = InLocation; }
	void SetContactNormal(const FVector& InNormal) { ContactNormal = InNormal; }
	void SetImpactDirection(const FVector& InDirection) { ImpactDirection = InDirection; }
	void SetDistance(float InDistance) { Distance = InDistance; }
	void SetConfidence(float InConfidence) { Confidence = InConfidence; }
	void SetAttackerVelocity(const FVector& InVelocity) { AttackerVelocity = InVelocity; }
	void SetImpactSpeed(float InSpeed) { ImpactSpeed = InSpeed; }
	void SetAttackerBone(FName InBone) { AttackerBone = InBone; }
	void SetVictimBone(FName InBone) { VictimBone = InBone; }
	void SetContactTime(float InTime) { ContactTime = InTime; }
	void SetIsActiveContact(bool bInActive) { bIsActiveContact = bInActive; }
	void SetIsPredictedContact(bool bInPredicted) { bIsPredictedContact = bInPredicted; }
	void SetAngleQuality(float InQuality) { AngleQuality = InQuality; }
	void SetPositionQuality(float InQuality) { PositionQuality = InQuality; }

	// === Factory ===
	static FProceduralContactPoint CreateDefault() { return FProceduralContactPoint(); }

	static FProceduralContactPoint CreateAtLocation(const FVector& InLocation, FName InAttackerBone, FName InVictimBone)
	{
		FProceduralContactPoint Point;
		Point.WorldLocation = InLocation;
		Point.AttackerBone = InAttackerBone;
		Point.VictimBone = InVictimBone;
		return Point;
	}
};

/**
 * Multi-contact analysis result for a single frame.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FMultiContactAnalysis
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	float Time = 0.0f;

	TMap<EContactPointType, FVector> AttackerContactPositions;
	TMap<EContactPointType, FVector> VictimContactPositions;
	TMap<TPair<EContactPointType, EContactPointType>, float> PairwiseDistances;
	TArray<TPair<EContactPointType, EContactPointType>> PenetrationPairs;

	UPROPERTY()
	float MaxPenetrationDepth = 0.0f;

	UPROPERTY()
	EContactPointType MostPenetratingAttacker = EContactPointType::RightHand;

	UPROPERTY()
	EContactPointType MostPenetratingVictim = EContactPointType::Head;

	TMap<EContactPointType, float> AttackerContactQualities;
	TMap<EContactPointType, float> VictimContactQualities;

	UPROPERTY()
	EContactPointType BestAttackerContact = EContactPointType::RightHand;

	UPROPERTY()
	EContactPointType BestVictimContact = EContactPointType::Head;

	UPROPERTY()
	float BestContactDistance = FLT_MAX;

	UPROPERTY()
	float BestContactQuality = 0.0f;

	UPROPERTY()
	int32 TotalActiveContacts = 0;

	UPROPERTY()
	float WeightedContactQuality = 0.0f;

	UPROPERTY()
	float OverallContactQuality = 0.0f;

	UPROPERTY()
	bool bHasPenetration = false;

public:
	// === Getters ===
	float GetTime() const { return Time; }
	const TMap<EContactPointType, FVector>& GetAttackerContactPositions() const { return AttackerContactPositions; }
	const TMap<EContactPointType, FVector>& GetVictimContactPositions() const { return VictimContactPositions; }
	const TMap<TPair<EContactPointType, EContactPointType>, float>& GetPairwiseDistances() const { return PairwiseDistances; }
	const TArray<TPair<EContactPointType, EContactPointType>>& GetPenetrationPairs() const { return PenetrationPairs; }
	float GetMaxPenetrationDepth() const { return MaxPenetrationDepth; }
	EContactPointType GetMostPenetratingAttacker() const { return MostPenetratingAttacker; }
	EContactPointType GetMostPenetratingVictim() const { return MostPenetratingVictim; }
	const TMap<EContactPointType, float>& GetAttackerContactQualities() const { return AttackerContactQualities; }
	const TMap<EContactPointType, float>& GetVictimContactQualities() const { return VictimContactQualities; }
	EContactPointType GetBestAttackerContact() const { return BestAttackerContact; }
	EContactPointType GetBestVictimContact() const { return BestVictimContact; }
	float GetBestContactDistance() const { return BestContactDistance; }
	float GetBestContactQuality() const { return BestContactQuality; }
	int32 GetTotalActiveContacts() const { return TotalActiveContacts; }
	float GetWeightedContactQuality() const { return WeightedContactQuality; }
	float GetOverallContactQuality() const { return OverallContactQuality; }
	bool HasPenetration() const { return bHasPenetration; }

	// === Setters ===
	void SetTime(float InTime) { Time = InTime; }
	TMap<EContactPointType, FVector>& GetAttackerContactPositionsMutable() { return AttackerContactPositions; }
	TMap<EContactPointType, FVector>& GetVictimContactPositionsMutable() { return VictimContactPositions; }
	TMap<TPair<EContactPointType, EContactPointType>, float>& GetPairwiseDistancesMutable() { return PairwiseDistances; }
	TArray<TPair<EContactPointType, EContactPointType>>& GetPenetrationPairsMutable() { return PenetrationPairs; }
	void SetMaxPenetrationDepth(float InDepth) { MaxPenetrationDepth = InDepth; }
	void SetMostPenetratingAttacker(EContactPointType InType) { MostPenetratingAttacker = InType; }
	void SetMostPenetratingVictim(EContactPointType InType) { MostPenetratingVictim = InType; }
	TMap<EContactPointType, float>& GetAttackerContactQualitiesMutable() { return AttackerContactQualities; }
	TMap<EContactPointType, float>& GetVictimContactQualitiesMutable() { return VictimContactQualities; }
	void SetBestAttackerContact(EContactPointType InType) { BestAttackerContact = InType; }
	void SetBestVictimContact(EContactPointType InType) { BestVictimContact = InType; }
	void SetBestContactDistance(float InDistance) { BestContactDistance = InDistance; }
	void SetBestContactQuality(float InQuality) { BestContactQuality = InQuality; }
	void SetTotalActiveContacts(int32 InCount) { TotalActiveContacts = InCount; }
	void SetWeightedContactQuality(float InQuality) { WeightedContactQuality = InQuality; }
	void SetOverallContactQuality(float InQuality) { OverallContactQuality = InQuality; }
	void SetHasPenetration(bool bInHasPenetration) { bHasPenetration = bInHasPenetration; }

	// === Factory ===
	static FMultiContactAnalysis CreateDefault() { return FMultiContactAnalysis(); }
	static FMultiContactAnalysis CreateAtTime(float InTime)
	{
		FMultiContactAnalysis Analysis;
		Analysis.Time = InTime;
		return Analysis;
	}
};

// ============================================================================
// TRAJECTORY & MOTION STRUCTS
// ============================================================================

/**
 * Bone trajectory data over time.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FBoneTrajectory
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	FName BoneName = NAME_None;

	TArray<FVector> Positions;
	TArray<FVector> Velocities;
	TArray<float> Speeds;

	UPROPERTY()
	float MaxSpeed = 0.0f;

	UPROPERTY()
	float MaxSpeedTime = 0.0f;

	UPROPERTY()
	int32 MaxSpeedSampleIndex = 0;

	UPROPERTY()
	FLinearColor TrajectoryColor = FLinearColor::White;

public:
	// === Getters ===
	FName GetBoneName() const { return BoneName; }
	const TArray<FVector>& GetPositions() const { return Positions; }
	const TArray<FVector>& GetVelocities() const { return Velocities; }
	const TArray<float>& GetSpeeds() const { return Speeds; }
	float GetMaxSpeed() const { return MaxSpeed; }
	float GetMaxSpeedTime() const { return MaxSpeedTime; }
	int32 GetMaxSpeedSampleIndex() const { return MaxSpeedSampleIndex; }
	FLinearColor GetTrajectoryColor() const { return TrajectoryColor; }

	// === Setters ===
	void SetBoneName(FName InName) { BoneName = InName; }
	TArray<FVector>& GetPositionsMutable() { return Positions; }
	TArray<FVector>& GetVelocitiesMutable() { return Velocities; }
	TArray<float>& GetSpeedsMutable() { return Speeds; }
	void SetMaxSpeed(float InSpeed) { MaxSpeed = InSpeed; }
	void SetMaxSpeedTime(float InTime) { MaxSpeedTime = InTime; }
	void SetMaxSpeedSampleIndex(int32 InIndex) { MaxSpeedSampleIndex = InIndex; }
	void SetTrajectoryColor(const FLinearColor& InColor) { TrajectoryColor = InColor; }

	// === Factory ===
	static FBoneTrajectory CreateDefault() { return FBoneTrajectory(); }
	static FBoneTrajectory CreateForBone(FName InBoneName, const FLinearColor& InColor = FLinearColor::White)
	{
		FBoneTrajectory Trajectory;
		Trajectory.BoneName = InBoneName;
		Trajectory.TrajectoryColor = InColor;
		return Trajectory;
	}
};

/**
 * Root motion analysis data.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FRootMotionAnalysis
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	FVector TotalTranslation = FVector::ZeroVector;

	UPROPERTY()
	FRotator TotalRotation = FRotator::ZeroRotator;

	TArray<FVector> TranslationDeltas;
	TArray<FRotator> RotationDeltas;
	TArray<float> Speeds;

	UPROPERTY()
	float MaxSpeed = 0.0f;

	UPROPERTY()
	float AverageSpeed = 0.0f;

public:
	// === Getters ===
	FVector GetTotalTranslation() const { return TotalTranslation; }
	FRotator GetTotalRotation() const { return TotalRotation; }
	const TArray<FVector>& GetTranslationDeltas() const { return TranslationDeltas; }
	const TArray<FRotator>& GetRotationDeltas() const { return RotationDeltas; }
	const TArray<float>& GetSpeeds() const { return Speeds; }
	float GetMaxSpeed() const { return MaxSpeed; }
	float GetAverageSpeed() const { return AverageSpeed; }

	// === Setters ===
	void SetTotalTranslation(const FVector& InTranslation) { TotalTranslation = InTranslation; }
	void SetTotalRotation(const FRotator& InRotation) { TotalRotation = InRotation; }
	TArray<FVector>& GetTranslationDeltasMutable() { return TranslationDeltas; }
	TArray<FRotator>& GetRotationDeltasMutable() { return RotationDeltas; }
	TArray<float>& GetSpeedsMutable() { return Speeds; }
	void SetMaxSpeed(float InSpeed) { MaxSpeed = InSpeed; }
	void SetAverageSpeed(float InSpeed) { AverageSpeed = InSpeed; }

	// === Factory ===
	static FRootMotionAnalysis CreateDefault() { return FRootMotionAnalysis(); }
};

/**
 * Per-frame trajectory sample for holistic analysis.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FTrajectoryFrameSample
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	float Time = 0.0f;

	UPROPERTY()
	float AttackerVelocityMagnitude = 0.0f;

	UPROPERTY()
	float VictimVelocityMagnitude = 0.0f;

	UPROPERTY()
	float CombinedActivity = 0.0f;

	UPROPERTY()
	float ClosestDistance = FLT_MAX;

	UPROPERTY()
	float ContactQuality = 0.0f;

	UPROPERTY()
	float AngleQuality = 0.0f;

	UPROPERTY()
	float OptimizationWeight = 1.0f;

	UPROPERTY()
	FVector ApproachDirection = FVector::ForwardVector;

	UPROPERTY()
	float ApproachSpeed = 0.0f;

	UPROPERTY()
	bool bIsHighActivityPhase = false;

	UPROPERTY()
	bool bIsContactPhase = false;

	UPROPERTY()
	bool bIsPeakVelocityFrame = false;

public:
	// === Getters ===
	float GetTime() const { return Time; }
	float GetAttackerVelocityMagnitude() const { return AttackerVelocityMagnitude; }
	float GetVictimVelocityMagnitude() const { return VictimVelocityMagnitude; }
	float GetCombinedActivity() const { return CombinedActivity; }
	float GetClosestDistance() const { return ClosestDistance; }
	float GetContactQuality() const { return ContactQuality; }
	float GetAngleQuality() const { return AngleQuality; }
	float GetOptimizationWeight() const { return OptimizationWeight; }
	FVector GetApproachDirection() const { return ApproachDirection; }
	float GetApproachSpeed() const { return ApproachSpeed; }
	bool IsHighActivityPhase() const { return bIsHighActivityPhase; }
	bool IsContactPhase() const { return bIsContactPhase; }
	bool IsPeakVelocityFrame() const { return bIsPeakVelocityFrame; }

	// === Setters ===
	void SetTime(float InTime) { Time = InTime; }
	void SetAttackerVelocityMagnitude(float InMagnitude) { AttackerVelocityMagnitude = InMagnitude; }
	void SetVictimVelocityMagnitude(float InMagnitude) { VictimVelocityMagnitude = InMagnitude; }
	void SetCombinedActivity(float InActivity) { CombinedActivity = InActivity; }
	void SetClosestDistance(float InDistance) { ClosestDistance = InDistance; }
	void SetContactQuality(float InQuality) { ContactQuality = InQuality; }
	void SetAngleQuality(float InQuality) { AngleQuality = InQuality; }
	void SetOptimizationWeight(float InWeight) { OptimizationWeight = InWeight; }
	void SetApproachDirection(const FVector& InDirection) { ApproachDirection = InDirection; }
	void SetApproachSpeed(float InSpeed) { ApproachSpeed = InSpeed; }
	void SetIsHighActivityPhase(bool bInActive) { bIsHighActivityPhase = bInActive; }
	void SetIsContactPhase(bool bInContact) { bIsContactPhase = bInContact; }
	void SetIsPeakVelocityFrame(bool bInPeak) { bIsPeakVelocityFrame = bInPeak; }

	// === Factory ===
	static FTrajectoryFrameSample CreateDefault() { return FTrajectoryFrameSample(); }
	static FTrajectoryFrameSample CreateAtTime(float InTime)
	{
		FTrajectoryFrameSample Sample;
		Sample.Time = InTime;
		return Sample;
	}
};

// ============================================================================
// ANALYSIS RESULT STRUCTS
// ============================================================================

/**
 * Distance analysis between characters.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FDistanceAnalysis
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	TArray<float> CenterDistances;
	TArray<float> ClosestBoneDistances;
	TArray<TPair<FName, FName>> ClosestBonePairs;

	UPROPERTY()
	float MinDistance = FLT_MAX;

	UPROPERTY()
	float MaxDistance = 0.0f;

	UPROPERTY()
	float MinDistanceTime = 0.0f;

	UPROPERTY()
	float MaxDistanceTime = 0.0f;

	UPROPERTY()
	float OptimalStartDistance = 0.0f;

	UPROPERTY()
	float OptimalDistanceConfidence = 0.0f;

public:
	// === Getters ===
	const TArray<float>& GetCenterDistances() const { return CenterDistances; }
	const TArray<float>& GetClosestBoneDistances() const { return ClosestBoneDistances; }
	const TArray<TPair<FName, FName>>& GetClosestBonePairs() const { return ClosestBonePairs; }
	float GetMinDistance() const { return MinDistance; }
	float GetMaxDistance() const { return MaxDistance; }
	float GetMinDistanceTime() const { return MinDistanceTime; }
	float GetMaxDistanceTime() const { return MaxDistanceTime; }
	float GetOptimalStartDistance() const { return OptimalStartDistance; }
	float GetOptimalDistanceConfidence() const { return OptimalDistanceConfidence; }

	// === Setters ===
	TArray<float>& GetCenterDistancesMutable() { return CenterDistances; }
	TArray<float>& GetClosestBoneDistancesMutable() { return ClosestBoneDistances; }
	TArray<TPair<FName, FName>>& GetClosestBonePairsMutable() { return ClosestBonePairs; }
	void SetMinDistance(float InDistance) { MinDistance = InDistance; }
	void SetMaxDistance(float InDistance) { MaxDistance = InDistance; }
	void SetMinDistanceTime(float InTime) { MinDistanceTime = InTime; }
	void SetMaxDistanceTime(float InTime) { MaxDistanceTime = InTime; }
	void SetOptimalStartDistance(float InDistance) { OptimalStartDistance = InDistance; }
	void SetOptimalDistanceConfidence(float InConfidence) { OptimalDistanceConfidence = InConfidence; }

	// === Factory ===
	static FDistanceAnalysis CreateDefault() { return FDistanceAnalysis(); }
};

/**
 * Timing analysis for sync points.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FTimingAnalysis
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	TArray<float> NaturalSyncTimes;
	TArray<float> SyncConfidences;

	UPROPERTY()
	float BestSyncTime = 0.0f;

	UPROPERTY()
	float BestSyncConfidence = 0.0f;

	TArray<TPair<float, float>> HighActivityRanges;

public:
	// === Getters ===
	const TArray<float>& GetNaturalSyncTimes() const { return NaturalSyncTimes; }
	const TArray<float>& GetSyncConfidences() const { return SyncConfidences; }
	float GetBestSyncTime() const { return BestSyncTime; }
	float GetBestSyncConfidence() const { return BestSyncConfidence; }
	const TArray<TPair<float, float>>& GetHighActivityRanges() const { return HighActivityRanges; }

	// === Setters ===
	TArray<float>& GetNaturalSyncTimesMutable() { return NaturalSyncTimes; }
	TArray<float>& GetSyncConfidencesMutable() { return SyncConfidences; }
	void SetBestSyncTime(float InTime) { BestSyncTime = InTime; }
	void SetBestSyncConfidence(float InConfidence) { BestSyncConfidence = InConfidence; }
	TArray<TPair<float, float>>& GetHighActivityRangesMutable() { return HighActivityRanges; }

	// === Factory ===
	static FTimingAnalysis CreateDefault() { return FTimingAnalysis(); }
};

/**
 * Holistic timeline analysis for intelligent optimization.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FHolisticTimelineAnalysis
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	TArray<FTrajectoryFrameSample> FrameSamples;

	UPROPERTY()
	float PeakVelocityTime = 0.0f;

	UPROPERTY()
	float PeakVelocityMagnitude = 0.0f;

	UPROPERTY()
	float HighActivityStartTime = 0.0f;

	UPROPERTY()
	float HighActivityEndTime = 0.0f;

	UPROPERTY()
	float ContactPhaseStartTime = 0.0f;

	UPROPERTY()
	float ContactPhaseEndTime = 0.0f;

	UPROPERTY()
	float TotalWeight = 0.0f;

	UPROPERTY()
	float AverageActivity = 0.0f;

	UPROPERTY()
	float AverageContactQuality = 0.0f;

	UPROPERTY()
	int32 HighActivityFrameCount = 0;

	UPROPERTY()
	int32 ContactPhaseFrameCount = 0;

	UPROPERTY()
	float WeightedContactScore = 0.0f;

	UPROPERTY()
	float WeightedAlignmentScore = 0.0f;

	UPROPERTY()
	float WeightedOverallScore = 0.0f;

public:
	// === Getters ===
	const TArray<FTrajectoryFrameSample>& GetFrameSamples() const { return FrameSamples; }
	float GetPeakVelocityTime() const { return PeakVelocityTime; }
	float GetPeakVelocityMagnitude() const { return PeakVelocityMagnitude; }
	float GetHighActivityStartTime() const { return HighActivityStartTime; }
	float GetHighActivityEndTime() const { return HighActivityEndTime; }
	float GetContactPhaseStartTime() const { return ContactPhaseStartTime; }
	float GetContactPhaseEndTime() const { return ContactPhaseEndTime; }
	float GetTotalWeight() const { return TotalWeight; }
	float GetAverageActivity() const { return AverageActivity; }
	float GetAverageContactQuality() const { return AverageContactQuality; }
	int32 GetHighActivityFrameCount() const { return HighActivityFrameCount; }
	int32 GetContactPhaseFrameCount() const { return ContactPhaseFrameCount; }
	float GetWeightedContactScore() const { return WeightedContactScore; }
	float GetWeightedAlignmentScore() const { return WeightedAlignmentScore; }
	float GetWeightedOverallScore() const { return WeightedOverallScore; }

	// === Setters ===
	TArray<FTrajectoryFrameSample>& GetFrameSamplesMutable() { return FrameSamples; }
	void SetPeakVelocityTime(float InTime) { PeakVelocityTime = InTime; }
	void SetPeakVelocityMagnitude(float InMagnitude) { PeakVelocityMagnitude = InMagnitude; }
	void SetHighActivityStartTime(float InTime) { HighActivityStartTime = InTime; }
	void SetHighActivityEndTime(float InTime) { HighActivityEndTime = InTime; }
	void SetContactPhaseStartTime(float InTime) { ContactPhaseStartTime = InTime; }
	void SetContactPhaseEndTime(float InTime) { ContactPhaseEndTime = InTime; }
	void SetTotalWeight(float InWeight) { TotalWeight = InWeight; }
	void SetAverageActivity(float InActivity) { AverageActivity = InActivity; }
	void SetAverageContactQuality(float InQuality) { AverageContactQuality = InQuality; }
	void SetHighActivityFrameCount(int32 InCount) { HighActivityFrameCount = InCount; }
	void SetContactPhaseFrameCount(int32 InCount) { ContactPhaseFrameCount = InCount; }
	void SetWeightedContactScore(float InScore) { WeightedContactScore = InScore; }
	void SetWeightedAlignmentScore(float InScore) { WeightedAlignmentScore = InScore; }
	void SetWeightedOverallScore(float InScore) { WeightedOverallScore = InScore; }

	// === Factory ===
	static FHolisticTimelineAnalysis CreateDefault() { return FHolisticTimelineAnalysis(); }
};

/**
 * Complete per-frame analysis.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FPairedFrameAnalysis
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	float Time = 0.0f;

	UPROPERTY()
	FVector AttackerLocation = FVector::ZeroVector;

	UPROPERTY()
	FVector VictimLocation = FVector::ZeroVector;

	UPROPERTY()
	FRotator AttackerRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FRotator VictimRotation = FRotator::ZeroRotator;

	UPROPERTY()
	float CharacterDistance = 0.0f;

	UPROPERTY()
	float FacingAngle = 0.0f;

	UPROPERTY()
	FVector AttackerRootMotionDelta = FVector::ZeroVector;

	UPROPERTY()
	FVector VictimRootMotionDelta = FVector::ZeroVector;

	UPROPERTY()
	float AttackerRootMotionSpeed = 0.0f;

	UPROPERTY()
	float VictimRootMotionSpeed = 0.0f;

	TArray<FProceduralContactPoint> ContactPoints;

	UPROPERTY()
	FProceduralContactPoint PrimaryContact;

	UPROPERTY()
	float ClosestBoneDistance = FLT_MAX;

	UPROPERTY()
	FName AttackerClosestBone = NAME_None;

	UPROPERTY()
	FName VictimClosestBone = NAME_None;

	UPROPERTY()
	FVector WeaponStartPos = FVector::ZeroVector;

	UPROPERTY()
	FVector WeaponEndPos = FVector::ZeroVector;

	UPROPERTY()
	FVector WeaponVelocity = FVector::ZeroVector;

	UPROPERTY()
	float WeaponSpeed = 0.0f;

	UPROPERTY()
	FVector AttackerCOM = FVector::ZeroVector;

	UPROPERTY()
	FVector VictimCOM = FVector::ZeroVector;

	TArray<FString> AttackerActiveNotifies;
	TArray<FString> VictimActiveNotifies;

public:
	// === Getters ===
	float GetTime() const { return Time; }
	FVector GetAttackerLocation() const { return AttackerLocation; }
	FVector GetVictimLocation() const { return VictimLocation; }
	FRotator GetAttackerRotation() const { return AttackerRotation; }
	FRotator GetVictimRotation() const { return VictimRotation; }
	float GetCharacterDistance() const { return CharacterDistance; }
	float GetFacingAngle() const { return FacingAngle; }
	FVector GetAttackerRootMotionDelta() const { return AttackerRootMotionDelta; }
	FVector GetVictimRootMotionDelta() const { return VictimRootMotionDelta; }
	float GetAttackerRootMotionSpeed() const { return AttackerRootMotionSpeed; }
	float GetVictimRootMotionSpeed() const { return VictimRootMotionSpeed; }
	const TArray<FProceduralContactPoint>& GetContactPoints() const { return ContactPoints; }
	const FProceduralContactPoint& GetPrimaryContact() const { return PrimaryContact; }
	float GetClosestBoneDistance() const { return ClosestBoneDistance; }
	FName GetAttackerClosestBone() const { return AttackerClosestBone; }
	FName GetVictimClosestBone() const { return VictimClosestBone; }
	FVector GetWeaponStartPos() const { return WeaponStartPos; }
	FVector GetWeaponEndPos() const { return WeaponEndPos; }
	FVector GetWeaponVelocity() const { return WeaponVelocity; }
	float GetWeaponSpeed() const { return WeaponSpeed; }
	FVector GetAttackerCOM() const { return AttackerCOM; }
	FVector GetVictimCOM() const { return VictimCOM; }
	const TArray<FString>& GetAttackerActiveNotifies() const { return AttackerActiveNotifies; }
	const TArray<FString>& GetVictimActiveNotifies() const { return VictimActiveNotifies; }

	// === Setters ===
	void SetTime(float InTime) { Time = InTime; }
	void SetAttackerLocation(const FVector& InLocation) { AttackerLocation = InLocation; }
	void SetVictimLocation(const FVector& InLocation) { VictimLocation = InLocation; }
	void SetAttackerRotation(const FRotator& InRotation) { AttackerRotation = InRotation; }
	void SetVictimRotation(const FRotator& InRotation) { VictimRotation = InRotation; }
	void SetCharacterDistance(float InDistance) { CharacterDistance = InDistance; }
	void SetFacingAngle(float InAngle) { FacingAngle = InAngle; }
	void SetAttackerRootMotionDelta(const FVector& InDelta) { AttackerRootMotionDelta = InDelta; }
	void SetVictimRootMotionDelta(const FVector& InDelta) { VictimRootMotionDelta = InDelta; }
	void SetAttackerRootMotionSpeed(float InSpeed) { AttackerRootMotionSpeed = InSpeed; }
	void SetVictimRootMotionSpeed(float InSpeed) { VictimRootMotionSpeed = InSpeed; }
	TArray<FProceduralContactPoint>& GetContactPointsMutable() { return ContactPoints; }
	void SetPrimaryContact(const FProceduralContactPoint& InContact) { PrimaryContact = InContact; }
	void SetClosestBoneDistance(float InDistance) { ClosestBoneDistance = InDistance; }
	void SetAttackerClosestBone(FName InBone) { AttackerClosestBone = InBone; }
	void SetVictimClosestBone(FName InBone) { VictimClosestBone = InBone; }
	void SetWeaponStartPos(const FVector& InPos) { WeaponStartPos = InPos; }
	void SetWeaponEndPos(const FVector& InPos) { WeaponEndPos = InPos; }
	void SetWeaponVelocity(const FVector& InVelocity) { WeaponVelocity = InVelocity; }
	void SetWeaponSpeed(float InSpeed) { WeaponSpeed = InSpeed; }
	void SetAttackerCOM(const FVector& InCOM) { AttackerCOM = InCOM; }
	void SetVictimCOM(const FVector& InCOM) { VictimCOM = InCOM; }
	TArray<FString>& GetAttackerActiveNotifiesMutable() { return AttackerActiveNotifies; }
	TArray<FString>& GetVictimActiveNotifiesMutable() { return VictimActiveNotifies; }

	// === Factory ===
	static FPairedFrameAnalysis CreateDefault() { return FPairedFrameAnalysis(); }
	static FPairedFrameAnalysis CreateAtTime(float InTime)
	{
		FPairedFrameAnalysis Analysis;
		Analysis.Time = InTime;
		return Analysis;
	}
};

// ============================================================================
// SPATIAL RELATIONSHIP STRUCTS
// ============================================================================

/**
 * Result of inferring spatial relationship from animation data.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FSpatialRelationshipInference
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	ESpatialRelationship InferredRelationship = ESpatialRelationship::Facing;

	UPROPERTY()
	float Confidence = 0.0f;

	UPROPERTY()
	FVector PrimaryContactNormal = FVector::ForwardVector;

	UPROPERTY()
	FName VictimContactBone = NAME_None;

	UPROPERTY()
	float VictimFacingAngle = 0.0f;

	UPROPERTY()
	FString ReasoningText;

public:
	// === Getters ===
	ESpatialRelationship GetInferredRelationship() const { return InferredRelationship; }
	float GetConfidence() const { return Confidence; }
	FVector GetPrimaryContactNormal() const { return PrimaryContactNormal; }
	FName GetVictimContactBone() const { return VictimContactBone; }
	float GetVictimFacingAngle() const { return VictimFacingAngle; }
	const FString& GetReasoningText() const { return ReasoningText; }

	// === Setters ===
	void SetInferredRelationship(ESpatialRelationship InRelationship) { InferredRelationship = InRelationship; }
	void SetConfidence(float InConfidence) { Confidence = InConfidence; }
	void SetPrimaryContactNormal(const FVector& InNormal) { PrimaryContactNormal = InNormal; }
	void SetVictimContactBone(FName InBone) { VictimContactBone = InBone; }
	void SetVictimFacingAngle(float InAngle) { VictimFacingAngle = InAngle; }
	void SetReasoningText(const FString& InText) { ReasoningText = InText; }

	// === Factory ===
	static FSpatialRelationshipInference CreateDefault() { return FSpatialRelationshipInference(); }
	static FSpatialRelationshipInference CreateWithRelationship(ESpatialRelationship InRelationship, float InConfidence, const FString& InReasoning = TEXT(""))
	{
		FSpatialRelationshipInference Inference;
		Inference.InferredRelationship = InRelationship;
		Inference.Confidence = InConfidence;
		Inference.ReasoningText = InReasoning;
		return Inference;
	}
};

/**
 * Rotation constraints based on spatial relationship.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FSpatialRotationConstraint
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	float TargetYaw = 0.0f;

	UPROPERTY()
	float Tolerance = 30.0f;

	UPROPERTY()
	bool bConstrained = false;

public:
	// === Getters ===
	float GetTargetYaw() const { return TargetYaw; }
	float GetTolerance() const { return Tolerance; }
	bool IsConstrained() const { return bConstrained; }

	// === Setters ===
	void SetTargetYaw(float InYaw) { TargetYaw = InYaw; }
	void SetTolerance(float InTolerance) { Tolerance = InTolerance; }
	void SetConstrained(bool bInConstrained) { bConstrained = bInConstrained; }

	/**
	 * Returns true if a rotation is within the constraint.
	 * Note: For standalone validation, prefer UPairedAnimationAnalysisLibrary::IsYawWithinConstraint()
	 */
	bool IsWithinConstraint(float VictimYaw) const
	{
		if (!bConstrained) return true;
		// Simple angle difference check - same logic as UPairedAnimationAnalysisLibrary::IsYawWithinConstraint
		float Diff = FMath::Abs(FMath::FindDeltaAngleDegrees(TargetYaw, VictimYaw));
		return Diff <= Tolerance;
	}

	// === Factory ===
	static FSpatialRotationConstraint CreateDefault() { return FSpatialRotationConstraint(); }

	static FSpatialRotationConstraint CreateUnconstrained()
	{
		FSpatialRotationConstraint Constraint;
		Constraint.bConstrained = false;
		return Constraint;
	}

	static FSpatialRotationConstraint CreateConstrained(float InTargetYaw, float InTolerance = 30.0f)
	{
		FSpatialRotationConstraint Constraint;
		Constraint.TargetYaw = InTargetYaw;
		Constraint.Tolerance = InTolerance;
		Constraint.bConstrained = true;
		return Constraint;
	}

	/**
	 * Create constraint based on spatial relationship.
	 * Note: These values MUST stay synchronized with UPairedAnimationAnalysisLibrary::GetRelationshipConstraints().
	 * Values are intentionally duplicated here to avoid circular include dependency.
	 */
	static FSpatialRotationConstraint CreateForRelationship(ESpatialRelationship Relationship)
	{
		// Constraint values: Facing=180°, Behind=0°, LeftSide=90°, RightSide=-90°, Tolerance=30°
		// These values match UPairedAnimationAnalysisLibrary::GetRelationshipConstraints()
		switch (Relationship)
		{
			case ESpatialRelationship::Facing:
				return CreateConstrained(180.0f, 30.0f);
			case ESpatialRelationship::Behind:
				return CreateConstrained(0.0f, 30.0f);
			case ESpatialRelationship::LeftSide:
				return CreateConstrained(90.0f, 30.0f);
			case ESpatialRelationship::RightSide:
				return CreateConstrained(-90.0f, 30.0f);
			case ESpatialRelationship::Custom:
			case ESpatialRelationship::Inferred:
			default:
				return CreateUnconstrained();
		}
	}
};

// ============================================================================
// OPTIMIZATION RESULT STRUCTS
// ============================================================================

/**
 * Optimization result from auto-analysis (legacy internal struct).
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FOptimizationResult
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	bool bSuccess = false;

	/** Set to true if the user cancelled the optimization operation (PT-10) */
	UPROPERTY()
	bool bWasCancelled = false;

	UPROPERTY()
	float RecommendedDistance = 150.0f;

	UPROPERTY()
	FRotator RecommendedAttackerRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FRotator RecommendedVictimRotation = FRotator(0.0f, 180.0f, 0.0f);

	UPROPERTY()
	float RecommendedSyncTime = 0.0f;

	UPROPERTY()
	float ContactQuality = 0.0f;

	UPROPERTY()
	float AlignmentQuality = 0.0f;

	UPROPERTY()
	float TimingQuality = 0.0f;

	UPROPERTY()
	float OverallScore = 0.0f;

	TArray<FString> Warnings;
	TArray<FString> Suggestions;

public:
	// === Getters ===
	bool IsSuccess() const { return bSuccess; }
	bool WasCancelled() const { return bWasCancelled; }
	float GetRecommendedDistance() const { return RecommendedDistance; }
	FRotator GetRecommendedAttackerRotation() const { return RecommendedAttackerRotation; }
	FRotator GetRecommendedVictimRotation() const { return RecommendedVictimRotation; }
	float GetRecommendedSyncTime() const { return RecommendedSyncTime; }
	float GetContactQuality() const { return ContactQuality; }
	float GetAlignmentQuality() const { return AlignmentQuality; }
	float GetTimingQuality() const { return TimingQuality; }
	float GetOverallScore() const { return OverallScore; }
	const TArray<FString>& GetWarnings() const { return Warnings; }
	const TArray<FString>& GetSuggestions() const { return Suggestions; }

	// === Setters ===
	void SetSuccess(bool bInSuccess) { bSuccess = bInSuccess; }
	void SetCancelled(bool bInCancelled) { bWasCancelled = bInCancelled; }
	void SetRecommendedDistance(float InDistance) { RecommendedDistance = InDistance; }
	void SetRecommendedAttackerRotation(const FRotator& InRotation) { RecommendedAttackerRotation = InRotation; }
	void SetRecommendedVictimRotation(const FRotator& InRotation) { RecommendedVictimRotation = InRotation; }
	void SetRecommendedSyncTime(float InTime) { RecommendedSyncTime = InTime; }
	void SetContactQuality(float InQuality) { ContactQuality = InQuality; }
	void SetAlignmentQuality(float InQuality) { AlignmentQuality = InQuality; }
	void SetTimingQuality(float InQuality) { TimingQuality = InQuality; }
	void SetOverallScore(float InScore) { OverallScore = InScore; }
	TArray<FString>& GetWarningsMutable() { return Warnings; }
	TArray<FString>& GetSuggestionsMutable() { return Suggestions; }

	// === Factory ===
	static FOptimizationResult CreateDefault() { return FOptimizationResult(); }
	static FOptimizationResult CreateSuccess(float InDistance, const FRotator& InAttackerRot, const FRotator& InVictimRot, float InScore)
	{
		FOptimizationResult Result;
		Result.bSuccess = true;
		Result.RecommendedDistance = InDistance;
		Result.RecommendedAttackerRotation = InAttackerRot;
		Result.RecommendedVictimRotation = InVictimRot;
		Result.OverallScore = InScore;
		return Result;
	}

	static FOptimizationResult CreateFailure(const FString& Reason)
	{
		FOptimizationResult Result;
		Result.bSuccess = false;
		Result.Warnings.Add(Reason);
		return Result;
	}
};

// ============================================================================
// ANALYSIS CONTEXT & LIBRARY RESULT STRUCTS
// ============================================================================

/**
 * Context struct for paired animation analysis.
 * Captures the state needed to perform analysis without coupling to UI.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FPairedAnimationAnalysisContext
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY()
	TWeakObjectPtr<UDebugSkelMeshComponent> AttackerMesh;

	UPROPERTY()
	TWeakObjectPtr<UDebugSkelMeshComponent> VictimMesh;

	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> AttackerMontage;

	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> VictimMontage;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
	float Distance = 150.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
	FRotator AttackerRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
	FRotator VictimRotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
	FVector AttackerPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Configuration", meta = (AllowPrivateAccess = "true"))
	FVector VictimPosition = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (AllowPrivateAccess = "true"))
	float VictimTimeOffset = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (AllowPrivateAccess = "true"))
	float MaxDuration = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (AllowPrivateAccess = "true"))
	float ContactThreshold = 50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thresholds", meta = (AllowPrivateAccess = "true"))
	float PenetrationThreshold = 15.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bones", meta = (AllowPrivateAccess = "true"))
	FName AttackerWeaponStartSocket = TEXT("WeaponStart");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bones", meta = (AllowPrivateAccess = "true"))
	FName AttackerWeaponEndSocket = TEXT("WeaponEnd");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bones", meta = (AllowPrivateAccess = "true"))
	FName AttackerHandBone = TEXT("hand_r");

public:
	// === Getters ===
	UDebugSkelMeshComponent* GetAttackerMesh() const { return AttackerMesh.Get(); }
	UDebugSkelMeshComponent* GetVictimMesh() const { return VictimMesh.Get(); }
	UAnimMontage* GetAttackerMontage() const { return AttackerMontage.Get(); }
	UAnimMontage* GetVictimMontage() const { return VictimMontage.Get(); }
	float GetDistance() const { return Distance; }
	FRotator GetAttackerRotation() const { return AttackerRotation; }
	FRotator GetVictimRotation() const { return VictimRotation; }
	FVector GetAttackerPosition() const { return AttackerPosition; }
	FVector GetVictimPosition() const { return VictimPosition; }
	float GetVictimTimeOffset() const { return VictimTimeOffset; }
	float GetMaxDuration() const { return MaxDuration; }
	float GetContactThreshold() const { return ContactThreshold; }
	float GetPenetrationThreshold() const { return PenetrationThreshold; }
	FName GetAttackerWeaponStartSocket() const { return AttackerWeaponStartSocket; }
	FName GetAttackerWeaponEndSocket() const { return AttackerWeaponEndSocket; }
	FName GetAttackerHandBone() const { return AttackerHandBone; }

	// === Setters ===
	// Note: For TWeakObjectPtr fields (AttackerMesh, VictimMesh, AttackerMontage, VictimMontage),
	// use direct assignment since fields are public (e.g., Context.AttackerMesh = MyMesh)
	void SetDistance(float InDistance) { Distance = InDistance; }
	void SetAttackerRotation(const FRotator& InRotation) { AttackerRotation = InRotation; }
	void SetVictimRotation(const FRotator& InRotation) { VictimRotation = InRotation; }
	void SetAttackerPosition(const FVector& InPosition) { AttackerPosition = InPosition; }
	void SetVictimPosition(const FVector& InPosition) { VictimPosition = InPosition; }
	void SetVictimTimeOffset(float InOffset) { VictimTimeOffset = InOffset; }
	void SetMaxDuration(float InDuration) { MaxDuration = InDuration; }
	void SetContactThreshold(float InThreshold) { ContactThreshold = InThreshold; }
	void SetPenetrationThreshold(float InThreshold) { PenetrationThreshold = InThreshold; }
	void SetAttackerWeaponStartSocket(FName InSocket) { AttackerWeaponStartSocket = InSocket; }
	void SetAttackerWeaponEndSocket(FName InSocket) { AttackerWeaponEndSocket = InSocket; }
	void SetAttackerHandBone(FName InBone) { AttackerHandBone = InBone; }

	// === Validation ===
	bool IsValid() const { return AttackerMesh.IsValid() && VictimMesh.IsValid(); }

	// === Factory ===
	static FPairedAnimationAnalysisContext CreateDefault() { return FPairedAnimationAnalysisContext(); }
};

/**
 * Result of single-frame configuration evaluation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FConfigurationEvaluationResult
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float Score = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float ContactQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float PenetrationPenalty = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	bool bHasPenetration = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float ClosestBoneDistance = FLT_MAX;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FName AttackerClosestBone;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FName VictimClosestBone;

public:
	// === Getters ===
	float GetScore() const { return Score; }
	float GetContactQuality() const { return ContactQuality; }
	float GetPenetrationPenalty() const { return PenetrationPenalty; }
	bool HasPenetration() const { return bHasPenetration; }
	float GetClosestBoneDistance() const { return ClosestBoneDistance; }
	FName GetAttackerClosestBone() const { return AttackerClosestBone; }
	FName GetVictimClosestBone() const { return VictimClosestBone; }

	// === Setters ===
	void SetScore(float InScore) { Score = InScore; }
	void SetContactQuality(float InQuality) { ContactQuality = InQuality; }
	void SetPenetrationPenalty(float InPenalty) { PenetrationPenalty = InPenalty; }
	void SetHasPenetration(bool bInHasPenetration) { bHasPenetration = bInHasPenetration; }
	void SetClosestBoneDistance(float InDistance) { ClosestBoneDistance = InDistance; }
	void SetAttackerClosestBone(FName InBone) { AttackerClosestBone = InBone; }
	void SetVictimClosestBone(FName InBone) { VictimClosestBone = InBone; }

	// === Factory ===
	static FConfigurationEvaluationResult CreateDefault() { return FConfigurationEvaluationResult(); }
};

/**
 * Result of holistic (multi-frame) configuration evaluation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FHolisticEvaluationResult
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float OverallScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float PeakContactQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float AverageContactQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float PeakContactTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	int32 SampleCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	TArray<float> PerFrameScores;

public:
	// === Getters ===
	float GetOverallScore() const { return OverallScore; }
	float GetPeakContactQuality() const { return PeakContactQuality; }
	float GetAverageContactQuality() const { return AverageContactQuality; }
	float GetPeakContactTime() const { return PeakContactTime; }
	int32 GetSampleCount() const { return SampleCount; }
	const TArray<float>& GetPerFrameScores() const { return PerFrameScores; }

	// === Setters ===
	void SetOverallScore(float InScore) { OverallScore = InScore; }
	void SetPeakContactQuality(float InQuality) { PeakContactQuality = InQuality; }
	void SetAverageContactQuality(float InQuality) { AverageContactQuality = InQuality; }
	void SetPeakContactTime(float InTime) { PeakContactTime = InTime; }
	void SetSampleCount(int32 InCount) { SampleCount = InCount; }
	TArray<float>& GetPerFrameScoresMutable() { return PerFrameScores; }

	// === Factory ===
	static FHolisticEvaluationResult CreateDefault() { return FHolisticEvaluationResult(); }
};

/**
 * Result of distance optimization.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FDistanceOptimizationResult
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float OptimalDistance = 150.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float Score = 0.0f;

	TArray<TPair<float, float>> DistanceScoreCurve;

public:
	// === Getters ===
	float GetOptimalDistance() const { return OptimalDistance; }
	float GetScore() const { return Score; }
	const TArray<TPair<float, float>>& GetDistanceScoreCurve() const { return DistanceScoreCurve; }

	// === Setters ===
	void SetOptimalDistance(float InDistance) { OptimalDistance = InDistance; }
	void SetScore(float InScore) { Score = InScore; }
	TArray<TPair<float, float>>& GetDistanceScoreCurveMutable() { return DistanceScoreCurve; }

	// === Factory ===
	static FDistanceOptimizationResult CreateDefault() { return FDistanceOptimizationResult(); }
};

/**
 * Result of rotation optimization.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FRotationOptimizationResult
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FRotator OptimalRotation = FRotator::ZeroRotator;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float Score = 0.0f;

	TArray<TPair<float, float>> YawScoreCurve;

public:
	// === Getters ===
	FRotator GetOptimalRotation() const { return OptimalRotation; }
	float GetScore() const { return Score; }
	const TArray<TPair<float, float>>& GetYawScoreCurve() const { return YawScoreCurve; }

	// === Setters ===
	void SetOptimalRotation(const FRotator& InRotation) { OptimalRotation = InRotation; }
	void SetScore(float InScore) { Score = InScore; }
	TArray<TPair<float, float>>& GetYawScoreCurveMutable() { return YawScoreCurve; }

	// === Factory ===
	static FRotationOptimizationResult CreateDefault() { return FRotationOptimizationResult(); }
};

/**
 * Combined full optimization result for Global Paired Orientation.
 * Composes the individual optimization results rather than duplicating fields.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FFullOptimizationResult
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	bool bSuccess = false;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FDistanceOptimizationResult DistanceResult;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FRotationOptimizationResult AttackerRotationResult;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FRotationOptimizationResult VictimRotationResult;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FSpatialRelationshipInference SpatialRelationship;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float OverallScore = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	TArray<FString> Warnings;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	TArray<FString> Suggestions;

public:
	// === Getters ===
	bool IsSuccess() const { return bSuccess; }
	const FDistanceOptimizationResult& GetDistanceResult() const { return DistanceResult; }
	const FRotationOptimizationResult& GetAttackerRotationResult() const { return AttackerRotationResult; }
	const FRotationOptimizationResult& GetVictimRotationResult() const { return VictimRotationResult; }
	const FSpatialRelationshipInference& GetSpatialRelationship() const { return SpatialRelationship; }
	float GetOverallScore() const { return OverallScore; }
	const TArray<FString>& GetWarnings() const { return Warnings; }
	const TArray<FString>& GetSuggestions() const { return Suggestions; }

	// Convenience accessors
	float GetOptimalDistance() const { return DistanceResult.GetOptimalDistance(); }
	FRotator GetOptimalAttackerRotation() const { return AttackerRotationResult.GetOptimalRotation(); }
	FRotator GetOptimalVictimRotation() const { return VictimRotationResult.GetOptimalRotation(); }

	// === Setters ===
	void SetSuccess(bool bInSuccess) { bSuccess = bInSuccess; }
	FDistanceOptimizationResult& GetDistanceResultMutable() { return DistanceResult; }
	FRotationOptimizationResult& GetAttackerRotationResultMutable() { return AttackerRotationResult; }
	FRotationOptimizationResult& GetVictimRotationResultMutable() { return VictimRotationResult; }
	FSpatialRelationshipInference& GetSpatialRelationshipMutable() { return SpatialRelationship; }
	void SetOverallScore(float InScore) { OverallScore = InScore; }
	TArray<FString>& GetWarningsMutable() { return Warnings; }
	TArray<FString>& GetSuggestionsMutable() { return Suggestions; }

	// === Factory ===
	static FFullOptimizationResult CreateDefault() { return FFullOptimizationResult(); }
	static FFullOptimizationResult CreateFailure(const FString& Reason)
	{
		FFullOptimizationResult Result;
		Result.bSuccess = false;
		Result.Warnings.Add(Reason);
		return Result;
	}
};

/**
 * Per-frame contact analysis snapshot (lightweight).
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FContactAnalysisSnapshot
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float Time = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float ContactQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float ClosestDistance = FLT_MAX;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FName AttackerBone;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FName VictimBone;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	FVector ContactPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	bool bHasPenetration = false;

public:
	// === Getters ===
	float GetTime() const { return Time; }
	float GetContactQuality() const { return ContactQuality; }
	float GetClosestDistance() const { return ClosestDistance; }
	FName GetAttackerBone() const { return AttackerBone; }
	FName GetVictimBone() const { return VictimBone; }
	FVector GetContactPoint() const { return ContactPoint; }
	bool HasPenetration() const { return bHasPenetration; }

	// === Setters ===
	void SetTime(float InTime) { Time = InTime; }
	void SetContactQuality(float InQuality) { ContactQuality = InQuality; }
	void SetClosestDistance(float InDistance) { ClosestDistance = InDistance; }
	void SetAttackerBone(FName InBone) { AttackerBone = InBone; }
	void SetVictimBone(FName InBone) { VictimBone = InBone; }
	void SetContactPoint(const FVector& InPoint) { ContactPoint = InPoint; }
	void SetHasPenetration(bool bInHasPenetration) { bHasPenetration = bInHasPenetration; }

	// === Factory ===
	static FContactAnalysisSnapshot CreateDefault() { return FContactAnalysisSnapshot(); }
	static FContactAnalysisSnapshot CreateAtTime(float InTime)
	{
		FContactAnalysisSnapshot Snapshot;
		Snapshot.Time = InTime;
		return Snapshot;
	}
};

/**
 * Timeline of contact analysis across an animation.
 */
USTRUCT(BlueprintType)
struct KATANACOMBATEDITOR_API FContactAnalysisTimeline
{
	GENERATED_BODY()

public: // Fields - public for backward compatibility, getters/setters available for new code
	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	TArray<FContactAnalysisSnapshot> Snapshots;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float BestContactTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float BestContactQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float AverageContactQuality = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float WorstContactQuality = FLT_MAX;

	UPROPERTY(BlueprintReadOnly, Category = "Result", meta = (AllowPrivateAccess = "true"))
	float QualityVariance = 0.0f;

public:
	// === Getters ===
	const TArray<FContactAnalysisSnapshot>& GetSnapshots() const { return Snapshots; }
	float GetBestContactTime() const { return BestContactTime; }
	float GetBestContactQuality() const { return BestContactQuality; }
	float GetAverageContactQuality() const { return AverageContactQuality; }
	float GetWorstContactQuality() const { return WorstContactQuality; }
	float GetQualityVariance() const { return QualityVariance; }

	// === Setters ===
	TArray<FContactAnalysisSnapshot>& GetSnapshotsMutable() { return Snapshots; }
	void SetBestContactTime(float InTime) { BestContactTime = InTime; }
	void SetBestContactQuality(float InQuality) { BestContactQuality = InQuality; }
	void SetAverageContactQuality(float InQuality) { AverageContactQuality = InQuality; }
	void SetWorstContactQuality(float InQuality) { WorstContactQuality = InQuality; }
	void SetQualityVariance(float InVariance) { QualityVariance = InVariance; }

	// === Factory ===
	static FContactAnalysisTimeline CreateDefault() { return FContactAnalysisTimeline(); }
};

// ============================================================================
// UNDO/REDO SUPPORT
// ============================================================================

/**
 * Captures the optimization state for undo/redo support.
 * Stores everything modified by ApplyOptimizationResult.
 */
USTRUCT()
struct KATANACOMBATEDITOR_API FPreviewOptimizationState
{
	GENERATED_BODY()

public:
	UPROPERTY()
	float Distance = 150.0f;

	UPROPERTY()
	FRotator AttackerRotation = FRotator::ZeroRotator;

	UPROPERTY()
	FRotator VictimRotation = FRotator(0.0f, 180.0f, 0.0f);

	UPROPERTY()
	FString Description;

	// === Factory ===
	static FPreviewOptimizationState CreateFromValues(float InDistance, const FRotator& InAttackerRot, const FRotator& InVictimRot, const FString& InDescription = TEXT(""))
	{
		FPreviewOptimizationState State;
		State.Distance = InDistance;
		State.AttackerRotation = InAttackerRot;
		State.VictimRotation = InVictimRot;
		State.Description = InDescription;
		return State;
	}
};

// ============================================================================
// PREVIEW MODEL (PT-11 REFACTOR)
// ============================================================================

/**
 * Model struct for paired animation preview state.
 *
 * PT-11: Extracted from SPairedAnimationPreview "God Widget" to enable:
 * - Testable state management without UI coupling
 * - Clear separation between state (Model) and presentation (Widget)
 * - Reusable state across different view implementations
 *
 * This struct holds all preview state that was previously scattered across
 * widget member variables. The widget now owns a single Model instance.
 *
 * Usage:
 *   FPairedAnimationPreviewModel Model = FPairedAnimationPreviewModel::CreateDefault();
 *   Model.SetAttackerMontage(MyMontage);
 *   Model.InvalidateCaches();
 */
USTRUCT()
struct KATANACOMBATEDITOR_API FPairedAnimationPreviewModel
{
	GENERATED_BODY()

public:
	// ========================================================================
	// ASSET REFERENCES
	// Note: TWeakObjectPtr fields use direct assignment (e.g., Model.AttackerMontage = Montage)
	// ========================================================================

	/** Animation montage for the attacker character */
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> AttackerMontage;

	/** Animation montage for the victim character */
	UPROPERTY()
	TWeakObjectPtr<UAnimMontage> VictimMontage;

	/** Skeletal mesh for the attacker character */
	UPROPERTY()
	TWeakObjectPtr<USkeletalMesh> AttackerSkeleton;

	/** Skeletal mesh for the victim character */
	UPROPERTY()
	TWeakObjectPtr<USkeletalMesh> VictimSkeleton;

	// ========================================================================
	// CHARACTER CONFIGURATION
	// ========================================================================

	/** Configuration for attacker character (position, rotation, color, tracked bones) */
	UPROPERTY()
	FCharacterPreviewConfig AttackerConfig;

	/** Configuration for victim character */
	UPROPERTY()
	FCharacterPreviewConfig VictimConfig;

	/** Weapon mesh configuration for attacker */
	UPROPERTY()
	FWeaponMeshConfig AttackerWeaponConfig;

	/** Weapon mesh configuration for victim */
	UPROPERTY()
	FWeaponMeshConfig VictimWeaponConfig;

	/** Bone configuration for multi-contact tracking */
	UPROPERTY()
	FMultiContactBoneConfig ContactBoneConfig;

	// ========================================================================
	// POSITIONING
	// ========================================================================

	/** Whether victim position is locked relative to attacker */
	UPROPERTY()
	bool bLockVictimToAttacker = true;

	/** Distance between attacker and victim when locked */
	UPROPERTY()
	float LockedDistance = PairedAnimPreviewConfig::Defaults::Distance;

	/** Current spatial relationship constraint for optimization */
	UPROPERTY()
	ESpatialRelationship SpatialRelationship = ESpatialRelationship::Inferred;

	/** Last inferred spatial relationship from animation analysis */
	UPROPERTY()
	FSpatialRelationshipInference LastInferredRelationship;

	// ========================================================================
	// PLAYBACK STATE
	// ========================================================================

	/** Current playback time in seconds */
	UPROPERTY()
	float CurrentTime = 0.0f;

	/** Minimum time (usually 0.0) */
	UPROPERTY()
	float MinTime = 0.0f;

	/** Maximum duration based on selected montage sections */
	UPROPERTY()
	float MaxDuration = 0.0f;

	/** Whether animation is currently playing */
	UPROPERTY()
	bool bIsPlaying = false;

	/** Playback speed multiplier (1.0 = normal speed) */
	UPROPERTY()
	float PlaybackSpeed = 1.0f;

	/** Whether to loop playback at end */
	UPROPERTY()
	bool bLoopPlayback = true;

	/** Whether to ping-pong (reverse at ends) */
	UPROPERTY()
	bool bPingPongPlayback = false;

	/** Current ping-pong direction (true = forward, false = backward) */
	UPROPERTY()
	bool bPingPongForward = true;

	// ========================================================================
	// MONTAGE SECTION SELECTION
	// ========================================================================

	/** Selected section name for attacker montage (NAME_None = full montage) */
	UPROPERTY()
	FName AttackerMontageSection = NAME_None;

	/** Selected section name for victim montage (NAME_None = full montage) */
	UPROPERTY()
	FName VictimMontageSection = NAME_None;

	/** Start time of selected attacker section */
	UPROPERTY()
	float AttackerSectionStart = 0.0f;

	/** End time of selected attacker section */
	UPROPERTY()
	float AttackerSectionEnd = 0.0f;

	/** Start time of selected victim section */
	UPROPERTY()
	float VictimSectionStart = 0.0f;

	/** End time of selected victim section */
	UPROPERTY()
	float VictimSectionEnd = 0.0f;

	/**
	 * Timing offset for victim montage relative to attacker.
	 * Positive = victim starts later, Negative = victim starts earlier.
	 * Range: -2.0 to 2.0 seconds.
	 */
	UPROPERTY()
	float VictimTimeOffset = 0.0f;

	// ========================================================================
	// ANALYSIS SETTINGS
	// ========================================================================

	/** Active visualization layers (bitfield) */
	UPROPERTY()
	EVisualizationLayer VisualizationLayers = EVisualizationLayer::Skeletons | EVisualizationLayer::ContactPoints;

	/** Threshold distance for contact detection */
	UPROPERTY()
	float ContactThreshold = PairedAnimPreviewConfig::Defaults::ContactThreshold;

	/** Number of samples for trajectory/timeline analysis */
	UPROPERTY()
	int32 AnalysisSampleCount = PairedAnimPreviewConfig::Defaults::AnalysisSampleCount;

	/**
	 * Weights for multi-contact scoring by contact point type.
	 * Higher weights mean that contact point type is more important for optimization.
	 *
	 * Note: Default values synchronized with PairedAnimPreviewConfig::Weights namespace.
	 * If modifying defaults, update both locations.
	 */
	UPROPERTY()
	TMap<EContactPointType, float> ContactTypeWeights;

	// ========================================================================
	// CACHE STATE FLAGS
	// ========================================================================

	/** Whether frame analysis cache needs rebuild */
	UPROPERTY()
	bool bFrameAnalysisCacheDirty = true;

	/** Whether trajectory cache needs rebuild */
	UPROPERTY()
	bool bTrajectoryCacheDirty = true;

	/** Whether holistic analysis cache needs rebuild */
	UPROPERTY()
	bool bHolisticCacheDirty = true;

	/** Whether spatial relationship inference cache needs rebuild */
	UPROPERTY()
	bool bSpatialInferenceCacheDirty = true;

	// ========================================================================
	// CACHED ANALYSIS RESULTS
	// Note: These are populated by analysis operations and consumed by UI/visualization
	// ========================================================================

	/** Per-frame analysis results (bone positions, contacts, etc.) */
	TArray<FPairedFrameAnalysis> FrameAnalysisCache;

	/** Bone trajectories for attacker */
	TArray<FBoneTrajectory> AttackerTrajectories;

	/** Bone trajectories for victim */
	TArray<FBoneTrajectory> VictimTrajectories;

	/** Holistic timeline analysis results */
	UPROPERTY()
	FHolisticTimelineAnalysis HolisticAnalysis;

	/** Last optimization result for display */
	UPROPERTY()
	FOptimizationResult LastOptimizationResult;

	// ========================================================================
	// UNDO/REDO HISTORY (PT-19)
	// ========================================================================

	/** History stack for optimization undo/redo */
	TArray<FPreviewOptimizationState> OptimizationHistory;

	/** Current position in history stack (-1 = no history) */
	UPROPERTY()
	int32 CurrentHistoryIndex = -1;

	/** Maximum history entries to keep */
	UPROPERTY()
	int32 MaxHistorySize = PairedAnimPreviewConfig::Defaults::HistoryMaxSize;

public:
	// ========================================================================
	// INITIALIZATION
	// ========================================================================

	/**
	 * Initialize contact type weights with default values from config.
	 * Values sourced from PairedAnimPreviewConfig::Weights namespace.
	 */
	void InitializeContactTypeWeights()
	{
		ContactTypeWeights.Empty();
		ContactTypeWeights.Add(EContactPointType::Head, PairedAnimPreviewConfig::Weights::Head);
		ContactTypeWeights.Add(EContactPointType::LeftHand, PairedAnimPreviewConfig::Weights::LeftHand);
		ContactTypeWeights.Add(EContactPointType::RightHand, PairedAnimPreviewConfig::Weights::RightHand);
		ContactTypeWeights.Add(EContactPointType::LeftFoot, PairedAnimPreviewConfig::Weights::LeftFoot);
		ContactTypeWeights.Add(EContactPointType::RightFoot, PairedAnimPreviewConfig::Weights::RightFoot);
		ContactTypeWeights.Add(EContactPointType::Pelvis, PairedAnimPreviewConfig::Weights::Pelvis);
		ContactTypeWeights.Add(EContactPointType::WeaponTip, PairedAnimPreviewConfig::Weights::WeaponTip);
		ContactTypeWeights.Add(EContactPointType::WeaponMid, PairedAnimPreviewConfig::Weights::WeaponMid);
		ContactTypeWeights.Add(EContactPointType::WeaponBase, PairedAnimPreviewConfig::Weights::WeaponBase);
	}

	// ========================================================================
	// ASSET ACCESSORS
	// ========================================================================

	UAnimMontage* GetAttackerMontage() const { return AttackerMontage.Get(); }
	UAnimMontage* GetVictimMontage() const { return VictimMontage.Get(); }
	USkeletalMesh* GetAttackerSkeleton() const { return AttackerSkeleton.Get(); }
	USkeletalMesh* GetVictimSkeleton() const { return VictimSkeleton.Get(); }

	bool HasValidAttackerMontage() const { return AttackerMontage.IsValid(); }
	bool HasValidVictimMontage() const { return VictimMontage.IsValid(); }
	bool HasValidAttackerSkeleton() const { return AttackerSkeleton.IsValid(); }
	bool HasValidVictimSkeleton() const { return VictimSkeleton.IsValid(); }

	/** Returns true if both characters have montages assigned */
	bool HasBothMontages() const { return HasValidAttackerMontage() && HasValidVictimMontage(); }

	/** Returns true if minimum required assets are present for preview */
	bool CanPreview() const { return HasValidAttackerSkeleton() && HasValidVictimSkeleton(); }

	// ========================================================================
	// TIME CALCULATIONS
	// ========================================================================

	/**
	 * Get the effective attacker time within section bounds.
	 * Clamps CurrentTime to [AttackerSectionStart, AttackerSectionEnd].
	 */
	float GetAttackerTime() const
	{
		if (!FMath::IsNearlyEqual(AttackerSectionStart, AttackerSectionEnd, KINDA_SMALL_NUMBER))
		{
			return FMath::Clamp(CurrentTime + AttackerSectionStart, AttackerSectionStart, AttackerSectionEnd);
		}
		return CurrentTime;
	}

	/**
	 * Get the effective victim time, accounting for offset and section bounds.
	 *
	 * Time Offset Semantics:
	 * - Positive VictimTimeOffset: Victim animation starts LATER than attacker
	 *   (e.g., VictimTimeOffset=1.0 means at CurrentTime=0, victim is at time -1.0 → clamped to 0)
	 * - Negative VictimTimeOffset: Victim animation starts EARLIER than attacker
	 *   (e.g., VictimTimeOffset=-1.0 means at CurrentTime=0, victim is at time 1.0)
	 *
	 * The FMath::Max(0.0f, ...) ensures we never request a negative animation time.
	 */
	float GetVictimTime() const
	{
		const float BaseVictimTime = FMath::Max(0.0f, CurrentTime - VictimTimeOffset);
		if (!FMath::IsNearlyEqual(VictimSectionStart, VictimSectionEnd, KINDA_SMALL_NUMBER))
		{
			return FMath::Clamp(BaseVictimTime + VictimSectionStart, VictimSectionStart, VictimSectionEnd);
		}
		return BaseVictimTime;
	}

	/** Get playback progress as 0-1 ratio */
	float GetPlaybackProgress() const
	{
		if (MaxDuration > KINDA_SMALL_NUMBER)
		{
			return FMath::Clamp(CurrentTime / MaxDuration, 0.0f, 1.0f);
		}
		return 0.0f;
	}

	// ========================================================================
	// CACHE MANAGEMENT
	// ========================================================================

	/** Invalidate all analysis caches (call when configuration changes) */
	void InvalidateAllCaches()
	{
		bFrameAnalysisCacheDirty = true;
		bTrajectoryCacheDirty = true;
		bHolisticCacheDirty = true;
	}

	/** Invalidate only position-dependent caches (call when distance/rotation changes) */
	void InvalidatePositionCaches()
	{
		bFrameAnalysisCacheDirty = true;
		bHolisticCacheDirty = true;
	}

	/** Clear all cached data */
	void ClearCaches()
	{
		FrameAnalysisCache.Empty();
		AttackerTrajectories.Empty();
		VictimTrajectories.Empty();
		HolisticAnalysis = FHolisticTimelineAnalysis::CreateDefault();
		InvalidateAllCaches();
	}

	// ========================================================================
	// UNDO/REDO SUPPORT (PT-19)
	// ========================================================================

	/**
	 * Push current optimization state to history.
	 * Truncates any redo states ahead of current position.
	 */
	void PushStateToHistory(const FString& Description = TEXT(""))
	{
		// Truncate any redo states
		if (CurrentHistoryIndex < OptimizationHistory.Num() - 1)
		{
			OptimizationHistory.SetNum(CurrentHistoryIndex + 1);
		}

		// Create and push new state
		FPreviewOptimizationState State = FPreviewOptimizationState::CreateFromValues(
			LockedDistance,
			AttackerConfig.GetRotationOffset(),
			VictimConfig.GetRotationOffset(),
			Description
		);
		OptimizationHistory.Add(State);
		CurrentHistoryIndex = OptimizationHistory.Num() - 1;

		// Enforce max history size
		if (OptimizationHistory.Num() > MaxHistorySize)
		{
			OptimizationHistory.RemoveAt(0, OptimizationHistory.Num() - MaxHistorySize);
			CurrentHistoryIndex = OptimizationHistory.Num() - 1;
		}
	}

	/** Returns true if undo is available */
	bool CanUndo() const { return CurrentHistoryIndex > 0; }

	/** Returns true if redo is available */
	bool CanRedo() const { return CurrentHistoryIndex < OptimizationHistory.Num() - 1; }

	/**
	 * Get the undo state (state before current).
	 * Returns nullptr if undo is not available.
	 */
	const FPreviewOptimizationState* GetUndoState() const
	{
		if (!CanUndo())
		{
			return nullptr;
		}
		const int32 UndoIndex = CurrentHistoryIndex - 1;
		// Defensive bounds check (should always pass if CanUndo() returned true)
		if (!OptimizationHistory.IsValidIndex(UndoIndex))
		{
			return nullptr;
		}
		return &OptimizationHistory[UndoIndex];
	}

	/**
	 * Get the redo state (state after current).
	 * Returns nullptr if redo is not available.
	 */
	const FPreviewOptimizationState* GetRedoState() const
	{
		if (!CanRedo())
		{
			return nullptr;
		}
		const int32 RedoIndex = CurrentHistoryIndex + 1;
		// Defensive bounds check (should always pass if CanRedo() returned true)
		if (!OptimizationHistory.IsValidIndex(RedoIndex))
		{
			return nullptr;
		}
		return &OptimizationHistory[RedoIndex];
	}

	/** Move to undo state (decrements history index) */
	void ApplyUndo()
	{
		if (CanUndo())
		{
			CurrentHistoryIndex--;
		}
	}

	/** Move to redo state (increments history index) */
	void ApplyRedo()
	{
		if (CanRedo())
		{
			CurrentHistoryIndex++;
		}
	}

	/** Get current state from history, or nullptr if no history */
	const FPreviewOptimizationState* GetCurrentHistoryState() const
	{
		if (!OptimizationHistory.IsValidIndex(CurrentHistoryIndex))
		{
			return nullptr;
		}
		return &OptimizationHistory[CurrentHistoryIndex];
	}

	// ========================================================================
	// VISUALIZATION LAYER HELPERS
	// ========================================================================

	bool IsVisualizationLayerEnabled(EVisualizationLayer Layer) const
	{
		return EnumHasAnyFlags(VisualizationLayers, Layer);
	}

	void SetVisualizationLayerEnabled(EVisualizationLayer Layer, bool bEnabled)
	{
		if (bEnabled)
		{
			VisualizationLayers |= Layer;
		}
		else
		{
			VisualizationLayers &= ~Layer;
		}
	}

	void ToggleVisualizationLayer(EVisualizationLayer Layer)
	{
		VisualizationLayers ^= Layer;
	}

	// ========================================================================
	// WEIGHT ACCESSORS
	// ========================================================================

	/** Get weight for a contact point type (returns 1.0 if not found) */
	float GetContactTypeWeight(EContactPointType Type) const
	{
		const float* Weight = ContactTypeWeights.Find(Type);
		return Weight ? *Weight : 1.0f;
	}

	/** Set weight for a contact point type */
	void SetContactTypeWeight(EContactPointType Type, float Weight)
	{
		ContactTypeWeights.Add(Type, Weight);
	}

	// ========================================================================
	// RESET
	// ========================================================================

	/** Reset to default state (clears all references and caches) */
	void Reset()
	{
		// Clear asset references
		AttackerMontage.Reset();
		VictimMontage.Reset();
		AttackerSkeleton.Reset();
		VictimSkeleton.Reset();

		// Reset configs to defaults
		AttackerConfig = FCharacterPreviewConfig::CreateAttacker();
		VictimConfig = FCharacterPreviewConfig::CreateVictim();
		AttackerWeaponConfig = FWeaponMeshConfig::CreateDefault();
		VictimWeaponConfig = FWeaponMeshConfig::CreateDefault();
		ContactBoneConfig = FMultiContactBoneConfig::CreateDefault();

		// Reset positioning
		bLockVictimToAttacker = true;
		LockedDistance = PairedAnimPreviewConfig::Defaults::Distance;
		SpatialRelationship = ESpatialRelationship::Inferred;
		LastInferredRelationship = FSpatialRelationshipInference::CreateDefault();

		// Reset playback
		CurrentTime = 0.0f;
		MinTime = 0.0f;
		MaxDuration = 0.0f;
		bIsPlaying = false;
		PlaybackSpeed = PairedAnimPreviewConfig::Defaults::PlaybackSpeed;
		bLoopPlayback = true;
		bPingPongPlayback = false;
		bPingPongForward = true;

		// Reset section selection
		AttackerMontageSection = NAME_None;
		VictimMontageSection = NAME_None;
		AttackerSectionStart = 0.0f;
		AttackerSectionEnd = 0.0f;
		VictimSectionStart = 0.0f;
		VictimSectionEnd = 0.0f;
		VictimTimeOffset = 0.0f;

		// Reset analysis settings
		VisualizationLayers = EVisualizationLayer::Skeletons | EVisualizationLayer::ContactPoints;
		ContactThreshold = PairedAnimPreviewConfig::Defaults::ContactThreshold;
		AnalysisSampleCount = PairedAnimPreviewConfig::Defaults::AnalysisSampleCount;
		InitializeContactTypeWeights();

		// Clear caches
		ClearCaches();

		// Clear history
		OptimizationHistory.Empty();
		CurrentHistoryIndex = -1;
		LastOptimizationResult = FOptimizationResult::CreateDefault();
	}

	// ========================================================================
	// FACTORY
	// ========================================================================

	static FPairedAnimationPreviewModel CreateDefault()
	{
		FPairedAnimationPreviewModel Model;
		Model.AttackerConfig = FCharacterPreviewConfig::CreateAttacker();
		Model.VictimConfig = FCharacterPreviewConfig::CreateVictim();
		Model.InitializeContactTypeWeights();
		return Model;
	}
};
