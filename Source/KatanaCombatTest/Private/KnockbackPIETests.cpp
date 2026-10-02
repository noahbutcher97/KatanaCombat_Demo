#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/HitReactionSettings.h"
#include "Debug/ActionReactionTelemetry.h"
#include "Interfaces/DamageableInterface.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"
#include "Utilities/KnockbackResolution.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "HAL/IConsoleManager.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "UObject/GarbageCollection.h"
#include "UObject/ReferenceChainSearch.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"

namespace
{
const TCHAR* KnockbackMap = TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1");
const TCHAR* LightAttackPath = TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1");
const TCHAR* HeavyAttackPath = TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Heavy/New/HeavyAttack_1.HeavyAttack_1");
const FName KnockbackOwner(TEXT("HitKnockback"));

/** The transition runs act this long after the hit: inside both the Light (0.2 s) and the Heavy (0.25 s) push. */
constexpr double ManipulationDelay = 0.08;
/** The section-end run raises the reaction's play rate so its section ends about this long after the hit. */
constexpr double SectionEndTarget = 0.1;
/** A push's terminal row arrives within its duration (plus any hitstop that froze it) and this margin of the hit. */
constexpr double TerminalRowMargin = 0.1;

/** What a run does to the reaction after the hit. Both runs of a pair do the same, so their difference is the push. */
enum class ERunManipulation : uint8
{
	None,
	/** ApplyHitstop right after ApplyDamage, as BaseCombatCharacter does for every hit in play. */
	Hitstop,
	/** Montage_Play another root-motion reaction mid-push: animation channel to animation channel. */
	ReplaceMontage,
	/** StopAllMontages(0) mid-push: animation channel to movement channel. */
	StopMontages,
	/**
	 * Raise the reaction's play rate, with the instance's auto blend-out off, so its section ends mid-push and it
	 * holds its last pose: still the root-motion montage, no longer playing. That is the state the executor's
	 * advancing-montage gate exists for. The stock auto blend-out never reaches it: it calls Stop(), which clears
	 * the root-motion montage when the blend-out starts, so the push already moves to the movement channel then.
	 */
	SectionHolds
};

bool IsTransition(const ERunManipulation Manipulation)
{
	return Manipulation == ERunManipulation::ReplaceMontage || Manipulation == ERunManipulation::StopMontages
		|| Manipulation == ERunManipulation::SectionHolds;
}

/** Runs whose push moves from the animation channel to the movement channel while it runs. */
bool HandsOffToMovement(const ERunManipulation Manipulation)
{
	return Manipulation == ERunManipulation::StopMontages || Manipulation == ERunManipulation::SectionHolds;
}

/** Transition runs trace every update up to this long after the hit. */
constexpr double TraceSeconds = 0.45;

FString ManipulationName(const ERunManipulation Manipulation)
{
	switch (Manipulation)
	{
	case ERunManipulation::Hitstop: return TEXT("hitstop");
	case ERunManipulation::ReplaceMontage: return TEXT("montage_replaced");
	case ERunManipulation::StopMontages: return TEXT("montage_stopped");
	case ERunManipulation::SectionHolds: return TEXT("section_holds");
	default: return TEXT("none");
	}
}

/** Turns Combat.ActionReaction.Debug on for its lifetime and restores the previous value on every exit. */
struct FScopedActionReactionTelemetry
{
	IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.ActionReaction.Debug"));
	int32 Previous = Variable ? Variable->GetInt() : 0;
	FScopedActionReactionTelemetry() { if (Variable) { Variable->Set(1, ECVF_SetByCode); } }
	~FScopedActionReactionTelemetry() { if (Variable) { Variable->Set(Previous, ECVF_SetByCode); } }
	FScopedActionReactionTelemetry(const FScopedActionReactionTelemetry&) = delete;
	FScopedActionReactionTelemetry& operator=(const FScopedActionReactionTelemetry&) = delete;
};

// NaN-safe comparisons. This build compares NaN as equal to anything (TestEqual(NaN, 60) passes), so every
// assertion on a double checks FMath::IsFinite, which tests the bits, before it compares.

/** A finite value within a tolerance of the expected one. */
bool IsFiniteAndNear(const double Value, const double Expected, const double Tolerance)
{
	return FMath::IsFinite(Value) && FMath::IsFinite(Expected) && FMath::Abs(Value - Expected) <= Tolerance;
}

/** A finite value strictly above the bound. */
bool IsFiniteAbove(const double Value, const double Bound)
{
	return FMath::IsFinite(Value) && FMath::IsFinite(Bound) && Value > Bound;
}

/** A finite value within [Low, High]. */
bool IsFiniteWithin(const double Value, const double Low, const double High)
{
	return FMath::IsFinite(Value) && Value >= Low && Value <= High;
}

bool IsTerminalDisposition(const FString& Disposition)
{
	return Disposition == TEXT("Reached") || Disposition == TEXT("Blocked")
		|| Disposition == TEXT("Invalid") || Disposition == TEXT("Cancelled");
}

struct FMeasuredRun
{
	/** The pair's JSON key and log label, for example light_montage_replaced. */
	FString Name;
	FString Type;
	const TCHAR* AttackPath = nullptr;
	bool bPush = false;
	ERunManipulation Manipulation = ERunManipulation::None;

	/** The push the game's own knockback rules resolve for this attack at the hit. */
	float ResolvedPush = 0.0f;
	float ResolvedDuration = 0.0f;
	EDisplacementAnimationBlend ResolvedBlend = EDisplacementAnimationBlend::AddToAnimation;

	/** Travel along the push direction from the hit location: the largest sample, and the value once the run has settled. */
	double PeakTravel = 0.0;
	double NetTravel = 0.0;
	/** Seconds from the hit until the tracked reaction stopped extracting root motion. */
	double RootMotionDuration = 0.0;
	/** Velocity along the push direction and travel when that root motion ended, so a residual slide is visible. */
	double VelocityAtRootMotionEnd = 0.0;
	double TravelAtRootMotionEnd = 0.0;
	/** Seconds from the hit until the push stopped running (its terminal row), and the travel then. */
	double PushEndTime = -1.0;
	double TravelAtPushEnd = 0.0;
	/** The smallest horizontal gap between the enemy's and the player's capsules. */
	double MinClearance = TNumericLimits<double>::Max();
	FString Montage;
	FString Section;
	bool bRootMotionAtHit = false;

	/** The manipulation, when it happened, and whether its premise held (it really did what the run claims). */
	FString ManipulationNote;
	double ManipulationTime = -1.0;
	bool bManipulationPremise = false;
	double VelocityAtManipulation = 0.0;
	/** Montage-stopped runs: world time between the stop and the next update, the frame that moves on leftover velocity. */
	double CarryFrameDelta = 0.0;
	bool bPushRunningAtManipulation = false;
	double HitstopDuration = 0.0;
	float PlayRate = 1.0f;

