// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/CounterIndicatorComponent.h"
#include "Core/CombatComponent.h"
#include "Blueprint/UserWidget.h"

UCounterIndicatorComponent::UCounterIndicatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	// Default widget space settings
	SetWidgetSpace(EWidgetSpace::Screen);
	SetDrawAtDesiredSize(true);
}

void UCounterIndicatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// Cache combat component reference
	if (AActor* Owner = GetOwner())
	{
		CachedCombatComponent = Owner->FindComponentByClass<UCombatComponent>();
	}

	// Create widget if class is assigned
	if (IndicatorWidgetClass)
	{
		SetWidgetClass(IndicatorWidgetClass);
	}

	// Start hidden
	SetVisibility(false);
	bIsVisible = false;

	// Position above character
	SetRelativeLocation(FVector(0.0f, 0.0f, VerticalOffset));
}

void UCounterIndicatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateIndicatorState();
}

void UCounterIndicatorComponent::ShowIndicator()
{
	if (!bIsVisible)
	{
		bIsVisible = true;
		SetVisibility(true);
		SetComponentTickEnabled(true);
		ApplyColor(NormalColor);
	}
}

void UCounterIndicatorComponent::HideIndicator()
{
	if (bIsVisible)
	{
		bIsVisible = false;
		bIsPerfectWindow = false;
		SetVisibility(false);
		SetComponentTickEnabled(false);
	}
}

void UCounterIndicatorComponent::SetPerfectWindowActive(bool bActive)
{
	if (bIsPerfectWindow != bActive)
	{
		bIsPerfectWindow = bActive;
		ApplyColor(bActive ? PerfectWindowColor : NormalColor);
	}
}

void UCounterIndicatorComponent::UpdateIndicatorState()
{
	if (!CachedCombatComponent.Get())
	{
		return;
	}

	// Check if owner is in counter window
	const bool bInCounterWindow = CachedCombatComponent->IsInCounterWindow();

	if (bInCounterWindow && !bIsVisible)
	{
		ShowIndicator();
	}
	else if (!bInCounterWindow && bIsVisible)
	{
		HideIndicator();
	}

	// Update perfect window state if visible
	if (bIsVisible)
	{
		const float CounterProgress = CachedCombatComponent->GetCounterWindowProgress();
		const bool bShouldBePerfect = CounterProgress >= PerfectTimingThreshold;
		SetPerfectWindowActive(bShouldBePerfect);
	}
}

void UCounterIndicatorComponent::ApplyColor(const FLinearColor& Color)
{
	// The widget should implement color changing logic
	// For now, we can use the widget's color and opacity property
	SetTintColorAndOpacity(Color);
}
