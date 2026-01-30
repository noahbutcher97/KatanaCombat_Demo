// Copyright Epic Games, Inc. All Rights Reserved.

#include "PairedAnimationPreview.h"
#include "Animation/AnimMontage.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AdvancedPreviewScene.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMeshSocket.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "PropertyCustomizationHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "DrawDebugHelpers.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/FileHelper.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "PairedAnimationPreview"

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

static const FNumberFormattingOptions& GetNumberFormat0()
{
	static FNumberFormattingOptions Options = []() {
		FNumberFormattingOptions Opts;
		Opts.SetMaximumFractionalDigits(0);
		return Opts;
	}();
	return Options;
}

static const FNumberFormattingOptions& GetNumberFormat1()
{
	static FNumberFormattingOptions Options = []() {
		FNumberFormattingOptions Opts;
		Opts.SetMaximumFractionalDigits(1);
		return Opts;
	}();
	return Options;
}

static const FNumberFormattingOptions& GetNumberFormat2()
{
	static FNumberFormattingOptions Options = []() {
		FNumberFormattingOptions Opts;
		Opts.SetMaximumFractionalDigits(2);
		return Opts;
	}();
	return Options;
}

static const FNumberFormattingOptions& GetNumberFormat3()
{
	static FNumberFormattingOptions Options = []() {
		FNumberFormattingOptions Opts;
		Opts.SetMaximumFractionalDigits(3);
		return Opts;
	}();
	return Options;
}

// ============================================================================
// CUSTOM VIEWPORT CLIENT FOR SHARED SCENE
// ============================================================================

class FPairedPreviewViewportClient : public FEditorViewportClient
{
public:
	FPairedPreviewViewportClient(FAdvancedPreviewScene* InPreviewScene)
		: FEditorViewportClient(nullptr, InPreviewScene)
		, PreviewScenePtr(InPreviewScene)
	{
		SetViewLocation(FVector(-400.0f, 0.0f, 100.0f));
		SetViewRotation(FRotator(-10.0f, 0.0f, 0.0f));
		SetRealtime(true);

		// Better camera settings
		SetCameraSpeedSetting(3);
		EngineShowFlags.SetGrid(true);
	}

	virtual void Tick(float DeltaSeconds) override
	{
		FEditorViewportClient::Tick(DeltaSeconds);
		if (PreviewScenePtr)
		{
			PreviewScenePtr->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
		}
	}

	void FocusOnPoint(const FVector& Point, float Distance = 300.0f)
	{
		SetViewLocation(Point + FVector(-Distance, 0.0f, Distance * 0.3f));
		SetLookAtLocation(Point);
	}

private:
	FAdvancedPreviewScene* PreviewScenePtr;
};

// ============================================================================
// CUSTOM VIEWPORT WIDGET
// ============================================================================

class SPairedPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SPairedPreviewViewport) {}
		SLATE_ARGUMENT(FAdvancedPreviewScene*, PreviewScene)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PreviewScene = InArgs._PreviewScene;
		SEditorViewport::Construct(SEditorViewport::FArguments());
	}

	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
	{
		ViewportClient = MakeShareable(new FPairedPreviewViewportClient(PreviewScene));
		return ViewportClient.ToSharedRef();
	}

	TSharedPtr<FPairedPreviewViewportClient> GetViewportClient() const { return ViewportClient; }

private:
	FAdvancedPreviewScene* PreviewScene = nullptr;
	TSharedPtr<FPairedPreviewViewportClient> ViewportClient;
};

// ============================================================================
// TAB REGISTRATION
// ============================================================================

void SPairedAnimationPreview::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		GetTabName(),
		FOnSpawnTab::CreateStatic(&SPairedAnimationPreview::SpawnTab))
		.SetDisplayName(LOCTEXT("TabTitle", "Paired Animation Preview"))
		.SetTooltipText(LOCTEXT("TabTooltip", "Industry-grade tool for analyzing and optimizing paired combat animations"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Add to Window menu (same pattern as MontageAnalysisDashboard)
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension(
		"LevelEditor",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("PreviewMenuEntry", "Paired Animation Preview"),
				LOCTEXT("PreviewMenuTooltip", "Open the Paired Animation Preview tool for analyzing combat animation pairs"),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateLambda([]()
				{
					FGlobalTabmanager::Get()->TryInvokeTab(GetTabName());
				}))
			);
		})
	);
	LevelEditorModule.GetMenuExtensibilityManager()->AddExtender(MenuExtender);
}

void SPairedAnimationPreview::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GetTabName());
}

TSharedRef<SDockTab> SPairedAnimationPreview::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SPairedAnimationPreview)
		];
}

// ============================================================================
// CONSTRUCTION & DESTRUCTION
// ============================================================================

void SPairedAnimationPreview::Construct(const FArguments& InArgs)
{
	// Initialize default configs
	AttackerConfig.Color = AttackerColor;
	AttackerConfig.PositionOffset = FVector::ZeroVector;
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;

	VictimConfig.Color = VictimColor;
	VictimConfig.PositionOffset = FVector(LockedDistance, 0.0f, 0.0f);
	VictimConfig.RotationOffset = FRotator(0.0f, 180.0f, 0.0f);

	SetupSharedPreviewScene();

	ChildSlot
	[
		BuildMainLayout()
	];
}

SPairedAnimationPreview::~SPairedAnimationPreview()
{
	if (AttackerMeshComponent)
	{
		AttackerMeshComponent->DestroyComponent();
	}
	if (VictimMeshComponent)
	{
		VictimMeshComponent->DestroyComponent();
	}
}

// ============================================================================
// PREVIEW SCENE SETUP
// ============================================================================

void SPairedAnimationPreview::SetupSharedPreviewScene()
{
	SharedPreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));

	// Create attacker mesh component
	AttackerMeshComponent = NewObject<UDebugSkelMeshComponent>();
	SharedPreviewScene->AddComponent(AttackerMeshComponent, FTransform::Identity);

	// Create victim mesh component
	VictimMeshComponent = NewObject<UDebugSkelMeshComponent>();
	SharedPreviewScene->AddComponent(VictimMeshComponent, FTransform::Identity);

	ApplyCharacterConfigs();
}

