// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/HitReactionDataCustomization.h"
#include "Data/HitReactionData.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Misc/MessageDialog.h"
#include "Animation/AnimMontage.h"

#define LOCTEXT_NAMESPACE "HitReactionDataCustomization"

TSharedRef<IDetailCustomization> FHitReactionDataCustomization::MakeInstance()
{
    return MakeShareable(new FHitReactionDataCustomization);
}

void FHitReactionDataCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    // Get the HitReactionData being edited
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);

    if (Objects.Num() != 1) return;

    CachedReactionData = Cast<UHitReactionData>(Objects[0].Get());
    if (!CachedReactionData.IsValid()) return;

    // Refresh section dropdown options
    RefreshSectionOptions();

    // Add validation warnings at top
    AddValidationWarnings(DetailBuilder);

    // Customize "Animation|Section" category
    IDetailCategoryBuilder& SectionCategory = DetailBuilder.EditCategory(
        TEXT("Animation|Section"),
        LOCTEXT("MontageSectionCategory", "Montage Section")
    );

    // Add section selector and info
    SectionCategory.AddCustomRow(LOCTEXT("SectionSelectorRow", "Section Selector"))
    .WholeRowContent()
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f)
        [
            CreateSectionSelector(DetailBuilder)
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f)
        [
            CreateSectionInfoWidget()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f)
        [
            CreateActionButtons()
        ]
    ];
}

void FHitReactionDataCustomization::AddValidationWarnings(IDetailLayoutBuilder& DetailBuilder)
{
    if (!CachedReactionData.IsValid()) return;

    IDetailCategoryBuilder& WarningCategory = DetailBuilder.EditCategory(
        TEXT("Validation"),
        LOCTEXT("ValidationCategory", "Validation"),
        ECategoryPriority::Important
    );

    // Check if montage is missing
    if (!CachedReactionData->ReactionMontage)
    {
        WarningCategory.AddCustomRow(LOCTEXT("MissingMontageWarning", "Missing Montage"))
        .WholeRowContent()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .BorderBackgroundColor(FLinearColor(0.8f, 0.2f, 0.1f))
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("NoMontage", "No montage assigned! Assign a ReactionMontage to begin configuration."))
                .Font(IDetailLayoutBuilder::GetDetailFontBold())
            ]
        ];
        return;
    }

    // Check if section is valid
    if (CachedReactionData->MontageSection != NAME_None)
    {
        const int32 SectionIndex = CachedReactionData->ReactionMontage->GetSectionIndex(CachedReactionData->MontageSection);
        if (SectionIndex == INDEX_NONE)
        {
            WarningCategory.AddCustomRow(LOCTEXT("InvalidSectionWarning", "Invalid Section"))
            .WholeRowContent()
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                .BorderBackgroundColor(FLinearColor(0.8f, 0.4f, 0.1f))
                .Padding(8.0f)
                [
                    SNew(STextBlock)
                    .Text(FText::Format(
                        LOCTEXT("SectionNotFound", "Section '{0}' not found in montage!"),
                        FText::FromName(CachedReactionData->MontageSection)))
                    .Font(IDetailLayoutBuilder::GetDetailFontBold())
                ]
            ];
        }
    }

    // Check i-frame timing
    if (CachedReactionData->bHasIFrames && CachedReactionData->IFrameEnd <= CachedReactionData->IFrameStart)
    {
        WarningCategory.AddCustomRow(LOCTEXT("IFrameWarning", "I-Frame Warning"))
        .WholeRowContent()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .BorderBackgroundColor(FLinearColor(0.8f, 0.4f, 0.1f))
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("IFrameInvalid", "IFrameEnd must be greater than IFrameStart!"))
                .Font(IDetailLayoutBuilder::GetDetailFontBold())
            ]
        ];
    }
}

TSharedRef<SWidget> FHitReactionDataCustomization::CreateSectionSelector(IDetailLayoutBuilder& DetailBuilder)
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        [
            SNew(SComboBox<TSharedPtr<FName>>)
            .OptionsSource(&SectionOptions)
            .OnGenerateWidget_Lambda([](const TSharedPtr<FName>& Section) -> TSharedRef<SWidget>
            {
                FName DisplayName = (*Section == NAME_None) ?
                                   FName("(Entire Montage)") : *Section;
                return SNew(STextBlock).Text(FText::FromName(DisplayName));
            })
            .OnSelectionChanged_Lambda([this](const TSharedPtr<FName>& NewSelection, ESelectInfo::Type SelectType)
            {
                if (CachedReactionData.IsValid() && NewSelection.IsValid())
                {
                    CachedReactionData->MontageSection = *NewSelection;
                    CachedReactionData->MarkPackageDirty();
                    RefreshDetails();
                }
            })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    if (!CachedReactionData.IsValid())
                        return LOCTEXT("NoneSelected", "(None)");

                    FName DisplayName = (CachedReactionData->MontageSection == NAME_None) ?
                                       FName("(Entire Montage)") : CachedReactionData->MontageSection;
                    return FText::FromName(DisplayName);
                })
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SButton)
            .Text(FText::FromString(TEXT("\u21BB")))  // Unicode refresh symbol
            .ToolTipText(LOCTEXT("RefreshSectionsTooltip", "Refresh section list from montage"))
            .OnClicked_Lambda([this]() -> FReply
            {
                RefreshSectionOptions();
                RefreshDetails();
                return FReply::Handled();
            })
        ];
}

TSharedRef<SWidget> FHitReactionDataCustomization::CreateSectionInfoWidget()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() -> FText
            {
                if (!CachedReactionData.IsValid())
                    return LOCTEXT("NoData", "No HitReactionData");

                float Start, End;
                CachedReactionData->GetSectionTimeRange(Start, End);

                return FText::FromString(FString::Printf(
                    TEXT("Section Range: %.2fs - %.2fs (%.2fs duration)"),
                    Start, End, End - Start
                ));
            })
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
        ];
}

