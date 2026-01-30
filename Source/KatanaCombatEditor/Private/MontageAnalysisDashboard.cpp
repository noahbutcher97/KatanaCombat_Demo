// Copyright Epic Games, Inc. All Rights Reserved.

#include "MontageAnalysisDashboard.h"
#include "MontageAnalyzerTools.h"
#include "Data/PairedAnimationData.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Engine/SkeletalMesh.h"

#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Docking/SDockTab.h"

#include "PropertyCustomizationHelpers.h"
#include "Framework/Docking/TabManager.h"
#include "LevelEditor.h"
#include "AdvancedPreviewScene.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "MontageAnalysisDashboard"

// ============================================================================
// CUSTOM VIEWPORT FOR PREVIEW
// ============================================================================

class SMontagePreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SMontagePreviewViewport) {}
		SLATE_ARGUMENT(TSharedPtr<FAdvancedPreviewScene>, PreviewScene)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		PreviewScene = InArgs._PreviewScene;
		SEditorViewport::Construct(SEditorViewport::FArguments());
	}

	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
	{
		ViewportClient = MakeShareable(new FEditorViewportClient(nullptr, PreviewScene.Get(), SharedThis(this)));
		ViewportClient->SetRealtime(true);
		ViewportClient->bSetListenerPosition = false;
		ViewportClient->SetViewLocation(FVector(-200, 0, 100));
		ViewportClient->SetViewRotation(FRotator(-15, 0, 0));
		return ViewportClient.ToSharedRef();
	}

	TSharedPtr<FAdvancedPreviewScene> PreviewScene;
	TSharedPtr<FEditorViewportClient> ViewportClient;
};

// ============================================================================
// TAB REGISTRATION
// ============================================================================

void SMontageAnalysisDashboard::RegisterTabSpawner()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
		GetTabName(),
		FOnSpawnTab::CreateStatic(&SMontageAnalysisDashboard::SpawnTab))
		.SetDisplayName(LOCTEXT("DashboardTitle", "Montage Analysis Dashboard"))
		.SetTooltipText(LOCTEXT("DashboardTooltip", "Visual montage analysis with timeline scrubbing and per-frame analytics"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);

	// Add to Window menu
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>("LevelEditor");
	TSharedPtr<FExtender> MenuExtender = MakeShareable(new FExtender);
	MenuExtender->AddMenuExtension(
		"LevelEditor",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("DashboardMenuEntry", "Montage Analysis Dashboard"),
				LOCTEXT("DashboardMenuTooltip", "Open the visual Montage Analysis Dashboard"),
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

void SMontageAnalysisDashboard::UnregisterTabSpawner()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(GetTabName());
}

TSharedRef<SDockTab> SMontageAnalysisDashboard::SpawnTab(const FSpawnTabArgs& Args)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		.Label(LOCTEXT("DashboardTabLabel", "Montage Analysis"))
		[
			SNew(SMontageAnalysisDashboard)
		];
}

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

void SMontageAnalysisDashboard::Construct(const FArguments& InArgs)
{
	SetupPreviewScene();

	ChildSlot
	[
		SNew(SVerticalBox)

		// Toolbar
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			BuildToolbar()
		]

		// Main content - splitter between viewport and analytics
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SSplitter)
			.Orientation(Orient_Horizontal)

			// Left: Viewport
			+ SSplitter::Slot()
			.Value(0.5f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				[
					BuildViewport()
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					BuildTimeline()
				]
			]

			// Right: Analytics panels
			+ SSplitter::Slot()
			.Value(0.5f)
			[
				BuildAnalyticsPanel()
			]
		]
	];
}

SMontageAnalysisDashboard::~SMontageAnalysisDashboard()
{
	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->DestroyComponent();
		PreviewMeshComponent = nullptr;
	}
	PreviewScene.Reset();
}

// ============================================================================
// PREVIEW SCENE
// ============================================================================