void SPairedAnimationPreview::UpdateAttackerMesh(USkeletalMesh* Mesh)
{
	if (AttackerMeshComponent && Mesh)
	{
		AttackerMeshComponent->SetSkeletalMesh(Mesh);
		AttackerMeshComponent->SetForcedLOD(1);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::UpdateVictimMesh(USkeletalMesh* Mesh)
{
	if (VictimMeshComponent && Mesh)
	{
		VictimMeshComponent->SetSkeletalMesh(Mesh);
		VictimMeshComponent->SetForcedLOD(1);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::ApplyCharacterConfigs()
{
	if (AttackerMeshComponent)
	{
		FTransform AttackerTransform;
		AttackerTransform.SetLocation(AttackerConfig.PositionOffset);
		AttackerTransform.SetRotation(AttackerConfig.RotationOffset.Quaternion());
		AttackerTransform.SetScale3D(FVector(AttackerConfig.Scale));
		AttackerMeshComponent->SetRelativeTransform(AttackerTransform);
	}

	if (VictimMeshComponent)
	{
		FVector VictimPosition;
		if (bLockVictimToAttacker)
		{
			// Lock mode: Victim is always LockedDistance away on X axis (world forward)
			// This keeps them facing each other without rotating victim when attacker rotates
			// Victim's own position offset Y/Z can still adjust lateral/vertical position
			VictimPosition = FVector(
				AttackerConfig.PositionOffset.X + LockedDistance,
				AttackerConfig.PositionOffset.Y + VictimConfig.PositionOffset.Y,
				AttackerConfig.PositionOffset.Z + VictimConfig.PositionOffset.Z
			);
		}
		else
		{
			// Unlocked mode: Use victim's position offset directly
			// LockedDistance still sets the X component for convenience
			VictimPosition = FVector(
				LockedDistance,
				VictimConfig.PositionOffset.Y,
				VictimConfig.PositionOffset.Z
			);
		}

		FTransform VictimTransform;
		VictimTransform.SetLocation(VictimPosition);
		VictimTransform.SetRotation(VictimConfig.RotationOffset.Quaternion());
		VictimTransform.SetScale3D(FVector(VictimConfig.Scale));
		VictimMeshComponent->SetRelativeTransform(VictimTransform);
	}

	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::UpdateCharacterPositions()
{
	ApplyCharacterConfigs();
}

void SPairedAnimationPreview::UpdateAnimations(float Time)
{
	// Best practice from UE5 research: Just call SetPosition()
	// The preview scene's TickPreviewScene() handles animation updates
	// Manual TickAnimation/RefreshBoneTransforms can cause fighting

	if (AttackerMeshComponent && AttackerMontage.IsValid())
	{
		float AttackerTime = GetAttackerTime();
		AttackerMeshComponent->SetPosition(AttackerTime);
	}

	if (VictimMeshComponent && VictimMontage.IsValid())
	{
		float VictimTime = GetVictimTime();
		VictimMeshComponent->SetPosition(VictimTime);
	}
}

// ============================================================================
// CONFIGURATION CALLBACKS
// ============================================================================

void SPairedAnimationPreview::OnAttackerPositionChanged(FVector NewPosition)
{
	AttackerConfig.PositionOffset = NewPosition;
	ApplyCharacterConfigs();
}

void SPairedAnimationPreview::OnVictimPositionChanged(FVector NewPosition)
{
	VictimConfig.PositionOffset = NewPosition;
	// Always apply - in lock mode, Y/Z offsets still work
	ApplyCharacterConfigs();
}

void SPairedAnimationPreview::OnAttackerRotationChanged(FRotator NewRotation)
{
	AttackerConfig.RotationOffset = NewRotation;
	ApplyCharacterConfigs();
}

void SPairedAnimationPreview::OnVictimRotationChanged(FRotator NewRotation)
{
	VictimConfig.RotationOffset = NewRotation;
	ApplyCharacterConfigs();
}

void SPairedAnimationPreview::OnLockedDistanceChanged(float NewDistance)
{
	LockedDistance = NewDistance;
	ApplyCharacterConfigs();
}

// ============================================================================
// ASSET SELECTION
// ============================================================================

FString SPairedAnimationPreview::GetAttackerMontagePath() const
{
	return AttackerMontage.IsValid() ? AttackerMontage->GetPathName() : FString();
}

FString SPairedAnimationPreview::GetVictimMontagePath() const
{
	return VictimMontage.IsValid() ? VictimMontage->GetPathName() : FString();
}

FString SPairedAnimationPreview::GetAttackerSkeletonPath() const
{
	return AttackerSkeleton.IsValid() ? AttackerSkeleton->GetPathName() : FString();
}

FString SPairedAnimationPreview::GetVictimSkeletonPath() const
{
	return VictimSkeleton.IsValid() ? VictimSkeleton->GetPathName() : FString();
}

void SPairedAnimationPreview::OnAttackerMontageSelected(const FAssetData& AssetData)
{
	AttackerMontage = Cast<UAnimMontage>(AssetData.GetAsset());
	if (AttackerMeshComponent && AttackerMontage.IsValid())
	{
		// Best practice: Just set mode, animation, and position
		// Don't call Play() or Stop() - we control position directly via SetPosition()
		AttackerMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		AttackerMeshComponent->SetAnimation(AttackerMontage.Get());
		AttackerMeshComponent->SetPosition(0.0f);
	}
	RecalculateMaxDuration();
	bAnalysisCacheDirty = true;

	// Clear caches when montage changes
	FrameAnalysisCache.Empty();
	AttackerTrajectories.Empty();

	// Reset time to start
	CurrentTime = 0.0f;
	bIsPlaying = false;
	UpdateAnimations(CurrentTime);
}

void SPairedAnimationPreview::OnVictimMontageSelected(const FAssetData& AssetData)
{
	VictimMontage = Cast<UAnimMontage>(AssetData.GetAsset());
	if (VictimMeshComponent && VictimMontage.IsValid())
	{
		// Best practice: Just set mode, animation, and position
		// Don't call Play() or Stop() - we control position directly via SetPosition()
		VictimMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		VictimMeshComponent->SetAnimation(VictimMontage.Get());
		VictimMeshComponent->SetPosition(0.0f);
	}
	RecalculateMaxDuration();
	bAnalysisCacheDirty = true;

	// Clear caches when montage changes
	FrameAnalysisCache.Empty();
	VictimTrajectories.Empty();

	// Reset time to start
	CurrentTime = 0.0f;
	bIsPlaying = false;
	UpdateAnimations(CurrentTime);
}

void SPairedAnimationPreview::OnAttackerSkeletonSelected(const FAssetData& AssetData)
{
	AttackerSkeleton = Cast<USkeletalMesh>(AssetData.GetAsset());
	UpdateAttackerMesh(AttackerSkeleton.Get());
}

void SPairedAnimationPreview::OnVictimSkeletonSelected(const FAssetData& AssetData)
{
	VictimSkeleton = Cast<USkeletalMesh>(AssetData.GetAsset());
	UpdateVictimMesh(VictimSkeleton.Get());
}

void SPairedAnimationPreview::RecalculateMaxDuration()
{
	MaxDuration = 0.0f;
	if (AttackerMontage.IsValid())
	{
		MaxDuration = FMath::Max(MaxDuration, AttackerMontage->GetPlayLength());
	}
	if (VictimMontage.IsValid())
	{
		MaxDuration = FMath::Max(MaxDuration, VictimMontage->GetPlayLength() + VictimTimeOffset);
	}
}

// ============================================================================
// SOCKET CONFIGURATION
// ============================================================================

void SPairedAnimationPreview::OnWeaponStartSocketChanged(const FText& NewText, ETextCommit::Type CommitType)
{
	AttackerConfig.WeaponStartSocket = FName(*NewText.ToString());
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::OnWeaponEndSocketChanged(const FText& NewText, ETextCommit::Type CommitType)
{
	AttackerConfig.WeaponEndSocket = FName(*NewText.ToString());
	bAnalysisCacheDirty = true;
}

TArray<FName> SPairedAnimationPreview::GetAvailableSockets(UDebugSkelMeshComponent* Mesh) const
{
	TArray<FName> Sockets;
	if (Mesh && Mesh->GetSkeletalMeshAsset())
	{
		const TArray<USkeletalMeshSocket*>& MeshSockets = Mesh->GetSkeletalMeshAsset()->GetActiveSocketList();
		for (const USkeletalMeshSocket* Socket : MeshSockets)
		{
			if (Socket)
			{
				Sockets.Add(Socket->SocketName);
			}
		}
	}
	return Sockets;
}

// ============================================================================
// PROCEDURAL ANALYSIS ENGINE
// ============================================================================

FVector SPairedAnimationPreview::GetBoneWorldLocation(UDebugSkelMeshComponent* Mesh, FName BoneName) const
{
	if (!Mesh || BoneName.IsNone()) return FVector::ZeroVector;

	int32 BoneIndex = Mesh->GetBoneIndex(BoneName);
	if (BoneIndex != INDEX_NONE)
	{
		return Mesh->GetBoneTransform(BoneIndex).GetLocation();
	}
	return FVector::ZeroVector;
}

FVector SPairedAnimationPreview::GetSocketWorldLocation(UDebugSkelMeshComponent* Mesh, FName SocketName) const
{
	if (!Mesh || SocketName.IsNone()) return FVector::ZeroVector;
	return Mesh->GetSocketLocation(SocketName);
}

FVector SPairedAnimationPreview::ComputeBoneVelocity(UDebugSkelMeshComponent* Mesh, FName BoneName, float Time, float DeltaTime)
{
	if (!Mesh || !AttackerMontage.IsValid()) return FVector::ZeroVector;

	// Sample bone position at two times
	float TimeBefore = FMath::Max(0.0f, Time - DeltaTime);
	float TimeAfter = FMath::Min(MaxDuration, Time + DeltaTime);

	// Save current position
	float OriginalTime = CurrentTime;

	// Get position before
	Mesh->SetPosition(TimeBefore);
	Mesh->RefreshBoneTransforms();
	FVector PosBefore = GetBoneWorldLocation(Mesh, BoneName);

	// Get position after
	Mesh->SetPosition(TimeAfter);
	Mesh->RefreshBoneTransforms();
	FVector PosAfter = GetBoneWorldLocation(Mesh, BoneName);

	// Restore
	Mesh->SetPosition(OriginalTime);
	Mesh->RefreshBoneTransforms();

	// Compute velocity
	float ActualDelta = TimeAfter - TimeBefore;
	if (ActualDelta > SMALL_NUMBER)
	{
		return (PosAfter - PosBefore) / ActualDelta;
	}
	return FVector::ZeroVector;
}

FName SPairedAnimationPreview::FindClosestBone(UDebugSkelMeshComponent* Mesh, const FVector& WorldLocation, float& OutDistance) const
{
	OutDistance = FLT_MAX;
	FName ClosestBone = NAME_None;

	if (!Mesh) return ClosestBone;

	TArray<FName> AllBones = GetAllBoneNames(Mesh);
	for (const FName& BoneName : AllBones)
	{
		FVector BoneLocation = GetBoneWorldLocation(Mesh, BoneName);
		float Dist = FVector::Dist(WorldLocation, BoneLocation);
		if (Dist < OutDistance)
		{
			OutDistance = Dist;
			ClosestBone = BoneName;
		}
	}
	return ClosestBone;
}

float SPairedAnimationPreview::ComputeClosestSkeletonDistance(FName& OutAttackerBone, FName& OutVictimBone) const
{
	float MinDistance = FLT_MAX;
	OutAttackerBone = NAME_None;
	OutVictimBone = NAME_None;

	if (!AttackerMeshComponent || !VictimMeshComponent) return MinDistance;

	TArray<FName> AttackerBones = GetAllBoneNames(AttackerMeshComponent);
	TArray<FName> VictimBones = GetAllBoneNames(VictimMeshComponent);

	for (const FName& ABone : AttackerBones)
	{
		FVector APos = GetBoneWorldLocation(AttackerMeshComponent, ABone);
		for (const FName& VBone : VictimBones)
		{
			FVector VPos = GetBoneWorldLocation(VictimMeshComponent, VBone);
			float Dist = FVector::Dist(APos, VPos);
			if (Dist < MinDistance)
			{
				MinDistance = Dist;
				OutAttackerBone = ABone;
				OutVictimBone = VBone;
			}
		}
	}
	return MinDistance;
}

FVector SPairedAnimationPreview::ComputeCenterOfMass(UDebugSkelMeshComponent* Mesh) const
{
	if (!Mesh) return FVector::ZeroVector;

	FVector COM = FVector::ZeroVector;
	TArray<FName> Bones = GetAllBoneNames(Mesh);

	if (Bones.Num() == 0) return COM;

	for (const FName& BoneName : Bones)
	{
		COM += GetBoneWorldLocation(Mesh, BoneName);
	}
	return COM / Bones.Num();
}

TArray<FName> SPairedAnimationPreview::GetAllBoneNames(UDebugSkelMeshComponent* Mesh) const
{
	TArray<FName> BoneNames;
	if (Mesh && Mesh->GetSkeletalMeshAsset())
	{
		const FReferenceSkeleton& RefSkel = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		for (int32 i = 0; i < RefSkel.GetNum(); ++i)
		{
			BoneNames.Add(RefSkel.GetBoneName(i));
		}
	}
	return BoneNames;
}

TArray<FString> SPairedAnimationPreview::GetActiveNotifies(UAnimMontage* Montage, float Time) const
{
	TArray<FString> ActiveNotifies;
	if (!Montage) return ActiveNotifies;

	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		float NotifyStart = NotifyEvent.GetTriggerTime();
		float NotifyEnd = NotifyEvent.GetEndTriggerTime();

		if (Time >= NotifyStart && Time <= NotifyEnd)
		{
			FString NotifyName = NotifyEvent.NotifyName.ToString();
			if (NotifyName.IsEmpty() && NotifyEvent.NotifyStateClass)
			{
				NotifyName = NotifyEvent.NotifyStateClass->GetName();
			}
			else if (NotifyName.IsEmpty() && NotifyEvent.Notify)
			{
				NotifyName = NotifyEvent.Notify->GetClass()->GetName();
			}
			ActiveNotifies.Add(NotifyName);
		}
	}
	return ActiveNotifies;
}

TArray<FProceduralContactPoint> SPairedAnimationPreview::ComputeContactPoints(float Time)
{
	TArray<FProceduralContactPoint> ContactPoints;

	if (!AttackerMeshComponent || !VictimMeshComponent) return ContactPoints;

	// Update animations to this time
	UpdateAnimations(Time);

	// Get weapon socket positions
	FVector WeaponStart = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponStartSocket);
	FVector WeaponEnd = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponEndSocket);

	// If sockets not found, try hand bones as fallback
	if (WeaponStart.IsZero())
	{
		WeaponStart = GetBoneWorldLocation(AttackerMeshComponent, TEXT("hand_r"));
	}
	if (WeaponEnd.IsZero())
	{
		WeaponEnd = WeaponStart + FVector(50.0f, 0.0f, 0.0f);
	}

	// Sample along the weapon
	const int32 SampleCount = 10;
	for (int32 i = 0; i <= SampleCount; ++i)
	{
		float Alpha = static_cast<float>(i) / SampleCount;
		FVector SamplePoint = FMath::Lerp(WeaponStart, WeaponEnd, Alpha);

		// Find closest victim bone
		float ClosestDist = FLT_MAX;
		FName ClosestVictimBone = FindClosestBone(VictimMeshComponent, SamplePoint, ClosestDist);

		if (ClosestDist < ContactThreshold)
		{
			FProceduralContactPoint Contact;
			Contact.WorldLocation = SamplePoint;
			Contact.Distance = ClosestDist;
			Contact.Confidence = 1.0f - (ClosestDist / ContactThreshold);
			Contact.AttackerBone = (i == 0) ? AttackerConfig.WeaponStartSocket :
								   (i == SampleCount) ? AttackerConfig.WeaponEndSocket : TEXT("WeaponMid");
			Contact.VictimBone = ClosestVictimBone;
			Contact.ContactTime = Time;
			Contact.bIsActiveContact = (ClosestDist < ContactThreshold * 0.5f);

			// Compute velocity and impact direction
			FVector Velocity = ComputeBoneVelocity(AttackerMeshComponent, TEXT("hand_r"), Time);
			Contact.AttackerVelocity = Velocity;
			Contact.ImpactSpeed = Velocity.Size();
			Contact.ImpactDirection = Velocity.GetSafeNormal();

			// Compute contact normal (from victim bone to contact point)
			FVector VictimBonePos = GetBoneWorldLocation(VictimMeshComponent, ClosestVictimBone);
			Contact.ContactNormal = (SamplePoint - VictimBonePos).GetSafeNormal();

			// Quality metrics
			float DotAngle = FMath::Abs(FVector::DotProduct(Contact.ImpactDirection, Contact.ContactNormal));
			Contact.AngleQuality = DotAngle;  // 1.0 = perpendicular impact

			// Position quality - how centered on victim
			FVector VictimCenter = VictimMeshComponent->GetComponentLocation();
			float DistFromCenter = FVector::Dist(SamplePoint, VictimCenter);
			Contact.PositionQuality = FMath::Clamp(1.0f - (DistFromCenter / 200.0f), 0.0f, 1.0f);

			ContactPoints.Add(Contact);
		}
	}

	// Sort by confidence
	ContactPoints.Sort([](const FProceduralContactPoint& A, const FProceduralContactPoint& B)
	{
		return A.Confidence > B.Confidence;
	});

	return ContactPoints;
}

TArray<FProceduralContactPoint> SPairedAnimationPreview::PredictFutureContacts(float InCurrentTime, float LookAheadTime)
{
	TArray<FProceduralContactPoint> PredictedContacts;

	const float TimeStep = 0.016f;  // ~60fps
	for (float t = InCurrentTime; t <= InCurrentTime + LookAheadTime; t += TimeStep)
	{
		TArray<FProceduralContactPoint> FrameContacts = ComputeContactPoints(t);
		for (FProceduralContactPoint& Contact : FrameContacts)
		{
			Contact.bIsPredictedContact = (t > InCurrentTime);
			if (Contact.Confidence > 0.5f)
			{
				PredictedContacts.Add(Contact);
			}
		}
	}

	return PredictedContacts;
}

FPairedFrameAnalysis SPairedAnimationPreview::AnalyzeFrame(float Time)
{
	FPairedFrameAnalysis Analysis;
	Analysis.Time = Time;

	if (!AttackerMeshComponent || !VictimMeshComponent) return Analysis;

	// Update animations
	UpdateAnimations(Time);

	// Character positions
	Analysis.AttackerLocation = AttackerMeshComponent->GetComponentLocation();
	Analysis.VictimLocation = VictimMeshComponent->GetComponentLocation();
	Analysis.AttackerRotation = AttackerMeshComponent->GetComponentRotation();
	Analysis.VictimRotation = VictimMeshComponent->GetComponentRotation();
	Analysis.CharacterDistance = FVector::Dist(Analysis.AttackerLocation, Analysis.VictimLocation);

	// Facing angle
	FVector AttackerForward = Analysis.AttackerRotation.Vector();
	FVector VictimForward = Analysis.VictimRotation.Vector();
	Analysis.FacingAngle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(AttackerForward, VictimForward)));

	// Weapon state
	Analysis.WeaponStartPos = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponStartSocket);
	Analysis.WeaponEndPos = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponEndSocket);
	if (Analysis.WeaponStartPos.IsZero())
	{
		Analysis.WeaponStartPos = GetBoneWorldLocation(AttackerMeshComponent, TEXT("hand_r"));
		Analysis.WeaponEndPos = Analysis.WeaponStartPos + FVector(50.0f, 0.0f, 0.0f);
	}

	Analysis.WeaponVelocity = ComputeBoneVelocity(AttackerMeshComponent, TEXT("hand_r"), Time);
	Analysis.WeaponSpeed = Analysis.WeaponVelocity.Size();

	// Contact points
	Analysis.ContactPoints = ComputeContactPoints(Time);
	if (Analysis.ContactPoints.Num() > 0)
	{
		Analysis.PrimaryContact = Analysis.ContactPoints[0];
	}

	// Closest bones
	Analysis.ClosestBoneDistance = ComputeClosestSkeletonDistance(
		Analysis.AttackerClosestBone, Analysis.VictimClosestBone);

	// Centers of mass
	Analysis.AttackerCOM = ComputeCenterOfMass(AttackerMeshComponent);
	Analysis.VictimCOM = ComputeCenterOfMass(VictimMeshComponent);

	// Active notifies
	if (AttackerMontage.IsValid())
	{
		Analysis.AttackerActiveNotifies = GetActiveNotifies(AttackerMontage.Get(), GetAttackerTime());
	}
	if (VictimMontage.IsValid())
	{
		Analysis.VictimActiveNotifies = GetActiveNotifies(VictimMontage.Get(), GetVictimTime());
	}

	return Analysis;
}

