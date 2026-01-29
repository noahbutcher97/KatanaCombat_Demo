// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class UHitReactionData;
class IDetailLayoutBuilder;
class SWidget;

/**
 * Custom details panel for UHitReactionData assets
 * Provides section dropdown and info display for designers
 *
 * Features:
 * - Section selector dropdown (auto-populated from montage)
 * - Section info display (time range, duration)
 * - Refresh button for section list
 * - Validation of montage section
 * - Open montage editor button
 */
class FHitReactionDataCustomization : public IDetailCustomization
{
public:
    /** Makes a new instance of this detail layout class for a specific detail view */
    static TSharedRef<IDetailCustomization> MakeInstance();

    /** IDetailCustomization interface */
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    /** Currently selected HitReactionData asset */
    TWeakObjectPtr<UHitReactionData> CachedReactionData;

    /** Available montage sections for dropdown */
    TArray<TSharedPtr<FName>> SectionOptions;

    // ============================================================================
    // UI BUILDING
    // ============================================================================

    /** Add validation warnings at top if needed */
    void AddValidationWarnings(IDetailLayoutBuilder& DetailBuilder);

    /** Create montage section selector widget */
    TSharedRef<SWidget> CreateSectionSelector(IDetailLayoutBuilder& DetailBuilder);

    /** Create section info widget (shows time range) */
    TSharedRef<SWidget> CreateSectionInfoWidget();

    /** Create action buttons row */
    TSharedRef<SWidget> CreateActionButtons();

    // ============================================================================
    // BUTTON HANDLERS
    // ============================================================================

    /** Called when "Validate" button is pressed */
    FReply OnValidateClicked();

    /** Called when "Open Montage" button is pressed */
    FReply OnOpenMontageClicked();

    // ============================================================================
    // HELPERS
    // ============================================================================

    /** Refresh section options from montage */
    void RefreshSectionOptions();

    /** Refresh the details panel after changes */
    void RefreshDetails();
};