	/** Section-end runs: when the reaction stopped playing, and the push clock sampled every update while it ran. */
	double SectionEndTime = -1.0;
	bool bPushRunningAtSectionEnd = false;
	/** Whether the reaction was still the root-motion montage (IsPlayingRootMotion) when it stopped playing. */
	bool bRootMotionFlagAtSectionEnd = false;
	int32 RunningSamplesAfterSectionEnd = 0;
	int32 ElapsedSamples = 0;
	int32 ElapsedStalls = 0;
	double MinElapsedRate = TNumericLimits<double>::Max();
	FString FirstStall;

	/** The victim's HitKnockback telemetry rows for this run. */
	FString KnockbackRows;
	int32 StartedRows = 0;
	int32 TerminalRows = 0;
	FString TerminalDisposition = TEXT("None");
	FString TerminalDetail;
	/** Seconds from the hit (the Started row) to the terminal row; negative without one. */
	double TerminalTime = -1.0;
	bool bTelemetryFull = false;

	/** Transition runs: one line per update for the first TraceSeconds after the hit. */
	TArray<FString> Trace;
};

class FKnockbackMeasurementCommand final : public IAutomationLatentCommand
{
public:
	explicit FKnockbackMeasurementCommand(FAutomationTestBase* InTest)
		: Test(InTest), CommandStart(FPlatformTime::Seconds())
	{
		// Control then push for each pair: the difference is the push, the control is the reaction's own travel.
		AddPair(TEXT("light"), TEXT("light"), LightAttackPath, ERunManipulation::None);
		AddPair(TEXT("heavy"), TEXT("heavy"), HeavyAttackPath, ERunManipulation::None);

		// Every hit in play freezes both fighters right after the damage, so the push starts under hitstop.
		const UAttackData* Light = LoadObject<UAttackData>(nullptr, LightAttackPath);
		const UAttackData* Heavy = LoadObject<UAttackData>(nullptr, HeavyAttackPath);
		if (Light && Light->HitstopConfig.IsActive())
		{
			AddPair(TEXT("light_hitstop"), TEXT("light"), LightAttackPath, ERunManipulation::Hitstop);
		}
		else if (Heavy && Heavy->HitstopConfig.IsActive())
		{
			AddPair(TEXT("heavy_hitstop"), TEXT("heavy"), HeavyAttackPath, ERunManipulation::Hitstop);
		}
		else
		{
			Test->AddInfo(TEXT("Hitstop run skipped: neither the Light nor the Heavy attack has an active HitstopConfig"));
		}

		AddPair(TEXT("light_montage_replaced"), TEXT("light"), LightAttackPath, ERunManipulation::ReplaceMontage);
		AddPair(TEXT("light_montage_stopped"), TEXT("light"), LightAttackPath, ERunManipulation::StopMontages);
		// The longest push, so the most of it runs after the section has ended and the reaction holds its pose.
		AddPair(TEXT("heavy_section_holds"), TEXT("heavy"), HeavyAttackPath, ERunManipulation::SectionHolds);
	}

	virtual bool Update() override
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!bReady)
		{
			if (!Telemetry.Variable)
			{
				Test->AddError(TEXT("Combat.ActionReaction.Debug is not registered, so the push outcome rows cannot be read"));
				return true;
			}
			if (World && FindFixture(World))
			{
				bReady = true;
			}
			else if (bFailed || FPlatformTime::Seconds() - CommandStart > 20.0)
			{
				if (!bFailed)
				{
					Test->AddError(TEXT("PIE did not provide a player and an enemy within 20 seconds"));
				}
				return true;
			}
			return false;
		}
		if (!World || !Player.IsValid() || !Enemy.IsValid())
		{
			Test->AddError(TEXT("Fixture actors were destroyed during the measurement"));
			return true;
		}
		if (RunIndex >= Runs.Num())
		{
			Report();
			return true;
		}

		// Each run: place the enemy and let it settle, hit it, then sample it every update until both the reaction's
		// root motion and the push have ended, plus a short margin for the last movement to land.
		const double Now = World->GetTimeSeconds();
		FMeasuredRun& Run = Runs[RunIndex];
		if (Stage == 0)
		{
			Place(Run);
			StageStart = Now;
			Stage = 1;
		}
		else if (Stage == 1 && Now - StageStart >= 0.5)
		{
			Hit(Run, World);
			StageStart = Now;
			Stage = 2;
		}
		else if (Stage == 2)
		{
			Sample(Run, Now);
		}
		return false;
	}

