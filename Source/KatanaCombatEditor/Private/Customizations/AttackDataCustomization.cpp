// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/AttackDataCustomization.h"
#include "Data/AttackData.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Misc/MessageDialog.h"
#include "Animation/AnimMontage.h"

#define LOCTEXT_NAMESPACE "AttackDataCustomization"

TSharedRef<IDetailCustomization> FAttackDataCustomization::MakeInstance()
{
    return MakeShareable(new FAttackDataCustomization);
}

void FAttackDataCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
    // Get the AttackData being edited
    TArray<TWeakObjectPtr<UObject>> Objects;
    DetailBuilder.GetObjectsBeingCustomized(Objects);
    
    if (Objects.Num() != 1) return;
    
    CachedAttackData = Cast<UAttackData>(Objects[0].Get());
    if (!CachedAttackData.IsValid()) return;
    
    // Refresh section dropdown options
    RefreshSectionOptions();
    
    // Add validation warnings at top
    AddValidationWarnings(DetailBuilder);
    
    // Customize "Attack|Montage Section" category
    IDetailCategoryBuilder& MontageCategory = DetailBuilder.EditCategory(
        TEXT("Attack|Montage Section"), 
        LOCTEXT("MontageSectionCategory", "Montage Section")
    );
    
    // Add section selector and info
    MontageCategory.AddCustomRow(LOCTEXT("SectionSelectorRow", "Section Selector"))
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
    ];
    
    // Customize "Heavy Attack" category
    IDetailCategoryBuilder& HeavyCategory = DetailBuilder.EditCategory(
        TEXT("Heavy Attack"),
        LOCTEXT("HeavyAttackCategory", "Heavy Attack")
    );

    // Add charge loop section selector
    HeavyCategory.AddCustomRow(LOCTEXT("ChargeLoopSelectorRow", "Charge Loop Selector"))
    .NameContent()
    [
        SNew(STextBlock)
        .Text(LOCTEXT("ChargeLoopSection", "Charge Loop Section"))
        .ToolTipText(LOCTEXT("ChargeLoopSectionTooltip", "Montage section that loops during charge (NAME_None = no loop)"))
        .Font(IDetailLayoutBuilder::GetDetailFontBold())
    ]
    .ValueContent()
    [
        CreateChargeLoopSelector(DetailBuilder)
    ];

    // Add charge release section selector
    HeavyCategory.AddCustomRow(LOCTEXT("ChargeReleaseSelectorRow", "Charge Release Selector"))
    .NameContent()
    [
        SNew(STextBlock)
        .Text(LOCTEXT("ChargeReleaseSection", "Charge Release Section"))
        .ToolTipText(LOCTEXT("ChargeReleaseSectionTooltip", "Montage section to play on release - the actual attack after charging (NAME_None = continue normal)"))
        .Font(IDetailLayoutBuilder::GetDetailFontBold())
    ]
    .ValueContent()
    [
        CreateChargeReleaseSelector(DetailBuilder)
    ];

    // Customize Timing category
    IDetailCategoryBuilder& TimingCategory = DetailBuilder.EditCategory(TEXT("Timing"));

    // Add timing editor and action buttons
    TimingCategory.AddCustomRow(LOCTEXT("TimingToolsRow", "Timing Tools"))
    .WholeRowContent()
    [
        SNew(SVerticalBox)

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 4.0f)
        [
            CreateActionButtons()
        ]

        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 8.0f)
        [
            SNew(STextBlock)
            .Text(this, &FAttackDataCustomization::GetTimingPreviewText)
            .Font(IDetailLayoutBuilder::GetDetailFont())
            .WrapTextAt(400.0f)
        ]
    ];
}