void SMontageAnalysisDashboard::SetupPreviewScene()
{
	PreviewScene = MakeShareable(new FAdvancedPreviewScene(FPreviewScene::ConstructionValues()));
	PreviewScene->SetFloorVisibility(true);

	// Create preview mesh component
	PreviewMeshComponent = NewObject<UDebugSkelMeshComponent>();
	PreviewScene->AddComponent(PreviewMeshComponent, FTransform::Identity);
}

void SMontageAnalysisDashboard::UpdatePreviewMesh(USkeletalMesh* Mesh)
{
	if (PreviewMeshComponent && Mesh)
	{
		PreviewMeshComponent->SetSkeletalMesh(Mesh);
		PreviewMeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	}
}

void SMontageAnalysisDashboard::UpdatePreviewAnimation(float Time)
{
	if (PreviewMeshComponent && CurrentMontage.IsValid())
	{
		PreviewMeshComponent->SetAnimation(CurrentMontage.Get());
		PreviewMeshComponent->SetPosition(Time);
	}
}

// ============================================================================
// UI BUILDERS
// ============================================================================

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildToolbar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)

			// Montage picker
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0)
			[
				SNew(STextBlock).Text(LOCTEXT("MontageLabel", "Montage:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.3f)
			.Padding(4, 0)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UAnimMontage::StaticClass())
				.ObjectPath(this, &SMontageAnalysisDashboard::GetMontagePath)
				.OnObjectChanged(this, &SMontageAnalysisDashboard::OnMontageSelected)
			]

			// Skeleton picker
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0)
			[
				SNew(STextBlock).Text(LOCTEXT("SkeletonLabel", "Skeleton:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.3f)
			.Padding(4, 0)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(USkeletalMesh::StaticClass())
				.ObjectPath(this, &SMontageAnalysisDashboard::GetSkeletonPath)
				.OnObjectChanged(this, &SMontageAnalysisDashboard::OnSkeletonSelected)
			]

			// Paired Data picker (optional)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4, 0)
			[
				SNew(STextBlock).Text(LOCTEXT("PairedDataLabel", "Paired Data:"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.3f)
			.Padding(4, 0)
			[
				SNew(SObjectPropertyEntryBox)
				.AllowedClass(UPairedAnimationData::StaticClass())
				.ObjectPath(this, &SMontageAnalysisDashboard::GetPairedDataPath)
				.OnObjectChanged(this, &SMontageAnalysisDashboard::OnPairedDataSelected)
			]
		];
}

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildViewport()
{
	ViewportWidget = SNew(SMontagePreviewViewport)
		.PreviewScene(PreviewScene);

	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.DarkGroupBorder"))
		[
			ViewportWidget.ToSharedRef()
		];
}

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildTimeline()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SVerticalBox)

			// Playback controls
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 2)
			[
				SNew(SHorizontalBox)

				// Step backward
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepBackBtn", "|<"))
					.OnClicked_Lambda([this]() { OnStepBackward(); return FReply::Handled(); })
				]

				// Play/Pause
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2, 0)
				[
					SNew(SButton)
					.Text_Lambda([this]() { return bIsPlaying ? LOCTEXT("PauseBtn", "||") : LOCTEXT("PlayBtn", ">"); })
					.OnClicked_Lambda([this]() { OnPlayPauseClicked(); return FReply::Handled(); })
				]

				// Step forward
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2, 0)
				[
					SNew(SButton)
					.Text(LOCTEXT("StepForwardBtn", ">|"))
					.OnClicked_Lambda([this]() { OnStepForward(); return FReply::Handled(); })
				]

				// Time display
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10, 0)
				[
					SAssignNew(TimeDisplayText, STextBlock)
					.Text(this, &SMontageAnalysisDashboard::GetCurrentTimeText)
				]

				// Frame display
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10, 0)
				[
					SAssignNew(FrameDisplayText, STextBlock)
					.Text(this, &SMontageAnalysisDashboard::GetCurrentFrameText)
				]

				// Speed control
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(10, 0)
				[
					SNew(STextBlock).Text(LOCTEXT("SpeedLabel", "Speed:"))
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2, 0)
				[
					SNew(SBox)
					.WidthOverride(80)
					[
						SNew(SSlider)
						.MinValue(0.1f)
						.MaxValue(2.0f)
						.Value(1.0f)
						.OnValueChanged_Lambda([this](float Value) { PlaybackSpeed = Value; })
					]
				]
			]

			// Timeline slider
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SAssignNew(TimelineSlider, SSlider)
				.MinValue(0.0f)
				.MaxValue(1.0f)
				.Value(0.0f)
				.OnValueChanged(this, &SMontageAnalysisDashboard::OnTimelineValueChanged)
			]
		];
}

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildAnalyticsPanel()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SScrollBox)

			// Current Frame Info
			+ SScrollBox::Slot()
			.Padding(8)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("FrameInfoHeader", "FRAME INFORMATION"))
					.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4)
				[
					SAssignNew(PhaseDisplayText, STextBlock)
					.Text(LOCTEXT("PhaseDefault", "Phase: --"))
				]
			]

			+ SScrollBox::Slot()
			.Padding(8, 0)
			[
				SNew(SSeparator)
			]

			// Root Motion Panel
			+ SScrollBox::Slot()
			.Padding(8)
			[
				BuildRootMotionPanel()
			]

			+ SScrollBox::Slot()
			.Padding(8, 0)
			[
				SNew(SSeparator)
			]

			// Active Notifies Panel
			+ SScrollBox::Slot()
			.Padding(8)
			[
				BuildNotifyPanel()
			]

			+ SScrollBox::Slot()
			.Padding(8, 0)
			[
				SNew(SSeparator)
			]

			// Bone Data Panel
			+ SScrollBox::Slot()
			.Padding(8)
			[
				BuildBoneDataPanel()
			]

			+ SScrollBox::Slot()
			.Padding(8, 0)
			[
				SNew(SSeparator)
			]

			// Warping Panel
			+ SScrollBox::Slot()
			.Padding(8)
			[
				BuildWarpingPanel()
			]
		];
}

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildRootMotionPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("RootMotionHeader", "ROOT MOTION"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10, 0, 0, 0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bShowRootMotionTrail ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowRootMotionTrail = (State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowTrailLabel", "Show Trail"))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		[
			SAssignNew(RootMotionText, STextBlock)
			.Text(LOCTEXT("RootMotionDefault", "Translation: --\nRotation: --\nVelocity: --\nSpeed: --"))
		];
}

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildNotifyPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NotifiesHeader", "ACTIVE NOTIFIES"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10, 0, 0, 0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bShowNotifyMarkers ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowNotifyMarkers = (State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowMarkersLabel", "Show Markers"))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		[
			SAssignNew(ActiveNotifiesText, STextBlock)
			.Text(LOCTEXT("NotifiesDefault", "(none active)"))
		];
}

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildBoneDataPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("BoneDataHeader", "BONE DATA"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10, 0, 0, 0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bShowBoneTrajectories ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowBoneTrajectories = (State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowTrajectoriesLabel", "Show Trajectories"))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		[
			SAssignNew(BoneDataText, STextBlock)
			.Text(LOCTEXT("BoneDataDefault", "hand_r: --\nhand_l: --\nfoot_r: --\nfoot_l: --"))
		];
}

