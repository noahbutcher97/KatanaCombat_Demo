// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Interfaces/CombatInterface.h"
#include "GameFramework/Actor.h"

UAnimNotify_AttackPhaseTransition::UAnimNotify_AttackPhaseTransition()
{
	TransitionToPhase = EAttackPhase::Active;

#if WITH_EDITORONLY_DATA
	NotifyColor = FColor(200, 100, 255, 255);  // Purple for phase transitions
#endif
}

void UAnimNotify_AttackPhaseTransition::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
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
		ICombatInterface::Execute_OnAttackPhaseTransition(Owner, TransitionToPhase);
	}
}

FString UAnimNotify_AttackPhaseTransition::GetNotifyName_Implementation() const
{
	FString PhaseName;

	switch (TransitionToPhase)
	{
		case EAttackPhase::Active:
			PhaseName = TEXT("Active");
			break;
		case EAttackPhase::Recovery:
			PhaseName = TEXT("Recovery");
			break;
		case EAttackPhase::Windup:
			PhaseName = TEXT("Windup");
			break;
		case EAttackPhase::None:
			PhaseName = TEXT("None");
			break;
		default:
			PhaseName = TEXT("Unknown");
			break;
	}

	return FString::Printf(TEXT("Transition to %s"), *PhaseName);
}