FPairedFrameAnalysis SPairedAnimationPreview::GetAnalysisAtTime(float Time) const
{
	if (FrameAnalysisCache.Num() == 0)
	{
		return FPairedFrameAnalysis();
	}

	// Find closest cached frame
	int32 Index = FMath::Clamp(
		FMath::RoundToInt(Time * AnalysisSampleRate),
		0,
		FrameAnalysisCache.Num() - 1);

	return FrameAnalysisCache[Index];
}

void SPairedAnimationPreview::RebuildAnalysisCache()
{
	FrameAnalysisCache.Empty();

	if (MaxDuration <= 0.0f) return;

	float TimeStep = 1.0f / AnalysisSampleRate;
	for (float t = 0.0f; t <= MaxDuration; t += TimeStep)
	{
		FrameAnalysisCache.Add(AnalyzeFrame(t));
	}

	RebuildDistanceAnalysis();
	RebuildTimingAnalysis();
	bAnalysisCacheDirty = false;
}

void SPairedAnimationPreview::RebuildTrajectoryCache()
{
	AttackerTrajectories.Empty();
	VictimTrajectories.Empty();

	if (MaxDuration <= 0.0f) return;

	// Track key bones
	TArray<FName> TrackedBones = {
		TEXT("hand_r"), TEXT("hand_l"),
		TEXT("foot_r"), TEXT("foot_l"),
		TEXT("head"), TEXT("pelvis")
	};

	float TimeStep = MaxDuration / TrajectorySampleCount;

	for (const FName& BoneName : TrackedBones)
	{
		// Attacker trajectory
		if (AttackerMeshComponent)
		{
			FBoneTrajectory AttackerTraj;
			AttackerTraj.BoneName = BoneName;
			AttackerTraj.TrajectoryColor = AttackerColor;

			for (int32 i = 0; i <= TrajectorySampleCount; ++i)
			{
				float t = i * TimeStep;
				UpdateAnimations(t);

				FVector Pos = GetBoneWorldLocation(AttackerMeshComponent, BoneName);
				FVector Vel = ComputeBoneVelocity(AttackerMeshComponent, BoneName, t);

				AttackerTraj.Positions.Add(Pos);
				AttackerTraj.Velocities.Add(Vel);
				AttackerTraj.Speeds.Add(Vel.Size());

				if (Vel.Size() > AttackerTraj.MaxSpeed)
				{
					AttackerTraj.MaxSpeed = Vel.Size();
					AttackerTraj.MaxSpeedTime = t;
					AttackerTraj.MaxSpeedSampleIndex = i;
				}
			}
			AttackerTrajectories.Add(AttackerTraj);
		}

		// Victim trajectory
		if (VictimMeshComponent)
		{
			FBoneTrajectory VictimTraj;
			VictimTraj.BoneName = BoneName;
			VictimTraj.TrajectoryColor = VictimColor;

			for (int32 i = 0; i <= TrajectorySampleCount; ++i)
			{
				float t = i * TimeStep;
				UpdateAnimations(t);

				FVector Pos = GetBoneWorldLocation(VictimMeshComponent, BoneName);
				FVector Vel = ComputeBoneVelocity(VictimMeshComponent, BoneName, t);

				VictimTraj.Positions.Add(Pos);
				VictimTraj.Velocities.Add(Vel);
				VictimTraj.Speeds.Add(Vel.Size());

				if (Vel.Size() > VictimTraj.MaxSpeed)
				{
					VictimTraj.MaxSpeed = Vel.Size();
					VictimTraj.MaxSpeedTime = t;
					VictimTraj.MaxSpeedSampleIndex = i;
				}
			}
			VictimTrajectories.Add(VictimTraj);
		}
	}
}

