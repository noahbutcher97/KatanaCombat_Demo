// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "CounterIndicatorComponent.generated.h"

class UCombatComponent;

/**
 * Visual indicator component that shows when an enemy can be countered
 *
 * Display States:
 * - Hidden: Enemy not attacking or out of counter window
 * - Normal: Enemy is in counter window, can be countered
 * - Perfect: In "perfect timing" portion of window (last 20%)
 *
 * Usage:
 * 1. Add to enemy character
 * 2. Assign IndicatorWidgetClass (UMG widget with color-changing border)
 * 3. Component auto-updates based on owner's CombatComponent counter state
 *
 * The widget should expose a "SetPerfectWindow" function or use the bound colors
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UCounterIndicatorComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UCounterIndicatorComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ============================================================================
	// CONFIGURATION
	// ============================================================================

	/** Widget class to display as the indicator (should support color changes) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Setup")
	TSubclassOf<UUserWidget> IndicatorWidgetClass;

	/** Color when in normal counter window */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Colors")
	FLinearColor NormalColor = FLinearColor::White;

	/** Color when in perfect timing window (last 20% of counter window) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Colors")
	FLinearColor PerfectWindowColor = FLinearColor::Yellow;

	/** Vertical offset above character */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Position")
	float VerticalOffset = 200.0f;

	/** Perfect timing threshold (0.8 = last 20% of window) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Indicator|Timing")
	float PerfectTimingThreshold = 0.8f;

	// ============================================================================
	// API
	// ============================================================================

	/** Show the counter indicator */
	UFUNCTION(BlueprintCallable, Category = "Counter Indicator")
	void ShowIndicator();

	/** Hide the counter indicator */
	UFUNCTION(BlueprintCallable, Category = "Counter Indicator")
	void HideIndicator();

	/** Set whether we're in perfect timing window (changes color) */
	UFUNCTION(BlueprintCallable, Category = "Counter Indicator")
	void SetPerfectWindowActive(bool bActive);

	/** Check if indicator is currently visible */
	UFUNCTION(BlueprintPure, Category = "Counter Indicator")
	bool IsIndicatorVisible() const { return bIsVisible; }

protected:
	/** Cached reference to owner's combat component */
	UPROPERTY()
	TObjectPtr<UCombatComponent> CachedCombatComponent;

	/** Current visibility state */
	bool bIsVisible = false;

	/** Current perfect window state */
	bool bIsPerfectWindow = false;

	/** Update indicator based on combat component state */
	void UpdateIndicatorState();

	/** Apply color to widget */
	void ApplyColor(const FLinearColor& Color);
};
