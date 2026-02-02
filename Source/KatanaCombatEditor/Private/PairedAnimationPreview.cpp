// Copyright Epic Games, Inc. All Rights Reserved.

#include "PairedAnimationPreview.h"
#include "PairedAnimationPreviewConfig.h"
#include "Animation/AnimMontage.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "AdvancedPreviewScene.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMeshSocket.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
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
#include "Misc/ScopedSlowTask.h"
#include "DesktopPlatformModule.h"

#define LOCTEXT_NAMESPACE "PairedAnimationPreview"

// ============================================================================
// PT-16: CENTRALIZED CONFIGURATION
// ============================================================================
// Configuration values moved to PairedAnimationPreviewConfig.h for proper
// header/implementation separation. Include that header to access config values.

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

/**
 * PT-23: Unified number formatting helper.
 * Returns cached FNumberFormattingOptions for the specified decimal places.
 * Replaces GetNumberFormat0/1/2/3 individual functions.
 */
static const FNumberFormattingOptions& GetNumberFormat(int32 DecimalPlaces)
{
	// Cache formatting options for common decimal place counts (0-4)
	static TArray<FNumberFormattingOptions> CachedFormats = []() {
		TArray<FNumberFormattingOptions> Formats;
		Formats.SetNum(5); // 0-4 decimal places
		for (int32 i = 0; i < 5; ++i)
		{
			Formats[i].SetMaximumFractionalDigits(i);
		}
		return Formats;
	}();

	// Clamp to valid range
	const int32 ClampedPlaces = FMath::Clamp(DecimalPlaces, 0, 4);
	return CachedFormats[ClampedPlaces];
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
		// PT-16: Use centralized config values
		SetViewLocation(PairedAnimPreviewConfig::Camera::GetInitialLocation());
		SetViewRotation(PairedAnimPreviewConfig::Camera::GetInitialRotation());
		SetRealtime(true);

		// Better camera settings
		SetCameraSpeedSetting(PairedAnimPreviewConfig::Camera::SpeedSetting);
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

	void FocusOnPoint(const FVector& Point, float Distance = PairedAnimPreviewConfig::Camera::FocusDistance)
	{
		SetViewLocation(Point + FVector(-Distance, 0.0f, Distance * PairedAnimPreviewConfig::Camera::FocusHeightRatio));
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

	// Initialize section options with default "Entire Montage" option
	AttackerSectionOptions.Add(MakeShared<FName>(NAME_None));
	VictimSectionOptions.Add(MakeShared<FName>(NAME_None));

	SetupSharedPreviewScene();

	ChildSlot
	[
		BuildMainLayout()
	];
}

SPairedAnimationPreview::~SPairedAnimationPreview()
{
	// PT-12: Don't manually destroy components - FAdvancedPreviewScene owns them
	// and will handle cleanup when SharedPreviewScene is destroyed.
	// Manual DestroyComponent() calls risk double-free during editor shutdown.
	AttackerMeshComponent = nullptr;
	VictimMeshComponent = nullptr;
	AttackerWeaponMeshComponent = nullptr;
	VictimWeaponMeshComponent = nullptr;
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

	// Create weapon mesh components (will be attached to skeletons when weapons are assigned)
	AttackerWeaponMeshComponent = NewObject<UStaticMeshComponent>();
	SharedPreviewScene->AddComponent(AttackerWeaponMeshComponent, FTransform::Identity);
	AttackerWeaponMeshComponent->SetVisibility(false);

	VictimWeaponMeshComponent = NewObject<UStaticMeshComponent>();
	SharedPreviewScene->AddComponent(VictimWeaponMeshComponent, FTransform::Identity);
	VictimWeaponMeshComponent->SetVisibility(false);

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
		// Re-attach weapons to new skeleton
		ReattachWeapons();
	}
}

