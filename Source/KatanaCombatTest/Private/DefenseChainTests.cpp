// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"
#include "CombatEventRecorder.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimNotify_ChainStageTransition.h"
#include "Animation/AnimNotifyState_PairedAnimationSync.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Containers/Ticker.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/TargetingComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackConfiguration.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Data/HitReactionSettings.h"
#include "Data/PairedAnimationData.h"
#include "Data/TargetingSettings.h"
#include "Debug/ActionReactionTelemetry.h"
#include "Debug/DefenseTelemetry.h"
#include "Subsystems/CombatEffectsWorldSubsystem.h"
#include "Utilities/CombatGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/BoxComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"

#include <limits>

struct FDefenseChainFixture
{
	UWorld* World = nullptr;
	APlayerCharacter* Defender = nullptr;
	UCombatComponent* DefenderCombat = nullptr;
	AEnemyCharacter* SourceAttacker = nullptr;
	UCombatComponent* SourceCombat = nullptr;
	UPairedAnimationComponent* Paired = nullptr;
	UPairedAnimationComponent* SourcePaired = nullptr;
	UAttackData* SourceAttack = nullptr;
	UAttackData* CounterAttack = nullptr;
	UDefenseConfiguration* DefenseConfig = nullptr;
	FAttackInstanceId AttackInstance;

