#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "EngineUtils.h"
#include "AI/EnemyCombatAIComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Characters/PlayerCharacter.h"
#include "Characters/EnemyCharacter.h"
#include "Core/HitReactionComponent.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"
#include "Data/HitReactionSettings.h"
#include "Interfaces/DamageableInterface.h"
#include "Utilities/KnockbackResolution.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
const TCHAR* KnockbackMap = TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1");
const TCHAR* LightAttackPath = TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1");
const TCHAR* HeavyAttackPath = TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Heavy/New/HeavyAttack_1.HeavyAttack_1");

struct FMeasuredRun
{
	FString Type;
	const TCHAR* AttackPath = nullptr;
	bool bPush = false;
	/** The push the game's own knockback rules resolve for this attack at the hit. */
	float ResolvedPush = 0.0f;
	EDisplacementAnimationBlend ResolvedBlend = EDisplacementAnimationBlend::AddToAnimation;
	/** Travel along the push direction from the hit location: the largest sample, and the value once the reaction has ended. */
	double PeakTravel = 0.0;
	double NetTravel = 0.0;
	/** Seconds from the hit until the reaction's root motion ended. */
	double RootMotionDuration = 0.0;
	FString Montage;
	FString Section;
	bool bRootMotionAtHit = false;
};

class FKnockbackMeasurementCommand final : public IAutomationLatentCommand
{
public:
	explicit FKnockbackMeasurementCommand(FAutomationTestBase* InTest)
		: Test(InTest), CommandStart(FPlatformTime::Seconds())
	{
		// Control then push for each type: the difference is the push, the control is the reaction's own travel.
		Runs.Add({TEXT("light"), LightAttackPath, false});
		Runs.Add({TEXT("light"), LightAttackPath, true});
		Runs.Add({TEXT("heavy"), HeavyAttackPath, false});
		Runs.Add({TEXT("heavy"), HeavyAttackPath, true});
	}

	virtual bool Update() override
	{
		UWorld* World = AutomationCommon::GetAnyGameWorld();
		if (!bReady)
		{
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

		// Each run: place the enemy and let it settle, hit it, then sample its travel every update until the
		// reaction's root motion has ended, plus a short margin for the last movement to land.
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
			Hit(Run);
			StageStart = Now;
			Stage = 2;
		}
		else if (Stage == 2)
		{
			const double Elapsed = Now - StageStart;
			const double Travel = FVector::DotProduct(Enemy->GetActorLocation() - RunOrigin, Forward);
			Run.PeakTravel = FMath::Max(Run.PeakTravel, Travel);
			if (RootMotionEnd < 0.0 && !IsReactionRootMotionActive())
			{
				RootMotionEnd = Elapsed;
				Run.RootMotionDuration = Elapsed;
			}
			const bool bSettled = RootMotionEnd >= 0.0 && Elapsed >= RootMotionEnd + RunMargin;
			const bool bCapped = !bSettled && Elapsed >= RunCap;
			if (bSettled || bCapped)
			{
				if (bCapped)
				{
					Test->AddError(FString::Printf(TEXT("%s %s run: the reaction's root motion had not ended after %.2f s"),
						*Run.Type, Run.bPush ? TEXT("push") : TEXT("control"), Elapsed));
					Run.RootMotionDuration = Elapsed;
				}
				Run.NetTravel = Travel;
				Stage = 0;
				++RunIndex;
			}
		}
		return false;
	}

private:
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

	void Place(const FMeasuredRun& Run)
	{
		// Gate A's reset: end the previous reaction so its blend-out and leftover velocity do not carry into this run.
		if (UAnimInstance* Anim = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr)
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
	}

	void Hit(FMeasuredRun& Run)
	{
		RootMotionEnd = -1.0;
		HitMontage.Reset();
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
		Run.ResolvedBlend = Config.AnimationBlend;
		RunOrigin = Enemy->GetActorLocation();
		FHitReactionInfo Info;
		Info.Attacker = Player.Get();
		Info.AttackData = Attack;
		Info.Damage = 1.0f;
		Info.DirectionToAttacker = -Forward;
		Enemy->HitReactionComponent->ClearReactionHistory();
		FMath::RandInit(0x4B42);
		IDamageableInterface::Execute_ApplyDamage(Enemy.Get(), Info); // synchronous: ApplyDamage -> PlayHitReaction
		const UAnimInstance* Anim = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
		const UAnimMontage* Montage = Anim ? Anim->GetCurrentActiveMontage() : nullptr;
		HitMontage = Montage;
		Run.Montage = GetNameSafe(Montage);
		Run.Section = Montage ? Anim->Montage_GetCurrentSection(Montage).ToString() : FString(TEXT("None"));
		Run.bRootMotionAtHit = Enemy->IsPlayingRootMotion();
	}