void SPairedAnimationPreview::UpdateAttackerWeaponMesh(UStaticMesh* Mesh)
{
	if (!AttackerWeaponMeshComponent) return;

	if (Mesh)
	{
		AttackerWeaponMeshComponent->SetStaticMesh(Mesh);
		AttackerWeaponConfig.SetWeaponMesh(Mesh);
		AttackerWeaponMeshComponent->SetVisibility(true);

		// Attach to skeleton if available
		if (AttackerMeshComponent && AttackerMeshComponent->GetSkeletalMeshAsset())
		{
			FName Socket = AttackerWeaponConfig.GetAttachmentSocket();
			if (AttackerMeshComponent->DoesSocketExist(Socket))
			{
				AttackerWeaponMeshComponent->AttachToComponent(
					AttackerMeshComponent,
					FAttachmentTransformRules::SnapToTargetNotIncludingScale,
					Socket);

				// Calculate attachment offset accounting for grip socket
				FTransform AttachOffset = AttackerWeaponConfig.GetAttachmentOffset();
				FName GripSocket = AttackerWeaponConfig.GetWeaponGripSocket();
				if (!GripSocket.IsNone() && AttackerWeaponMeshComponent->DoesSocketExist(GripSocket))
				{
					// Get the grip socket's transform relative to component (local space)
					FTransform GripSocketTransform = AttackerWeaponMeshComponent->GetSocketTransform(GripSocket, RTS_Component);
					// Apply inverse to position grip socket at attachment point
					FVector GripOffset = -GripSocketTransform.GetLocation();
					AttachOffset.SetLocation(AttachOffset.GetLocation() + GripOffset);
				}
				AttackerWeaponMeshComponent->SetRelativeTransform(AttachOffset);
			}
		}

		// Refresh weapon socket options since mesh changed
		RefreshAttackerWeaponSocketOptions();
	}
	else
	{
		AttackerWeaponMeshComponent->SetStaticMesh(nullptr);
		AttackerWeaponConfig.SetWeaponMesh(nullptr);
		AttackerWeaponMeshComponent->SetVisibility(false);
		AttackerWeaponMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		RefreshAttackerWeaponSocketOptions();
	}

	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::UpdateVictimWeaponMesh(UStaticMesh* Mesh)
{
	if (!VictimWeaponMeshComponent) return;

	if (Mesh)
	{
		VictimWeaponMeshComponent->SetStaticMesh(Mesh);
		VictimWeaponConfig.SetWeaponMesh(Mesh);
		VictimWeaponMeshComponent->SetVisibility(true);

		// Attach to skeleton if available
		if (VictimMeshComponent && VictimMeshComponent->GetSkeletalMeshAsset())
		{
			FName Socket = VictimWeaponConfig.GetAttachmentSocket();
			if (VictimMeshComponent->DoesSocketExist(Socket))
			{
				VictimWeaponMeshComponent->AttachToComponent(
					VictimMeshComponent,
					FAttachmentTransformRules::SnapToTargetNotIncludingScale,
					Socket);

				// Calculate attachment offset accounting for grip socket
				FTransform AttachOffset = VictimWeaponConfig.GetAttachmentOffset();
				FName GripSocket = VictimWeaponConfig.GetWeaponGripSocket();
				if (!GripSocket.IsNone() && VictimWeaponMeshComponent->DoesSocketExist(GripSocket))
				{
					// Get the grip socket's transform relative to component (local space)
					FTransform GripSocketTransform = VictimWeaponMeshComponent->GetSocketTransform(GripSocket, RTS_Component);
					// Apply inverse to position grip socket at attachment point
					FVector GripOffset = -GripSocketTransform.GetLocation();
					AttachOffset.SetLocation(AttachOffset.GetLocation() + GripOffset);
				}
				VictimWeaponMeshComponent->SetRelativeTransform(AttachOffset);
			}
		}

		// Refresh weapon socket options since mesh changed
		RefreshVictimWeaponSocketOptions();
	}
	else
	{
		VictimWeaponMeshComponent->SetStaticMesh(nullptr);
		VictimWeaponConfig.SetWeaponMesh(nullptr);
		VictimWeaponMeshComponent->SetVisibility(false);
		VictimWeaponMeshComponent->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		RefreshVictimWeaponSocketOptions();
	}

	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::ReattachWeapons()
{
	// Re-attach attacker weapon
	if (AttackerWeaponMeshComponent && AttackerWeaponConfig.IsValid() && AttackerMeshComponent)
	{
		FName Socket = AttackerWeaponConfig.GetAttachmentSocket();
		if (AttackerMeshComponent->DoesSocketExist(Socket))
		{
			AttackerWeaponMeshComponent->AttachToComponent(
				AttackerMeshComponent,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				Socket);

			// Calculate attachment offset accounting for grip socket
			FTransform AttachOffset = AttackerWeaponConfig.GetAttachmentOffset();
			FName GripSocket = AttackerWeaponConfig.GetWeaponGripSocket();
			if (!GripSocket.IsNone() && AttackerWeaponMeshComponent->DoesSocketExist(GripSocket))
			{
				// Get the grip socket's transform relative to component (local space)
				FTransform GripSocketTransform = AttackerWeaponMeshComponent->GetSocketTransform(GripSocket, RTS_Component);
				FVector GripOffset = -GripSocketTransform.GetLocation();
				AttachOffset.SetLocation(AttachOffset.GetLocation() + GripOffset);
			}
			AttackerWeaponMeshComponent->SetRelativeTransform(AttachOffset);
		}
	}

	// Re-attach victim weapon
	if (VictimWeaponMeshComponent && VictimWeaponConfig.IsValid() && VictimMeshComponent)
	{
		FName Socket = VictimWeaponConfig.GetAttachmentSocket();
		if (VictimMeshComponent->DoesSocketExist(Socket))
		{
			VictimWeaponMeshComponent->AttachToComponent(
				VictimMeshComponent,
				FAttachmentTransformRules::SnapToTargetNotIncludingScale,
				Socket);

			// Calculate attachment offset accounting for grip socket
			FTransform AttachOffset = VictimWeaponConfig.GetAttachmentOffset();
			FName GripSocket = VictimWeaponConfig.GetWeaponGripSocket();
			if (!GripSocket.IsNone() && VictimWeaponMeshComponent->DoesSocketExist(GripSocket))
			{
				// Get the grip socket's transform relative to component (local space)
				FTransform GripSocketTransform = VictimWeaponMeshComponent->GetSocketTransform(GripSocket, RTS_Component);
				FVector GripOffset = -GripSocketTransform.GetLocation();
				AttachOffset.SetLocation(AttachOffset.GetLocation() + GripOffset);
			}
			VictimWeaponMeshComponent->SetRelativeTransform(AttachOffset);
		}
	}

	bAnalysisCacheDirty = true;
}

FVector SPairedAnimationPreview::GetWeaponContactPosition(UStaticMeshComponent* WeaponMesh, const FWeaponMeshConfig& Config, EContactPointType ContactType) const
{
	if (!WeaponMesh || !WeaponMesh->GetStaticMesh()) return FVector::ZeroVector;

	// Try to use weapon sockets first
	FName SocketName = NAME_None;
	if (ContactType == EContactPointType::WeaponTip)
	{
		SocketName = Config.GetWeaponTipSocket();
	}
	else if (ContactType == EContactPointType::WeaponBase)
	{
		SocketName = Config.GetWeaponBaseSocket();
	}

	if (SocketName != NAME_None && WeaponMesh->DoesSocketExist(SocketName))
	{
		return WeaponMesh->GetSocketLocation(SocketName);
	}

	// Fall back to mesh bounds
	FBoxSphereBounds Bounds = WeaponMesh->CalcBounds(WeaponMesh->GetComponentTransform());
	FVector LocalExtent = Bounds.BoxExtent;

	// Assume weapon is oriented with X as the length axis
	switch (ContactType)
	{
		case EContactPointType::WeaponTip:
			// Tip is at the far end of the weapon
			return Bounds.Origin + FVector(LocalExtent.X, 0, 0);

		case EContactPointType::WeaponBase:
			// Base is at the near end of the weapon
			return Bounds.Origin - FVector(LocalExtent.X, 0, 0);

		case EContactPointType::WeaponMid:
		default:
			// Mid is at the center
			return Bounds.Origin;
	}
}

// ============================================================================
// WEAPON SOCKET CONFIGURATION
// ============================================================================

TArray<FName> SPairedAnimationPreview::GetSkeletalMeshSockets(UDebugSkelMeshComponent* MeshComp) const
{
	TArray<FName> SocketNames;
	if (!MeshComp || !MeshComp->GetSkeletalMeshAsset()) return SocketNames;

	// Add a "None" option first
	SocketNames.Add(NAME_None);

	// Get sockets from the skeletal mesh
	const TArray<USkeletalMeshSocket*>& Sockets = MeshComp->GetSkeletalMeshAsset()->GetActiveSocketList();
	for (const USkeletalMeshSocket* Socket : Sockets)
	{
		if (Socket)
		{
			SocketNames.Add(Socket->SocketName);
		}
	}

	// Also add bone names as potential attachment points
	const FReferenceSkeleton& RefSkeleton = MeshComp->GetSkeletalMeshAsset()->GetRefSkeleton();
	for (int32 i = 0; i < RefSkeleton.GetNum(); ++i)
	{
		SocketNames.AddUnique(RefSkeleton.GetBoneName(i));
	}

	return SocketNames;
}

TArray<FName> SPairedAnimationPreview::GetStaticMeshSockets(UStaticMesh* StaticMesh) const
{
	TArray<FName> SocketNames;
	if (!StaticMesh) return SocketNames;

	// Add a "None" option first (for bounds-based fallback)
	SocketNames.Add(NAME_None);

	// Get sockets from the static mesh
	const TArray<UStaticMeshSocket*>& Sockets = StaticMesh->Sockets;
	for (const UStaticMeshSocket* Socket : Sockets)
	{
		if (Socket)
		{
			SocketNames.Add(Socket->SocketName);
		}
	}

	return SocketNames;
}

// PT-24: Consolidated socket refresh method - eliminates code duplication
void SPairedAnimationPreview::RefreshWeaponSocketOptionsForCharacter(bool bIsAttacker)
{
	// Get references to the appropriate character's data
	UDebugSkelMeshComponent* MeshComponent = bIsAttacker ? AttackerMeshComponent : VictimMeshComponent;
	FWeaponMeshConfig& WeaponConfig = bIsAttacker ? AttackerWeaponConfig : VictimWeaponConfig;
	TArray<TSharedPtr<FName>>& CharSocketOptions = bIsAttacker ? AttackerCharacterSocketOptions : VictimCharacterSocketOptions;
	TArray<TSharedPtr<FName>>& WeaponSocketOptions = bIsAttacker ? AttackerWeaponSocketOptions : VictimWeaponSocketOptions;

	// Character sockets (from skeletal mesh)
	CharSocketOptions.Empty();
	TArray<FName> CharSockets = GetSkeletalMeshSockets(MeshComponent);
	for (const FName& Socket : CharSockets)
	{
		CharSocketOptions.Add(MakeShared<FName>(Socket));
	}

	// Weapon sockets (from static mesh)
	WeaponSocketOptions.Empty();
	if (UStaticMesh* WeaponMesh = WeaponConfig.GetWeaponMesh())
	{
		TArray<FName> WpnSockets = GetStaticMeshSockets(WeaponMesh);
		for (const FName& Socket : WpnSockets)
		{
			WeaponSocketOptions.Add(MakeShared<FName>(Socket));
		}
	}
	else
	{
		WeaponSocketOptions.Add(MakeShared<FName>(NAME_None));
	}

	// Refresh combo boxes if they exist
	TSharedPtr<SComboBox<TSharedPtr<FName>>>& CharSocketCombo = bIsAttacker ? AttackerCharacterSocketCombo : VictimCharacterSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>>& GripSocketCombo = bIsAttacker ? AttackerWeaponGripSocketCombo : VictimWeaponGripSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>>& TipSocketCombo = bIsAttacker ? AttackerWeaponTipSocketCombo : VictimWeaponTipSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>>& MidSocketCombo = bIsAttacker ? AttackerWeaponMidSocketCombo : VictimWeaponMidSocketCombo;
	TSharedPtr<SComboBox<TSharedPtr<FName>>>& BaseSocketCombo = bIsAttacker ? AttackerWeaponBaseSocketCombo : VictimWeaponBaseSocketCombo;

	if (CharSocketCombo.IsValid())
	{
		CharSocketCombo->RefreshOptions();
	}
	if (GripSocketCombo.IsValid())
	{
		GripSocketCombo->RefreshOptions();
	}
	if (TipSocketCombo.IsValid())
	{
		TipSocketCombo->RefreshOptions();
	}
	if (MidSocketCombo.IsValid())
	{
		MidSocketCombo->RefreshOptions();
	}
	if (BaseSocketCombo.IsValid())
	{
		BaseSocketCombo->RefreshOptions();
	}
}

void SPairedAnimationPreview::OnAttackerCharacterSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		AttackerWeaponConfig.SetAttachmentSocket(*NewSocket);
		ReattachWeapons();
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnAttackerWeaponGripSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		AttackerWeaponConfig.SetWeaponGripSocket(*NewSocket);
		ReattachWeapons();
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnAttackerWeaponTipSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		AttackerWeaponConfig.SetWeaponTipSocket(*NewSocket);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnAttackerWeaponMidSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		AttackerWeaponConfig.SetWeaponMidSocket(*NewSocket);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnAttackerWeaponBaseSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		AttackerWeaponConfig.SetWeaponBaseSocket(*NewSocket);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnVictimCharacterSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		VictimWeaponConfig.SetAttachmentSocket(*NewSocket);
		ReattachWeapons();
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnVictimWeaponGripSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		VictimWeaponConfig.SetWeaponGripSocket(*NewSocket);
		ReattachWeapons();
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnVictimWeaponTipSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		VictimWeaponConfig.SetWeaponTipSocket(*NewSocket);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnVictimWeaponMidSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		VictimWeaponConfig.SetWeaponMidSocket(*NewSocket);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnVictimWeaponBaseSocketChanged(TSharedPtr<FName> NewSocket, ESelectInfo::Type SelectType)
{
	if (NewSocket.IsValid())
	{
		VictimWeaponConfig.SetWeaponBaseSocket(*NewSocket);
		bAnalysisCacheDirty = true;
	}
}

void SPairedAnimationPreview::OnAttackerWeaponOffsetChanged(FVector NewOffset)
{
	FTransform CurrentOffset = AttackerWeaponConfig.GetAttachmentOffset();
	CurrentOffset.SetLocation(NewOffset);
	AttackerWeaponConfig.SetAttachmentOffset(CurrentOffset);
	ReattachWeapons();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::OnAttackerWeaponRotationChanged(FRotator NewRotation)
{
	FTransform CurrentOffset = AttackerWeaponConfig.GetAttachmentOffset();
	CurrentOffset.SetRotation(NewRotation.Quaternion());
	AttackerWeaponConfig.SetAttachmentOffset(CurrentOffset);
	ReattachWeapons();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::OnVictimWeaponOffsetChanged(FVector NewOffset)
{
	FTransform CurrentOffset = VictimWeaponConfig.GetAttachmentOffset();
	CurrentOffset.SetLocation(NewOffset);
	VictimWeaponConfig.SetAttachmentOffset(CurrentOffset);
	ReattachWeapons();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::OnVictimWeaponRotationChanged(FRotator NewRotation)
{
	FTransform CurrentOffset = VictimWeaponConfig.GetAttachmentOffset();
	CurrentOffset.SetRotation(NewRotation.Quaternion());
	VictimWeaponConfig.SetAttachmentOffset(CurrentOffset);
	ReattachWeapons();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::ApplyCharacterConfigs()
{
	// Mesh yaw offset: Skeletal meshes are authored with +Y as visual forward,
	// but Unreal uses +X as component forward. Add -90° to align mesh visuals
	// with expected component orientation.
	static constexpr float MeshYawOffset = -90.0f;

	if (AttackerMeshComponent)
	{
		FTransform AttackerTransform;
		AttackerTransform.SetLocation(AttackerConfig.PositionOffset);

		// Apply mesh yaw offset to align visual forward with component forward
		FRotator AdjustedAttackerRotation = AttackerConfig.RotationOffset;
		AdjustedAttackerRotation.Yaw += MeshYawOffset;
		AttackerTransform.SetRotation(AdjustedAttackerRotation.Quaternion());

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

		// Apply mesh yaw offset to align visual forward with component forward
		FRotator AdjustedVictimRotation = VictimConfig.RotationOffset;
		AdjustedVictimRotation.Yaw += MeshYawOffset;
		VictimTransform.SetRotation(AdjustedVictimRotation.Quaternion());

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
	// CRITICAL FIX: Use the passed Time parameter, not CurrentTime!
	// This was causing the holistic optimization to produce different results
	// at different timeline positions - it was always evaluating at CurrentTime
	// instead of the sampled time points.
	//
	// SECOND FIX: Must call RefreshBoneTransforms() after SetPosition() to ensure
	// bone world transforms are updated immediately for contact point analysis.
	// Without this, animations with root motion produce frame-dependent results.
	//
	// THIRD FIX: Each character loops independently within their section bounds.
	// This allows each montage section to repeat without waiting for the other.

	if (AttackerMeshComponent && AttackerMontage.IsValid())
	{
		float AttackerTime = Time;

		// Wrap attacker time within section bounds for independent looping
		float SectionLength = AttackerSectionEnd - AttackerSectionStart;
		if (SectionLength > 0.0f && bLoopPlayback)
		{
			// Calculate how far we are past section start, then wrap within section
			float TimeInSection = FMath::Fmod(AttackerTime - AttackerSectionStart, SectionLength);
			if (TimeInSection < 0.0f) TimeInSection += SectionLength;  // Handle negative mod
			AttackerTime = AttackerSectionStart + TimeInSection;
		}
		else
		{
			// Clamp to section bounds if not looping
			AttackerTime = FMath::Clamp(AttackerTime, AttackerSectionStart, AttackerSectionEnd);
		}

		AttackerMeshComponent->SetPosition(AttackerTime);
		AttackerMeshComponent->RefreshBoneTransforms();
	}

	if (VictimMeshComponent && VictimMontage.IsValid())
	{
		float VictimTime = FMath::Max(0.0f, Time - VictimTimeOffset);

		// Wrap victim time within section bounds for independent looping
		float SectionLength = VictimSectionEnd - VictimSectionStart;
		if (SectionLength > 0.0f && bLoopPlayback)
		{
			// Calculate how far we are past section start, then wrap within section
			float TimeInSection = FMath::Fmod(VictimTime - VictimSectionStart, SectionLength);
			if (TimeInSection < 0.0f) TimeInSection += SectionLength;  // Handle negative mod
			VictimTime = VictimSectionStart + TimeInSection;
		}
		else
		{
			// Clamp to section bounds if not looping
			VictimTime = FMath::Clamp(VictimTime, VictimSectionStart, VictimSectionEnd);
		}

		VictimMeshComponent->SetPosition(VictimTime);
		VictimMeshComponent->RefreshBoneTransforms();
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

	// Reset section selection and refresh options
	AttackerMontageSection = NAME_None;
	RefreshAttackerSectionOptions();

	RecalculateMaxDuration();
	bAnalysisCacheDirty = true;
	bHolisticCacheDirty = true;

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

	// Reset section selection and refresh options
	VictimMontageSection = NAME_None;
	RefreshVictimSectionOptions();

	RecalculateMaxDuration();
	bAnalysisCacheDirty = true;
	bHolisticCacheDirty = true;

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

void SPairedAnimationPreview::OnAttackerWeaponMeshSelected(const FAssetData& AssetData)
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset());
	UpdateAttackerWeaponMesh(Mesh);
}

void SPairedAnimationPreview::OnVictimWeaponMeshSelected(const FAssetData& AssetData)
{
	UStaticMesh* Mesh = Cast<UStaticMesh>(AssetData.GetAsset());
	UpdateVictimWeaponMesh(Mesh);
}

FString SPairedAnimationPreview::GetAttackerWeaponMeshPath() const
{
	if (UStaticMesh* Mesh = AttackerWeaponConfig.GetWeaponMesh())
	{
		return Mesh->GetPathName();
	}
	return FString();
}

FString SPairedAnimationPreview::GetVictimWeaponMeshPath() const
{
	if (UStaticMesh* Mesh = VictimWeaponConfig.GetWeaponMesh())
	{
		return Mesh->GetPathName();
	}
	return FString();
}

void SPairedAnimationPreview::RecalculateMaxDuration()
{
	MinTime = 0.0f;
	MaxDuration = 0.0f;
	AttackerSectionStart = 0.0f;
	AttackerSectionEnd = 0.0f;
	VictimSectionStart = 0.0f;
	VictimSectionEnd = 0.0f;

	// Calculate effective duration considering section selection
	if (AttackerMontage.IsValid())
	{
		GetSectionTimeRange(AttackerMontage.Get(), AttackerMontageSection, AttackerSectionStart, AttackerSectionEnd);
		float AttackerDuration = AttackerSectionEnd - AttackerSectionStart;
		MaxDuration = FMath::Max(MaxDuration, AttackerSectionEnd);
		MinTime = AttackerSectionStart;  // Use attacker section start as the effective start
	}
	if (VictimMontage.IsValid())
	{
		GetSectionTimeRange(VictimMontage.Get(), VictimMontageSection, VictimSectionStart, VictimSectionEnd);
		float VictimDuration = VictimSectionEnd - VictimSectionStart;
		// Victim effective end accounts for offset
		MaxDuration = FMath::Max(MaxDuration, VictimSectionEnd + VictimTimeOffset);
	}
}

// ============================================================================
// SPATIAL RELATIONSHIP (PT-2)
// ============================================================================

FSpatialRelationshipInference SPairedAnimationPreview::InferSpatialRelationship()
{
	FSpatialRelationshipInference Result;
	Result.SetInferredRelationship(ESpatialRelationship::Facing);
	Result.SetConfidence(0.0f);

	if (!AttackerMeshComponent || !VictimMeshComponent)
	{
		Result.SetReasoningText(TEXT("No meshes loaded"));
		return Result;
	}

	// Ensure holistic analysis is up to date
	if (bHolisticCacheDirty)
	{
		RebuildHolisticAnalysis();
	}

	// Find the frame with best contact quality (this is where the attack actually connects)
	float BestContactTime = TimingAnalysis.BestSyncTime;
	if (BestContactTime <= 0.0f)
	{
		// Fallback: use activity peak
		for (const FTrajectoryFrameSample& Sample : HolisticAnalysis.FrameSamples)
		{
			if (Sample.ContactQuality > Result.GetConfidence())
			{
				Result.SetConfidence(Sample.ContactQuality);
				BestContactTime = Sample.Time;
			}
		}
	}

	// CRITICAL FIX: Save current config and temporarily apply IDENTITY rotation for victim
	// This ensures inference is based on animation content, not user configuration.
	// Without this, optimization flip-flops because each run reads the previous run's rotation.
	FRotator SavedVictimRotation = VictimConfig.GetRotationOffset();
	FRotator SavedAttackerRotation = AttackerConfig.GetRotationOffset();
	float SavedDistance = LockedDistance;

	// Apply neutral configuration for inference
	VictimConfig.SetRotationOffset(FRotator::ZeroRotator);
	AttackerConfig.SetRotationOffset(FRotator::ZeroRotator);
	LockedDistance = 150.0f;  // Standard inference distance
	ApplyCharacterConfigs();

	// Analyze at the best contact time with neutral config
	UpdateAnimations(BestContactTime);
	FMultiContactAnalysis ContactAnalysis = ComputeMultiContactPoints(BestContactTime);

	// Get the contact normal from the best contact pair
	if (ContactAnalysis.GetBestContactDistance() < ContactThreshold)
	{
		FVector AttackerPos = ContactAnalysis.GetAttackerContactPositions().FindRef(ContactAnalysis.GetBestAttackerContact());
		FVector VictimPos = ContactAnalysis.GetVictimContactPositions().FindRef(ContactAnalysis.GetBestVictimContact());
		Result.SetPrimaryContactNormal((AttackerPos - VictimPos).GetSafeNormal());
		Result.SetVictimContactBone(VictimBoneConfig.GetBoneForType(ContactAnalysis.GetBestVictimContact()));
	}

	// Calculate victim's facing relative to attacker (now based on neutral/identity rotation)
	FVector AttackerLoc = AttackerMeshComponent->GetComponentLocation();
	FVector VictimLoc = VictimMeshComponent->GetComponentLocation();
	FVector VictimForward = VictimMeshComponent->GetComponentRotation().Vector();
	FVector DirToAttacker = (AttackerLoc - VictimLoc).GetSafeNormal2D();

	// Angle between victim's forward and direction to attacker
	// 0 = victim facing attacker, 180 = victim facing away
	float DotProduct = FVector::DotProduct(VictimForward.GetSafeNormal2D(), DirToAttacker);
	Result.SetVictimFacingAngle(FMath::RadiansToDegrees(FMath::Acos(DotProduct)));

	// Determine which side of victim the attacker is on
	FVector VictimRight = FVector::CrossProduct(FVector::UpVector, VictimForward).GetSafeNormal();
	float SideDot = FVector::DotProduct(VictimRight, DirToAttacker);

	// Infer relationship based on facing angle and contact bone
	FString Reasoning;

	// Use VictimContactBone as additional evidence
	FName VictimContactBone = Result.GetVictimContactBone();
	bool bContactsBack = (VictimContactBone == TEXT("spine_01") ||
						   VictimContactBone == TEXT("spine_02") ||
						   VictimContactBone == TEXT("spine_03"));
	bool bContactsFront = (VictimContactBone == TEXT("chest") ||
						   VictimContactBone == TEXT("neck_01") ||
						   VictimContactBone == TEXT("head"));

	float VictimFacingAngle = Result.GetVictimFacingAngle();
	if (VictimFacingAngle < 45.0f)
	{
		// Victim is facing attacker (front attack)
		Result.SetInferredRelationship(ESpatialRelationship::Facing);
		Result.SetConfidence(0.9f - (VictimFacingAngle / 90.0f));
		Reasoning = FString::Printf(TEXT("Victim facing attacker (%.1f deg)"), VictimFacingAngle);
	}
	else if (VictimFacingAngle > 135.0f)
	{
		// Victim facing away (backstab)
		Result.SetInferredRelationship(ESpatialRelationship::Behind);
		Result.SetConfidence(0.9f - ((180.0f - VictimFacingAngle) / 90.0f));
		Reasoning = FString::Printf(TEXT("Victim facing away (%.1f deg)"), VictimFacingAngle);
	}
	else if (SideDot > 0.3f)
	{
		// Attacker on victim's right side
		Result.SetInferredRelationship(ESpatialRelationship::RightSide);
		Result.SetConfidence(FMath::Abs(SideDot));
		Reasoning = FString::Printf(TEXT("Attacker on victim's right (side dot: %.2f)"), SideDot);
	}
	else if (SideDot < -0.3f)
	{
		// Attacker on victim's left side
		Result.SetInferredRelationship(ESpatialRelationship::LeftSide);
		Result.SetConfidence(FMath::Abs(SideDot));
		Reasoning = FString::Printf(TEXT("Attacker on victim's left (side dot: %.2f)"), SideDot);
	}
	else
	{
		// Ambiguous - could be facing or behind
		if (bContactsBack)
		{
			Result.SetInferredRelationship(ESpatialRelationship::Behind);
			Result.SetConfidence(0.6f);
			Reasoning = FString::Printf(TEXT("Contact on back bones (%s)"), *VictimContactBone.ToString());
		}
		else
		{
			Result.SetInferredRelationship(ESpatialRelationship::Facing);
			Result.SetConfidence(0.5f);
			Reasoning = TEXT("Ambiguous angle - defaulting to Facing");
		}
	}

	// Boost confidence if contact bone evidence aligns
	if ((Result.GetInferredRelationship() == ESpatialRelationship::Behind && bContactsBack) ||
		(Result.GetInferredRelationship() == ESpatialRelationship::Facing && bContactsFront))
	{
		Result.SetConfidence(FMath::Min(1.0f, Result.GetConfidence() + 0.2f));
		Reasoning += TEXT(" (confirmed by contact bone)");
	}

	Result.SetReasoningText(Reasoning);
	bSpatialInferenceCacheDirty = false;
	InferredRelationship = Result;

	// Restore original configuration after inference
	VictimConfig.SetRotationOffset(SavedVictimRotation);
	AttackerConfig.SetRotationOffset(SavedAttackerRotation);
	LockedDistance = SavedDistance;
	ApplyCharacterConfigs();
	UpdateAnimations(CurrentTime);  // Restore to current timeline position

	return Result;
}

FSpatialRotationConstraint SPairedAnimationPreview::GetRotationConstraintForRelationship() const
{
	FSpatialRotationConstraint Constraint;

	ESpatialRelationship EffectiveRelationship = GetEffectiveSpatialRelationship();

	switch (EffectiveRelationship)
	{
		case ESpatialRelationship::Facing:
			// Victim should face attacker (180 degrees relative to attacker forward)
			Constraint.SetTargetYaw(180.0f);
			Constraint.SetTolerance(30.0f);
			Constraint.SetConstrained(true);
			break;

		case ESpatialRelationship::Behind:
			// Victim should face away from attacker (0 degrees - back to attacker)
			Constraint.SetTargetYaw(0.0f);
			Constraint.SetTolerance(30.0f);
			Constraint.SetConstrained(true);
			break;

		case ESpatialRelationship::LeftSide:
			// Victim's left side to attacker (90 degrees)
			Constraint.SetTargetYaw(90.0f);
			Constraint.SetTolerance(30.0f);
			Constraint.SetConstrained(true);
			break;

		case ESpatialRelationship::RightSide:
			// Victim's right side to attacker (-90 degrees)
			Constraint.SetTargetYaw(-90.0f);
			Constraint.SetTolerance(30.0f);
			Constraint.SetConstrained(true);
			break;

		case ESpatialRelationship::Custom:
		case ESpatialRelationship::Inferred:
		default:
			// No constraint - full search space
			Constraint.SetConstrained(false);
			break;
	}

	return Constraint;
}

ESpatialRelationship SPairedAnimationPreview::GetEffectiveSpatialRelationship() const
{
	if (CurrentSpatialRelationship == ESpatialRelationship::Inferred)
	{
		// Use the cached inferred relationship
		return InferredRelationship.GetInferredRelationship();
	}
	return CurrentSpatialRelationship;
}

void SPairedAnimationPreview::OnSpatialRelationshipChanged(ESpatialRelationship NewRelationship)
{
	CurrentSpatialRelationship = NewRelationship;

	// If changed to Inferred, recalculate
	if (NewRelationship == ESpatialRelationship::Inferred)
	{
		bSpatialInferenceCacheDirty = true;
		InferSpatialRelationship();
	}

	// Mark holistic cache dirty since optimization constraints changed
	bHolisticCacheDirty = true;
}

FString SPairedAnimationPreview::GetRelationshipDisplayName(ESpatialRelationship Relationship)
{
	switch (Relationship)
	{
		case ESpatialRelationship::Inferred:  return TEXT("Auto-Detect");
		case ESpatialRelationship::Facing:    return TEXT("Facing (Front)");
		case ESpatialRelationship::Behind:    return TEXT("Behind (Back)");
		case ESpatialRelationship::LeftSide:  return TEXT("Left Side");
		case ESpatialRelationship::RightSide: return TEXT("Right Side");
		case ESpatialRelationship::Custom:    return TEXT("Custom (No Constraint)");
		default: return TEXT("Unknown");
	}
}

// ============================================================================
// MONTAGE SECTION SELECTION
// ============================================================================

void SPairedAnimationPreview::RefreshAttackerSectionOptions()
{
	AttackerSectionOptions.Empty();

	// Always add "Entire Montage" option (NAME_None)
	AttackerSectionOptions.Add(MakeShared<FName>(NAME_None));

	if (AttackerMontage.IsValid())
	{
		// Iterate over composite sections in the montage
		for (int32 i = 0; i < AttackerMontage->CompositeSections.Num(); ++i)
		{
			FName SectionName = AttackerMontage->CompositeSections[i].SectionName;
			if (!SectionName.IsNone())
			{
				AttackerSectionOptions.Add(MakeShared<FName>(SectionName));
			}
		}
	}

	// Reset selection to "Entire Montage"
	AttackerMontageSection = NAME_None;

	// Refresh combo box if it exists
	if (AttackerSectionCombo.IsValid())
	{
		AttackerSectionCombo->RefreshOptions();
		if (AttackerSectionOptions.Num() > 0)
		{
			AttackerSectionCombo->SetSelectedItem(AttackerSectionOptions[0]);
		}
	}
}

void SPairedAnimationPreview::RefreshVictimSectionOptions()
{
	VictimSectionOptions.Empty();

	// Always add "Entire Montage" option (NAME_None)
	VictimSectionOptions.Add(MakeShared<FName>(NAME_None));

	if (VictimMontage.IsValid())
	{
		// Iterate over composite sections in the montage
		for (int32 i = 0; i < VictimMontage->CompositeSections.Num(); ++i)
		{
			FName SectionName = VictimMontage->CompositeSections[i].SectionName;
			if (!SectionName.IsNone())
			{
				VictimSectionOptions.Add(MakeShared<FName>(SectionName));
			}
		}
	}

	// Reset selection to "Entire Montage"
	VictimMontageSection = NAME_None;

	// Refresh combo box if it exists
	if (VictimSectionCombo.IsValid())
	{
		VictimSectionCombo->RefreshOptions();
		if (VictimSectionOptions.Num() > 0)
		{
			VictimSectionCombo->SetSelectedItem(VictimSectionOptions[0]);
		}
	}
}

void SPairedAnimationPreview::OnAttackerSectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectType)
{
	if (NewSelection.IsValid())
	{
		AttackerMontageSection = *NewSelection;
		RecalculateMaxDuration();
		bAnalysisCacheDirty = true;
		bHolisticCacheDirty = true;

		// Reset to section start
		float SectionStart = 0.0f;
		float SectionEnd = 0.0f;
		GetSectionTimeRange(AttackerMontage.Get(), AttackerMontageSection, SectionStart, SectionEnd);
		CurrentTime = SectionStart;
		UpdateAnimations(CurrentTime);
	}
}

void SPairedAnimationPreview::OnVictimSectionChanged(TSharedPtr<FName> NewSelection, ESelectInfo::Type SelectType)
{
	if (NewSelection.IsValid())
	{
		VictimMontageSection = *NewSelection;
		RecalculateMaxDuration();
		bAnalysisCacheDirty = true;
		bHolisticCacheDirty = true;
		UpdateAnimations(CurrentTime);
	}
}

float SPairedAnimationPreview::GetSectionStartTime(UAnimMontage* Montage, FName SectionName) const
{
	if (!Montage || SectionName.IsNone())
	{
		return 0.0f;
	}

	int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex != INDEX_NONE)
	{
		return Montage->GetAnimCompositeSection(SectionIndex).GetTime();
	}

	return 0.0f;
}

float SPairedAnimationPreview::GetSectionDuration(UAnimMontage* Montage, FName SectionName) const
{
	if (!Montage)
	{
		return 0.0f;
	}

	if (SectionName.IsNone())
	{
		// Entire montage
		return Montage->GetPlayLength();
	}

	float StartTime = 0.0f;
	float EndTime = 0.0f;
	GetSectionTimeRange(Montage, SectionName, StartTime, EndTime);
	return EndTime - StartTime;
}

void SPairedAnimationPreview::GetSectionTimeRange(UAnimMontage* Montage, FName SectionName, float& OutStart, float& OutEnd) const
{
	OutStart = 0.0f;
	OutEnd = 0.0f;

	if (!Montage)
	{
		return;
	}

	if (SectionName.IsNone())
	{
		// Entire montage
		OutStart = 0.0f;
		OutEnd = Montage->GetPlayLength();
		return;
	}

	int32 SectionIndex = Montage->GetSectionIndex(SectionName);
	if (SectionIndex == INDEX_NONE)
	{
		// Section not found, use entire montage
		OutStart = 0.0f;
		OutEnd = Montage->GetPlayLength();
		return;
	}

	// Get section start time
	OutStart = Montage->GetAnimCompositeSection(SectionIndex).GetTime();

	// PT-13: Optimized end time lookup
	// Try O(1) lookup first - check if next array index is the chronologically next section
	const int32 NumSections = Montage->CompositeSections.Num();

	if (SectionIndex + 1 < NumSections)
	{
		float NextIndexStart = Montage->GetAnimCompositeSection(SectionIndex + 1).GetTime();
		if (NextIndexStart > OutStart)
		{
			// Fast path: next array element is chronologically next
			OutEnd = NextIndexStart;
			return;
		}
	}

	// Last section in array - use montage length
	if (SectionIndex + 1 >= NumSections)
	{
		OutEnd = Montage->GetPlayLength();
		return;
	}

	// Slow path: sections are not chronologically ordered in array
	// Find the section with the smallest start time that's greater than ours
	float NextSectionStart = Montage->GetPlayLength();
	for (int32 i = 0; i < NumSections; ++i)
	{
		if (i != SectionIndex)
		{
			float OtherStart = Montage->GetAnimCompositeSection(i).GetTime();
			if (OtherStart > OutStart && OtherStart < NextSectionStart)
			{
				NextSectionStart = OtherStart;
			}
		}
	}

	OutEnd = NextSectionStart;
}

// ============================================================================
// SOCKET CONFIGURATION
// ============================================================================

void SPairedAnimationPreview::OnWeaponStartSocketChanged(const FText& NewText, ETextCommit::Type CommitType)
{
	AttackerConfig.SetWeaponStartSocket(FName(*NewText.ToString()));
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::OnWeaponEndSocketChanged(const FText& NewText, ETextCommit::Type CommitType)
{
	AttackerConfig.SetWeaponEndSocket(FName(*NewText.ToString()));
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

			// FIX: Confidence now includes distance AND angle quality for better scoring
			float DistanceQuality = FMath::Max(0.0f, 1.0f - (ClosestDist / ContactThreshold));
			// Weighted blend: 60% distance proximity + 40% impact angle quality
			Contact.Confidence = (DistanceQuality * 0.6f) + (Contact.AngleQuality * 0.4f);

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

	// Active notifies - use passed Time parameter with victim offset
	if (AttackerMontage.IsValid())
	{
		Analysis.AttackerActiveNotifies = GetActiveNotifies(AttackerMontage.Get(), Time);
	}
	if (VictimMontage.IsValid())
	{
		float VictimTime = FMath::Max(0.0f, Time - VictimTimeOffset);
		Analysis.VictimActiveNotifies = GetActiveNotifies(VictimMontage.Get(), VictimTime);
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

bool SPairedAnimationPreview::RebuildTrajectoryCacheWithProgress()
{
	// PT-22: Progress-enabled trajectory cache rebuild
	AttackerTrajectories.Empty();
	VictimTrajectories.Empty();

	if (MaxDuration <= 0.0f) return true;

	TArray<FName> TrackedBones = {
		TEXT("hand_r"), TEXT("hand_l"),
		TEXT("foot_r"), TEXT("foot_l"),
		TEXT("head"), TEXT("pelvis")
	};

	// Calculate total work: bones * characters * samples
	const int32 TotalWork = TrackedBones.Num() * 2 * (TrajectorySampleCount + 1);
	FScopedSlowTask SlowTask(TotalWork, LOCTEXT("BuildingTrajectories", "Building Bone Trajectories..."));
	SlowTask.MakeDialog(true);

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
				if (SlowTask.ShouldCancel())
				{
					return false;
				}
				SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("TracingAttackerBone", "Tracing attacker {0}..."), FText::FromName(BoneName)));

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
				if (SlowTask.ShouldCancel())
				{
					return false;
				}
				SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("TracingVictimBone", "Tracing victim {0}..."), FText::FromName(BoneName)));

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

	return true;
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
// HOLISTIC TIMELINE ANALYSIS
// ============================================================================

void SPairedAnimationPreview::RebuildHolisticAnalysis()
{
	HolisticAnalysis = FHolisticTimelineAnalysis();
	HolisticAnalysis.FrameSamples.Empty();

	if (MaxDuration <= 0.0f || !AttackerMeshComponent || !VictimMeshComponent)
	{
		bHolisticCacheDirty = false;
		return;
	}

	// Sample the entire animation timeline at high resolution
	const int32 NumSamples = FMath::Max(30, FMath::CeilToInt(MaxDuration * 30.0f)); // 30 samples per second
	const float TimeStep = MaxDuration / NumSamples;

	// First pass: Collect raw trajectory data
	for (int32 i = 0; i <= NumSamples; ++i)
	{
		float t = i * TimeStep;
		FTrajectoryFrameSample Sample = SampleTrajectoryFrame(t);
		HolisticAnalysis.FrameSamples.Add(Sample);
	}

	// Detect activity and contact phases
	DetectActivityPhases();
	DetectContactPhases();

	// Compute optimization weights based on activity and contact phases
	ComputeOptimizationWeights();

	bHolisticCacheDirty = false;
}

FTrajectoryFrameSample SPairedAnimationPreview::SampleTrajectoryFrame(float Time)
{
	FTrajectoryFrameSample Sample;
	Sample.Time = Time;

	if (!AttackerMeshComponent || !VictimMeshComponent)
	{
		return Sample;
	}

	// Update animations to this time
	UpdateAnimations(Time);

	// Compute velocities using central difference
	const float DeltaTime = 0.016f; // ~60fps sampling

	// Get hand velocities (primary attack effectors)
	FVector AttackerHandVel = ComputeBoneVelocity(AttackerMeshComponent, TEXT("hand_r"), Time, DeltaTime);
	FVector VictimPelvisVel = ComputeBoneVelocity(VictimMeshComponent, TEXT("pelvis"), Time, DeltaTime);

	Sample.AttackerVelocityMagnitude = AttackerHandVel.Size();
	Sample.VictimVelocityMagnitude = VictimPelvisVel.Size();

	// Combined activity: weighted sum (attacker motion matters more for combat)
	float MaxExpectedVelocity = 1000.0f; // Reasonable max for hand motion in cm/s
	Sample.CombinedActivity = FMath::Clamp(
		(Sample.AttackerVelocityMagnitude * 0.7f + Sample.VictimVelocityMagnitude * 0.3f) / MaxExpectedVelocity,
		0.0f, 1.0f);

	// Get multi-contact analysis for this frame
	FMultiContactAnalysis MultiContact = ComputeMultiContactPoints(Time);
	Sample.ClosestDistance = MultiContact.BestContactDistance;
	Sample.ContactQuality = MultiContact.WeightedContactQuality;

	// Compute approach direction and speed
	FVector AttackerPos = AttackerMeshComponent->GetComponentLocation();
	FVector VictimPos = VictimMeshComponent->GetComponentLocation();
	Sample.ApproachDirection = (VictimPos - AttackerPos).GetSafeNormal();

	// Project attacker velocity onto approach direction
	Sample.ApproachSpeed = FVector::DotProduct(AttackerHandVel, Sample.ApproachDirection);

	// Compute angle quality - how well aligned is the weapon with the approach direction
	FVector WeaponStart = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponStartSocket);
	FVector WeaponEnd = GetSocketWorldLocation(AttackerMeshComponent, AttackerConfig.WeaponEndSocket);
	FVector WeaponDir = (WeaponEnd - WeaponStart).GetSafeNormal();
	float AngleDot = FMath::Abs(FVector::DotProduct(WeaponDir, Sample.ApproachDirection));
	Sample.AngleQuality = FMath::Clamp(1.0f - AngleDot, 0.0f, 1.0f); // Perpendicular is best for slashing

	return Sample;
}

void SPairedAnimationPreview::DetectActivityPhases()
{
	if (HolisticAnalysis.FrameSamples.Num() < 2) return;

	// Find peak velocity and high activity threshold
	float MaxVelocity = 0.0f;
	int32 PeakIndex = 0;

	for (int32 i = 0; i < HolisticAnalysis.FrameSamples.Num(); ++i)
	{
		FTrajectoryFrameSample& Sample = HolisticAnalysis.FrameSamples[i];
		if (Sample.AttackerVelocityMagnitude > MaxVelocity)
		{
			MaxVelocity = Sample.AttackerVelocityMagnitude;
			PeakIndex = i;
		}
	}

	HolisticAnalysis.PeakVelocityMagnitude = MaxVelocity;
	HolisticAnalysis.PeakVelocityTime = HolisticAnalysis.FrameSamples[PeakIndex].Time;
	HolisticAnalysis.FrameSamples[PeakIndex].bIsPeakVelocityFrame = true;

	// High activity threshold: 40% of peak velocity
	float HighActivityThreshold = MaxVelocity * 0.4f;
	bool bInHighActivity = false;

	for (FTrajectoryFrameSample& Sample : HolisticAnalysis.FrameSamples)
	{
		bool bIsHigh = Sample.AttackerVelocityMagnitude > HighActivityThreshold;
		Sample.bIsHighActivityPhase = bIsHigh;

		if (bIsHigh)
		{
			HolisticAnalysis.HighActivityFrameCount++;
			if (!bInHighActivity)
			{
				HolisticAnalysis.HighActivityStartTime = Sample.Time;
				bInHighActivity = true;
			}
			HolisticAnalysis.HighActivityEndTime = Sample.Time;
		}
	}

	// Compute average activity
	float TotalActivity = 0.0f;
	for (const FTrajectoryFrameSample& Sample : HolisticAnalysis.FrameSamples)
	{
		TotalActivity += Sample.CombinedActivity;
	}
	HolisticAnalysis.AverageActivity = TotalActivity / HolisticAnalysis.FrameSamples.Num();
}

void SPairedAnimationPreview::DetectContactPhases()
{
	if (HolisticAnalysis.FrameSamples.Num() < 2) return;

	// Contact phase: frames where contact quality is above threshold
	const float ContactQualityThreshold = 0.3f;
	bool bInContactPhase = false;

	float TotalContactQuality = 0.0f;

	for (FTrajectoryFrameSample& Sample : HolisticAnalysis.FrameSamples)
	{
		bool bIsContact = Sample.ContactQuality > ContactQualityThreshold;
		Sample.bIsContactPhase = bIsContact;

		if (bIsContact)
		{
			HolisticAnalysis.ContactPhaseFrameCount++;
			TotalContactQuality += Sample.ContactQuality;

			if (!bInContactPhase)
			{
				HolisticAnalysis.ContactPhaseStartTime = Sample.Time;
				bInContactPhase = true;
			}
			HolisticAnalysis.ContactPhaseEndTime = Sample.Time;
		}
	}

	if (HolisticAnalysis.ContactPhaseFrameCount > 0)
	{
		HolisticAnalysis.AverageContactQuality = TotalContactQuality / HolisticAnalysis.ContactPhaseFrameCount;
	}
}

void SPairedAnimationPreview::ComputeOptimizationWeights()
{
	if (HolisticAnalysis.FrameSamples.Num() < 2) return;

	// Weight scheme:
	// - High activity phases get weight 2.0 (most important for attack animations)
	// - Contact phases get weight 1.5 (where contact matters)
	// - Peak velocity frame gets weight 3.0 (critical impact moment)
	// - Other frames get weight 0.5 (still considered but less important)

	float TotalWeight = 0.0f;
	float WeightedContact = 0.0f;
	float WeightedAlignment = 0.0f;

	for (FTrajectoryFrameSample& Sample : HolisticAnalysis.FrameSamples)
	{
		float Weight = 0.5f; // Base weight

		if (Sample.bIsPeakVelocityFrame)
		{
			Weight = 3.0f;
		}
		else if (Sample.bIsHighActivityPhase && Sample.bIsContactPhase)
		{
			Weight = 2.5f; // Both high activity AND contact
		}
		else if (Sample.bIsHighActivityPhase)
		{
			Weight = 2.0f;
		}
		else if (Sample.bIsContactPhase)
		{
			Weight = 1.5f;
		}

		Sample.OptimizationWeight = Weight;
		TotalWeight += Weight;
		WeightedContact += Sample.ContactQuality * Weight;
		WeightedAlignment += Sample.AngleQuality * Weight;
	}

	HolisticAnalysis.TotalWeight = TotalWeight;

	if (TotalWeight > 0.0f)
	{
		HolisticAnalysis.WeightedContactScore = WeightedContact / TotalWeight;
		HolisticAnalysis.WeightedAlignmentScore = WeightedAlignment / TotalWeight;
		HolisticAnalysis.WeightedOverallScore = (HolisticAnalysis.WeightedContactScore * 0.6f +
												 HolisticAnalysis.WeightedAlignmentScore * 0.4f);
	}
}

float SPairedAnimationPreview::GetActivityWeightAtTime(float Time) const
{
	if (HolisticAnalysis.FrameSamples.Num() == 0)
	{
		return 1.0f;
	}

	// Find closest frame sample
	float MinDiff = FLT_MAX;
	float Weight = 1.0f;

	for (const FTrajectoryFrameSample& Sample : HolisticAnalysis.FrameSamples)
	{
		float Diff = FMath::Abs(Sample.Time - Time);
		if (Diff < MinDiff)
		{
			MinDiff = Diff;
			Weight = Sample.OptimizationWeight;
		}
	}

	return Weight;
}

float SPairedAnimationPreview::EvaluateConfigurationHolistic(float Distance, FRotator AttackerRot, FRotator VictimRot)
{
	// Save original configuration
	float OriginalDistance = LockedDistance;
	FRotator OriginalAttackerRot = AttackerConfig.RotationOffset;
	FRotator OriginalVictimRot = VictimConfig.RotationOffset;
	float OriginalTime = CurrentTime;

	// Apply test configuration
	LockedDistance = Distance;
	AttackerConfig.RotationOffset = AttackerRot;
	VictimConfig.RotationOffset = VictimRot;
	ApplyCharacterConfigs();

	// Sample animation at key times and compute contact quality
	// CRITICAL FIX: Must call UpdateAnimations(t) before computing contact points!
	float WeightedScore = 0.0f;
	float TotalWeight = 0.0f;
	float TotalPenetrationPenalty = 0.0f;

	// Sample at multiple times across the animation
	const int32 NumSamples = 20;
	for (int32 i = 0; i <= NumSamples; ++i)
	{
		float t = (NumSamples > 0) ? (i * MaxDuration / NumSamples) : 0.0f;

		// CRITICAL: Update mesh pose to time t before computing contact points
		UpdateAnimations(t);

		FMultiContactAnalysis MultiAnalysis = ComputeMultiContactPoints(t);

		// Weight frames by contact quality (higher quality frames matter more)
		float FrameScore = MultiAnalysis.WeightedContactQuality;
		float Weight = 1.0f + FrameScore;  // Frames with good contact weighted higher

		WeightedScore += FrameScore * Weight;
		TotalWeight += Weight;

		if (MultiAnalysis.bHasPenetration)
		{
			TotalPenetrationPenalty += MultiAnalysis.MaxPenetrationDepth * Weight;
		}
	}

	// Restore original configuration
	LockedDistance = OriginalDistance;
	AttackerConfig.RotationOffset = OriginalAttackerRot;
	VictimConfig.RotationOffset = OriginalVictimRot;
	ApplyCharacterConfigs();
	UpdateAnimations(OriginalTime);

	if (TotalWeight <= 0.0f)
	{
		return 0.0f;
	}

	// Compute final score with penetration penalty
	float NormalizedScore = WeightedScore / TotalWeight;
	float NormalizedPenalty = (TotalPenetrationPenalty / TotalWeight) / 30.0f;
	NormalizedPenalty = FMath::Clamp(NormalizedPenalty, 0.0f, 0.3f);

	return NormalizedScore - NormalizedPenalty;
}

// ============================================================================
// OPTIMIZATION ENGINE
// ============================================================================

float SPairedAnimationPreview::EvaluateConfigurationAtFrame(float Distance, FRotator AttackerRot, FRotator VictimRot, float Time)
{
	// Evaluates configuration quality at a SINGLE frame.
	// Used for finding Global Paired Orientation (starting positions).
	// This is frame-independent - the optimal starting config doesn't change
	// based on what frame you're viewing.

	// Save original state
	float OriginalDistance = LockedDistance;
	FRotator OriginalAttackerRot = AttackerConfig.RotationOffset;
	FRotator OriginalVictimRot = VictimConfig.RotationOffset;
	float OriginalTime = CurrentTime;

	// Apply test configuration
	LockedDistance = Distance;
	AttackerConfig.RotationOffset = AttackerRot;
	VictimConfig.RotationOffset = VictimRot;
	ApplyCharacterConfigs();
	UpdateAnimations(Time);

	// Compute score at this specific frame
	FMultiContactAnalysis Analysis = ComputeMultiContactPoints(Time);
	float Score = Analysis.OverallContactQuality;

	// Penalize penetrations
	if (Analysis.bHasPenetration)
	{
		Score -= Analysis.MaxPenetrationDepth / 50.0f;
	}

	// Restore original state
	LockedDistance = OriginalDistance;
	AttackerConfig.RotationOffset = OriginalAttackerRot;
	VictimConfig.RotationOffset = OriginalVictimRot;
	ApplyCharacterConfigs();
	UpdateAnimations(OriginalTime);

	return Score;
}

float SPairedAnimationPreview::EvaluateConfiguration(float Distance, FRotator AttackerRot, FRotator VictimRot)
{
	// Legacy function - evaluate at reference frame (t=0) for starting orientation
	return EvaluateConfigurationAtFrame(Distance, AttackerRot, VictimRot, 0.0f);
}

float SPairedAnimationPreview::FindOptimalDistance(float MinDist, float MaxDist, int32 Steps)
{
	float BestDistance = LockedDistance;
	float BestScore = -1.0f;

	// Evaluate at reference frame (t=0) for Global Paired Orientation.
	// The starting distance doesn't depend on what frame you're viewing -
	// it's the setup position before the animation plays.
	const float ReferenceTime = 0.0f;

	for (int32 i = 0; i <= Steps; ++i)
	{
		float d = MinDist + (i * (MaxDist - MinDist) / Steps);
		float Score = EvaluateConfigurationAtFrame(d, AttackerConfig.RotationOffset, VictimConfig.RotationOffset, ReferenceTime);
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
	float BestScore = -1.0f;

	// Evaluate at reference frame (t=0) for Global Paired Orientation
	const float ReferenceTime = 0.0f;

	// Search full 360 degree range
	for (int32 i = 0; i <= Steps; ++i)
	{
		float Yaw = (i * 360.0f / Steps);
		FRotator TestRot(0.0f, Yaw, 0.0f);
		float Score = EvaluateConfigurationAtFrame(LockedDistance, TestRot, VictimConfig.RotationOffset, ReferenceTime);
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
	float BestScore = -1.0f;

	// Evaluate at reference frame (t=0) for Global Paired Orientation
	const float ReferenceTime = 0.0f;

	// Get rotation constraint from spatial relationship (PT-2)
	FSpatialRotationConstraint Constraint = GetRotationConstraintForRelationship();

	if (Constraint.IsConstrained())
	{
		// Constrained search: only search within the valid range for this relationship
		float MinYaw = Constraint.GetTargetYaw() - Constraint.GetTolerance();
		float MaxYaw = Constraint.GetTargetYaw() + Constraint.GetTolerance();
		float Range = MaxYaw - MinYaw;

		for (int32 i = 0; i <= Steps; ++i)
		{
			float Yaw = MinYaw + (i * Range / Steps);
			FRotator TestRot(0.0f, Yaw, 0.0f);
			float Score = EvaluateConfigurationAtFrame(LockedDistance, AttackerConfig.RotationOffset, TestRot, ReferenceTime);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestRotation = TestRot;
			}
		}
	}
	else
	{
		// Unconstrained search: full 360 degree range
		for (int32 i = 0; i <= Steps; ++i)
		{
			float Yaw = -180.0f + (i * 360.0f / Steps);
			FRotator TestRot(0.0f, Yaw, 0.0f);
			float Score = EvaluateConfigurationAtFrame(LockedDistance, AttackerConfig.RotationOffset, TestRot, ReferenceTime);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestRotation = TestRot;
			}
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

	// PT-10: Create progress dialog with cancellation support
	// 5 phases: Spatial inference, Distance, Attacker rotation, Victim rotation, Sync time
	const float TotalWorkUnits = 5.0f;
	FScopedSlowTask SlowTask(TotalWorkUnits, LOCTEXT("OptimizingPairedAnimation", "Optimizing Paired Animation..."));
	SlowTask.MakeDialog(true, false);  // bShowCancelButton=true, bShowProgressDialog=false (it's shown by MakeDialog)

	// Phase 0: Infer spatial relationship if set to Auto-Detect (PT-2)
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("Phase0_SpatialRelationship", "Analyzing spatial relationship..."));
	if (SlowTask.ShouldCancel())
	{
		Result.bWasCancelled = true;
		Result.Warnings.Add(TEXT("Optimization cancelled by user."));
		return Result;
	}

	if (CurrentSpatialRelationship == ESpatialRelationship::Inferred || bSpatialInferenceCacheDirty)
	{
		FSpatialRelationshipInference Inference = InferSpatialRelationship();
		Result.Suggestions.Add(FString::Printf(TEXT("Spatial relationship: %s (%.0f%% confidence) - %s"),
			*GetRelationshipDisplayName(Inference.InferredRelationship),
			Inference.Confidence * 100.0f,
			*Inference.ReasoningText));
	}
	else
	{
		// Report the user-selected relationship
		Result.Suggestions.Add(FString::Printf(TEXT("Using spatial relationship: %s (user-selected)"),
			*GetRelationshipDisplayName(CurrentSpatialRelationship)));
	}

	// CRITICAL: Reset to neutral baseline before optimization to ensure deterministic results.
	// Without this, sequential optimization flip-flops because each phase depends on previous state.
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator::ZeroRotator;
	LockedDistance = 150.0f;  // Neutral starting distance
	ApplyCharacterConfigs();

	// Phase 1: Find optimal distance (from neutral rotations)
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("Phase1_Distance", "Finding optimal distance (50 samples)..."));
	if (SlowTask.ShouldCancel())
	{
		Result.bWasCancelled = true;
		Result.Warnings.Add(TEXT("Optimization cancelled by user."));
		return Result;
	}
	Result.RecommendedDistance = FindOptimalDistance(50.0f, 400.0f, 50);

	// Phase 2: Find optimal attacker rotation at that distance (victim still at 0)
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("Phase2_AttackerRotation", "Finding optimal attacker rotation (36 samples)..."));
	if (SlowTask.ShouldCancel())
	{
		Result.bWasCancelled = true;
		Result.Warnings.Add(TEXT("Optimization cancelled by user."));
		return Result;
	}
	LockedDistance = Result.RecommendedDistance;
	ApplyCharacterConfigs();
	Result.RecommendedAttackerRotation = FindOptimalAttackerRotation(36);

	// Phase 3: Find optimal victim rotation (now with optimal attacker rotation)
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("Phase3_VictimRotation", "Finding optimal victim rotation (36 samples)..."));
	if (SlowTask.ShouldCancel())
	{
		Result.bWasCancelled = true;
		Result.Warnings.Add(TEXT("Optimization cancelled by user."));
		return Result;
	}
	AttackerConfig.RotationOffset = Result.RecommendedAttackerRotation;
	ApplyCharacterConfigs();
	Result.RecommendedVictimRotation = FindOptimalVictimRotation(36);

	// Phase 4: Find optimal sync time
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("Phase4_SyncTime", "Finding optimal sync time..."));
	if (SlowTask.ShouldCancel())
	{
		Result.bWasCancelled = true;
		Result.Warnings.Add(TEXT("Optimization cancelled by user."));
		return Result;
	}
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

	// Push current state to history for undo support (PT-19)
	PushStateToHistory(TEXT("Before Optimization"));

	LockedDistance = Result.RecommendedDistance;
	AttackerConfig.RotationOffset = Result.RecommendedAttackerRotation;
	VictimConfig.RotationOffset = Result.RecommendedVictimRotation;
	ApplyCharacterConfigs();

	// Don't change CurrentTime - optimization is holistic and frame-independent
	// User should stay on whatever frame they were viewing
	UpdateAnimations(CurrentTime);

	bAnalysisCacheDirty = true;
}

// ============================================================================
// UNDO/REDO SUPPORT (PT-19)
// ============================================================================

void SPairedAnimationPreview::PushStateToHistory(const FString& Description)
{
	// Clear any redo states if we're not at the end of history
	if (CurrentHistoryIndex < OptimizationHistory.Num() - 1)
	{
		OptimizationHistory.SetNum(CurrentHistoryIndex + 1);
	}

	// Capture current state
	FPreviewOptimizationState State = FPreviewOptimizationState::CreateFromValues(
		LockedDistance,
		AttackerConfig.RotationOffset,
		VictimConfig.RotationOffset,
		Description
	);

	// Add to history
	OptimizationHistory.Add(State);
	CurrentHistoryIndex = OptimizationHistory.Num() - 1;

	// Limit history size
	if (OptimizationHistory.Num() > MaxHistorySize)
	{
		OptimizationHistory.RemoveAt(0);
		CurrentHistoryIndex = FMath::Max(0, CurrentHistoryIndex - 1);
	}
}

void SPairedAnimationPreview::UndoOptimization()
{
	if (!CanUndo())
	{
		return;
	}

	// Apply the current history state (which was saved before the change)
	const FPreviewOptimizationState& State = OptimizationHistory[CurrentHistoryIndex];
	ApplyHistoryState(State);

	// Move back in history
	CurrentHistoryIndex--;
}

void SPairedAnimationPreview::RedoOptimization()
{
	if (!CanRedo())
	{
		return;
	}

	// Move forward in history
	CurrentHistoryIndex++;

	// If there's a next state, apply it
	if (CurrentHistoryIndex + 1 < OptimizationHistory.Num())
	{
		// We need to look at the state AFTER the current index to get what was changed TO
		// This is a bit tricky - our history stores "before" states
		// So redo means: we're at state N, user wants to go to state N+1
		// But we only stored "before" states, so we need to invert the logic
	}

	// For simplicity, just apply the state at the new index
	// The redo stack would ideally store "after" states too, but for now
	// we'll note this as a limitation
	if (CurrentHistoryIndex < OptimizationHistory.Num())
	{
		const FPreviewOptimizationState& State = OptimizationHistory[CurrentHistoryIndex];
		ApplyHistoryState(State);
	}
}

void SPairedAnimationPreview::ApplyHistoryState(const FPreviewOptimizationState& State)
{
	LockedDistance = State.Distance;
	AttackerConfig.RotationOffset = State.AttackerRotation;
	VictimConfig.RotationOffset = State.VictimRotation;

	ApplyCharacterConfigs();
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

	// PT-10: Don't apply or update display if user cancelled the operation
	if (Result.bWasCancelled)
	{
		return;
	}

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

	// PT-22: Add progress feedback for distance optimization
	const int32 Steps = 50;
	FScopedSlowTask SlowTask(Steps + 1, LOCTEXT("OptimizingDistance", "Finding Optimal Distance..."));
	SlowTask.MakeDialog(true);

	float BestDistance = LockedDistance;
	float BestScore = -1.0f;
	const float ReferenceTime = 0.0f;
	const float MinDist = 50.0f;
	const float MaxDist = 400.0f;

	for (int32 i = 0; i <= Steps; ++i)
	{
		if (SlowTask.ShouldCancel())
		{
			return; // User cancelled
		}
		SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("TestingDistance", "Testing distance {0}..."), FText::AsNumber(i + 1)));

		float d = MinDist + (i * (MaxDist - MinDist) / Steps);
		float Score = EvaluateConfigurationAtFrame(d, AttackerConfig.RotationOffset, VictimConfig.RotationOffset, ReferenceTime);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestDistance = d;
		}
	}

	LockedDistance = BestDistance;
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

	// PT-22: Add progress feedback for rotation optimization
	const int32 Steps = 36;
	const int32 TotalSteps = (Steps + 1) * 2; // Attacker + Victim
	FScopedSlowTask SlowTask(TotalSteps, LOCTEXT("OptimizingRotation", "Finding Optimal Rotations..."));
	SlowTask.MakeDialog(true);

	const float ReferenceTime = 0.0f;

	// Phase 1: Find optimal attacker rotation
	{
		FRotator BestRotation = AttackerConfig.RotationOffset;
		float BestScore = -1.0f;

		for (int32 i = 0; i <= Steps; ++i)
		{
			if (SlowTask.ShouldCancel())
			{
				return;
			}
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("TestingAttackerRot", "Attacker rotation {0}/36..."), FText::AsNumber(i + 1)));

			float Yaw = (i * 360.0f / Steps);
			FRotator TestRot(0.0f, Yaw, 0.0f);
			float Score = EvaluateConfigurationAtFrame(LockedDistance, TestRot, VictimConfig.RotationOffset, ReferenceTime);
			if (Score > BestScore)
			{
				BestScore = Score;
				BestRotation = TestRot;
			}
		}
		AttackerConfig.RotationOffset = BestRotation;
	}

	// Phase 2: Find optimal victim rotation (with updated attacker rotation)
	{
		FRotator BestRotation = VictimConfig.RotationOffset;
		float BestScore = -1.0f;
		FSpatialRotationConstraint Constraint = GetRotationConstraintForRelationship();

		if (Constraint.IsConstrained())
		{
			float MinYaw = Constraint.GetTargetYaw() - Constraint.GetTolerance();
			float MaxYaw = Constraint.GetTargetYaw() + Constraint.GetTolerance();
			float Range = MaxYaw - MinYaw;

			for (int32 i = 0; i <= Steps; ++i)
			{
				if (SlowTask.ShouldCancel())
				{
					return;
				}
				SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("TestingVictimRot", "Victim rotation {0}/36..."), FText::AsNumber(i + 1)));

				float Yaw = MinYaw + (i * Range / Steps);
				FRotator TestRot(0.0f, Yaw, 0.0f);
				float Score = EvaluateConfigurationAtFrame(LockedDistance, AttackerConfig.RotationOffset, TestRot, ReferenceTime);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestRotation = TestRot;
				}
			}
		}
		else
		{
			for (int32 i = 0; i <= Steps; ++i)
			{
				if (SlowTask.ShouldCancel())
				{
					return;
				}
				SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("TestingVictimRotUnconstr", "Victim rotation {0}/36..."), FText::AsNumber(i + 1)));

				float Yaw = -180.0f + (i * 360.0f / Steps);
				FRotator TestRot(0.0f, Yaw, 0.0f);
				float Score = EvaluateConfigurationAtFrame(LockedDistance, AttackerConfig.RotationOffset, TestRot, ReferenceTime);
				if (Score > BestScore)
				{
					BestScore = Score;
					BestRotation = TestRot;
				}
			}
		}
		VictimConfig.RotationOffset = BestRotation;
	}

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

	// PT-22: Add progress feedback for sync time analysis
	// Calculate number of samples based on duration and sample rate
	const int32 NumSamples = (MaxDuration > 0.0f) ? FMath::CeilToInt(MaxDuration * AnalysisSampleRate) + 1 : 30;
	FScopedSlowTask SlowTask(NumSamples + 2, LOCTEXT("FindingSyncTime", "Finding Optimal Sync Time..."));
	SlowTask.MakeDialog(true);

	// Phase 1: Rebuild analysis cache with progress
	if (MaxDuration > 0.0f)
	{
		float TimeStep = 1.0f / AnalysisSampleRate;
		for (float t = 0.0f; t <= MaxDuration; t += TimeStep)
		{
			if (SlowTask.ShouldCancel())
			{
				return;
			}
			SlowTask.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("AnalyzingFrame", "Analyzing frame at {0}s..."),
				FText::AsNumber(t, &FNumberFormattingOptions::DefaultNoGrouping())));
			FrameAnalysisCache.Add(AnalyzeFrame(t));
		}
	}

	// Phase 2: Rebuild analysis
	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("BuildingDistanceAnalysis", "Building distance analysis..."));
	if (SlowTask.ShouldCancel()) return;
	RebuildDistanceAnalysis();

	SlowTask.EnterProgressFrame(1.0f, LOCTEXT("BuildingTimingAnalysis", "Building timing analysis..."));
	if (SlowTask.ShouldCancel()) return;
	RebuildTimingAnalysis();

	bAnalysisCacheDirty = false;

	// Jump to optimal sync time
	CurrentTime = TimingAnalysis.BestSyncTime;
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