void SPairedAnimationPreview::RebuildDistanceAnalysis()
{
	DistanceAnalysis = FDistanceAnalysis();

	for (const FPairedFrameAnalysis& Frame : FrameAnalysisCache)
	{
		DistanceAnalysis.CenterDistances.Add(Frame.CharacterDistance);
		DistanceAnalysis.ClosestBoneDistances.Add(Frame.ClosestBoneDistance);
		DistanceAnalysis.ClosestBonePairs.Add(
			TPair<FName, FName>(Frame.AttackerClosestBone, Frame.VictimClosestBone));

		if (Frame.CharacterDistance < DistanceAnalysis.MinDistance)
		{
			DistanceAnalysis.MinDistance = Frame.CharacterDistance;
			DistanceAnalysis.MinDistanceTime = Frame.Time;
		}
		if (Frame.CharacterDistance > DistanceAnalysis.MaxDistance)
		{
			DistanceAnalysis.MaxDistance = Frame.CharacterDistance;
			DistanceAnalysis.MaxDistanceTime = Frame.Time;
		}
	}
}

void SPairedAnimationPreview::RebuildTimingAnalysis()
{
	TimingAnalysis = FTimingAnalysis();

	float BestConfidence = 0.0f;
	for (const FPairedFrameAnalysis& Frame : FrameAnalysisCache)
	{
		if (Frame.PrimaryContact.Confidence > 0.5f)
		{
			TimingAnalysis.NaturalSyncTimes.Add(Frame.Time);
			TimingAnalysis.SyncConfidences.Add(Frame.PrimaryContact.Confidence);

			if (Frame.PrimaryContact.Confidence > BestConfidence)
			{
				BestConfidence = Frame.PrimaryContact.Confidence;
				TimingAnalysis.BestSyncTime = Frame.Time;
				TimingAnalysis.BestSyncConfidence = BestConfidence;
			}
		}

		// Detect high activity ranges (weapon speed > threshold)
		if (Frame.WeaponSpeed > 500.0f)
		{
			if (TimingAnalysis.HighActivityRanges.Num() == 0 ||
				Frame.Time - TimingAnalysis.HighActivityRanges.Last().Value > 0.1f)
			{
				TimingAnalysis.HighActivityRanges.Add(TPair<float, float>(Frame.Time, Frame.Time));
			}
			else
			{
				TimingAnalysis.HighActivityRanges.Last().Value = Frame.Time;
			}
		}
	}
}

// ============================================================================
// OPTIMIZATION ENGINE
// ============================================================================

float SPairedAnimationPreview::EvaluateConfiguration(float Distance, FRotator AttackerRot, FRotator VictimRot)
{
	// Temporarily apply configuration
	float OriginalDistance = LockedDistance;
	FRotator OriginalAttackerRot = AttackerConfig.RotationOffset;
	FRotator OriginalVictimRot = VictimConfig.RotationOffset;

	LockedDistance = Distance;
	AttackerConfig.RotationOffset = AttackerRot;
	VictimConfig.RotationOffset = VictimRot;
	ApplyCharacterConfigs();

	// Evaluate over entire animation
	float TotalScore = 0.0f;
	int32 SampleCount = 0;
	float BestContactConfidence = 0.0f;

	float TimeStep = MaxDuration / 30.0f;  // Quick sampling
	for (float t = 0.0f; t <= MaxDuration; t += TimeStep)
	{
		FPairedFrameAnalysis Frame = AnalyzeFrame(t);
		if (Frame.PrimaryContact.Confidence > BestContactConfidence)
		{
			BestContactConfidence = Frame.PrimaryContact.Confidence;
		}
		TotalScore += Frame.PrimaryContact.Confidence;
		SampleCount++;
	}

	// Restore original
	LockedDistance = OriginalDistance;
	AttackerConfig.RotationOffset = OriginalAttackerRot;
	VictimConfig.RotationOffset = OriginalVictimRot;
	ApplyCharacterConfigs();

	return (SampleCount > 0) ? (TotalScore / SampleCount + BestContactConfidence) * 0.5f : 0.0f;
}

float SPairedAnimationPreview::FindOptimalDistance(float MinDist, float MaxDist, int32 Steps)
{
	float BestDistance = LockedDistance;
	float BestScore = 0.0f;

	float StepSize = (MaxDist - MinDist) / Steps;
	for (float d = MinDist; d <= MaxDist; d += StepSize)
	{
		float Score = EvaluateConfiguration(d, AttackerConfig.RotationOffset, VictimConfig.RotationOffset);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDistance = d;
		}
	}

	return BestDistance;
}

FRotator SPairedAnimationPreview::FindOptimalAttackerRotation(int32 Steps)
{
	FRotator BestRotation = AttackerConfig.RotationOffset;
	float BestScore = 0.0f;

	float AngleStep = 360.0f / Steps;
	for (float Yaw = 0.0f; Yaw < 360.0f; Yaw += AngleStep)
	{
		FRotator TestRot(0.0f, Yaw, 0.0f);
		float Score = EvaluateConfiguration(LockedDistance, TestRot, VictimConfig.RotationOffset);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestRotation = TestRot;
		}
	}

	return BestRotation;
}

FRotator SPairedAnimationPreview::FindOptimalVictimRotation(int32 Steps)
{
	FRotator BestRotation = VictimConfig.RotationOffset;
	float BestScore = 0.0f;

	float AngleStep = 360.0f / Steps;
	for (float Yaw = 0.0f; Yaw < 360.0f; Yaw += AngleStep)
	{
		FRotator TestRot(0.0f, Yaw, 0.0f);
		float Score = EvaluateConfiguration(LockedDistance, AttackerConfig.RotationOffset, TestRot);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestRotation = TestRot;
		}
	}

	return BestRotation;
}

float SPairedAnimationPreview::FindOptimalSyncTime()
{
	RebuildAnalysisCache();
	return TimingAnalysis.BestSyncTime;
}