	/** True while the enemy plays root motion from the montage that started at the hit. */
	bool IsReactionRootMotionActive() const
	{
		const UAnimInstance* Anim = Enemy->GetMesh() ? Enemy->GetMesh()->GetAnimInstance() : nullptr;
		// Montage_IsActive(nullptr) means "any montage", so a missing hit montage counts as ended.
		return Anim && HitMontage.IsValid() && Enemy->IsPlayingRootMotion() && Anim->Montage_IsActive(HitMontage.Get());
	}

	static FString BlendName(const EDisplacementAnimationBlend Blend)
	{
		return StaticEnum<EDisplacementAnimationBlend>()->GetNameStringByValue(static_cast<int64>(Blend));
	}

	void Report()
	{
		TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
		for (int32 Pair = 0; Pair + 1 < Runs.Num(); Pair += 2)
		{
			const FMeasuredRun& Control = Runs[Pair];
			const FMeasuredRun& Push = Runs[Pair + 1];
			const double Added = Push.NetTravel - Control.NetTravel;
			const FString Blend = BlendName(Push.ResolvedBlend);
			Test->AddInfo(FString::Printf(
				TEXT("%s: reaction alone peak %.1f cm, net %.1f cm over %.2f s (%s, section %s); with knockback net %.1f cm; added %.1f cm (resolved %.1f cm, %s)"),
				*Control.Type, Control.PeakTravel, Control.NetTravel, Control.RootMotionDuration, *Control.Montage, *Control.Section,
				Push.NetTravel, Added, Push.ResolvedPush, *Blend));
			Test->TestNotEqual(*FString::Printf(TEXT("%s: a reaction montage played"), *Control.Type), Control.Montage, FString(TEXT("None")));
			Test->TestEqual(*FString::Printf(TEXT("%s: both runs played the same reaction"), *Control.Type), Push.Montage, Control.Montage);
			Test->TestEqual(*FString::Printf(TEXT("%s: both runs played the same reaction section"), *Control.Type), Push.Section, Control.Section);
			Test->TestTrue(*FString::Printf(TEXT("%s: the reaction plays root motion, so the animation channel carries the push"), *Control.Type),
				Control.bRootMotionAtHit && Push.bRootMotionAtHit);
			if (Push.ResolvedBlend == EDisplacementAnimationBlend::AddToAnimation)
			{
				Test->TestTrue(*FString::Printf(TEXT("%s: knockback adds the resolved push"), *Control.Type),
					FMath::IsNearlyEqual(Added, static_cast<double>(Push.ResolvedPush), FMath::Max(8.0, 0.25 * Push.ResolvedPush)));
			}
			else
			{
				Test->AddInfo(FString::Printf(TEXT("%s: the added-push relation is not asserted for the %s blend"), *Control.Type, *Blend));
			}

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
			Json->SetObjectField(Control.Type, Entry);
		}
		FString Out;
		FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Out));
		FFileHelper::SaveStringToFile(Out, *(FPaths::ProjectSavedDir() / TEXT("Logs/KnockbackMeasurement.json")));
	}

	FAutomationTestBase* Test;
	double CommandStart;
	bool bReady = false;
	bool bFailed = false;
	TArray<FMeasuredRun> Runs;
	int32 RunIndex = 0;
	int32 Stage = 0;
	double StageStart = 0.0;
	/** Seconds after the hit at which the reaction's root motion ended; negative until it has. */
	double RootMotionEnd = -1.0;
	static constexpr double RunMargin = 0.25;
	static constexpr double RunCap = 6.0;
	TWeakObjectPtr<const UAnimMontage> HitMontage;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<AEnemyCharacter> Enemy;
	// Strong references: only one copy is assigned to the enemy at a time, and PIE garbage collection must not take the other.
	TStrongObjectPtr<UHitReactionSettings> PushSettings;
	TStrongObjectPtr<UHitReactionSettings> ControlSettings;
	FVector PlayerStart = FVector::ZeroVector;
	FVector Forward = FVector::ForwardVector;
	FVector RunOrigin = FVector::ZeroVector;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnockbackPIEMeasurementTest, "KatanaCombat.Knockback.PIE.ReactionMeasurement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FKnockbackPIEMeasurementTest::RunTest(const FString&)
{
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(KnockbackMap));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FKnockbackMeasurementCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