// ============================================================================
// MULTI-CONTACT POINT ANALYSIS
// ============================================================================

void SPairedAnimationPreview::InitializeContactTypeWeights()
{
	// Weight contact types by importance for paired animation quality
	ContactTypeWeights.Empty();

	// Body contact types
	ContactTypeWeights.Add(EContactPointType::Head, 1.0f);       // Head contact = most important (kill shots)
	ContactTypeWeights.Add(EContactPointType::RightHand, 0.9f);  // Weapon hand (attacker)
	ContactTypeWeights.Add(EContactPointType::LeftHand, 0.6f);   // Support hand
	ContactTypeWeights.Add(EContactPointType::Pelvis, 0.5f);     // Body stability
	ContactTypeWeights.Add(EContactPointType::RightFoot, 0.3f);  // Positioning
	ContactTypeWeights.Add(EContactPointType::LeftFoot, 0.3f);   // Positioning

	// Weapon contact types - highest priority when weapons are available
	ContactTypeWeights.Add(EContactPointType::WeaponTip, 1.0f);  // Weapon tip = primary strike point
	ContactTypeWeights.Add(EContactPointType::WeaponMid, 0.8f);  // Mid-blade for slashing
	ContactTypeWeights.Add(EContactPointType::WeaponBase, 0.6f); // Hilt/base for close combat
}