TSharedRef<SWidget> SMontageAnalysisDashboard::BuildWarpingPanel()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("WarpingHeader", "MOTION WARPING"))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(10, 0, 0, 0)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([this]() { return bShowWarpTargets ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([this](ECheckBoxState State) { bShowWarpTargets = (State == ECheckBoxState::Checked); })
				[
					SNew(STextBlock).Text(LOCTEXT("ShowWarpLabel", "Show Warp Targets"))
				]
			]
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 4)
		[
			SAssignNew(WarpDataText, STextBlock)
			.Text(LOCTEXT("WarpDataDefault", "No warp data"))
		];
}

// ============================================================================
// TIMELINE CONTROLS
// ============================================================================

void SMontageAnalysisDashboard::OnTimelineValueChanged(float NewValue)
{
	if (MontageDuration > 0.0f)
	{
		CurrentTime = NewValue * MontageDuration;
		UpdatePreviewAnimation(CurrentTime);
		UpdateAnalyticsDisplay();
	}
}

void SMontageAnalysisDashboard::OnPlayPauseClicked()
{
	bIsPlaying = !bIsPlaying;
}

void SMontageAnalysisDashboard::OnStepForward()
{
	if (MontageDuration > 0.0f)
	{
		float FrameTime = 1.0f / 30.0f; // 30 fps step
		CurrentTime = FMath::Min(CurrentTime + FrameTime, MontageDuration);
		TimelineSlider->SetValue(CurrentTime / MontageDuration);
		UpdatePreviewAnimation(CurrentTime);
		UpdateAnalyticsDisplay();
	}
}