FOptimizationResult SPairedAnimationPreview::RunFullOptimization()
{
	FOptimizationResult Result;

	if (!AttackerMontage.IsValid() || !VictimMontage.IsValid())
	{
		Result.bSuccess = false;
		Result.Warnings.Add(TEXT("Both attacker and victim montages must be loaded"));
		return Result;
	}

	// Phase 1: Find optimal distance
	Result.RecommendedDistance = FindOptimalDistance(50.0f, 400.0f, 50);

	// Phase 2: Find optimal attacker rotation at that distance
	LockedDistance = Result.RecommendedDistance;
	ApplyCharacterConfigs();
	Result.RecommendedAttackerRotation = FindOptimalAttackerRotation(36);

	// Phase 3: Find optimal victim rotation
	AttackerConfig.RotationOffset = Result.RecommendedAttackerRotation;
	ApplyCharacterConfigs();
	Result.RecommendedVictimRotation = FindOptimalVictimRotation(36);

	// Phase 4: Find optimal sync time
	VictimConfig.RotationOffset = Result.RecommendedVictimRotation;
	ApplyCharacterConfigs();
	RebuildAnalysisCache();
	Result.RecommendedSyncTime = TimingAnalysis.BestSyncTime;

	// Compute quality scores
	Result.ContactQuality = TimingAnalysis.BestSyncConfidence;
	Result.AlignmentQuality = FMath::Abs(FVector::DotProduct(
		AttackerConfig.RotationOffset.Vector(),
		-VictimConfig.RotationOffset.Vector()));

	FPairedFrameAnalysis SyncFrame = AnalyzeFrame(Result.RecommendedSyncTime);
	Result.TimingQuality = (SyncFrame.WeaponSpeed > 100.0f) ? 1.0f : SyncFrame.WeaponSpeed / 100.0f;

	Result.OverallScore = (Result.ContactQuality + Result.AlignmentQuality + Result.TimingQuality) / 3.0f;
	Result.bSuccess = true;

	// Generate suggestions
	if (Result.ContactQuality < 0.5f)
	{
		Result.Suggestions.Add(TEXT("Contact confidence is low. Consider adjusting weapon sockets or using different animations."));
	}
	if (Result.RecommendedDistance < 80.0f)
	{
		Result.Warnings.Add(TEXT("Characters are very close. May cause mesh interpenetration."));
	}
	if (Result.RecommendedDistance > 300.0f)
	{
		Result.Warnings.Add(TEXT("Characters are far apart. Motion warp distance may be excessive."));
	}

	LastOptimizationResult = Result;
	return Result;
}