float SPairedAnimationPreview::GetPenetrationThreshold(EContactPointType Type) const
{
	// Return capsule/bone/weapon radius estimate for penetration detection
	switch (Type)
	{
		// Body contact types
		case EContactPointType::Head: return 12.0f;
		case EContactPointType::Pelvis: return 15.0f;
		case EContactPointType::LeftHand:
		case EContactPointType::RightHand: return 8.0f;
		case EContactPointType::LeftFoot:
		case EContactPointType::RightFoot: return 10.0f;

		// Weapon contact types - smaller radii for precise contact
		case EContactPointType::WeaponTip: return 3.0f;   // Sharp tip
		case EContactPointType::WeaponMid: return 5.0f;   // Blade width
		case EContactPointType::WeaponBase: return 8.0f;  // Hilt/guard area

		default: return 10.0f;
	}
}

float SPairedAnimationPreview::ComputePairwiseDistance(EContactPointType AttackerContact, EContactPointType VictimContact) const
{
	if (!AttackerMeshComponent || !VictimMeshComponent) return FLT_MAX;

	FName AttackerBone = AttackerBoneConfig.GetBoneForType(AttackerContact);
	FName VictimBone = VictimBoneConfig.GetBoneForType(VictimContact);

	FVector AttackerPos = GetBoneWorldLocation(AttackerMeshComponent, AttackerBone);
	FVector VictimPos = GetBoneWorldLocation(VictimMeshComponent, VictimBone);

	return FVector::Dist(AttackerPos, VictimPos);
}