void SMontageAnalysisDashboard::OnStepBackward()
{
	if (MontageDuration > 0.0f)
	{
		float FrameTime = 1.0f / 30.0f;
		CurrentTime = FMath::Max(CurrentTime - FrameTime, 0.0f);
		TimelineSlider->SetValue(CurrentTime / MontageDuration);
		UpdatePreviewAnimation(CurrentTime);
		UpdateAnalyticsDisplay();
	}
}

void SMontageAnalysisDashboard::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (bIsPlaying && MontageDuration > 0.0f)
	{
		CurrentTime += InDeltaTime * PlaybackSpeed;
		if (CurrentTime >= MontageDuration)
		{
			CurrentTime = 0.0f; // Loop
		}

		TimelineSlider->SetValue(CurrentTime / MontageDuration);
		UpdatePreviewAnimation(CurrentTime);
		UpdateAnalyticsDisplay();
	}
}

// ============================================================================
// ASSET SELECTION
// ============================================================================

FString SMontageAnalysisDashboard::GetMontagePath() const
{
	return CurrentMontage.IsValid() ? CurrentMontage->GetPathName() : FString();
}

FString SMontageAnalysisDashboard::GetSkeletonPath() const
{
	return CurrentSkeleton.IsValid() ? CurrentSkeleton->GetPathName() : FString();
}

FString SMontageAnalysisDashboard::GetPairedDataPath() const
{
	return CurrentPairedData.IsValid() ? CurrentPairedData->GetPathName() : FString();
}

void SMontageAnalysisDashboard::OnMontageSelected(const FAssetData& AssetData)
{
	CurrentMontage = Cast<UAnimMontage>(AssetData.GetAsset());
	OnMontageChanged();
}

void SMontageAnalysisDashboard::OnSkeletonSelected(const FAssetData& AssetData)
{
	CurrentSkeleton = Cast<USkeletalMesh>(AssetData.GetAsset());
	if (CurrentSkeleton.IsValid())
	{
		UpdatePreviewMesh(CurrentSkeleton.Get());
	}
}

void SMontageAnalysisDashboard::OnPairedDataSelected(const FAssetData& AssetData)
{
	CurrentPairedData = Cast<UPairedAnimationData>(AssetData.GetAsset());

	// Auto-fill montage and skeleton from paired data
	if (CurrentPairedData.IsValid())
	{
		if (CurrentPairedData->AttackerMontage && !CurrentMontage.IsValid())
		{
			CurrentMontage = CurrentPairedData->AttackerMontage;
			OnMontageChanged();
		}
	}
}

// ============================================================================
// MONTAGE ANALYSIS
// ============================================================================

