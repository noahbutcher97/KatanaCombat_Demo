// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Input/Reply.h"

class UAttackData;
class IDetailLayoutBuilder;
class SWidget;

/**
 * Custom details panel for UAttackData assets
 * Provides visual timeline and helper buttons for designers
 * 
 * Features:
 * - Visual timeline showing attack phases
 * - Section selector dropdown (auto-populated from montage)
 * - Section info display (time range, duration)
 * - Auto-calculate timing button
 * - Generate AnimNotifies button
 * - Validate montage section button
 * - Timing preview text
 * - Section conflict warnings
 * - Open montage editor button
 * 
 * This customization makes it easier for designers to configure attacks
 * without needing to manually calculate timing values or add notifies.
 */
class FAttackDataCustomization : public IDetailCustomization
{
public:
    /** Makes a new instance of this detail layout class for a specific detail view */
    static TSharedRef<IDetailCustomization> MakeInstance();

    /** IDetailCustomization interface */
    virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
    /** Currently selected AttackData asset */
    TWeakObjectPtr<UAttackData> CachedAttackData;

    /** Available montage sections for dropdown */
    TArray<TSharedPtr<FName>> SectionOptions;

    // ============================================================================
    // UI BUILDING
    // ============================================================================

    /** Add validation warnings at top if needed */
    void AddValidationWarnings(IDetailLayoutBuilder& DetailBuilder);

    /** Create montage section selector widget */
    TSharedRef<SWidget> CreateSectionSelector(IDetailLayoutBuilder& DetailBuilder);

    /** Create charge loop section selector widget (for heavy attacks) */
    TSharedRef<SWidget> CreateChargeLoopSelector(IDetailLayoutBuilder& DetailBuilder);

    /** Create charge release section selector widget (for heavy attacks) */
    TSharedRef<SWidget> CreateChargeReleaseSelector(IDetailLayoutBuilder& DetailBuilder);

    /** Create section info widget (shows time range) */
    TSharedRef<SWidget> CreateSectionInfoWidget();

    /** Create timing editor widget with phase sliders */
    TSharedRef<SWidget> CreateTimingEditor();

    /** Create action buttons row */
    TSharedRef<SWidget> CreateActionButtons();

    /** Create phase timing row (for timing editor) */
    TSharedRef<SWidget> CreatePhaseTimingRow(
        const FText& PhaseName,
        const FLinearColor& Color,
        TAttribute<float> StartValue,
        TAttribute<float> EndValue,
        FOnFloatValueChanged OnStartChanged,
        FOnFloatValueChanged OnEndChanged
    );

    // ============================================================================
    // BUTTON HANDLERS
    // ============================================================================

    /** Called when "Auto-Calculate Timing" button is pressed */
    FReply OnAutoCalculateClicked();

    /** Called when "Generate Notifies" button is pressed */
    FReply OnGenerateNotifiesClicked();

    /** Called when "Validate Section" button is pressed */
    FReply OnValidateSectionClicked();

    /** Called when "Preview Timeline" button is pressed */
    FReply OnPreviewTimelineClicked();

    /** Called when "Open Montage" button is pressed */
    FReply OnOpenMontageClicked();

    // ============================================================================
    // HELPERS
    // ============================================================================

    /** Refresh section options from montage */
    void RefreshSectionOptions();

    /** Get timing preview string for display */
    FText GetTimingPreviewText() const;

    /** Check if section has conflicts with other AttackData */
    TArray<UAttackData*> FindSectionConflicts() const;

    /** Show timeline preview window */
    void ShowTimelinePreview();

    /** Refresh the details panel after changes */
    void RefreshDetails();
};