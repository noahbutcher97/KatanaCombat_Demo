// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimNotifyState_PairedAnimationCollision.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Core/CombatComponent.h"
#include "Utilities/PairedAnimationUtilityLibrary.h"
#include "Engine/World.h"

UAnimNotifyState_PairedAnimationCollision::UAnimNotifyState_PairedAnimationCollision()
	: SavedPawnCollisionResponse(ECR_Block)
	, SavedCollisionEnabled(ECollisionEnabled::QueryAndPhysics)
	, SavedMovementMode(MOVE_Walking)
	, bModifiedCollision(false)
	, bModifiedMovement(false)
{
}

void UAnimNotifyState_PairedAnimationCollision::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp)
	{
		return;
	}

	AActor* Owner = MeshComp->GetOwner();
	ACharacter* Character = Cast<ACharacter>(Owner);
	if (!Character)
	{
		return;
	}

	// Cache owner for restoration
	CachedOwnerCharacter = Character;

	// Get capsule component
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	// ========================================================================
	// COLLISION MODIFICATION
	// ========================================================================

	if (bDisablePawnCollision || bDisableCapsulePhysics)
	{
		// Save current collision state for fallback mode
		SavedPawnCollisionResponse = Capsule->GetCollisionResponseToChannel(ECC_Pawn);
		SavedCollisionEnabled = Capsule->GetCollisionEnabled();
		bModifiedCollision = true;

		if (bDisablePawnCollision)
		{
			if (bUseTrackedPartnersOnly)
			{
				// PREFERRED: Use tracked partners from CombatComponent
				// Only ignores collision with specific registered partners
				UCombatComponent* CombatComp = GetOwnerCombatComponent();
				if (CombatComp)
				{
					// Clear any stale entries from previous use
					IgnoredPartners.Empty();

					// Get partners from CombatComponent and ignore each one
					const TArray<TWeakObjectPtr<AActor>>& Partners = CombatComp->PairedAnimationPartners;
					for (const TWeakObjectPtr<AActor>& PartnerRef : Partners)
					{
						if (AActor* Partner = PartnerRef.Get())
						{
							// Use IgnoreActorWhenMoving for targeted collision ignore
							Capsule->IgnoreActorWhenMoving(Partner, true);
							IgnoredPartners.Add(Partner);

							UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Ignoring collision with partner %s"),
								*Character->GetName(), *Partner->GetName());
						}
					}

					if (IgnoredPartners.Num() > 0)
					{
						UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Disabled collision with %d tracked partners"),
							*Character->GetName(), IgnoredPartners.Num());
					}
					else
					{
						UE_LOG(LogTemp, Warning, TEXT("[PairedAnimCollision] %s: No tracked partners found! "
							"Call AddPairedPartner() before playing paired animation."),
							*Character->GetName());
					}
				}
				else
				{
					UE_LOG(LogTemp, Warning, TEXT("[PairedAnimCollision] %s: No CombatComponent found, "
						"falling back to global pawn collision disable"),
						*Character->GetName());

					// Fallback to global pawn ignore if no CombatComponent
					Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
				}
			}
			else
			{
				// FALLBACK: Global pawn collision disable (not recommended)
				Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

				UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Disabled ALL pawn collision (global mode)"),
					*Character->GetName());
			}
		}

		if (bDisableCapsulePhysics)
		{
			// Disable all capsule physics (for fully motion-warped paired animations)
			Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);

			UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Disabled capsule physics"),
				*Character->GetName());
		}
	}

	// ========================================================================
	// MOVEMENT MODIFICATION
	// ========================================================================

	if (bDisableMovement)
	{
		UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
		if (MovementComp)
		{
			// Save current movement mode
			SavedMovementMode = MovementComp->MovementMode;
			bModifiedMovement = true;

			// Zero out velocity to prevent momentum carry-over
			MovementComp->Velocity = FVector::ZeroVector;

			// Disable movement (prevents CharacterMovement from fighting root motion)
			MovementComp->DisableMovement();

			UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Disabled movement (was %s)"),
				*Character->GetName(),
				*UEnum::GetValueAsString(SavedMovementMode.GetValue()));
		}
	}
}

void UAnimNotifyState_PairedAnimationCollision::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	RestoreState();
}