void SPairedAnimationPreview::ApplyOptimizationResult(const FOptimizationResult& Result)
{
	if (!Result.bSuccess) return;

	LockedDistance = Result.RecommendedDistance;
	AttackerConfig.RotationOffset = Result.RecommendedAttackerRotation;
	VictimConfig.RotationOffset = Result.RecommendedVictimRotation;
	ApplyCharacterConfigs();

	CurrentTime = Result.RecommendedSyncTime;
	UpdateAnimations(CurrentTime);

	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::OnOptimizeClicked()
{
	// Validate we have what we need
	if (!AttackerMontage.IsValid() || !VictimMontage.IsValid() ||
		!AttackerMeshComponent || !VictimMeshComponent)
	{
		return;
	}

	// Clear all caches for fresh optimization
	bAnalysisCacheDirty = true;
	FrameAnalysisCache.Empty();
	AttackerTrajectories.Empty();
	VictimTrajectories.Empty();

	FOptimizationResult Result = RunFullOptimization();
	ApplyOptimizationResult(Result);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnFindOptimalDistanceClicked()
{
	// Validate we have what we need
	if (!AttackerMontage.IsValid() || !VictimMontage.IsValid() ||
		!AttackerMeshComponent || !VictimMeshComponent)
	{
		return;
	}

	// Clear caches to ensure fresh evaluation
	bAnalysisCacheDirty = true;
	FrameAnalysisCache.Empty();

	float OptimalDist = FindOptimalDistance();
	LockedDistance = OptimalDist;
	ApplyCharacterConfigs();
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnFindOptimalRotationClicked()
{
	// Validate we have what we need
	if (!AttackerMontage.IsValid() || !VictimMontage.IsValid() ||
		!AttackerMeshComponent || !VictimMeshComponent)
	{
		return;
	}

	// Clear caches to ensure fresh evaluation
	bAnalysisCacheDirty = true;
	FrameAnalysisCache.Empty();

	AttackerConfig.RotationOffset = FindOptimalAttackerRotation();
	VictimConfig.RotationOffset = FindOptimalVictimRotation();
	ApplyCharacterConfigs();
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnFindOptimalSyncClicked()
{
	// Validate we have what we need
	if (!AttackerMontage.IsValid() || !VictimMontage.IsValid() ||
		!AttackerMeshComponent || !VictimMeshComponent)
	{
		return;
	}

	// Force rebuild of analysis cache
	bAnalysisCacheDirty = true;
	FrameAnalysisCache.Empty();

	float SyncTime = FindOptimalSyncTime();
	CurrentTime = SyncTime;
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

// ============================================================================
// VISUALIZATION
// ============================================================================

bool SPairedAnimationPreview::IsVisualizationActive(EVisualizationLayer Layer) const
{
	return EnumHasAnyFlags(ActiveVisualizationLayers, Layer);
}

void SPairedAnimationPreview::SetVisualizationActive(EVisualizationLayer Layer, bool bActive)
{
	if (bActive)
	{
		ActiveVisualizationLayers |= Layer;
	}
	else
	{
		ActiveVisualizationLayers &= ~Layer;
	}
}

void SPairedAnimationPreview::DrawDebugVisualization()
{
	if (!SharedPreviewScene) return;
	UWorld* World = SharedPreviewScene->GetWorld();
	if (!World) return;

	// Flush previous debug draws
	FlushPersistentDebugLines(World);

	if (IsVisualizationActive(EVisualizationLayer::ContactPoints)) DrawContactPoints();
	if (IsVisualizationActive(EVisualizationLayer::WeaponTrace)) DrawWeaponTrace();
	if (IsVisualizationActive(EVisualizationLayer::DistanceLines)) DrawDistanceLines();
	if (IsVisualizationActive(EVisualizationLayer::CenterOfMass)) DrawCenterOfMass();
	if (IsVisualizationActive(EVisualizationLayer::VelocityVectors)) DrawVelocityVectors();
}

void SPairedAnimationPreview::DrawSkeletons()
{
	// Skeleton drawing handled by UDebugSkelMeshComponent naturally
}

void SPairedAnimationPreview::DrawBoneNames()
{
	// Would require 3D text rendering - complex for this context
}

void SPairedAnimationPreview::DrawVelocityVectors()
{
	if (!SharedPreviewScene) return;
	UWorld* World = SharedPreviewScene->GetWorld();

	FPairedFrameAnalysis Analysis = AnalyzeFrame(CurrentTime);

	// Draw weapon velocity
	if (!Analysis.WeaponStartPos.IsZero())
	{
		FVector VelEnd = Analysis.WeaponStartPos + Analysis.WeaponVelocity.GetSafeNormal() * 50.0f;
		DrawDebugDirectionalArrow(World, Analysis.WeaponStartPos, VelEnd,
			10.0f, FColor::Cyan, false, -1.0f, 0, 2.0f);
	}
}

void SPairedAnimationPreview::DrawContactPoints()
{
	if (!SharedPreviewScene) return;
	UWorld* World = SharedPreviewScene->GetWorld();

	FPairedFrameAnalysis Analysis = AnalyzeFrame(CurrentTime);

	for (const FProceduralContactPoint& Contact : Analysis.ContactPoints)
	{
		// Contact sphere - size based on confidence
		float SphereRadius = 5.0f + Contact.Confidence * 10.0f;
		FColor SphereColor = Contact.bIsActiveContact ?
			FColor::Yellow : FColor(255, 200, 50);

		DrawDebugSphere(World, Contact.WorldLocation, SphereRadius,
			12, SphereColor, false, -1.0f, 0, 1.0f);

		// Contact normal
		FVector NormalEnd = Contact.WorldLocation + Contact.ContactNormal * 30.0f;
		DrawDebugDirectionalArrow(World, Contact.WorldLocation, NormalEnd,
			5.0f, FColor::Blue, false, -1.0f, 0, 1.0f);

		// Impact direction
		if (Contact.ImpactSpeed > 10.0f)
		{
			FVector ImpactEnd = Contact.WorldLocation - Contact.ImpactDirection * 30.0f;
			DrawDebugDirectionalArrow(World, ImpactEnd, Contact.WorldLocation,
				5.0f, FColor::Red, false, -1.0f, 0, 1.5f);
		}
	}

	// Primary contact highlight
	if (Analysis.ContactPoints.Num() > 0)
	{
		DrawDebugSphere(World, Analysis.PrimaryContact.WorldLocation, 20.0f,
			16, FColor::Green, false, -1.0f, 0, 2.0f);
	}
}

void SPairedAnimationPreview::DrawContactTrails()
{
	if (!SharedPreviewScene || FrameAnalysisCache.Num() < 2) return;
	UWorld* World = SharedPreviewScene->GetWorld();

	// Draw trail of primary contact points
	for (int32 i = 1; i < FrameAnalysisCache.Num(); ++i)
	{
		const FProceduralContactPoint& PrevContact = FrameAnalysisCache[i - 1].PrimaryContact;
		const FProceduralContactPoint& CurrContact = FrameAnalysisCache[i].PrimaryContact;

		if (PrevContact.Confidence > 0.3f && CurrContact.Confidence > 0.3f)
		{
			DrawDebugLine(World, PrevContact.WorldLocation, CurrContact.WorldLocation,
				FColor::Orange, false, -1.0f, 0, 1.0f);
		}
	}
}

void SPairedAnimationPreview::DrawWeaponTrace()
{
	if (!SharedPreviewScene || !AttackerMeshComponent) return;
	UWorld* World = SharedPreviewScene->GetWorld();

	FVector WeaponStart = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponStartSocket);
	FVector WeaponEnd = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponEndSocket);

	if (WeaponStart.IsZero())
	{
		WeaponStart = GetBoneWorldLocation(AttackerMeshComponent, TEXT("hand_r"));
		WeaponEnd = WeaponStart + FVector(50.0f, 0.0f, 0.0f);
	}

	// Draw weapon line
	DrawDebugLine(World, WeaponStart, WeaponEnd, FColor::Red, false, -1.0f, 0, 3.0f);

	// Draw socket markers
	DrawDebugSphere(World, WeaponStart, 5.0f, 8, FColor::Green, false, -1.0f, 0, 1.0f);
	DrawDebugSphere(World, WeaponEnd, 5.0f, 8, FColor::Red, false, -1.0f, 0, 1.0f);
}

void SPairedAnimationPreview::DrawRootMotionPath()
{
	// Would show accumulated root motion path - TODO
}

void SPairedAnimationPreview::DrawDistanceLines()
{
	if (!SharedPreviewScene || !AttackerMeshComponent || !VictimMeshComponent) return;
	UWorld* World = SharedPreviewScene->GetWorld();

	FVector AttackerPos = AttackerMeshComponent->GetComponentLocation();
	FVector VictimPos = VictimMeshComponent->GetComponentLocation();

	// Draw distance line
	DrawDebugLine(World, AttackerPos, VictimPos, FColor::White, false, -1.0f, 0, 1.0f);

	// Draw midpoint
	FVector Midpoint = (AttackerPos + VictimPos) * 0.5f;
	DrawDebugSphere(World, Midpoint, 5.0f, 8, FColor::White, false, -1.0f, 0, 1.0f);
}

void SPairedAnimationPreview::DrawReachEnvelopes()
{
	// Would show arm reach spheres - TODO
}

void SPairedAnimationPreview::DrawCenterOfMass()
{
	if (!SharedPreviewScene) return;
	UWorld* World = SharedPreviewScene->GetWorld();

	FPairedFrameAnalysis Analysis = AnalyzeFrame(CurrentTime);

	DrawDebugSphere(World, Analysis.AttackerCOM, 8.0f, 8, FColor::Blue, false, -1.0f, 0, 1.5f);
	DrawDebugSphere(World, Analysis.VictimCOM, 8.0f, 8, FColor::Orange, false, -1.0f, 0, 1.5f);
}

void SPairedAnimationPreview::DrawCollisionBounds()
{
	// Would show capsule collision bounds - TODO
}

// ============================================================================
// PLAYBACK CONTROLS
// ============================================================================

void SPairedAnimationPreview::OnTimelineValueChanged(float NewValue)
{
	CurrentTime = NewValue * MaxDuration;
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnPlayPauseClicked()
{
	bIsPlaying = !bIsPlaying;

	// Just toggle state - DON'T call Stop() as it causes animation fighting
	// The Tick() function handles playback, UpdateAnimations() handles position
	// When paused, we simply don't advance CurrentTime in Tick()
	if (!bIsPlaying)
	{
		// Force update to ensure we're at exact frame
		UpdateAnimations(CurrentTime);
	}
}

void SPairedAnimationPreview::OnStepForward()
{
	CurrentTime = FMath::Min(CurrentTime + (1.0f / 60.0f), MaxDuration);
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnStepBackward()
{
	CurrentTime = FMath::Max(CurrentTime - (1.0f / 60.0f), 0.0f);
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnStepForwardLarge()
{
	CurrentTime = FMath::Min(CurrentTime + 0.1f, MaxDuration);
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnStepBackwardLarge()
{
	CurrentTime = FMath::Max(CurrentTime - 0.1f, 0.0f);
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnResetClicked()
{
	CurrentTime = 0.0f;
	bIsPlaying = false;

	// Reinitialize animations if they exist
	// Best practice: Just set mode, animation, and position - no Stop() calls
	if (AttackerMeshComponent && AttackerMontage.IsValid())
	{
		AttackerMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		AttackerMeshComponent->SetAnimation(AttackerMontage.Get());
		AttackerMeshComponent->SetPosition(0.0f);
	}

	if (VictimMeshComponent && VictimMontage.IsValid())
	{
		VictimMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
		VictimMeshComponent->SetAnimation(VictimMontage.Get());
		VictimMeshComponent->SetPosition(0.0f);
	}

	// Reset config positions
	AttackerConfig.PositionOffset = FVector::ZeroVector;
	VictimConfig.PositionOffset = FVector(LockedDistance, 0.0f, 0.0f);
	ApplyCharacterConfigs();

	// Clear caches to force rebuild
	bAnalysisCacheDirty = true;
	FrameAnalysisCache.Empty();
	AttackerTrajectories.Empty();
	VictimTrajectories.Empty();

	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnGoToMaxContactClicked()
{
	if (bAnalysisCacheDirty)
	{
		RebuildAnalysisCache();
	}
	CurrentTime = TimingAnalysis.BestSyncTime;
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnGoToMaxSpeedClicked()
{
	if (AttackerTrajectories.Num() == 0)
	{
		RebuildTrajectoryCache();
	}

	// Find hand trajectory and go to max speed time
	for (const FBoneTrajectory& Traj : AttackerTrajectories)
	{
		if (Traj.BoneName == TEXT("hand_r"))
		{
			CurrentTime = Traj.MaxSpeedTime;
			UpdateAnimations(CurrentTime);
			UpdateAnalyticsDisplay();
			break;
		}
	}
}

void SPairedAnimationPreview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	// Advance time if playing
	if (bIsPlaying && MaxDuration > 0.0f)
	{
		float TimeAdvance = InDeltaTime * PlaybackSpeed;

		if (bPingPongPlayback)
		{
			CurrentTime += TimeAdvance * PingPongDirection;
			if (CurrentTime >= MaxDuration)
			{
				CurrentTime = MaxDuration;
				PingPongDirection = -1;
			}
			else if (CurrentTime <= 0.0f)
			{
				CurrentTime = 0.0f;
				PingPongDirection = 1;
			}
		}
		else
		{
			CurrentTime += TimeAdvance;
			if (CurrentTime >= MaxDuration)
			{
				if (bLoopPlayback)
				{
					CurrentTime = 0.0f;
				}
				else
				{
					CurrentTime = MaxDuration;
					bIsPlaying = false;
				}
			}
		}

		UpdateAnimations(CurrentTime);
		UpdateAnalyticsDisplay();
	}

	// CRITICAL: Tick the preview scene to update skeletal mesh animations
	// This is the best practice from UE5 documentation - the preview scene
	// handles animation updates when we call SetPosition()
	if (SharedPreviewScene)
	{
		SharedPreviewScene->GetWorld()->Tick(LEVELTICK_ViewportsOnly, InDeltaTime);
	}

	// Always draw visualization
	DrawDebugVisualization();

	// Update slider position
	if (TimelineSlider.IsValid() && MaxDuration > 0.0f)
	{
		TimelineSlider->SetValue(CurrentTime / MaxDuration);
	}
}

// ============================================================================
// TEXT DISPLAYS
// ============================================================================

void SPairedAnimationPreview::UpdateAnalyticsDisplay()
{
	if (TimeDisplayText.IsValid())
	{
		TimeDisplayText->SetText(GetTimeDisplayText());
	}
	if (ContactInfoText.IsValid())
	{
		ContactInfoText->SetText(GetContactInfoText());
	}
	if (DistanceInfoText.IsValid())
	{
		DistanceInfoText->SetText(GetDistanceInfoText());
	}
	if (VelocityInfoText.IsValid())
	{
		VelocityInfoText->SetText(GetVelocityInfoText());
	}
	if (OptimizationInfoText.IsValid())
	{
		OptimizationInfoText->SetText(GetOptimizationInfoText());
	}
}

FText SPairedAnimationPreview::GetTimeDisplayText() const
{
	return FText::Format(
		LOCTEXT("TimeDisplay", "{0} / {1}s  |  Attacker: {2}s  Victim: {3}s"),
		FText::AsNumber(CurrentTime, &GetNumberFormat2()),
		FText::AsNumber(MaxDuration, &GetNumberFormat2()),
		FText::AsNumber(GetAttackerTime(), &GetNumberFormat2()),
		FText::AsNumber(GetVictimTime(), &GetNumberFormat2()));
}

FText SPairedAnimationPreview::GetContactInfoText() const
{
	FPairedFrameAnalysis Analysis = const_cast<SPairedAnimationPreview*>(this)->AnalyzeFrame(CurrentTime);

	if (Analysis.ContactPoints.Num() == 0)
	{
		return LOCTEXT("NoContact", "No contacts detected");
	}

	const FProceduralContactPoint& Primary = Analysis.PrimaryContact;
	return FText::Format(
		LOCTEXT("ContactInfo", "Contacts: {0}  |  Best: {1} ({2}%)  |  Dist: {3}u  |  Impact: {4} u/s"),
		FText::AsNumber(Analysis.ContactPoints.Num()),
		FText::FromName(Primary.VictimBone),
		FText::AsNumber(Primary.Confidence * 100.0f, &GetNumberFormat0()),
		FText::AsNumber(Primary.Distance, &GetNumberFormat1()),
		FText::AsNumber(Primary.ImpactSpeed, &GetNumberFormat0()));
}

FText SPairedAnimationPreview::GetDistanceInfoText() const
{
	FPairedFrameAnalysis Analysis = const_cast<SPairedAnimationPreview*>(this)->AnalyzeFrame(CurrentTime);

	return FText::Format(
		LOCTEXT("DistanceInfo", "Center Dist: {0}u  |  Bone Dist: {1}u ({2} ↔ {3})"),
		FText::AsNumber(Analysis.CharacterDistance, &GetNumberFormat1()),
		FText::AsNumber(Analysis.ClosestBoneDistance, &GetNumberFormat1()),
		FText::FromName(Analysis.AttackerClosestBone),
		FText::FromName(Analysis.VictimClosestBone));
}

FText SPairedAnimationPreview::GetVelocityInfoText() const
{
	FPairedFrameAnalysis Analysis = const_cast<SPairedAnimationPreview*>(this)->AnalyzeFrame(CurrentTime);

	return FText::Format(
		LOCTEXT("VelocityInfo", "Weapon Speed: {0} u/s  |  Direction: ({1}, {2}, {3})"),
		FText::AsNumber(Analysis.WeaponSpeed, &GetNumberFormat0()),
		FText::AsNumber(Analysis.WeaponVelocity.X, &GetNumberFormat0()),
		FText::AsNumber(Analysis.WeaponVelocity.Y, &GetNumberFormat0()),
		FText::AsNumber(Analysis.WeaponVelocity.Z, &GetNumberFormat0()));
}

FText SPairedAnimationPreview::GetOptimizationInfoText() const
{
	if (!LastOptimizationResult.bSuccess)
	{
		return LOCTEXT("NoOptimization", "Run optimization to get recommendations");
	}

	return FText::Format(
		LOCTEXT("OptimizationInfo", "Score: {0}%  |  Dist: {1}u  |  Sync: {2}s  |  Contact: {3}%"),
		FText::AsNumber(LastOptimizationResult.OverallScore * 100.0f, &GetNumberFormat0()),
		FText::AsNumber(LastOptimizationResult.RecommendedDistance, &GetNumberFormat0()),
		FText::AsNumber(LastOptimizationResult.RecommendedSyncTime, &GetNumberFormat2()),
		FText::AsNumber(LastOptimizationResult.ContactQuality * 100.0f, &GetNumberFormat0()));
}

FText SPairedAnimationPreview::GetStatusText() const
{
	if (!AttackerMontage.IsValid() || !VictimMontage.IsValid())
	{
		return LOCTEXT("Status_LoadMontages", "Load attacker and victim montages to begin");
	}
	if (bIsPlaying)
	{
		return LOCTEXT("Status_Playing", "Playing...");
	}
	return LOCTEXT("Status_Ready", "Ready");
}

// ============================================================================
// PRESETS
// ============================================================================

void SPairedAnimationPreview::ApplyPreset_Finisher()
{
	LockedDistance = 150.0f;
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator(0.0f, 180.0f, 0.0f);
	ContactThreshold = 50.0f;
	ApplyCharacterConfigs();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::ApplyPreset_Counter()
{
	LockedDistance = 120.0f;
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator(0.0f, 180.0f, 0.0f);
	ContactThreshold = 40.0f;
	ApplyCharacterConfigs();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::ApplyPreset_Parry()
{
	LockedDistance = 100.0f;
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator(0.0f, 180.0f, 0.0f);
	ContactThreshold = 30.0f;
	ApplyCharacterConfigs();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::SaveCurrentAsPreset(const FString& PresetName)
{
	// TODO: Save to config file
}

void SPairedAnimationPreview::LoadPreset(const FString& PresetName)
{
	// TODO: Load from config file
}

// ============================================================================
// EXPORT
// ============================================================================

void SPairedAnimationPreview::ExportAnalysisToCSV()
{
	if (bAnalysisCacheDirty)
	{
		RebuildAnalysisCache();
	}

	FString CSVContent = TEXT("Time,CharacterDistance,ClosestBoneDistance,ContactConfidence,WeaponSpeed,AttackerBone,VictimBone\n");

	for (const FPairedFrameAnalysis& Frame : FrameAnalysisCache)
	{
		CSVContent += FString::Printf(TEXT("%.3f,%.1f,%.1f,%.2f,%.0f,%s,%s\n"),
			Frame.Time,
			Frame.CharacterDistance,
			Frame.ClosestBoneDistance,
			Frame.PrimaryContact.Confidence,
			Frame.WeaponSpeed,
			*Frame.AttackerClosestBone.ToString(),
			*Frame.VictimClosestBone.ToString());
	}

	FPlatformApplicationMisc::ClipboardCopy(*CSVContent);
}

void SPairedAnimationPreview::ExportAnalysisToJSON()
{
	// TODO: Full JSON export
}

void SPairedAnimationPreview::CopyAnalysisToClipboard()
{
	FPairedFrameAnalysis Analysis = AnalyzeFrame(CurrentTime);

	FString ClipboardText = FString::Printf(
		TEXT("Paired Animation Analysis @ %.3fs\n")
		TEXT("=====================================\n")
		TEXT("Distance: %.1f units\n")
		TEXT("Closest Bones: %s <-> %s (%.1f units)\n")
		TEXT("Contact Confidence: %.0f%%\n")
		TEXT("Weapon Speed: %.0f u/s\n")
		TEXT("Active Contacts: %d\n"),
		CurrentTime,
		Analysis.CharacterDistance,
		*Analysis.AttackerClosestBone.ToString(),
		*Analysis.VictimClosestBone.ToString(),
		Analysis.ClosestBoneDistance,
		Analysis.PrimaryContact.Confidence * 100.0f,
		Analysis.WeaponSpeed,
		Analysis.ContactPoints.Num());

	FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
}

// ============================================================================
// UI CONSTRUCTION
// ============================================================================

TSharedRef<SWidget> SPairedAnimationPreview::BuildMainLayout()
{
	return SNew(SSplitter)
		.Orientation(Orient_Horizontal)

		// Left Panel - Controls
		+ SSplitter::Slot()
		.Value(0.35f)
		[
			SNew(SScrollBox)
			+ SScrollBox::Slot()
			[
				SNew(SVerticalBox)

				// Quick Actions Bar
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					BuildQuickActionsBar()
				]

				// Asset Selection
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					BuildAssetSelectionPanel()
				]

				// Positioning
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					BuildPositioningPanel()
				]

				// Optimization
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					BuildOptimizationPanel()
				]

				// Visualization
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					BuildVisualizationPanel()
				]

				// Settings
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					BuildSettingsPanel()
				]
			]
		]

		// Right Panel - Viewport & Analysis
		+ SSplitter::Slot()
		.Value(0.65f)
		[
			SNew(SVerticalBox)

			// 3D Viewport
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SAssignNew(ViewportWidget, SPairedPreviewViewport)
				.PreviewScene(SharedPreviewScene.Get())
			]

			// Timeline Controls
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				BuildTimelineControls()
			]

			// Analysis Panel
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(4.0f)
			[
				BuildAnalysisPanel()
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildQuickActionsBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AutoOptimize", "Auto-Optimize"))
				.ToolTipText(LOCTEXT("AutoOptimizeTip", "Find optimal distance, rotation, and sync time"))
				.OnClicked_Lambda([this]() { OnOptimizeClicked(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GoToContact", "→ Best Contact"))
				.ToolTipText(LOCTEXT("GoToContactTip", "Jump to frame with best contact"))
				.OnClicked_Lambda([this]() { OnGoToMaxContactClicked(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("GoToSpeed", "→ Max Speed"))
				.ToolTipText(LOCTEXT("GoToSpeedTip", "Jump to frame with max weapon speed"))
				.OnClicked_Lambda([this]() { OnGoToMaxSpeedClicked(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("CopyAnalysis", "Copy"))
				.ToolTipText(LOCTEXT("CopyAnalysisTip", "Copy current frame analysis to clipboard"))
				.OnClicked_Lambda([this]() { CopyAnalysisToClipboard(); return FReply::Handled(); })
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ExportCSV", "Export CSV"))
				.ToolTipText(LOCTEXT("ExportCSVTip", "Export full timeline analysis to CSV"))
				.OnClicked_Lambda([this]() { ExportAnalysisToCSV(); return FReply::Handled(); })
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildAssetSelectionPanel()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("Assets", "Assets"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SNew(SVerticalBox)

			// Attacker Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AttackerLabel", "ATTACKER"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(AttackerColor)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Skeleton", "Mesh"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(USkeletalMesh::StaticClass())
					.ObjectPath_Lambda([this]() { return GetAttackerSkeletonPath(); })
					.OnObjectChanged(this, &SPairedAnimationPreview::OnAttackerSkeletonSelected)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Montage", "Montage"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UAnimMontage::StaticClass())
					.ObjectPath_Lambda([this]() { return GetAttackerMontagePath(); })
					.OnObjectChanged(this, &SPairedAnimationPreview::OnAttackerMontageSelected)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(SSeparator)
			]

			// Victim Section
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VictimLabel", "VICTIM"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 10))
				.ColorAndOpacity(VictimColor)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Skeleton2", "Mesh"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(USkeletalMesh::StaticClass())
					.ObjectPath_Lambda([this]() { return GetVictimSkeletonPath(); })
					.OnObjectChanged(this, &SPairedAnimationPreview::OnVictimSkeletonSelected)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Montage2", "Montage"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UAnimMontage::StaticClass())
					.ObjectPath_Lambda([this]() { return GetVictimMontagePath(); })
					.OnObjectChanged(this, &SPairedAnimationPreview::OnVictimMontageSelected)
				]
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildPositioningPanel()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("Positioning", "Positioning"))
		.InitiallyCollapsed(false)
		.BodyContent()
		[
			SNew(SVerticalBox)

			// Lock toggle
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bLockVictimToAttacker ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) {
						bLockVictimToAttacker = (State == ECheckBoxState::Checked);
						ApplyCharacterConfigs();
					})
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("LockVictim", "Lock victim to attacker"))
				]
			]

			// Distance
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Distance", "Distance"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(10.0f)
					.MaxValue(1000.0f)
					.Value_Lambda([this]() { return LockedDistance; })
					.OnValueChanged_Lambda([this](float Val) { OnLockedDistanceChanged(Val); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("FindOptDist", "Find"))
					.ToolTipText(LOCTEXT("FindOptDistTip", "Find optimal distance"))
					.OnClicked_Lambda([this]() { OnFindOptimalDistanceClicked(); return FReply::Handled(); })
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AttackerPos", "Attacker Rotation (Yaw)"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-180.0f)
				.MaxValue(180.0f)
				.Value_Lambda([this]() { return AttackerConfig.RotationOffset.Yaw; })
				.OnValueChanged_Lambda([this](float Val) {
					OnAttackerRotationChanged(FRotator(0.0f, Val, 0.0f));
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VictimRot", "Victim Rotation (Yaw)"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SSpinBox<float>)
				.MinValue(-180.0f)
				.MaxValue(180.0f)
				.Value_Lambda([this]() { return VictimConfig.RotationOffset.Yaw; })
				.OnValueChanged_Lambda([this](float Val) {
					OnVictimRotationChanged(FRotator(0.0f, Val, 0.0f));
				})
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindOptRot", "Find Optimal Rotations"))
				.OnClicked_Lambda([this]() { OnFindOptimalRotationClicked(); return FReply::Handled(); })
			]

			// Presets
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Presets", "Quick Presets"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("PresetFinisher", "Finisher"))
					.OnClicked_Lambda([this]() { ApplyPreset_Finisher(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("PresetCounter", "Counter"))
					.OnClicked_Lambda([this]() { ApplyPreset_Counter(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("PresetParry", "Parry"))
					.OnClicked_Lambda([this]() { ApplyPreset_Parry(); return FReply::Handled(); })
				]
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildTimelineControls()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SVerticalBox)

			// Time display
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(TimeDisplayText, STextBlock)
				.Text(GetTimeDisplayText())
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 11))
			]

			// Timeline slider
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(TimelineSlider, SSlider)
				.Value(0.0f)
				.OnValueChanged(this, &SPairedAnimationPreview::OnTimelineValueChanged)
			]

			// Playback controls
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepBackLarge", "<<"))
					.ToolTipText(LOCTEXT("StepBackLargeTip", "Step back 0.1s"))
					.OnClicked_Lambda([this]() { OnStepBackwardLarge(); return FReply::Handled(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepBack", "<"))
					.ToolTipText(LOCTEXT("StepBackTip", "Step back 1 frame"))
					.OnClicked_Lambda([this]() { OnStepBackward(); return FReply::Handled(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text_Lambda([this]() { return bIsPlaying ? LOCTEXT("Pause", "||") : LOCTEXT("Play", ">"); })
					.OnClicked_Lambda([this]() { OnPlayPauseClicked(); return FReply::Handled(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepFwd", ">"))
					.ToolTipText(LOCTEXT("StepFwdTip", "Step forward 1 frame"))
					.OnClicked_Lambda([this]() { OnStepForward(); return FReply::Handled(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepFwdLarge", ">>"))
					.ToolTipText(LOCTEXT("StepFwdLargeTip", "Step forward 0.1s"))
					.OnClicked_Lambda([this]() { OnStepForwardLarge(); return FReply::Handled(); })
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("Reset", "Reset"))
					.OnClicked_Lambda([this]() { OnResetClicked(); return FReply::Handled(); })
				]

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNullWidget::NullWidget
				]

				// Speed control
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(8.0f, 0.0f, 2.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("Speed", "Speed"))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(0.1f)
					.MaxValue(2.0f)
					.Value_Lambda([this]() { return PlaybackSpeed; })
					.OnValueChanged_Lambda([this](float Val) { PlaybackSpeed = Val; })
					.MinDesiredWidth(60.0f)
				]

				// Loop toggle
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 2.0f, 0.0f)
				[
					SNew(SCheckBox)
					.IsChecked_Lambda([this]() { return bLoopPlayback ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
					.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bLoopPlayback = (State == ECheckBoxState::Checked); })
					.ToolTipText(LOCTEXT("LoopTip", "Loop playback"))
					[
						SNew(STextBlock).Text(LOCTEXT("Loop", "Loop"))
					]
				]
			]

			// Victim offset
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("VictimOffset", "Victim Time Offset"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(-2.0f)
					.MaxValue(2.0f)
					.Value_Lambda([this]() { return VictimTimeOffset; })
					.OnValueChanged_Lambda([this](float Val) {
						VictimTimeOffset = Val;
						RecalculateMaxDuration();
						bAnalysisCacheDirty = true;
					})
				]
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildAnalysisPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(ContactInfoText, STextBlock)
				.Text(GetContactInfoText())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(DistanceInfoText, STextBlock)
				.Text(GetDistanceInfoText())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(VelocityInfoText, STextBlock)
				.Text(GetVelocityInfoText())
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(OptimizationInfoText, STextBlock)
				.Text(GetOptimizationInfoText())
				.ColorAndOpacity(FLinearColor(0.5f, 1.0f, 0.5f))
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildOptimizationPanel()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("Optimization", "Optimization"))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RunFullOpt", "Run Full Optimization"))
				.ToolTipText(LOCTEXT("RunFullOptTip", "Find optimal distance, rotations, and sync time"))
				.OnClicked_Lambda([this]() { OnOptimizeClicked(); return FReply::Handled(); })
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("FindDist", "Find Distance"))
					.OnClicked_Lambda([this]() { OnFindOptimalDistanceClicked(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("FindRot", "Find Rotation"))
					.OnClicked_Lambda([this]() { OnFindOptimalRotationClicked(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("FindSync", "Find Sync"))
					.OnClicked_Lambda([this]() { OnFindOptimalSyncClicked(); return FReply::Handled(); })
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RebuildCache", "Rebuild Analysis Cache"))
				.OnClicked_Lambda([this]() {
					RebuildAnalysisCache();
					RebuildTrajectoryCache();
					return FReply::Handled();
				})
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildVisualizationPanel()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("Visualization", "Visualization"))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return IsVisualizationActive(EVisualizationLayer::ContactPoints) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetVisualizationActive(EVisualizationLayer::ContactPoints, State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowContacts", "Contact Points"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return IsVisualizationActive(EVisualizationLayer::WeaponTrace) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetVisualizationActive(EVisualizationLayer::WeaponTrace, State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowWeapon", "Weapon Trace"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return IsVisualizationActive(EVisualizationLayer::DistanceLines) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetVisualizationActive(EVisualizationLayer::DistanceLines, State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowDistance", "Distance Lines"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return IsVisualizationActive(EVisualizationLayer::VelocityVectors) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetVisualizationActive(EVisualizationLayer::VelocityVectors, State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowVelocity", "Velocity Vectors"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return IsVisualizationActive(EVisualizationLayer::CenterOfMass) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetVisualizationActive(EVisualizationLayer::CenterOfMass, State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowCOM", "Center of Mass"))
				]
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildSettingsPanel()
{
	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("Settings", "Settings"))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SNew(SVerticalBox)

			// Weapon sockets
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WeaponSockets", "Weapon Sockets"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("StartSocket", "Start"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SAssignNew(WeaponStartSocketInput, SEditableTextBox)
					.Text(FText::FromName(AttackerConfig.WeaponStartSocket))
					.OnTextCommitted(this, &SPairedAnimationPreview::OnWeaponStartSocketChanged)
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("EndSocket", "End"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SAssignNew(WeaponEndSocketInput, SEditableTextBox)
					.Text(FText::FromName(AttackerConfig.WeaponEndSocket))
					.OnTextCommitted(this, &SPairedAnimationPreview::OnWeaponEndSocketChanged)
				]
			]

			// Contact threshold
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AnalysisSettings", "Analysis Settings"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("ContactThreshold", "Contact Threshold"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(10.0f)
					.MaxValue(200.0f)
					.Value_Lambda([this]() { return ContactThreshold; })
					.OnValueChanged_Lambda([this](float Val) {
						ContactThreshold = Val;
						bAnalysisCacheDirty = true;
					})
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("SampleRate", "Sample Rate"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SSpinBox<int32>)
					.MinValue(15)
					.MaxValue(120)
					.Value_Lambda([this]() { return AnalysisSampleRate; })
					.OnValueChanged_Lambda([this](int32 Val) {
						AnalysisSampleRate = Val;
						bAnalysisCacheDirty = true;
					})
				]
			]
		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildGraphsPanel()
{
	// Placeholder for future graphs (distance over time, speed over time, etc.)
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE
