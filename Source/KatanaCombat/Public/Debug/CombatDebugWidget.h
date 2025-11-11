// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CombatDebugWidget.generated.h"

class SCombatDebugDopeSheet;
class UCombatComponentV2;
class SWidget;

/**
 * Component that manages the combat debug overlay
 * Displays dope sheet visualization of V2 combat system
 */
UCLASS(Blueprintable, ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class KATANACOMBAT_API UCombatDebugWidget : public UActorComponent
{
	GENERATED_BODY()

public:
	UCombatDebugWidget();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Toggle debug overlay visibility */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void ToggleDebugOverlay();

	/** Show debug overlay */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void ShowDebugOverlay();

	/** Hide debug overlay */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void HideDebugOverlay();

	/** Is debug overlay visible? */
	UFUNCTION(BlueprintPure, Category = "Combat|Debug")
	bool IsDebugOverlayVisible() const { return bIsVisible; }

	/** Set view range for timeline */
	UFUNCTION(BlueprintCallable, Category = "Combat|Debug")
	void SetViewRange(float Min, float Max);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Combat component being visualized */
	UPROPERTY()
	TObjectPtr<UCombatComponentV2> CombatComponent;

	/** Slate widget */
	TSharedPtr<SCombatDebugDopeSheet> DopeSheetWidget;

	/** Widget container */
	TSharedPtr<SWidget> WidgetContainer;

	/** Is overlay visible? */
	bool bIsVisible;

	/** Current montage time */
	float CurrentTime;

	/** View range */
	float ViewRangeMin;
	float ViewRangeMax;

	/** Update frequency (times per second) */
	float UpdateFrequency;
	float TimeSinceLastUpdate;

	/** Create the Slate widget */
	void CreateWidget();

	/** Remove the Slate widget */
	void RemoveWidget();

	/** Update widget data */
	void UpdateWidget();
};
