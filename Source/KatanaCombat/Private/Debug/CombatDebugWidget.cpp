// Copyright Epic Games, Inc. All Rights Reserved.

#include "Debug/CombatDebugWidget.h"
#include "Debug/SCombatDebugDopeSheet.h"
#include "Core/CombatComponentV2.h"
#include "GameFramework/Character.h"
#include "Animation/AnimInstance.h"
#include "Widgets/SWeakWidget.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"

UCombatDebugWidget::UCombatDebugWidget()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;

	bIsVisible = false;
	CurrentTime = 0.0f;
	ViewRangeMin = 0.0f;
	ViewRangeMax = 5.0f;
	UpdateFrequency = 30.0f; // 30 FPS for UI updates
	TimeSinceLastUpdate = 0.0f;
}

void UCombatDebugWidget::BeginPlay()
{
	Super::BeginPlay();

	// Find combat component
	AActor* Owner = GetOwner();
	if (Owner)
	{
		CombatComponent = Owner->FindComponentByClass<UCombatComponentV2>();
		if (!CombatComponent.Get())
		{
			UE_LOG(LogTemp, Warning, TEXT("[CombatDebugWidget] No CombatComponentV2 found on %s"), *Owner->GetName());
		}
	}
}

void UCombatDebugWidget::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	HideDebugOverlay();
	Super::EndPlay(EndPlayReason);
}

void UCombatDebugWidget::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsVisible || !DopeSheetWidget.IsValid() || !IsValid(CombatComponent))
	{
		return;
	}

	// Update at fixed frequency
	TimeSinceLastUpdate += DeltaTime;
	const float UpdateInterval = 1.0f / UpdateFrequency;

	if (TimeSinceLastUpdate >= UpdateInterval)
	{
		UpdateWidget();
		TimeSinceLastUpdate = 0.0f;
	}
}

void UCombatDebugWidget::ToggleDebugOverlay()
{
	if (bIsVisible)
	{
		HideDebugOverlay();
	}
	else
	{
		ShowDebugOverlay();
	}
}

void UCombatDebugWidget::ShowDebugOverlay()
{
	if (bIsVisible)
	{
		return;
	}

	CreateWidget();
	bIsVisible = true;

	UE_LOG(LogTemp, Log, TEXT("[CombatDebugWidget] Debug overlay shown"));
}

void UCombatDebugWidget::HideDebugOverlay()
{
	if (!bIsVisible)
	{
		return;
	}

	RemoveWidget();
	bIsVisible = false;

	UE_LOG(LogTemp, Log, TEXT("[CombatDebugWidget] Debug overlay hidden"));
}

void UCombatDebugWidget::SetViewRange(float Min, float Max)
{
	ViewRangeMin = Min;
	ViewRangeMax = Max;

	if (DopeSheetWidget.IsValid())
	{
		DopeSheetWidget->SetViewRange(Min, Max);
	}
}

void UCombatDebugWidget::CreateWidget()
{
	if (!IsValid(CombatComponent))
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatDebugWidget] Cannot create widget: No CombatComponentV2"));
		return;
	}

	// Get game viewport
	UGameViewportClient* GameViewport = GetWorld()->GetGameViewport();
	if (!GameViewport)
	{
		UE_LOG(LogTemp, Error, TEXT("[CombatDebugWidget] Cannot create widget: No GameViewport"));
		return;
	}

	// Create dope sheet widget
	DopeSheetWidget = SNew(SCombatDebugDopeSheet, CombatComponent.Get())
		.ViewRangeMin(ViewRangeMin)
		.ViewRangeMax(ViewRangeMax)
		.CurrentTime(CurrentTime);

	// Wrap in container with positioning
	WidgetContainer = SNew(SWeakWidget)
		.PossiblyNullContent(
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(10.0f)
			[
				DopeSheetWidget.ToSharedRef()
			]
		);

	// Add to viewport
	GameViewport->AddViewportWidgetContent(WidgetContainer.ToSharedRef(), 100); // High Z-order

	UE_LOG(LogTemp, Log, TEXT("[CombatDebugWidget] Widget created and added to viewport"));
}

void UCombatDebugWidget::RemoveWidget()
{
	if (!WidgetContainer.IsValid())
	{
		return;
	}

	// Get game viewport
	UGameViewportClient* GameViewport = GetWorld()->GetGameViewport();
	if (GameViewport)
	{
		GameViewport->RemoveViewportWidgetContent(WidgetContainer.ToSharedRef());
	}

	WidgetContainer.Reset();
	DopeSheetWidget.Reset();

	UE_LOG(LogTemp, Log, TEXT("[CombatDebugWidget] Widget removed from viewport"));
}

void UCombatDebugWidget::UpdateWidget()
{
	if (!DopeSheetWidget.IsValid() || !IsValid(CombatComponent))
	{
		return;
	}

	// Update current time from montage
	if (ACharacter* Character = Cast<ACharacter>(GetOwner()))
	{
		if (UAnimInstance* AnimInstance = Character->GetMesh()->GetAnimInstance())
		{
			if (UAnimMontage* CurrentMontage = AnimInstance->GetCurrentActiveMontage())
			{
				CurrentTime = AnimInstance->Montage_GetPosition(CurrentMontage);
			}
		}
	}

	// Update widget
	DopeSheetWidget->SetCurrentTime(CurrentTime);
	DopeSheetWidget->RefreshData();

	// Auto-scroll view range to follow playhead
	if (CurrentTime > ViewRangeMax - 0.5f)
	{
		// Scroll forward
		const float Range = ViewRangeMax - ViewRangeMin;
		ViewRangeMin = CurrentTime - 1.0f;
		ViewRangeMax = ViewRangeMin + Range;
		DopeSheetWidget->SetViewRange(ViewRangeMin, ViewRangeMax);
	}
	else if (CurrentTime < ViewRangeMin + 0.5f && CurrentTime > 0.5f)
	{
		// Scroll backward
		const float Range = ViewRangeMax - ViewRangeMin;
		ViewRangeMax = CurrentTime + (Range - 1.0f);
		ViewRangeMin = ViewRangeMax - Range;
		DopeSheetWidget->SetViewRange(ViewRangeMin, ViewRangeMax);
	}
}