bool SPairedAnimationPreview::DetectPenetration(float Distance, EContactPointType AttackerType, EContactPointType VictimType, float& OutPenetrationDepth) const
{
	float AttackerRadius = GetPenetrationThreshold(AttackerType);
	float VictimRadius = GetPenetrationThreshold(VictimType);
	float CombinedRadius = AttackerRadius + VictimRadius;

	if (Distance < CombinedRadius)
	{
		OutPenetrationDepth = CombinedRadius - Distance;
		return true;
	}

	OutPenetrationDepth = 0.0f;
	return false;
}

FMultiContactAnalysis SPairedAnimationPreview::ComputeMultiContactPoints(float Time)
{
	FMultiContactAnalysis Result;
	Result.Time = Time;

	if (!AttackerMeshComponent || !VictimMeshComponent) return Result;

	// Update animations to this time
	UpdateAnimations(Time);

	// Initialize weights if not done
	if (ContactTypeWeights.Num() == 0)
	{
		InitializeContactTypeWeights();
	}

	float BestQuality = 0.0f;

	// Compute all contact point positions
	for (int32 TypeIdx = 0; TypeIdx < static_cast<int32>(EContactPointType::COUNT); ++TypeIdx)
	{
		EContactPointType ContactType = static_cast<EContactPointType>(TypeIdx);

		// Handle weapon vs body contact types differently
		if (FMultiContactBoneConfig::IsWeaponContactType(ContactType))
		{
			// Weapon contact types - use weapon mesh positions if available
			if (HasAttackerWeapon())
			{
				FVector AttackerPos = GetWeaponContactPosition(AttackerWeaponMeshComponent, AttackerWeaponConfig, ContactType);
				Result.AttackerContactPositions.Add(ContactType, AttackerPos);
			}

			if (HasVictimWeapon())
			{
				FVector VictimPos = GetWeaponContactPosition(VictimWeaponMeshComponent, VictimWeaponConfig, ContactType);
				Result.VictimContactPositions.Add(ContactType, VictimPos);
			}
		}
		else
		{
			// Body contact types - use bone positions
			FName AttackerBone = AttackerBoneConfig.GetBoneForType(ContactType);
			FVector AttackerPos = GetBoneWorldLocation(AttackerMeshComponent, AttackerBone);
			Result.AttackerContactPositions.Add(ContactType, AttackerPos);

			FName VictimBone = VictimBoneConfig.GetBoneForType(ContactType);
			FVector VictimPos = GetBoneWorldLocation(VictimMeshComponent, VictimBone);
			Result.VictimContactPositions.Add(ContactType, VictimPos);
		}
	}

	// Compute pairwise distances and detect penetrations
	for (int32 AType = 0; AType < static_cast<int32>(EContactPointType::COUNT); ++AType)
	{
		EContactPointType AttackerType = static_cast<EContactPointType>(AType);

		// Skip if attacker position wasn't computed (e.g., weapon type without weapon mesh)
		FVector* AttackerPosPtr = Result.AttackerContactPositions.Find(AttackerType);
		if (!AttackerPosPtr) continue;
		FVector AttackerPos = *AttackerPosPtr;

		for (int32 VType = 0; VType < static_cast<int32>(EContactPointType::COUNT); ++VType)
		{
			EContactPointType VictimType = static_cast<EContactPointType>(VType);

			// Skip if victim position wasn't computed (e.g., weapon type without weapon mesh)
			FVector* VictimPosPtr = Result.VictimContactPositions.Find(VictimType);
			if (!VictimPosPtr) continue;
			FVector VictimPos = *VictimPosPtr;

			float Distance = FVector::Dist(AttackerPos, VictimPos);
			Result.PairwiseDistances.Add(TPair<EContactPointType, EContactPointType>(AttackerType, VictimType), Distance);

			// Check penetration
			float PenetrationDepth = 0.0f;
			if (DetectPenetration(Distance, AttackerType, VictimType, PenetrationDepth))
			{
				Result.PenetrationPairs.Add(TPair<EContactPointType, EContactPointType>(AttackerType, VictimType));
				Result.bHasPenetration = true;
				if (PenetrationDepth > Result.MaxPenetrationDepth)
				{
					Result.MaxPenetrationDepth = PenetrationDepth;
					Result.MostPenetratingAttacker = AttackerType;
					Result.MostPenetratingVictim = VictimType;
				}
			}

			// Check contact quality (within threshold)
			if (Distance < ContactThreshold)
			{
				float DistanceQuality = FMath::Max(0.0f, 1.0f - (Distance / ContactThreshold));

				// Store quality for this contact type (keep best)
				float* ExistingAttackerQuality = Result.AttackerContactQualities.Find(AttackerType);
				if (!ExistingAttackerQuality || DistanceQuality > *ExistingAttackerQuality)
				{
					Result.AttackerContactQualities.Add(AttackerType, DistanceQuality);
				}

				float* ExistingVictimQuality = Result.VictimContactQualities.Find(VictimType);
				if (!ExistingVictimQuality || DistanceQuality > *ExistingVictimQuality)
				{
					Result.VictimContactQualities.Add(VictimType, DistanceQuality);
				}

				// Track best contact pair
				float Weight = ContactTypeWeights.Contains(AttackerType) ? ContactTypeWeights[AttackerType] : 0.5f;
				float WeightedQuality = DistanceQuality * Weight;
				if (WeightedQuality > BestQuality)
				{
					BestQuality = WeightedQuality;
					Result.BestAttackerContact = AttackerType;
					Result.BestVictimContact = VictimType;
					Result.BestContactDistance = Distance;
					Result.BestContactQuality = DistanceQuality;
				}

				Result.TotalActiveContacts++;
			}
		}
	}

	// Compute weighted quality score
	Result.WeightedContactQuality = EvaluateMultiContactQuality(Result);

	return Result;
}