TSharedRef<SWidget> FHitReactionDataCustomization::CreateActionButtons()
{
    return SNew(SHorizontalBox)

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("Validate", "Validate"))
            .ToolTipText(LOCTEXT("ValidateTooltip", "Check if montage section is properly configured"))
            .OnClicked(this, &FHitReactionDataCustomization::OnValidateClicked)
            .IsEnabled_Lambda([this]() { return CachedReactionData.IsValid(); })
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("OpenMontage", "Open Montage Editor"))
            .ToolTipText(LOCTEXT("OpenMontageTooltip", "Open the animation montage in the editor"))
            .OnClicked(this, &FHitReactionDataCustomization::OnOpenMontageClicked)
            .IsEnabled_Lambda([this]() { return CachedReactionData.IsValid() && CachedReactionData->ReactionMontage != nullptr; })
        ];
}

FReply FHitReactionDataCustomization::OnValidateClicked()
{
    if (!CachedReactionData.IsValid())
        return FReply::Handled();

    // Collect validation issues
    TArray<FString> Errors;
    TArray<FString> Warnings;

    // Check montage exists
    if (!CachedReactionData->ReactionMontage)
    {
        Errors.Add(TEXT("No ReactionMontage assigned"));
    }
    else
    {
        // Check section exists if specified
        if (CachedReactionData->MontageSection != NAME_None)
        {
            const int32 SectionIndex = CachedReactionData->ReactionMontage->GetSectionIndex(CachedReactionData->MontageSection);
            if (SectionIndex == INDEX_NONE)
            {
                Errors.Add(FString::Printf(TEXT("Section '%s' not found in montage '%s'"),
                    *CachedReactionData->MontageSection.ToString(),
                    *CachedReactionData->ReactionMontage->GetName()));
            }
        }

        // Check i-frame timing against section length
        if (CachedReactionData->bHasIFrames)
        {
            if (CachedReactionData->IFrameEnd <= CachedReactionData->IFrameStart)
            {
                Errors.Add(FString::Printf(TEXT("IFrameEnd (%.2f) must be greater than IFrameStart (%.2f)"),
                    CachedReactionData->IFrameEnd, CachedReactionData->IFrameStart));
            }

            const float SectionLength = CachedReactionData->GetSectionLength();
            if (SectionLength > 0.0f && CachedReactionData->IFrameEnd > SectionLength)
            {
                Warnings.Add(FString::Printf(TEXT("IFrameEnd (%.2f) exceeds section length (%.2f)"),
                    CachedReactionData->IFrameEnd, SectionLength));
            }
        }

        // Check stun duration vs section length
        const float SectionLength = CachedReactionData->GetSectionLength();
        if (SectionLength > 0.0f && CachedReactionData->StunDuration > SectionLength)
        {
            Warnings.Add(FString::Printf(TEXT("StunDuration (%.2f) exceeds section length (%.2f)"),
                CachedReactionData->StunDuration, SectionLength));
        }
    }

    // Check paired reaction config
    if (CachedReactionData->bIsPairedReaction)
    {
        if (CachedReactionData->PairedType == EPairedReactionType::None)
        {
            Errors.Add(TEXT("bIsPairedReaction is true but PairedType is None"));
        }
        if (CachedReactionData->PairedReactionName == NAME_None)
        {
            Warnings.Add(TEXT("bIsPairedReaction is true but PairedReactionName is not set"));
        }
    }

    // Show result
    if (Errors.Num() == 0 && Warnings.Num() == 0)
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            LOCTEXT("ValidationSuccess", "HitReactionData is valid!"));
    }
    else
    {
        FString Message;

        if (Errors.Num() > 0)
        {
            Message += TEXT("Errors:");
            for (const FString& Error : Errors)
            {
                Message += FString::Printf(TEXT("\n  - %s"), *Error);
            }
        }

        if (Warnings.Num() > 0)
        {
            if (!Message.IsEmpty()) Message += TEXT("\n\n");
            Message += TEXT("Warnings:");
            for (const FString& Warning : Warnings)
            {
                Message += FString::Printf(TEXT("\n  - %s"), *Warning);
            }
        }

        FMessageDialog::Open(EAppMsgType::Ok,
            FText::FromString(Message));
    }

    return FReply::Handled();
}

FReply FHitReactionDataCustomization::OnOpenMontageClicked()
{
    if (CachedReactionData.IsValid() && CachedReactionData->ReactionMontage)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
            ->OpenEditorForAsset(CachedReactionData->ReactionMontage);
    }
    return FReply::Handled();
}

void FHitReactionDataCustomization::RefreshSectionOptions()
{
    SectionOptions.Empty();

    // Always add "Entire Montage" option
    SectionOptions.Add(MakeShareable(new FName(NAME_None)));

    if (CachedReactionData.IsValid() && CachedReactionData->ReactionMontage)
    {
        UAnimMontage* Montage = CachedReactionData->ReactionMontage;
        for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
        {
            FName SectionName = Montage->CompositeSections[i].SectionName;
            SectionOptions.Add(MakeShareable(new FName(SectionName)));
        }
    }
}

void FHitReactionDataCustomization::RefreshDetails()
{
    // Request a refresh from the property editor module
    if (GEditor)
    {
        GEditor->GetTimerManager()->SetTimerForNextTick([]()
        {
            if (GEditor)
            {
                FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
                PropertyModule.NotifyCustomizationModuleChanged();
            }
        });
    }
}

#undef LOCTEXT_NAMESPACE