private:
	void AddPair(const TCHAR* Name, const TCHAR* Type, const TCHAR* AttackPath, const ERunManipulation Manipulation)
	{
		for (const bool bPush : {false, true})
		{
			FMeasuredRun& Run = Runs.AddDefaulted_GetRef();
			Run.Name = Name;
			Run.Type = Type;
			Run.AttackPath = AttackPath;
			Run.bPush = bPush;
			Run.Manipulation = Manipulation;
		}
	}

	bool FindFixture(UWorld* World)
	{
		TArray<AEnemyCharacter*> Enemies;
		for (TActorIterator<APlayerCharacter> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Player = *It;
				break;
			}
		}
		for (TActorIterator<AEnemyCharacter> It(World); It; ++It)
		{
			if (IsValid(*It))
			{
				Enemies.Add(*It);
			}
		}
		if (!Player.IsValid() || Enemies.IsEmpty())
		{
			return false;
		}
		Enemies.Sort([](const AEnemyCharacter& Left, const AEnemyCharacter& Right) { return Left.GetName() < Right.GetName(); });
		Enemy = Enemies[0];
		PlayerStart = Player->GetActorLocation();
		Forward = Player->GetActorForwardVector().GetSafeNormal2D();
		for (int32 Index = 0; Index < Enemies.Num(); ++Index)
		{
			FreezeAI(Enemies[Index]);
			if (Enemies[Index] != Enemy.Get())
			{
				Enemies[Index]->SetActorLocation(PlayerStart + FVector(1800.0 + Index * 150.0, 1000.0, 0.0));
			}
		}
		if (!Enemy->GetCombatComponent() || !Enemy->GetTargetingComponent())
		{
			Test->AddError(TEXT("The enemy has no combat or targeting component"));
			bFailed = true;
			return false;
		}

		// Transient settings copies, so the saved DA_HitReaction is never modified in PIE.
		UHitReactionSettings* Base = Enemy->HitReactionComponent->GetEffectiveSettings();
		if (!Base)
		{
			Test->AddError(TEXT("The enemy has no hit reaction settings"));
			bFailed = true;
			return false;
		}
		PushSettings.Reset(DuplicateObject<UHitReactionSettings>(Base, GetTransientPackage()));
		ControlSettings.Reset(DuplicateObject<UHitReactionSettings>(Base, GetTransientPackage()));
		PushSettings->KnockbackScale = 1.0f;
		ControlSettings->KnockbackScale = 0.0f;

		// The replacement for the montage-replaced run: another reaction DA_HitReaction references (Light, Back).
		if (const FHitReactionEntry* Back = Base->GetDirectionalReaction(EHitIntensity::Light, EAttackDirection::Backward))
		{
			if (Back->ReactionMontage)
			{
				ReplacementMontage = Back->ReactionMontage.Get();
				ReplacementSection = Back->MontageSection;
			}
			else if (!Back->ReactionMontages.IsEmpty())
			{
				ReplacementMontage = Back->ReactionMontages[0].Montage.Get();
				ReplacementSection = Back->ReactionMontages[0].MontageSection;
			}
			ReplacementPlayRate = Back->PlayRate;
		}
		return true;
	}

	/** Gate A's pattern: keep the controller possessing (movement keeps running) but stop its logic. */
	static void FreezeAI(AEnemyCharacter* Target)
	{
		if (UEnemyCombatAIComponent* AI = Target->CombatAIComponent.Get())
		{
			AI->CancelQueuedAttackRequest();
			AI->SetCombatTarget(nullptr);
		}
		if (AController* Controller = Target->GetController())
		{
			Controller->SetActorTickEnabled(false);
			TArray<UActorComponent*> Components;
			Controller->GetComponents(Components);
			for (UActorComponent* Component : Components)
			{
				if (Component)
				{
					Component->SetComponentTickEnabled(false);
				}
			}
		}
	}

	UAnimInstance* EnemyAnim() const
	{
		return Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
	}

	void Place(const FMeasuredRun& Run)
	{
		// Gate A's reset: end the previous reaction so its blend-out and leftover velocity do not carry into this run.
		if (UAnimInstance* Anim = EnemyAnim())
		{
			Anim->StopAllMontages(0.0f);
		}
		if (UCharacterMovementComponent* Movement = Enemy->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
		const FRotator FacePlayer = (-Forward).Rotation();
		Enemy->SetActorLocationAndRotation(PlayerStart + Forward * 150.0, FacePlayer,
			false, nullptr, ETeleportType::TeleportPhysics);
		// The enemy turns toward its controller's desired rotation, and the frozen controller still holds its last one.
		if (AController* Controller = Enemy->GetController())
		{
			Controller->SetControlRotation(FacePlayer);
		}
		Enemy->HitReactionComponent->HitReactionSettingsOverride = Run.bPush ? PushSettings.Get() : ControlSettings.Get();
		Enemy->SetHealth(Enemy->MaxHealth);
		if (Enemy->CustomTimeDilation != 1.0f || Player->CustomTimeDilation != 1.0f)
		{
			Test->AddError(FString::Printf(TEXT("%s %s run: a previous hitstop still holds a fighter (enemy %.4f, player %.4f)"),
				*Run.Name, Run.bPush ? TEXT("push") : TEXT("control"), Enemy->CustomTimeDilation, Player->CustomTimeDilation));
		}
	}

	void Hit(FMeasuredRun& Run, const UWorld* World)
	{
		RootMotionEnd = -1.0;
		PushEnd = -1.0;
		HitMontage.Reset();
		TrackedMontage.Reset();
		bHasPreviousSample = false;
		bAwaitingStopCheck = false;
		UAttackData* Attack = LoadObject<UAttackData>(nullptr, Run.AttackPath);
		if (!Attack)
		{
			Test->AddError(FString::Printf(TEXT("Missing attack data %s"), Run.AttackPath));
			return;
		}
		// The game's own rules (UHitReactionComponent::StartKnockback) at charge 0 with the push run's victim scale.
		const FKnockbackConfig Config = KnockbackResolution::Resolve(Attack, Player->CombatSettings.Get());
		Run.ResolvedPush = KnockbackResolution::PushDistance(
			Config.Distance, 0.0f, Attack->MaxChargeKnockbackMultiplier, PushSettings->KnockbackScale);
		Run.ResolvedDuration = Config.Duration;
		Run.ResolvedBlend = Config.AnimationBlend;
		RunOrigin = Enemy->GetActorLocation();
		FHitReactionInfo Info;
		Info.Attacker = Player.Get();
		Info.AttackData = Attack;
		Info.Damage = 1.0f;
		Info.DirectionToAttacker = -Forward;
		Enemy->HitReactionComponent->ClearReactionHistory();
		// Each run reads only its own rows.
		Enemy->GetCombatComponent()->ClearActionReactionTelemetry();
		HitTime = World->GetTimeSeconds();
		FMath::RandInit(0x4B42);
		IDamageableInterface::Execute_ApplyDamage(Enemy.Get(), Info); // synchronous: ApplyDamage -> PlayHitReaction
		if (Run.Manipulation == ERunManipulation::Hitstop)
		{
			// As BaseCombatCharacter::HandleWeaponHit does in play: after the damage, so the reaction has started.
			Run.HitstopDuration = Attack->HitstopConfig.Duration;
			const bool bApplied = UCinematicEffectsUtilityLibrary::ApplyHitstop(Player.Get(), Enemy.Get(), Attack->HitstopConfig, false);
			Run.bManipulationPremise = bApplied && Enemy->CustomTimeDilation < 1.0f;
			Run.ManipulationTime = 0.0;
			Run.ManipulationNote = FString::Printf(TEXT("hitstop %.3f s applied after the damage (%s, victim dilation %.4f)"),
				Run.HitstopDuration, bApplied ? TEXT("applied") : TEXT("not applied"), Enemy->CustomTimeDilation);
		}
		const UAnimInstance* Anim = EnemyAnim();
		const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
		HitMontage = Montage;
		TrackedMontage = Montage;
		Run.Montage = GetNameSafe(Montage);
		Run.Section = Montage ? Anim->Montage_GetCurrentSection(Montage).ToString() : FString(TEXT("None"));
		Run.bRootMotionAtHit = Enemy->IsPlayingRootMotion();
		if (Run.Manipulation == ERunManipulation::SectionHolds)
		{
			RaisePlayRate(Run);
		}
	}

	/**
	 * Section-hold runs: make the reaction's section end SectionEndTarget after the hit, while the push still runs,
	 * and keep the instance as the root-motion montage once it stops playing.
	 */
	void RaisePlayRate(FMeasuredRun& Run)
	{
		UAnimInstance* Anim = EnemyAnim();
		const UAnimMontage* Montage = HitMontage.Get();
		Run.ManipulationTime = 0.0;
		if (!Anim || !Montage)
		{
			Run.ManipulationNote = TEXT("no reaction montage to speed up");
			return;
		}
		const int32 SectionIndex = Montage->GetSectionIndex(Anim->Montage_GetCurrentSection(Montage));
		float SectionStart = 0.0f;
		float SectionEnd = 0.0f;
		Montage->GetSectionStartAndEndTime(SectionIndex, SectionStart, SectionEnd);
		const float Position = Anim->Montage_GetPosition(Montage);
		const float RateScale = FMath::Max(KINDA_SMALL_NUMBER, Montage->RateScale);
		Run.PlayRate = FMath::Clamp(static_cast<float>((SectionEnd - Position) / (SectionEndTarget * RateScale)), 1.0f, 200.0f);
		Anim->Montage_SetPlayRate(Montage, Run.PlayRate);
		// Instance-level only, as a montage authored without auto blend-out (or Sequencer's) plays: at the section's
		// end it stops playing but holds its last pose, without Stop(), so it stays the root-motion montage.
		FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
		if (Instance)
		{
			Instance->bEnableAutoBlendOut = false;
		}
		Run.ManipulationNote = FString::Printf(TEXT("%s section %s [%.3f, %.3f] s from %.3f s at play rate %.1f, auto blend-out %s, aiming to end %.2f s after the hit"),
			*Run.Montage, *Run.Section, SectionStart, SectionEnd, Position, Run.PlayRate, Instance ? TEXT("off") : TEXT("not found"), SectionEndTarget);
	}

	/** True while the push request exists and is running; OutState holds its motion state when it exists. */
	bool ReadPushState(FAlignmentMotionState& OutState) const
	{
		OutState = FAlignmentMotionState();
		OutState.Outcome = EAlignmentMotionOutcome::Invalid; // no request: not a default-constructed Running state
		const UTargetingComponent* Targeting = Enemy->GetTargetingComponent();
		const FAlignmentRequestHandle Handle = Enemy->HitReactionComponent->GetKnockbackAlignmentHandleForTesting();
		return Targeting && Handle.IsValid() && Targeting->GetAlignmentMotionState(Handle, OutState)
			&& OutState.Outcome == EAlignmentMotionOutcome::Running;
	}

	double Clearance() const
	{
		const UCapsuleComponent* EnemyCapsule = Enemy->GetCapsuleComponent();
		const UCapsuleComponent* PlayerCapsule = Player->GetCapsuleComponent();
		const double Radii = (EnemyCapsule ? EnemyCapsule->GetScaledCapsuleRadius() : 0.0)
			+ (PlayerCapsule ? PlayerCapsule->GetScaledCapsuleRadius() : 0.0);
		return FVector::Dist2D(Enemy->GetActorLocation(), Player->GetActorLocation()) - Radii;
	}

	/** The montage-replaced and montage-stopped runs act ManipulationDelay after the hit. */
	void ApplyTimedManipulation(FMeasuredRun& Run, const double Elapsed)
	{
		if ((Run.Manipulation != ERunManipulation::ReplaceMontage && Run.Manipulation != ERunManipulation::StopMontages)
			|| Run.ManipulationTime >= 0.0 || Elapsed < ManipulationDelay)
		{
			return;
		}
		FAlignmentMotionState State;
		Run.bPushRunningAtManipulation = ReadPushState(State);
		Run.VelocityAtManipulation = FVector::DotProduct(Enemy->GetVelocity(), Forward);
		Run.ManipulationTime = Elapsed;
		UAnimInstance* Anim = EnemyAnim();
		if (!Anim)
		{
			Run.ManipulationNote = TEXT("no anim instance");
			return;
		}
		if (Run.Manipulation == ERunManipulation::ReplaceMontage)
		{
			UAnimMontage* Replacement = ReplacementMontage.Get();
			const float Length = Replacement && Replacement != HitMontage.Get() ? Anim->Montage_Play(Replacement, ReplacementPlayRate) : 0.0f;
			if (Length > 0.0f && ReplacementSection != NAME_None)
			{
				// As PlayHitReaction plays a section-only reaction.
				Anim->Montage_JumpToSection(ReplacementSection, Replacement);
				Anim->Montage_SetNextSection(ReplacementSection, NAME_None, Replacement);
			}
			const FAnimMontageInstance* RootInstance = Enemy->GetRootMotionAnimMontageInstance();
			Run.bManipulationPremise = Length > 0.0f && Replacement->HasRootMotion()
				&& RootInstance && RootInstance->Montage == Replacement;
			TrackedMontage = Replacement;
			Run.ManipulationNote = FString::Printf(TEXT("%s section %s replaced by %s section %s at %.3f s"),
				*Run.Montage, *Run.Section, *GetNameSafe(Replacement), *ReplacementSection.ToString(), Elapsed);
		}
		else
		{
			Anim->StopAllMontages(0.0f);
			bAwaitingStopCheck = true;
			Run.ManipulationNote = FString::Printf(TEXT("%s section %s stopped at %.3f s"), *Run.Montage, *Run.Section, Elapsed);
		}
	}

	/** Section-hold runs: when the reaction stops playing, and the push clock on every update while the push runs. */
	void TrackSectionEnd(FMeasuredRun& Run, const double Elapsed, const double Now, const bool bPushRunning, const FAlignmentMotionState& State)
	{
		const UAnimInstance* Anim = EnemyAnim();
		const FAnimMontageInstance* Instance = Anim && HitMontage.IsValid() ? Anim->GetInstanceForMontage(HitMontage.Get()) : nullptr;
		const bool bSectionPlaying = Instance && Instance->IsPlaying();
		if (Run.SectionEndTime < 0.0 && !bSectionPlaying)
		{
			Run.SectionEndTime = Elapsed;
			Run.bPushRunningAtSectionEnd = bPushRunning;
			Run.bRootMotionFlagAtSectionEnd = Enemy->IsPlayingRootMotion();
			// The held reaction is still the root-motion montage although it no longer plays.
			Run.bManipulationPremise = Run.bRootMotionFlagAtSectionEnd;
		}
		else if (Run.SectionEndTime >= 0.0 && bPushRunning)
		{
			++Run.RunningSamplesAfterSectionEnd;
		}

		// While the push runs, its request clock advances on every update in which the victim's time advanced:
		// a root-motion montage that has stopped advancing (here, holding its last pose) must hand the push on,
		// not freeze it.
		if (!Run.bPush)
		{
			return;
		}
		if (!bPushRunning)
		{
			bHasPreviousSample = false;
			return;
		}
		if (bHasPreviousSample)
		{
			const double ActorDelta = (Now - PreviousSampleTime) * Enemy->CustomTimeDilation;
			if (ActorDelta > 0.0)
			{
				++Run.ElapsedSamples;
				const double Advance = State.Elapsed - PreviousElapsed;
				Run.MinElapsedRate = FMath::Min(Run.MinElapsedRate, Advance / ActorDelta);
				if (!(FMath::IsFinite(Advance) && Advance > 0.0))
				{
					++Run.ElapsedStalls;
					if (Run.FirstStall.IsEmpty())
					{
						Run.FirstStall = FString::Printf(TEXT("at %.3f s: elapsed %.4f -> %.4f over %.4f s of actor time (section %s)"),
							Elapsed, PreviousElapsed, State.Elapsed, ActorDelta, bSectionPlaying ? TEXT("playing") : TEXT("ended"));
					}
				}
			}
		}
		bHasPreviousSample = true;
		PreviousElapsed = State.Elapsed;
		PreviousSampleTime = Now;
	}

	/** The tracked reaction's montage instance, including one that is blending out or holding its last pose. */
	const FAnimMontageInstance* TrackedInstance() const
	{
		const UAnimInstance* Anim = EnemyAnim();
		return Anim && TrackedMontage.IsValid() ? Anim->GetInstanceForMontage(TrackedMontage.Get()) : nullptr;
	}

	/**
	 * True while the tracked reaction can move the character: its instance plays and a root-motion montage is set.
	 * Stop(), which the auto blend-out near a section's end also calls, clears the anim instance's root-motion montage
	 * (UAnimInstance::ClearMontageInstanceReferences), so from the blend-out's start no montage root motion reaches
	 * character movement although the instance still plays.
	 */
	bool IsTrackedRootMotionActive() const
	{
		const FAnimMontageInstance* Instance = TrackedInstance();
		return Instance && Instance->IsPlaying() && Enemy->IsPlayingRootMotion();
	}

	void Sample(FMeasuredRun& Run, const double Now)
	{
		const double Elapsed = Now - StageStart;
		if (bAwaitingStopCheck && Elapsed > Run.ManipulationTime)
		{
			// The update after the stop: the montage has terminated, so no root motion remains to carry the push.
			Run.bManipulationPremise = !Enemy->IsPlayingRootMotion();
			Run.CarryFrameDelta = Elapsed - Run.ManipulationTime;
			bAwaitingStopCheck = false;
		}
		ApplyTimedManipulation(Run, Elapsed);

		const double Travel = FVector::DotProduct(Enemy->GetActorLocation() - RunOrigin, Forward);
		Run.PeakTravel = FMath::Max(Run.PeakTravel, Travel);
		Run.MinClearance = FMath::Min(Run.MinClearance, Clearance());

		FAlignmentMotionState State;
		const bool bPushRunning = ReadPushState(State);
		if (Run.Manipulation == ERunManipulation::SectionHolds)
		{
			TrackSectionEnd(Run, Elapsed, Now, bPushRunning, State);
		}
		if (IsTransition(Run.Manipulation) && Elapsed <= TraceSeconds)
		{
			// Enough to see a handoff frame by frame: where the character went, and what the push clock did meanwhile.
			const FAnimMontageInstance* Instance = TrackedInstance();
			Run.Trace.Add(FString::Printf(TEXT("%.3f s: travel %.2f cm, velocity %.1f cm/s, push %s elapsed %.4f s, root-motion montage %d, tracked montage playing %d"),
				Elapsed, Travel, FVector::DotProduct(Enemy->GetVelocity(), Forward),
				*StaticEnum<EAlignmentMotionOutcome>()->GetNameStringByValue(static_cast<int64>(State.Outcome)), State.Elapsed,
				Enemy->IsPlayingRootMotion() ? 1 : 0, Instance && Instance->IsPlaying() ? 1 : 0));
		}
		if (RootMotionEnd < 0.0 && !IsTrackedRootMotionActive())
		{
			RootMotionEnd = Elapsed;
			Run.RootMotionDuration = Elapsed;
			Run.VelocityAtRootMotionEnd = FVector::DotProduct(Enemy->GetVelocity(), Forward);
			Run.TravelAtRootMotionEnd = Travel;
		}
		if (PushEnd < 0.0 && !bPushRunning)
		{
			// The first update after the frame that wrote the terminal row: the push's last step has landed.
			PushEnd = Elapsed;
			Run.PushEndTime = Elapsed;
			Run.TravelAtPushEnd = Travel;
		}

		const bool bSettled = RootMotionEnd >= 0.0 && PushEnd >= 0.0 && Elapsed >= FMath::Max(RootMotionEnd, PushEnd) + RunMargin;
		const bool bCapped = !bSettled && Elapsed >= RunCap;
		if (!bSettled && !bCapped)
		{
			return;
		}
		if (bCapped)
		{
			Test->AddError(FString::Printf(TEXT("%s %s run: after %.2f s the reaction's root motion %s and the push %s"),
				*Run.Name, Run.bPush ? TEXT("push") : TEXT("control"), Elapsed,
				RootMotionEnd >= 0.0 ? TEXT("had ended") : TEXT("had not ended"), PushEnd >= 0.0 ? TEXT("had ended") : TEXT("still ran")));
			Run.RootMotionDuration = Elapsed;
		}
		Run.NetTravel = Travel;
		ReadTelemetry(Run);
		Test->AddInfo(FString::Printf(
			TEXT("%s %s run: %s section %s; %s; net %.2f cm, peak %.2f cm; root motion ended at %.3f s (%.1f cm/s, then %.2f cm); rows %s; terminal %s at %.3f s"),
			*Run.Name, Run.bPush ? TEXT("push") : TEXT("control"), *Run.Montage, *Run.Section,
			Run.ManipulationNote.IsEmpty() ? TEXT("no manipulation") : *Run.ManipulationNote,
			Run.NetTravel, Run.PeakTravel, Run.RootMotionDuration, Run.VelocityAtRootMotionEnd, Run.NetTravel - Run.TravelAtRootMotionEnd,
			Run.KnockbackRows.IsEmpty() ? TEXT("none") : *Run.KnockbackRows, *Run.TerminalDisposition, Run.TerminalTime));
		Stage = 0;
		++RunIndex;
	}

	/** This run's HitKnockback rows: the Started row, the terminal row and anything between. */
	void ReadTelemetry(FMeasuredRun& Run) const
	{
		const UCombatComponent* Combat = Enemy->GetCombatComponent();
		if (!Combat)
		{
			return;
		}
		const TArray<FActionReactionTelemetryRecord>& Rows = Combat->GetActionReactionTelemetry();
		// A full ring may have dropped this run's first rows, which would read as a missing Started row.
		Run.bTelemetryFull = Rows.Num() >= UCombatComponent::GetActionReactionTelemetryCapacity();
		TArray<FString> Dispositions;
		double StartedAt = HitTime;
		double TerminalAt = 0.0;
		for (const FActionReactionTelemetryRecord& Row : Rows)
		{
			if (Row.AlignmentOwner != KnockbackOwner)
			{
				continue;
			}
			const FString Disposition = Row.AlignmentDisposition.ToString();
			Dispositions.Add(Disposition);
			if (Disposition == TEXT("Started"))
			{
				++Run.StartedRows;
				StartedAt = Row.SimulationTimestamp; // written synchronously at the hit
			}
			else if (IsTerminalDisposition(Disposition) && ++Run.TerminalRows == 1)
			{
				Run.TerminalDisposition = Disposition;
				Run.TerminalDetail = Row.Detail;
				TerminalAt = Row.SimulationTimestamp;
			}
		}
		if (Run.TerminalRows > 0)
		{
			Run.TerminalTime = TerminalAt - StartedAt;
		}
		Run.KnockbackRows = FString::Join(Dispositions, TEXT(","));
	}

	static FString BlendName(const EDisplacementAnimationBlend Blend)
	{
		return StaticEnum<EDisplacementAnimationBlend>()->GetNameStringByValue(static_cast<int64>(Blend));
	}

	const FMeasuredRun* FindPlainPush(const FString& Type) const
	{
		return Runs.FindByPredicate([&Type](const FMeasuredRun& Run)
		{
			return Run.bPush && Run.Manipulation == ERunManipulation::None && Run.Type == Type;
		});
	}

	void AssertManipulation(const FMeasuredRun& Control, const FMeasuredRun& Push, const TCHAR* Label)
	{
		switch (Push.Manipulation)
		{
		case ERunManipulation::Hitstop:
		{
			Test->TestTrue(*FString::Printf(TEXT("%s: hitstop was applied and froze the victim in both runs"), Label),
				Control.bManipulationPremise && Push.bManipulationPremise);
			if (const FMeasuredRun* Plain = FindPlainPush(Push.Type))
			{
				// The freeze really overlapped the push: it reached later than the same push without hitstop. Only
				// strictly later: the push's first tick installs its channel without advancing its clock, so a freeze
				// that also covers that tick delays the push by fewer ticks than it lasts.
				Test->TestTrue(*FString::Printf(TEXT("%s: the frozen push reached later than the plain one (%.3f s vs %.3f s, hitstop %.3f s)"),
					Label, Push.TerminalTime, Plain->TerminalTime, Push.HitstopDuration),
					IsFiniteWithin(Plain->TerminalTime, 0.0, TNumericLimits<double>::Max())
					&& IsFiniteAbove(Push.TerminalTime, Plain->TerminalTime + 0.001));
			}
			break;
		}
		case ERunManipulation::ReplaceMontage:
			Test->TestTrue(*FString::Printf(TEXT("%s: the replacement became the root-motion montage in both runs"), Label),
				Control.bManipulationPremise && Push.bManipulationPremise);
			Test->TestTrue(*FString::Printf(TEXT("%s: the push was running when the montage was replaced"), Label),
				Push.bPushRunningAtManipulation);
			break;
		case ERunManipulation::StopMontages:
			Test->TestTrue(*FString::Printf(TEXT("%s: no root motion played after the stop in either run"), Label),
				Control.bManipulationPremise && Push.bManipulationPremise);
			Test->TestTrue(*FString::Printf(TEXT("%s: the push was running when the montages stopped"), Label),
				Push.bPushRunningAtManipulation);
			break;
		case ERunManipulation::SectionHolds:
			Test->TestTrue(*FString::Printf(TEXT("%s: the held reaction stayed the root-motion montage after its section ended, in both runs"), Label),
				Control.bManipulationPremise && Push.bManipulationPremise);
			Test->TestTrue(*FString::Printf(TEXT("%s: the reaction section ended during both runs (%.3f s, %.3f s)"),
				Label, Control.SectionEndTime, Push.SectionEndTime),
				IsFiniteWithin(Control.SectionEndTime, 0.0, RunCap) && IsFiniteWithin(Push.SectionEndTime, 0.0, RunCap));
			Test->TestTrue(*FString::Printf(TEXT("%s: the section ended while the push ran"), Label), Push.bPushRunningAtSectionEnd);
			Test->TestTrue(*FString::Printf(TEXT("%s: the push ran on after the section ended (%d updates)"),
				Label, Push.RunningSamplesAfterSectionEnd), Push.RunningSamplesAfterSectionEnd > 0);
			Test->TestTrue(*FString::Printf(TEXT("%s: the push clock was sampled (%d updates)"), Label, Push.ElapsedSamples),
				Push.ElapsedSamples >= 2);
			Test->TestEqual(*FString::Printf(TEXT("%s: the push clock advanced on every update with actor time (first stall: %s)"),
				Label, Push.FirstStall.IsEmpty() ? TEXT("none") : *Push.FirstStall), Push.ElapsedStalls, 0);
			break;
		default:
			break;
		}
	}

	void Report()
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		for (int32 Pair = 0; Pair + 1 < Runs.Num(); Pair += 2)
		{
			const FMeasuredRun& Control = Runs[Pair];
			const FMeasuredRun& Push = Runs[Pair + 1];
			const TCHAR* Label = *Control.Name;
			const double Added = Push.NetTravel - Control.NetTravel;
			const double Tolerance = FMath::Max(1.5, 0.04 * static_cast<double>(Push.ResolvedPush));
			// A hitstop holds the push still while the victim is frozen, so its terminal row may come that much later.
			const double TerminalBound = static_cast<double>(Push.ResolvedDuration) + Push.HitstopDuration + TerminalRowMargin;
			const FString Blend = BlendName(Push.ResolvedBlend);
			Test->AddInfo(FString::Printf(
				TEXT("%s: reaction alone peak %.1f cm, net %.1f cm over %.2f s (%s, section %s); with knockback net %.1f cm; added %.2f cm (resolved %.1f cm, tolerance %.2f, %s); push %s at %.3f s (bound %.3f s)"),
				Label, Control.PeakTravel, Control.NetTravel, Control.RootMotionDuration, *Control.Montage, *Control.Section,
				Push.NetTravel, Added, Push.ResolvedPush, Tolerance, *Blend, *Push.TerminalDisposition, Push.TerminalTime, TerminalBound));

			// Premises: both runs played the same root-motion reaction and neither touched the player.
			Test->TestNotEqual(*FString::Printf(TEXT("%s: a reaction montage played"), Label), Control.Montage, FString(TEXT("None")));
			Test->TestEqual(*FString::Printf(TEXT("%s: both runs played the same reaction"), Label), Push.Montage, Control.Montage);
			Test->TestEqual(*FString::Printf(TEXT("%s: both runs played the same reaction section"), Label), Push.Section, Control.Section);
			Test->TestTrue(*FString::Printf(TEXT("%s: the reaction plays root motion, so the animation channel carries the push"), Label),
				Control.bRootMotionAtHit && Push.bRootMotionAtHit);
			Test->TestTrue(*FString::Printf(TEXT("%s: the enemy never reached the player's capsule (closest %.1f cm, %.1f cm)"),
				Label, Control.MinClearance, Push.MinClearance), IsFiniteAbove(Control.MinClearance, 0.0) && IsFiniteAbove(Push.MinClearance, 0.0));
			AssertManipulation(Control, Push, Label);

			// Handoff runs: the push's own delivered distance. Until the reaction's root motion ends both runs move
			// alike but for the push; after it, the push run's movement-channel source overrides (then zeroes) the
			// velocity the reaction left behind, while the control keeps sliding on it, so the end-of-run difference
			// is not the push. Delivered = (push - control) at root-motion end + push run from then to its terminal row.
			const double DeliveredBeforeHandoff = Push.TravelAtRootMotionEnd - Control.TravelAtRootMotionEnd;
			const double DeliveredAfterHandoff = Push.TravelAtPushEnd - Push.TravelAtRootMotionEnd;
			const double Delivered = DeliveredBeforeHandoff + DeliveredAfterHandoff;
			const double CarryBound = FMath::Max(0.0, Push.VelocityAtManipulation) * Push.CarryFrameDelta;
			if (Push.ResolvedBlend != EDisplacementAnimationBlend::AddToAnimation)
			{
				Test->AddInfo(FString::Printf(TEXT("%s: the added-push relation is not asserted for the %s blend"), Label, *Blend));
			}
			else if (HandsOffToMovement(Push.Manipulation))
			{
				Test->TestTrue(*FString::Printf(TEXT("%s: both runs' root motion ended on the same update (%.3f s, %.3f s)"),
					Label, Control.RootMotionDuration, Push.RootMotionDuration),
					IsFiniteAndNear(Control.RootMotionDuration, Push.RootMotionDuration, 0.01));
				Test->TestTrue(*FString::Printf(TEXT("%s: the root motion ended before the push did (%.3f s, push %.3f s)"),
					Label, Push.RootMotionDuration, Push.PushEndTime),
					IsFiniteWithin(Push.RootMotionDuration, 0.0, Push.PushEndTime));
				if (Push.Manipulation == ERunManipulation::StopMontages)
				{
					// Zero-blend stop: character movement integrates one frame on the leftover root-motion velocity (push
					// included) before the executor installs the movement source, so up to that velocity times one frame more.
					Test->TestTrue(*FString::Printf(
						TEXT("%s: the push delivers the resolved push plus at most one frame of carry (delivered %.3f = %.3f + %.3f cm, resolved %.1f cm, carry bound %.3f cm = %.1f cm/s x %.4f s, tolerance %.2f cm)"),
						Label, Delivered, DeliveredBeforeHandoff, DeliveredAfterHandoff, Push.ResolvedPush, CarryBound,
						Push.VelocityAtManipulation, Push.CarryFrameDelta, Tolerance),
						IsFiniteAbove(CarryBound, 0.0) && IsFiniteWithin(Delivered,
							Push.ResolvedPush - Tolerance, Push.ResolvedPush + CarryBound + Tolerance));
				}
				else
				{
					Test->TestTrue(*FString::Printf(
						TEXT("%s: the push delivers the resolved push (delivered %.3f = %.3f + %.3f cm, resolved %.1f cm, tolerance %.2f cm)"),
						Label, Delivered, DeliveredBeforeHandoff, DeliveredAfterHandoff, Push.ResolvedPush, Tolerance),
						IsFiniteAndNear(Delivered, Push.ResolvedPush, Tolerance));
				}
			}
			else
			{
				Test->TestTrue(*FString::Printf(TEXT("%s: knockback adds the resolved push (added %.3f cm, resolved %.1f cm, tolerance %.2f cm)"),
					Label, Added, Push.ResolvedPush, Tolerance), IsFiniteAndNear(Added, Push.ResolvedPush, Tolerance));
			}

			// The push's own outcome rows: one Started, then one terminal Reached that arrives promptly.
			Test->TestFalse(*FString::Printf(TEXT("%s: the telemetry ring did not fill during either run"), Label),
				Control.bTelemetryFull || Push.bTelemetryFull);
			Test->TestEqual(*FString::Printf(TEXT("%s: the control run starts no push (rows: %s)"), Label, *Control.KnockbackRows),
				Control.StartedRows, 0);
			Test->TestEqual(*FString::Printf(TEXT("%s: one HitKnockback Started row (rows: %s)"), Label, *Push.KnockbackRows),
				Push.StartedRows, 1);
			Test->TestEqual(*FString::Printf(TEXT("%s: one terminal row (rows: %s)"), Label, *Push.KnockbackRows),
				Push.TerminalRows, 1);
			Test->TestEqual(*FString::Printf(TEXT("%s: the push ends Reached (detail: %s)"), Label, *Push.TerminalDetail),
				Push.TerminalDisposition, FString(TEXT("Reached")));
			Test->TestTrue(*FString::Printf(TEXT("%s: the terminal row arrives within %.3f s of the hit (%.3f s)"), Label, TerminalBound, Push.TerminalTime),
				IsFiniteWithin(Push.TerminalTime, 0.0, TerminalBound));

			TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
			Entry->SetStringField(TEXT("reaction_montage"), Control.Montage);
			Entry->SetStringField(TEXT("reaction_section"), Control.Section);
			Entry->SetBoolField(TEXT("root_motion_at_hit"), Control.bRootMotionAtHit);
			Entry->SetNumberField(TEXT("reaction_peak_cm"), Control.PeakTravel);
			Entry->SetNumberField(TEXT("reaction_net_cm"), Control.NetTravel);
			Entry->SetNumberField(TEXT("reaction_duration_s"), Control.RootMotionDuration);
			Entry->SetNumberField(TEXT("with_knockback_cm"), Push.NetTravel);
			Entry->SetNumberField(TEXT("added_cm"), Added);
			Entry->SetNumberField(TEXT("resolved_push_cm"), Push.ResolvedPush);
			Entry->SetStringField(TEXT("resolved_blend"), Blend);
			Entry->SetNumberField(TEXT("resolved_duration_s"), Push.ResolvedDuration);
			Entry->SetNumberField(TEXT("tolerance_cm"), Tolerance);
			Entry->SetStringField(TEXT("terminal_disposition"), Push.TerminalDisposition);
			Entry->SetStringField(TEXT("terminal_detail"), Push.TerminalDetail);
			Entry->SetNumberField(TEXT("terminal_time_s"), Push.TerminalTime);
			Entry->SetNumberField(TEXT("terminal_bound_s"), TerminalBound);
			Entry->SetStringField(TEXT("knockback_rows"), Push.KnockbackRows);
			Entry->SetStringField(TEXT("control_terminal_disposition"), Control.TerminalDisposition);
			if (Control.TerminalTime >= 0.0)
			{
				Entry->SetNumberField(TEXT("control_terminal_time_s"), Control.TerminalTime);
			}
			Entry->SetStringField(TEXT("control_knockback_rows"), Control.KnockbackRows);
			Entry->SetNumberField(TEXT("reaction_velocity_at_root_motion_end_cms"), Control.VelocityAtRootMotionEnd);
			Entry->SetNumberField(TEXT("reaction_slide_after_root_motion_cm"), Control.NetTravel - Control.TravelAtRootMotionEnd);
			Entry->SetNumberField(TEXT("with_knockback_root_motion_end_s"), Push.RootMotionDuration);
			Entry->SetNumberField(TEXT("with_knockback_velocity_at_root_motion_end_cms"), Push.VelocityAtRootMotionEnd);
			Entry->SetNumberField(TEXT("with_knockback_slide_after_root_motion_cm"), Push.NetTravel - Push.TravelAtRootMotionEnd);
			Entry->SetNumberField(TEXT("with_knockback_push_end_s"), Push.PushEndTime);
			if (HandsOffToMovement(Push.Manipulation))
			{
				// The asserted measure for handoff runs; added_cm above stays as the end-of-run record.
				Entry->SetNumberField(TEXT("delivered_push_cm"), Delivered);
				Entry->SetNumberField(TEXT("delivered_before_handoff_cm"), DeliveredBeforeHandoff);
				Entry->SetNumberField(TEXT("delivered_after_handoff_cm"), DeliveredAfterHandoff);
			}
			if (Push.Manipulation == ERunManipulation::StopMontages)
			{
				Entry->SetNumberField(TEXT("carry_frame_delta_s"), Push.CarryFrameDelta);
				Entry->SetNumberField(TEXT("carry_bound_cm"), CarryBound);
			}
			Entry->SetNumberField(TEXT("min_clearance_cm"), FMath::Min(Control.MinClearance, Push.MinClearance));
			Entry->SetStringField(TEXT("manipulation"), ManipulationName(Push.Manipulation));
			if (Push.Manipulation != ERunManipulation::None)
			{
				Entry->SetStringField(TEXT("manipulation_note"), Push.ManipulationNote);
				Entry->SetNumberField(TEXT("manipulation_s"), Push.ManipulationTime);
				Entry->SetBoolField(TEXT("manipulation_premise"), Control.bManipulationPremise && Push.bManipulationPremise);
			}
			if (Push.Manipulation == ERunManipulation::Hitstop)
			{
				Entry->SetNumberField(TEXT("hitstop_s"), Push.HitstopDuration);
			}
			if (Push.Manipulation == ERunManipulation::ReplaceMontage || Push.Manipulation == ERunManipulation::StopMontages)
			{
				Entry->SetNumberField(TEXT("reaction_velocity_at_manipulation_cms"), Control.VelocityAtManipulation);
				Entry->SetNumberField(TEXT("with_knockback_velocity_at_manipulation_cms"), Push.VelocityAtManipulation);
				Entry->SetBoolField(TEXT("push_running_at_manipulation"), Push.bPushRunningAtManipulation);
			}
			if (Push.Manipulation == ERunManipulation::SectionHolds)
			{
				Entry->SetNumberField(TEXT("play_rate"), Push.PlayRate);
				Entry->SetNumberField(TEXT("section_end_s"), Push.SectionEndTime);
				Entry->SetBoolField(TEXT("push_running_at_section_end"), Push.bPushRunningAtSectionEnd);
				Entry->SetBoolField(TEXT("root_motion_montage_at_section_end"), Push.bRootMotionFlagAtSectionEnd);
				Entry->SetNumberField(TEXT("running_updates_after_section_end"), Push.RunningSamplesAfterSectionEnd);
				Entry->SetNumberField(TEXT("elapsed_samples"), Push.ElapsedSamples);
				Entry->SetNumberField(TEXT("elapsed_stalls"), Push.ElapsedStalls);
				if (Push.ElapsedSamples > 0)
				{
					Entry->SetNumberField(TEXT("min_elapsed_rate"), Push.MinElapsedRate);
				}
				Entry->SetStringField(TEXT("first_stall"), Push.FirstStall);
			}
			if (IsTransition(Push.Manipulation))
			{
				const auto TraceArray = [](const TArray<FString>& Lines)
				{
					TArray<TSharedPtr<FJsonValue>> Values;
					for (const FString& Line : Lines)
					{
						Values.Add(MakeShared<FJsonValueString>(Line));
					}
					return Values;
				};
				Entry->SetArrayField(TEXT("control_trace"), TraceArray(Control.Trace));
				Entry->SetArrayField(TEXT("with_knockback_trace"), TraceArray(Push.Trace));
			}
			Json->SetObjectField(Control.Name, Entry);
		}
		FString Out;
		FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Out));
		FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("Logs/KnockbackMeasurement.json")));
	}

	FAutomationTestBase* Test;
	double CommandStart;
	/** Telemetry is on from construction (before the map load) until this command is destroyed, on every exit. */
	FScopedActionReactionTelemetry Telemetry;
	bool bReady = false;
	bool bFailed = false;
	TArray<FMeasuredRun> Runs;
	int32 RunIndex = 0;
	int32 Stage = 0;
	double StageStart = 0.0;
	double HitTime = 0.0;
	/** Seconds after the hit at which the tracked reaction's root motion and the push ended; negative until they have. */
	double RootMotionEnd = -1.0;
	double PushEnd = -1.0;
	static constexpr double RunMargin = 0.25;
	static constexpr double RunCap = 6.0;
	/** The reaction montage that started at the hit, and the montage whose root motion the run waits on. */
	TWeakObjectPtr<const UAnimMontage> HitMontage;
	TWeakObjectPtr<const UAnimMontage> TrackedMontage;
	TWeakObjectPtr<UAnimMontage> ReplacementMontage;
	FName ReplacementSection = NAME_None;
	float ReplacementPlayRate = 1.0f;
	bool bAwaitingStopCheck = false;
	bool bHasPreviousSample = false;
	double PreviousElapsed = 0.0;
	double PreviousSampleTime = 0.0;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<AEnemyCharacter> Enemy;
	// Strong references: only one copy is assigned to the enemy at a time, and PIE garbage collection must not take the other.
	TStrongObjectPtr<UHitReactionSettings> PushSettings;
	TStrongObjectPtr<UHitReactionSettings> ControlSettings;
	FVector PlayerStart = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector RunOrigin = FVector::ZeroVector;
};

