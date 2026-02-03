// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SEditorViewport.h"
#include "EditorViewportClient.h"
#include "AdvancedPreviewScene.h"
#include "PairedAnimationPreviewConfig.h"

/**
 * PT-11 Phase 2: Viewport client for paired animation preview.
 *
 * Handles camera control and world ticking for the preview scene.
 * Extracted from PairedAnimationPreview.cpp for modularity.
 */
class KATANACOMBATEDITOR_API FPairedPreviewViewportClient : public FEditorViewportClient
{
public:
	FPairedPreviewViewportClient(FAdvancedPreviewScene* InPreviewScene);

	// FEditorViewportClient interface
	virtual void Tick(float DeltaSeconds) override;

	/**
	 * Focus the camera on a specific point.
	 * @param Point World location to focus on
	 * @param Distance Distance from the point (default from config)
	 */
	void FocusOnPoint(const FVector& Point, float Distance = PairedAnimPreviewConfig::Camera::FocusDistance);

	/** Get the preview scene this client is viewing */
	FAdvancedPreviewScene* GetPreviewScene() const { return PreviewScenePtr; }

private:
	FAdvancedPreviewScene* PreviewScenePtr = nullptr;
};

/**
 * PT-11 Phase 2: Slate viewport widget for paired animation preview.
 *
 * Hosts the FPairedPreviewViewportClient and provides the viewport rendering surface.
 * Can be used standalone or embedded in larger widgets.
 */
class KATANACOMBATEDITOR_API SPairedPreviewViewport : public SEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SPairedPreviewViewport) {}
		/** The preview scene to display */
		SLATE_ARGUMENT(FAdvancedPreviewScene*, PreviewScene)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// SEditorViewport interface
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;

	/** Get the viewport client for camera control */
	TSharedPtr<FPairedPreviewViewportClient> GetViewportClient() const { return ViewportClient; }

	/** Focus the viewport camera on a specific point */
	void FocusOnPoint(const FVector& Point, float Distance = PairedAnimPreviewConfig::Camera::FocusDistance);

private:
	FAdvancedPreviewScene* PreviewScene = nullptr;
	TSharedPtr<FPairedPreviewViewportClient> ViewportClient;
};