void SMontageAnalysisDashboard::OnMontageChanged()
{
	if (!CurrentMontage.IsValid())
	{
		MontageDuration = 0.0f;
		CurrentTime = 0.0f;
		return;
	}

	MontageDuration = CurrentMontage->GetPlayLength();
	CurrentTime = 0.0f;

	// Try to get skeleton from montage if not set
	if (!CurrentSkeleton.IsValid())
	{
		if (USkeleton* Skeleton = CurrentMontage->GetSkeleton())
		{
			// Find a skeletal mesh that uses this skeleton
			// For now, just notify user they need to select one
		}
	}

	// Rebuild analysis cache
	RebuildFrameCache();

	// Update preview
	if (CurrentSkeleton.IsValid())
	{
		UpdatePreviewMesh(CurrentSkeleton.Get());
	}
	UpdatePreviewAnimation(0.0f);
	UpdateAnalyticsDisplay();
}

void SMontageAnalysisDashboard::RebuildFrameCache()
{
	FrameDataCache.Empty();

	if (!CurrentMontage.IsValid() || MontageDuration <= 0.0f)
	{
		return;
	}

	int32 NumSamples = FMath::CeilToInt(MontageDuration * FrameCacheResolution);
	FrameDataCache.Reserve(NumSamples);

	for (int32 i = 0; i <= NumSamples; ++i)
	{
		float Time = (float)i / (float)FrameCacheResolution;
		FrameDataCache.Add(AnalyzeFrame(Time));
	}
}

FFrameAnalysisData SMontageAnalysisDashboard::AnalyzeFrame(float Time)
{
	FFrameAnalysisData Data;
	Data.Time = Time;

	if (!CurrentMontage.IsValid())
	{
		return Data;
	}

	UAnimMontage* Montage = CurrentMontage.Get();

	// Root Motion
	if (UMontageAnalyzerTools::HasRootMotion(Montage))
	{
		FTransform RootTransform = UMontageAnalyzerTools::GetRootMotionAtTime(Montage, Time);
		Data.RootMotionTranslation = RootTransform.GetLocation();
		Data.RootMotionRotation = RootTransform.GetRotation().Rotator();

		FVector Direction = UMontageAnalyzerTools::GetRootMotionDirectionAtTime(Montage, Time);
		Data.RootMotionVelocity = Direction;
		Data.RootMotionSpeed = Direction.Size();
	}

	// Active Notifies
	for (const FAnimNotifyEvent& Notify : Montage->Notifies)
	{
		float NotifyStart = Notify.GetTriggerTime();
		float NotifyEnd = NotifyStart + Notify.GetDuration();

		if (Time >= NotifyStart && Time <= NotifyEnd)
		{
			if (Notify.NotifyStateClass)
			{
				Data.ActiveNotifyStates.Add(Notify.NotifyStateClass->GetClass()->GetName());
			}
		}

		// Instant notifies (check if we just passed them)
		if (Notify.Notify && FMath::Abs(Time - NotifyStart) < 0.02f)
		{
			Data.ActiveNotifies.Add(Notify.Notify->GetClass()->GetName());
		}
	}

	// Determine phase from notify states
	for (const FString& NotifyName : Data.ActiveNotifyStates)
	{
		if (NotifyName.Contains(TEXT("Windup")))
		{
			Data.CurrentPhase = TEXT("Windup");
		}
		else if (NotifyName.Contains(TEXT("Active")))
		{
			Data.CurrentPhase = TEXT("Active");
		}
		else if (NotifyName.Contains(TEXT("Recovery")))
		{
			Data.CurrentPhase = TEXT("Recovery");
		}
		else if (NotifyName.Contains(TEXT("Parry")))
		{
			Data.CurrentPhase = TEXT("Parry Window");
		}
		else if (NotifyName.Contains(TEXT("Combo")))
		{
			Data.CurrentPhase = TEXT("Combo Window");
		}
	}

	if (Data.CurrentPhase.IsEmpty())
	{
		Data.CurrentPhase = TEXT("Idle");
	}

	return Data;
}