float SPairedAnimationPreview::EvaluateMultiContactQuality(const FMultiContactAnalysis& Analysis) const
{
	if (ContactTypeWeights.Num() == 0)
	{
		return Analysis.BestContactQuality;
	}

	float WeightedSum = 0.0f;
	float WeightSum = 0.0f;

	// Sum up weighted contact qualities
	for (const auto& Pair : Analysis.AttackerContactQualities)
	{
		float Weight = ContactTypeWeights.Contains(Pair.Key) ? ContactTypeWeights[Pair.Key] : 0.5f;
		WeightedSum += Pair.Value * Weight;
		WeightSum += Weight;
	}

	// Penalize penetrations (overlapping meshes look bad)
	float PenetrationPenalty = FMath::Clamp(Analysis.MaxPenetrationDepth / 20.0f, 0.0f, 0.5f);

	float BaseQuality = (WeightSum > 0.0f) ? (WeightedSum / WeightSum) : 0.0f;
	return BaseQuality * (1.0f - PenetrationPenalty);
}

float SPairedAnimationPreview::EvaluateConfigurationWithMultiContact(float Distance, FRotator AttackerRot, FRotator VictimRot)
{
	// Temporarily apply configuration
	float OriginalDistance = LockedDistance;
	FRotator OriginalAttackerRot = AttackerConfig.RotationOffset;
	FRotator OriginalVictimRot = VictimConfig.RotationOffset;

	LockedDistance = Distance;
	AttackerConfig.RotationOffset = AttackerRot;
	VictimConfig.RotationOffset = VictimRot;
	ApplyCharacterConfigs();

	// Evaluate using multi-contact analysis
	float TotalScore = 0.0f;
	int32 SampleCount = 0;
	float BestMultiContactQuality = 0.0f;
	float TotalPenetrationPenalty = 0.0f;

	const int32 NumSamples = 30;
	for (int32 i = 0; i <= NumSamples; ++i)
	{
		float t = (NumSamples > 0) ? (i * MaxDuration / NumSamples) : 0.0f;
		FMultiContactAnalysis MultiAnalysis = ComputeMultiContactPoints(t);

		TotalScore += MultiAnalysis.WeightedContactQuality;
		if (MultiAnalysis.WeightedContactQuality > BestMultiContactQuality)
		{
			BestMultiContactQuality = MultiAnalysis.WeightedContactQuality;
		}
		if (MultiAnalysis.bHasPenetration)
		{
			TotalPenetrationPenalty += MultiAnalysis.MaxPenetrationDepth;
		}
		SampleCount++;
	}

	// Restore original configuration
	LockedDistance = OriginalDistance;
	AttackerConfig.RotationOffset = OriginalAttackerRot;
	VictimConfig.RotationOffset = OriginalVictimRot;
	ApplyCharacterConfigs();

	// Score: 60% peak quality + 40% average quality - penetration penalty
	float AverageQuality = (SampleCount > 0) ? (TotalScore / SampleCount) : 0.0f;
	float AveragePenetration = (SampleCount > 0) ? (TotalPenetrationPenalty / SampleCount) : 0.0f;
	float PenetrationPenalty = FMath::Clamp(AveragePenetration / 30.0f, 0.0f, 0.3f);

	return (BestMultiContactQuality * 0.6f) + (AverageQuality * 0.4f) - PenetrationPenalty;
}

void SPairedAnimationPreview::DrawMultiContactPoints()
{
	if (!SharedPreviewScene) return;
	UWorld* World = SharedPreviewScene->GetWorld();
	if (!World) return;

	FMultiContactAnalysis Analysis = ComputeMultiContactPoints(CurrentTime);

	// PT-16: Color coding for quality using centralized config
	auto GetQualityColor = [](float Quality) -> FColor
	{
		// Red (bad) -> Yellow (mediocre) -> Green (good)
		if (Quality < PairedAnimPreviewConfig::Analysis::MediocreContactThreshold)
		{
			return PairedAnimPreviewConfig::Colors::ContactBad;
		}
		else if (Quality < PairedAnimPreviewConfig::Analysis::GoodContactThreshold)
		{
			return PairedAnimPreviewConfig::Colors::ContactMediocre;
		}
		return PairedAnimPreviewConfig::Colors::ContactGood;
	};

	// Draw attacker contact points
	for (const auto& Pair : Analysis.AttackerContactPositions)
	{
		FVector Pos = Pair.Value;
		float* QualityPtr = Analysis.AttackerContactQualities.Find(Pair.Key);
		float Quality = QualityPtr ? *QualityPtr : 0.0f;

		FColor Color = GetQualityColor(Quality);
		float Radius = 5.0f + (Quality * 8.0f);  // Larger = better quality

		DrawDebugSphere(World, Pos, Radius, 8, Color, false, -1.0f, 0, 1.5f);
	}

	// Draw victim contact points
	for (const auto& Pair : Analysis.VictimContactPositions)
	{
		FVector Pos = Pair.Value;
		float* QualityPtr = Analysis.VictimContactQualities.Find(Pair.Key);
		float Quality = QualityPtr ? *QualityPtr : 0.0f;

		FColor Color = GetQualityColor(Quality);
		float Radius = 5.0f + (Quality * 8.0f);

		DrawDebugSphere(World, Pos, Radius, 8, Color, false, -1.0f, 0, 1.5f);
	}

	// PT-16: Draw lines between penetrating pairs using centralized config
	for (const auto& PenPair : Analysis.PenetrationPairs)
	{
		FVector AttackerPos = Analysis.AttackerContactPositions[PenPair.Key];
		FVector VictimPos = Analysis.VictimContactPositions[PenPair.Value];

		DrawDebugLine(World, AttackerPos, VictimPos, PairedAnimPreviewConfig::Colors::CurrentConnection,
			false, -1.0f, 0, PairedAnimPreviewConfig::Sizes::ThickLineWidth);
	}

	// Draw line to best contact pair
	if (Analysis.BestContactQuality > 0.0f)
	{
		FVector BestAttackerPos = Analysis.AttackerContactPositions[Analysis.BestAttackerContact];
		FVector BestVictimPos = Analysis.VictimContactPositions[Analysis.BestVictimContact];

		DrawDebugLine(World, BestAttackerPos, BestVictimPos, PairedAnimPreviewConfig::Colors::OptimalConnection,
			false, -1.0f, 0, PairedAnimPreviewConfig::Sizes::MediumLineWidth);
	}
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

	// Draw reference grid first (so other visualization draws on top)
	if (IsVisualizationActive(EVisualizationLayer::AxisGrid)) DrawAxisGrid();

	if (IsVisualizationActive(EVisualizationLayer::ContactPoints)) DrawContactPoints();
	if (IsVisualizationActive(EVisualizationLayer::WeaponTrace)) DrawWeaponTrace();
	if (IsVisualizationActive(EVisualizationLayer::DistanceLines)) DrawDistanceLines();
	if (IsVisualizationActive(EVisualizationLayer::CenterOfMass)) DrawCenterOfMass();
	if (IsVisualizationActive(EVisualizationLayer::VelocityVectors)) DrawVelocityVectors();
	if (IsVisualizationActive(EVisualizationLayer::MultiContact)) DrawMultiContactPoints();
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

void SPairedAnimationPreview::DrawAxisGrid()
{
	if (!SharedPreviewScene) return;
	UWorld* World = SharedPreviewScene->GetWorld();
	if (!World) return;

	// Configuration - clean, professional look
	const float GridSize = 200.0f;
	const float GridStep = 50.0f;
	const float AxisLength = 100.0f;
	const FVector Origin = FVector::ZeroVector;

	// Bright, saturated colors matching Unreal's viewport gizmo
	const FColor XAxisColor = FColor(255, 80, 80);		// Bright red
	const FColor YAxisColor = FColor(80, 255, 80);		// Bright green
	const FColor ZAxisColor = FColor(80, 130, 255);		// Bright blue
	const FColor GridColor = FColor(60, 60, 60);		// Subtle dark gray

	// --- Simple floor grid (XY plane) ---
	const float HalfGrid = GridSize / 2.0f;
	for (float X = -HalfGrid; X <= HalfGrid; X += GridStep)
	{
		if (!FMath::IsNearlyZero(X))  // Skip center lines, axes will cover them
		{
			DrawDebugLine(World, FVector(X, -HalfGrid, 0.0f), FVector(X, HalfGrid, 0.0f),
				GridColor, false, -1.0f, 0, 0.5f);
		}
	}
	for (float Y = -HalfGrid; Y <= HalfGrid; Y += GridStep)
	{
		if (!FMath::IsNearlyZero(Y))
		{
			DrawDebugLine(World, FVector(-HalfGrid, Y, 0.0f), FVector(HalfGrid, Y, 0.0f),
				GridColor, false, -1.0f, 0, 0.5f);
		}
	}

	// --- Main axis lines (thick, prominent) ---
	// X Axis (Red) - FORWARD in Unreal
	DrawDebugLine(World, FVector(-20.0f, 0.0f, 0.0f), FVector(AxisLength, 0.0f, 0.0f),
		XAxisColor, false, -1.0f, 0, 4.0f);
	// Arrowhead for X
	DrawDebugLine(World, FVector(AxisLength, 0.0f, 0.0f), FVector(AxisLength - 15.0f, 8.0f, 0.0f),
		XAxisColor, false, -1.0f, 0, 3.0f);
	DrawDebugLine(World, FVector(AxisLength, 0.0f, 0.0f), FVector(AxisLength - 15.0f, -8.0f, 0.0f),
		XAxisColor, false, -1.0f, 0, 3.0f);

	// Y Axis (Green) - RIGHT in Unreal
	DrawDebugLine(World, FVector(0.0f, -20.0f, 0.0f), FVector(0.0f, AxisLength, 0.0f),
		YAxisColor, false, -1.0f, 0, 4.0f);
	// Arrowhead for Y
	DrawDebugLine(World, FVector(0.0f, AxisLength, 0.0f), FVector(8.0f, AxisLength - 15.0f, 0.0f),
		YAxisColor, false, -1.0f, 0, 3.0f);
	DrawDebugLine(World, FVector(0.0f, AxisLength, 0.0f), FVector(-8.0f, AxisLength - 15.0f, 0.0f),
		YAxisColor, false, -1.0f, 0, 3.0f);

	// Z Axis (Blue) - UP in Unreal
	DrawDebugLine(World, Origin, FVector(0.0f, 0.0f, AxisLength),
		ZAxisColor, false, -1.0f, 0, 4.0f);
	// Arrowhead for Z
	DrawDebugLine(World, FVector(0.0f, 0.0f, AxisLength), FVector(8.0f, 0.0f, AxisLength - 15.0f),
		ZAxisColor, false, -1.0f, 0, 3.0f);
	DrawDebugLine(World, FVector(0.0f, 0.0f, AxisLength), FVector(-8.0f, 0.0f, AxisLength - 15.0f),
		ZAxisColor, false, -1.0f, 0, 3.0f);

	// --- Axis labels using simple letter shapes ---
	const float LabelOffset = AxisLength + 20.0f;
	const float LetterSize = 8.0f;

	// "X" label (two crossing lines)
	FVector XLabelPos = FVector(LabelOffset, 0.0f, 0.0f);
	DrawDebugLine(World, XLabelPos + FVector(0, -LetterSize, -LetterSize), XLabelPos + FVector(0, LetterSize, LetterSize),
		XAxisColor, false, -1.0f, 0, 2.5f);
	DrawDebugLine(World, XLabelPos + FVector(0, -LetterSize, LetterSize), XLabelPos + FVector(0, LetterSize, -LetterSize),
		XAxisColor, false, -1.0f, 0, 2.5f);

	// "Y" label (V with stem)
	FVector YLabelPos = FVector(0.0f, LabelOffset, 0.0f);
	DrawDebugLine(World, YLabelPos + FVector(-LetterSize, 0, LetterSize), YLabelPos,
		YAxisColor, false, -1.0f, 0, 2.5f);
	DrawDebugLine(World, YLabelPos + FVector(LetterSize, 0, LetterSize), YLabelPos,
		YAxisColor, false, -1.0f, 0, 2.5f);
	DrawDebugLine(World, YLabelPos, YLabelPos + FVector(0, 0, -LetterSize),
		YAxisColor, false, -1.0f, 0, 2.5f);

	// "Z" label (three horizontal lines connected)
	FVector ZLabelPos = FVector(0.0f, 0.0f, LabelOffset);
	DrawDebugLine(World, ZLabelPos + FVector(-LetterSize, -LetterSize, 0), ZLabelPos + FVector(LetterSize, -LetterSize, 0),
		ZAxisColor, false, -1.0f, 0, 2.5f);
	DrawDebugLine(World, ZLabelPos + FVector(LetterSize, -LetterSize, 0), ZLabelPos + FVector(-LetterSize, LetterSize, 0),
		ZAxisColor, false, -1.0f, 0, 2.5f);
	DrawDebugLine(World, ZLabelPos + FVector(-LetterSize, LetterSize, 0), ZLabelPos + FVector(LetterSize, LetterSize, 0),
		ZAxisColor, false, -1.0f, 0, 2.5f);

	// --- Character forward direction indicators (at pelvis height for visibility) ---
	const float PelvisHeight = 100.0f;

	if (AttackerMeshComponent)
	{
		FVector AttackerPos = AttackerMeshComponent->GetComponentLocation();
		FVector ArrowStart = FVector(AttackerPos.X, AttackerPos.Y, PelvisHeight);
		FVector AttackerForward = AttackerMeshComponent->GetForwardVector();
		FVector ArrowEnd = ArrowStart + AttackerForward * 80.0f;

		// Thicker cyan arrow for attacker forward
		DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 15.0f, FColor::Cyan, false, -1.0f, 0, 3.0f);
	}

	if (VictimMeshComponent)
	{
		FVector VictimPos = VictimMeshComponent->GetComponentLocation();
		FVector ArrowStart = FVector(VictimPos.X, VictimPos.Y, PelvisHeight);
		FVector VictimForward = VictimMeshComponent->GetForwardVector();
		FVector ArrowEnd = ArrowStart + VictimForward * 80.0f;

		// Thicker orange arrow for victim forward
		DrawDebugDirectionalArrow(World, ArrowStart, ArrowEnd, 15.0f, FColor::Orange, false, -1.0f, 0, 3.0f);
	}
}

// ============================================================================
// PLAYBACK CONTROLS
// ============================================================================

void SPairedAnimationPreview::OnTimelineValueChanged(float NewValue)
{
	// Convert slider value (0-1) to time within section range (MinTime to MaxDuration)
	float Range = MaxDuration - MinTime;
	CurrentTime = MinTime + (NewValue * Range);
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
	CurrentTime = FMath::Max(CurrentTime - (1.0f / 60.0f), MinTime);
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
	CurrentTime = FMath::Max(CurrentTime - 0.1f, MinTime);
	UpdateAnimations(CurrentTime);
	UpdateAnalyticsDisplay();
}

