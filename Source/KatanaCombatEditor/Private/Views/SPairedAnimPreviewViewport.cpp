// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/SPairedAnimPreviewViewport.h"
#include "AdvancedPreviewScene.h"

// ============================================================================
// VIEWPORT CLIENT
// ============================================================================

FPairedPreviewViewportClient::FPairedPreviewViewportClient(FAdvancedPreviewScene* InPreviewScene)
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

void FPairedPreviewViewportClient::Tick(float DeltaSeconds)
{
	FEditorViewportClient::Tick(DeltaSeconds);
	if (PreviewScenePtr)
	{
		PreviewScenePtr->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
	}
}

void FPairedPreviewViewportClient::FocusOnPoint(const FVector& Point, float Distance)
{
	SetViewLocation(Point + FVector(-Distance, 0.0f, Distance * PairedAnimPreviewConfig::Camera::FocusHeightRatio));
	SetLookAtLocation(Point);
}

// ============================================================================
// VIEWPORT WIDGET
// ============================================================================

void SPairedPreviewViewport::Construct(const FArguments& InArgs)
{
	PreviewScene = InArgs._PreviewScene;
	SEditorViewport::Construct(SEditorViewport::FArguments());
}

TSharedRef<FEditorViewportClient> SPairedPreviewViewport::MakeEditorViewportClient()
{
	ViewportClient = MakeShareable(new FPairedPreviewViewportClient(PreviewScene));
	return ViewportClient.ToSharedRef();
}

void SPairedPreviewViewport::FocusOnPoint(const FVector& Point, float Distance)
{
	if (ViewportClient.IsValid())
	{
		ViewportClient->FocusOnPoint(Point, Distance);
	}
}
