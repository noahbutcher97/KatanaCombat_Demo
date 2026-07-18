// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AI/EnemyAITypes.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DefenseMatrixProofDirector.generated.h"

class UAttackData;
class UEnemyCombatAIComponent;
class AEnemyCharacter;
class APlayerCharacter;

/** One deterministic case exposed by the dedicated defense proof map. */
USTRUCT(BlueprintType)
struct FDefenseMatrixProofCase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	FName CaseName = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	TObjectPtr<UAttackData> Attack = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	FName AttackerAnchorTag = NAME_None;

	/** Applied after fixture reset so defensive alignment starts from reviewed state. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	bool bApplyDefenderTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof",
		meta = (EditCondition = "bApplyDefenderTransform"))
	FTransform DefenderTransform = FTransform::Identity;

	/** Applied after fixture reset and before guard alignment or attack startup. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	bool bApplyAttackerTransform = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof",
		meta = (EditCondition = "bApplyAttackerTransform"))
	FTransform AttackerTransform = FTransform::Identity;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	bool bBeginHeldGuard = true;
};

/**
 * Runtime-light controller for the dedicated defense matrix map.
 * PIE automation selects named cases while the map remains playable without test code.
 */
UCLASS()
class KATANACOMBAT_API ADefenseMatrixProofDirector : public AActor
{
	GENERATED_BODY()

public:
	ADefenseMatrixProofDirector();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof",
		meta = (ClampMin = "1"))
	int32 ProofMaxConcurrentAttackers = 2;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	TArray<FDefenseMatrixProofCase> Cases;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	bool bAutoStartHandsOffCase = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	FName HandsOffCase = TEXT("NormalBlockMiddleCenter");

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Defense Proof")
	FName ActiveCase = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "Defense Proof")
	bool StartNamedCase(FName CaseName);

	/** Start two independently authored attackers under the map's two-token policy. */
	UFUNCTION(BlueprintCallable, Category = "Defense Proof")
	bool StartNamedThreatPair(FName FirstCaseName, FName SecondCaseName);

	UFUNCTION(BlueprintCallable, Category = "Defense Proof")
	void ResetFixture();

	UFUNCTION(BlueprintPure, Category = "Defense Proof")
	TArray<FName> GetCaseNames() const;

	UFUNCTION(BlueprintPure, Category = "Defense Proof")
	APlayerCharacter* GetFixturePlayer() const;

	UFUNCTION(BlueprintPure, Category = "Defense Proof")
	AEnemyCharacter* GetFixtureEnemy(FName AnchorTag) const;

	UFUNCTION(BlueprintPure, Category = "Defense Proof")
	bool WasLastResetComplete() const { return bLastResetComplete; }

	UFUNCTION(BlueprintPure, Category = "Defense Proof")
	int32 GetCapturedMaxConcurrentAttackers() const
	{
		return PreviousMaxConcurrentAttackers;
	}

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void StartHandsOffCase();
	APlayerCharacter* FindFixturePlayer() const;
	AEnemyCharacter* FindFixtureEnemy(FName AnchorTag) const;
	const FDefenseMatrixProofCase* FindCase(FName CaseName) const;
	bool ValidateCaseForStart(const FDefenseMatrixProofCase& ProofCase) const;
	bool ConfigureEnemyForCase(
		AEnemyCharacter* Enemy,
		const FDefenseMatrixProofCase& ProofCase) const;
	void ClearEnemyAttackConfigs();
	void CaptureFixtureState();

	struct FEnemyFixtureState
	{
		TArray<FEnemyAttackConfig> Attacks;
		EEnemyAttackSelection SelectionMode = EEnemyAttackSelection::Single;
		TWeakObjectPtr<AActor> CombatTarget;
		FTransform ActorTransform = FTransform::Identity;
	};

	int32 PreviousMaxConcurrentAttackers = 1;
	bool bCapturedTokenPolicy = false;
	bool bFixtureStateCaptured = false;
	bool bLastResetComplete = false;
	TMap<TWeakObjectPtr<UEnemyCombatAIComponent>, FEnemyFixtureState> OriginalEnemyStates;
	TWeakObjectPtr<APlayerCharacter> FixturePlayer;
	FTransform OriginalPlayerTransform = FTransform::Identity;
	float OriginalPlayerHealth = 0.0f;
	float OriginalPlayerDamageResistance = 1.0f;
	bool bOriginalPlayerHasSuperArmor = false;
	FTimerHandle HandsOffTimerHandle;
};