void UAnimNotifyState_PairedAnimationCollision::RestoreState()
{
	ACharacter* Character = CachedOwnerCharacter.Get();
	if (!Character)
	{
		return;
	}

	// ========================================================================
	// COLLISION RESTORATION
	// ========================================================================

	if (bModifiedCollision)
	{
		UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
		if (Capsule)
		{
			// Restore collision enabled state (always needed)
			Capsule->SetCollisionEnabled(SavedCollisionEnabled);

			// Track if we had any tracked actors to restore
			const bool bHadTrackedPartners = IgnoredPartners.Num() > 0;
			const bool bHadDynamicObstructions = DynamicallyIgnoredActors.Num() > 0;

			// Restore collision with tracked partners
			if (bHadTrackedPartners)
			{
				for (const TWeakObjectPtr<AActor>& PartnerRef : IgnoredPartners)
				{
					if (AActor* Partner = PartnerRef.Get())
					{
						// Re-enable collision with this specific partner
						Capsule->IgnoreActorWhenMoving(Partner, false);

						UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Restored collision with partner %s"),
							*Character->GetName(), *Partner->GetName());
					}
				}

				UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Restored collision with %d partners"),
					*Character->GetName(), IgnoredPartners.Num());

				IgnoredPartners.Empty();
			}

			// Restore collision with dynamically added actors
			if (bHadDynamicObstructions)
			{
				for (const TWeakObjectPtr<AActor>& ActorRef : DynamicallyIgnoredActors)
				{
					if (AActor* Actor = ActorRef.Get())
					{
						Capsule->IgnoreActorWhenMoving(Actor, false);

						UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Restored collision with dynamic obstruction %s"),
							*Character->GetName(), *Actor->GetName());
					}
				}

				UE_LOG(LogTemp, Log, TEXT("[PairedAnimCollision] %s: Restored collision with %d dynamic obstructions"),
					*Character->GetName(), DynamicallyIgnoredActors.Num());

				DynamicallyIgnoredActors.Empty();
			}

			// If neither list had entries, we were in fallback mode
			if (!bHadTrackedPartners && !bHadDynamicObstructions)
			{
				// Fallback mode - restore global pawn collision response
				Capsule->SetCollisionResponseToChannel(ECC_Pawn, SavedPawnCollisionResponse);

				UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Restored global pawn collision"),
					*Character->GetName());
			}
		}

		bModifiedCollision = false;
	}

	// ========================================================================
	// MOVEMENT RESTORATION
	// ========================================================================

	if (bModifiedMovement)
	{
		UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement();
		if (MovementComp)
		{
			// Restore movement mode
			MovementComp->SetMovementMode(SavedMovementMode);

			UE_LOG(LogTemp, Verbose, TEXT("[PairedAnimCollision] %s: Restored movement mode to %s"),
				*Character->GetName(),
				*UEnum::GetValueAsString(SavedMovementMode.GetValue()));
		}

		bModifiedMovement = false;
	}

	// Clear cached reference
	CachedOwnerCharacter.Reset();
}

FString UAnimNotifyState_PairedAnimationCollision::GetNotifyName_Implementation() const
{
	FString Modifiers;

	if (bDisablePawnCollision)
	{
		Modifiers += TEXT("Pawn");
	}

	if (bDisableCapsulePhysics)
	{
		if (!Modifiers.IsEmpty()) Modifiers += TEXT("+");
		Modifiers += TEXT("Physics");
	}

	if (bDisableMovement)
	{
		if (!Modifiers.IsEmpty()) Modifiers += TEXT("+");
		Modifiers += TEXT("Move");
	}

	if (Modifiers.IsEmpty())
	{
		return TEXT("Paired Collision");
	}

	return FString::Printf(TEXT("Paired Collision [%s]"), *Modifiers);
}

UCombatComponent* UAnimNotifyState_PairedAnimationCollision::GetOwnerCombatComponent() const
{
	ACharacter* Character = CachedOwnerCharacter.Get();
	if (!Character)
	{
		return nullptr;
	}

	return Character->FindComponentByClass<UCombatComponent>();
}

void UAnimNotifyState_PairedAnimationCollision::NotifyTick(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	// Only scan if enabled and we're using tracked partners mode
	if (bScanForDynamicObstructions && bUseTrackedPartnersOnly && bDisablePawnCollision)
	{
		ScanForDynamicObstructions();
	}
}

void UAnimNotifyState_PairedAnimationCollision::ScanForDynamicObstructions()
{
	ACharacter* Character = CachedOwnerCharacter.Get();
	if (!Character)
	{
		return;
	}

	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	if (!Capsule)
	{
		return;
	}

	UWorld* World = Character->GetWorld();
	if (!World)
	{
		return;
	}

	// Build ignore list (self + already ignored actors)
	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(Character);
	for (const TWeakObjectPtr<AActor>& Ref : IgnoredPartners)
	{
		if (AActor* Actor = Ref.Get())
		{
			IgnoreActors.Add(Actor);
		}
	}
	for (const TWeakObjectPtr<AActor>& Ref : DynamicallyIgnoredActors)
	{
		if (AActor* Actor = Ref.Get())
		{
			IgnoreActors.Add(Actor);
		}
	}

	// Use utility library to find obstructing actors
	TArray<AActor*> ObstructingActors = UPairedAnimationUtilityLibrary::FindObstructingActorsInRadius(
		World,
		Character->GetActorLocation(),
		DynamicObstructionRadius,
		IgnoreActors
	);

	// Add any new actors to the dynamically ignored list
	for (AActor* Actor : ObstructingActors)
	{
		if (!Actor || Actor == Character)
		{
			continue;
		}

		// Add to ignored list and disable collision
		Capsule->IgnoreActorWhenMoving(Actor, true);
		DynamicallyIgnoredActors.Add(Actor);

		UE_LOG(LogTemp, Log, TEXT("[PairedAnimCollision] %s: Dynamically ignoring obstruction %s (entered danger zone)"),
			*Character->GetName(), *Actor->GetName());
	}
}

bool UAnimNotifyState_PairedAnimationCollision::IsActorIgnored(AActor* Actor) const
{
	if (!Actor)
	{
		return false;
	}

	// Check pre-registered partners
	for (const TWeakObjectPtr<AActor>& Ref : IgnoredPartners)
	{
		if (Ref.Get() == Actor)
		{
			return true;
		}
	}

	// Check dynamically added actors
	for (const TWeakObjectPtr<AActor>& Ref : DynamicallyIgnoredActors)
	{
		if (Ref.Get() == Actor)
		{
			return true;
		}
	}

	return false;
}