void FAttackDataCustomization::AddValidationWarnings(IDetailLayoutBuilder& DetailBuilder)
{
    if (!CachedAttackData.IsValid()) return;
    
    IDetailCategoryBuilder& WarningCategory = DetailBuilder.EditCategory(
        TEXT("Validation"), 
        LOCTEXT("ValidationCategory", "Validation"),
        ECategoryPriority::Important
    );
    
    // Check if montage is missing
    if (!CachedAttackData->AttackMontage)
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
                .Text(LOCTEXT("NoMontage", "⚠️ No montage assigned! Assign an AttackMontage to begin configuration."))
                .Font(IDetailLayoutBuilder::GetDetailFontBold())
            ]
        ];
        return;
    }
    
    // Check if using AnimNotify timing but notifies are missing
    if (CachedAttackData->bUseAnimNotifyTiming)
    {
        FText ErrorMsg;
        if (!CachedAttackData->ValidateMontageSection(ErrorMsg))
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
                    .Text(FText::Format(LOCTEXT("ValidationError", "⚠️ {0}"), ErrorMsg))
                    .Font(IDetailLayoutBuilder::GetDetailFontBold())
                ]
            ];
        }
        else if (!CachedAttackData->HasValidNotifyTimingInSection())
        {
            WarningCategory.AddCustomRow(LOCTEXT("MissingNotifiesWarning", "Missing Notifies"))
            .WholeRowContent()
            [
                SNew(SBorder)
                .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
                .BorderBackgroundColor(FLinearColor(0.8f, 0.6f, 0.1f))
                .Padding(8.0f)
                [
                    SNew(SVerticalBox)
                    
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("MissingNotifies", 
                            "⚠️ Section is missing required AnimNotifyState_AttackPhase notifies!"))
                        .Font(IDetailLayoutBuilder::GetDetailFontBold())
                    ]
                    
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    .Padding(0.0f, 4.0f)
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("NotifiesHint", 
                            "Use 'Generate Notifies' button below to automatically add them."))
                        .Font(IDetailLayoutBuilder::GetDetailFont())
                    ]
                ]
            ];
        }
    }
    
    // Check for section conflicts
    TArray<UAttackData*> Conflicts = FindSectionConflicts();
    if (Conflicts.Num() > 0)
    {
        FString ConflictList;
        for (UAttackData* Conflict : Conflicts)
        {
            ConflictList += FString::Printf(TEXT("\n• %s"), *Conflict->GetName());
        }
        
        WarningCategory.AddCustomRow(LOCTEXT("SectionConflictWarning", "Section Conflicts"))
        .WholeRowContent()
        [
            SNew(SBorder)
            .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
            .BorderBackgroundColor(FLinearColor(0.6f, 0.4f, 0.8f))
            .Padding(8.0f)
            [
                SNew(STextBlock)
                .Text(FText::FromString(FString::Printf(
                    TEXT("ℹ️ Other AttackData assets use the same montage section:%s"), 
                    *ConflictList)))
                .Font(IDetailLayoutBuilder::GetDetailFont())
            ]
        ];
    }
}

TSharedRef<SWidget> FAttackDataCustomization::CreateSectionSelector(IDetailLayoutBuilder& DetailBuilder)
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
                if (CachedAttackData.IsValid() && NewSelection.IsValid())
                {
                    CachedAttackData->MontageSection = *NewSelection;
                    CachedAttackData->MarkPackageDirty();
                    RefreshDetails();
                }
            })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    if (!CachedAttackData.IsValid()) 
                        return LOCTEXT("NoneSelected", "(None)");
                    
                    FName DisplayName = (CachedAttackData->MontageSection == NAME_None) ?
                                       FName("(Entire Montage)") : CachedAttackData->MontageSection;
                    return FText::FromName(DisplayName);
                })
            ]
        ]
        
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("RefreshSections", "↻"))
            .ToolTipText(LOCTEXT("RefreshSectionsTooltip", "Refresh section list from montage"))
            .OnClicked_Lambda([this]() -> FReply
            {
                RefreshSectionOptions();
                RefreshDetails();
                return FReply::Handled();
            })
        ];
}

TSharedRef<SWidget> FAttackDataCustomization::CreateChargeLoopSelector(IDetailLayoutBuilder& DetailBuilder)
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
                                   FName("(No Loop)") : *Section;
                return SNew(STextBlock).Text(FText::FromName(DisplayName));
            })
            .OnSelectionChanged_Lambda([this](const TSharedPtr<FName>& NewSelection, ESelectInfo::Type SelectType)
            {
                if (CachedAttackData.IsValid() && NewSelection.IsValid())
                {
                    CachedAttackData->ChargeLoopSection = *NewSelection;
                    CachedAttackData->MarkPackageDirty();
                    RefreshDetails();
                }
            })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    if (!CachedAttackData.IsValid())
                        return LOCTEXT("NoneSelected", "(None)");

                    FName DisplayName = (CachedAttackData->ChargeLoopSection == NAME_None) ?
                                       FName("(No Loop)") : CachedAttackData->ChargeLoopSection;
                    return FText::FromName(DisplayName);
                })
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("RefreshSections", "↻"))
            .ToolTipText(LOCTEXT("RefreshSectionsTooltip", "Refresh section list from montage"))
            .OnClicked_Lambda([this]() -> FReply
            {
                RefreshSectionOptions();
                RefreshDetails();
                return FReply::Handled();
            })
        ];
}

