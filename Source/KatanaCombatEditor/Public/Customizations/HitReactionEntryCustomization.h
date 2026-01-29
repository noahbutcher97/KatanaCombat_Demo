// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IPropertyTypeCustomization.h"

class UAnimMontage;

/**
 * Property type customization for FHitReactionEntry struct
 * Provides section dropdown populated from the selected montage
 */
class FHitReactionEntryCustomization : public IPropertyTypeCustomization
{
public:
    static TSharedRef<IPropertyTypeCustomization> MakeInstance();

    /** IPropertyTypeCustomization interface */
    virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
    virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:
    /** Property handles */
    TSharedPtr<IPropertyHandle> MontageHandle;
    TSharedPtr<IPropertyHandle> SectionHandle;

    /** Available montage sections for dropdown */
    TArray<TSharedPtr<FName>> SectionOptions;

    /** Get the currently selected montage */
    UAnimMontage* GetMontage() const;

    /** Get the currently selected section */
    FName GetSection() const;

    /** Refresh section options from montage */
    void RefreshSectionOptions();

    /** Called when montage selection changes */
    void OnMontageChanged();

    /** Create the section selector widget */
    TSharedRef<SWidget> CreateSectionSelector();

    /** Create the section info widget */
    TSharedRef<SWidget> CreateSectionInfoWidget();
};