	bool Initialize()
	{
		World = FCombatTestHelpers::CreateTestWorld();
		Defender = FCombatTestHelpers::CreateTestCharacterWithCombat(World, DefenderCombat);
		SourceAttacker = FCombatTestHelpers::CreateTestEnemyCharacter(
			World, FVector(250.0f, 0.0f, 0.0f));
		SourceCombat = SourceAttacker ? SourceAttacker->CombatComponent.Get() : nullptr;
		Paired = Defender ? Defender->PairedAnimationComponent.Get() : nullptr;
		SourcePaired = SourceAttacker ? SourceAttacker->PairedAnimationComponent.Get() : nullptr;
		if (!World || !Defender || !DefenderCombat || !SourceAttacker
			|| !SourceCombat || !Paired || !SourcePaired)
		{
			return false;
		}
		BindCombatComponentCaches();
		SourceAttacker->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));

		UCombatSettings* Settings = FCombatTestHelpers::CreateTestCombatSettings();
		CounterAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
		if (UAttackConfiguration* Attacks = Settings ? Settings->GetAttackConfiguration() : nullptr)
		{
			Attacks->DefaultLightAttack = CounterAttack;
			Attacks->DefaultHeavyAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
		}
		Defender->CombatSettings = Settings;
		DefenderCombat->CombatSettings = Settings;

		UTargetingSettings* TargetingSettings = NewObject<UTargetingSettings>();
		TargetingSettings->MaxTargetDistance = 1500.0f;
		TargetingSettings->bRequireLineOfSight = false;
		Defender->TargetingComponent->TargetingSettingsOverride = TargetingSettings;

		DefenseConfig = NewObject<UDefenseConfiguration>();
		DefenseConfig->DefenseThreatRange = 1000.0f;
		DefenseConfig->MaximumHighConfidencePredictionAge = 1.0f;
		DefenseConfig->HardGuardConeHalfAngle = 70.0f;
		DefenseConfig->MaximumAutomaticTurn = 70.0f;
		DefenseConfig->DefenseTurnRate = 360.0f;
		DefenseConfig->PerfectParryFinalTolerance = 10.0f;
		DefenseConfig->NoMontageParryBridgeSeconds = 0.05f;
		DefenseConfig->CounterWindowSeconds = 5.0f;
		DefenseConfig->FinisherReadySeconds = 5.0f;
		DefenseConfig->TimeDilationLeaseWatchdogSeconds = 10.0f;
		DefenderCombat->DefenseConfigurationOverride = DefenseConfig;

		SourceAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
		SourceAttack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
		return ArmParryableAttack(SourceAttacker, SourceAttack, 41, 401, AttackInstance);
	}

	/**
	 * Cache each participant's paired animation component on its combat component, as component BeginPlay
	 * does at runtime, so the input gate that reads the cache (a committed defender rejects input) is live.
	 * The test world never runs BeginPlay.
	 */
	void BindCombatComponentCaches() const
	{
		DefenderCombat->CachedPairedAnimComp = Paired;
		SourceCombat->CachedPairedAnimComp = SourcePaired;
	}

	/**
	 * Let the defender's hit reactions play real montages on its anim instance with Settings, as component
	 * BeginPlay (which caches the instance) does at runtime. Needs ConfigureProductionMeshes first.
	 */
	bool EnableDefenderHitReactionPlayback(UHitReactionSettings* Settings) const
	{
		UHitReactionComponent* HitReaction = Defender->HitReactionComponent.Get();
		UAnimInstance* DefenderAnim = Defender->GetMesh() ? Defender->GetMesh()->GetAnimInstance() : nullptr;
		if (!HitReaction || !DefenderAnim || !Settings)
		{
			return false;
		}
		HitReaction->OwnerCharacter = Defender;
		HitReaction->AnimInstance = DefenderAnim;
		HitReaction->HitReactionSettingsOverride = Settings;
		return true;
	}

	/**
	 * Give Attacker an active attack aimed at the defender, with a high-confidence threat prediction and
	 * open Hit and Parry windows, so a Block press can commit a perfect parry against it.
	 */
	bool ArmParryableAttack(
		AEnemyCharacter* Attacker,
		UAttackData* Attack,
		const int32 AttackGeneration,
		const int32 MontageInstanceId,
		FAttackInstanceId& OutAttackInstance) const
	{
		UCombatComponent* AttackerCombat = Attacker ? Attacker->CombatComponent.Get() : nullptr;
		if (!AttackerCombat || !Attack)
		{
			return false;
		}
		AttackerCombat->SeedAttackWindowStateForTesting(Attack, EAttackPhase::Windup, AttackGeneration);
		AttackerCombat->SetAttackIntentTarget(Defender);

		const double Now = World->GetTimeSeconds();
		FAttackThreatPrediction Prediction;
		Prediction.IntendedTarget = Defender;
		Prediction.PathOrigin = Attacker->GetActorLocation();
		Prediction.PathDirection =
			(Defender->GetActorLocation() - Attacker->GetActorLocation()).GetSafeNormal();
		Prediction.PredictedContactPoint = Defender->GetActorLocation();
		Prediction.SourceSocket = TEXT("weapon_tip");
		Prediction.DefenderTargetBone = TEXT("spine_03");
		Prediction.PredictionSimulationTimestamp = Now;
		Prediction.PredictedContactSimulationTime = Now + 0.20;
		Prediction.Lane = EIncomingAttackLane::Center;
		Prediction.Height = EAttackHeight::Middle;
		Prediction.Confidence = EDefensePredictionConfidence::High;
		Prediction.bPathIntersectsThreatVolume = true;
		AttackerCombat->PublishAttackThreatPrediction(Prediction);

		FAnimNotifyRuntimeSourceId HitSource;
		HitSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/HitWindow"));
		HitSource.NotifyEventIndex = 1;
		FAnimNotifyRuntimeSourceId ParrySource;
		ParrySource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/ParryWindow"));
		ParrySource.NotifyEventIndex = 2;
		const FAttackWindowInstanceId HitWindow = AttackerCombat->OpenAttackWindow(
			EAttackWindowKind::Hit, HitSource, MontageInstanceId, 0.40f);
		const FAttackWindowInstanceId ParryWindow = AttackerCombat->OpenAttackWindow(
			EAttackWindowKind::Parry, ParrySource, MontageInstanceId, 0.40f);
		OutAttackInstance = ParryWindow.AttackInstance;
		return HitWindow.IsValid() && ParryWindow.IsValid();
	}

	bool StartCommittedParry() const
	{
		DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		return Paired->GetChainState() == EChainCounterState::ParryActive
			&& SourceCombat->IsAttackConsumed(AttackInstance);
	}

	bool OpenCounterWindow() const
	{
		const int32 Generation = Paired->ActiveDefenseSequence.StageGeneration;
		const FDefenseAsyncHandle AsyncHandle =
			Paired->ActiveDefenseSequence.BridgeFallbackHandle;
		Paired->HandleNoMontageDefenseBridgeElapsed(Generation, AsyncHandle);
		return Paired->GetChainState() == EChainCounterState::CounterWindow;
	}

	bool PreflightStage(
		UPairedAnimationData* Data,
		const EPairedReactionType ReactionType,
		FString& OutFailureReason) const
	{
		return Paired
			&& Paired->PreflightDefenseChainStage(Data, ReactionType, OutFailureReason);
	}

	bool PreflightBridge(
		UPairedAnimationData* Data,
		const FName ReviewedMarker,
		FString& OutFailureReason) const
	{
		return PreflightBridgeWithResolution(
			Data,
			ReviewedMarker,
			DefenderCombat->GetLastInputDefenseResolutionForTesting(),
			OutFailureReason);
	}

	bool PreflightBridgeWithResolution(
		UPairedAnimationData* Data,
		const FName ReviewedMarker,
		const FDefenseResolution& Resolution,
		FString& OutFailureReason) const
	{
		FDefensePresentationPayload Presentation;
		Presentation.PairedBridgeData = Data;
		Presentation.ReviewedDeflectionMarker = ReviewedMarker;
		return Paired
			&& Paired->PreflightDefenseBridge(
				Resolution,
				Presentation,
				OutFailureReason);
	}

	void SetPlaybackOverride(
		TFunction<bool(EPairedAnimationRole, const UPairedAnimationData*, int32&)> Override) const
	{
		Paired->DefenseStagePlaybackOverrideForTesting = MoveTemp(Override);
	}

	void EnableLethalCounterData() const
	{
		Paired->bAllowLethalCounterPairedData = true;
	}

	bool TryCompetingPairedStart(
		AActor* Target,
		UPairedAnimationData* Data,
		const EPairedReactionType ReactionType) const
	{
		return Paired
			&& Paired->TryStartPairedAnimationWithTarget(Target, Data, ReactionType);
	}

	/** Replace the no-montage fallback with a montage-backed bridge started through the real stage path. */
	bool StartBridgeStage(UPairedAnimationData* BridgeData) const
	{
		Paired->CancelDefenseAsyncHandle(Paired->ActiveDefenseSequence.BridgeFallbackHandle);
		Paired->ActiveDefenseSequence.BridgeFallbackHandle = {};
		return Paired->TryStartDefenseChainStage(
				BridgeData,
				EPairedReactionType::Parry,
				EChainCounterState::ParryActive)
			&& Paired->GetChainState() == EChainCounterState::ParryActive
			&& Paired->ActiveDefenseSequence.ActivePairedData == BridgeData;
	}

	/** Deliver the bridge driver's exact OpenCounterWindow marker (authored as notify 0). */
	void DeliverBridgeMarker(const UPairedAnimationData* BridgeData) const
	{
		FAnimNotifyRuntimeSourceId Source;
		Source.SourceAnimation = FSoftObjectPath(BridgeData->AttackerMontage);
		Source.NotifyEventIndex = 0;
		Paired->HandleChainStageTransition(
			EChainStageTransitionType::OpenCounterWindow,
			Paired->ActiveDefenseSequence.AttackerMontageInstanceId,
			Source);
	}

	/** Both bridge roles finish without interruption, including the source role's deferred verification. */
	void EndBridgeMontagesNaturally(const UPairedAnimationData* BridgeData) const
	{
		Paired->HandleOwnerPairedMontageEnded(BridgeData->AttackerMontage, false);
		SourcePaired->HandleOwnerPairedMontageEnded(BridgeData->VictimMontage, false);
		FTSTicker::GetCoreTicker().Tick(0.0f);
	}

	/**
	 * Route each role's real stage montage end to its paired component, as UCombatComponent::OnMontageEnded
	 * does at runtime; component BeginPlay, which binds that route, never runs in the test world.
	 */
	void RouteStageMontageEnds(const UPairedAnimationData* Stage) const
	{
		UPairedAnimationComponent* DefenderPaired = Paired;
		UPairedAnimationComponent* SourcePairedComponent = SourcePaired;
		FOnMontageEnded DefenderEnded = FOnMontageEnded::CreateWeakLambda(
			DefenderPaired,
			[DefenderPaired](UAnimMontage* Montage, const bool bInterrupted)
			{
				DefenderPaired->HandleOwnerPairedMontageEnded(Montage, bInterrupted);
			});
		FOnMontageEnded SourceEnded = FOnMontageEnded::CreateWeakLambda(
			SourcePairedComponent,
			[SourcePairedComponent](UAnimMontage* Montage, const bool bInterrupted)
			{
				SourcePairedComponent->HandleOwnerPairedMontageEnded(Montage, bInterrupted);
			});
		Defender->GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(DefenderEnded, Stage->AttackerMontage);
		SourceAttacker->GetMesh()->GetAnimInstance()->Montage_SetEndDelegate(SourceEnded, Stage->VictimMontage);
	}

	/**
	 * Route every montage end on both participants through their combat components, as combat component
	 * BeginPlay binds it at runtime, so stopped and rolled-back stage montages reach the paired components too.
	 * Do not combine with RouteStageMontageEnds, which would deliver the same end twice.
	 */
	void RouteAllMontageEndsThroughCombat() const
	{
		Defender->GetMesh()->GetAnimInstance()->OnMontageEnded.AddDynamic(
			DefenderCombat, &UCombatComponent::OnMontageEnded);
		SourceAttacker->GetMesh()->GetAnimInstance()->OnMontageEnded.AddDynamic(
			SourceCombat, &UCombatComponent::OnMontageEnded);
	}

	/**
	 * Put both participants in walking movement, as character BeginPlay does at runtime, so a movement
	 * lock and its release are observable; the test world never runs BeginPlay.
	 */
	void EnableDefaultMovement() const
	{
		Defender->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		SourceAttacker->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}

	/** Both roles have consumed every retired stage-montage callback, so a later stage can start. */
	bool HasNoPendingRoleMontageCallbacks() const
	{
		return Paired->RetiredOwnerMontageCallbacks.IsEmpty()
			&& SourcePaired->RetiredOwnerMontageCallbacks.IsEmpty();
	}

	/** Give both participants a production mesh and AnimBP so stages play real montage instances. */
	bool ConfigureProductionMeshes() const
	{
		const auto ConfigureMesh = [](ABaseCombatCharacter* Character, const TCHAR* ClassPath)
		{
			UClass* Class = LoadClass<ABaseCombatCharacter>(nullptr, ClassPath);
			const ABaseCombatCharacter* Defaults = Class
				? Class->GetDefaultObject<ABaseCombatCharacter>()
				: nullptr;
			if (!Character || !Defaults || !Defaults->GetMesh() || !Character->GetMesh())
			{
				return false;
			}
			Character->GetMesh()->SetSkeletalMesh(Defaults->GetMesh()->GetSkeletalMeshAsset());
			Character->GetMesh()->SetAnimInstanceClass(Defaults->GetMesh()->GetAnimClass());
			return Character->GetMesh()->GetAnimInstance() != nullptr;
		};
		return ConfigureMesh(Defender, TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_Player.BP_Player_C"))
			&& ConfigureMesh(
				SourceAttacker,
				TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_EnemyCharacter.BP_EnemyCharacter_C"));
	}

	/**
	 * Tick both participants' animation (montage weight, then position) and dispatch their queued notifies
	 * and montage events, in the engine's per-frame order: all notifies before any montage event.
	 */
	void AdvanceMontages(const float Seconds, const float Step) const
	{
		USkeletalMeshComponent* DefenderMesh = Defender->GetMesh();
		USkeletalMeshComponent* SourceMesh = SourceAttacker->GetMesh();
		float Remaining = Seconds;
		while (Remaining > UE_KINDA_SMALL_NUMBER)
		{
			const float Delta = FMath::Min(Step, Remaining);
			DefenderMesh->TickAnimation(Delta, false);
			SourceMesh->TickAnimation(Delta, false);
			if (UAnimInstance* DefenderAnim = DefenderMesh->GetAnimInstance())
			{
				DefenderAnim->DispatchQueuedAnimEvents();
			}
			if (UAnimInstance* SourceAnim = SourceMesh->GetAnimInstance())
			{
				SourceAnim->DispatchQueuedAnimEvents();
			}
			Remaining -= Delta;
		}
	}

	/** Make the bridge marker's runtime identity stale so it can never open CounterWindow. */
	void InvalidateBridgeMarkerIdentity() const
	{
		Paired->ActiveDefenseSequence.AttackerMontageInstanceId += 1000;
	}

	/** Make a source-driven bridge marker's runtime identity stale so it can never open CounterWindow. */
	void InvalidateSourceBridgeMarkerIdentity() const
	{
		Paired->ActiveDefenseSequence.VictimMontageInstanceId += 1000;
	}

	EChainCounterState GetChainState() const
	{
		return Paired->GetChainState();
	}

	void SetPendingRoleMontageCallback(
		UAnimMontage* Montage,
		const EPairedAnimationRole Role,
		const bool bPending) const
	{
		UPairedAnimationComponent* RoleComponent =
			Role == EPairedAnimationRole::Attacker ? Paired : SourcePaired;
		if (bPending)
		{
			RoleComponent->RetireOwnerMontageCallback(Montage);
		}
		else
		{
			RoleComponent->CancelRetiredOwnerMontageCallback(Montage);
		}
	}

	void Destroy() const
	{
		if (Paired && Paired->GetChainState() != EChainCounterState::None)
		{
			Paired->CancelPairedAnimation(0.0f);
		}
		FCombatTestHelpers::DestroyTestWorld(World);
	}
};

namespace
{

UPairedAnimationData* CreateChainStageData(
	const EPairedReactionType ReactionType,
	const EChainStageTransitionType Transition = EChainStageTransitionType::OpenCounterWindow,
	const FName MarkerName = NAME_None,
	const bool bAutoContinue = false)
{
	UPairedAnimationData* Data = NewObject<UPairedAnimationData>();
	Data->ReactionType = ReactionType;
	Data->AttackerMontage = NewObject<UAnimMontage>(Data);
	Data->VictimMontage = NewObject<UAnimMontage>(Data);
	Data->bIsLethal = false;
	Data->BaseDamage = 7.0f;
	Data->DamageMultiplier = 1.0f;
	Data->ChainTransitionPolicy.RequiredMarker = MarkerName;
	Data->ChainTransitionPolicy.bAutoContinue = bAutoContinue;
	Data->ChainTransitionPolicy.bFinisherRetryable = true;
	if (!MarkerName.IsNone())
	{
		Data->AttackerMontage->SetCompositeLength(1.0f);
		FCompositeSection StageSection;
		StageSection.SectionName = TEXT("Stage");
		StageSection.SetTime(0.0f);
		Data->AttackerMontage->CompositeSections.Add(StageSection);
		Data->AttackerMontageSection = StageSection.SectionName;
		UAnimNotify_ChainStageTransition* Notify =
			NewObject<UAnimNotify_ChainStageTransition>(Data->AttackerMontage);
		Notify->Transition = Transition;
		Notify->MarkerName = MarkerName;
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.SetTime(0.5f);
		Data->AttackerMontage->Notifies.Add(Event);
	}
	return Data;
}

FAnimNotifyRuntimeSourceId MakeMarkerSource(const UAnimMontage* Montage, const int32 Index = 0)
{
	FAnimNotifyRuntimeSourceId Source;
	Source.SourceAnimation = FSoftObjectPath(Montage);
	Source.NotifyEventIndex = Index;
	return Source;
}

class FScopedActionReactionTelemetry
{
public:
	explicit FScopedActionReactionTelemetry(const int32 Value)
	{
		Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
		if (Variable)
		{
			Previous = Variable->GetInt();
			Variable->Set(Value, ECVF_SetByCode);
		}
	}

	~FScopedActionReactionTelemetry()
	{
		if (Variable)
		{
			Variable->Set(Previous, ECVF_SetByCode);
		}
	}

private:
	IConsoleVariable* Variable = nullptr;
	int32 Previous = 0;
};

int32 CountActionReactionTelemetry(
	const TConstArrayView<FActionReactionTelemetryRecord> Records,
	const EActionReactionTelemetryEvent Event,
	const EActionReactionTelemetryReason Reason)
{
	int32 Count = 0;
	for (const FActionReactionTelemetryRecord& Record : Records)
	{
		Count += Record.Event == Event && Record.Reason == Reason ? 1 : 0;
	}
	return Count;
}

class FScopedIntConsoleVariableOverride
{
public:
	FScopedIntConsoleVariableOverride(const TCHAR* Name, const int32 Value)
	{
		Variable = IConsoleManager::Get().FindConsoleVariable(Name);
		if (Variable)
		{
			Previous = Variable->GetInt();
			Variable->Set(Value, ECVF_SetByCode);
		}
	}

	~FScopedIntConsoleVariableOverride()
	{
		if (Variable)
		{
			Variable->Set(Previous, ECVF_SetByCode);
		}
	}

private:
	IConsoleVariable* Variable = nullptr;
	int32 Previous = 0;
};

/** A bridge whose roles attest compatible terminal poses: its montages may end inside CounterWindow. */
UPairedAnimationData* CreateTerminalPoseBridgeData()
{
	UPairedAnimationData* Bridge = CreateChainStageData(
		EPairedReactionType::Parry,
		EChainStageTransitionType::OpenCounterWindow,
		TEXT("CounterReady"));
	Bridge->ChainTransitionPolicy.bAttackerTerminalPoseCompatible = true;
	Bridge->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	return Bridge;
}

const FCombatInputRecord* FindInputRecord(const UCombatComponent* Combat, const uint64 Serial)
{
	return Combat->GetCombatInputHistory().FindByPredicate(
		[Serial](const FCombatInputRecord& Record)
		{
			return Record.Serial == Serial;
		});
}

int32 CountDefenseCleanups(
	const UCombatComponent* Combat,
	const FName StageName,
	const FName Reason)
{
	int32 Count = 0;
	for (const FDefenseTelemetryRecord& Record : Combat->GetDefenseTelemetry())
	{
		Count += Record.Event == EDefenseTelemetryEvent::Cleanup
			&& Record.StageName == StageName
			&& Record.CleanupReason == Reason
			? 1 : 0;
	}
	return Count;
}

/** Explicit bridge montage layout for real-instance tests; no value is read from shipped content. */
struct FBridgeMontageTimings
{
	float BridgeEnd = 0.0f;
	float ReadyEnd = 0.0f;
	float BlendOut = 0.0f;
	float MarkerTime = 0.0f;
};

/**
 * Build a playable bridge role montage: section Bridge [0, BridgeEnd) with no authored successor and a
 * CounterReady section [BridgeEnd, ReadyEnd), auto blend-out enabled, and optionally the driver marker.
 */
UAnimMontage* CreateBridgeRoleMontage(
	UAnimSequenceBase* Clip,
	const FBridgeMontageTimings& Timings,
	const bool bDriverMarker)
{
	UAnimMontage* Montage = Clip
		? UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			Clip, TEXT("DefaultSlot"), 0.0f, Timings.BlendOut, 1.0f, 1, -1.0f, 0.0f)
		: nullptr;
	if (!Montage
		|| Montage->SlotAnimTracks.IsEmpty()
		|| Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty()
		|| Clip->GetPlayLength() < Timings.ReadyEnd)
	{
		return nullptr;
	}
	FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
	Segment.StartPos = 0.0f;
	Segment.AnimStartTime = 0.0f;
	Segment.AnimEndTime = Timings.ReadyEnd;
	Segment.AnimPlayRate = 1.0f;
	Segment.LoopingCount = 1;
	Montage->SetCompositeLength(Timings.ReadyEnd);
	Montage->BlendOut.SetBlendTime(Timings.BlendOut);
	Montage->BlendOutTriggerTime = -1.0f;
	Montage->bEnableAutoBlendOut = true;

	Montage->CompositeSections.Reset();
	FCompositeSection BridgeSection;
	BridgeSection.SectionName = TEXT("Bridge");
	BridgeSection.Link(Montage, 0.0f);
	BridgeSection.NextSectionName = NAME_None;
	Montage->CompositeSections.Add(BridgeSection);
	FCompositeSection ReadySection;
	ReadySection.SectionName = TEXT("CounterReady");
	ReadySection.Link(Montage, Timings.BridgeEnd);
	ReadySection.NextSectionName = NAME_None;
	Montage->CompositeSections.Add(ReadySection);

	if (bDriverMarker)
	{
		UAnimNotify_ChainStageTransition* Notify =
			NewObject<UAnimNotify_ChainStageTransition>(Montage);
		Notify->Transition = EChainStageTransitionType::OpenCounterWindow;
		Notify->MarkerName = TEXT("CounterReady");
		FAnimNotifyEvent Event;
		Event.Notify = Notify;
		Event.NotifyName = TEXT("ChainStageTransition");
		Event.Link(Montage, Timings.MarkerTime);
		Montage->Notifies.Add(Event);
	}
	Montage->RefreshCacheData();
	return Montage;
}

/**
 * A bridge whose roles both author CounterReady ready sections, like the shipped parry bridge proof data. The
 * window marker is on DriverRole's montage.
 */
UPairedAnimationData* CreateHeldBridgeData(
	const FBridgeMontageTimings& Timings,
	const EPairedAnimationRole DriverRole = EPairedAnimationRole::Attacker)
{
	UAnimSequenceBase* DefenderClip = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/Assets/Animations/DynamicKatana/AS_Parry_R_Seq.AS_Parry_R_Seq"));
	UAnimSequenceBase* SourceClip = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/Assets/Animations/DynamicKatana/AS_Block_Hit_Break_Seq.AS_Block_Hit_Break_Seq"));
	const bool bDefenderDrives = DriverRole == EPairedAnimationRole::Attacker;
	UAnimMontage* DefenderMontage = CreateBridgeRoleMontage(DefenderClip, Timings, bDefenderDrives);
	UAnimMontage* SourceMontage = CreateBridgeRoleMontage(SourceClip, Timings, !bDefenderDrives);
	if (!DefenderMontage || !SourceMontage)
	{
		return nullptr;
	}
	UPairedAnimationData* Bridge = NewObject<UPairedAnimationData>();
	Bridge->ChainTransitionPolicy.DriverRole = DriverRole;
	Bridge->ReactionType = EPairedReactionType::Parry;
	Bridge->AttackerMontage = DefenderMontage;
	Bridge->AttackerMontageSection = TEXT("Bridge");
	Bridge->VictimMontage = SourceMontage;
	Bridge->VictimMontageSection = TEXT("Bridge");
	Bridge->bIsLethal = false;
	Bridge->BaseDamage = 0.0f;
	Bridge->ChainTransitionPolicy.RequiredMarker = TEXT("CounterReady");
	Bridge->ChainTransitionPolicy.AttackerReadySection = TEXT("CounterReady");
	Bridge->ChainTransitionPolicy.VictimReadySection = TEXT("CounterReady");
	return Bridge;
}

/** A third party's weapon contact on Target, for the production contact resolution path. */
FDefenseContactRequest MakeChainThirdPartyContact(
	ABaseCombatCharacter* Source,
	ABaseCombatCharacter* Target,
	UAttackData* AttackData,
	const int32 TraceGeneration)
{
	Source->WeaponComponent->SetCompatibilityTraceGenerationForTesting(TraceGeneration);
	FDefenseContactRequest Request;
	FWeaponTraceInstanceId TraceId;
	TraceId.WeaponComponent = Source->WeaponComponent.Get();
	TraceId.TraceGeneration = TraceGeneration;
	Request.ContactId = FContactInstanceId::FromCompatibilityTrace(TraceId);
	Request.Query.Stage = EDefenseQueryStage::Contact;
	Request.Query.Attack.AttackData = AttackData;
	Request.Query.Attack.AttackType = AttackData->AttackType;
	Request.Query.Attack.AttackTags = AttackData->AttackTags;
	Request.Query.Attack.AuthoredHeight = AttackData->DefenseProfile.Height;
	Request.Query.Attack.NominalLane = AttackData->DefenseProfile.NominalLane;
	Request.Query.Attack.SwingShape = AttackData->DefenseProfile.SwingShape;
	Request.Query.Attack.AttackerTransform = Source->GetActorTransform();
	Request.Query.Attack.bAttackerAlive = !Source->IsDeadOrDying();
	Request.Query.Attack.bAttackActive = true;
	Request.HitInfo = FCombatTestHelpers::CreateTestHitInfo(
		Source,
		AttackData->BaseDamage,
		(Source->GetActorLocation() - Target->GetActorLocation()).GetSafeNormal(),
		AttackData);
	Request.HitInfo.ImpactPoint = Target->GetActorLocation();
	Request.HitInfo.ImpactNormal = FVector::BackwardVector;
	Request.HitInfo.BoneName = TEXT("spine_03");
	Request.HitInfo.WeaponVelocity =
		(Target->GetActorLocation() - Source->GetActorLocation()).GetSafeNormal() * 1000.0f;
	Request.TraceStart = Source->GetActorLocation();
	Request.TraceEnd = Target->GetActorLocation();
	Request.ActiveSourceSocket = TEXT("weapon_end");
	return Request;
}

/** The first slot of a skeleton slot group other than the default group, or NAME_None if there is none. */
FName FindAlternateGroupSlot(const USkeleton* Skeleton)
{
	if (!Skeleton)
	{
		return NAME_None;
	}
	for (const FAnimSlotGroup& Group : Skeleton->GetSlotGroups())
	{
		if (Group.GroupName != FAnimSlotGroup::DefaultGroupName && !Group.SlotNames.IsEmpty())
		{
			return Group.SlotNames[0];
		}
	}
	return NAME_None;
}

/** A playable single-section stage role montage on SlotName, without root motion or markers. */
UAnimMontage* CreatePlayableStageRoleMontage(
	UAnimSequenceBase* Clip,
	const FName SlotName,
	const FName SectionName,
	const float Length)
{
	UAnimMontage* Montage = Clip
		? UAnimMontage::CreateSlotAnimationAsDynamicMontage(
			Clip, SlotName, 0.0f, 0.1f, 1.0f, 1, -1.0f, 0.0f)
		: nullptr;
	if (!Montage
		|| Montage->SlotAnimTracks.IsEmpty()
		|| Montage->SlotAnimTracks[0].AnimTrack.AnimSegments.IsEmpty()
		|| Clip->GetPlayLength() < Length)
	{
		return nullptr;
	}
	FAnimSegment& Segment = Montage->SlotAnimTracks[0].AnimTrack.AnimSegments[0];
	Segment.StartPos = 0.0f;
	Segment.AnimStartTime = 0.0f;
	Segment.AnimEndTime = Length;
	Segment.AnimPlayRate = 1.0f;
	Segment.LoopingCount = 1;
	Montage->SetCompositeLength(Length);
	Montage->bEnableRootMotionTranslation = false;
	Montage->bEnableRootMotionRotation = false;
	Montage->CompositeSections.Reset();
	FCompositeSection Section;
	Section.SectionName = SectionName;
	Section.Link(Montage, 0.0f);
	Section.NextSectionName = NAME_None;
	Montage->CompositeSections.Add(Section);
	Montage->RefreshCacheData();
	return Montage;
}

/** Counter stage data with playable role montages on SlotName, distinct from any bridge's montages. */
UPairedAnimationData* CreatePlayableCounterData(const FName SlotName, const float Length)
{
	UAnimSequenceBase* DefenderClip = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/Assets/Animations/DynamicKatana/AS_Parry_R_Seq.AS_Parry_R_Seq"));
	UAnimSequenceBase* SourceClip = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/Assets/Animations/DynamicKatana/AS_Block_Hit_Break_Seq.AS_Block_Hit_Break_Seq"));
	UAnimMontage* DefenderMontage =
		CreatePlayableStageRoleMontage(DefenderClip, SlotName, TEXT("Counter"), Length);
	UAnimMontage* SourceMontage =
		CreatePlayableStageRoleMontage(SourceClip, SlotName, TEXT("Counter"), Length);
	if (!DefenderMontage || !SourceMontage)
	{
		return nullptr;
	}
	UPairedAnimationData* Counter = NewObject<UPairedAnimationData>();
	Counter->ReactionType = EPairedReactionType::Counter;
	Counter->AttackerMontage = DefenderMontage;
	Counter->AttackerMontageSection = TEXT("Counter");
	Counter->VictimMontage = SourceMontage;
	Counter->VictimMontageSection = TEXT("Counter");
	Counter->bIsLethal = false;
	Counter->BaseDamage = 0.0f;
	Counter->DamageMultiplier = 1.0f;
	return Counter;
}

/** Finisher stage data with playable, non-lethal role montages on SlotName, distinct from any other stage's. */
UPairedAnimationData* CreatePlayableFinisherData(const FName SlotName, const float Length)
{
	UPairedAnimationData* Finisher = CreatePlayableCounterData(SlotName, Length);
	if (Finisher)
	{
		Finisher->ReactionType = EPairedReactionType::Finisher;
	}
	return Finisher;
}

/** Author one Chain stage-transition marker named MarkerName on Montage at Time. */
void AddChainStageMarker(
	UAnimMontage* Montage,
	const EChainStageTransitionType Transition,
	const FName MarkerName,
	const float Time)
{
	UAnimNotify_ChainStageTransition* Notify = NewObject<UAnimNotify_ChainStageTransition>(Montage);
	Notify->Transition = Transition;
	Notify->MarkerName = MarkerName;
	FAnimNotifyEvent Event;
	Event.Notify = Notify;
	Event.NotifyName = TEXT("ChainStageTransition");
	Event.Link(Montage, Time);
	Montage->Notifies.Add(Event);
	Montage->RefreshCacheData();
}

/**
 * Break Montage the way an authoring error does: a second slot track with the same slot name. Stage preflight
 * still accepts it (its sections, markers and numerics are unchanged), but Montage_Play rejects a montage whose
 * slot setup is invalid, with a warning, before it stops any other montage.
 */
void MakeMontageUnplayable(UAnimMontage* Montage)
{
	if (Montage && !Montage->SlotAnimTracks.IsEmpty())
	{
		const FSlotAnimationTrack DuplicateTrack = Montage->SlotAnimTracks[0];
		Montage->SlotAnimTracks.Add(DuplicateTrack);
	}
}

/** The engine warning Montage_Play logs for a montage that MakeMontageUnplayable broke. */
const TCHAR* const UnplayableMontageWarning = TEXT("is already used in this Montage");

/** A terminal-pose bridge whose source attacker's montage carries the window marker (the source drives it). */
UPairedAnimationData* CreateSourceDrivenTerminalPoseBridgeData()
{
	UPairedAnimationData* Bridge = CreateTerminalPoseBridgeData();
	Bridge->ChainTransitionPolicy.DriverRole = EPairedAnimationRole::Victim;
	Bridge->VictimMontage->SetCompositeLength(1.0f);
	FCompositeSection SourceStageSection;
	SourceStageSection.SectionName = TEXT("Stage");
	SourceStageSection.SetTime(0.0f);
	Bridge->VictimMontage->CompositeSections.Add(SourceStageSection);
	Bridge->VictimMontageSection = SourceStageSection.SectionName;
	UAnimNotify_ChainStageTransition* SourceMarker =
		NewObject<UAnimNotify_ChainStageTransition>(Bridge->VictimMontage);
	SourceMarker->Transition = EChainStageTransitionType::OpenCounterWindow;
	SourceMarker->MarkerName = Bridge->ChainTransitionPolicy.RequiredMarker;
	FAnimNotifyEvent SourceMarkerEvent;
	SourceMarkerEvent.Notify = SourceMarker;
	SourceMarkerEvent.SetTime(0.5f);
	Bridge->VictimMontage->Notifies.Add(SourceMarkerEvent);
	return Bridge;
}

/**
 * Hit reaction settings that play Reaction for every direction at both intensities, with no stun, so any
 * landed hit that is allowed a reaction starts one.
 */
UHitReactionSettings* CreateEveryDirectionReactionSettings(UAnimMontage* Reaction)
{
	FHitReactionEntry Entry;
	Entry.ReactionMontage = Reaction;
	Entry.StunDuration = 0.0f;
	FDirectionalReactionSet Set;
	Set.Front = Entry;
	Set.Back = Entry;
	Set.Left = Entry;
	Set.Right = Entry;
	UHitReactionSettings* Settings = NewObject<UHitReactionSettings>();
	Settings->DirectionalReactions.Add(EHitIntensity::Light, Set);
	Settings->DirectionalReactions.Add(EHitIntensity::Heavy, Set);
	return Settings;
}

/** A playable hit reaction montage for the production skeleton. */
UAnimMontage* CreatePlayableReactionMontage()
{
	UAnimSequenceBase* Clip = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/Assets/Animations/DynamicKatana/AS_Block_Hit_Break_Seq.AS_Block_Hit_Break_Seq"));
	return CreatePlayableStageRoleMontage(Clip, FAnimSlotGroup::DefaultSlotName, TEXT("Reaction"), 0.5f);
}

/** A playback-override stage instance id source: each role start gets a fresh positive id. */
TFunction<bool(EPairedAnimationRole, const UPairedAnimationData*, int32&)> MakeStagePlaybackOverride(
	const int32 FirstInstanceId)
{
	return [NextInstanceId = FirstInstanceId](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId) mutable
	{
		(void)Role;
		(void)Data;
		OutInstanceId = ++NextInstanceId;
		return true;
	};
}

/**
 * Reach CounterWindow through a montage-backed bridge on the playback override, then end the bridge
 * montages: the defender is now free and the source attacker held.
 */
bool ReachFreedCounterWindow(FDefenseChainFixture& Fixture, const int32 FirstInstanceId)
{
	Fixture.EnableDefaultMovement();
	Fixture.SetPlaybackOverride(MakeStagePlaybackOverride(FirstInstanceId));
	UPairedAnimationData* Bridge = CreateTerminalPoseBridgeData();
	if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(Bridge))
	{
		return false;
	}
	Fixture.DeliverBridgeMarker(Bridge);
	if (Fixture.GetChainState() != EChainCounterState::CounterWindow)
	{
		return false;
	}
	Fixture.EndBridgeMontagesNaturally(Bridge);
	return Fixture.GetChainState() == EChainCounterState::CounterWindow;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseSyncPreservesAlignmentOwnership,
	"KatanaCombat.Defense.Chain.SyncPreservesAlignmentOwnership",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDefenseSyncPreservesAlignmentOwnership::RunTest(const FString&)
{
	FDefenseChainFixture Fixture;
	if (!TestTrue(TEXT("Defense fixture initialized"), Fixture.Initialize())
		|| !TestTrue(TEXT("Public Block input commits the retained sequence"), Fixture.StartCommittedParry()))
	{
		Fixture.Destroy(); return false;
	}
	auto* Notify = NewObject<UAnimNotifyState_PairedAnimationSync>();
	Notify->bApplyDamage = false; Notify->bLogMisalignment = false;
	for (const bool bDefenderFirst : {true, false})
	{
		Fixture.Defender->SetActorLocation(FVector::ZeroVector);
		Fixture.SourceAttacker->SetActorLocation(FVector(140, 0, 0));
		for (const bool bDefender : {bDefenderFirst, !bDefenderFirst})
		{
			Notify->NotifyBegin(bDefender ? Fixture.Defender->GetMesh() : Fixture.SourceAttacker->GetMesh(),
				nullptr, .08f, FAnimNotifyEventReference());
		}
		TestTrue(TEXT("Notify cannot bypass defender alignment ownership"), Fixture.Defender->GetActorLocation().Equals(FVector::ZeroVector));
		TestTrue(TEXT("Notify cannot bypass source alignment ownership"), Fixture.SourceAttacker->GetActorLocation().Equals(FVector(140, 0, 0)));
		TestEqual(TEXT("Retained sequence survives presentation-only sync"), Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	}
	Fixture.Destroy(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainContextLeaseOwnershipTest,
	"KatanaCombat.Defense.Chain.ContextLeaseOwnership",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainContextLeaseOwnershipTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Character = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	const FGameplayTag Tag = KatanaCombatGameplayTags::ContextParryCounter();

	if (!TestNotNull(TEXT("Character exists"), Character)
		|| !TestNotNull(TEXT("Combat component exists"), Combat)
		|| !TestTrue(TEXT("Canonical context tag exists"), Tag.IsValid()))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	const FCombatContextLeaseHandle First = Combat->AcquireContextTagLease(Tag, TEXT("DefenseSequenceA"));
	const FCombatContextLeaseHandle Second = Combat->AcquireContextTagLease(Tag, TEXT("DefenseSequenceB"));
	TestTrue(TEXT("First owner receives a valid handle"), First.IsValid());
	TestTrue(TEXT("Second owner receives a valid handle"), Second.IsValid());
	TestTrue(TEXT("Tag is active with both owners"), Combat->HasActiveContextTag(Tag));

	Combat->ReleaseContextTagLease(First);
	TestTrue(TEXT("One owner cannot remove another owner's tag contribution"), Combat->HasActiveContextTag(Tag));
	Combat->ReleaseContextTagLease(First);
	TestTrue(TEXT("Duplicate release is side-effect free"), Combat->HasActiveContextTag(Tag));
	Combat->ReleaseContextTagLease(Second);
	TestFalse(TEXT("Last release removes the tag"), Combat->HasActiveContextTag(Tag));

	const FCombatContextLeaseHandle InvalidTag = Combat->AcquireContextTagLease({}, TEXT("Invalid"));
	const FCombatContextLeaseHandle InvalidOwner = Combat->AcquireContextTagLease(Tag, NAME_None);
	TestFalse(TEXT("Invalid tag fails closed"), InvalidTag.IsValid());
	TestFalse(TEXT("Unnamed owner fails closed"), InvalidOwner.IsValid());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainAuthoringContractTest,
	"KatanaCombat.Defense.Chain.AuthoringContract",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainAuthoringContractTest::RunTest(const FString& Parameters)
{
	UPairedAnimationData* Data = NewObject<UPairedAnimationData>();
	TestNotNull(TEXT("Paired data can own Chain policy"), Data);
	TestEqual(TEXT("Driver defaults to initiating montage role"),
		Data->ChainTransitionPolicy.DriverRole, EPairedAnimationRole::Attacker);
	TestFalse(TEXT("Unreviewed default does not claim a retainable pose"),
		Data->ChainTransitionPolicy.HasRetainableReadyPose());

	Data->ChainTransitionPolicy.AttackerReadySection = TEXT("CounterReady");
	Data->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	TestTrue(TEXT("Each role can satisfy readiness by section or reviewed terminal pose"),
		Data->ChainTransitionPolicy.HasRetainableReadyPose());
	TestTrue(TEXT("FinisherActive is distinct from FinisherReady"),
		EChainCounterState::FinisherActive != EChainCounterState::FinisherReady);

	UPairedAnimationData* InvalidReadySection = CreateChainStageData(
		EPairedReactionType::Parry,
		EChainStageTransitionType::OpenCounterWindow,
		TEXT("CounterReady"));
	InvalidReadySection->ChainTransitionPolicy.AttackerReadySection = TEXT("MissingReadySection");
	InvalidReadySection->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	TestFalse(TEXT("A nonexistent ready section cannot satisfy the authoring contract"),
		InvalidReadySection->IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCollisionLeaseIdentityTest,
	"KatanaCombat.Defense.Chain.CollisionLeaseIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCollisionLeaseIdentityTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* Owner = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* Partner = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	UPairedAnimationComponent* Paired = Owner ? Owner->PairedAnimationComponent.Get() : nullptr;
	if (!TestNotNull(TEXT("Owner paired component exists"), Paired)
		|| !TestNotNull(TEXT("Partner exists"), Partner))
	{
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	Paired->AddPairedPartner(Partner);
	FAnimNotifyRuntimeSourceId FirstSource;
	FirstSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/CollisionMontage"));
	FirstSource.NotifyEventIndex = 2;
	FAnimNotifyRuntimeSourceId SecondSource = FirstSource;
	SecondSource.NotifyEventIndex = 3;
	const EMovementMode BaselineMovementMode =
		Owner->GetCharacterMovement()->MovementMode.GetValue();

	TestTrue(TEXT("First exact notify window acquires state"), Paired->BeginPairedCollisionNotify(
		FirstSource, 41, true, true, false, true, false, 150.0f));
	TestTrue(TEXT("Overlapping exact notify window acquires independent state"), Paired->BeginPairedCollisionNotify(
		SecondSource, 41, true, true, false, true, false, 150.0f));
	TestEqual(TEXT("Both windows are independently owned"), Paired->GetActivePairedStateLeaseCount(), 2);
	TestEqual(TEXT("Movement is disabled while either owner is active"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	TestTrue(TEXT("Tracked partner collision is ignored"),
		Owner->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Partner));

	Paired->EndPairedCollisionNotify(FirstSource, 999);
	TestEqual(TEXT("Stale montage End cannot release a live window"),
		Paired->GetActivePairedStateLeaseCount(), 2);
	Paired->EndPairedCollisionNotify(FirstSource, 41);
	TestEqual(TEXT("Exact End releases only its own window"),
		Paired->GetActivePairedStateLeaseCount(), 1);
	Paired->EndPairedAnimation();
	TestEqual(TEXT("Legacy paired cleanup cannot restore movement owned by another lease"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	TestEqual(TEXT("Overlap keeps movement disabled"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	Paired->EndPairedCollisionNotify(SecondSource, 41);
	TestEqual(TEXT("Last exact End releases all notify-owned state"),
		Paired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Last release restores the captured movement mode"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), BaselineMovementMode);
	TestFalse(TEXT("Last release restores partner collision"),
		Owner->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Partner));

	FAnimNotifyRuntimeSourceId BaselineSource = FirstSource;
	BaselineSource.NotifyEventIndex = 4;
	Owner->GetCapsuleComponent()->IgnoreActorWhenMoving(Partner, true);
	TestTrue(TEXT("A new lease captures a pre-existing move-ignore baseline"),
		Paired->BeginPairedCollisionNotify(
			BaselineSource, 42, true, true, false, true, false, 150.0f));
	Paired->EndPairedCollisionNotify(BaselineSource, 42);
	TestTrue(TEXT("Lease release preserves a pre-existing move-ignore relationship"),
		Owner->GetCapsuleComponent()->GetMoveIgnoreActors().Contains(Partner));
	Owner->GetCapsuleComponent()->IgnoreActorWhenMoving(Partner, false);

	FAnimNotifyRuntimeSourceId CollisionOnlySource = FirstSource;
	CollisionOnlySource.NotifyEventIndex = 5;
	TestTrue(TEXT("Collision-only lease acquires partner-ignore ownership"),
		Paired->BeginPairedCollisionNotify(
			CollisionOnlySource, 43, true, true, false, false, true, 150.0f));
	Owner->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	Owner->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Owner->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	Paired->TickPairedCollisionNotify(CollisionOnlySource, 43);
	TestEqual(TEXT("Collision-only recompute cannot rewrite unowned movement"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Falling);
	TestEqual(TEXT("Collision-only recompute cannot rewrite unowned capsule mode"),
		Owner->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Tracked-partner recompute cannot rewrite unowned pawn response"),
		Owner->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);
	Paired->EndPairedCollisionNotify(CollisionOnlySource, 43);
	TestEqual(TEXT("Collision-only release preserves external movement state"),
		Owner->GetCharacterMovement()->MovementMode.GetValue(), MOVE_Falling);
	TestEqual(TEXT("Collision-only release preserves external capsule state"),
		Owner->GetCapsuleComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("Collision-only release preserves external pawn response"),
		Owner->GetCapsuleComponent()->GetCollisionResponseToChannel(ECC_Pawn), ECR_Overlap);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSequenceOwnershipTimeoutTest,
	"KatanaCombat.Defense.Chain.SequenceOwnershipTimeout",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSequenceOwnershipTimeoutTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create defense Chain fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DefenseConfig->CounterWindowSeconds = 0.01f;
	UEnemyCombatAIComponent* SourceAI = Fixture.SourceAttacker->CombatAIComponent.Get();
	if (!SourceAI)
	{
		AddError(TEXT("Defense Chain fixture source has no combat AI component"));
		Fixture.Destroy();
		return false;
	}
	FEnemyAttackConfig AttackConfig;
	AttackConfig.AttackData = Fixture.SourceAttack;
	AttackConfig.MinRange = 0.0f;
	AttackConfig.MaxRange = 1000.0f;
	SourceAI->AvailableAttacks = {AttackConfig};
	SourceAI->SetCombatTarget(Fixture.Defender);
	TestTrue(TEXT("Source AI is attack-capable before retained Chain ownership"),
		SourceAI->CanAttemptAttack());
	if (!Fixture.StartCommittedParry())
	{
		AddError(TEXT("Failed to start committed perfect parry"));
		Fixture.Destroy();
		return false;
	}
	AActor* DefenderUnrelatedPartner = Fixture.World->SpawnActor<AActor>();
	AActor* SourceUnrelatedPartner = Fixture.World->SpawnActor<AActor>();
	Fixture.Paired->AddPairedPartner(DefenderUnrelatedPartner);
	Fixture.SourcePaired->AddPairedPartner(SourceUnrelatedPartner);

	const FDefenseSequenceContext& Parry = Fixture.Paired->GetActiveDefenseSequenceContext();
	TestEqual(TEXT("Committed sequence starts in ParryActive"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	TestTrue(TEXT("Sequence retains the defender"), Parry.Defender.Get() == Fixture.Defender);
	TestTrue(TEXT("Sequence retains the source attacker"),
		Parry.SourceAttacker.Get() == Fixture.SourceAttacker);
	TestEqual(TEXT("Sequence retains the immutable committed interaction"),
		Parry.OriginatingInteraction,
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting().InteractionId);
	TestTrue(TEXT("Retained Chain owns source AI attack suppression"),
		SourceAI->IsDefenseChainSuppressed());
	TestFalse(TEXT("Suppressed source AI cannot request another attack"),
		SourceAI->CanAttemptAttack());
	FDefenseInteractionId WrongInteraction = Parry.OriginatingInteraction;
	++WrongInteraction.Epoch;
	TestFalse(TEXT("A different interaction cannot release Chain AI suppression"),
		SourceAI->ReleaseDefenseChainSuppression(WrongInteraction));
	TestTrue(TEXT("Mismatched release leaves the exact Chain suppression active"),
		SourceAI->IsDefenseChainSuppressed());
	TestTrue(TEXT("Sequence owns input before a response window"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Sequence owns Context.ParryCounter"), Fixture.DefenderCombat->HasActiveContextTag(
		KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("No-montage fallback still links the source participant"),
		Fixture.Paired->GetPairedPartnerCount(), 2);
	TestEqual(TEXT("No-montage fallback links the defender on the source"),
		Fixture.SourcePaired->GetPairedPartnerCount(), 2);
	FAnimNotifyRuntimeSourceId NotifySource;
	NotifySource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/TerminalCollision"));
	NotifySource.NotifyEventIndex = 7;
	TestTrue(TEXT("A compatibility notify can overlap canonical sequence ownership"),
		Fixture.Paired->BeginPairedCollisionNotify(
			NotifySource, 77, true, true, false, true, false, 150.0f));

	TestTrue(TEXT("Simulation bridge reaches CounterWindow"), Fixture.OpenCounterWindow());
	const FDefenseSequenceContext& Waiting = Fixture.Paired->GetActiveDefenseSequenceContext();
	TestTrue(TEXT("CounterWindow owns an unscaled response deadline"),
		Waiting.ResponseDeadlineUnscaled > FPlatformTime::Seconds());
	FTSTicker::GetCoreTicker().Tick(0.02f);

	TestEqual(TEXT("Unscaled deadline performs terminal cleanup"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Timeout releases retained interaction"),
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction.IsValid());
	TestFalse(TEXT("Terminal cleanup releases source AI suppression"),
		SourceAI->IsDefenseChainSuppressed());
	TestTrue(TEXT("Parry stagger remains active after retained Chain cleanup"),
		Fixture.SourceAttacker->HitReactionComponent->IsStaggered());
	TestFalse(TEXT("Released Chain suppression does not bypass active parry stagger"),
		SourceAI->CanAttemptAttack());
	Fixture.SourceAttacker->HitReactionComponent->EndStagger();
	TestTrue(TEXT("Source AI can attack again after Chain and stagger cleanup"),
		SourceAI->CanAttemptAttack());
	TestFalse(TEXT("Timeout releases input ownership"), Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("Timeout releases the context tag"), Fixture.DefenderCombat->HasActiveContextTag(
		KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("Timeout removes only the sequence's defender registration"),
		Fixture.Paired->GetPairedPartnerCount(), 1);
	TestTrue(TEXT("Timeout preserves unrelated defender-side partner ownership"),
		Fixture.Paired->IsPairedPartner(DefenderUnrelatedPartner));
	TestEqual(TEXT("Timeout removes only the sequence's source registration"),
		Fixture.SourcePaired->GetPairedPartnerCount(), 1);
	TestTrue(TEXT("Timeout preserves unrelated source-side partner ownership"),
		Fixture.SourcePaired->IsPairedPartner(SourceUnrelatedPartner));
	TestEqual(TEXT("Terminal cleanup releases surviving notify-owned state"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	Fixture.Paired->EndPairedCollisionNotify(NotifySource, 77);
	TestEqual(TEXT("A stale notify End after terminal cleanup is harmless"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestTrue(TEXT("Terminal cleanup preserves held guard"), Fixture.DefenderCombat->IsBlocking());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainNoMontageBystanderTargetLifecycleTest,
	"KatanaCombat.Defense.Chain.NoMontageBystanderTargetLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainNoMontageBystanderTargetLifecycleTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create no-montage defense Chain fixture"));
		Fixture.Destroy();
		return false;
	}

	AEnemyCharacter* DefenderBystander = FCombatTestHelpers::CreateTestEnemyCharacter(
		Fixture.World, FVector(350.0f, 100.0f, 0.0f));
	AEnemyCharacter* SourceBystander = FCombatTestHelpers::CreateTestEnemyCharacter(
		Fixture.World, FVector(350.0f, -100.0f, 0.0f));
	AEnemyCharacter* FreshSelector = FCombatTestHelpers::CreateTestEnemyCharacter(
		Fixture.World, FVector(450.0f, 0.0f, 0.0f));
	UEnemyCombatAIComponent* DefenderBystanderAI = DefenderBystander
		? DefenderBystander->GetCombatAIComponent()
		: nullptr;
	UEnemyCombatAIComponent* SourceBystanderAI = SourceBystander
		? SourceBystander->GetCombatAIComponent()
		: nullptr;
	UEnemyCombatAIComponent* FreshSelectorAI = FreshSelector
		? FreshSelector->GetCombatAIComponent()
		: nullptr;
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UCombatTokenSubsystem* TokenSubsystem = NewObject<UCombatTokenSubsystem>(GameInstance);
	UAttackData* BystanderAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	if (!DefenderBystanderAI || !SourceBystanderAI || !FreshSelectorAI
		|| !TokenSubsystem || !BystanderAttack)
	{
		AddError(TEXT("Failed to create no-montage bystander AI fixture"));
		Fixture.Destroy();
		return false;
	}

	TokenSubsystem->MaxConcurrentAttackers = 2;
	TokenSubsystem->TokenCooldownPerEnemy = 0.0f;
	FEnemyAttackConfig AttackConfig;
	AttackConfig.AttackData = BystanderAttack;
	AttackConfig.MinRange = 0.0f;
	AttackConfig.MaxRange = 1000.0f;
	for (UEnemyCombatAIComponent* CombatAI : {DefenderBystanderAI, SourceBystanderAI, FreshSelectorAI})
	{
		CombatAI->SetTokenSubsystemForTesting(TokenSubsystem);
		CombatAI->AvailableAttacks = {AttackConfig};
	}
	DefenderBystanderAI->SetCombatTarget(Fixture.Defender);
	SourceBystanderAI->SetCombatTarget(Fixture.SourceAttacker);
	TestTrue(TEXT("Defender bystander owns a token before sequence takeover"),
		DefenderBystanderAI->TryInitiateAttack());
	TestTrue(TEXT("Source bystander owns a token before sequence takeover"),
		SourceBystanderAI->TryInitiateAttack());

	TestTrue(TEXT("Committed perfect parry enters the production no-montage bridge"),
		Fixture.StartCommittedParry());
	TestFalse(TEXT("No-montage takeover immediately releases the defender bystander's token"),
		DefenderBystanderAI->HasAttackToken());
	TestFalse(TEXT("No-montage takeover immediately releases the source bystander's token"),
		SourceBystanderAI->HasAttackToken());
	TestFalse(TEXT("Retained defender is unavailable to normal AI attacks"),
		DefenderBystanderAI->CanAttemptAttack());
	TestFalse(TEXT("Retained source is unavailable to normal AI attacks"),
		SourceBystanderAI->CanAttemptAttack());

	FreshSelectorAI->SetCombatTarget(Fixture.Defender);
	TestNull(TEXT("A no-montage defense participant cannot be selected as a fresh target"),
		FreshSelectorAI->CombatTarget.Get());

	Fixture.Paired->CancelPairedAnimation(0.0f);
	TestEqual(TEXT("Temporary defense ownership retains the defender target for resume"),
		DefenderBystanderAI->CombatTarget.Get(), static_cast<AActor*>(Fixture.Defender));
	TestEqual(TEXT("Temporary defense ownership retains the source target for resume"),
		SourceBystanderAI->CombatTarget.Get(), static_cast<AActor*>(Fixture.SourceAttacker));
	TestTrue(TEXT("Defender bystander reacquires after cleanup without target reset"),
		DefenderBystanderAI->TryInitiateAttack());
	TestTrue(TEXT("Source bystander reacquires after cleanup without target reset"),
		SourceBystanderAI->TryInitiateAttack());

	TokenSubsystem->ResetAllTokens();
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainMarkerIdentityTest,
	"KatanaCombat.Defense.Chain.MarkerIdentity",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainMarkerIdentityTest::RunTest(const FString& Parameters)
{
	FScopedActionReactionTelemetry TelemetryEnabled(1);
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
	{
		AddError(TEXT("Failed to create marker identity fixture"));
		Fixture.Destroy();
		return false;
	}

	Fixture.Paired->CancelDefenseAsyncHandle(
		Fixture.Paired->ActiveDefenseSequence.BridgeFallbackHandle);
	Fixture.Paired->ActiveDefenseSequence.BridgeFallbackHandle = {};
	UPairedAnimationData* BridgeData = CreateChainStageData(
		EPairedReactionType::Parry,
		EChainStageTransitionType::OpenCounterWindow,
		TEXT("CounterReady"));
	BridgeData->ChainTransitionPolicy.bAttackerTerminalPoseCompatible = true;
	BridgeData->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	Fixture.Paired->ActiveDefenseSequence.ActivePairedData = BridgeData;
	Fixture.Paired->ActiveDefenseSequence.AttackerMontageInstanceId = 101;
	Fixture.Paired->ActiveDefenseSequence.VictimMontageInstanceId = 202;
	Fixture.Paired->ActivePairedAnimData = BridgeData;
	Fixture.Paired->ActivePairedReactionType = EPairedReactionType::Parry;
	const FAnimNotifyRuntimeSourceId Source = MakeMarkerSource(BridgeData->AttackerMontage);
	Fixture.DefenderCombat->ClearActionReactionTelemetry();

	Fixture.SourcePaired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 202, Source);
	TestEqual(TEXT("Partner role cannot drive the bridge marker"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 999, Source);
	TestEqual(TEXT("Stale montage instance cannot drive the marker"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow,
		101,
		MakeMarkerSource(BridgeData->AttackerMontage, 3));
	TestEqual(TEXT("Wrong runtime notify source cannot drive the marker"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	AActor* UnrelatedReporter = Fixture.World->SpawnActor<AActor>();
	Fixture.Paired->HandleChainStageTransitionFromActor(
		UnrelatedReporter,
		EChainStageTransitionType::OpenCounterWindow,
		101,
		Source);
	TestEqual(TEXT("An unrelated reporter cannot drive the marker"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);

	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 101, Source);
	TestEqual(TEXT("Exact driver marker opens CounterWindow"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterWindow);
	TestEqual(TEXT("One exact marker creates one response deadline"),
		Fixture.Paired->DefenseResponseTickers.Num(), 1);
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow, 101, Source);
	TestEqual(TEXT("Duplicate marker cannot create another deadline"),
		Fixture.Paired->DefenseResponseTickers.Num(), 1);
	TestTrue(TEXT("Marker handoff retains paired presentation ownership"),
		Fixture.Paired->IsPairedAnimationActive());

	auto CountTelemetry = [&Fixture](
		const EActionReactionTelemetryEvent Event,
		const EActionReactionTelemetryReason Reason)
	{
		return CountActionReactionTelemetry(
			Fixture.DefenderCombat->GetActionReactionTelemetry(), Event, Reason);
	};
	TestEqual(TEXT("Partner marker rejection records its role mismatch"),
		CountTelemetry(
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerDriverRoleMismatch),
		1);
	TestEqual(TEXT("Stale marker rejection records its montage instance mismatch"),
		CountTelemetry(
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerMontageInstanceMismatch),
		1);
	TestEqual(TEXT("Invalid marker address records its notify index failure"),
		CountTelemetry(
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerNotifyIndexInvalid),
		1);
	TestEqual(TEXT("Unrelated marker reporter records its participant mismatch"),
		CountTelemetry(
			EActionReactionTelemetryEvent::PairedStageMarkerRejected,
			EActionReactionTelemetryReason::MarkerReporterMismatch),
		1);
	TestEqual(TEXT("Exact driver marker records one accepted transition"),
		CountTelemetry(
			EActionReactionTelemetryEvent::PairedStageMarkerAccepted,
			EActionReactionTelemetryReason::MarkerAccepted),
		1);
	const FActionReactionTelemetryRecord* AcceptedDriverMarker =
		Fixture.DefenderCombat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::PairedStageMarkerAccepted
					&& Record.Reason == EActionReactionTelemetryReason::MarkerAccepted;
			});
	if (AcceptedDriverMarker)
	{
		TestEqual(TEXT("Marker telemetry keeps the defending owner as actor"),
			AcceptedDriverMarker->Actor.Get(), static_cast<AActor*>(Fixture.Defender));
		TestEqual(TEXT("Marker telemetry keeps the source attacker as counterpart"),
			AcceptedDriverMarker->Counterpart.Get(), static_cast<AActor*>(Fixture.SourceAttacker));
		TestTrue(TEXT("Marker telemetry identifies its reporting participant"),
			AcceptedDriverMarker->Detail.Contains(Fixture.Defender->GetPathName()));
	}

	Fixture.Destroy();

	FDefenseChainFixture VictimDriver;
	if (!VictimDriver.Initialize() || !VictimDriver.StartCommittedParry())
	{
		AddError(TEXT("Failed to create victim-driver marker fixture"));
		VictimDriver.Destroy();
		return false;
	}
	VictimDriver.Paired->CancelDefenseAsyncHandle(
		VictimDriver.Paired->ActiveDefenseSequence.BridgeFallbackHandle);
	VictimDriver.Paired->ActiveDefenseSequence.BridgeFallbackHandle = {};
	UPairedAnimationData* VictimBridgeData = CreateChainStageData(
		EPairedReactionType::Parry,
		EChainStageTransitionType::OpenCounterWindow,
		TEXT("VictimCounterReady"));
	VictimBridgeData->ChainTransitionPolicy.DriverRole = EPairedAnimationRole::Victim;
	VictimBridgeData->ChainTransitionPolicy.bAttackerTerminalPoseCompatible = true;
	VictimBridgeData->ChainTransitionPolicy.bVictimTerminalPoseCompatible = true;
	VictimBridgeData->VictimMontage->Notifies = MoveTemp(
		VictimBridgeData->AttackerMontage->Notifies);
	VictimBridgeData->VictimMontage->SetCompositeLength(1.0f);
	VictimBridgeData->VictimMontage->CompositeSections = MoveTemp(
		VictimBridgeData->AttackerMontage->CompositeSections);
	VictimBridgeData->VictimMontageSection = VictimBridgeData->AttackerMontageSection;
	VictimBridgeData->AttackerMontageSection = NAME_None;
	VictimBridgeData->VictimMontage->Notifies[0].SetTime(0.5f);
	VictimDriver.SourceAttacker->SetActorLocationAndRotation(
		FVector(175.0f, 0.0f, 0.0f),
		FRotator(0.0f, 180.0f, 0.0f));
	VictimDriver.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 303 : 404;
		return Data != nullptr;
	});
	FString VictimPreflightFailure;
	VictimBridgeData->AttackerBlendIn = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("Bridge preflight rejects a nonfinite playback value"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Bridge numeric refusal reports an actionable reason"),
		VictimPreflightFailure.Contains(TEXT("numeric")));
	VictimBridgeData->AttackerBlendIn = 0.1f;
	VictimBridgeData->VictimWarpConfig.bWarpRotation = false;
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects a role without rotation warping"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(*FString::Printf(
		TEXT("Rotation-warp refusal reports the canonical role contract: %s"),
		*VictimPreflightFailure),
		VictimPreflightFailure.Contains(TEXT("rotation warp")));
	VictimBridgeData->VictimWarpConfig.bWarpRotation = true;
	VictimDriver.SetPendingRoleMontageCallback(
		VictimBridgeData->VictimMontage,
		EPairedAnimationRole::Victim,
		true);
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects an unresolved prior role callback"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Pending-callback refusal reports the ownership ambiguity"),
		VictimPreflightFailure.Contains(TEXT("unresolved prior callback")));
	VictimDriver.SetPendingRoleMontageCallback(
		VictimBridgeData->VictimMontage,
		EPairedAnimationRole::Victim,
		false);
	const FAnimNotifyEvent DuplicateDriverMarker =
		VictimBridgeData->VictimMontage->Notifies[0];
	VictimBridgeData->VictimMontage->Notifies.Add(DuplicateDriverMarker);
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects duplicate matching driver markers"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Duplicate marker refusal reports the marker contract"),
		VictimPreflightFailure.Contains(TEXT("one reviewed Chain marker")));
	VictimBridgeData->VictimMontage->Notifies.RemoveAt(1);
	VictimBridgeData->VictimMontage->SetCompositeLength(1.5f);
	FCompositeSection BridgeSection;
	BridgeSection.SectionName = TEXT("Bridge");
	BridgeSection.SetTime(0.0f);
	VictimBridgeData->VictimMontage->CompositeSections.Add(BridgeSection);
	FCompositeSection ReadySection;
	ReadySection.SectionName = TEXT("Ready");
	ReadySection.SetTime(1.0f);
	VictimBridgeData->VictimMontage->CompositeSections.Add(ReadySection);
	VictimBridgeData->VictimMontageSection = TEXT("Bridge");
	VictimBridgeData->VictimMontage->Notifies[0].SetTime(1.0f);
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects a driver marker at the played section boundary"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Boundary-marker refusal reports the playable-section contract"),
		VictimPreflightFailure.Contains(TEXT("played section")));
	VictimBridgeData->VictimMontageSection = NAME_None;
	VictimBridgeData->VictimMontage->CompositeSections.Reset();
	VictimBridgeData->VictimMontage->Notifies[0].SetTime(0.0f);
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects sectionless time-zero markers"),
		VictimDriver.PreflightBridge(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			VictimPreflightFailure));
	TestTrue(TEXT("Sectionless marker refusal reports the playable-section contract"),
		VictimPreflightFailure.Contains(TEXT("played section")));

	VictimBridgeData->VictimMontage->CompositeSections.Add(BridgeSection);
	VictimBridgeData->VictimMontage->CompositeSections.Add(ReadySection);
	VictimBridgeData->VictimMontageSection = TEXT("Bridge");
	VictimBridgeData->VictimMontage->Notifies[0].SetTime(0.5f);
	VictimPreflightFailure.Reset();
	FDefenseResolution AlreadyAligned =
		VictimDriver.DefenderCombat->GetLastInputDefenseResolutionForTesting();
	AlreadyAligned.Decision.MeasuredYawDegrees = 5.0f;
	AlreadyAligned.Decision.RequiredFinalTolerance = 10.0f;
	AlreadyAligned.Decision.AvailableTurnDegrees = 0.0f;
	UDefenseConfiguration* SourceDefenseConfig = NewObject<UDefenseConfiguration>();
	SourceDefenseConfig->MaximumAutomaticTurn = 70.0f;
	SourceDefenseConfig->DefenseTurnRate = 180.0f;
	VictimDriver.SourceCombat->DefenseConfigurationOverride = SourceDefenseConfig;

	FDefenseResolution ContactDeadlineLimited = AlreadyAligned;
	ContactDeadlineLimited.PredictedContact.bIsValid = true;
	ContactDeadlineLimited.PredictedContact.ContactSimulationTime =
		VictimDriver.World->GetTimeSeconds() + 0.02;
	VictimDriver.SourceAttacker->SetActorRotation(FRotator(0.0f, 110.0f, 0.0f));
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects source rotation that cannot finish before contact"),
		VictimDriver.PreflightBridgeWithResolution(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			ContactDeadlineLimited,
			VictimPreflightFailure));
	TestTrue(TEXT("Contact-deadline refusal reports the rotation budget"),
		VictimPreflightFailure.Contains(TEXT("rotation budget")));

	FDefenseResolution MarkerDeadlineLimited = AlreadyAligned;
	MarkerDeadlineLimited.PredictedContact.bIsValid = true;
	MarkerDeadlineLimited.PredictedContact.ContactSimulationTime =
		VictimDriver.World->GetTimeSeconds() + 1.0;
	VictimBridgeData->VictimMontage->Notifies[0].SetTime(0.05f);
	VictimDriver.SourceAttacker->SetActorRotation(FRotator(0.0f, 150.0f, 0.0f));
	float ResolvedMarkerOffset = 0.0f;
	TestTrue(TEXT("Marker deadline fixture resolves the exact played-section offset"),
		UAnimNotify_ChainStageTransition::TryGetSinglePlayableMarkerOffset(
			VictimBridgeData->VictimMontage,
			TEXT("VictimCounterReady"),
			EChainStageTransitionType::OpenCounterWindow,
			VictimBridgeData->VictimMontageSection,
			ResolvedMarkerOffset));
	TestEqual(TEXT("Marker deadline fixture uses a 50 ms section-relative marker"),
		ResolvedMarkerOffset, 0.05f, 0.001f);
	TestEqual(TEXT("Marker deadline fixture montage rate is unscaled"),
		VictimBridgeData->VictimMontage->RateScale, 1.0f, 0.001f);
	TestTrue(TEXT("Marker deadline fixture source dilation is a finite simulation rate"),
		FMath::IsFinite(VictimDriver.SourceAttacker->CustomTimeDilation)
			&& VictimDriver.SourceAttacker->CustomTimeDilation > 0.0f);
	const float SourceYawDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(
		VictimDriver.SourceAttacker->GetActorRotation().Yaw,
		(VictimDriver.Defender->GetActorLocation()
			- VictimDriver.SourceAttacker->GetActorLocation()).Rotation().Yaw));
	TestEqual(TEXT("Marker deadline fixture requires thirty degrees of source yaw"),
		SourceYawDelta, 30.0f, 0.001f);
	VictimPreflightFailure.Reset();
	TestFalse(TEXT("Bridge preflight rejects source rotation that cannot finish before its marker"),
		VictimDriver.PreflightBridgeWithResolution(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			MarkerDeadlineLimited,
			VictimPreflightFailure));
	TestTrue(TEXT("Marker-deadline refusal reports the rotation budget"),
		VictimPreflightFailure.Contains(TEXT("rotation budget")));

	VictimBridgeData->VictimMontage->Notifies[0].SetTime(0.5f);
	VictimDriver.SourceAttacker->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	AlreadyAligned.PredictedContact.ContactSimulationTime =
		VictimDriver.World->GetTimeSeconds() + 0.20;
	TestTrue(TEXT("A bridge inside final tolerance requires no invented turn budget"),
		VictimDriver.PreflightBridgeWithResolution(
			VictimBridgeData,
			TEXT("VictimCounterReady"),
			AlreadyAligned,
			VictimPreflightFailure));
	VictimPreflightFailure.Reset();
	const bool bVictimPreflightPassed = VictimDriver.PreflightBridge(
		VictimBridgeData,
		TEXT("VictimCounterReady"),
		VictimPreflightFailure);
	TestTrue(*FString::Printf(
		TEXT("Bridge preflight accepts the explicitly authored victim driver role: %s"),
		*VictimPreflightFailure),
		bVictimPreflightPassed);
	VictimDriver.Paired->ActiveDefenseSequence.ActivePairedData = VictimBridgeData;
	VictimDriver.Paired->ActiveDefenseSequence.AttackerMontageInstanceId = 303;
	VictimDriver.Paired->ActiveDefenseSequence.VictimMontageInstanceId = 404;
	VictimDriver.Paired->ActivePairedAnimData = VictimBridgeData;
	VictimDriver.Paired->ActivePairedReactionType = EPairedReactionType::Parry;
	VictimDriver.DefenderCombat->ClearActionReactionTelemetry();
	VictimDriver.SourcePaired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow,
		404,
		MakeMarkerSource(VictimBridgeData->VictimMontage));
	TestEqual(TEXT("The authored victim driver marker opens CounterWindow"),
		VictimDriver.Paired->GetChainState(), EChainCounterState::CounterWindow);
	const FActionReactionTelemetryRecord* VictimMarker =
		VictimDriver.DefenderCombat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::PairedStageMarkerAccepted;
			});
	TestNotNull(TEXT("Victim-driven marker should retain its accepted observation"), VictimMarker);
	if (VictimMarker)
	{
		TestEqual(TEXT("Victim-driven marker identifies the emitting victim montage"),
			VictimMarker->MontagePath, FSoftObjectPath(VictimBridgeData->VictimMontage));
		TestEqual(TEXT("Victim-driven marker keeps the source attacker as counterpart"),
			VictimMarker->Counterpart.Get(), static_cast<AActor*>(VictimDriver.SourceAttacker));
		TestTrue(TEXT("Victim-driven marker identifies its reporting participant"),
			VictimMarker->Detail.Contains(VictimDriver.SourceAttacker->GetPathName()));
	}
	VictimDriver.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainPartialStartRollbackTest,
	"KatanaCombat.Defense.Chain.PartialStartRollback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainPartialStartRollbackTest::RunTest(const FString& Parameters)
{
	FScopedActionReactionTelemetry TelemetryEnabled(1);
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create partial-start fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	Fixture.CounterAttack->CounterData = CounterData;
	const int32 OriginalGeneration =
		Fixture.Paired->GetActiveDefenseSequenceContext().StageGeneration;
	const double OriginalDeadline =
		Fixture.Paired->GetActiveDefenseSequenceContext().ResponseDeadlineUnscaled;
	Fixture.Paired->DefenseStagePlaybackOverrideForTesting = [](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 301 : INDEX_NONE;
		return Role == EPairedAnimationRole::Attacker;
	};
	Fixture.DefenderCombat->ClearActionReactionTelemetry();

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	const FDefenseSequenceContext& RolledBack =
		Fixture.Paired->GetActiveDefenseSequenceContext();
	TestEqual(TEXT("One-role failure keeps CounterWindow retryable"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterWindow);
	TestTrue(TEXT("Rollback retires the attempted generation"),
		RolledBack.StageGeneration > OriginalGeneration);
	TestEqual(TEXT("Rollback preserves the original real-time deadline"),
		RolledBack.ResponseDeadlineUnscaled, OriginalDeadline, 0.001);
	TestFalse(TEXT("Rollback does not claim an active paired stage"),
		Fixture.Paired->IsPairedAnimationActive());
	TestEqual(TEXT("Rollback releases successor defender collision"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Rollback releases successor source collision"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);
	TestFalse(TEXT("Rollback restores source hit-reaction ownership"),
		Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
	TestFalse(TEXT("Rollback keeps the defender free, as the open window left it"),
		Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Rollback retains sequence context ownership"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	const FCombatInputRecord& Record = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("Failed stage input remains ChainOnly"), Record.Route, ECombatInputRoute::ChainOnly);
	TestEqual(TEXT("Failed stage input expires"), Record.Disposition, ECombatInputDisposition::Expired);
	TestTrue(TEXT("Stale failed-stage end callback is consumed"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(CounterData->AttackerMontage, true));
	TestEqual(TEXT("Stale failed-stage callback cannot clean the response window"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterWindow);
	const int32 PlaybackFailures = CountActionReactionTelemetry(
		Fixture.DefenderCombat->GetActionReactionTelemetry(),
		EActionReactionTelemetryEvent::PairedStageStartFailed,
		EActionReactionTelemetryReason::StagePlaybackFailed);
	TestEqual(TEXT("Partial paired start records one playback failure"), PlaybackFailures, 1);

	Fixture.Destroy();
	return true;
}

namespace
{
/** A Light attack that sets its own push, so the chain-release tests do not rest on the shipped Light default. */
UAttackData* CreateChainReleasePushingAttack()
{
	UAttackData* Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	Attack->Knockback.bOverrideDistance = true;
	Attack->Knockback.Distance = 30.0f;
	Attack->Knockback.bOverrideDuration = true;
	Attack->Knockback.Duration = 0.3f;
	return Attack;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainStageStartReleasesDefenderPushTest,
	"KatanaCombat.Defense.Chain.StageStartReleasesDefenderPush",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainStageStartReleasesDefenderPushTest::RunTest(const FString& Parameters)
{
	FScopedActionReactionTelemetry TelemetryEnabled(1);
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create the defender-push fixture"));
		Fixture.Destroy();
		return false;
	}
	UHitReactionComponent* DefenderReaction = Fixture.Defender->HitReactionComponent;
	UTargetingComponent* DefenderTargeting = Fixture.Defender->TargetingComponent;
	if (!TestNotNull(TEXT("Defender hit reaction"), DefenderReaction)
		|| !TestNotNull(TEXT("Defender targeting"), DefenderTargeting))
	{
		Fixture.Destroy();
		return false;
	}

	// A hit from the source attacker pushes the defender, who starts the counter stage while the push still runs.
	const FHitReactionInfo Hit = FCombatTestHelpers::CreateTestHitInfo(
		Fixture.SourceAttacker,
		10.0f,
		FVector(1.0f, 0.0f, 0.0f),
		CreateChainReleasePushingAttack());
	if (!TestTrue(TEXT("The defender is pushed"), DefenderReaction->StartKnockback(Hit)))
	{
		Fixture.Destroy();
		return false;
	}
	const FAlignmentRequestHandle Push = DefenderReaction->KnockbackAlignmentHandle;
	FAlignmentRequestSpec PushSpec;
	TestTrue(TEXT("The defender's targeting holds the push"),
		DefenderTargeting->GetAlignmentRequestSpec(Push, PushSpec)
		&& PushSpec.Executor == EAlignmentExecutor::ProceduralDisplacement);

	Fixture.CounterAttack->CounterData = CreateChainStageData(EPairedReactionType::Counter);
	int32 NextInstanceId = 700;
	Fixture.SetPlaybackOverride([&NextInstanceId](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return true;
	});
	Fixture.DefenderCombat->ClearActionReactionTelemetry();
	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);

	TestEqual(TEXT("The counter stage started"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	FAlignmentRequestSpec Remaining;
	TestFalse(TEXT("The stage start released the defender's push"),
		DefenderTargeting->GetAlignmentRequestSpec(Push, Remaining));
	TestTrue(TEXT("The defender's stage alignment is the active request"),
		DefenderTargeting->GetActiveAlignmentRequest()
			== Fixture.Paired->GetActiveDefenseSequenceContext().AttackerAlignmentLease);
	// Released by the stage start itself, with its reason.
	const TArray<FActionReactionTelemetryRecord> Cancelled =
		Fixture.DefenderCombat->GetActionReactionTelemetry().FilterByPredicate(
			[](const FActionReactionTelemetryRecord& Row)
			{
				return Row.AlignmentOwner == FName(TEXT("HitKnockback"))
					&& Row.AlignmentDisposition == FName(TEXT("Cancelled"));
			});
	if (TestEqual(TEXT("The released push writes one Cancelled row"), Cancelled.Num(), 1))
	{
		TestEqual(TEXT("Released by the chain stage start"),
			Cancelled[0].Detail, FString(TEXT("ChainStart")));
	}

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSequenceBeginReleasesDefenderPushTest,
	"KatanaCombat.Defense.Chain.SequenceBeginReleasesDefenderPush",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSequenceBeginReleasesDefenderPushTest::RunTest(const FString& Parameters)
{
	FScopedActionReactionTelemetry TelemetryEnabled(1);
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create the defender-push fixture"));
		Fixture.Destroy();
		return false;
	}
	UHitReactionComponent* DefenderReaction = Fixture.Defender->HitReactionComponent;
	UTargetingComponent* DefenderTargeting = Fixture.Defender->TargetingComponent;
	if (!TestNotNull(TEXT("Defender hit reaction"), DefenderReaction)
		|| !TestNotNull(TEXT("Defender targeting"), DefenderTargeting))
	{
		Fixture.Destroy();
		return false;
	}

	// A hit from the source attacker pushes the defender, who then perfect-parries while the push still runs.
	const FHitReactionInfo Hit = FCombatTestHelpers::CreateTestHitInfo(
		Fixture.SourceAttacker,
		10.0f,
		FVector(1.0f, 0.0f, 0.0f),
		CreateChainReleasePushingAttack());
	if (!TestTrue(TEXT("The defender is pushed"), DefenderReaction->StartKnockback(Hit)))
	{
		Fixture.Destroy();
		return false;
	}
	const FAlignmentRequestHandle Push = DefenderReaction->KnockbackAlignmentHandle;
	FAlignmentRequestSpec PushSpec;
	TestTrue(TEXT("The defender's targeting holds the push"),
		DefenderTargeting->GetAlignmentRequestSpec(Push, PushSpec)
		&& PushSpec.Executor == EAlignmentExecutor::ProceduralDisplacement);

	Fixture.DefenderCombat->ClearActionReactionTelemetry();
	if (!TestTrue(TEXT("Public Block input commits the retained sequence"), Fixture.StartCommittedParry()))
	{
		Fixture.Destroy();
		return false;
	}
	// The fixture authors no paired bridge, so the sequence takes the no-montage parry bridge: no stage starts.
	TestFalse(TEXT("No paired stage started (no-montage parry bridge)"),
		Fixture.Paired->IsPairedAnimationActive());
	FAlignmentRequestSpec Remaining;
	TestFalse(TEXT("The sequence start released the defender's push"),
		DefenderTargeting->GetAlignmentRequestSpec(Push, Remaining));
	// Released by the sequence start itself, with the chain's reason.
	const TArray<FActionReactionTelemetryRecord> Cancelled =
		Fixture.DefenderCombat->GetActionReactionTelemetry().FilterByPredicate(
			[](const FActionReactionTelemetryRecord& Row)
			{
				return Row.AlignmentOwner == FName(TEXT("HitKnockback"))
					&& Row.AlignmentDisposition == FName(TEXT("Cancelled"));
			});
	if (TestEqual(TEXT("The released push writes one Cancelled row"), Cancelled.Num(), 1))
	{
		TestEqual(TEXT("Released by the parry sequence start"),
			Cancelled[0].Detail, FString(TEXT("ChainStart")));
	}

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainRetainedStageLifecycleTest,
	"KatanaCombat.Defense.Chain.RetainedStageLifecycle",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainRetainedStageLifecycleTest::RunTest(const FString& Parameters)
{
	FScopedActionReactionTelemetry TelemetryEnabled(1);
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create retained-stage fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(
		EPairedReactionType::Counter,
		EChainStageTransitionType::AutoContinue,
		TEXT("CounterImpact"),
		true);
	UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
	FinisherData->AttackerWarpConfig.WarpTargetName = TEXT("DefenseFinisherAttacker");
	FinisherData->VictimWarpConfig.WarpTargetName = TEXT("DefenseFinisherVictim");
	CounterData->bApplySlowMotion = true;
	CounterData->SlowMotionScale = 0.5f;
	FinisherData->bApplySlowMotion = true;
	FinisherData->SlowMotionScale = 0.3f;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.CounterAttack->FinisherData = FinisherData;
	int32 NextInstanceId = 500;
	Fixture.Paired->DefenseStagePlaybackOverrideForTesting = [&NextInstanceId](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return true;
	};

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("ChainOnly attack starts CounterActive"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	TestEqual(TEXT("Counter stage owns one defender collision lease"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 1);
	TestEqual(TEXT("Counter stage owns one source collision lease"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 1);
	TestTrue(TEXT("Counter stage retains sequence input ownership"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Counter stage retains the context tag"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	FAlignmentRequestSpec CounterDefenderAlignment;
	FAlignmentRequestSpec CounterSourceAlignment;
	const FDefenseSequenceContext& CounterSequence =
		Fixture.Paired->GetActiveDefenseSequenceContext();
	TestTrue(TEXT("Counter stage retains defender alignment ownership"),
		Fixture.Defender->TargetingComponent->GetAlignmentRequestSpec(
			CounterSequence.AttackerAlignmentLease,
			CounterDefenderAlignment));
	TestTrue(TEXT("Counter stage retains source alignment ownership"),
		Fixture.SourceAttacker->TargetingComponent->GetAlignmentRequestSpec(
			CounterSequence.VictimAlignmentLease,
			CounterSourceAlignment));
	UPairedAnimationData* ReusedRoleMontage =
		CreateChainStageData(EPairedReactionType::Finisher);
	ReusedRoleMontage->AttackerMontage = CounterData->AttackerMontage;
	FString ReusedMontageFailure;
	TestFalse(TEXT("An adjacent stage cannot reuse a same-role montage without instance identity"),
		Fixture.PreflightStage(
			ReusedRoleMontage,
			EPairedReactionType::Finisher,
			ReusedMontageFailure));
	TestTrue(TEXT("Reused-montage refusal reports an actionable reason"),
		ReusedMontageFailure.Contains(TEXT("distinct role montages")));
	const UDefenseConfiguration* SourceDefenseConfig =
		Fixture.SourceCombat->GetEffectiveDefenseConfiguration();
	TestEqual(TEXT("Counter defender alignment uses the effective defense turn rate"),
		CounterDefenderAlignment.MaximumTurnRate,
		Fixture.DefenseConfig->DefenseTurnRate);
	TestEqual(TEXT("Counter source alignment uses the effective defense turn rate"),
		CounterSourceAlignment.MaximumTurnRate,
		SourceDefenseConfig ? SourceDefenseConfig->DefenseTurnRate : 0.0f);
	constexpr float SpentDefenderBudget = 25.0f;
	constexpr float SpentSourceBudget = 30.0f;
	CounterDefenderAlignment.RemainingTurnBudget = SpentDefenderBudget;
	CounterSourceAlignment.RemainingTurnBudget = SpentSourceBudget;
	TestTrue(TEXT("Fixture can account for defender yaw already spent by the stage"),
		Fixture.Defender->TargetingComponent->UpdateAlignmentRequest(
			CounterSequence.AttackerAlignmentLease,
			CounterDefenderAlignment));
	TestTrue(TEXT("Fixture can account for source yaw already spent by the stage"),
		Fixture.SourceAttacker->TargetingComponent->UpdateAlignmentRequest(
			CounterSequence.VictimAlignmentLease,
			CounterSourceAlignment));
	UCombatEffectsWorldSubsystem* Effects =
		Fixture.World->GetSubsystem<UCombatEffectsWorldSubsystem>();
	const FTimeDilationLeaseHandle CounterTimeLease = CounterSequence.TimeDilationLease;
	TestTrue(TEXT("Counter stage owns its exact world-time lease"),
		Effects && Effects->IsLeaseActive(CounterTimeLease));
	TestEqual(TEXT("Counter stage applies its requested time scale"),
		Fixture.World->GetWorldSettings()->TimeDilation, 0.5f);
	const FCombatInputRecord& CounterInput = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("Successful response uses ChainOnly route"),
		CounterInput.Route, ECombatInputRoute::ChainOnly);
	TestEqual(TEXT("Successful response consumes its edge"),
		CounterInput.Disposition, ECombatInputDisposition::Consumed);
	const int32 ClearQueueCallsBeforeSuccessor =
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting();

	const float HealthBeforeCounter = Fixture.SourceAttacker->CurrentHealth;
	const int32 CounterStageGeneration =
		Fixture.Paired->GetActiveDefenseSequenceContext().StageGeneration;
	const int32 CounterMontageId =
		Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId;
	Fixture.DefenderCombat->ClearActionReactionTelemetry();
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::AutoContinue,
		CounterMontageId,
		MakeMarkerSource(CounterData->AttackerMontage));
	TestEqual(TEXT("Exact auto marker starts FinisherActive without exposing None"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);
	TestEqual(TEXT("Counter damage commits once before the successor"),
		Fixture.SourceAttacker->CurrentHealth,
		HealthBeforeCounter - CounterData->BaseDamage,
		KINDA_SMALL_NUMBER);
	const FActionReactionTelemetryRecord* AcceptedMarker =
		Fixture.DefenderCombat->GetActionReactionTelemetry().FindByPredicate(
			[](const FActionReactionTelemetryRecord& Record)
			{
				return Record.Event == EActionReactionTelemetryEvent::PairedStageMarkerAccepted
					&& Record.Reason == EActionReactionTelemetryReason::MarkerAccepted;
			});
	TestNotNull(TEXT("Auto-continue should retain its accepted marker observation"), AcceptedMarker);
	if (AcceptedMarker)
	{
		TestEqual(TEXT("Marker observation retains the outgoing counter generation"),
			AcceptedMarker->PrimaryActionGeneration, CounterStageGeneration);
		TestEqual(TEXT("Marker observation retains the outgoing counter state"),
			AcceptedMarker->ReactionClass, FName(TEXT("CounterActive")));
		TestEqual(TEXT("Marker observation identifies the emitting counter montage"),
			AcceptedMarker->MontagePath, FSoftObjectPath(CounterData->AttackerMontage));
	}
	TestEqual(TEXT("Successor replaces, rather than overlaps, defender stage lease"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 1);
	TestEqual(TEXT("Successor replaces, rather than overlaps, source stage lease"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 1);
	TestTrue(TEXT("Successor retains input ownership"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("Successor retains context ownership"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("Successor does not clear the action queue"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		ClearQueueCallsBeforeSuccessor);
	const FDefenseSequenceContext& FinisherSequence =
		Fixture.Paired->GetActiveDefenseSequenceContext();
	FAlignmentRequestSpec FinisherDefenderAlignment;
	FAlignmentRequestSpec FinisherSourceAlignment;
	TestTrue(TEXT("Successor retains defender alignment ownership"),
		Fixture.Defender->TargetingComponent->GetAlignmentRequestSpec(
			FinisherSequence.AttackerAlignmentLease,
			FinisherDefenderAlignment));
	TestTrue(TEXT("Successor retains source alignment ownership"),
		Fixture.SourceAttacker->TargetingComponent->GetAlignmentRequestSpec(
			FinisherSequence.VictimAlignmentLease,
			FinisherSourceAlignment));
	TestEqual(TEXT("Successor cannot raise the defender's configured turn rate"),
		FinisherDefenderAlignment.MaximumTurnRate,
		Fixture.DefenseConfig->DefenseTurnRate);
	TestEqual(TEXT("Successor cannot raise the source's configured turn rate"),
		FinisherSourceAlignment.MaximumTurnRate,
		SourceDefenseConfig ? SourceDefenseConfig->DefenseTurnRate : 0.0f);
	TestEqual(TEXT("A new defender warp target inherits the remaining yaw budget"),
		FinisherDefenderAlignment.RemainingTurnBudget,
		SpentDefenderBudget);
	TestEqual(TEXT("A new source warp target inherits the remaining yaw budget"),
		FinisherSourceAlignment.RemainingTurnBudget,
		SpentSourceBudget);
	const FAlignmentRequestHandle FinalDefenderAlignment =
		FinisherSequence.AttackerAlignmentLease;
	const FAlignmentRequestHandle FinalSourceAlignment =
		FinisherSequence.VictimAlignmentLease;
	const FTimeDilationLeaseHandle FinalTimeLease = FinisherSequence.TimeDilationLease;
	TestTrue(TEXT("Successor owns its exact world-time lease"),
		Effects && Effects->IsLeaseActive(FinalTimeLease));
	TestFalse(TEXT("Successor retires the outgoing world-time lease"),
		Effects && Effects->IsLeaseActive(CounterTimeLease));
	TestEqual(TEXT("Successor applies its own requested time scale"),
		Fixture.World->GetWorldSettings()->TimeDilation, 0.3f);
	TestTrue(TEXT("Outgoing counter end callback is consumed"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(CounterData->AttackerMontage, true));
	TestEqual(TEXT("Outgoing callback cannot clean the finisher"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);

	const float HealthBeforeFinisher = Fixture.SourceAttacker->CurrentHealth;
	TestTrue(TEXT("Active finisher completion is owned by the Chain"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(FinisherData->AttackerMontage, false));
	TestEqual(TEXT("Final completion reaches terminal None"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestEqual(TEXT("Finisher damage commits once"),
		Fixture.SourceAttacker->CurrentHealth,
		HealthBeforeFinisher - FinisherData->BaseDamage,
		KINDA_SMALL_NUMBER);
	TestFalse(TEXT("Terminal completion releases input"), Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("Terminal completion releases context"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	TestEqual(TEXT("Terminal completion releases defender collision"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Terminal completion releases source collision"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);
	TestEqual(TEXT("Terminal completion clears defender partners"),
		Fixture.Paired->GetPairedPartnerCount(), 0);
	TestEqual(TEXT("Terminal completion clears source partners"),
		Fixture.SourcePaired->GetPairedPartnerCount(), 0);
	TestEqual(TEXT("Terminal completion clears the action queue exactly once"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		ClearQueueCallsBeforeSuccessor + 1);
	TestFalse(TEXT("Terminal completion releases its exact world-time ownership"),
		Effects && Effects->IsLeaseActive(FinalTimeLease));
	TestEqual(TEXT("Terminal completion restores world-time baseline"),
		Fixture.World->GetWorldSettings()->TimeDilation, 1.0f);
	FAlignmentRequestSpec ReleasedAlignment;
	TestFalse(TEXT("Terminal completion releases defender alignment ownership"),
		Fixture.Defender->TargetingComponent->GetAlignmentRequestSpec(
			FinalDefenderAlignment,
			ReleasedAlignment));
	TestFalse(TEXT("Terminal completion releases source alignment ownership"),
		Fixture.SourceAttacker->TargetingComponent->GetAlignmentRequestSpec(
			FinalSourceAlignment,
			ReleasedAlignment));
	const float HealthAfterCompletion = Fixture.SourceAttacker->CurrentHealth;
	TestFalse(TEXT("Duplicate terminal callback is no longer owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(FinisherData->AttackerMontage, false));
	TestEqual(TEXT("Duplicate terminal callback cannot replay damage"),
		Fixture.SourceAttacker->CurrentHealth, HealthAfterCompletion);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainDuplicateIntermediateCallbackTest,
	"KatanaCombat.Defense.Chain.DuplicateIntermediateCallback",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainDuplicateIntermediateCallbackTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create duplicate-intermediate-callback fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
	CounterData->bIsLethal = true;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.CounterAttack->FinisherData = FinisherData;
	int32 NextInstanceId = 650;
	Fixture.SetPlaybackOverride([&NextInstanceId, FinisherData](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return Data != FinisherData || Role != EPairedAnimationRole::Victim;
	});

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Counter starts before its normal completion callback"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	Fixture.SourceAttacker->CurrentHealth = 5.0f;
	const int32 QueueClearsBeforeCompletion =
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting();
	TestTrue(TEXT("First counter completion callback is sequence-owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestEqual(TEXT("First counter completion enters FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("Default Chain counter damage cannot kill a low-health source"),
		Fixture.SourceAttacker->CurrentHealth,
		1.0f,
		KINDA_SMALL_NUMBER);
	TestFalse(TEXT("Authored-lethal counter data remains nonlethal without the explicit opt-in"),
		Fixture.SourceAttacker->IsDeadOrDying());

	TestTrue(TEXT("Duplicate counter completion callback remains sequence-owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestEqual(TEXT("Duplicate completion cannot terminate FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("Duplicate completion cannot replay counter damage"),
		Fixture.SourceAttacker->CurrentHealth,
		1.0f,
		KINDA_SMALL_NUMBER);
	TestFalse(TEXT("Counter completion frees the defender while FinisherReady waits"),
		Fixture.Paired->IsInputBlocked());
	TestEqual(TEXT("Duplicate completion cannot clear the action queue"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		QueueClearsBeforeCompletion);

	const int32 GenerationBeforeFailedFinisher =
		Fixture.Paired->GetActiveDefenseSequenceContext().StageGeneration;
	Fixture.DefenderCombat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
	TestEqual(TEXT("A partial finisher start rolls back to FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestNotEqual(TEXT("Rollback advances the stage generation"),
		Fixture.Paired->GetActiveDefenseSequenceContext().StageGeneration,
		GenerationBeforeFailedFinisher);
	TestTrue(TEXT("The stale counter callback remains owned after rollback rekeying"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestEqual(TEXT("The rekeyed stale callback cannot terminate FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("The rekeyed stale callback cannot replay counter damage"),
		Fixture.SourceAttacker->CurrentHealth,
		1.0f,
		KINDA_SMALL_NUMBER);
	TestEqual(TEXT("The rekeyed stale callback cannot clear the action queue"),
		Fixture.DefenderCombat->GetClearQueueCallCountForTesting(),
		QueueClearsBeforeCompletion);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainLethalCounterOptInTest,
	"KatanaCombat.Defense.Chain.LethalCounterOptIn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainLethalCounterOptInTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create lethal-counter fixture"));
		Fixture.Destroy();
		return false;
	}

	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	CounterData->bIsLethal = true;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.EnableLethalCounterData();
	Fixture.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 681 : 682;
		return true;
	});

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Opted-in lethal counter reaches CounterActive"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	Fixture.SourceAttacker->CurrentHealth = 5.0f;
	TestTrue(TEXT("Lethal counter completion callback is sequence-owned"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			CounterData->AttackerMontage, false));
	TestTrue(TEXT("Explicit lethal opt-in permits authored counter data to kill"),
		Fixture.SourceAttacker->IsDeadOrDying());
	TestEqual(TEXT("Death during damage performs terminal Chain cleanup"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Death cleanup releases sequence input ownership"),
		Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("Death cleanup releases sequence context ownership"),
		Fixture.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	TestFalse(TEXT("Death cleanup releases the retained interaction"),
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction.IsValid());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainRetryableFinisherTest,
	"KatanaCombat.Defense.Chain.RetryableFinisher",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainRetryableFinisherTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create retryable-finisher fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(
		EPairedReactionType::Counter,
		EChainStageTransitionType::AutoContinue,
		TEXT("AutoFinisher"),
		true);
	UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.CounterAttack->FinisherData = FinisherData;
	bool bFailFinisherVictim = true;
	int32 NextInstanceId = 700;
	Fixture.Paired->DefenseStagePlaybackOverrideForTesting = [
		&bFailFinisherVictim,
		&NextInstanceId,
		FinisherData](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return !(Data == FinisherData
			&& Role == EPairedAnimationRole::Victim
			&& bFailFinisherVictim);
	};

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	const int32 CounterMontageId =
		Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId;
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::AutoContinue,
		CounterMontageId,
		MakeMarkerSource(CounterData->AttackerMontage));
	TestEqual(TEXT("Retryable partial finisher failure enters FinisherReady"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherReady);
	TestEqual(TEXT("Rollback retains counter defender collision ownership"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 1);
	TestEqual(TEXT("Rollback retains counter source collision ownership"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 1);
	TestTrue(TEXT("FinisherReady retains sequence input ownership"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("FinisherReady owns a fresh real-time deadline"),
		Fixture.Paired->GetActiveDefenseSequenceContext().ResponseDeadlineUnscaled
			> FPlatformTime::Seconds());

	bFailFinisherVictim = false;
	Fixture.DefenderCombat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
	TestEqual(TEXT("A later physical input can retry the finisher"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);
	const FCombatInputRecord& RetryInput = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("Finisher retry remains ChainOnly"),
		RetryInput.Route, ECombatInputRoute::ChainOnly);
	TestEqual(TEXT("Successful finisher retry consumes only its own edge"),
		RetryInput.Disposition, ECombatInputDisposition::Consumed);
	Fixture.Paired->HandleOwnerPairedMontageEnded(FinisherData->AttackerMontage, false);
	TestEqual(TEXT("Retried finisher completes terminally"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);

	Fixture.Destroy();

	FDefenseChainFixture NonRetryable;
	if (!NonRetryable.Initialize()
		|| !NonRetryable.StartCommittedParry()
		|| !NonRetryable.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create non-retryable finisher fixture"));
		NonRetryable.Destroy();
		return false;
	}
	UPairedAnimationData* NonRetryCounter = CreateChainStageData(
		EPairedReactionType::Counter,
		EChainStageTransitionType::AutoContinue,
		TEXT("NonRetryableFinisher"),
		true);
	NonRetryCounter->ChainTransitionPolicy.bFinisherRetryable = false;
	UPairedAnimationData* InvalidFinisher = CreateChainStageData(EPairedReactionType::Finisher);
	NonRetryable.CounterAttack->CounterData = NonRetryCounter;
	NonRetryable.CounterAttack->FinisherData = InvalidFinisher;
	int32 NonRetryInstanceId = 800;
	NonRetryable.SetPlaybackOverride([InvalidFinisher, &NonRetryInstanceId](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NonRetryInstanceId;
		return Data != InvalidFinisher || Role != EPairedAnimationRole::Victim;
	});
	NonRetryable.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	NonRetryable.Paired->HandleChainStageTransition(
		EChainStageTransitionType::AutoContinue,
		NonRetryable.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
		MakeMarkerSource(NonRetryCounter->AttackerMontage));
	TestEqual(TEXT("A non-retryable automatic finisher failure cleans up terminally"),
		NonRetryable.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Non-retryable failure releases input ownership"),
		NonRetryable.Paired->IsInputBlocked());
	TestFalse(TEXT("Non-retryable failure releases context ownership"),
		NonRetryable.DefenderCombat->HasActiveContextTag(
			KatanaCombatGameplayTags::ContextParryCounter()));
	NonRetryable.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainParticipantDeathTest,
	"KatanaCombat.Defense.Chain.ParticipantDeath",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainParticipantDeathTest::RunTest(const FString& Parameters)
{
	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create partner-death fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.SourceAttacker->bIsDying = true;
		Fixture.SourceAttacker->FinalizeDeath();
		TestEqual(TEXT("Source death cancels a no-montage retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Source death releases input"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Source death releases context"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create owner-death fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.Defender->bIsDying = true;
		Fixture.Defender->FinalizeDeath();
		TestEqual(TEXT("Owner death cancels the retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Owner death releases input"), Fixture.Paired->IsInputBlocked());
		TestEqual(TEXT("Owner death clears source partner registration"),
			Fixture.SourcePaired->GetPairedPartnerCount(), 0);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create raw participant-destruction fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.SourceAttacker->Destroy();
		TestEqual(TEXT("Raw source destruction cancels the retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Raw source destruction releases input"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Raw source destruction releases context"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create raw owner-destruction fixture"));
			Fixture.Destroy();
			return false;
		}
		Fixture.Defender->Destroy();
		TestEqual(TEXT("Raw owner destruction cancels the retained sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Raw owner destruction releases input"), Fixture.Paired->IsInputBlocked());
		TestEqual(TEXT("Raw owner destruction clears source partner registration"),
			Fixture.SourcePaired->GetPairedPartnerCount(), 0);
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainStagePreflightGeometryTest,
	"KatanaCombat.Defense.Chain.StagePreflightGeometry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainStagePreflightGeometryTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create stage-preflight fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 901 : 902;
		return true;
	});
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	FString FailureReason;
	UAnimMontage* VictimMontage = CounterData->VictimMontage;
	CounterData->VictimMontage = nullptr;
	TestFalse(TEXT("A successor with a missing role montage fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Montage refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("montage")));
	CounterData->VictimMontage = VictimMontage;
	CounterData->VictimBlendOut = std::numeric_limits<float>::quiet_NaN();
	TestFalse(TEXT("A successor with a nonfinite playback value fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Numeric refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("numeric")));
	CounterData->VictimBlendOut = 0.2f;
	CounterData->BaseDamage = TNumericLimits<float>::Max();
	CounterData->DamageMultiplier = 10.0f;
	TestFalse(TEXT("A successor with an unrepresentable damage product fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Damage overflow refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("numeric")));
	CounterData->BaseDamage = 100.0f;
	CounterData->DamageMultiplier = 1.0f;
	CounterData->AttackerWarpConfig.bWarpRotation = false;
	FailureReason.Reset();
	TestFalse(TEXT("A successor role without rotation warping fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Stage rotation-warp refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("rotation warp")));
	CounterData->AttackerWarpConfig.bWarpRotation = true;
	Fixture.SetPendingRoleMontageCallback(
		CounterData->AttackerMontage,
		EPairedAnimationRole::Attacker,
		true);
	FailureReason.Reset();
	TestFalse(TEXT("A successor with an unresolved prior callback fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Stage callback refusal reports the ownership ambiguity"),
		FailureReason.Contains(TEXT("unresolved prior callback")));
	Fixture.SetPendingRoleMontageCallback(
		CounterData->AttackerMontage,
		EPairedAnimationRole::Attacker,
		false);

	CounterData->MaxTriggerDistance = 200.0f;
	TestFalse(TEXT("A successor outside its maximum trigger range fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Range refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("trigger range")));

	CounterData->MaxTriggerDistance = 300.0f;
	CounterData->VictimWarpConfig.MaxWarpDistance = 100.0f;
	TestFalse(TEXT("A role exceeding its authored warp budget fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Warp refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("translation budget")));

	CounterData->VictimWarpConfig.MaxWarpDistance = 300.0f;
	AActor* Obstacle = Fixture.World->SpawnActor<AActor>();
	UBoxComponent* BlockingBox = Obstacle ? NewObject<UBoxComponent>(Obstacle) : nullptr;
	if (Obstacle && BlockingBox)
	{
		Obstacle->SetRootComponent(BlockingBox);
		Obstacle->AddInstanceComponent(BlockingBox);
		BlockingBox->SetBoxExtent(FVector(25.0f, 100.0f, 100.0f));
		BlockingBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		BlockingBox->SetCollisionResponseToAllChannels(ECR_Ignore);
		BlockingBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		BlockingBox->RegisterComponent();
		Obstacle->SetActorLocation(FVector(175.0f, 0.0f, 0.0f));
	}
	TestFalse(TEXT("A blocked participant or warp sweep fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Sweep refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("blocked")));

	if (BlockingBox)
	{
		BlockingBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	Fixture.SourceAttacker->SetActorLocation(FVector(10.0f, 0.0f, 0.0f));
	FailureReason.Reset();
	TestTrue(TEXT("A retained successor may start below the initial minimum range"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));

	Fixture.SourceAttacker->SetActorLocationAndRotation(
		FVector(250.0f, 0.0f, 0.0f),
		FRotator::ZeroRotator);
	CounterData->AttackerWarpConfig.bWarpTranslation = false;
	CounterData->VictimWarpConfig.bWarpTranslation = false;
	FailureReason.Reset();
	TestFalse(TEXT("A role that cannot face its partner within the remaining yaw budget fails closed"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	TestTrue(TEXT("Rotation refusal reports an actionable reason"),
		FailureReason.Contains(TEXT("rotation budget")));
	CounterData->VictimWarpConfig.FacingPolicy = EPairedFacingPolicy::MatchPartnerHeading;
	TestTrue(TEXT("Matching headings requires no victim half-turn and fits the same retained budget"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	CounterData->AttackerWarpConfig.FacingPolicy = EPairedFacingPolicy::FaceAwayFromPartner;
	TestFalse(TEXT("Facing away still rejects a defender half-turn beyond its budget"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	CounterData->AttackerWarpConfig.FacingPolicy = static_cast<EPairedFacingPolicy>(255);
	TestFalse(TEXT("An unknown role policy cannot enter a retained stage"),
		Fixture.PreflightStage(CounterData, EPairedReactionType::Counter, FailureReason));
	CounterData->AttackerWarpConfig.FacingPolicy = EPairedFacingPolicy::FacePartner;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Policy-aware preflight permits the actual counter transition"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	const FDefenseSequenceContext& Sequence = Fixture.Paired->GetActiveDefenseSequenceContext();
	FAlignmentRequestSpec DefenderSpec, VictimSpec;
	TestTrue(TEXT("Defender stage owns its alignment request"), Fixture.Defender->TargetingComponent->GetAlignmentRequestSpec(Sequence.AttackerAlignmentLease, DefenderSpec));
	TestTrue(TEXT("Victim stage owns its alignment request"), Fixture.SourceAttacker->TargetingComponent->GetAlignmentRequestSpec(Sequence.VictimAlignmentLease, VictimSpec));
	TestEqual(TEXT("Stage carries defender policy into alignment"), DefenderSpec.FacingPolicy, EPairedFacingPolicy::FacePartner);
	TestEqual(TEXT("Stage carries victim policy into alignment"), VictimSpec.FacingPolicy, EPairedFacingPolicy::MatchPartnerHeading);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSourceMontageOwnershipTest,
	"KatanaCombat.Defense.Chain.SourceMontageOwnership",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSourceMontageOwnershipTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create source-montage fixture"));
		Fixture.Destroy();
		return false;
	}
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 911 : 912;
		return true;
	});

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Counter stage starts before the source callback"),
		Fixture.Paired->GetChainState(), EChainCounterState::CounterActive);
	TestTrue(TEXT("The source component forwards its victim-role callback to the sequence owner"),
		Fixture.SourcePaired->HandleOwnerPairedMontageEnded(
			CounterData->VictimMontage, true));
	TestEqual(TEXT("An interrupted source montage performs terminal sequence cleanup"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);
	TestFalse(TEXT("Source interruption releases sequence input"), Fixture.Paired->IsInputBlocked());
	TestEqual(TEXT("Source interruption releases source collision ownership"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);

	Fixture.Destroy();

	FDefenseChainFixture NormalCompletion;
	if (!NormalCompletion.Initialize()
		|| !NormalCompletion.StartCommittedParry()
		|| !NormalCompletion.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create source normal-completion fixture"));
		NormalCompletion.Destroy();
		return false;
	}
	UPairedAnimationData* NormalCounterData = CreateChainStageData(EPairedReactionType::Counter);
	NormalCompletion.CounterAttack->CounterData = NormalCounterData;
	NormalCompletion.SetPlaybackOverride([](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = Role == EPairedAnimationRole::Attacker ? 921 : 922;
		return true;
	});
	NormalCompletion.DefenderCombat->OnInputEvent(
		EInputType::LightAttack,
		EInputEventType::Press);
	const float HealthBefore = NormalCompletion.SourceAttacker->CurrentHealth;
	TestTrue(TEXT("A normal source-role end is deferred for same-frame owner ordering"),
		NormalCompletion.SourcePaired->HandleOwnerPairedMontageEnded(
			NormalCounterData->VictimMontage, false));
	TestEqual(TEXT("The source callback alone does not cancel the active counter"),
		NormalCompletion.Paired->GetChainState(), EChainCounterState::CounterActive);
	TestTrue(TEXT("The authoritative owner callback completes the counter"),
		NormalCompletion.Paired->HandleOwnerPairedMontageEnded(
			NormalCounterData->AttackerMontage, false));
	TestTrue(TEXT("Same-frame callback ordering still applies counter damage"),
		NormalCompletion.SourceAttacker->CurrentHealth < HealthBefore);
	FTSTicker::GetCoreTicker().Tick(0.0f);
	TestEqual(TEXT("Deferred source verification cannot reopen or recancel completion"),
		NormalCompletion.Paired->GetChainState(), EChainCounterState::None);
	NormalCompletion.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainLethalFinisherBlendOutTest,
	"KatanaCombat.Defense.Chain.LethalFinisherBlendOut",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainLethalFinisherBlendOutTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to create lethal-finisher blend-out fixture"));
		Fixture.Destroy();
		return false;
	}

	UPairedAnimationData* CounterData = CreateChainStageData(
		EPairedReactionType::Counter,
		EChainStageTransitionType::AutoContinue,
		TEXT("FinisherReady"),
		true);
	UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
	FinisherData->bIsLethal = true;
	FinisherData->BaseDamage = 100.0f;
	Fixture.CounterAttack->CounterData = CounterData;
	Fixture.CounterAttack->FinisherData = FinisherData;
	Fixture.SetPlaybackOverride([NextInstanceId = 1000](
		const EPairedAnimationRole Role,
		const UPairedAnimationData*,
		int32& OutInstanceId) mutable
	{
		OutInstanceId = ++NextInstanceId + (Role == EPairedAnimationRole::Victim ? 100 : 0);
		return true;
	});

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	const int32 CounterMontageId =
		Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId;
	Fixture.Paired->HandleChainStageTransition(
		EChainStageTransitionType::AutoContinue,
		CounterMontageId,
		MakeMarkerSource(CounterData->AttackerMontage));
	TestEqual(TEXT("Reviewed transition starts the lethal finisher"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);

	TestTrue(TEXT("Victim-role blend-out routes to the canonical sequence owner"),
		Fixture.SourcePaired->HandleOwnerPairedMontageBlendingOut(
			FinisherData->VictimMontage, false));
	TestTrue(TEXT("Lethal finisher damage enters the victim death lifecycle"),
		Fixture.SourceAttacker->IsDeadOrDying());
	Fixture.Paired->OnPairedPartnerDeath(Fixture.SourceAttacker);
	TestEqual(TEXT("Expected finisher death does not cancel the active sequence"),
		Fixture.Paired->GetChainState(), EChainCounterState::FinisherActive);
	const float HealthAfterBlendOut = Fixture.SourceAttacker->CurrentHealth;
	TestTrue(TEXT("Duplicate participant blend-out remains owned"),
		Fixture.Paired->HandleOwnerPairedMontageBlendingOut(
			FinisherData->AttackerMontage, false));
	TestEqual(TEXT("Duplicate participant blend-out cannot replay lethal damage"),
		Fixture.SourceAttacker->CurrentHealth, HealthAfterBlendOut);
	TestTrue(TEXT("Owner montage end completes the expected lethal finisher"),
		Fixture.Paired->HandleOwnerPairedMontageEnded(
			FinisherData->AttackerMontage, false));
	TestEqual(TEXT("Lethal finisher reaches terminal chain state"),
		Fixture.Paired->GetChainState(), EChainCounterState::None);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCleanupListenerReentryTest,
	"KatanaCombat.Defense.Chain.CleanupListenerReentry",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCleanupListenerReentryTest::RunTest(const FString& Parameters)
{
	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create listener-action fixture"));
			Fixture.Destroy();
			return false;
		}
		UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
		Recorder->bBeginBlockOnPairedEnded = true;
		Recorder->PairedActionTarget = Fixture.Defender;
		Fixture.Paired->OnPairedAnimationEnded.AddDynamic(
			Recorder, &UCombatEventRecorder::HandlePairedAnimationEnded);
		Fixture.DefenderCombat->EndBlock();
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Terminal cleanup broadcasts exactly once"),
			Recorder->PairedAnimationEndedCount, 1);
		TestTrue(TEXT("A listener may start a new action after cleanup released ownership"),
			Recorder->bBeginBlockOnPairedEndedResult);
		TestTrue(TEXT("The listener-started block remains active after broadcast returns"),
			Fixture.DefenderCombat->IsBlocking());
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create listener-destruction fixture"));
			Fixture.Destroy();
			return false;
		}
		UCombatEventRecorder* Recorder = NewObject<UCombatEventRecorder>();
		Recorder->bDestroyOnPairedEnded = true;
		Recorder->ActorToDestroy = Fixture.Defender;
		Fixture.Paired->OnPairedAnimationEnded.AddDynamic(
			Recorder, &UCombatEventRecorder::HandlePairedAnimationEnded);
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Destructive listener still observes one terminal event"),
			Recorder->PairedAnimationEndedCount, 1);
		TestTrue(TEXT("Destroying the sequence owner from the terminal listener is safe"),
			Fixture.Defender->IsActorBeingDestroyed());
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCompetingPairedStartRejectedTest,
	"KatanaCombat.Defense.Chain.CompetingPairedStartRejected",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCompetingPairedStartRejectedTest::RunTest(const FString& Parameters)
{
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
	{
		AddError(TEXT("Failed to create competing-paired-start fixture"));
		Fixture.Destroy();
		return false;
	}

	AEnemyCharacter* CompetingTarget = FCombatTestHelpers::CreateTestEnemyCharacter(
		Fixture.World,
		FVector(0.0f, 250.0f, 0.0f));
	UPairedAnimationData* CompetingData = CreateChainStageData(
		EPairedReactionType::Finisher);
	const FDefenseInteractionId InteractionBefore =
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction;
	TestFalse(TEXT("A generic paired start cannot replace a retained defense sequence"),
		Fixture.TryCompetingPairedStart(
			CompetingTarget,
			CompetingData,
			EPairedReactionType::Finisher));
	TestEqual(TEXT("Rejected competing start preserves the active Chain state"),
		Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
	TestTrue(TEXT("Rejected competing start preserves the exact interaction"),
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction
			== InteractionBefore);
	TestTrue(TEXT("Rejected competing start preserves the source partner"),
		Fixture.Paired->IsPairedPartner(Fixture.SourceAttacker));
	TestFalse(TEXT("Rejected competing start does not register another target"),
		Fixture.Paired->IsPairedPartner(CompetingTarget));

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCancellationMatrixTest,
	"KatanaCombat.Defense.Chain.CancellationMatrix",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCancellationMatrixTest::RunTest(const FString& Parameters)
{
	auto AssertTerminalCancellation = [this](
		FDefenseChainFixture& Fixture,
		const EChainCounterState ExpectedState)
	{
		TestEqual(TEXT("Fixture reached the requested cancellable state"),
			Fixture.Paired->GetChainState(), ExpectedState);
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Cancellation reaches terminal None"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Cancellation releases retained interaction"),
			Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction.IsValid());
		TestFalse(TEXT("Cancellation releases input ownership"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Cancellation releases context ownership"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		TestEqual(TEXT("Cancellation releases defender stage leases"),
			Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
		TestEqual(TEXT("Cancellation releases source stage leases"),
			Fixture.SourcePaired->GetActivePairedStateLeaseCount(), 0);
		Fixture.Paired->CancelPairedAnimation(0.0f);
		TestEqual(TEXT("Duplicate cancellation is idempotent"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
	};

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create ParryActive cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		AssertTerminalCancellation(Fixture, EChainCounterState::ParryActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create CounterWindow cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		AssertTerminalCancellation(Fixture, EChainCounterState::CounterWindow);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create CounterActive cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
		Fixture.CounterAttack->CounterData = CounterData;
		Fixture.SetPlaybackOverride([](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId)
		{
			OutInstanceId = Role == EPairedAnimationRole::Attacker ? 921 : 922;
			return true;
		});
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		AssertTerminalCancellation(Fixture, EChainCounterState::CounterActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create FinisherReady cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* CounterData = CreateChainStageData(
			EPairedReactionType::Counter,
			EChainStageTransitionType::AutoContinue,
			TEXT("ReadyCancel"),
			true);
		UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
		Fixture.CounterAttack->CounterData = CounterData;
		Fixture.CounterAttack->FinisherData = FinisherData;
		Fixture.SetPlaybackOverride([FinisherData](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId)
		{
			OutInstanceId = Role == EPairedAnimationRole::Attacker ? 931 : 932;
			return Data != FinisherData || Role != EPairedAnimationRole::Victim;
		});
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		Fixture.Paired->HandleChainStageTransition(
			EChainStageTransitionType::AutoContinue,
			Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
			MakeMarkerSource(CounterData->AttackerMontage));
		AssertTerminalCancellation(Fixture, EChainCounterState::FinisherReady);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			AddError(TEXT("Failed to create FinisherActive cancellation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* CounterData = CreateChainStageData(
			EPairedReactionType::Counter,
			EChainStageTransitionType::AutoContinue,
			TEXT("ActiveCancel"),
			true);
		UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
		Fixture.CounterAttack->CounterData = CounterData;
		Fixture.CounterAttack->FinisherData = FinisherData;
		int32 NextInstanceId = 940;
		Fixture.SetPlaybackOverride([&NextInstanceId](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId)
		{
			OutInstanceId = ++NextInstanceId;
			return true;
		});
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		Fixture.Paired->HandleChainStageTransition(
			EChainStageTransitionType::AutoContinue,
			Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
			MakeMarkerSource(CounterData->AttackerMontage));
		AssertTerminalCancellation(Fixture, EChainCounterState::FinisherActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry())
		{
			AddError(TEXT("Failed to create legacy API isolation fixture"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* UnrelatedData = CreateChainStageData(
			EPairedReactionType::Finisher);
		Fixture.Paired->BeginPairedAnimation(
			UnrelatedData,
			EPairedReactionType::Finisher,
			true);
		TestEqual(TEXT("Legacy begin cannot replace an active defense sequence"),
			Fixture.Paired->GetChainState(), EChainCounterState::ParryActive);
		TestNull(TEXT("Legacy begin cannot install unrelated paired data"),
			Fixture.Paired->ActivePairedAnimData.Get());
		Fixture.Paired->EndPairedAnimation();
		TestEqual(TEXT("Legacy end routes active defense ownership through terminal cleanup"),
			Fixture.Paired->GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Legacy end releases defense input ownership"),
			Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("Legacy end releases defense context ownership"),
			Fixture.DefenderCombat->HasActiveContextTag(
				KatanaCombatGameplayTags::ContextParryCounter()));
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSharedNotifyAcrossActorsTest,
	"KatanaCombat.Defense.Chain.SharedNotifyAcrossActors",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSharedNotifyAcrossActorsTest::RunTest(const FString& Parameters)
{
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	APlayerCharacter* First = FCombatTestHelpers::CreateTestPlayerCharacter(World);
	AEnemyCharacter* FirstPartner = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 0.0f, 0.0f));
	APlayerCharacter* Second = FCombatTestHelpers::CreateTestPlayerCharacter(
		World, FVector(0.0f, 500.0f, 0.0f));
	AEnemyCharacter* SecondPartner = FCombatTestHelpers::CreateTestEnemyCharacter(
		World, FVector(100.0f, 500.0f, 0.0f));
	UPairedAnimationComponent* FirstPaired = First ? First->PairedAnimationComponent.Get() : nullptr;
	UPairedAnimationComponent* SecondPaired = Second ? Second->PairedAnimationComponent.Get() : nullptr;
	if (!FirstPaired || !SecondPaired || !FirstPartner || !SecondPartner)
	{
		AddError(TEXT("Failed to create shared-notify actors"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}
	FirstPaired->AddPairedPartner(FirstPartner);
	SecondPaired->AddPairedPartner(SecondPartner);
	FAnimNotifyRuntimeSourceId SharedSource;
	SharedSource.SourceAnimation = FSoftObjectPath(TEXT("/Game/Test/Chain/SharedNotify"));
	SharedSource.NotifyEventIndex = 4;
	const EMovementMode FirstBaseline = First->GetCharacterMovement()->MovementMode;
	const EMovementMode SecondBaseline = Second->GetCharacterMovement()->MovementMode;

	TestTrue(TEXT("First actor acquires the shared notify identity"),
		FirstPaired->BeginPairedCollisionNotify(
			SharedSource, 77, true, true, false, true, false, 150.0f));
	TestTrue(TEXT("Second actor independently acquires the same notify identity"),
		SecondPaired->BeginPairedCollisionNotify(
			SharedSource, 77, true, true, false, true, false, 150.0f));
	FirstPaired->EndPairedCollisionNotify(SharedSource, 77);
	TestEqual(TEXT("First End restores only the first actor"),
		First->GetCharacterMovement()->MovementMode.GetValue(), FirstBaseline);
	TestEqual(TEXT("First End cannot restore the second actor"),
		Second->GetCharacterMovement()->MovementMode.GetValue(), MOVE_None);
	SecondPaired->EndPairedCollisionNotify(SharedSource, 77);
	TestEqual(TEXT("Second exact End restores its own baseline"),
		Second->GetCharacterMovement()->MovementMode.GetValue(), SecondBaseline);

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCounterWindowOutlivesBridgeMontageTest,
	"KatanaCombat.Defense.Chain.CounterWindowOutlivesBridgeMontage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCounterWindowOutlivesBridgeMontageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float CounterWindowSeconds = 1.0f;
	constexpr float ResponseDelaySeconds = 0.5f;
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create bridge-lifetime fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
	UPairedAnimationData* BridgeData = CreateTerminalPoseBridgeData();
	UPairedAnimationData* CounterData = CreateChainStageData(EPairedReactionType::Counter);
	Fixture.CounterAttack->CounterData = CounterData;
	int32 NextInstanceId = 1200;
	Fixture.SetPlaybackOverride([&NextInstanceId](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId)
	{
		OutInstanceId = ++NextInstanceId;
		return true;
	});
	if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(BridgeData))
	{
		AddError(TEXT("Failed to start a montage-backed parry bridge"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DeliverBridgeMarker(BridgeData);
	TestEqual(TEXT("The bridge marker opens CounterWindow"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);

	Fixture.EndBridgeMontagesNaturally(BridgeData);
	TestEqual(TEXT("CounterWindow survives the natural end of both bridge montages"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	TestFalse(TEXT("The surviving window frees the defender once its bridge has ended"),
		Fixture.Paired->IsInputBlocked());
	FTSTicker::GetCoreTicker().Tick(ResponseDelaySeconds);
	TestEqual(TEXT("CounterWindow stays open until its configured deadline"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("A counter input after the bridge ended starts the counter stage"),
		Fixture.GetChainState(), EChainCounterState::CounterActive);
	const FCombatInputRecord& CounterInput = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("The late counter input stays on the Chain route"),
		CounterInput.Route, ECombatInputRoute::ChainOnly);
	TestEqual(TEXT("The late counter input is consumed by the counter"),
		CounterInput.Disposition, ECombatInputDisposition::Consumed);

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCounterInputDuringBridgeIsBufferedTest,
	"KatanaCombat.Defense.Chain.CounterInputDuringBridgeIsBuffered",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCounterInputDuringBridgeIsBufferedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float CounterWindowSeconds = 1.0f;
	auto StartBridge = [](FDefenseChainFixture& Fixture, UPairedAnimationData*& OutBridge)
	{
		OutBridge = nullptr;
		if (!Fixture.Initialize())
		{
			return false;
		}
		Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
		OutBridge = CreateTerminalPoseBridgeData();
		Fixture.CounterAttack->CounterData = CreateChainStageData(EPairedReactionType::Counter);
		Fixture.SetPlaybackOverride([NextInstanceId = 1300](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId) mutable
		{
			OutInstanceId = ++NextInstanceId;
			return true;
		});
		return Fixture.StartCommittedParry() && Fixture.StartBridgeStage(OutBridge);
	};
	auto PressAndCapture = [](const FDefenseChainFixture& Fixture, const EInputType InputType)
	{
		Fixture.DefenderCombat->OnInputEvent(InputType, EInputEventType::Press);
		return Fixture.DefenderCombat->GetCombatInputHistory().Last().Serial;
	};

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* BridgeData = nullptr;
		if (!StartBridge(Fixture, BridgeData))
		{
			AddError(TEXT("Failed to start a bridge for the buffered-Light case"));
			Fixture.Destroy();
			return false;
		}
		const int32 QueueBefore = Fixture.DefenderCombat->GetPendingActionCount();
		const uint64 HeavySerial = PressAndCapture(Fixture, EInputType::HeavyAttack);
		const uint64 LightSerial = PressAndCapture(Fixture, EInputType::LightAttack);
		TestEqual(TEXT("A press during the bridge does not skip the bridge"),
			Fixture.GetChainState(), EChainCounterState::ParryActive);
		TestEqual(TEXT("A press during the bridge never enters the normal attack queue"),
			Fixture.DefenderCombat->GetPendingActionCount(), QueueBefore);
		const FCombatInputRecord* Heavy = FindInputRecord(Fixture.DefenderCombat, HeavySerial);
		const FCombatInputRecord* Light = FindInputRecord(Fixture.DefenderCombat, LightSerial);
		if (TestNotNull(TEXT("The replaced bridge press keeps its record"), Heavy))
		{
			TestEqual(TEXT("The replaced bridge press stays on the Chain route"),
				Heavy->Route, ECombatInputRoute::ChainOnly);
			TestEqual(TEXT("A newer bridge press replaces the older one"),
				Heavy->Disposition, ECombatInputDisposition::Replaced);
		}
		if (TestNotNull(TEXT("The buffered bridge press keeps its record"), Light))
		{
			TestEqual(TEXT("The buffered bridge press is on the Chain route"),
				Light->Route, ECombatInputRoute::ChainOnly);
			TestEqual(TEXT("The newest bridge press is buffered"),
				Light->Disposition, ECombatInputDisposition::Queued);
		}

		Fixture.DeliverBridgeMarker(BridgeData);
		TestEqual(TEXT("The buffered Light starts the counter as the window opens"),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
		Light = FindInputRecord(Fixture.DefenderCombat, LightSerial);
		if (TestNotNull(TEXT("The executed bridge press keeps its record"), Light))
		{
			TestEqual(TEXT("The buffered Light is consumed by the counter"),
				Light->Disposition, ECombatInputDisposition::Consumed);
		}
		TestEqual(TEXT("Releasing the buffer never enters the normal attack queue"),
			Fixture.DefenderCombat->GetPendingActionCount(), QueueBefore);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* BridgeData = nullptr;
		if (!StartBridge(Fixture, BridgeData))
		{
			AddError(TEXT("Failed to start a bridge for the newest-Heavy case"));
			Fixture.Destroy();
			return false;
		}
		const uint64 LightSerial = PressAndCapture(Fixture, EInputType::LightAttack);
		const uint64 HeavySerial = PressAndCapture(Fixture, EInputType::HeavyAttack);
		AddExpectedErrorPlain(
			TEXT("Selected attack lacks usable paired counter data"),
			EAutomationExpectedErrorFlags::Contains,
			1);
		Fixture.DeliverBridgeMarker(BridgeData);
		TestEqual(TEXT("A newest Heavy without counter data leaves the window open"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		const FCombatInputRecord* Light = FindInputRecord(Fixture.DefenderCombat, LightSerial);
		const FCombatInputRecord* Heavy = FindInputRecord(Fixture.DefenderCombat, HeavySerial);
		if (TestNotNull(TEXT("The replaced Light keeps its record"), Light))
		{
			TestEqual(TEXT("The older Light was replaced, not executed"),
				Light->Disposition, ECombatInputDisposition::Replaced);
		}
		if (TestNotNull(TEXT("The newest Heavy keeps its record"), Heavy))
		{
			TestEqual(TEXT("The newest Heavy expires when it cannot advance"),
				Heavy->Disposition, ECombatInputDisposition::Expired);
		}
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* BridgeData = nullptr;
		if (!StartBridge(Fixture, BridgeData))
		{
			AddError(TEXT("Failed to start a bridge for the stale-input case"));
			Fixture.Destroy();
			return false;
		}
		const uint64 BufferedSerial = PressAndCapture(Fixture, EInputType::LightAttack);
		Fixture.EndBridgeMontagesNaturally(BridgeData);
		TestEqual(TEXT("A bridge that ends before its marker still ends the sequence"),
			Fixture.GetChainState(), EChainCounterState::None);
		const FCombatInputRecord* Buffered = FindInputRecord(Fixture.DefenderCombat, BufferedSerial);
		if (TestNotNull(TEXT("The discarded bridge press keeps its record"), Buffered))
		{
			TestEqual(TEXT("A sequence that never opened its window discards the buffered press"),
				Buffered->Disposition, ECombatInputDisposition::Expired);
		}
		PressAndCapture(Fixture, EInputType::LightAttack);
		TestEqual(TEXT("A press after the sequence ended takes the normal route, not a stale Chain"),
			Fixture.DefenderCombat->GetCombatInputHistory().Last().Route,
			ECombatInputRoute::NormalQueue);
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCounterWindowExpiresAtDeadlineTest,
	"KatanaCombat.Defense.Chain.CounterWindowExpiresAtDeadline",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCounterWindowExpiresAtDeadlineTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float CounterWindowSeconds = 0.75f;
	constexpr float BeforeDeadlineSeconds = 0.5f;
	constexpr float PastDeadlineSeconds = 0.3f;
	FScopedIntConsoleVariableOverride DefenseTelemetry(TEXT("Combat.Defense.Debug"), 1);
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create window-deadline fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
	UPairedAnimationData* BridgeData = CreateTerminalPoseBridgeData();
	Fixture.SetPlaybackOverride([NextInstanceId = 1400](
		const EPairedAnimationRole Role,
		const UPairedAnimationData* Data,
		int32& OutInstanceId) mutable
	{
		OutInstanceId = ++NextInstanceId;
		return true;
	});
	if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(BridgeData))
	{
		AddError(TEXT("Failed to start a montage-backed parry bridge"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DeliverBridgeMarker(BridgeData);
	TestEqual(TEXT("The bridge marker opens CounterWindow"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	Fixture.DefenderCombat->ClearDefenseTelemetry();

	Fixture.EndBridgeMontagesNaturally(BridgeData);
	TestEqual(TEXT("The bridge montages' end does not close the window"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	FTSTicker::GetCoreTicker().Tick(BeforeDeadlineSeconds);
	TestEqual(TEXT("The window is open before its configured deadline"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	FTSTicker::GetCoreTicker().Tick(PastDeadlineSeconds);
	TestEqual(TEXT("The window closes at its configured deadline"),
		Fixture.GetChainState(), EChainCounterState::None);
	TestEqual(TEXT("Deadline cleanup records its reason against CounterWindow"),
		CountDefenseCleanups(
			Fixture.DefenderCombat,
			TEXT("CounterWindow"),
			TEXT("CounterWindowExpired")),
		1);
	TestEqual(TEXT("The window is never reported as a bridge that ended before it"),
		CountDefenseCleanups(
			Fixture.DefenderCombat,
			TEXT("CounterWindow"),
			TEXT("BridgeEndedBeforeCounter")),
		0);
	TestFalse(TEXT("Deadline cleanup releases input ownership"), Fixture.Paired->IsInputBlocked());

	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainParriedAttackerVulnerableForWindowTest,
	"KatanaCombat.Defense.Chain.ParriedAttackerVulnerableForWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainParriedAttackerVulnerableForWindowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float CounterWindowSeconds = 1.0f;
	constexpr float ParryStaggerSeconds = 0.4f;
	auto StartWindowAfterStagger = [this](
		FDefenseChainFixture& Fixture,
		UEnemyCombatAIComponent*& OutSourceAI)
	{
		OutSourceAI = nullptr;
		if (!Fixture.Initialize())
		{
			return false;
		}
		Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
		UDefenseConfiguration* SourceConfig = NewObject<UDefenseConfiguration>();
		SourceConfig->ParryStaggerDuration = ParryStaggerSeconds;
		SourceConfig->MaximumAutomaticTurn = 70.0f;
		SourceConfig->DefenseTurnRate = 360.0f;
		Fixture.SourceCombat->DefenseConfigurationOverride = SourceConfig;
		Fixture.CounterAttack->CounterData = CreateChainStageData(EPairedReactionType::Counter);
		OutSourceAI = Fixture.SourceAttacker->CombatAIComponent.Get();
		if (!OutSourceAI)
		{
			return false;
		}
		FEnemyAttackConfig AttackConfig;
		AttackConfig.AttackData = Fixture.SourceAttack;
		AttackConfig.MinRange = 0.0f;
		AttackConfig.MaxRange = 1000.0f;
		OutSourceAI->AvailableAttacks = {AttackConfig};
		OutSourceAI->SetCombatTarget(Fixture.Defender);
		TestTrue(TEXT("The source AI can attack before the parry"), OutSourceAI->CanAttemptAttack());
		Fixture.SetPlaybackOverride([NextInstanceId = 1500](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId) mutable
		{
			OutInstanceId = ++NextInstanceId;
			return true;
		});
		UPairedAnimationData* BridgeData = CreateTerminalPoseBridgeData();
		if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(BridgeData))
		{
			return false;
		}
		Fixture.DeliverBridgeMarker(BridgeData);
		Fixture.EndBridgeMontagesNaturally(BridgeData);
		Fixture.SourceAttacker->HitReactionComponent->TickComponent(
			ParryStaggerSeconds + 0.1f,
			LEVELTICK_All,
			nullptr);
		return true;
	};
	auto ExpectHeldForWindow = [this](
		const FDefenseChainFixture& Fixture,
		const UEnemyCombatAIComponent* SourceAI)
	{
		TestFalse(TEXT("The configured parry stagger expired inside the window"),
			Fixture.SourceAttacker->HitReactionComponent->IsStaggered());
		TestEqual(TEXT("The counter window is still open after the bridge and the stagger ended"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		TestTrue(TEXT("The Chain still suppresses the parried attacker's AI"),
			SourceAI->IsDefenseChainSuppressed());
		TestFalse(TEXT("The parried attacker's AI cannot attack while a counter is allowed"),
			SourceAI->CanAttemptAttack());
		TestFalse(TEXT("The parried attacker cannot process attack input while a counter is allowed"),
			Fixture.SourceCombat->CanProcessInput(EInputType::LightAttack));
		TestTrue(TEXT("The parried attacker stays in the Chain's paired victim state"),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
	};

	{
		FDefenseChainFixture Fixture;
		UEnemyCombatAIComponent* SourceAI = nullptr;
		if (!StartWindowAfterStagger(Fixture, SourceAI))
		{
			AddError(TEXT("Failed to reach CounterWindow after the parry stagger"));
			Fixture.Destroy();
			return false;
		}
		ExpectHeldForWindow(Fixture, SourceAI);
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		TestEqual(TEXT("The parried attacker is still counterable after its stagger expired"),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UEnemyCombatAIComponent* SourceAI = nullptr;
		if (!StartWindowAfterStagger(Fixture, SourceAI))
		{
			AddError(TEXT("Failed to reach CounterWindow for the release case"));
			Fixture.Destroy();
			return false;
		}
		ExpectHeldForWindow(Fixture, SourceAI);
		FTSTicker::GetCoreTicker().Tick(CounterWindowSeconds + 0.1f);
		TestEqual(TEXT("The uncountered window resolves at its deadline"),
			Fixture.GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("Window resolution releases the AI suppression"),
			SourceAI->IsDefenseChainSuppressed());
		TestFalse(TEXT("Window resolution releases the paired victim state"),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		TestTrue(TEXT("The released attacker can process attack input"),
			Fixture.SourceCombat->CanProcessInput(EInputType::LightAttack));
		TestTrue(TEXT("The released attacker's AI can attack again"),
			SourceAI->CanAttemptAttack());
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainFinisherReadyOutlivesCounterMontageTest,
	"KatanaCombat.Defense.Chain.FinisherReadyOutlivesCounterMontage",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainFinisherReadyOutlivesCounterMontageTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float FinisherReadySeconds = 0.75f;
	constexpr float BeforeDeadlineSeconds = 0.5f;
	constexpr float PastDeadlineSeconds = 0.3f;
	FScopedIntConsoleVariableOverride DefenseTelemetry(TEXT("Combat.Defense.Debug"), 1);
	auto ReachFinisherReady = [](
		FDefenseChainFixture& Fixture,
		UPairedAnimationData*& OutCounter,
		bool*& OutFailFinisherVictim)
	{
		if (!Fixture.Initialize() || !Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
		{
			return false;
		}
		Fixture.DefenseConfig->FinisherReadySeconds = FinisherReadySeconds;
		OutCounter = CreateChainStageData(
			EPairedReactionType::Counter,
			EChainStageTransitionType::AutoContinue,
			TEXT("RetryAfterCounterEnd"),
			true);
		UPairedAnimationData* FinisherData = CreateChainStageData(EPairedReactionType::Finisher);
		Fixture.CounterAttack->CounterData = OutCounter;
		Fixture.CounterAttack->FinisherData = FinisherData;
		Fixture.SetPlaybackOverride([OutFailFinisherVictim, FinisherData, NextInstanceId = 1600](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId) mutable
		{
			OutInstanceId = ++NextInstanceId;
			return !(Data == FinisherData
				&& Role == EPairedAnimationRole::Victim
				&& *OutFailFinisherVictim);
		});
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		Fixture.Paired->HandleChainStageTransition(
			EChainStageTransitionType::AutoContinue,
			Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
			MakeMarkerSource(OutCounter->AttackerMontage));
		return Fixture.GetChainState() == EChainCounterState::FinisherReady;
	};

	{
		bool bFailFinisherVictim = true;
		bool* FailFinisherVictim = &bFailFinisherVictim;
		FDefenseChainFixture Fixture;
		UPairedAnimationData* CounterData = nullptr;
		if (!ReachFinisherReady(Fixture, CounterData, FailFinisherVictim))
		{
			AddError(TEXT("Failed to reach a retryable FinisherReady"));
			Fixture.Destroy();
			return false;
		}
		Fixture.Paired->HandleOwnerPairedMontageEnded(CounterData->AttackerMontage, false);
		Fixture.SourcePaired->HandleOwnerPairedMontageEnded(CounterData->VictimMontage, false);
		FTSTicker::GetCoreTicker().Tick(0.0f);
		TestEqual(TEXT("FinisherReady survives the natural end of both counter montages"),
			Fixture.GetChainState(), EChainCounterState::FinisherReady);
		bFailFinisherVictim = false;
		Fixture.DefenderCombat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
		TestEqual(TEXT("A retry after the counter montage ended still starts the finisher"),
			Fixture.GetChainState(), EChainCounterState::FinisherActive);
		Fixture.Destroy();
	}

	{
		bool bFailFinisherVictim = true;
		bool* FailFinisherVictim = &bFailFinisherVictim;
		FDefenseChainFixture Fixture;
		UPairedAnimationData* CounterData = nullptr;
		if (!ReachFinisherReady(Fixture, CounterData, FailFinisherVictim))
		{
			AddError(TEXT("Failed to reach a retryable FinisherReady for the deadline case"));
			Fixture.Destroy();
			return false;
		}
		Fixture.DefenderCombat->ClearDefenseTelemetry();
		Fixture.Paired->HandleOwnerPairedMontageEnded(CounterData->AttackerMontage, false);
		Fixture.SourcePaired->HandleOwnerPairedMontageEnded(CounterData->VictimMontage, false);
		FTSTicker::GetCoreTicker().Tick(BeforeDeadlineSeconds);
		TestEqual(TEXT("FinisherReady is open before its configured deadline"),
			Fixture.GetChainState(), EChainCounterState::FinisherReady);
		FTSTicker::GetCoreTicker().Tick(PastDeadlineSeconds);
		TestEqual(TEXT("FinisherReady closes at its configured deadline"),
			Fixture.GetChainState(), EChainCounterState::None);
		TestEqual(TEXT("FinisherReady deadline cleanup records its own reason"),
			CountDefenseCleanups(
				Fixture.DefenderCombat,
				TEXT("FinisherReady"),
				TEXT("FinisherReadyExpired")),
			1);
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainBridgeFreesDefenderTest,
	"KatanaCombat.Defense.Chain.BridgeFreesDefenderAndHoldsAttacker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseChainBridgeFreesDefenderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FBridgeMontageTimings Timings;
	Timings.BridgeEnd = 0.70f;
	Timings.ReadyEnd = 0.80f;
	Timings.BlendOut = 0.25f;
	Timings.MarkerTime = 0.65f;
	constexpr float CounterWindowSeconds = 2.0f;
	constexpr float FrameSeconds = 1.0f / 60.0f;
	TestTrue(TEXT("The fixture marker lies inside the bridge section's auto blend-out tail"),
		Timings.MarkerTime > Timings.BridgeEnd - Timings.BlendOut);

	auto StartRealBridge = [&Timings](FDefenseChainFixture& Fixture, UPairedAnimationData*& OutBridge)
	{
		OutBridge = nullptr;
		if (!Fixture.Initialize() || !Fixture.ConfigureProductionMeshes())
		{
			return false;
		}
		Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
		Fixture.EnableDefaultMovement();
		OutBridge = CreateHeldBridgeData(Timings);
		if (!OutBridge || !Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(OutBridge))
		{
			return false;
		}
		Fixture.RouteStageMontageEnds(OutBridge);
		return true;
	};
	auto ExpectDefenderCommitted = [this](
		const FDefenseChainFixture& Fixture,
		const UPairedAnimationData* Bridge,
		const TCHAR* Context)
	{
		TestTrue(FString::Printf(TEXT("%s: the defender's bridge montage is playing"), Context),
			Fixture.Defender->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Bridge->AttackerMontage));
		TestTrue(FString::Printf(TEXT("%s: the defender stays committed to its bridge"), Context),
			Fixture.Paired->IsInputBlocked());
		TestTrue(FString::Printf(TEXT("%s: the defender is still a paired participant"), Context),
			Fixture.Paired->IsPairedAnimationActive());
	};
	auto ExpectDefenderFreeAttackerHeld = [this](
		const FDefenseChainFixture& Fixture,
		const UPairedAnimationData* Bridge,
		const TCHAR* Context)
	{
		UAnimInstance* DefenderAnim = Fixture.Defender->GetMesh()->GetAnimInstance();
		UAnimInstance* SourceAnim = Fixture.SourceAttacker->GetMesh()->GetAnimInstance();
		TestEqual(FString::Printf(TEXT("%s: CounterWindow is still open"), Context),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		TestFalse(FString::Printf(TEXT("%s: the defender's bridge montage has ended"), Context),
			DefenderAnim->Montage_IsPlaying(Bridge->AttackerMontage));
		TestFalse(FString::Printf(TEXT("%s: the defender's input is free"), Context),
			Fixture.Paired->IsInputBlocked());
		TestTrue(FString::Printf(TEXT("%s: the defender's movement input is allowed"), Context),
			Fixture.DefenderCombat->SubmitMovementInput(FVector2D(0.0f, 1.0f), FRotator::ZeroRotator));
		TestTrue(FString::Printf(TEXT("%s: the defender's movement is enabled"), Context),
			Fixture.Defender->GetCharacterMovement()->MovementMode != MOVE_None);
		TestEqual(FString::Printf(TEXT("%s: the defender holds no collision or movement lease"), Context),
			Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
		TestFalse(FString::Printf(TEXT("%s: the defender is no longer a paired participant"), Context),
			Fixture.Paired->IsPairedAnimationActive());
		TestFalse(FString::Printf(TEXT("%s: the sequence no longer holds the defender"), Context),
			Fixture.Paired->IsDefenseSequenceParticipant());
		TestTrue(FString::Printf(TEXT("%s: the source's bridge montage is still playing"), Context),
			SourceAnim->Montage_IsPlaying(Bridge->VictimMontage));
		TestEqual(FString::Printf(TEXT("%s: the source holds CounterReady"), Context),
			SourceAnim->Montage_GetCurrentSection(Bridge->VictimMontage),
			FName(TEXT("CounterReady")));
		TestTrue(FString::Printf(TEXT("%s: the sequence still holds the source attacker"), Context),
			Fixture.SourcePaired->IsDefenseSequenceParticipant());
		TestTrue(FString::Printf(TEXT("%s: the source stays in the paired victim state"), Context),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		TestTrue(FString::Printf(TEXT("%s: the source's movement stays locked"), Context),
			Fixture.SourceAttacker->GetCharacterMovement()->MovementMode == MOVE_None);
		TestTrue(FString::Printf(TEXT("%s: the source keeps its collision and movement lease"), Context),
			Fixture.SourcePaired->GetActivePairedStateLeaseCount() > 0);
	};

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Bridge = nullptr;
		if (!StartRealBridge(Fixture, Bridge))
		{
			AddError(TEXT("Failed to start a real-montage bridge"));
			Fixture.Destroy();
			return false;
		}
		Fixture.AdvanceMontages(Timings.MarkerTime - 0.05f, FrameSeconds);
		const FAnimMontageInstance* BridgeInstance = Fixture.Defender->GetMesh()->GetAnimInstance()
			->GetActiveInstanceForMontage(Bridge->AttackerMontage);
		TestTrue(TEXT("The bridge has not begun an automatic blend-out before its marker"),
			BridgeInstance && !BridgeInstance->IsStopped());
		TestEqual(TEXT("The bridge is still ParryActive before its marker"),
			Fixture.GetChainState(), EChainCounterState::ParryActive);
		ExpectDefenderCommitted(Fixture, Bridge, TEXT("Before the marker"));
		Fixture.AdvanceMontages(0.06f, FrameSeconds);
		TestEqual(TEXT("The real driver marker opens CounterWindow"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		ExpectDefenderCommitted(Fixture, Bridge, TEXT("Window open, bridge still playing"));
		Fixture.AdvanceMontages(1.0f, FrameSeconds);
		ExpectDefenderFreeAttackerHeld(Fixture, Bridge, TEXT("Frame by frame"));

		FTSTicker::GetCoreTicker().Tick(CounterWindowSeconds);
		TestEqual(TEXT("The window still resolves at its deadline"),
			Fixture.GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("The deadline releases the held source attacker"),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		TestFalse(TEXT("The deadline stops the source attacker's held pose"),
			Fixture.SourceAttacker->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Bridge->VictimMontage));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Bridge = nullptr;
		if (!StartRealBridge(Fixture, Bridge))
		{
			AddError(TEXT("Failed to start a real-montage bridge for the long-frame case"));
			Fixture.Destroy();
			return false;
		}
		Fixture.AdvanceMontages(Timings.MarkerTime - 0.03f, FrameSeconds);
		const float LongFrame = Timings.BridgeEnd - Timings.MarkerTime + 0.05f;
		Fixture.AdvanceMontages(LongFrame, LongFrame);
		TestEqual(TEXT("One frame across the marker and the bridge end still opens CounterWindow"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		Fixture.AdvanceMontages(0.5f, FrameSeconds);
		ExpectDefenderFreeAttackerHeld(Fixture, Bridge, TEXT("Long frame"));
		Fixture.Destroy();
	}

	{
		FScopedIntConsoleVariableOverride DefenseTelemetry(TEXT("Combat.Defense.Debug"), 1);
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Bridge = nullptr;
		if (!StartRealBridge(Fixture, Bridge))
		{
			AddError(TEXT("Failed to start a real-montage bridge for the missed-marker case"));
			Fixture.Destroy();
			return false;
		}
		Fixture.InvalidateBridgeMarkerIdentity();
		Fixture.DefenderCombat->ClearDefenseTelemetry();
		Fixture.AdvanceMontages(Timings.ReadyEnd + 0.1f, FrameSeconds);
		TestEqual(TEXT("A defender bridge that ends without opening CounterWindow ends the sequence"),
			Fixture.GetChainState(), EChainCounterState::None);
		TestFalse(TEXT("The missed-marker cleanup releases input ownership"),
			Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("The missed-marker cleanup releases the source attacker"),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		TestEqual(TEXT("The missed-marker cleanup is reported as a bridge ending before its window"),
			CountDefenseCleanups(
				Fixture.DefenderCombat,
				TEXT("ParryActive"),
				TEXT("BridgeEndedBeforeCounter")),
			1);
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainRealBridgeCounterHandoffTest,
	"KatanaCombat.Defense.Chain.RealBridgeCounterHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseChainRealBridgeCounterHandoffTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FBridgeMontageTimings Timings;
	Timings.BridgeEnd = 0.70f;
	Timings.ReadyEnd = 0.80f;
	Timings.BlendOut = 0.25f;
	Timings.MarkerTime = 0.65f;
	constexpr float CounterWindowSeconds = 2.0f;
	constexpr float CounterLength = 0.80f;
	constexpr float FrameSeconds = 1.0f / 60.0f;

	auto StartRealBridgeWithCounter = [&Timings](
		FDefenseChainFixture& Fixture,
		const FName CounterSlot,
		UPairedAnimationData*& OutBridge,
		UPairedAnimationData*& OutCounter)
	{
		OutBridge = CreateHeldBridgeData(Timings);
		OutCounter = CreatePlayableCounterData(CounterSlot, CounterLength);
		if (!OutBridge || !OutCounter)
		{
			return false;
		}
		Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
		Fixture.CounterAttack->CounterData = OutCounter;
		if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(OutBridge))
		{
			return false;
		}
		Fixture.RouteStageMontageEnds(OutBridge);
		return true;
	};
	// A Light press during the bridge is buffered; the real marker, dispatched by the defender's anim
	// instance, opens the window and the buffered press starts the counter inside that notify dispatch.
	auto PressDuringBridgeAndHandOff = [this, &Timings](
		const FDefenseChainFixture& Fixture,
		const UPairedAnimationData* Bridge,
		const UPairedAnimationData* Counter,
		const TCHAR* Context)
	{
		UAnimInstance* DefenderAnim = Fixture.Defender->GetMesh()->GetAnimInstance();
		UAnimInstance* SourceAnim = Fixture.SourceAttacker->GetMesh()->GetAnimInstance();
		Fixture.AdvanceMontages(Timings.MarkerTime - 0.05f, FrameSeconds);
		TestEqual(FString::Printf(TEXT("%s: the bridge is ParryActive before its marker"), Context),
			Fixture.GetChainState(), EChainCounterState::ParryActive);
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		const uint64 Serial = Fixture.DefenderCombat->GetCombatInputHistory().Last().Serial;
		const FCombatInputRecord* Record = FindInputRecord(Fixture.DefenderCombat, Serial);
		if (TestNotNull(*FString::Printf(TEXT("%s: the bridge press keeps its record"), Context), Record))
		{
			TestEqual(FString::Printf(TEXT("%s: the bridge press is buffered"), Context),
				Record->Disposition, ECombatInputDisposition::Queued);
		}

		Fixture.AdvanceMontages(0.06f, FrameSeconds);
		TestEqual(FString::Printf(TEXT("%s: the real marker releases the buffered press into the counter"), Context),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
		Record = FindInputRecord(Fixture.DefenderCombat, Serial);
		if (TestNotNull(*FString::Printf(TEXT("%s: the executed press keeps its record"), Context), Record))
		{
			TestEqual(FString::Printf(TEXT("%s: the buffered press is on the Chain route"), Context),
				Record->Route, ECombatInputRoute::ChainOnly);
			TestEqual(FString::Printf(TEXT("%s: the buffered press is consumed by the counter"), Context),
				Record->Disposition, ECombatInputDisposition::Consumed);
		}
		TestTrue(FString::Printf(TEXT("%s: the defender plays its counter"), Context),
			DefenderAnim->Montage_IsPlaying(Counter->AttackerMontage));
		TestTrue(FString::Printf(TEXT("%s: the source plays its counter reaction"), Context),
			SourceAnim->Montage_IsPlaying(Counter->VictimMontage));
		TestFalse(FString::Printf(TEXT("%s: the counter stops the defender's bridge"), Context),
			DefenderAnim->Montage_IsPlaying(Bridge->AttackerMontage));
		TestFalse(FString::Printf(TEXT("%s: the counter stops the source's held bridge loop"), Context),
			SourceAnim->Montage_IsPlaying(Bridge->VictimMontage));

		Fixture.AdvanceMontages(0.3f, FrameSeconds);
		TestTrue(FString::Printf(TEXT("%s: both stopped bridge roles' retired callbacks are consumed"), Context),
			Fixture.HasNoPendingRoleMontageCallbacks());
		TestEqual(FString::Printf(TEXT("%s: the stopped bridge callbacks leave the counter running"), Context),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
	};

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Bridge = nullptr;
		UPairedAnimationData* Counter = nullptr;
		if (!Fixture.Initialize() || !Fixture.ConfigureProductionMeshes()
			|| !StartRealBridgeWithCounter(Fixture, FAnimSlotGroup::DefaultSlotName, Bridge, Counter))
		{
			AddError(TEXT("Failed to start a real-montage bridge with a same-group counter"));
			Fixture.Destroy();
			return false;
		}
		PressDuringBridgeAndHandOff(Fixture, Bridge, Counter, TEXT("Same montage group"));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.ConfigureProductionMeshes())
		{
			AddError(TEXT("Failed to create the cross-group handoff fixture"));
			Fixture.Destroy();
			return false;
		}
		const USkeletalMesh* DefenderMesh = Fixture.Defender->GetMesh()->GetSkeletalMeshAsset();
		const FName AlternateSlot = FindAlternateGroupSlot(DefenderMesh ? DefenderMesh->GetSkeleton() : nullptr);
		if (AlternateSlot.IsNone())
		{
			AddError(TEXT("The production skeleton has no slot outside the default group, so the cross-group handoff cannot run"));
			Fixture.Destroy();
			return false;
		}
		UPairedAnimationData* Bridge = nullptr;
		UPairedAnimationData* Counter = nullptr;
		if (!StartRealBridgeWithCounter(Fixture, AlternateSlot, Bridge, Counter))
		{
			AddError(TEXT("Failed to start a real-montage bridge with a cross-group counter"));
			Fixture.Destroy();
			return false;
		}
		TestTrue(TEXT("The counter plays in a different montage group from the bridge"),
			Counter->AttackerMontage->GetGroupName() != Bridge->AttackerMontage->GetGroupName());
		AddInfo(FString::Printf(TEXT("Cross-group counter slot %s in group %s; bridge group %s"),
			*AlternateSlot.ToString(),
			*Counter->AttackerMontage->GetGroupName().ToString(),
			*Bridge->AttackerMontage->GetGroupName().ToString()));
		PressDuringBridgeAndHandOff(Fixture, Bridge, Counter, TEXT("Different montage group"));
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainFreedDefenderMovesAndGuardsTest,
	"KatanaCombat.Defense.Chain.FreedDefenderMovesAndGuards",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainFreedDefenderMovesAndGuardsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float CounterWindowSeconds = 5.0f;
	const FVector2D MovementInput(0.0f, 1.0f);
	FScopedActionReactionTelemetry Telemetry(1);
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create the freed-defender fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
	Fixture.CounterAttack->CounterData = CreateChainStageData(EPairedReactionType::Counter);
	Fixture.EnableDefaultMovement();
	Fixture.SetPlaybackOverride(MakeStagePlaybackOverride(1800));
	UPairedAnimationData* Bridge = CreateTerminalPoseBridgeData();
	if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(Bridge))
	{
		AddError(TEXT("Failed to start a montage-backed parry bridge"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DeliverBridgeMarker(Bridge);
	TestEqual(TEXT("The bridge marker opens CounterWindow"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	TestTrue(TEXT("While its bridge plays the defender stays committed"),
		Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("While its bridge plays the defender cannot move"),
		Fixture.DefenderCombat->SubmitMovementInput(MovementInput, FRotator::ZeroRotator));
	// The committed defender's input gate is live in this fixture: a Block press is rejected by the combat
	// state, so the guard accepted after the bridge ends is the release at work.
	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
	const int32 StateRejectionsBefore = CountActionReactionTelemetry(
		Fixture.DefenderCombat->GetActionReactionTelemetry(),
		EActionReactionTelemetryEvent::InputFinalized,
		EActionReactionTelemetryReason::CombatStateRejected);
	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestFalse(TEXT("While its bridge plays the committed defender cannot raise a guard"),
		Fixture.DefenderCombat->IsBlocking());
	TestEqual(TEXT("The committed defender's Block press is rejected by its combat state"),
		CountActionReactionTelemetry(
			Fixture.DefenderCombat->GetActionReactionTelemetry(),
			EActionReactionTelemetryEvent::InputFinalized,
			EActionReactionTelemetryReason::CombatStateRejected) - StateRejectionsBefore,
		1);
	const FDefenseInteractionId OriginalInteraction =
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction;

	Fixture.EndBridgeMontagesNaturally(Bridge);
	TestEqual(TEXT("The bridge's end does not close the window"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	TestFalse(TEXT("Once its bridge ends the defender's input is free"),
		Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("The freed defender's movement input is allowed"),
		Fixture.DefenderCombat->SubmitMovementInput(MovementInput, FRotator::ZeroRotator));
	TestTrue(TEXT("The freed defender's movement is enabled"),
		Fixture.Defender->GetCharacterMovement()->MovementMode != MOVE_None);
	TestEqual(TEXT("The freed defender holds no collision or movement lease"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestFalse(TEXT("The freed defender is not a paired participant"),
		Fixture.Paired->IsPairedAnimationActive());
	TestFalse(TEXT("The sequence no longer holds the freed defender"),
		Fixture.Paired->IsDefenseSequenceParticipant());
	TestTrue(TEXT("The sequence still holds the parried attacker"),
		Fixture.SourcePaired->IsDefenseSequenceParticipant());
	TestTrue(TEXT("The parried attacker stays in the paired victim state"),
		Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
	TestTrue(TEXT("The parried attacker's movement stays locked"),
		Fixture.SourceAttacker->GetCharacterMovement()->MovementMode == MOVE_None);
	TestTrue(TEXT("The parried attacker keeps its collision and movement lease"),
		Fixture.SourcePaired->GetActivePairedStateLeaseCount() > 0);

	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
	TestFalse(TEXT("Releasing Block lowers the parry press's guard"), Fixture.DefenderCombat->IsBlocking());
	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("The freed defender can guard"), Fixture.DefenderCombat->IsBlocking());
	const FCombatInputRecord GuardRecord = Fixture.DefenderCombat->GetCombatInputHistory().Last();
	TestEqual(TEXT("Block in the window is stateful control"),
		GuardRecord.Route, ECombatInputRoute::StatefulControl);
	TestEqual(TEXT("Block in the window is consumed"),
		GuardRecord.Disposition, ECombatInputDisposition::Consumed);
	TestEqual(TEXT("Guarding does not end the window"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);

	AEnemyCharacter* SecondAttacker = FCombatTestHelpers::CreateTestEnemyCharacter(
		Fixture.World, FVector(400.0f, 0.0f, 0.0f));
	UAttackData* SecondAttack = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
	SecondAttack->AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	FAttackInstanceId SecondInstance;
	if (!SecondAttacker)
	{
		AddError(TEXT("Failed to spawn a second attacker"));
		Fixture.Destroy();
		return false;
	}
	SecondAttacker->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
	if (!Fixture.ArmParryableAttack(SecondAttacker, SecondAttack, 43, 403, SecondInstance))
	{
		AddError(TEXT("Failed to arm the second attacker's parryable attack"));
		Fixture.Destroy();
		return false;
	}
	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
	Fixture.DefenderCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
	TestTrue(TEXT("Block still guards against a second attacker"), Fixture.DefenderCombat->IsBlocking());
	TestFalse(TEXT("Block cannot commit a second perfect parry while the window is open"),
		SecondAttacker->CombatComponent->IsAttackConsumed(SecondInstance));
	TestEqual(TEXT("The Block press resolves as guard entry, not a perfect parry"),
		Fixture.DefenderCombat->GetLastInputDefenseResolutionForTesting().Decision.Outcome,
		EDefenseOutcome::GuardEntered);
	TestEqual(TEXT("The open window keeps its state"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	TestTrue(TEXT("The open window keeps its original owner"),
		Fixture.Paired->GetActiveDefenseSequenceContext().OriginatingInteraction == OriginalInteraction);
	Fixture.Destroy();
	return true;
}

namespace
{
/** A third party able to land a weapon contact on the defender, and the attack it lands with. */
struct FChainThirdParty
{
	AEnemyCharacter* Character = nullptr;
	UAttackData* Attack = nullptr;
};

/**
 * Reach CounterWindow through a montage-backed bridge on the playback override, with production meshes and the
 * defender's hit reactions able to play Reaction, and a third party in reach. The bridge has not ended: the
 * defender is still committed.
 */
bool ReachCommittedWindowWithThirdParty(
	FDefenseChainFixture& Fixture,
	const int32 FirstInstanceId,
	const float ThirdPartyDamage,
	UPairedAnimationData*& OutBridge,
	UAnimMontage*& OutReaction,
	FChainThirdParty& OutThirdParty)
{
	if (!Fixture.Initialize() || !Fixture.ConfigureProductionMeshes())
	{
		return false;
	}
	OutReaction = CreatePlayableReactionMontage();
	if (!OutReaction
		|| !Fixture.EnableDefenderHitReactionPlayback(CreateEveryDirectionReactionSettings(OutReaction)))
	{
		return false;
	}
	Fixture.SetPlaybackOverride(MakeStagePlaybackOverride(FirstInstanceId));
	OutBridge = CreateTerminalPoseBridgeData();
	OutThirdParty.Character = FCombatTestHelpers::CreateTestEnemyCharacter(
		Fixture.World, FVector(-200.0f, 0.0f, 0.0f));
	OutThirdParty.Attack = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
	OutThirdParty.Attack->BaseDamage = ThirdPartyDamage;
	if (!OutThirdParty.Character || !Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(OutBridge))
	{
		return false;
	}
	Fixture.DeliverBridgeMarker(OutBridge);
	return Fixture.GetChainState() == EChainCounterState::CounterWindow;
}

/** Resolve and land ThirdParty's weapon contact on the defender through the production contact path. */
FDefenseContactReceipt LandThirdPartyContact(
	const FDefenseChainFixture& Fixture,
	const FChainThirdParty& ThirdParty,
	const int32 TraceGeneration)
{
	const FDefenseContactRequest Request = MakeChainThirdPartyContact(
		ThirdParty.Character, Fixture.Defender, ThirdParty.Attack, TraceGeneration);
	const FDefenseContactReceipt Receipt =
		ThirdParty.Character->ResolveWeaponContactCandidate(Fixture.Defender, Request);
	ThirdParty.Character->FinalizeResolvedWeaponContact(Fixture.Defender, Receipt);
	return Receipt;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainThirdPartyHitLandsDuringWindowTest,
	"KatanaCombat.Defense.Chain.ThirdPartyHitLandsDuringWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseChainThirdPartyHitLandsDuringWindowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float ThirdPartyDamage = 20.0f;
	FDefenseChainFixture Fixture;
	UPairedAnimationData* Bridge = nullptr;
	UAnimMontage* Reaction = nullptr;
	FChainThirdParty ThirdParty;
	if (!ReachCommittedWindowWithThirdParty(Fixture, 1900, ThirdPartyDamage, Bridge, Reaction, ThirdParty))
	{
		AddError(TEXT("Failed to reach CounterWindow with a third party and playable hit reactions"));
		Fixture.Destroy();
		return false;
	}

	const float HealthBeforeCommittedContact = Fixture.Defender->CurrentHealth;
	const FDefenseContactReceipt CommittedReceipt = LandThirdPartyContact(Fixture, ThirdParty, 1);
	TestEqual(TEXT("While its bridge plays the committed defender is not hit by a third party"),
		CommittedReceipt.Resolution.Decision.Outcome, EDefenseOutcome::IgnoredInvalid);
	TestEqual(TEXT("The ignored contact applies no damage"),
		Fixture.Defender->CurrentHealth, HealthBeforeCommittedContact);

	Fixture.EndBridgeMontagesNaturally(Bridge);
	TestEqual(TEXT("The freed defender waits in CounterWindow"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	// Super armor is the production way for a landed hit to cause no reaction: the defender could play one
	// (its anim instance and reaction settings are live), so an open window below is not an artifact of the
	// fixture.
	Fixture.Defender->HitReactionComponent->bHasSuperArmor = true;
	const float HealthBeforeFreeContact = Fixture.Defender->CurrentHealth;
	const FDefenseContactReceipt FreeReceipt = LandThirdPartyContact(Fixture, ThirdParty, 2);
	TestEqual(TEXT("A third party's contact lands on the freed defender"),
		FreeReceipt.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestTrue(TEXT("The landed contact reports its damage"), FreeReceipt.AppliedDamage > 0.0f);
	TestTrue(TEXT("The landed contact damages the freed defender"),
		Fixture.Defender->CurrentHealth < HealthBeforeFreeContact);
	TestFalse(TEXT("Super armor plays no hit reaction"),
		Fixture.Defender->GetMesh()->GetAnimInstance()->Montage_IsPlaying(Reaction));
	TestEqual(TEXT("A landed hit that causes no reaction leaves the window open"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	TestTrue(TEXT("The parried attacker stays held through the third party's hit"),
		Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainThirdPartyHitReactionEndsWindowTest,
	"KatanaCombat.Defense.Chain.ThirdPartyHitReactionEndsWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseChainThirdPartyHitReactionEndsWindowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float ThirdPartyDamage = 20.0f;
	FScopedIntConsoleVariableOverride DefenseTelemetry(TEXT("Combat.Defense.Debug"), 1);
	FDefenseChainFixture Fixture;
	UPairedAnimationData* Bridge = nullptr;
	UAnimMontage* Reaction = nullptr;
	FChainThirdParty ThirdParty;
	if (!ReachCommittedWindowWithThirdParty(Fixture, 1950, ThirdPartyDamage, Bridge, Reaction, ThirdParty))
	{
		AddError(TEXT("Failed to reach CounterWindow with a third party and playable hit reactions"));
		Fixture.Destroy();
		return false;
	}
	Fixture.EndBridgeMontagesNaturally(Bridge);
	TestEqual(TEXT("The freed defender waits in CounterWindow"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	const UEnemyCombatAIComponent* SourceAI = Fixture.SourceAttacker->CombatAIComponent.Get();
	Fixture.DefenderCombat->ClearDefenseTelemetry();

	const FDefenseContactReceipt Receipt = LandThirdPartyContact(Fixture, ThirdParty, 2);
	TestEqual(TEXT("A third party's contact lands on the freed defender"),
		Receipt.Resolution.Decision.Outcome, EDefenseOutcome::Hit);
	TestEqual(TEXT("The hit reaction the contact starts ends the window"),
		Fixture.GetChainState(), EChainCounterState::None);
	TestEqual(TEXT("Cleanup records the defender's reaction"),
		CountDefenseCleanups(
			Fixture.DefenderCombat,
			TEXT("CounterWindow"),
			TEXT("DefenderHitReaction")),
		1);
	TestFalse(TEXT("The parried attacker leaves the paired victim state"),
		Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
	TestFalse(TEXT("The parried attacker is no longer held"),
		Fixture.SourcePaired->IsDefenseSequenceParticipant());
	TestFalse(TEXT("The parried attacker's AI is released"),
		SourceAI && SourceAI->IsDefenseChainSuppressed());
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainCounterInputRecommitsFreedDefenderTest,
	"KatanaCombat.Defense.Chain.CounterInputRecommitsFreedDefender",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainCounterInputRecommitsFreedDefenderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float CounterWindowSeconds = 1.0f;
	const FVector OutOfRangeLocation(-2000.0f, 0.0f, 0.0f);
	auto PressLight = [](const FDefenseChainFixture& Fixture)
	{
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		return Fixture.DefenderCombat->GetCombatInputHistory().Last();
	};
	auto ReachFreedWindow = [](FDefenseChainFixture& Fixture, UPairedAnimationData*& OutCounter)
	{
		if (!Fixture.Initialize())
		{
			return false;
		}
		Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
		OutCounter = CreateChainStageData(EPairedReactionType::Counter);
		Fixture.CounterAttack->CounterData = OutCounter;
		return ReachFreedCounterWindow(Fixture, 2000);
	};

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		if (!ReachFreedWindow(Fixture, Counter))
		{
			AddError(TEXT("Failed to reach a freed CounterWindow"));
			Fixture.Destroy();
			return false;
		}
		TestFalse(TEXT("The defender is free before the counter press"), Fixture.Paired->IsInputBlocked());
		const int32 QueueBefore = Fixture.DefenderCombat->GetPendingActionCount();
		const FCombatInputRecord Record = PressLight(Fixture);
		TestEqual(TEXT("Light in the window starts the counter"),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
		TestEqual(TEXT("Light in the window takes the Chain route"), Record.Route, ECombatInputRoute::ChainOnly);
		TestEqual(TEXT("Light in the window is consumed by the counter"),
			Record.Disposition, ECombatInputDisposition::Consumed);
		TestEqual(TEXT("Light in the window never enters the normal attack queue"),
			Fixture.DefenderCombat->GetPendingActionCount(), QueueBefore);
		TestTrue(TEXT("The counter commits the defender's input again"), Fixture.Paired->IsInputBlocked());
		TestTrue(TEXT("The counter makes the defender a paired participant again"),
			Fixture.Paired->IsPairedAnimationActive());
		TestTrue(TEXT("The sequence holds the defender again"),
			Fixture.Paired->IsDefenseSequenceParticipant());
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		if (!ReachFreedWindow(Fixture, Counter))
		{
			AddError(TEXT("Failed to reach a freed CounterWindow for the failed-start case"));
			Fixture.Destroy();
			return false;
		}
		bool bFailCounterVictim = true;
		Fixture.SetPlaybackOverride([&bFailCounterVictim, Counter, NextInstanceId = 2100](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId) mutable
		{
			OutInstanceId = ++NextInstanceId;
			return !(Data == Counter && Role == EPairedAnimationRole::Victim && bFailCounterVictim);
		});
		const FCombatInputRecord Failed = PressLight(Fixture);
		TestEqual(TEXT("A counter that fails to start leaves the window open"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		TestEqual(TEXT("The failed counter press expires"),
			Failed.Disposition, ECombatInputDisposition::Expired);
		TestEqual(TEXT("The failed counter press never falls through to a normal attack"),
			Failed.Route, ECombatInputRoute::ChainOnly);
		TestFalse(TEXT("A failed counter leaves the defender free"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("A failed counter does not make the defender a paired participant"),
			Fixture.Paired->IsPairedAnimationActive());
		TestTrue(TEXT("A failed counter keeps the parried attacker in the paired victim state"),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		TestTrue(TEXT("A failed counter keeps the parried attacker's movement locked"),
			Fixture.SourceAttacker->GetCharacterMovement()->MovementMode == MOVE_None);
		bFailCounterVictim = false;
		PressLight(Fixture);
		TestEqual(TEXT("A later press in the same window still starts the counter"),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		if (!ReachFreedWindow(Fixture, Counter))
		{
			AddError(TEXT("Failed to reach a freed CounterWindow for the out-of-range case"));
			Fixture.Destroy();
			return false;
		}
		Fixture.Defender->SetActorLocation(OutOfRangeLocation);
		AddExpectedErrorPlain(TEXT("Stage preflight failed"), EAutomationExpectedErrorFlags::Contains, 1);
		const FCombatInputRecord OutOfRange = PressLight(Fixture);
		TestEqual(TEXT("A press from out of range leaves the window open until its deadline"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		TestEqual(TEXT("The out-of-range press expires"),
			OutOfRange.Disposition, ECombatInputDisposition::Expired);
		TestEqual(TEXT("The out-of-range press never falls through to a normal attack"),
			OutOfRange.Route, ECombatInputRoute::ChainOnly);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		if (!ReachFreedWindow(Fixture, Counter))
		{
			AddError(TEXT("Failed to reach a freed CounterWindow for the after-window case"));
			Fixture.Destroy();
			return false;
		}
		FTSTicker::GetCoreTicker().Tick(CounterWindowSeconds + 0.1f);
		TestEqual(TEXT("The window closes at its deadline"),
			Fixture.GetChainState(), EChainCounterState::None);
		const FCombatInputRecord AfterWindow = PressLight(Fixture);
		TestEqual(TEXT("After the window closes, Light is a normal attack again"),
			AfterWindow.Route, ECombatInputRoute::NormalQueue);
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainDefenderReactionCancelsWindowTest,
	"KatanaCombat.Defense.Chain.DefenderReactionCancelsWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainDefenderReactionCancelsWindowTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float StunSeconds = 0.3f;
	constexpr float StaggerSeconds = 0.4f;
	FScopedIntConsoleVariableOverride DefenseTelemetry(TEXT("Combat.Defense.Debug"), 1);
	struct FReactionCase
	{
		const TCHAR* Name;
		TFunction<void(const FDefenseChainFixture&)> React;
	};
	const TArray<FReactionCase> Cases = {
		{TEXT("A hit stun"), [](const FDefenseChainFixture& Fixture)
			{
				Fixture.Defender->HitReactionComponent->ApplyHitStun(StunSeconds);
			}},
		{TEXT("A hit reaction"), [](const FDefenseChainFixture& Fixture)
			{
				Fixture.Defender->HitReactionComponent->OnHitReactionStarted.Broadcast(
					EAttackDirection::Backward, false);
			}},
		{TEXT("A stagger"), [](const FDefenseChainFixture& Fixture)
			{
				Fixture.Defender->HitReactionComponent->ApplyStagger(StaggerSeconds, false);
			}},
	};

	for (const FReactionCase& Case : Cases)
	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !ReachFreedCounterWindow(Fixture, 2200))
		{
			AddError(FString::Printf(TEXT("%s: failed to reach a freed CounterWindow"), Case.Name));
			Fixture.Destroy();
			return false;
		}
		const UEnemyCombatAIComponent* SourceAI = Fixture.SourceAttacker->CombatAIComponent.Get();
		TestTrue(FString::Printf(TEXT("%s: the window suppresses the parried attacker's AI"), Case.Name),
			SourceAI && SourceAI->IsDefenseChainSuppressed());
		Fixture.DefenderCombat->ClearDefenseTelemetry();

		Case.React(Fixture);
		TestEqual(FString::Printf(TEXT("%s on the freed defender ends the window"), Case.Name),
			Fixture.GetChainState(), EChainCounterState::None);
		TestEqual(FString::Printf(TEXT("%s: cleanup records the defender's reaction"), Case.Name),
			CountDefenseCleanups(
				Fixture.DefenderCombat,
				TEXT("CounterWindow"),
				TEXT("DefenderHitReaction")),
			1);
		TestFalse(FString::Printf(TEXT("%s: the parried attacker's AI is released"), Case.Name),
			SourceAI && SourceAI->IsDefenseChainSuppressed());
		TestFalse(FString::Printf(TEXT("%s: the parried attacker leaves the paired victim state"), Case.Name),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		TestTrue(FString::Printf(TEXT("%s: the parried attacker can act again"), Case.Name),
			Fixture.SourceCombat->CanProcessInput(EInputType::LightAttack));
		TestTrue(FString::Printf(TEXT("%s: the parried attacker's movement is unlocked"), Case.Name),
			Fixture.SourceAttacker->GetCharacterMovement()->MovementMode != MOVE_None);
		TestFalse(FString::Printf(TEXT("%s: the parried attacker is no longer held"), Case.Name),
			Fixture.SourcePaired->IsDefenseSequenceParticipant());
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSourceDrivenBridgeFreesDefenderTest,
	"KatanaCombat.Defense.Chain.SourceDrivenBridgeFreesDefenderAtWindow",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSourceDrivenBridgeFreesDefenderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create the source-driven bridge fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.SetPlaybackOverride(MakeStagePlaybackOverride(2300));
	UPairedAnimationData* Bridge = CreateSourceDrivenTerminalPoseBridgeData();
	if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(Bridge))
	{
		AddError(TEXT("Failed to start a source-driven parry bridge"));
		Fixture.Destroy();
		return false;
	}

	Fixture.Paired->HandleOwnerPairedMontageEnded(Bridge->AttackerMontage, false);
	TestEqual(TEXT("A defender bridge that ends before the source's marker does not end the sequence"),
		Fixture.GetChainState(), EChainCounterState::ParryActive);
	TestTrue(TEXT("The defender stays committed until the source's marker opens the window"),
		Fixture.Paired->IsInputBlocked());

	Fixture.SourcePaired->HandleChainStageTransition(
		EChainStageTransitionType::OpenCounterWindow,
		Fixture.Paired->GetActiveDefenseSequenceContext().VictimMontageInstanceId,
		MakeMarkerSource(Bridge->VictimMontage));
	TestEqual(TEXT("The source attacker's marker opens CounterWindow"),
		Fixture.GetChainState(), EChainCounterState::CounterWindow);
	TestFalse(TEXT("The window frees a defender whose bridge already ended"),
		Fixture.Paired->IsInputBlocked());
	TestFalse(TEXT("The freed defender is not a paired participant"),
		Fixture.Paired->IsPairedAnimationActive());
	TestTrue(TEXT("The sequence still holds the source attacker"),
		Fixture.SourcePaired->IsDefenseSequenceParticipant());
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainFailedCounterStartFreesDefenderTest,
	"KatanaCombat.Defense.Chain.FailedCounterStartFreesDefenderAfterBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseChainFailedCounterStartFreesDefenderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FBridgeMontageTimings Timings;
	Timings.BridgeEnd = 0.70f;
	Timings.ReadyEnd = 0.80f;
	Timings.BlendOut = 0.25f;
	Timings.MarkerTime = 0.65f;
	constexpr float CounterWindowSeconds = 2.0f;
	constexpr float CounterLength = 0.80f;
	constexpr float FrameSeconds = 1.0f / 60.0f;
	// Long enough for a bridge still playing to finish and for every stopped montage to report its end.
	constexpr float SettleSeconds = 1.2f;
	const FVector2D MovementInput(0.0f, 1.0f);

	UAnimSequenceBase* SkeletonClip = LoadObject<UAnimSequenceBase>(nullptr,
		TEXT("/Game/Assets/Animations/DynamicKatana/AS_Parry_R_Seq.AS_Parry_R_Seq"));
	const FName AlternateSlot = FindAlternateGroupSlot(SkeletonClip ? SkeletonClip->GetSkeleton() : nullptr);
	if (AlternateSlot.IsNone())
	{
		AddError(TEXT("The production skeleton has no slot outside the default group, so the cross-group case cannot run"));
		return false;
	}
	// One rejected source-role counter start per case.
	AddExpectedErrorPlain(UnplayableMontageWarning, EAutomationExpectedErrorFlags::Contains, 2);

	// A Light press buffered during a real bridge fires at the marker. The defender's counter starts, but
	// Montage_Play rejects the source attacker's counter montage, so the start rolls back into CounterWindow.
	auto RunCase = [this, &Timings, &MovementInput](const FName CounterSlot, const TCHAR* Context)
	{
		FDefenseChainFixture Fixture;
		if (!Fixture.Initialize() || !Fixture.ConfigureProductionMeshes())
		{
			AddError(FString::Printf(TEXT("%s: failed to create the real-montage fixture"), Context));
			Fixture.Destroy();
			return false;
		}
		Fixture.EnableDefaultMovement();
		UPairedAnimationData* Bridge = CreateHeldBridgeData(Timings);
		UPairedAnimationData* Counter = CreatePlayableCounterData(CounterSlot, CounterLength);
		UPairedAnimationData* RetryCounter = CreatePlayableCounterData(CounterSlot, CounterLength);
		if (!Bridge || !Counter || !RetryCounter)
		{
			AddError(FString::Printf(TEXT("%s: failed to build playable stage montages"), Context));
			Fixture.Destroy();
			return false;
		}
		MakeMontageUnplayable(Counter->VictimMontage);
		Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
		Fixture.CounterAttack->CounterData = Counter;
		if (!Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(Bridge))
		{
			AddError(FString::Printf(TEXT("%s: failed to start the real bridge"), Context));
			Fixture.Destroy();
			return false;
		}
		Fixture.RouteAllMontageEndsThroughCombat();
		UAnimInstance* DefenderAnim = Fixture.Defender->GetMesh()->GetAnimInstance();
		UAnimInstance* SourceAnim = Fixture.SourceAttacker->GetMesh()->GetAnimInstance();

		Fixture.AdvanceMontages(Timings.MarkerTime - 0.05f, FrameSeconds);
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		const uint64 Serial = Fixture.DefenderCombat->GetCombatInputHistory().Last().Serial;
		Fixture.AdvanceMontages(0.06f, FrameSeconds);
		TestEqual(FString::Printf(TEXT("%s: the failed counter start rolls back into CounterWindow"), Context),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		const FCombatInputRecord* Record = FindInputRecord(Fixture.DefenderCombat, Serial);
		if (TestNotNull(*FString::Printf(TEXT("%s: the buffered press keeps its record"), Context), Record))
		{
			TestEqual(FString::Printf(TEXT("%s: the press whose counter failed expires"), Context),
				Record->Disposition, ECombatInputDisposition::Expired);
			TestEqual(FString::Printf(TEXT("%s: the failed press stays on the Chain route"), Context),
				Record->Route, ECombatInputRoute::ChainOnly);
		}
		TestFalse(FString::Printf(TEXT("%s: the source attacker's counter reaction never started"), Context),
			SourceAnim->Montage_IsPlaying(Counter->VictimMontage));
		TestFalse(FString::Printf(TEXT("%s: the rollback stops the defender's counter"), Context),
			DefenderAnim->Montage_IsPlaying(Counter->AttackerMontage));

		Fixture.AdvanceMontages(SettleSeconds, FrameSeconds);
		TestEqual(FString::Printf(TEXT("%s: the window stays open for its deadline"), Context),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		TestFalse(FString::Printf(TEXT("%s: no bridge montage is left on the defender"), Context),
			DefenderAnim->Montage_IsPlaying(Bridge->AttackerMontage));
		TestFalse(FString::Printf(TEXT("%s: the defender's input is free"), Context),
			Fixture.Paired->IsInputBlocked());
		TestTrue(FString::Printf(TEXT("%s: the defender's movement input is allowed"), Context),
			Fixture.DefenderCombat->SubmitMovementInput(MovementInput, FRotator::ZeroRotator));
		TestEqual(FString::Printf(TEXT("%s: the defender holds no collision or movement lease"), Context),
			Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
		TestFalse(FString::Printf(TEXT("%s: the defender is not a paired participant"), Context),
			Fixture.Paired->IsPairedAnimationActive());
		TestFalse(FString::Printf(TEXT("%s: the sequence no longer holds the defender"), Context),
			Fixture.Paired->IsDefenseSequenceParticipant());
		TestTrue(FString::Printf(TEXT("%s: the source attacker keeps its held bridge pose"), Context),
			SourceAnim->Montage_IsPlaying(Bridge->VictimMontage));
		TestTrue(FString::Printf(TEXT("%s: the source attacker stays in the paired victim state"), Context),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		TestTrue(FString::Printf(TEXT("%s: every retired role-montage callback is resolved"), Context),
			Fixture.HasNoPendingRoleMontageCallbacks());

		Fixture.CounterAttack->CounterData = RetryCounter;
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		TestEqual(FString::Printf(TEXT("%s: a later press in the same window starts the counter"), Context),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
		TestTrue(FString::Printf(TEXT("%s: the retried counter plays on the source attacker"), Context),
			SourceAnim->Montage_IsPlaying(RetryCounter->VictimMontage));
		Fixture.AdvanceMontages(0.3f, FrameSeconds);
		TestTrue(FString::Printf(TEXT("%s: the retried counter leaves no unresolved role-montage callback"), Context),
			Fixture.HasNoPendingRoleMontageCallbacks());
		Fixture.Destroy();
		return true;
	};

	if (!RunCase(FAnimSlotGroup::DefaultSlotName, TEXT("Same montage group")))
	{
		return false;
	}
	return RunCase(AlternateSlot, TEXT("Different montage group"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainFailedAutoFinisherStartFreesDefenderTest,
	"KatanaCombat.Defense.Chain.FailedAutoFinisherStartFreesDefender",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseChainFailedAutoFinisherStartFreesDefenderTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float StageLength = 0.80f;
	constexpr float AutoContinueMarkerTime = 0.40f;
	constexpr float FinisherReadySeconds = 2.0f;
	constexpr float FrameSeconds = 1.0f / 60.0f;
	constexpr float SettleSeconds = 1.0f;
	const FName MarkerName(TEXT("FinisherReady"));
	const FVector2D MovementInput(0.0f, 1.0f);
	AddExpectedErrorPlain(UnplayableMontageWarning, EAutomationExpectedErrorFlags::Contains, 1);

	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize() || !Fixture.ConfigureProductionMeshes())
	{
		AddError(TEXT("Failed to create the real-montage fixture"));
		Fixture.Destroy();
		return false;
	}
	Fixture.EnableDefaultMovement();
	UPairedAnimationData* Counter = CreatePlayableCounterData(FAnimSlotGroup::DefaultSlotName, StageLength);
	UPairedAnimationData* Finisher = CreatePlayableFinisherData(FAnimSlotGroup::DefaultSlotName, StageLength);
	UPairedAnimationData* PlayableFinisher =
		CreatePlayableFinisherData(FAnimSlotGroup::DefaultSlotName, StageLength);
	if (!Counter || !Finisher || !PlayableFinisher)
	{
		AddError(TEXT("Failed to build playable stage montages"));
		Fixture.Destroy();
		return false;
	}
	AddChainStageMarker(
		Counter->AttackerMontage,
		EChainStageTransitionType::AutoContinue,
		MarkerName,
		AutoContinueMarkerTime);
	Counter->ChainTransitionPolicy.RequiredMarker = MarkerName;
	Counter->ChainTransitionPolicy.bAutoContinue = true;
	Counter->ChainTransitionPolicy.bFinisherRetryable = true;
	MakeMontageUnplayable(Finisher->VictimMontage);
	Fixture.CounterAttack->CounterData = Counter;
	Fixture.CounterAttack->FinisherData = Finisher;
	Fixture.DefenseConfig->FinisherReadySeconds = FinisherReadySeconds;
	if (!Fixture.StartCommittedParry() || !Fixture.OpenCounterWindow())
	{
		AddError(TEXT("Failed to reach CounterWindow through the no-montage bridge"));
		Fixture.Destroy();
		return false;
	}
	Fixture.RouteAllMontageEndsThroughCombat();
	UAnimInstance* DefenderAnim = Fixture.Defender->GetMesh()->GetAnimInstance();
	UAnimInstance* SourceAnim = Fixture.SourceAttacker->GetMesh()->GetAnimInstance();

	Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	TestEqual(TEXT("Light in the window starts the real counter"),
		Fixture.GetChainState(), EChainCounterState::CounterActive);
	TestTrue(TEXT("The defender plays its counter"), DefenderAnim->Montage_IsPlaying(Counter->AttackerMontage));

	// The counter's real marker starts the finisher automatically. The defender's finisher montage starts (and
	// stops the counter in its group), but Montage_Play rejects the source attacker's, so the start rolls back
	// and the retryable counter waits in FinisherReady.
	Fixture.AdvanceMontages(AutoContinueMarkerTime + 0.05f, FrameSeconds);
	TestEqual(TEXT("The failed automatic finisher leaves a retryable FinisherReady"),
		Fixture.GetChainState(), EChainCounterState::FinisherReady);
	TestFalse(TEXT("The defender's finisher start stopped its counter montage"),
		DefenderAnim->Montage_IsPlaying(Counter->AttackerMontage));
	TestFalse(TEXT("The rollback stops the defender's finisher"),
		DefenderAnim->Montage_IsPlaying(Finisher->AttackerMontage));
	TestFalse(TEXT("The source attacker's finisher reaction never started"),
		SourceAnim->Montage_IsPlaying(Finisher->VictimMontage));

	Fixture.AdvanceMontages(SettleSeconds, FrameSeconds);
	TestEqual(TEXT("FinisherReady stays open for its deadline"),
		Fixture.GetChainState(), EChainCounterState::FinisherReady);
	TestFalse(TEXT("The defender's input is free"), Fixture.Paired->IsInputBlocked());
	TestTrue(TEXT("The defender's movement input is allowed"),
		Fixture.DefenderCombat->SubmitMovementInput(MovementInput, FRotator::ZeroRotator));
	TestEqual(TEXT("The defender holds no collision or movement lease"),
		Fixture.Paired->GetActivePairedStateLeaseCount(), 0);
	TestFalse(TEXT("The defender is not a paired participant"), Fixture.Paired->IsPairedAnimationActive());
	TestFalse(TEXT("The sequence no longer holds the defender"), Fixture.Paired->IsDefenseSequenceParticipant());
	TestTrue(TEXT("The source attacker stays in the paired victim state"),
		Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
	TestTrue(TEXT("The sequence still holds the source attacker"),
		Fixture.SourcePaired->IsDefenseSequenceParticipant());
	TestTrue(TEXT("Every retired role-montage callback is resolved"), Fixture.HasNoPendingRoleMontageCallbacks());

	Finisher->VictimMontage = PlayableFinisher->VictimMontage;
	Fixture.DefenderCombat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
	TestEqual(TEXT("A press in FinisherReady retries the finisher"),
		Fixture.GetChainState(), EChainCounterState::FinisherActive);
	TestTrue(TEXT("The retried finisher commits the defender again"), Fixture.Paired->IsInputBlocked());
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainBridgeDefenderMontageMustEndTest,
	"KatanaCombat.Defense.Chain.BridgeDefenderMontageMustEndOnItsOwn",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainBridgeDefenderMontageMustEndTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseChainFixture Fixture;
	if (!Fixture.Initialize())
	{
		AddError(TEXT("Failed to create the bridge preflight fixture"));
		Fixture.Destroy();
		return false;
	}
	// Montage playback is not under test: the override stands in for the participants' anim instances.
	Fixture.SetPlaybackOverride(MakeStagePlaybackOverride(2700));
	if (!Fixture.StartCommittedParry())
	{
		AddError(TEXT("Failed to start a committed parry for bridge preflight"));
		Fixture.Destroy();
		return false;
	}
	struct FBridgeCase
	{
		const TCHAR* Name;
		UPairedAnimationData* Bridge;
	};
	const FBridgeCase Cases[] = {
		{TEXT("Defender-driven bridge"), CreateTerminalPoseBridgeData()},
		{TEXT("Source-driven bridge"), CreateSourceDrivenTerminalPoseBridgeData()},
	};
	for (const FBridgeCase& Case : Cases)
	{
		FString Reason;
		const bool bAcceptedWithBlendOut =
			Fixture.PreflightStage(Case.Bridge, EPairedReactionType::Parry, Reason);
		TestTrue(FString::Printf(TEXT("%s: a defender montage that blends out on its own passes preflight (%s)"),
				Case.Name, *Reason),
			bAcceptedWithBlendOut);

		// Without auto blend-out the defender's bridge holds its last frame and never ends, so the defender is
		// never freed and a missed marker is never caught.
		Case.Bridge->AttackerMontage->bEnableAutoBlendOut = false;
		const bool bAcceptedWithoutBlendOut =
			Fixture.PreflightStage(Case.Bridge, EPairedReactionType::Parry, Reason);
		TestFalse(FString::Printf(TEXT("%s: a defender montage that never ends is rejected"), Case.Name),
			bAcceptedWithoutBlendOut);
		TestTrue(FString::Printf(TEXT("%s: the rejection names the disabled auto blend-out (%s)"), Case.Name, *Reason),
			Reason.Contains(TEXT("auto blend-out")));

		// The source attacker's montage is held in its ready pose until the window resolves, so it may hold.
		Case.Bridge->AttackerMontage->bEnableAutoBlendOut = true;
		Case.Bridge->VictimMontage->bEnableAutoBlendOut = false;
		const bool bAcceptedHeldSource =
			Fixture.PreflightStage(Case.Bridge, EPairedReactionType::Parry, Reason);
		TestTrue(FString::Printf(TEXT("%s: the source attacker's montage may disable auto blend-out (%s)"),
				Case.Name, *Reason),
			bAcceptedHeldSource);
	}
	Fixture.Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainFinisherInputDuringCounterIsBufferedTest,
	"KatanaCombat.Defense.Chain.FinisherInputDuringCounterIsBuffered",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseChainFinisherInputDuringCounterIsBufferedTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	constexpr float FinisherReadySeconds = 1.0f;
	const FName MarkerName(TEXT("FinisherReady"));
	struct FCounterSetup
	{
		bool bAutoContinue = false;
		bool bWithFinisher = true;
	};
	// Reach CounterActive from a freed CounterWindow: the counter commits the defender again.
	auto ReachCounterActive = [&MarkerName](
		FDefenseChainFixture& Fixture,
		const FCounterSetup Setup,
		const int32 FirstInstanceId,
		UPairedAnimationData*& OutCounter,
		UPairedAnimationData*& OutFinisher)
	{
		if (!Fixture.Initialize())
		{
			return false;
		}
		Fixture.DefenseConfig->FinisherReadySeconds = FinisherReadySeconds;
		OutCounter = Setup.bAutoContinue
			? CreateChainStageData(
				EPairedReactionType::Counter,
				EChainStageTransitionType::AutoContinue,
				MarkerName,
				true)
			: CreateChainStageData(EPairedReactionType::Counter);
		OutFinisher = Setup.bWithFinisher ? CreateChainStageData(EPairedReactionType::Finisher) : nullptr;
		Fixture.CounterAttack->CounterData = OutCounter;
		Fixture.CounterAttack->FinisherData = OutFinisher;
		if (!ReachFreedCounterWindow(Fixture, FirstInstanceId))
		{
			return false;
		}
		Fixture.DefenderCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		return Fixture.GetChainState() == EChainCounterState::CounterActive
			&& Fixture.Paired->IsInputBlocked();
	};
	auto Press = [](const FDefenseChainFixture& Fixture, const EInputType InputType)
	{
		Fixture.DefenderCombat->OnInputEvent(InputType, EInputEventType::Press);
		return Fixture.DefenderCombat->GetCombatInputHistory().Last().Serial;
	};
	auto ExpectRecord = [this](
		const FDefenseChainFixture& Fixture,
		const uint64 Serial,
		const ECombatInputRoute Route,
		const ECombatInputDisposition Disposition,
		const TCHAR* What)
	{
		const FCombatInputRecord* Record = FindInputRecord(Fixture.DefenderCombat, Serial);
		if (TestNotNull(*FString::Printf(TEXT("%s: the press keeps its record"), What), Record))
		{
			TestEqual(FString::Printf(TEXT("%s: route"), What), Record->Route, Route);
			TestEqual(FString::Printf(TEXT("%s: disposition"), What), Record->Disposition, Disposition);
		}
	};
	auto DeliverAutoContinueMarker = [](const FDefenseChainFixture& Fixture, const UPairedAnimationData* Counter)
	{
		Fixture.Paired->HandleChainStageTransition(
			EChainStageTransitionType::AutoContinue,
			Fixture.Paired->GetActiveDefenseSequenceContext().AttackerMontageInstanceId,
			MakeMarkerSource(Counter->AttackerMontage));
	};

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		UPairedAnimationData* Finisher = nullptr;
		if (!ReachCounterActive(Fixture, FCounterSetup(), 2400, Counter, Finisher))
		{
			AddError(TEXT("Failed to reach CounterActive for the newest-press case"));
			Fixture.Destroy();
			return false;
		}
		const int32 QueueBefore = Fixture.DefenderCombat->GetPendingActionCount();
		const uint64 HeavySerial = Press(Fixture, EInputType::HeavyAttack);
		const uint64 LightSerial = Press(Fixture, EInputType::LightAttack);
		TestEqual(TEXT("A press during the counter does not cut the counter short"),
			Fixture.GetChainState(), EChainCounterState::CounterActive);
		ExpectRecord(Fixture, HeavySerial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Replaced,
			TEXT("The older press during the counter is replaced"));
		ExpectRecord(Fixture, LightSerial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Queued,
			TEXT("The newest press during the counter is buffered"));
		TestEqual(TEXT("A press during the counter never enters the normal attack queue"),
			Fixture.DefenderCombat->GetPendingActionCount(), QueueBefore);

		Fixture.Paired->HandleOwnerPairedMontageEnded(Counter->AttackerMontage, false);
		TestEqual(TEXT("The buffered press starts the finisher as FinisherReady opens"),
			Fixture.GetChainState(), EChainCounterState::FinisherActive);
		ExpectRecord(Fixture, LightSerial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Consumed,
			TEXT("The buffered press is consumed by the finisher"));
		TestEqual(TEXT("Releasing the buffer never enters the normal attack queue"),
			Fixture.DefenderCombat->GetPendingActionCount(), QueueBefore);
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		UPairedAnimationData* Finisher = nullptr;
		if (!ReachCounterActive(Fixture, FCounterSetup(), 2450, Counter, Finisher))
		{
			AddError(TEXT("Failed to reach CounterActive for the cleanup case"));
			Fixture.Destroy();
			return false;
		}
		const uint64 Serial = Press(Fixture, EInputType::LightAttack);
		ExpectRecord(Fixture, Serial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Queued,
			TEXT("A press during the counter is buffered"));
		Fixture.Paired->HandleOwnerPairedMontageEnded(Counter->AttackerMontage, true);
		TestEqual(TEXT("An interrupted counter ends the sequence"),
			Fixture.GetChainState(), EChainCounterState::None);
		ExpectRecord(Fixture, Serial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Expired,
			TEXT("Cleanup expires the buffered press"));
		const uint64 After = Press(Fixture, EInputType::LightAttack);
		const FCombatInputRecord* AfterRecord = FindInputRecord(Fixture.DefenderCombat, After);
		if (TestNotNull(TEXT("The press after the sequence keeps its record"), AfterRecord))
		{
			TestEqual(TEXT("After the sequence ends a press takes the normal route, not a stale Chain response"),
				AfterRecord->Route, ECombatInputRoute::NormalQueue);
		}
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		UPairedAnimationData* Finisher = nullptr;
		FCounterSetup Setup;
		Setup.bAutoContinue = true;
		if (!ReachCounterActive(Fixture, Setup, 2500, Counter, Finisher))
		{
			AddError(TEXT("Failed to reach an automatically continuing CounterActive"));
			Fixture.Destroy();
			return false;
		}
		const uint64 Serial = Press(Fixture, EInputType::LightAttack);
		ExpectRecord(Fixture, Serial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Queued,
			TEXT("A press during an automatically continuing counter is buffered"));
		DeliverAutoContinueMarker(Fixture, Counter);
		TestEqual(TEXT("The counter's marker starts the finisher by itself"),
			Fixture.GetChainState(), EChainCounterState::FinisherActive);
		ExpectRecord(Fixture, Serial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Expired,
			TEXT("A finisher that starts without the press expires it"));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		UPairedAnimationData* Finisher = nullptr;
		FCounterSetup Setup;
		Setup.bAutoContinue = true;
		if (!ReachCounterActive(Fixture, Setup, 2550, Counter, Finisher))
		{
			AddError(TEXT("Failed to reach an automatically continuing CounterActive for the retry case"));
			Fixture.Destroy();
			return false;
		}
		bool bFailFinisherVictimOnce = true;
		Fixture.SetPlaybackOverride([&bFailFinisherVictimOnce, Finisher, NextInstanceId = 2580](
			const EPairedAnimationRole Role,
			const UPairedAnimationData* Data,
			int32& OutInstanceId) mutable
		{
			OutInstanceId = ++NextInstanceId;
			if (Data == Finisher && Role == EPairedAnimationRole::Victim && bFailFinisherVictimOnce)
			{
				bFailFinisherVictimOnce = false;
				return false;
			}
			return true;
		});
		const uint64 Serial = Press(Fixture, EInputType::LightAttack);
		DeliverAutoContinueMarker(Fixture, Counter);
		TestFalse(TEXT("The automatic finisher start failed once"), bFailFinisherVictimOnce);
		TestEqual(TEXT("The buffered press retries the finisher as FinisherReady opens"),
			Fixture.GetChainState(), EChainCounterState::FinisherActive);
		ExpectRecord(Fixture, Serial, ECombatInputRoute::ChainOnly, ECombatInputDisposition::Consumed,
			TEXT("The buffered press is consumed by the retried finisher"));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Counter = nullptr;
		UPairedAnimationData* Finisher = nullptr;
		FCounterSetup Setup;
		Setup.bWithFinisher = false;
		if (!ReachCounterActive(Fixture, Setup, 2600, Counter, Finisher))
		{
			AddError(TEXT("Failed to reach CounterActive without a finisher"));
			Fixture.Destroy();
			return false;
		}
		const int32 QueueBefore = Fixture.DefenderCombat->GetPendingActionCount();
		const uint64 Serial = Press(Fixture, EInputType::LightAttack);
		ExpectRecord(Fixture, Serial, ECombatInputRoute::NormalQueue, ECombatInputDisposition::Rejected,
			TEXT("A counter with no finisher to reach keeps rejecting presses"));
		TestEqual(TEXT("The rejected press never enters the normal attack queue"),
			Fixture.DefenderCombat->GetPendingActionCount(), QueueBefore);
		Fixture.Destroy();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseChainSourceDrivenRealBridgeTest,
	"KatanaCombat.Defense.Chain.SourceDrivenRealBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseChainSourceDrivenRealBridgeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FBridgeMontageTimings Timings;
	Timings.BridgeEnd = 0.70f;
	Timings.ReadyEnd = 0.80f;
	Timings.BlendOut = 0.25f;
	Timings.MarkerTime = 0.65f;
	constexpr float CounterWindowSeconds = 2.0f;
	constexpr float FrameSeconds = 1.0f / 60.0f;
	FScopedIntConsoleVariableOverride DefenseTelemetry(TEXT("Combat.Defense.Debug"), 1);
	auto StartSourceDrivenBridge = [&Timings](FDefenseChainFixture& Fixture, UPairedAnimationData*& OutBridge)
	{
		OutBridge = nullptr;
		if (!Fixture.Initialize() || !Fixture.ConfigureProductionMeshes())
		{
			return false;
		}
		Fixture.DefenseConfig->CounterWindowSeconds = CounterWindowSeconds;
		Fixture.EnableDefaultMovement();
		OutBridge = CreateHeldBridgeData(Timings, EPairedAnimationRole::Victim);
		if (!OutBridge || !Fixture.StartCommittedParry() || !Fixture.StartBridgeStage(OutBridge))
		{
			return false;
		}
		Fixture.RouteAllMontageEndsThroughCombat();
		return true;
	};

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Bridge = nullptr;
		if (!StartSourceDrivenBridge(Fixture, Bridge))
		{
			AddError(TEXT("Failed to start a real source-driven bridge"));
			Fixture.Destroy();
			return false;
		}
		Fixture.AdvanceMontages(Timings.MarkerTime + 0.01f, FrameSeconds);
		TestEqual(TEXT("The source attacker's real marker opens CounterWindow"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		Fixture.AdvanceMontages(1.0f, FrameSeconds);
		TestEqual(TEXT("The source attacker's hold does not end the open window"),
			Fixture.GetChainState(), EChainCounterState::CounterWindow);
		TestFalse(TEXT("The defender is freed once its bridge has ended"), Fixture.Paired->IsInputBlocked());
		TestEqual(TEXT("The source attacker holds CounterReady"),
			Fixture.SourceAttacker->GetMesh()->GetAnimInstance()->Montage_GetCurrentSection(Bridge->VictimMontage),
			FName(TEXT("CounterReady")));
		Fixture.Destroy();
	}

	{
		FDefenseChainFixture Fixture;
		UPairedAnimationData* Bridge = nullptr;
		if (!StartSourceDrivenBridge(Fixture, Bridge))
		{
			AddError(TEXT("Failed to start a real source-driven bridge for the missed-marker case"));
			Fixture.Destroy();
			return false;
		}
		Fixture.InvalidateSourceBridgeMarkerIdentity();
		Fixture.DefenderCombat->ClearDefenseTelemetry();
		Fixture.AdvanceMontages(Timings.BridgeEnd - 0.03f, FrameSeconds);
		TestEqual(TEXT("Before the source attacker reaches its hold the bridge is still ParryActive"),
			Fixture.GetChainState(), EChainCounterState::ParryActive);
		Fixture.AdvanceMontages(0.1f, FrameSeconds);
		TestEqual(TEXT("A source attacker that reaches its hold without opening CounterWindow ends the sequence"),
			Fixture.GetChainState(), EChainCounterState::None);
		TestEqual(TEXT("The missed marker is reported as a bridge ending before its window"),
			CountDefenseCleanups(
				Fixture.DefenderCombat,
				TEXT("ParryActive"),
				TEXT("BridgeEndedBeforeCounter")),
			1);
		TestFalse(TEXT("The missed-marker cleanup releases the defender's input"), Fixture.Paired->IsInputBlocked());
		TestFalse(TEXT("The missed-marker cleanup releases the source attacker"),
			Fixture.SourceAttacker->HitReactionComponent->IsInPairedAnimationState());
		Fixture.Destroy();
	}
	return true;
}