TSharedRef<SWidget> FAttackDataCustomization::CreateChargeReleaseSelector(IDetailLayoutBuilder& DetailBuilder)
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
                                   FName("(Continue Normal)") : *Section;
                return SNew(STextBlock).Text(FText::FromName(DisplayName));
            })
            .OnSelectionChanged_Lambda([this](const TSharedPtr<FName>& NewSelection, ESelectInfo::Type SelectType)
            {
                if (CachedAttackData.IsValid() && NewSelection.IsValid())
                {
                    CachedAttackData->ChargeReleaseSection = *NewSelection;
                    CachedAttackData->MarkPackageDirty();
                    RefreshDetails();
                }
            })
            [
                SNew(STextBlock)
                .Text_Lambda([this]() -> FText
                {
                    if (!CachedAttackData.IsValid())
                        return LOCTEXT("NoneSelected", "(None)");

                    FName DisplayName = (CachedAttackData->ChargeReleaseSection == NAME_None) ?
                                       FName("(Continue Normal)") : CachedAttackData->ChargeReleaseSection;
                    return FText::FromName(DisplayName);
                })
            ]
        ]

        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f, 0.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("RefreshSections", "↻"))
            .ToolTipText(LOCTEXT("RefreshSectionsTooltip", "Refresh section list from montage"))
            .OnClicked_Lambda([this]() -> FReply
            {
                RefreshSectionOptions();
                RefreshDetails();
                return FReply::Handled();
            })
        ];
}

TSharedRef<SWidget> FAttackDataCustomization::CreateSectionInfoWidget()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() -> FText
            {
                if (!CachedAttackData.IsValid()) 
                    return LOCTEXT("NoData", "No AttackData");
                
                float Start, End;
                CachedAttackData->GetSectionTimeRange(Start, End);
                
                return FText::FromString(FString::Printf(
                    TEXT("Section Range: %.2fs - %.2fs (%.2fs duration)"),
                    Start, End, End - Start
                ));
            })
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
        ];
}

TSharedRef<SWidget> FAttackDataCustomization::CreateTimingEditor()
{
    // Simplified timing editor - just shows the preview text
    // Full visual timeline editor can be implemented later if needed
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(8.0f)
        [
            SNew(STextBlock)
            .Text(LOCTEXT("TimingEditorPlaceholder", 
                "Use the buttons below to auto-calculate or generate timing from notifies.\n"
                "Visual timeline editor coming soon!"))
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ];
}

TSharedRef<SWidget> FAttackDataCustomization::CreatePhaseTimingRow(
    const FText& PhaseName,
    const FLinearColor& Color,
    TAttribute<float> StartValue,
    TAttribute<float> EndValue,
    FOnFloatValueChanged OnStartChanged,
    FOnFloatValueChanged OnEndChanged)
{
    // Simplified phase timing row - shows values but doesn't allow editing yet
    return SNew(SHorizontalBox)
        
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(PhaseName)
            .ColorAndOpacity(Color)
        ]
        
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([StartValue, EndValue]() -> FText
            {
                return FText::FromString(FString::Printf(
                    TEXT("%.2fs - %.2fs"), 
                    StartValue.Get(), 
                    EndValue.Get()
                ));
            })
        ];
}

TSharedRef<SWidget> FAttackDataCustomization::CreateActionButtons()
{
    return SNew(SHorizontalBox)
        
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("AutoCalculate", "Auto-Calculate Timing"))
            .ToolTipText(LOCTEXT("AutoCalculateTooltip", 
                "Automatically calculate timing based on attack type and montage length"))
            .OnClicked(this, &FAttackDataCustomization::OnAutoCalculateClicked)
            .IsEnabled_Lambda([this]() { return CachedAttackData.IsValid() && CachedAttackData->AttackMontage != nullptr; })
        ]
        
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("GenerateNotifies", "Generate AnimNotifies"))
            .ToolTipText(LOCTEXT("GenerateNotifiesTooltip", 
                "Generate AnimNotifyState_AttackPhase notifies in the montage section"))
            .OnClicked(this, &FAttackDataCustomization::OnGenerateNotifiesClicked)
            .IsEnabled_Lambda([this]() { return CachedAttackData.IsValid() && CachedAttackData->AttackMontage != nullptr; })
        ]
        
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("ValidateSection", "Validate"))
            .ToolTipText(LOCTEXT("ValidateSectionTooltip", 
                "Check if montage section is properly configured"))
            .OnClicked(this, &FAttackDataCustomization::OnValidateSectionClicked)
            .IsEnabled_Lambda([this]() { return CachedAttackData.IsValid(); })
        ]
        
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(2.0f)
        [
            SNew(SButton)
            .Text(LOCTEXT("OpenMontage", "Open Montage Editor"))
            .ToolTipText(LOCTEXT("OpenMontageTooltip", "Open the animation montage in the editor"))
            .OnClicked(this, &FAttackDataCustomization::OnOpenMontageClicked)
            .IsEnabled_Lambda([this]() { return CachedAttackData.IsValid() && CachedAttackData->AttackMontage != nullptr; })
        ];
}