// ============================================================================
// ANALYTICS DISPLAY
// ============================================================================

void SMontageAnalysisDashboard::UpdateAnalyticsDisplay()
{
	if (!CurrentMontage.IsValid())
	{
		return;
	}

	// Find closest cached frame
	int32 FrameIndex = FMath::Clamp(
		FMath::RoundToInt(CurrentTime * FrameCacheResolution),
		0,
		FrameDataCache.Num() - 1
	);

	if (FrameDataCache.IsValidIndex(FrameIndex))
	{
		const FFrameAnalysisData& Data = FrameDataCache[FrameIndex];

		// Phase
		if (PhaseDisplayText.IsValid())
		{
			PhaseDisplayText->SetText(FText::Format(
				LOCTEXT("PhaseFormat", "Phase: {0}"),
				FText::FromString(Data.CurrentPhase)));
		}

		// Root Motion
		if (RootMotionText.IsValid())
		{
			RootMotionText->SetText(FText::Format(
				LOCTEXT("RootMotionFormat", "Translation: ({0}, {1}, {2})\nRotation: (Y:{3}, P:{4}, R:{5})\nSpeed: {6} u/s"),
				FText::AsNumber(Data.RootMotionTranslation.X, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Data.RootMotionTranslation.Y, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Data.RootMotionTranslation.Z, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Data.RootMotionRotation.Yaw, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Data.RootMotionRotation.Pitch, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Data.RootMotionRotation.Roll, &FNumberFormattingOptions::DefaultNoGrouping()),
				FText::AsNumber(Data.RootMotionSpeed, &FNumberFormattingOptions::DefaultNoGrouping())
			));
		}

		// Active Notifies
		if (ActiveNotifiesText.IsValid())
		{
			FString NotifyText;
			for (const FString& Notify : Data.ActiveNotifyStates)
			{
				NotifyText += Notify + TEXT("\n");
			}
			for (const FString& Notify : Data.ActiveNotifies)
			{
				NotifyText += TEXT("[!] ") + Notify + TEXT("\n");
			}
			if (NotifyText.IsEmpty())
			{
				NotifyText = TEXT("(none active)");
			}
			ActiveNotifiesText->SetText(FText::FromString(NotifyText));
		}
	}
}

FText SMontageAnalysisDashboard::GetCurrentTimeText() const
{
	return FText::Format(
		LOCTEXT("TimeFormat", "Time: {0} / {1} s"),
		FText::AsNumber(CurrentTime, &FNumberFormattingOptions::DefaultNoGrouping()),
		FText::AsNumber(MontageDuration, &FNumberFormattingOptions::DefaultNoGrouping())
	);
}

FText SMontageAnalysisDashboard::GetCurrentFrameText() const
{
	int32 CurrentFrame = FMath::RoundToInt(CurrentTime * 30.0f);
	int32 TotalFrames = FMath::RoundToInt(MontageDuration * 30.0f);
	return FText::Format(
		LOCTEXT("FrameFormat", "Frame: {0} / {1}"),
		FText::AsNumber(CurrentFrame),
		FText::AsNumber(TotalFrames)
	);
}

FText SMontageAnalysisDashboard::GetRootMotionText() const
{
	return LOCTEXT("RootMotionDefault", "--");
}

FText SMontageAnalysisDashboard::GetActiveNotifiesText() const
{
	return LOCTEXT("NotifiesDefault", "(none)");
}

FText SMontageAnalysisDashboard::GetBoneDataText() const
{
	return LOCTEXT("BoneDataDefault", "--");
}

void SMontageAnalysisDashboard::DrawVisualizationOverlays()
{
	// TODO: Draw debug shapes in viewport for:
	// - Root motion trail
	// - Bone trajectories
	// - Warp targets
}

#undef LOCTEXT_NAMESPACE