void SPairedAnimationPreview::OnResetClicked()
{
	CurrentTime = MinTime;  // Reset to section start, not 0
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
		// PT-22: Add progress feedback for trajectory cache rebuild
		RebuildTrajectoryCacheWithProgress();
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
	if (bIsPlaying && MaxDuration > MinTime)
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
			else if (CurrentTime <= MinTime)
			{
				CurrentTime = MinTime;
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
					// Loop back to section start, not 0
					CurrentTime = MinTime;
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
	if (TimelineSlider.IsValid() && MaxDuration > MinTime)
	{
		// Normalize slider value within section range (MinTime to MaxDuration)
		float Range = MaxDuration - MinTime;
		TimelineSlider->SetValue((CurrentTime - MinTime) / Range);
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
		FText::AsNumber(CurrentTime, &GetNumberFormat(2)),
		FText::AsNumber(MaxDuration, &GetNumberFormat(2)),
		FText::AsNumber(GetAttackerTime(), &GetNumberFormat(2)),
		FText::AsNumber(GetVictimTime(), &GetNumberFormat(2)));
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
		FText::AsNumber(Primary.Confidence * 100.0f, &GetNumberFormat(0)),
		FText::AsNumber(Primary.Distance, &GetNumberFormat(1)),
		FText::AsNumber(Primary.ImpactSpeed, &GetNumberFormat(0)));
}

FText SPairedAnimationPreview::GetDistanceInfoText() const
{
	FPairedFrameAnalysis Analysis = const_cast<SPairedAnimationPreview*>(this)->AnalyzeFrame(CurrentTime);

	return FText::Format(
		LOCTEXT("DistanceInfo", "Center Dist: {0}u  |  Bone Dist: {1}u ({2} ↔ {3})"),
		FText::AsNumber(Analysis.CharacterDistance, &GetNumberFormat(1)),
		FText::AsNumber(Analysis.ClosestBoneDistance, &GetNumberFormat(1)),
		FText::FromName(Analysis.AttackerClosestBone),
		FText::FromName(Analysis.VictimClosestBone));
}

FText SPairedAnimationPreview::GetVelocityInfoText() const
{
	FPairedFrameAnalysis Analysis = const_cast<SPairedAnimationPreview*>(this)->AnalyzeFrame(CurrentTime);

	return FText::Format(
		LOCTEXT("VelocityInfo", "Weapon Speed: {0} u/s  |  Direction: ({1}, {2}, {3})"),
		FText::AsNumber(Analysis.WeaponSpeed, &GetNumberFormat(0)),
		FText::AsNumber(Analysis.WeaponVelocity.X, &GetNumberFormat(0)),
		FText::AsNumber(Analysis.WeaponVelocity.Y, &GetNumberFormat(0)),
		FText::AsNumber(Analysis.WeaponVelocity.Z, &GetNumberFormat(0)));
}

FText SPairedAnimationPreview::GetOptimizationInfoText() const
{
	if (!LastOptimizationResult.bSuccess)
	{
		return LOCTEXT("NoOptimization", "Run optimization to get recommendations");
	}

	return FText::Format(
		LOCTEXT("OptimizationInfo", "Score: {0}%  |  Dist: {1}u  |  Sync: {2}s  |  Contact: {3}%"),
		FText::AsNumber(LastOptimizationResult.OverallScore * 100.0f, &GetNumberFormat(0)),
		FText::AsNumber(LastOptimizationResult.RecommendedDistance, &GetNumberFormat(0)),
		FText::AsNumber(LastOptimizationResult.RecommendedSyncTime, &GetNumberFormat(2)),
		FText::AsNumber(LastOptimizationResult.ContactQuality * 100.0f, &GetNumberFormat(0)));
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
// ORIENTATION PRESETS
// ============================================================================

void SPairedAnimationPreview::ApplyOrientationPreset_Facing()
{
	// Characters facing each other - victim rotated 180° to face attacker
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator(0.0f, 180.0f, 0.0f);
	CurrentSpatialRelationship = ESpatialRelationship::Facing;
	bSpatialInferenceCacheDirty = true;
	ApplyCharacterConfigs();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::ApplyOrientationPreset_Behind()
{
	// Attacker behind victim - victim at 0° (back to attacker)
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator::ZeroRotator;
	CurrentSpatialRelationship = ESpatialRelationship::Behind;
	bSpatialInferenceCacheDirty = true;
	ApplyCharacterConfigs();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::ApplyOrientationPreset_LeftSide()
{
	// Attacker on victim's left side - victim rotated 90° (left shoulder toward attacker)
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator(0.0f, 90.0f, 0.0f);
	CurrentSpatialRelationship = ESpatialRelationship::LeftSide;
	bSpatialInferenceCacheDirty = true;
	ApplyCharacterConfigs();
	bAnalysisCacheDirty = true;
}

void SPairedAnimationPreview::ApplyOrientationPreset_RightSide()
{
	// Attacker on victim's right side - victim rotated -90° (right shoulder toward attacker)
	AttackerConfig.RotationOffset = FRotator::ZeroRotator;
	VictimConfig.RotationOffset = FRotator(0.0f, -90.0f, 0.0f);
	CurrentSpatialRelationship = ESpatialRelationship::RightSide;
	bSpatialInferenceCacheDirty = true;
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

				// Weapon Configuration (expandable, collapsed by default)
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(4.0f)
				[
					BuildWeaponConfigPanel()
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

			// Attacker Section Selection
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Section", "Section"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SAssignNew(AttackerSectionCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&AttackerSectionOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnAttackerSectionChanged)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item)
					{
						FString DisplayName = Item->IsNone() ? TEXT("(Entire Montage)") : Item->ToString();
						return SNew(STextBlock).Text(FText::FromString(DisplayName));
					})
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (AttackerMontageSection.IsNone())
							{
								return FText::FromString(TEXT("(Entire Montage)"));
							}
							return FText::FromName(AttackerMontageSection);
						})
					]
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

			// Victim Section Selection
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("Section2", "Section"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SAssignNew(VictimSectionCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&VictimSectionOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnVictimSectionChanged)
					.OnGenerateWidget_Lambda([](TSharedPtr<FName> Item)
					{
						FString DisplayName = Item->IsNone() ? TEXT("(Entire Montage)") : Item->ToString();
						return SNew(STextBlock).Text(FText::FromString(DisplayName));
					})
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							if (VictimMontageSection.IsNone())
							{
								return FText::FromString(TEXT("(Entire Montage)"));
							}
							return FText::FromName(VictimMontageSection);
						})
					]
				]
			]

		];
}

