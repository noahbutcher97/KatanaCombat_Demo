// Copyright Epic Games, Inc. All Rights Reserved.

#include "Customizations/HitReactionEntryCustomization.h"
#include "CombatTypes.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Animation/AnimMontage.h"

#define LOCTEXT_NAMESPACE "HitReactionEntryCustomization"

TSharedRef<IPropertyTypeCustomization> FHitReactionEntryCustomization::MakeInstance()
{
    return MakeShareable(new FHitReactionEntryCustomization);
}

void FHitReactionEntryCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    // Default header with expandable content
    HeaderRow.NameContent()
    [
        PropertyHandle->CreatePropertyNameWidget()
    ];
}

void FHitReactionEntryCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
    // Get handles to the properties we need
    MontageHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FHitReactionEntry, ReactionMontage));
    SectionHandle = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FHitReactionEntry, MontageSection));

    // Refresh section options
    RefreshSectionOptions();

    // Add montage property with change callback
    ChildBuilder.AddProperty(MontageHandle.ToSharedRef())
        .CustomWidget()
        .NameContent()
        [
            MontageHandle->CreatePropertyNameWidget()
        ]
        .ValueContent()
        .MinDesiredWidth(250.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            [
                MontageHandle->CreatePropertyValueWidget()
            ]
        ];

    // Listen for montage changes
    MontageHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FHitReactionEntryCustomization::OnMontageChanged));

    // Add custom section selector row
    ChildBuilder.AddCustomRow(LOCTEXT("SectionRow", "Montage Section"))
        .NameContent()
        [
            SNew(STextBlock)
            .Text(LOCTEXT("MontageSection", "Montage Section"))
            .Font(IDetailLayoutBuilder::GetDetailFont())
        ]
        .ValueContent()
        .MinDesiredWidth(250.0f)
        [
            SNew(SVerticalBox)
            + SVerticalBox::Slot()
            .AutoHeight()
            [
                CreateSectionSelector()
            ]
            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 4.0f, 0.0f, 0.0f)
            [
                CreateSectionInfoWidget()
            ]
        ];

    // Add remaining properties
    uint32 NumChildren;
    PropertyHandle->GetNumChildren(NumChildren);

    for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
    {
        TSharedRef<IPropertyHandle> ChildHandle = PropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
        FName PropertyName = ChildHandle->GetProperty()->GetFName();

        // Skip montage and section (we handle them specially)
        if (PropertyName == GET_MEMBER_NAME_CHECKED(FHitReactionEntry, ReactionMontage) ||
            PropertyName == GET_MEMBER_NAME_CHECKED(FHitReactionEntry, MontageSection))
        {
            continue;
        }

        ChildBuilder.AddProperty(ChildHandle);
    }
}

UAnimMontage* FHitReactionEntryCustomization::GetMontage() const
{
    if (!MontageHandle.IsValid())
    {
        return nullptr;
    }

    UObject* Object = nullptr;
    MontageHandle->GetValue(Object);
    return Cast<UAnimMontage>(Object);
}

FName FHitReactionEntryCustomization::GetSection() const
{
    if (!SectionHandle.IsValid())
    {
        return NAME_None;
    }

    FName Section;
    SectionHandle->GetValue(Section);
    return Section;
}

void FHitReactionEntryCustomization::RefreshSectionOptions()
{
    SectionOptions.Empty();

    // Always add "Entire Montage" option
    SectionOptions.Add(MakeShareable(new FName(NAME_None)));

    UAnimMontage* Montage = GetMontage();
    if (Montage)
    {
        for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
        {
            FName SectionName = Montage->CompositeSections[i].SectionName;
            SectionOptions.Add(MakeShareable(new FName(SectionName)));
        }
    }
}

void FHitReactionEntryCustomization::OnMontageChanged()
{
    RefreshSectionOptions();

    // Reset section to None if current section is no longer valid
    FName CurrentSection = GetSection();
    if (CurrentSection != NAME_None)
    {
        bool bFound = false;
        for (const TSharedPtr<FName>& Option : SectionOptions)
        {
            if (*Option == CurrentSection)
            {
                bFound = true;
                break;
            }
        }

        if (!bFound && SectionHandle.IsValid())
        {
            SectionHandle->SetValue(NAME_None);
        }
    }
}

TSharedRef<SWidget> FHitReactionEntryCustomization::CreateSectionSelector()
{
    return SNew(SComboBox<TSharedPtr<FName>>)
        .OptionsSource(&SectionOptions)
        .OnGenerateWidget_Lambda([](const TSharedPtr<FName>& Section) -> TSharedRef<SWidget>
        {
            FString DisplayString = (*Section == NAME_None) ? TEXT("(Entire Montage)") : Section->ToString();
            return SNew(STextBlock).Text(FText::FromString(DisplayString));
        })
        .OnSelectionChanged_Lambda([this](const TSharedPtr<FName>& NewSelection, ESelectInfo::Type SelectType)
        {
            if (SectionHandle.IsValid() && NewSelection.IsValid())
            {
                SectionHandle->SetValue(*NewSelection);
            }
        })
        [
            SNew(STextBlock)
            .Text_Lambda([this]() -> FText
            {
                FName Section = GetSection();
                FString DisplayString = (Section == NAME_None) ? TEXT("(Entire Montage)") : Section.ToString();
                return FText::FromString(DisplayString);
            })
        ];
}

TSharedRef<SWidget> FHitReactionEntryCustomization::CreateSectionInfoWidget()
{
    return SNew(SBorder)
        .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() -> FText
            {
                UAnimMontage* Montage = GetMontage();
                if (!Montage)
                {
                    return LOCTEXT("NoMontage", "No montage selected");
                }

                FName Section = GetSection();
                float Start = 0.0f;
                float End = Montage->CalculateSequenceLength();

                if (Section != NAME_None)
                {
                    int32 SectionIndex = Montage->GetSectionIndex(Section);
                    if (SectionIndex != INDEX_NONE)
                    {
                        Start = Montage->GetAnimCompositeSection(SectionIndex).GetTime();

                        // Find end (next section or montage end)
                        for (int32 i = 0; i < Montage->CompositeSections.Num(); ++i)
                        {
                            if (i != SectionIndex)
                            {
                                float OtherStart = Montage->CompositeSections[i].GetTime();
                                if (OtherStart > Start && OtherStart < End)
                                {
                                    End = OtherStart;
                                }
                            }
                        }
                    }
                    else
                    {
                        return FText::Format(LOCTEXT("InvalidSection", "Section '{0}' not found!"),
                            FText::FromName(Section));
                    }
                }

                return FText::FromString(FString::Printf(
                    TEXT("Range: %.2fs - %.2fs (%.2fs duration)"),
                    Start, End, End - Start));
            })
            .Font(FCoreStyle::GetDefaultFontStyle("Regular", 8))
        ];
}

#undef LOCTEXT_NAMESPACE