FReply FAttackDataCustomization::OnAutoCalculateClicked()
{
    if (!CachedAttackData.IsValid()) 
        return FReply::Handled();
    
    CachedAttackData->AutoCalculateTimingFromSection();
    CachedAttackData->MarkPackageDirty();
    
    FMessageDialog::Open(EAppMsgType::Ok,
        LOCTEXT("TimingCalculated", "Timing has been auto-calculated based on montage length and attack type."));
    
    RefreshDetails();
    return FReply::Handled();
}

FReply FAttackDataCustomization::OnGenerateNotifiesClicked()
{
    if (!CachedAttackData.IsValid()) 
        return FReply::Handled();
    
    if (CachedAttackData->GenerateNotifiesInSection())
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            LOCTEXT("NotifiesGenerated", 
                "AnimNotifyState_AttackPhase notifies generated successfully!\n\n"
                "Open the montage editor to see the newly added notifies."));
    }
    else
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            LOCTEXT("NotifiesGenerationFailed", 
                "Failed to generate notifies. Check that the montage and section are valid."));
    }
    
    RefreshDetails();
    return FReply::Handled();
}

FReply FAttackDataCustomization::OnValidateSectionClicked()
{
    if (!CachedAttackData.IsValid()) 
        return FReply::Handled();
    
    FText ErrorMsg;
    if (CachedAttackData->ValidateMontageSection(ErrorMsg))
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            LOCTEXT("ValidationSuccess", "✓ Montage section is valid!"));
    }
    else
    {
        FMessageDialog::Open(EAppMsgType::Ok,
            FText::Format(LOCTEXT("ValidationFailed", "✗ Validation failed:\n\n{0}"), ErrorMsg));
    }
    
    return FReply::Handled();
}

FReply FAttackDataCustomization::OnPreviewTimelineClicked()
{
    ShowTimelinePreview();
    return FReply::Handled();
}

FReply FAttackDataCustomization::OnOpenMontageClicked()
{
    if (CachedAttackData.IsValid() && CachedAttackData->AttackMontage)
    {
        GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
            ->OpenEditorForAsset(CachedAttackData->AttackMontage);
    }
    return FReply::Handled();
}

void FAttackDataCustomization::RefreshSectionOptions()
{
    SectionOptions.Empty();
    
    // Always add "Entire Montage" option
    SectionOptions.Add(MakeShareable(new FName(NAME_None)));
    
    if (CachedAttackData.IsValid() && CachedAttackData->AttackMontage)
    {
        UAnimMontage* Montage = CachedAttackData->AttackMontage;
        for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
        {
            FName SectionName = Montage->CompositeSections[i].SectionName;
            SectionOptions.Add(MakeShareable(new FName(SectionName)));
        }
    }
}

FText FAttackDataCustomization::GetTimingPreviewText() const
{
    if (!CachedAttackData.IsValid())
        return LOCTEXT("NoPreview", "No preview available");
    
    return FText::FromString(CachedAttackData->GetTimingPreviewString());
}

TArray<UAttackData*> FAttackDataCustomization::FindSectionConflicts() const
{
    if (!CachedAttackData.IsValid())
        return TArray<UAttackData*>();
    
    return CachedAttackData->FindOtherUsersOfSection();
}

void FAttackDataCustomization::ShowTimelinePreview()
{
    // Future enhancement: Show a visual timeline window
    // For now, just show a message
    FMessageDialog::Open(EAppMsgType::Ok,
        LOCTEXT("PreviewNotImplemented", 
            "Visual timeline preview will be implemented in a future update.\n\n"
            "For now, use 'Open Montage Editor' to view the animation and notifies."));
}

void FAttackDataCustomization::RefreshDetails()
{
    // Request a refresh from the property editor module
    // This forces the details panel to rebuild
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