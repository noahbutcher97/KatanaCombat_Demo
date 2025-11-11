// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Interfaces/CombatInterface.h"
#include "GameFramework/Actor.h"

UAnimNotify_HoldWindowStart::UAnimNotify_HoldWindowStart()
{
	InputType = EInputType::LightAttack;

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(100, 200, 255, 255);  // Cyan for hold detection
#endif
}

void UAnimNotify_HoldWindowStart::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner)
	{
		return;
	}

	// Route to ICombatInterface on owner
	if (Owner->Implements<UCombatInterface>())
	{
		ICombatInterface::Execute_OnHoldWindowStart(Owner, InputType);
	}
}

FString UAnimNotify_HoldWindowStart::GetNotifyName_Implementation() const
{
	FString InputName;

	switch (InputType)
	{
		case EInputType::LightAttack:
			InputName = TEXT("Light Attack");
			break;
		case EInputType::HeavyAttack:
			InputName = TEXT("Heavy Attack");
			break;
		case EInputType::Evade:
			InputName = TEXT("Evade");
			break;
		case EInputType::Block:
			InputName = TEXT("Block");
			break;
		case EInputType::Special:
			InputName = TEXT("Special");
			break;
		case EInputType::None:
		default:
			InputName = TEXT("None");
			break;
	}

	return FString::Printf(TEXT("Hold Window Start (%s)"), *InputName);
}