/** The map load, PIE and the measurement; enqueued only once the preflight has found no leaked world. */
void EnqueueMeasurement(FAutomationTestBase* Test)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(KnockbackMap));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FKnockbackMeasurementCommand(Test));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
}

/**
 * Runs before the map load. FEditorLoadMap ends in UEditorEngine::CheckForWorldGCLeaks, which raises a Fatal for any
 * world that survives garbage collection without a world context. A world leaked by any earlier test would then end
 * the whole run at this test. This names the leak as a failure here and skips the map load and the measurement.
 */
class FLeakedWorldPreflightCommand final : public IAutomationLatentCommand
{
public:
	explicit FLeakedWorldPreflightCommand(FAutomationTestBase* InTest)
		: Test(InTest)
	{
	}

	virtual bool Update() override
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		TArray<UWorld*> Leaked;
		for (TObjectIterator<UWorld> It; It; ++It)
		{
			UWorld* World = *It;
			const bool bPersistentType = World->WorldType == EWorldType::Inactive
				|| World->WorldType == EWorldType::EditorPreview
				|| World->WorldType == EWorldType::GamePreview;
			if (!bPersistentType && !GEngine->GetWorldContextFromWorld(World))
			{
				Leaked.Add(World);
			}
		}
		if (Leaked.IsEmpty())
		{
			EnqueueMeasurement(Test);
			return true;
		}
		for (UWorld* World : Leaked)
		{
			Test->AddError(FString::Printf(
				TEXT("Leaked world %s (outer %s, type %s) survived garbage collection without a world context; an earlier test kept it alive. The map load would end the run on the editor's world-leak check, so it is skipped. The reference chain is in the log."),
				*World->GetPathName(), *GetPathNameSafe(World->GetOuter()), LexToString(World->WorldType)));
			FReferenceChainSearch::FindAndPrintStaleReferencesToObject(World, EPrintStaleReferencesOptions::Log);
		}
		return true;
	}

private:
	FAutomationTestBase* Test;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackPIEMeasurementTest, "KatanaCombat.Knockback.PIE.ReactionMeasurement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackPIEMeasurementTest::RunTest(const FString&)
{
	// The preflight enqueues the map load and the measurement only when no earlier test has leaked a world.
	ADD_LATENT_AUTOMATION_COMMAND(FLeakedWorldPreflightCommand(this));
	return true;
}