TSharedRef<SWidget> SPairedAnimationPreview::BuildWeaponConfigPanel()
{
	// Helper lambda for socket dropdown generation
	auto MakeSocketWidget = [](TSharedPtr<FName> Item) -> TSharedRef<SWidget>
	{
		FString DisplayName = Item->IsNone() ? TEXT("(Use Bounds)") : Item->ToString();
		return SNew(STextBlock).Text(FText::FromString(DisplayName));
	};

	return SNew(SExpandableArea)
		.AreaTitle(LOCTEXT("WeaponConfig", "Weapon Configuration"))
		.InitiallyCollapsed(true)
		.ToolTipText(LOCTEXT("WeaponConfigTip", "Configure weapon meshes, attachment sockets, and contact detection settings for both characters."))
		.BodyContent()
		[
			SNew(SVerticalBox)

			// ========== ATTACKER WEAPON ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("AttackerWeaponHeader", "ATTACKER WEAPON"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(0.2f, 0.6f, 1.0f))
			]

			// Attacker Weapon Mesh
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("WpnMesh", "Mesh"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UStaticMesh::StaticClass())
					.ObjectPath_Lambda([this]() { return GetAttackerWeaponMeshPath(); })
					.OnObjectChanged_Lambda([this](const FAssetData& AssetData)
					{
						OnAttackerWeaponMeshSelected(AssetData);
						RefreshAttackerWeaponSocketOptions();
					})
				]
			]

			// Attacker Character Socket (where weapon attaches on skeleton)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CharSocket", "Hand Socket"))
						.ToolTipText(LOCTEXT("CharSocketTip", "Socket on the character's skeleton where the weapon attaches (e.g., hand_r, weapon_r)"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(AttackerCharacterSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&AttackerCharacterSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnAttackerCharacterSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = AttackerWeaponConfig.GetAttachmentSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Select)") : Socket.ToString());
						})
					]
				]
			]

			// Attacker Weapon Grip Socket (where on weapon mesh to attach)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GripSocket", "Grip Socket"))
						.ToolTipText(LOCTEXT("GripSocketTip", "Socket on the weapon mesh where the character grips it (e.g., Hilt). The Hand Socket will attach to this point. Leave as (Mesh Origin) to attach at weapon mesh origin."))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(AttackerWeaponGripSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&AttackerWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnAttackerWeaponGripSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = AttackerWeaponConfig.GetWeaponGripSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Mesh Origin)") : Socket.ToString());
						})
					]
				]
			]

			// Attacker Weapon Tip Socket
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TipSocket", "Tip Socket"))
						.ToolTipText(LOCTEXT("TipSocketTip", "Socket on the weapon mesh for the blade tip. Used for contact detection. Leave as (Use Bounds) to auto-detect from mesh bounds."))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(AttackerWeaponTipSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&AttackerWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnAttackerWeaponTipSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = AttackerWeaponConfig.GetWeaponTipSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Use Bounds)") : Socket.ToString());
						})
					]
				]
			]

			// Attacker Weapon Mid Socket
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MidSocket", "Mid Socket"))
						.ToolTipText(LOCTEXT("MidSocketTip", "Socket on the weapon mesh for the blade middle. Optional - if only Tip and Base are set, they define weapon length. Set all 3 for full contact detection."))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(AttackerWeaponMidSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&AttackerWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnAttackerWeaponMidSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = AttackerWeaponConfig.GetWeaponMidSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Optional)") : Socket.ToString());
						})
					]
				]
			]

			// Attacker Weapon Base Socket
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BaseSocket", "Base Socket"))
						.ToolTipText(LOCTEXT("BaseSocketTip", "Socket on the weapon mesh for the hilt/base. Used for contact detection. Leave as (Use Bounds) to auto-detect from mesh bounds."))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(AttackerWeaponBaseSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&AttackerWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnAttackerWeaponBaseSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = AttackerWeaponConfig.GetWeaponBaseSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Use Bounds)") : Socket.ToString());
						})
					]
				]
			]

			// Attacker Weapon Offset (Expandable)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("AttackerWpnOffset", "Offset Adjustment"))
				.InitiallyCollapsed(true)
				.Padding(FMargin(10.0f, 2.0f, 2.0f, 2.0f))
				.BodyContent()
				[
					SNew(SVerticalBox)
					// Position offset
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(60.0f)
							[
								SNew(STextBlock).Text(LOCTEXT("PosOffset", "Position"))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-100.0f)
								.MaxValue(100.0f)
								.Value_Lambda([this]() { return AttackerWeaponConfig.GetAttachmentOffset().GetLocation().X; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FVector Offset = AttackerWeaponConfig.GetAttachmentOffset().GetLocation();
									Offset.X = Val;
									OnAttackerWeaponOffsetChanged(Offset);
								})
								.ToolTipText(LOCTEXT("XOffsetTip", "X offset (forward/back)"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-100.0f)
								.MaxValue(100.0f)
								.Value_Lambda([this]() { return AttackerWeaponConfig.GetAttachmentOffset().GetLocation().Y; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FVector Offset = AttackerWeaponConfig.GetAttachmentOffset().GetLocation();
									Offset.Y = Val;
									OnAttackerWeaponOffsetChanged(Offset);
								})
								.ToolTipText(LOCTEXT("YOffsetTip", "Y offset (left/right)"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-100.0f)
								.MaxValue(100.0f)
								.Value_Lambda([this]() { return AttackerWeaponConfig.GetAttachmentOffset().GetLocation().Z; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FVector Offset = AttackerWeaponConfig.GetAttachmentOffset().GetLocation();
									Offset.Z = Val;
									OnAttackerWeaponOffsetChanged(Offset);
								})
								.ToolTipText(LOCTEXT("ZOffsetTip", "Z offset (up/down)"))
							]
						]
					]
					// Rotation offset
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(60.0f)
							[
								SNew(STextBlock).Text(LOCTEXT("RotOffset", "Rotation"))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-180.0f)
								.MaxValue(180.0f)
								.Value_Lambda([this]() { return AttackerWeaponConfig.GetAttachmentOffset().Rotator().Pitch; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FRotator Rot = AttackerWeaponConfig.GetAttachmentOffset().Rotator();
									Rot.Pitch = Val;
									OnAttackerWeaponRotationChanged(Rot);
								})
								.ToolTipText(LOCTEXT("PitchTip", "Pitch (tilt forward/back)"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-180.0f)
								.MaxValue(180.0f)
								.Value_Lambda([this]() { return AttackerWeaponConfig.GetAttachmentOffset().Rotator().Yaw; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FRotator Rot = AttackerWeaponConfig.GetAttachmentOffset().Rotator();
									Rot.Yaw = Val;
									OnAttackerWeaponRotationChanged(Rot);
								})
								.ToolTipText(LOCTEXT("YawTip", "Yaw (rotate left/right)"))
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-180.0f)
								.MaxValue(180.0f)
								.Value_Lambda([this]() { return AttackerWeaponConfig.GetAttachmentOffset().Rotator().Roll; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FRotator Rot = AttackerWeaponConfig.GetAttachmentOffset().Rotator();
									Rot.Roll = Val;
									OnAttackerWeaponRotationChanged(Rot);
								})
								.ToolTipText(LOCTEXT("RollTip", "Roll (twist)"))
							]
						]
					]
				]
			]

			// Separator
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 6.0f)
			[
				SNew(SSeparator)
			]

			// ========== VICTIM WEAPON ==========
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("VictimWeaponHeader", "VICTIM WEAPON"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
				.ColorAndOpacity(FLinearColor(1.0f, 0.4f, 0.2f))
			]

			// Victim Weapon Mesh
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock).Text(LOCTEXT("WpnMesh2", "Mesh"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.AllowedClass(UStaticMesh::StaticClass())
					.ObjectPath_Lambda([this]() { return GetVictimWeaponMeshPath(); })
					.OnObjectChanged_Lambda([this](const FAssetData& AssetData)
					{
						OnVictimWeaponMeshSelected(AssetData);
						RefreshVictimWeaponSocketOptions();
					})
				]
			]

			// Victim Character Socket
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("CharSocket2", "Hand Socket"))
						.ToolTipText(LOCTEXT("CharSocket2Tip", "Socket on the character's skeleton where the weapon attaches"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(VictimCharacterSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&VictimCharacterSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnVictimCharacterSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = VictimWeaponConfig.GetAttachmentSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Select)") : Socket.ToString());
						})
					]
				]
			]

			// Victim Weapon Grip Socket (where on weapon mesh to attach)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GripSocket2", "Grip Socket"))
						.ToolTipText(LOCTEXT("GripSocket2Tip", "Socket on the weapon mesh where the character grips it (e.g., Hilt). The Hand Socket will attach to this point. Leave as (Mesh Origin) to attach at weapon mesh origin."))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(VictimWeaponGripSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&VictimWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnVictimWeaponGripSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = VictimWeaponConfig.GetWeaponGripSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Mesh Origin)") : Socket.ToString());
						})
					]
				]
			]

			// Victim Weapon Tip Socket
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("TipSocket2", "Tip Socket"))
						.ToolTipText(LOCTEXT("TipSocket2Tip", "Socket on the weapon mesh for the blade tip"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(VictimWeaponTipSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&VictimWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnVictimWeaponTipSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = VictimWeaponConfig.GetWeaponTipSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Use Bounds)") : Socket.ToString());
						})
					]
				]
			]

			// Victim Weapon Mid Socket
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MidSocket2", "Mid Socket"))
						.ToolTipText(LOCTEXT("MidSocket2Tip", "Socket on the weapon mesh for the blade middle. Optional - set all 3 for full contact detection."))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(VictimWeaponMidSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&VictimWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnVictimWeaponMidSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = VictimWeaponConfig.GetWeaponMidSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Optional)") : Socket.ToString());
						})
					]
				]
			]

			// Victim Weapon Base Socket
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(80.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("BaseSocket2", "Base Socket"))
						.ToolTipText(LOCTEXT("BaseSocket2Tip", "Socket on the weapon mesh for the hilt/base"))
					]
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SAssignNew(VictimWeaponBaseSocketCombo, SComboBox<TSharedPtr<FName>>)
					.OptionsSource(&VictimWeaponSocketOptions)
					.OnSelectionChanged(this, &SPairedAnimationPreview::OnVictimWeaponBaseSocketChanged)
					.OnGenerateWidget_Lambda(MakeSocketWidget)
					.Content()
					[
						SNew(STextBlock)
						.Text_Lambda([this]()
						{
							FName Socket = VictimWeaponConfig.GetWeaponBaseSocket();
							return FText::FromString(Socket.IsNone() ? TEXT("(Use Bounds)") : Socket.ToString());
						})
					]
				]
			]

			// Victim Weapon Offset (Expandable)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("VictimWpnOffset", "Offset Adjustment"))
				.InitiallyCollapsed(true)
				.Padding(FMargin(10.0f, 2.0f, 2.0f, 2.0f))
				.BodyContent()
				[
					SNew(SVerticalBox)
					// Position offset
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(60.0f)
							[
								SNew(STextBlock).Text(LOCTEXT("PosOffset2", "Position"))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-100.0f)
								.MaxValue(100.0f)
								.Value_Lambda([this]() { return VictimWeaponConfig.GetAttachmentOffset().GetLocation().X; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FVector Offset = VictimWeaponConfig.GetAttachmentOffset().GetLocation();
									Offset.X = Val;
									OnVictimWeaponOffsetChanged(Offset);
								})
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-100.0f)
								.MaxValue(100.0f)
								.Value_Lambda([this]() { return VictimWeaponConfig.GetAttachmentOffset().GetLocation().Y; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FVector Offset = VictimWeaponConfig.GetAttachmentOffset().GetLocation();
									Offset.Y = Val;
									OnVictimWeaponOffsetChanged(Offset);
								})
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-100.0f)
								.MaxValue(100.0f)
								.Value_Lambda([this]() { return VictimWeaponConfig.GetAttachmentOffset().GetLocation().Z; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FVector Offset = VictimWeaponConfig.GetAttachmentOffset().GetLocation();
									Offset.Z = Val;
									OnVictimWeaponOffsetChanged(Offset);
								})
							]
						]
					]
					// Rotation offset
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(2.0f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SBox)
							.WidthOverride(60.0f)
							[
								SNew(STextBlock).Text(LOCTEXT("RotOffset2", "Rotation"))
							]
						]
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-180.0f)
								.MaxValue(180.0f)
								.Value_Lambda([this]() { return VictimWeaponConfig.GetAttachmentOffset().Rotator().Pitch; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FRotator Rot = VictimWeaponConfig.GetAttachmentOffset().Rotator();
									Rot.Pitch = Val;
									OnVictimWeaponRotationChanged(Rot);
								})
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-180.0f)
								.MaxValue(180.0f)
								.Value_Lambda([this]() { return VictimWeaponConfig.GetAttachmentOffset().Rotator().Yaw; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FRotator Rot = VictimWeaponConfig.GetAttachmentOffset().Rotator();
									Rot.Yaw = Val;
									OnVictimWeaponRotationChanged(Rot);
								})
							]
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							.Padding(1.0f)
							[
								SNew(SSpinBox<float>)
								.MinValue(-180.0f)
								.MaxValue(180.0f)
								.Value_Lambda([this]() { return VictimWeaponConfig.GetAttachmentOffset().Rotator().Roll; })
								.OnValueChanged_Lambda([this](float Val)
								{
									FRotator Rot = VictimWeaponConfig.GetAttachmentOffset().Rotator();
									Rot.Roll = Val;
									OnVictimWeaponRotationChanged(Rot);
								})
							]
						]
					]
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
					.ToolTipText(LOCTEXT("LockVictimTip", "When enabled, the victim automatically maintains a fixed distance from the attacker. Disable to position victim independently."))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(4.0f, 0.0f)
				[
					SNew(STextBlock).Text(LOCTEXT("LockVictim", "Lock victim to attacker"))
				]
			]

			// Distance section header
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DistanceHeader", "Character Distance"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			// Distance slider
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
					.ToolTipText(LOCTEXT("DistanceSliderTip", "Distance between attacker and victim in Unreal units"))
				]
			]

			// Find Optimal Distance button (full width, matching rotation button)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindOptDist", "Find Optimal Distance"))
				.ToolTipText(LOCTEXT("FindOptDistTip", "Analyze animation trajectories to find the distance that minimizes penetration while maximizing contact quality across the entire timeline"))
				.OnClicked_Lambda([this]() { OnFindOptimalDistanceClicked(); return FReply::Handled(); })
			]

			// Rotation section header
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RotationHeader", "Character Rotations"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			// Attacker rotation
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("AttackerYaw", "Attacker Yaw"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(-180.0f)
					.MaxValue(180.0f)
					.Value_Lambda([this]() { return AttackerConfig.RotationOffset.Yaw; })
					.OnValueChanged_Lambda([this](float Val) {
						OnAttackerRotationChanged(FRotator(0.0f, Val, 0.0f));
					})
					.ToolTipText(LOCTEXT("AttackerYawTip", "Attacker's facing direction (yaw rotation in degrees, -180 to 180)"))
				]
			]

			// Victim rotation
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock).Text(LOCTEXT("VictimYaw", "Victim Yaw"))
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(4.0f, 0.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(-180.0f)
					.MaxValue(180.0f)
					.Value_Lambda([this]() { return VictimConfig.RotationOffset.Yaw; })
					.OnValueChanged_Lambda([this](float Val) {
						OnVictimRotationChanged(FRotator(0.0f, Val, 0.0f));
					})
					.ToolTipText(LOCTEXT("VictimYawTip", "Victim's facing direction (yaw rotation in degrees, -180 to 180)"))
				]
			]

			// Find Optimal Rotations button (full width)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindOptRot", "Find Optimal Rotations"))
				.ToolTipText(LOCTEXT("FindOptRotTip", "Analyze animation trajectories to find attacker and victim rotations that maximize contact quality while respecting the current spatial relationship constraints"))
				.OnClicked_Lambda([this]() { OnFindOptimalRotationClicked(); return FReply::Handled(); })
			]

			// Orientation Presets
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OrientationPresets", "Orientation Presets"))
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
					.Text(LOCTEXT("PresetFacing", "Facing"))
					.ToolTipText(LOCTEXT("PresetFacingTip", "Characters facing each other: Victim rotated 180 degrees to face attacker. Sets spatial relationship to 'Facing'. Use for front attacks, counters, and direct confrontations."))
					.OnClicked_Lambda([this]() { ApplyOrientationPreset_Facing(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("PresetBehind", "Behind"))
					.ToolTipText(LOCTEXT("PresetBehindTip", "Attacker behind victim: Victim at 0 degrees (back to attacker). Sets spatial relationship to 'Behind'. Use for backstab and assassination animations."))
					.OnClicked_Lambda([this]() { ApplyOrientationPreset_Behind(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("PresetLeft", "Left"))
					.ToolTipText(LOCTEXT("PresetLeftTip", "Attacker on victim's left: Victim rotated 90 degrees (left shoulder toward attacker). Sets spatial relationship to 'Left Side'. Use for side attacks from the left."))
					.OnClicked_Lambda([this]() { ApplyOrientationPreset_LeftSide(); return FReply::Handled(); })
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.Text(LOCTEXT("PresetRight", "Right"))
					.ToolTipText(LOCTEXT("PresetRightTip", "Attacker on victim's right: Victim rotated -90 degrees (right shoulder toward attacker). Sets spatial relationship to 'Right Side'. Use for side attacks from the right."))
					.OnClicked_Lambda([this]() { ApplyOrientationPreset_RightSide(); return FReply::Handled(); })
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
		.AreaTitle(LOCTEXT("Optimization", "Advanced Optimization"))
		.InitiallyCollapsed(true)
		.BodyContent()
		[
			SNew(SVerticalBox)

			// Info text
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("OptInfoText", "Individual optimization controls. Use Auto-Optimize in the quick bar for all-in-one optimization."))
				.AutoWrapText(true)
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			]

			// Individual optimization buttons
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 8.0f, 2.0f, 2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindDistOnly", "Optimize Distance Only"))
				.ToolTipText(LOCTEXT("FindDistOnlyTip", "Find the optimal distance between characters by analyzing bone proximity across the entire animation timeline. Does not modify rotations."))
				.OnClicked_Lambda([this]() { OnFindOptimalDistanceClicked(); return FReply::Handled(); })
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindRotOnly", "Optimize Rotations Only"))
				.ToolTipText(LOCTEXT("FindRotOnlyTip", "Find optimal attacker and victim rotations by analyzing contact quality across the timeline. Respects spatial relationship constraints (Facing, Behind, etc.). Does not modify distance."))
				.OnClicked_Lambda([this]() { OnFindOptimalRotationClicked(); return FReply::Handled(); })
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("FindSyncOnly", "Jump to Best Sync Frame"))
				.ToolTipText(LOCTEXT("FindSyncOnlyTip", "Analyze the timeline to find the frame with the best contact alignment, then jump the playhead to that position. Useful for finding the ideal impact moment."))
				.OnClicked_Lambda([this]() { OnFindOptimalSyncClicked(); return FReply::Handled(); })
			]

			// Cache management
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f, 16.0f, 2.0f, 2.0f)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CacheHeader", "Cache Management"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 9))
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RebuildCache", "Rebuild Analysis Cache"))
				.ToolTipText(LOCTEXT("RebuildCacheTip", "Force rebuild of all analysis caches (frame analysis, trajectories, holistic timeline). Use this if analysis results seem stale after changing animations or settings."))
				.OnClicked_Lambda([this]() {
					// PT-22: Progress-enabled cache rebuild
					const int32 NumAnalysisSamples = (MaxDuration > 0.0f) ? FMath::CeilToInt(MaxDuration * AnalysisSampleRate) + 1 : 30;
					const int32 NumTrajectorySamples = 6 * 2 * (TrajectorySampleCount + 1); // 6 bones * 2 characters
					const int32 TotalWork = NumAnalysisSamples + NumTrajectorySamples + 4; // +4 for sub-phases

					FScopedSlowTask SlowTask(TotalWork, LOCTEXT("RebuildingAllCaches", "Rebuilding All Analysis Caches..."));
					SlowTask.MakeDialog(true);

					// Phase 1: Frame analysis cache
					FrameAnalysisCache.Empty();
					if (MaxDuration > 0.0f)
					{
						float TimeStep = 1.0f / AnalysisSampleRate;
						for (float t = 0.0f; t <= MaxDuration; t += TimeStep)
						{
							if (SlowTask.ShouldCancel()) return FReply::Handled();
							SlowTask.EnterProgressFrame(1.0f, LOCTEXT("AnalyzingFrames", "Analyzing frames..."));
							FrameAnalysisCache.Add(AnalyzeFrame(t));
						}
					}

					SlowTask.EnterProgressFrame(1.0f, LOCTEXT("BuildingDistAnalysis", "Building distance analysis..."));
					if (SlowTask.ShouldCancel()) return FReply::Handled();
					RebuildDistanceAnalysis();

					SlowTask.EnterProgressFrame(1.0f, LOCTEXT("BuildingTimeAnalysis", "Building timing analysis..."));
					if (SlowTask.ShouldCancel()) return FReply::Handled();
					RebuildTimingAnalysis();
					bAnalysisCacheDirty = false;

					SlowTask.EnterProgressFrame(1.0f, LOCTEXT("BuildingHolisticAnalysis", "Building holistic analysis..."));
					if (SlowTask.ShouldCancel()) return FReply::Handled();
					RebuildHolisticAnalysis();

					// Phase 2: Trajectory cache
					SlowTask.EnterProgressFrame(1.0f, LOCTEXT("StartingTrajectories", "Starting trajectory analysis..."));
					if (SlowTask.ShouldCancel()) return FReply::Handled();

					AttackerTrajectories.Empty();
					VictimTrajectories.Empty();
					if (MaxDuration > 0.0f)
					{
						TArray<FName> TrackedBones = {
							TEXT("hand_r"), TEXT("hand_l"),
							TEXT("foot_r"), TEXT("foot_l"),
							TEXT("head"), TEXT("pelvis")
						};
						float TimeStep = MaxDuration / TrajectorySampleCount;

						for (const FName& BoneName : TrackedBones)
						{
							if (AttackerMeshComponent)
							{
								FBoneTrajectory AttackerTraj;
								AttackerTraj.BoneName = BoneName;
								AttackerTraj.TrajectoryColor = AttackerColor;
								for (int32 i = 0; i <= TrajectorySampleCount; ++i)
								{
									if (SlowTask.ShouldCancel()) return FReply::Handled();
									SlowTask.EnterProgressFrame(1.0f);
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
							if (VictimMeshComponent)
							{
								FBoneTrajectory VictimTraj;
								VictimTraj.BoneName = BoneName;
								VictimTraj.TrajectoryColor = VictimColor;
								for (int32 i = 0; i <= TrajectorySampleCount; ++i)
								{
									if (SlowTask.ShouldCancel()) return FReply::Handled();
									SlowTask.EnterProgressFrame(1.0f);
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

					UpdateAnalyticsDisplay();
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

			// Axis Grid - first for reference orientation
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return IsVisualizationActive(EVisualizationLayer::AxisGrid) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetVisualizationActive(EVisualizationLayer::AxisGrid, State == ECheckBoxState::Checked); })
				.ToolTipText(LOCTEXT("ShowAxisGridTip", "Display world axis grid (X=Red/Forward, Y=Green/Right, Z=Blue/Up) and character forward direction arrows. Note: Skeletal meshes at runtime typically have -90° yaw offset from capsule."))
				[
					SNew(STextBlock).Text(LOCTEXT("ShowAxisGrid", "Axis Grid"))
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return IsVisualizationActive(EVisualizationLayer::ContactPoints) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { SetVisualizationActive(EVisualizationLayer::ContactPoints, State == ECheckBoxState::Checked); })
				.ToolTipText(LOCTEXT("ShowContactsTip", "Display spheres at detected contact points between characters. Yellow = current contacts, Green = predicted future contacts."))
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
				.ToolTipText(LOCTEXT("ShowWeaponTip", "Display the weapon trace line between WeaponStart and WeaponEnd sockets. Red line shows the weapon's hit detection area."))
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
				.ToolTipText(LOCTEXT("ShowDistanceTip", "Display lines showing distances between key bones on attacker and victim. Helps visualize spacing and alignment."))
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
				.ToolTipText(LOCTEXT("ShowVelocityTip", "Display velocity arrows on key bones showing direction and speed of movement. Longer arrows indicate faster movement."))
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
				.ToolTipText(LOCTEXT("ShowCOMTip", "Display the calculated center of mass for each character. Useful for understanding balance and weight distribution during animations."))
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
					.ToolTipText(LOCTEXT("StartSocketTip", "Socket name for the weapon's base/handle (e.g., 'WeaponStart'). Used for weapon trace visualization and contact detection."))
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
					.ToolTipText(LOCTEXT("EndSocketTip", "Socket name for the weapon's tip/end (e.g., 'WeaponEnd'). Used for weapon trace visualization and contact detection."))
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
					.ToolTipText(LOCTEXT("ContactThresholdTip", "Maximum distance (in Unreal units) between bones to be considered a contact. Lower values = stricter contact detection."))
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
					.ToolTipText(LOCTEXT("SampleRateTip", "Frames per second to sample during analysis. Higher = more accurate but slower. 60 is typically sufficient."))
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
