// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyAITypes.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/CombatComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Debug/DefenseTelemetry.h"
#include "DefenseAssetValidationService.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"

namespace
{
constexpr TCHAR GateAMapPackage[] = TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1");
constexpr TCHAR GateAManifestRelativePath[] =
	TEXT("Tools/Codex/manifests/defense-gate-a.json");
constexpr TCHAR GateAAttackPath[] =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1");

struct FRenderedFrameValidation
{
	bool bDecoded = false;
	bool bHasNontrivialPixels = false;
	int32 Width = 0;
	int32 Height = 0;
	int32 ChannelRange = 0;
	float AverageBrightness = 0.0f;
};

bool AnalyzeRenderedFrameFile(
	const FString& Filename,
	FRenderedFrameValidation& OutValidation)
{
	OutValidation = {};
	FImage Image;
	if (!FImageUtils::LoadImage(*Filename, Image))
	{
		return false;
	}

	OutValidation.bDecoded = true;
	OutValidation.Width = Image.SizeX;
	OutValidation.Height = Image.SizeY;
	if (Image.SizeX < 32 || Image.SizeY < 32 || Image.NumSlices != 1)
	{
		return true;
	}

	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const int64 PixelCount = static_cast<int64>(Image.SizeX) * Image.SizeY;
	if (Image.RawData.Num() != PixelCount * static_cast<int64>(sizeof(FColor)))
	{
		OutValidation.bDecoded = false;
		return false;
	}

	const FColor* Pixels = reinterpret_cast<const FColor*>(Image.RawData.GetData());
	const FColor FirstPixel = Pixels[0];
	const int64 SampleStride = FMath::Max<int64>(1, PixelCount / 8192);
	int32 MinR = 255;
	int32 MinG = 255;
	int32 MinB = 255;
	int32 MaxR = 0;
	int32 MaxG = 0;
	int32 MaxB = 0;
	int32 SampleCount = 0;
	int32 VariedSampleCount = 0;
	double BrightnessTotal = 0.0;
	for (int64 Index = 0; Index < PixelCount; Index += SampleStride)
	{
		const FColor& Pixel = Pixels[Index];
		MinR = FMath::Min(MinR, static_cast<int32>(Pixel.R));
		MinG = FMath::Min(MinG, static_cast<int32>(Pixel.G));
		MinB = FMath::Min(MinB, static_cast<int32>(Pixel.B));
		MaxR = FMath::Max(MaxR, static_cast<int32>(Pixel.R));
		MaxG = FMath::Max(MaxG, static_cast<int32>(Pixel.G));
		MaxB = FMath::Max(MaxB, static_cast<int32>(Pixel.B));
		BrightnessTotal += FMath::Max3(Pixel.R, Pixel.G, Pixel.B);
		VariedSampleCount += FMath::Abs(static_cast<int32>(Pixel.R) - FirstPixel.R) > 8
			|| FMath::Abs(static_cast<int32>(Pixel.G) - FirstPixel.G) > 8
			|| FMath::Abs(static_cast<int32>(Pixel.B) - FirstPixel.B) > 8
			? 1 : 0;
		++SampleCount;
	}

	OutValidation.ChannelRange = FMath::Max3(MaxR - MinR, MaxG - MinG, MaxB - MinB);
	OutValidation.AverageBrightness = SampleCount > 0
		? static_cast<float>(BrightnessTotal / SampleCount)
		: 0.0f;
	OutValidation.bHasNontrivialPixels = OutValidation.ChannelRange >= 8
		&& VariedSampleCount >= 8
		&& OutValidation.AverageBrightness >= 2.0f;
	return true;
}

enum class EGateAProofStage : uint8
{
	WaitForPIE,
	HandsOffMapObserve,
	HeldGuardStart,
	HeldGuardObserve,
	NormalBlockStart,
	NormalBlockAwaitContact,
	NormalBlockObserve,
	SameEnemyRecovery,
	SameEnemyReattack,
	SameEnemyReattackObserve,
	TokenPolicyWait,
	TokenPolicyRun,
	OutOfConeStart,
	OutOfConeAwaitParry,
	OutOfConeAwaitContact,
	OutOfConeObserve,
	AlignmentDowngradeStart,
	AlignmentDowngradeAwaitParry,
	AlignmentDowngradeObserve,
	PerfectParryStart,
	PerfectParryAwaitWindow,
	PerfectParryBridge,
	PerfectParryCounter,
	PerfectParryFinisher,
	FinalCaptureWait,
	Done
};

const TCHAR* StageName(const EGateAProofStage Stage)
{
	switch (Stage)
	{
	case EGateAProofStage::WaitForPIE: return TEXT("WaitForPIE");
	case EGateAProofStage::HandsOffMapObserve: return TEXT("HandsOffMapObserve");
	case EGateAProofStage::HeldGuardStart: return TEXT("HeldGuardStart");
	case EGateAProofStage::HeldGuardObserve: return TEXT("HeldGuardObserve");
	case EGateAProofStage::NormalBlockStart: return TEXT("NormalBlockStart");
	case EGateAProofStage::NormalBlockAwaitContact: return TEXT("NormalBlockAwaitContact");
	case EGateAProofStage::NormalBlockObserve: return TEXT("NormalBlockObserve");
	case EGateAProofStage::SameEnemyRecovery: return TEXT("SameEnemyRecovery");
	case EGateAProofStage::SameEnemyReattack: return TEXT("SameEnemyReattack");
	case EGateAProofStage::SameEnemyReattackObserve: return TEXT("SameEnemyReattackObserve");
	case EGateAProofStage::TokenPolicyWait: return TEXT("TokenPolicyWait");
	case EGateAProofStage::TokenPolicyRun: return TEXT("TokenPolicyRun");
	case EGateAProofStage::OutOfConeStart: return TEXT("OutOfConeStart");
	case EGateAProofStage::OutOfConeAwaitParry: return TEXT("OutOfConeAwaitParry");
	case EGateAProofStage::OutOfConeAwaitContact: return TEXT("OutOfConeAwaitContact");
	case EGateAProofStage::OutOfConeObserve: return TEXT("OutOfConeObserve");
	case EGateAProofStage::AlignmentDowngradeStart: return TEXT("AlignmentDowngradeStart");
	case EGateAProofStage::AlignmentDowngradeAwaitParry: return TEXT("AlignmentDowngradeAwaitParry");
	case EGateAProofStage::AlignmentDowngradeObserve: return TEXT("AlignmentDowngradeObserve");
	case EGateAProofStage::PerfectParryStart: return TEXT("PerfectParryStart");
	case EGateAProofStage::PerfectParryAwaitWindow: return TEXT("PerfectParryAwaitWindow");
	case EGateAProofStage::PerfectParryBridge: return TEXT("PerfectParryBridge");
	case EGateAProofStage::PerfectParryCounter: return TEXT("PerfectParryCounter");
	case EGateAProofStage::PerfectParryFinisher: return TEXT("PerfectParryFinisher");
	case EGateAProofStage::FinalCaptureWait: return TEXT("FinalCaptureWait");
	case EGateAProofStage::Done: return TEXT("Done");
	default: return TEXT("Unknown");
	}
}

FString EnumName(const UEnum* Enum, const int64 Value)
{
	return Enum ? Enum->GetNameStringByValue(Value) : TEXT("Unknown");
}

FHitResult MakeProofHit(AActor* Target, const FVector& SourceLocation)
{
	FHitResult Hit;
	Hit.HitObjectHandle = FActorInstanceHandle(Target);
	Hit.TraceStart = SourceLocation;
	Hit.TraceEnd = Target ? Target->GetActorLocation() : SourceLocation;
	Hit.Location = Hit.TraceEnd;
	Hit.ImpactPoint = Hit.TraceEnd;
	Hit.ImpactNormal = (SourceLocation - Hit.TraceEnd).GetSafeNormal();
	Hit.BoneName = TEXT("spine_03");
	return Hit;
}

class FDefenseGateAPIEProofCommand final : public IAutomationLatentCommand
{
public:
	FDefenseGateAPIEProofCommand(
		FAutomationTestBase* InTest,
		TArray<FString> InExpectedCaseNames)
		: Test(InTest)
		, CommandStart(FPlatformTime::Seconds())
		, StageStart(CommandStart)
		, ExpectedCaseNames(MoveTemp(InExpectedCaseNames))
	{
		EvidenceDirectory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			FApp::CanEverRender()
				? TEXT("DefenseProof/GateA/Rendered")
				: TEXT("DefenseProof/GateA/Headless"));
		FramesDirectory = FPaths::Combine(EvidenceDirectory, TEXT("frames"));
	}

	virtual ~FDefenseGateAPIEProofCommand() override
	{
		CleanupProofState();
	}

	virtual bool Update() override
	{
		if (!Test)
		{
			return true;
		}

		if (Stage != EGateAProofStage::WaitForPIE
			&& Stage != EGateAProofStage::Done
			&& !World.IsValid())
		{
			Fail(TEXT("PIE world became invalid during Gate A proof"));
		}

		if (ShouldCaptureContinuousFrame())
		{
			CaptureFrame(StageName(Stage));
		}

		switch (Stage)
		{
		case EGateAProofStage::WaitForPIE:
			return UpdateWaitForPIE();
		case EGateAProofStage::HandsOffMapObserve:
			return UpdateHandsOffMapObserve();
		case EGateAProofStage::HeldGuardStart:
			return UpdateHeldGuardStart();
		case EGateAProofStage::HeldGuardObserve:
			return UpdateHeldGuardObserve();
		case EGateAProofStage::NormalBlockStart:
			return UpdateNormalBlockStart();
		case EGateAProofStage::NormalBlockAwaitContact:
			return UpdateNormalBlockAwaitContact();
		case EGateAProofStage::NormalBlockObserve:
			return UpdateNormalBlockObserve();
		case EGateAProofStage::SameEnemyRecovery:
			return UpdateSameEnemyRecovery();
		case EGateAProofStage::SameEnemyReattack:
			return UpdateSameEnemyReattack();
		case EGateAProofStage::SameEnemyReattackObserve:
			return UpdateSameEnemyReattackObserve();
		case EGateAProofStage::TokenPolicyWait:
			return UpdateTokenPolicyWait();
		case EGateAProofStage::TokenPolicyRun:
			return UpdateTokenPolicyRun();
		case EGateAProofStage::OutOfConeStart:
			return UpdateOutOfConeStart();
		case EGateAProofStage::OutOfConeAwaitParry:
			return UpdateOutOfConeAwaitParry();
		case EGateAProofStage::OutOfConeAwaitContact:
			return UpdateOutOfConeAwaitContact();
		case EGateAProofStage::OutOfConeObserve:
			return UpdateOutOfConeObserve();
		case EGateAProofStage::AlignmentDowngradeStart:
			return UpdateAlignmentDowngradeStart();
		case EGateAProofStage::AlignmentDowngradeAwaitParry:
			return UpdateAlignmentDowngradeAwaitParry();
		case EGateAProofStage::AlignmentDowngradeObserve:
			return UpdateAlignmentDowngradeObserve();
		case EGateAProofStage::PerfectParryStart:
			return UpdatePerfectParryStart();
		case EGateAProofStage::PerfectParryAwaitWindow:
			return UpdatePerfectParryAwaitWindow();
		case EGateAProofStage::PerfectParryBridge:
			return UpdatePerfectParryBridge();
		case EGateAProofStage::PerfectParryCounter:
			return UpdatePerfectParryCounter();
		case EGateAProofStage::PerfectParryFinisher:
			return UpdatePerfectParryFinisher();
		case EGateAProofStage::FinalCaptureWait:
			return UpdateFinalCaptureWait();
		case EGateAProofStage::Done:
			return true;
		default:
			Fail(TEXT("Unknown Gate A proof stage"));
			return false;
		}
	}

private:
	bool UpdateWaitForPIE()
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!PIEWorld)
		{
			if (FPlatformTime::Seconds() - CommandStart > 20.0)
			{
				Fail(TEXT("PIE world did not start within 20 seconds"));
			}
			return false;
		}

		APlayerCharacter* FoundPlayer = nullptr;
		TArray<AEnemyCharacter*> FoundEnemies;
		for (TActorIterator<APlayerCharacter> It(PIEWorld); It; ++It)
		{
			if (IsValid(*It))
			{
				FoundPlayer = *It;
				break;
			}
		}
		for (TActorIterator<AEnemyCharacter> It(PIEWorld); It; ++It)
		{
			if (IsValid(*It))
			{
				FoundEnemies.Add(*It);
			}
		}
		if (!FoundPlayer || FoundEnemies.Num() < 4)
		{
			if (FPlatformTime::Seconds() - CommandStart > 20.0)
			{
				Fail(FString::Printf(
					TEXT("Gate A map loaded without required player/four enemies (player=%s enemies=%d)"),
					FoundPlayer ? TEXT("yes") : TEXT("no"), FoundEnemies.Num()));
			}
			return false;
		}

		FoundEnemies.Sort([](const AEnemyCharacter& Left, const AEnemyCharacter& Right)
		{
			return Left.GetName() < Right.GetName();
		});
		World = PIEWorld;
		Player = FoundPlayer;
		Enemies.Reset();
		for (AEnemyCharacter* Enemy : FoundEnemies)
		{
			Enemies.Add(Enemy);
		}
		Attack = LoadObject<UAttackData>(nullptr, GateAAttackPath);
		PlayerCombat = Player->CombatComponent.Get();
		PlayerPaired = Player->PairedAnimationComponent.Get();
		TokenSubsystem = PIEWorld->GetGameInstance()
			? PIEWorld->GetGameInstance()->GetSubsystem<UCombatTokenSubsystem>()
			: nullptr;
		if (!Attack.IsValid() || !PlayerCombat.IsValid()
			|| !PlayerPaired.IsValid() || !TokenSubsystem.IsValid())
		{
			Fail(TEXT("Gate A PIE fixture is missing attack, combat, paired, or token authority"));
			return false;
		}

		IFileManager::Get().DeleteDirectory(*EvidenceDirectory, false, true);
		IFileManager::Get().MakeDirectory(*FramesDirectory, true);
		if (IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.Defense.Debug")))
		{
			PreviousDefenseDebugValue = Debug->GetInt();
			Debug->SetWithCurrentPriority(1);
			bDefenseDebugOverridden = true;
		}

		PlayerCombat->ClearDefenseTelemetry();
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			if (Enemy.IsValid() && Enemy->CombatComponent)
			{
				Enemy->CombatComponent->ClearDefenseTelemetry();
			}
		}
		BaseLocation = Player->GetActorLocation();
		RecordCase(TEXT("MapFixture"), true,
			FString::Printf(TEXT("player=%s enemies=%d attack=%s"),
				*Player->GetName(), Enemies.Num(), *Attack->GetPathName()));
		SetStage(EGateAProofStage::HandsOffMapObserve);
		return false;
	}

	bool UpdateHandsOffMapObserve()
	{
		constexpr int32 RequiredEnemyCount = 4;
		int32 ConfiguredCount = 0;
		int32 ReviewedAttackCount = 0;
		int32 ControllerCount = 0;
		int32 TargetingPlayerCount = 0;
		int32 ActiveStateCount = 0;
		int32 HighestGeneration = 0;
		for (int32 Index = 0; Index < RequiredEnemyCount; ++Index)
		{
			AEnemyCharacter* Enemy = EnemyAt(Index);
			UEnemyCombatAIComponent* AI = EnemyAI(Index);
			if (!Enemy || !AI)
			{
				continue;
			}
			const bool bConfigured = AI->AvailableAttacks.Num() == 1
				&& AI->AvailableAttacks[0].AttackData != nullptr
				&& AI->AttackSelectionMode == EEnemyAttackSelection::Single;
			ConfiguredCount += bConfigured ? 1 : 0;
			ReviewedAttackCount += bConfigured
				&& AI->AvailableAttacks[0].AttackData == Attack.Get()
				? 1 : 0;
			ControllerCount += Enemy->GetController() ? 1 : 0;
			TargetingPlayerCount += AI->CombatTarget.Get() == Player.Get() ? 1 : 0;
			ActiveStateCount += AI->CurrentState != EEnemyAIState::Idle
				|| AI->HasAttackToken()
				|| AI->IsWaitingForToken()
				? 1 : 0;
			HighestGeneration = FMath::Max(
				HighestGeneration,
				Enemy->CombatComponent
					? Enemy->CombatComponent->GetCurrentAttackGeneration()
					: 0);
		}

		HandsOffConfiguredEnemyCount = FMath::Max(HandsOffConfiguredEnemyCount, ConfiguredCount);
		HandsOffReviewedAttackCount = FMath::Max(
			HandsOffReviewedAttackCount, ReviewedAttackCount);
		HandsOffControllerCount = FMath::Max(HandsOffControllerCount, ControllerCount);
		HandsOffTargetingPlayerCount = FMath::Max(
			HandsOffTargetingPlayerCount, TargetingPlayerCount);
		HandsOffActiveStateCount = FMath::Max(HandsOffActiveStateCount, ActiveStateCount);
		HandsOffMaxAttackGeneration = FMath::Max(
			HandsOffMaxAttackGeneration, HighestGeneration);
		HandsOffMaxConcurrentTokens = FMath::Max(
			HandsOffMaxConcurrentTokens,
			TokenSubsystem->GetActiveAttackerCount());
		HandsOffMaxQueueLength = FMath::Max(
			HandsOffMaxQueueLength,
			TokenSubsystem->GetQueueLength());
		bHandsOffTokenPolicyValid = bHandsOffTokenPolicyValid
			&& TokenSubsystem->GetActiveAttackerCount() <= 1;

		const bool bPlayable = HandsOffConfiguredEnemyCount == RequiredEnemyCount
			&& HandsOffReviewedAttackCount > 0
			&& HandsOffControllerCount == RequiredEnemyCount
			&& HandsOffTargetingPlayerCount == RequiredEnemyCount
			&& HandsOffActiveStateCount > 0
			&& HandsOffMaxAttackGeneration > 0
			&& bHandsOffTokenPolicyValid;
		if (bPlayable && StageElapsed() >= 0.5)
		{
			Test->TestTrue(TEXT("Authored map AI has one concrete serialized Single-mode attack per enemy"),
				HandsOffConfiguredEnemyCount == RequiredEnemyCount);
			Test->TestTrue(TEXT("Authored map includes the reviewed Gate A attack without runtime seeding"),
				HandsOffReviewedAttackCount > 0);
			Test->TestTrue(TEXT("Authored map AI targets the player through its controller/StateTree"),
				HandsOffTargetingPlayerCount == RequiredEnemyCount);
			Test->TestTrue(TEXT("Hands-off map play starts a real attack generation"),
				HandsOffMaxAttackGeneration > 0);
			Test->TestTrue(TEXT("Hands-off map play respects the one-attacker token policy"),
				bHandsOffTokenPolicyValid && HandsOffMaxConcurrentTokens <= 1);
			RecordCase(
				TEXT("HandsOffMapPlayable"),
				true,
				FString::Printf(
					TEXT("configured=%d reviewed_attack=%d controllers=%d targets=%d active_states=%d max_generation=%d max_tokens=%d max_queue=%d"),
					HandsOffConfiguredEnemyCount,
					HandsOffReviewedAttackCount,
					HandsOffControllerCount,
					HandsOffTargetingPlayerCount,
					HandsOffActiveStateCount,
					HandsOffMaxAttackGeneration,
					HandsOffMaxConcurrentTokens,
					HandsOffMaxQueueLength));
			CaptureFrame(TEXT("hands_off_map_playable"));
			if (!InitializeControlledProof())
			{
				return false;
			}
			SetStage(EGateAProofStage::HeldGuardStart);
			return false;
		}

		if (StageElapsed() > 10.0)
		{
			Fail(FString::Printf(
				TEXT("Authored map did not become hands-off playable (configured=%d reviewed_attack=%d controllers=%d targets=%d active_states=%d max_generation=%d max_tokens=%d)"),
				HandsOffConfiguredEnemyCount,
				HandsOffReviewedAttackCount,
				HandsOffControllerCount,
				HandsOffTargetingPlayerCount,
				HandsOffActiveStateCount,
				HandsOffMaxAttackGeneration,
				HandsOffMaxConcurrentTokens));
		}
		return false;
	}

	bool InitializeControlledProof()
	{
		PlayerCombat->ClearDefenseTelemetry();
		TokenSubsystem->ResetAllTokens();
		BaseLocation = Player->GetActorLocation();
		Player->SetHealth(Player->MaxHealth);
		for (int32 Index = 0; Index < Enemies.Num(); ++Index)
		{
			AEnemyCharacter* Enemy = Enemies[Index].Get();
			if (!Enemy)
			{
				Fail(TEXT("Gate A enemy became invalid while entering controlled proof"));
				return false;
			}
			ConfigureEnemy(Enemy);
			if (UEnemyCombatAIComponent* AI = Enemy->CombatAIComponent.Get())
			{
				if (AI->IsAttacking())
				{
					AI->OnDamaged();
				}
				AI->SetCombatTarget(nullptr);
			}
			DisableControllerLogic(Enemy);
			ResetCharacterCombat(Enemy);
			Enemy->SetHealth(Enemy->MaxHealth);
			Enemy->SetActorLocation(
				BaseLocation + FVector(1800.0f + Index * 150.0f, 1000.0f, 0.0f));
			if (Enemy->CombatComponent)
			{
				Enemy->CombatComponent->ClearDefenseTelemetry();
			}
		}
		ResetCharacterCombat(Player.Get());
		PlayerCombat->OnDefenseResolvedNative.AddRaw(
			this, &FDefenseGateAPIEProofCommand::HandleDefenseResolved);
		bResolutionBound = true;

		AEnemyCharacter* NormalEnemy = EnemyAt(0);
		if (!NormalEnemy || !NormalEnemy->WeaponComponent)
		{
			Fail(TEXT("Gate A normal-block enemy is missing its weapon component"));
			return false;
		}
		NormalEnemy->WeaponComponent->OnRichContactAccountedForTesting.AddRaw(
			this, &FDefenseGateAPIEProofCommand::HandleNormalContactAccounted);
		bNormalContactBinding = true;
		UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.AddRaw(
			this, &FDefenseGateAPIEProofCommand::HandleImpactSoundPlaybackInvoked);
		bImpactSoundBinding = true;
		return true;
	}

	bool UpdateHeldGuardStart()
	{
		ResetPlayerFacing(0.0f);
		ResetResolutionCapture();
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		SetStage(EGateAProofStage::HeldGuardObserve);
		return false;
	}

	bool UpdateHeldGuardObserve()
	{
		if (StageElapsed() < 0.30)
		{
			return false;
		}
		const bool bBlocking = PlayerCombat->IsBlocking();
		const bool bNoThreat = !PlayerCombat->GetLockedDefenseThreat().AttackInstance.IsValid();
		Test->TestTrue(TEXT("Gate A held guard enters without a parry target"), bBlocking);
		Test->TestTrue(TEXT("Gate A held guard does not fabricate a threat"), bNoThreat);
		RecordCase(TEXT("HeldGuardWithoutThreat"), bBlocking && bNoThreat,
			TEXT("Block Press entered held guard with no active attack"));
		CaptureFrame(TEXT("held_guard"));
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
		SetStage(EGateAProofStage::NormalBlockStart);
		return false;
	}

	bool UpdateNormalBlockStart()
	{
		if (StageElapsed() < 0.15)
		{
			return false;
		}
		AEnemyCharacter* Enemy = EnemyAt(0);
		PrepareControlledPair(Enemy, 0.0f);
		NormalInitialHealth = Player->CurrentHealth;
		NormalBlockStartLocation = Player->GetActorLocation();
		bNormalAudioPlaybackInvoked = false;
		NormalPlayedSound.Reset();
		bNormalDuplicateChecked = false;
		bNormalDuplicateCached = false;
		bNormalDuplicateSameWindow = false;
		NormalDuplicateReceipt = {};
		ResetResolutionCapture();
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		if (!StartEnemyAttack(Enemy))
		{
			Fail(TEXT("Gate A normal-block enemy could not start LightAttack_1"));
			return false;
		}
		NormalAttackGeneration = Enemy->CombatComponent->GetCurrentAttackGeneration();
		ExpectResolutionFrom(Enemy, NormalAttackGeneration);
		SetStage(EGateAProofStage::NormalBlockAwaitContact);
		return false;
	}

	bool UpdateNormalBlockAwaitContact()
	{
		if (bHasResolution)
		{
			SetStage(EGateAProofStage::NormalBlockObserve);
			return false;
		}

		if (StageElapsed() > 4.0)
		{
			Fail(TEXT("Gate A normal block produced no physical weapon-trace contact"));
		}
		return false;
	}

	bool UpdateNormalBlockObserve()
	{
		if (!bNormalObserved)
		{
			bNormalObserved = true;
			NormalInteractionId = LastResolution.InteractionId;
			NormalBlockMontage = LastResolution.Presentation.Montage;
			const bool bOutcome = LastResolution.Decision.Outcome == EDefenseOutcome::NormalBlock;
			const bool bNoDamage = FMath::IsNearlyEqual(Player->CurrentHealth, NormalInitialHealth);
			const bool bMontage = NormalBlockMontage.IsValid();
			const bool bAudioPayload = LastResolution.Presentation.ImpactAudio.IsActive();
			const bool bAudioPlayback = bNormalAudioPlaybackInvoked
				&& NormalPlayedSound.IsValid()
				&& NormalPlayedSound.Get()
					== LastResolution.Presentation.ImpactAudio.ImpactSound;
			const bool bVFX = LastResolution.Presentation.ImpactVFX.IsActive();
			const bool bContinue = LastResolution.Decision.AttackerResponse == EAttackerResponse::Continue;
			const bool bDuplicateSameInteraction = NormalDuplicateReceipt.CommitStatus
				== EDefenseCommitStatus::Cached
				&& NormalDuplicateReceipt.Resolution.InteractionId == LastResolution.InteractionId;
			Test->TestTrue(TEXT("Gate A contact resolves NormalBlock"), bOutcome);
			Test->TestTrue(TEXT("Gate A normal block suppresses damage"), bNoDamage);
			Test->TestTrue(TEXT("Gate A normal block selects a montage"), bMontage);
			Test->TestTrue(TEXT("Gate A normal block selects audio"), bAudioPayload);
			Test->TestTrue(TEXT("Gate A normal block invokes engine audio playback"), bAudioPlayback);
			Test->TestTrue(TEXT("Gate A normal block selects VFX"), bVFX);
			Test->TestTrue(TEXT("Gate A normal block preserves Continue response"), bContinue);
			Test->TestTrue(TEXT("Gate A normal block exercised same-window duplicate accounting"),
				bNormalDuplicateChecked);
			Test->TestTrue(TEXT("Gate A duplicate retained the original attack-window identity"),
				bNormalDuplicateSameWindow);
			Test->TestTrue(TEXT("Gate A duplicate reaches the defender canonical cache"),
				bNormalDuplicateCached && bDuplicateSameInteraction);
			Test->TestTrue(TEXT("Gate A first physical contact consumes a positive hit budget"),
				NormalAcceptedBeforeDuplicate > 0);
			Test->TestEqual(TEXT("Duplicate normal-block contact does not consume hit budget twice"),
				NormalAcceptedAfterDuplicate, NormalAcceptedBeforeDuplicate);
			RecordCase(TEXT("NormalBlockMiddleCenter"),
				bOutcome && bNoDamage && bMontage && bAudioPayload && bAudioPlayback
					&& bVFX && bContinue
					&& bNormalDuplicateChecked
					&& bNormalDuplicateSameWindow
					&& bNormalDuplicateCached
					&& bDuplicateSameInteraction
					&& NormalAcceptedBeforeDuplicate > 0
					&& NormalAcceptedAfterDuplicate == NormalAcceptedBeforeDuplicate,
				FString::Printf(
					TEXT("row=%s attacker_row=%s contact=%s duplicate_budget=%d->%d"),
					*LastResolution.PresentationRow.ToString(),
					*LastResolution.AttackerPresentationRow.ToString(),
					TEXT("physical-trace+defender-cache-replay"),
					NormalAcceptedBeforeDuplicate,
					NormalAcceptedAfterDuplicate));
			CaptureFrame(TEXT("normal_block_impact"));
		}

		const bool bMontagePlaying = NormalBlockMontage.IsValid()
			&& Player->GetMesh()
			&& Player->GetMesh()->GetAnimInstance()
			&& Player->GetMesh()->GetAnimInstance()->Montage_IsPlaying(NormalBlockMontage.Get());
		if (bMontagePlaying && StageElapsed() < 2.0)
		{
			return false;
		}

		NormalBlockDriftCm = FVector::Dist2D(NormalBlockStartLocation, Player->GetActorLocation());
		Test->TestTrue(TEXT("Gate A normal-block actor drift is at most 1 cm"),
			NormalBlockDriftCm <= 1.0f + KINDA_SMALL_NUMBER);
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
		SetStage(EGateAProofStage::SameEnemyRecovery);
		return false;
	}

	bool UpdateSameEnemyRecovery()
	{
		UEnemyCombatAIComponent* AI = EnemyAI(0);
		if (AI && AI->CurrentState == EEnemyAIState::Circling && !AI->HasAttackToken())
		{
			RecordCase(TEXT("NormalAttackRecovery"), true,
				TEXT("same enemy returned to Circling without range exit"));
			SetStage(EGateAProofStage::SameEnemyReattack);
			return false;
		}
		if (StageElapsed() > 5.0)
		{
			Fail(TEXT("Normal-block attacker did not recover to Circling"));
		}
		return false;
	}

	bool UpdateSameEnemyReattack()
	{
		AEnemyCharacter* Enemy = EnemyAt(0);
		UEnemyCombatAIComponent* AI = EnemyAI(0);
		if (!AI || !AI->CanAttemptAttack())
		{
			if (StageElapsed() > 2.0)
			{
				Fail(TEXT("Recovered enemy never became eligible to request another token"));
			}
			return false;
		}
		if (!AI->TryInitiateAttack() || !AI->ExecuteAttack())
		{
			Fail(TEXT("Recovered enemy could not execute a second attack while target remained in range"));
			return false;
		}
		SecondAttackGeneration = Enemy->CombatComponent->GetCurrentAttackGeneration();
		SetStage(EGateAProofStage::SameEnemyReattackObserve);
		return false;
	}

	bool UpdateSameEnemyReattackObserve()
	{
		UEnemyCombatAIComponent* AI = EnemyAI(0);
		const bool bNewGeneration = SecondAttackGeneration > NormalAttackGeneration;
		const bool bAttacking = AI && AI->CurrentState == EEnemyAIState::Attacking;
		const bool bToken = AI && AI->HasAttackToken();
		Test->TestTrue(TEXT("Same enemy starts a later attack generation"), bNewGeneration);
		Test->TestTrue(TEXT("Same enemy reattack reaches Attacking"), bAttacking);
		Test->TestTrue(TEXT("Same enemy reattack owns a token"), bToken);
		RecordCase(TEXT("SameEnemyReattackWithoutRangeReset"),
			bNewGeneration && bAttacking && bToken,
			FString::Printf(TEXT("generation=%d previous=%d distance=%.1f"),
				SecondAttackGeneration, NormalAttackGeneration,
				AI ? AI->GetDistanceToTarget() : -1.0f));
		if (AI)
		{
			AI->OnDamaged();
		}
		SetStage(EGateAProofStage::TokenPolicyWait);
		return false;
	}

	bool UpdateTokenPolicyWait()
	{
		UEnemyCombatAIComponent* FirstAI = EnemyAI(0);
		if (FirstAI && FirstAI->CurrentState != EEnemyAIState::Staggered
			&& FirstAI->CurrentState != EEnemyAIState::Recovering)
		{
			SetStage(EGateAProofStage::TokenPolicyRun);
			return false;
		}
		if (StageElapsed() > 4.0)
		{
			Fail(TEXT("Same-enemy cancellation did not restore an attackable state"));
		}
		return false;
	}

	bool UpdateTokenPolicyRun()
	{
		TokenSubsystem->ResetAllTokens();
		const FVector RingOffsets[4] = {
			FVector(260.0f, 0.0f, 0.0f),
			FVector(-260.0f, 0.0f, 0.0f),
			FVector(0.0f, 260.0f, 0.0f),
			FVector(0.0f, -260.0f, 0.0f)};
		int32 ImmediateGrants = 0;
		for (int32 Index = 0; Index < 4; ++Index)
		{
			AEnemyCharacter* Enemy = EnemyAt(Index);
			UEnemyCombatAIComponent* AI = EnemyAI(Index);
			ResetCharacterCombat(Enemy);
			Enemy->SetActorLocation(BaseLocation + RingOffsets[Index]);
			Enemy->SetActorRotation((BaseLocation - Enemy->GetActorLocation()).Rotation());
			AI->SetCombatTarget(nullptr);
			AI->SetCombatTarget(Player.Get());
			ImmediateGrants += AI->TryInitiateAttack() ? 1 : 0;
		}
		const bool bOneToken = ImmediateGrants == 1
			&& TokenSubsystem->GetActiveAttackerCount() == 1
			&& TokenSubsystem->GetQueueLength() == 3;
		Test->TestTrue(TEXT("Four-enemy fixture grants exactly one default attack token"), bOneToken);

		AEnemyCharacter* FriendlySource = EnemyAt(1);
		AEnemyCharacter* FriendlyTarget = EnemyAt(2);
		const float FriendlyHealth = FriendlyTarget->CurrentHealth;
		const int32 FriendlyAcceptedBefore = FriendlySource->WeaponComponent->GetAcceptedHitCountForTesting();
		FriendlySource->WeaponComponent->SetCompatibilityTraceGenerationForTesting(9401);
		FriendlySource->WeaponComponent->ProcessHitForTesting(
			MakeProofHit(FriendlyTarget, FriendlySource->GetActorLocation()), Attack.Get());
		const int32 FriendlyAcceptedAfter = FriendlySource->WeaponComponent->GetAcceptedHitCountForTesting();
		const EDefenseOutcome FriendlyOutcome = LastTelemetryOutcome(FriendlyTarget->CombatComponent.Get());
		const bool bFriendlyIgnored = FMath::IsNearlyEqual(FriendlyTarget->CurrentHealth, FriendlyHealth)
			&& FriendlyAcceptedAfter == FriendlyAcceptedBefore
			&& FriendlyOutcome == EDefenseOutcome::IgnoredFriendly;
		Test->TestTrue(TEXT("Enemy-versus-enemy contact is intentionally ignored"), bFriendlyIgnored);
		RecordCase(TEXT("FourEnemyTokenAndFriendlyFirePolicy"), bOneToken && bFriendlyIgnored,
			FString::Printf(TEXT("active=%d queue=%d friendly_outcome=%s"),
				TokenSubsystem->GetActiveAttackerCount(), TokenSubsystem->GetQueueLength(),
				*EnumName(StaticEnum<EDefenseOutcome>(), static_cast<int64>(FriendlyOutcome))));
		CaptureOpponent = EnemyAt(0);
		CaptureFrame(TEXT("four_enemy_token_policy"));

		TokenSubsystem->ResetAllTokens();
		for (int32 Index = 0; Index < 4; ++Index)
		{
			EnemyAI(Index)->CancelQueuedAttackRequest();
			EnemyAI(Index)->SetCombatTarget(nullptr);
			ResetCharacterCombat(EnemyAt(Index));
		}
		CaptureOpponent.Reset();
		SetStage(EGateAProofStage::OutOfConeStart);
		return false;
	}

	bool UpdateOutOfConeStart()
	{
		AEnemyCharacter* Enemy = EnemyAt(1);
		PrepareControlledPair(Enemy, 180.0f);
		OutOfConeInitialHealth = Player->CurrentHealth;
		ResetResolutionCapture();
		if (!StartEnemyAttack(Enemy))
		{
			Fail(TEXT("Out-of-cone enemy could not start attack"));
			return false;
		}
		ExpectResolutionFrom(Enemy, Enemy->CombatComponent->GetCurrentAttackGeneration());
		SetStage(EGateAProofStage::OutOfConeAwaitParry);
		return false;
	}

	bool UpdateOutOfConeAwaitParry()
	{
		AEnemyCharacter* Enemy = EnemyAt(1);
		if (!HasFreshParryWindow(Enemy))
		{
			if (StageElapsed() > 3.0)
			{
				Fail(TEXT("Out-of-cone attack never published a high-confidence parry window"));
			}
			return false;
		}
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		OutOfConeInputResolution = PlayerCombat->GetLastInputDefenseResolutionForTesting();
		const bool bDowngraded = OutOfConeInputResolution.Decision.Outcome == EDefenseOutcome::GuardEntered
			&& OutOfConeInputResolution.Decision.Reason == EDefenseReason::OutsideHardCone;
		Test->TestTrue(TEXT("Out-of-hard-cone parry attempt remains guard only"), bDowngraded);
		SetStage(EGateAProofStage::OutOfConeAwaitContact);
		return false;
	}

	bool UpdateOutOfConeAwaitContact()
	{
		AEnemyCharacter* Enemy = EnemyAt(1);
		if (bHasResolution)
		{
			SetStage(EGateAProofStage::OutOfConeObserve);
			return false;
		}
		const FAttackWindowInstanceId HitWindow = Enemy->CombatComponent->GetActiveAttackWindow(
			EAttackWindowKind::Hit);
		if (HitWindow.IsValid())
		{
			InjectWeaponContact(Enemy, Player.Get());
		}
		if (StageElapsed() > 4.0)
		{
			Fail(TEXT("Out-of-cone attack produced no contact"));
		}
		return false;
	}

	bool UpdateOutOfConeObserve()
	{
		const bool bHit = LastResolution.Decision.Outcome == EDefenseOutcome::Hit
			|| LastResolution.Decision.Outcome == EDefenseOutcome::UnblockableHit;
		const bool bDamaged = Player->CurrentHealth < OutOfConeInitialHealth;
		Test->TestTrue(TEXT("Out-of-hard-cone contact resolves as a hit"), bHit);
		Test->TestTrue(TEXT("Out-of-hard-cone contact applies damage"), bDamaged);
		RecordCase(TEXT("OutOfHardConeContact"), bHit && bDamaged,
			FString::Printf(TEXT("outcome=%s reason=%s damage=%.2f"),
				*EnumName(StaticEnum<EDefenseOutcome>(), static_cast<int64>(LastResolution.Decision.Outcome)),
				*EnumName(StaticEnum<EDefenseReason>(), static_cast<int64>(LastResolution.Decision.Reason)),
				OutOfConeInitialHealth - Player->CurrentHealth));
		CaptureFrame(TEXT("out_of_cone_hit"));
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
		if (UEnemyCombatAIComponent* AI = EnemyAI(1))
		{
			AI->OnDamaged();
		}
		CaptureOpponent.Reset();
		SetStage(EGateAProofStage::AlignmentDowngradeStart);
		return false;
	}

	bool UpdateAlignmentDowngradeStart()
	{
		if (StageElapsed() < 0.20)
		{
			return false;
		}
		AEnemyCharacter* Enemy = EnemyAt(2);
		PrepareControlledPair(Enemy, 30.0f);
		ResetResolutionCapture();
		if (!StartEnemyAttack(Enemy))
		{
			Fail(TEXT("Alignment-downgrade enemy could not start attack"));
			return false;
		}
		ExpectResolutionFrom(Enemy, Enemy->CombatComponent->GetCurrentAttackGeneration());
		SetStage(EGateAProofStage::AlignmentDowngradeAwaitParry);
		return false;
	}

	bool UpdateAlignmentDowngradeAwaitParry()
	{
		AEnemyCharacter* Enemy = EnemyAt(2);
		if (!HasFreshParryWindow(Enemy))
		{
			if (StageElapsed() > 3.0)
			{
				Fail(TEXT("Alignment-downgrade attack never published a parry window"));
			}
			return false;
		}
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		AlignmentDowngradeResolution = PlayerCombat->GetLastInputDefenseResolutionForTesting();
		SetStage(EGateAProofStage::AlignmentDowngradeObserve);
		return false;
	}

	bool UpdateAlignmentDowngradeObserve()
	{
		const bool bGuard = AlignmentDowngradeResolution.Decision.Outcome == EDefenseOutcome::GuardEntered;
		const bool bUnreachable = AlignmentDowngradeResolution.Decision.Reason
			== EDefenseReason::PerfectAlignmentUnreachable;
		const bool bNotConsumed = !EnemyAt(2)->CombatComponent->BuildAttackExecutionSnapshot().bAttackConsumed;
		Test->TestTrue(TEXT("Insufficient perfect alignment downgrades to guard"), bGuard && bUnreachable);
		Test->TestTrue(TEXT("Alignment downgrade does not consume the attack"), bNotConsumed);
		RecordCase(TEXT("PerfectTimingInsufficientAlignment"),
			bGuard && bUnreachable && bNotConsumed,
			FString::Printf(TEXT("yaw=%.2f available_turn=%.2f reason=%s"),
				AlignmentDowngradeResolution.Decision.MeasuredYawDegrees,
				AlignmentDowngradeResolution.Decision.AvailableTurnDegrees,
				*EnumName(StaticEnum<EDefenseReason>(),
					static_cast<int64>(AlignmentDowngradeResolution.Decision.Reason))));
		CaptureFrame(TEXT("alignment_downgrade"));
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
		if (UEnemyCombatAIComponent* AI = EnemyAI(2))
		{
			AI->OnDamaged();
		}
		CaptureOpponent.Reset();
		SetStage(EGateAProofStage::PerfectParryStart);
		return false;
	}

	bool UpdatePerfectParryStart()
	{
		if (StageElapsed() < 0.20)
		{
			return false;
		}
		AEnemyCharacter* Enemy = EnemyAt(3);
		PrepareControlledPair(Enemy, 0.0f);
		ResetResolutionCapture();
		PerfectEnemyInitialHealth = Enemy->CurrentHealth;
		PerfectTokenReleaseBefore = EnemyAI(3)->GetTokenReleaseCountForTesting();
		if (!StartEnemyAttack(Enemy))
		{
			Fail(TEXT("Perfect-parry enemy could not start attack"));
			return false;
		}
		PerfectAttackGeneration = Enemy->CombatComponent->GetCurrentAttackGeneration();
		ExpectResolutionFrom(Enemy, PerfectAttackGeneration);
		SetStage(EGateAProofStage::PerfectParryAwaitWindow);
		return false;
	}

	bool UpdatePerfectParryAwaitWindow()
	{
		AEnemyCharacter* Enemy = EnemyAt(3);
		if (!HasFreshParryWindow(Enemy))
		{
			if (StageElapsed() > 3.0)
			{
				Fail(TEXT("Perfect-parry attack never published a high-confidence window"));
			}
			return false;
		}
		BridgeDefenderStart = Player->GetActorLocation();
		BridgeAttackerStart = Enemy->GetActorLocation();
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		PerfectResolution = PlayerCombat->GetLastInputDefenseResolutionForTesting();
		const bool bPerfect = PerfectResolution.Decision.Outcome == EDefenseOutcome::PerfectParry;
		const bool bParryActive = PlayerPaired->GetChainState() == EChainCounterState::ParryActive;
		const bool bConsumed = Enemy->CombatComponent->IsAttackConsumed(
			PerfectResolution.Decision.AttackInstance);
		const bool bTokenReleased = !EnemyAI(3)->HasAttackToken()
			&& EnemyAI(3)->GetTokenReleaseCountForTesting() == PerfectTokenReleaseBefore + 1;
		Test->TestTrue(TEXT("Reviewed Block Press commits perfect parry"), bPerfect);
		Test->TestTrue(TEXT("Perfect parry enters ParryActive bridge state"), bParryActive);
		Test->TestTrue(TEXT("Perfect parry consumes the exact attack generation"), bConsumed);
		Test->TestTrue(TEXT("Perfect parry releases the AI token exactly once"), bTokenReleased);
		if (!bPerfect || !bParryActive || !bConsumed || !bTokenReleased)
		{
			Fail(TEXT("Perfect-parry commit prerequisites failed"));
			return false;
		}
		CaptureFrame(TEXT("perfect_parry_bridge"));
		SetStage(EGateAProofStage::PerfectParryBridge);
		return false;
	}

	bool UpdatePerfectParryBridge()
	{
		AEnemyCharacter* Enemy = EnemyAt(3);
		if (PlayerPaired->GetChainState() == EChainCounterState::CounterWindow)
		{
			BridgeDefenderDistanceCm = FVector::Dist2D(BridgeDefenderStart, Player->GetActorLocation());
			BridgeAttackerDistanceCm = FVector::Dist2D(BridgeAttackerStart, Enemy->GetActorLocation());
			const bool bBridgeBudget = BridgeDefenderDistanceCm <= 75.0f + KINDA_SMALL_NUMBER
				&& BridgeAttackerDistanceCm <= 75.0f + KINDA_SMALL_NUMBER;
			Test->TestTrue(TEXT("Parry bridge keeps each role within 75 cm"), bBridgeBudget);
			Test->TestTrue(TEXT("CounterWindow is delayed until after ParryActive"), StageElapsed() > 0.0);
			CaptureFrame(TEXT("counter_window"));

			const bool bCanonicalCounterInput = PlayerCombat->GetDefaultLightAttack() == Attack.Get();
			Test->TestTrue(TEXT("Playable Light input resolves the reviewed Gate A attack"),
				bCanonicalCounterInput);
			PlayerCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
			const bool bCounterStarted = PlayerPaired->GetChainState() == EChainCounterState::CounterActive;
			Test->TestTrue(TEXT("Light input advances CounterWindow to paired counter"), bCounterStarted);
			RecordCase(TEXT("PerfectParryBridge"), bBridgeBudget && bCanonicalCounterInput && bCounterStarted,
				FString::Printf(TEXT("defender_cm=%.2f attacker_cm=%.2f generation=%d"),
					BridgeDefenderDistanceCm, BridgeAttackerDistanceCm, PerfectAttackGeneration));
			if (!bCounterStarted)
			{
				Fail(TEXT("Gate A paired counter did not start from the public input route"));
				return false;
			}
			SetStage(EGateAProofStage::PerfectParryCounter);
			return false;
		}
		if (PlayerPaired->GetChainState() == EChainCounterState::None)
		{
			Fail(TEXT("Parry bridge terminated before CounterWindow"));
		}
		else if (StageElapsed() > 6.0)
		{
			Fail(TEXT("Parry bridge marker did not open CounterWindow"));
		}
		return false;
	}

	bool UpdatePerfectParryCounter()
	{
		const EChainCounterState ChainState = PlayerPaired->GetChainState();
		if (ChainState == EChainCounterState::FinisherActive)
		{
			PerfectFinisherInitialHealth = EnemyAt(3)->CurrentHealth;
			PerfectFinisherStageGeneration =
				PlayerPaired->GetActiveDefenseSequenceContext().StageGeneration;
			RecordCase(TEXT("CounterToFinisherContinuity"), true,
				TEXT("counter marker auto-continued directly into FinisherActive"));
			CaptureFrame(TEXT("finisher_active"));
			SetStage(EGateAProofStage::PerfectParryFinisher);
			return false;
		}
		if (ChainState == EChainCounterState::FinisherReady)
		{
			Test->AddError(TEXT("Counter did not auto-continue at its reviewed FinisherReady marker"));
			PlayerCombat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
			if (PlayerPaired->GetChainState() == EChainCounterState::FinisherActive)
			{
				SetStage(EGateAProofStage::PerfectParryFinisher);
			}
			return false;
		}
		if (ChainState == EChainCounterState::None)
		{
			Fail(TEXT("Counter stage terminated without finisher continuity"));
		}
		else if (StageElapsed() > 10.0)
		{
			Fail(TEXT("Counter marker did not transition to finisher"));
		}
		return false;
	}

	bool UpdatePerfectParryFinisher()
	{
		if (PlayerPaired->GetChainState() == EChainCounterState::None)
		{
			AEnemyCharacter* Enemy = EnemyAt(3);
			const bool bCleanup = !PlayerPaired->IsInputBlocked()
				&& !PlayerPaired->IsPairedAnimationActive()
				&& PlayerPaired->GetPairedPartnerCount() == 0;
			const bool bSourceConsumed = Enemy->CombatComponent->IsAttackConsumed(
				PerfectResolution.Decision.AttackInstance);
			const bool bTokenClean = !EnemyAI(3)->HasAttackToken();
			const bool bLethal = Enemy->CurrentHealth <= KINDA_SMALL_NUMBER
				&& Enemy->IsDeadOrDying()
				&& EnemyAI(3)->CurrentState == EEnemyAIState::Dying;
			int32 FinisherDamageEvents = 0;
			int32 FinisherCleanupEvents = 0;
			for (const FDefenseTelemetryRecord& Record : PlayerCombat->GetDefenseTelemetry())
			{
				FinisherDamageEvents += Record.Event == EDefenseTelemetryEvent::StageDamage
					&& Record.StageName == TEXT("FinisherActive")
					&& Record.StageGeneration == PerfectFinisherStageGeneration
					? 1 : 0;
				FinisherCleanupEvents += Record.Event == EDefenseTelemetryEvent::Cleanup
					&& Record.StageName == TEXT("FinisherActive")
					&& Record.StageGeneration == PerfectFinisherStageGeneration
					&& Record.CleanupReason == TEXT("FinisherCompleted")
					? 1 : 0;
			}
			const bool bExactlyOnce = FinisherDamageEvents == 1;
			const bool bCompletedCleanup = FinisherCleanupEvents == 1;
			Test->TestTrue(TEXT("Finisher terminal cleanup restores input and paired ownership"), bCleanup);
			Test->TestTrue(TEXT("Finisher cleanup does not reopen consumed attack"), bSourceConsumed);
			Test->TestTrue(TEXT("Finisher cleanup leaves no AI token"), bTokenClean);
			Test->TestTrue(TEXT("Finisher applies lethal damage and removes the enemy from combat"), bLethal);
			Test->TestEqual(TEXT("Finisher stage damage is committed exactly once"),
				FinisherDamageEvents, 1);
			Test->TestEqual(TEXT("Finisher terminates through the canonical completed cleanup"),
				FinisherCleanupEvents, 1);
			RecordCase(
				TEXT("PerfectParryCounterFinisher"),
				bCleanup && bSourceConsumed && bTokenClean && bLethal
					&& bExactlyOnce && bCompletedCleanup,
				FString::Printf(
					TEXT("enemy_health=%.2f chain_initial=%.2f finisher_initial=%.2f damage_events=%d cleanup_events=%d token_releases=%d"),
					Enemy->CurrentHealth,
					PerfectEnemyInitialHealth,
					PerfectFinisherInitialHealth,
					FinisherDamageEvents,
					FinisherCleanupEvents,
					EnemyAI(3)->GetTokenReleaseCountForTesting()));
			CaptureFrame(TEXT("finisher_cleanup"));
			SetStage(EGateAProofStage::FinalCaptureWait);
			return false;
		}
		if (StageElapsed() > 14.0)
		{
			Fail(TEXT("Finisher did not reach terminal cleanup"));
		}
		return false;
	}

	bool UpdateFinalCaptureWait()
	{
		if (StageElapsed() < 0.75)
		{
			return false;
		}
		FinalizeEvidence();
		CleanupProofState();
		SetStage(EGateAProofStage::Done, false);
		return true;
	}

	void SetStage(const EGateAProofStage NewStage, const bool bCapture = true)
	{
		Stage = NewStage;
		StageStart = FPlatformTime::Seconds();
		if (bCapture)
		{
			CaptureFrame(StageName(NewStage));
		}
	}

	double StageElapsed() const
	{
		return FPlatformTime::Seconds() - StageStart;
	}

	void Fail(const FString& Message)
	{
		if (!bFatalFailure)
		{
			bFatalFailure = true;
			Test->AddError(Message);
			RecordCase(TEXT("FatalFailure"), false,
				FString::Printf(TEXT("stage=%s message=%s"), StageName(Stage), *Message));
		}
		if (Stage != EGateAProofStage::FinalCaptureWait && Stage != EGateAProofStage::Done)
		{
			SetStage(EGateAProofStage::FinalCaptureWait);
		}
	}

	void ResetResolutionCapture()
	{
		bHasResolution = false;
		LastResolution = {};
		ExpectedResolutionAttacker.Reset();
		ExpectedResolutionGeneration = 0;
	}

	void ExpectResolutionFrom(AEnemyCharacter* Enemy, const int32 AttackGeneration)
	{
		ExpectedResolutionAttacker = Enemy;
		ExpectedResolutionGeneration = AttackGeneration;
	}

	void HandleDefenseResolved(const FDefenseResolution& Resolution)
	{
		if (!ExpectedResolutionAttacker.IsValid()
			|| Resolution.Decision.AttackInstance.Attacker.Get() != ExpectedResolutionAttacker.Get()
			|| Resolution.Decision.AttackInstance.AttackGeneration != ExpectedResolutionGeneration)
		{
			return;
		}
		LastResolution = Resolution;
		bHasResolution = true;
		++ResolutionCount;
	}

	void HandleNormalContactAccounted(
		AActor* HitActor,
		const FContactInstanceId& ContactId,
		const int32 AcceptedHitCount)
	{
		AEnemyCharacter* Enemy = EnemyAt(0);
		if (Stage != EGateAProofStage::NormalBlockAwaitContact
			|| bNormalDuplicateChecked
			|| HitActor != Player.Get()
			|| !Enemy
			|| !Enemy->CombatComponent
			|| !Enemy->WeaponComponent)
		{
			return;
		}

		bNormalDuplicateChecked = true;
		NormalAcceptedBeforeDuplicate = AcceptedHitCount;
		const FAttackWindowInstanceId ActiveHitWindow = Enemy->CombatComponent->GetActiveAttackWindow(
			EAttackWindowKind::Hit);
		bNormalDuplicateSameWindow = ContactId.IsValid()
			&& ContactId.bUsesAttackWindow
			&& ActiveHitWindow.IsValid()
			&& ContactId.AttackWindow == ActiveHitWindow
			&& Enemy->WeaponComponent->GetActiveContactIdForTesting() == ContactId;
		const FHitResult DuplicateHit = MakeProofHit(Player.Get(), Enemy->GetActorLocation());
		const FDefenseContactRequest DuplicateRequest =
			Enemy->WeaponComponent->BuildDefenseContactRequestForTesting(
				DuplicateHit,
				Attack.Get());
		NormalDuplicateReceipt = Player->ResolveAndCommitCombatContact(DuplicateRequest);
		bNormalDuplicateCached = NormalDuplicateReceipt.CommitStatus
			== EDefenseCommitStatus::Cached;
		NormalAcceptedAfterDuplicate = Enemy->WeaponComponent->GetAcceptedHitCountForTesting();
	}

	void HandleImpactSoundPlaybackInvoked(
		UWorld* PlayedWorld,
		USoundBase* PlayedSound,
		const FVector& ImpactLocation,
		AActor* Attacker)
	{
		(void)ImpactLocation;
		if ((Stage == EGateAProofStage::NormalBlockAwaitContact
				|| Stage == EGateAProofStage::NormalBlockObserve)
			&& PlayedWorld == World.Get()
			&& PlayedSound
			&& Attacker == EnemyAt(0))
		{
			bNormalAudioPlaybackInvoked = true;
			NormalPlayedSound = PlayedSound;
		}
	}

	void RemoveResolutionBinding()
	{
		if (bResolutionBound && PlayerCombat.IsValid())
		{
			PlayerCombat->OnDefenseResolvedNative.RemoveAll(this);
		}
		bResolutionBound = false;
	}

	void RemoveProofBindings()
	{
		RemoveResolutionBinding();
		if (bNormalContactBinding)
		{
			if (AEnemyCharacter* Enemy = EnemyAt(0);
				Enemy && Enemy->WeaponComponent)
			{
				Enemy->WeaponComponent->OnRichContactAccountedForTesting.RemoveAll(this);
			}
		}
		bNormalContactBinding = false;
		if (bImpactSoundBinding)
		{
			UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.RemoveAll(this);
		}
		bImpactSoundBinding = false;
	}

	void CleanupProofState()
	{
		RemoveProofBindings();
		if (bDefenseDebugOverridden)
		{
			if (IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(
				TEXT("Combat.Defense.Debug")))
			{
				Debug->SetWithCurrentPriority(PreviousDefenseDebugValue);
			}
		}
		bDefenseDebugOverridden = false;
	}

	AEnemyCharacter* EnemyAt(const int32 Index) const
	{
		return Enemies.IsValidIndex(Index) ? Enemies[Index].Get() : nullptr;
	}

	UEnemyCombatAIComponent* EnemyAI(const int32 Index) const
	{
		AEnemyCharacter* Enemy = EnemyAt(Index);
		return Enemy ? Enemy->CombatAIComponent.Get() : nullptr;
	}

	void ConfigureEnemy(AEnemyCharacter* Enemy) const
	{
		UEnemyCombatAIComponent* AI = Enemy ? Enemy->CombatAIComponent.Get() : nullptr;
		if (!AI || !Attack.IsValid())
		{
			return;
		}
		FEnemyAttackConfig Config;
		Config.AttackData = Attack.Get();
		Config.MinRange = 0.0f;
		Config.MaxRange = 1000.0f;
		Config.SelectionWeight = 1.0f;
		AI->AvailableAttacks = {Config};
		AI->AttackSelectionMode = EEnemyAttackSelection::Single;
	}

	void DisableControllerLogic(AEnemyCharacter* Enemy) const
	{
		AController* Controller = Enemy ? Enemy->GetController() : nullptr;
		if (!Controller)
		{
			return;
		}
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

	void ResetCharacterCombat(ABaseCombatCharacter* Character) const
	{
		if (!Character)
		{
			return;
		}
		if (Character->CombatComponent)
		{
			Character->CombatComponent->ClearQueue(true);
			Character->CombatComponent->SetPhase(EAttackPhase::None);
		}
		if (Character->WeaponComponent && Character->WeaponComponent->IsHitDetectionEnabled())
		{
			Character->WeaponComponent->DisableHitDetection();
		}
		if (Character->GetMesh() && Character->GetMesh()->GetAnimInstance())
		{
			Character->GetMesh()->GetAnimInstance()->StopAllMontages(0.0f);
		}
		if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
		{
			Movement->StopMovementImmediately();
		}
	}

	void ResetPlayerFacing(const float PlayerYaw) const
	{
		if (!Player.IsValid())
		{
			return;
		}
		Player->SetActorLocationAndRotation(
			BaseLocation, FRotator(0.0f, PlayerYaw, 0.0f), false, nullptr, ETeleportType::TeleportPhysics);
		if (AController* Controller = Player->GetController())
		{
			Controller->SetControlRotation(FRotator::ZeroRotator);
		}
	}

	void PrepareControlledPair(AEnemyCharacter* Enemy, const float PlayerYaw)
	{
		CaptureOpponent = Enemy;
		TokenSubsystem->ResetAllTokens();
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
		ResetCharacterCombat(Player.Get());
		ResetPlayerFacing(PlayerYaw);
		Player->SetHealth(Player->MaxHealth);
		for (int32 Index = 0; Index < Enemies.Num(); ++Index)
		{
			AEnemyCharacter* Candidate = EnemyAt(Index);
			if (Candidate != Enemy)
			{
				Candidate->SetActorLocation(BaseLocation + FVector(1800.0f + Index * 150.0f, 1000.0f, 0.0f));
			}
		}
		ConfigureEnemy(Enemy);
		ResetCharacterCombat(Enemy);
		if (UEnemyCombatAIComponent* AI = Enemy ? Enemy->CombatAIComponent.Get() : nullptr)
		{
			AI->CancelQueuedAttackRequest();
			AI->SetCombatTarget(nullptr);
		}
		Enemy->SetHealth(Enemy->MaxHealth);
		Enemy->SetActorLocationAndRotation(
			BaseLocation + FVector(175.0f, 0.0f, 0.0f),
			FRotator(0.0f, 180.0f, 0.0f),
			false, nullptr, ETeleportType::TeleportPhysics);
	}

	bool StartEnemyAttack(AEnemyCharacter* Enemy) const
	{
		UEnemyCombatAIComponent* AI = Enemy ? Enemy->CombatAIComponent.Get() : nullptr;
		if (!AI)
		{
			return false;
		}
		ConfigureEnemy(Enemy);
		AI->SetCombatTarget(Player.Get());
		return AI->TryInitiateAttack() && AI->ExecuteAttack();
	}

	bool HasFreshParryWindow(AEnemyCharacter* Enemy) const
	{
		if (!Enemy || !Enemy->CombatComponent)
		{
			return false;
		}
		const FAttackExecutionSnapshot Snapshot = Enemy->CombatComponent->BuildAttackExecutionSnapshot();
		return Snapshot.ActiveParryWindow.IsValid()
			&& Snapshot.PredictedContact.bIsValid
			&& Snapshot.PredictedContact.Confidence == EDefensePredictionConfidence::High
			&& Snapshot.PredictedContact.IntendedTarget.Get() == Player.Get()
			&& Snapshot.PredictedContact.ContactSimulationTime > World->GetTimeSeconds();
	}

	void InjectWeaponContact(ABaseCombatCharacter* Source, ABaseCombatCharacter* Target) const
	{
		if (!Source || !Target || !Source->WeaponComponent || !Attack.IsValid())
		{
			return;
		}
		Source->WeaponComponent->ProcessHitForTesting(
			MakeProofHit(Target, Source->GetActorLocation()), Attack.Get());
	}

	EDefenseOutcome LastTelemetryOutcome(const UCombatComponent* Combat) const
	{
		if (!Combat)
		{
			return EDefenseOutcome::Rejected;
		}
		const TArray<FDefenseTelemetryRecord>& Records = Combat->GetDefenseTelemetry();
		for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
		{
			if (Records[Index].Event == EDefenseTelemetryEvent::Resolution)
			{
				return Records[Index].Outcome;
			}
		}
		return EDefenseOutcome::Rejected;
	}

	bool ShouldCaptureContinuousFrame()
	{
		if (Stage < EGateAProofStage::PerfectParryAwaitWindow
			|| Stage > EGateAProofStage::PerfectParryFinisher)
		{
			return false;
		}
		const double Now = FPlatformTime::Seconds();
		if (Now - LastContinuousCapture < 0.20)
		{
			return false;
		}
		LastContinuousCapture = Now;
		return true;
	}

	void CaptureFrame(const FString& Label)
	{
		if (!FApp::CanEverRender() || FScreenshotRequest::IsScreenshotRequested())
		{
			return;
		}

		APlayerController* PlayerController = Player.IsValid()
			? Cast<APlayerController>(Player->GetController())
			: nullptr;
		int32 ViewportWidth = 0;
		int32 ViewportHeight = 0;
		if (PlayerController)
		{
			PlayerController->GetViewportSize(ViewportWidth, ViewportHeight);
		}
		const auto ProjectActor = [PlayerController, ViewportWidth, ViewportHeight](
			const AActor* Actor,
			FVector2D& OutScreenPosition)
		{
			if (!PlayerController || !IsValid(Actor)
				|| ViewportWidth <= 0 || ViewportHeight <= 0)
			{
				return false;
			}

			FVector BoundsOrigin = Actor->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f);
			if (const ABaseCombatCharacter* Combatant = Cast<ABaseCombatCharacter>(Actor))
			{
				if (const USkeletalMeshComponent* Mesh = Combatant->GetMesh())
				{
					BoundsOrigin = Mesh->Bounds.Origin;
				}
			}
			else
			{
				FVector BoundsExtent = FVector::ZeroVector;
				Actor->GetActorBounds(false, BoundsOrigin, BoundsExtent);
			}
			const bool bProjected = PlayerController->ProjectWorldLocationToScreen(
				BoundsOrigin, OutScreenPosition, true);
			return bProjected
				&& OutScreenPosition.X >= 0.0f
				&& OutScreenPosition.X <= ViewportWidth
				&& OutScreenPosition.Y >= 0.0f
				&& OutScreenPosition.Y <= ViewportHeight;
		};

		FVector2D DefenderScreen = FVector2D::ZeroVector;
		const bool bDefenderInView = ProjectActor(Player.Get(), DefenderScreen);
		AEnemyCharacter* Source = CaptureOpponent.Get();
		const bool bSourceRequired = Source != nullptr;
		FVector2D SourceScreen = FVector2D::ZeroVector;
		bool bSourceInView = ProjectActor(Source, SourceScreen);
		if (!Source)
		{
			for (const TWeakObjectPtr<AEnemyCharacter>& Candidate : Enemies)
			{
				FVector2D CandidateScreen = FVector2D::ZeroVector;
				if (ProjectActor(Candidate.Get(), CandidateScreen))
				{
					Source = Candidate.Get();
					SourceScreen = CandidateScreen;
					bSourceInView = true;
					break;
				}
			}
		}
		const bool bFramingValid = bDefenderInView
			&& (!bSourceRequired || bSourceInView);
		bAllCapturedFramesFramed = bAllCapturedFramesFramed && bFramingValid;

		++RequestedFrameCount;
		const FString Filename = FString::Printf(TEXT("frame_%04d.png"), RequestedFrameCount);
		const FString AbsolutePath = FPaths::Combine(FramesDirectory, Filename);
		FScreenshotRequest::RequestScreenshot(AbsolutePath, false, false);
		TSharedPtr<FJsonObject> Frame = MakeShared<FJsonObject>();
		Frame->SetNumberField(TEXT("index"), RequestedFrameCount);
		Frame->SetStringField(TEXT("file"), Filename);
		Frame->SetStringField(TEXT("stage"), Label);
		Frame->SetNumberField(TEXT("simulation_time"), World.IsValid() ? World->GetTimeSeconds() : 0.0);
		Frame->SetNumberField(TEXT("viewport_width"), ViewportWidth);
		Frame->SetNumberField(TEXT("viewport_height"), ViewportHeight);
		Frame->SetStringField(TEXT("defender"),
			Player.IsValid() ? Player->GetName() : TEXT("None"));
		Frame->SetNumberField(TEXT("defender_screen_x"), DefenderScreen.X);
		Frame->SetNumberField(TEXT("defender_screen_y"), DefenderScreen.Y);
		Frame->SetBoolField(TEXT("defender_in_view"), bDefenderInView);
		Frame->SetStringField(TEXT("source"), IsValid(Source) ? Source->GetName() : TEXT("None"));
		Frame->SetNumberField(TEXT("source_screen_x"), SourceScreen.X);
		Frame->SetNumberField(TEXT("source_screen_y"), SourceScreen.Y);
		Frame->SetBoolField(TEXT("source_required"), bSourceRequired);
		Frame->SetBoolField(TEXT("source_in_view"), bSourceInView);
		Frame->SetBoolField(TEXT("framing_valid"), bFramingValid);
		CapturedFrames.Add(MakeShared<FJsonValueObject>(Frame));
	}

	void RecordCase(const FString& Name, const bool bPassed, const FString& Details)
	{
		if (RecordedCaseNames.Contains(Name))
		{
			Test->AddError(FString::Printf(TEXT("Gate A evidence recorded duplicate case '%s'"), *Name));
			bAllCasesPassed = false;
		}
		RecordedCaseNames.Add(Name);
		bAllCasesPassed = bAllCasesPassed && bPassed;
		TSharedPtr<FJsonObject> Case = MakeShared<FJsonObject>();
		Case->SetStringField(TEXT("name"), Name);
		Case->SetBoolField(TEXT("passed"), bPassed);
		Case->SetStringField(TEXT("details"), Details);
		Case->SetStringField(TEXT("stage"), StageName(Stage));
		Cases.Add(MakeShared<FJsonValueObject>(Case));
	}

	void FinalizeEvidence()
	{
		if (bEvidenceFinalized)
		{
			return;
		}
		bEvidenceFinalized = true;
		RemoveResolutionBinding();

		TArray<FDefenseTelemetryRecord> Telemetry;
		if (PlayerCombat.IsValid())
		{
			Telemetry.Append(PlayerCombat->GetDefenseTelemetry());
		}
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			if (Enemy.IsValid() && Enemy->CombatComponent)
			{
				Telemetry.Append(Enemy->CombatComponent->GetDefenseTelemetry());
			}
		}

		float MaxYawOverBudget = 0.0f;
		float MaxUnexpectedDisplacement = 0.0f;
		float MaxPelvisDelta = 0.0f;
		int32 AlignmentFrameCount = 0;
		bool bNormalResolutionTelemetry = false;
		bool bNormalPresentationTelemetry = false;
		TSet<FName> StageStarts;
		TSet<FName> StageTransitions;
		int32 CounterDamageEvents = 0;
		int32 FinisherDamageEvents = 0;
		int32 FinisherCompletedEvents = 0;
		for (const FDefenseTelemetryRecord& Record : Telemetry)
		{
			if (Record.InteractionId == NormalInteractionId)
			{
				bNormalResolutionTelemetry = bNormalResolutionTelemetry
					|| Record.Event == EDefenseTelemetryEvent::Resolution;
				bNormalPresentationTelemetry = bNormalPresentationTelemetry
					|| Record.Event == EDefenseTelemetryEvent::PresentationStart;
			}
			if (Record.Event == EDefenseTelemetryEvent::StageStart)
			{
				StageStarts.Add(Record.StageName);
			}
			if (Record.Event == EDefenseTelemetryEvent::StageTransition)
			{
				StageTransitions.Add(Record.StageName);
			}
			if (Record.Event == EDefenseTelemetryEvent::StageDamage)
			{
				CounterDamageEvents += Record.StageName == TEXT("CounterActive") ? 1 : 0;
				FinisherDamageEvents += Record.StageName == TEXT("FinisherActive") ? 1 : 0;
			}
			FinisherCompletedEvents += Record.Event == EDefenseTelemetryEvent::Cleanup
				&& Record.StageName == TEXT("FinisherActive")
				&& Record.CleanupReason == TEXT("FinisherCompleted")
				? 1 : 0;
			if (Record.Event != EDefenseTelemetryEvent::AlignmentFrame)
			{
				continue;
			}
			++AlignmentFrameCount;
			if (Record.FrameSimulationDelta > 0.0f && Record.MaximumTurnRate > 0.0f)
			{
				const float AllowedFrameYaw = Record.MaximumTurnRate * Record.FrameSimulationDelta;
				MaxYawOverBudget = FMath::Max(
					MaxYawOverBudget,
					FMath::Max(0.0f, FMath::Abs(Record.AppliedFrameYaw) - AllowedFrameYaw));
			}
			MaxUnexpectedDisplacement = FMath::Max(
				MaxUnexpectedDisplacement, Record.UnexpectedDisplacement.Size2D());
			MaxPelvisDelta = FMath::Max(MaxPelvisDelta, Record.PelvisDelta);
		}
		bool bCompleteCaseLedger = !ExpectedCaseNames.IsEmpty()
			&& RecordedCaseNames.Num() == ExpectedCaseNames.Num();
		for (const FString& ExpectedCase : ExpectedCaseNames)
		{
			bCompleteCaseLedger = bCompleteCaseLedger && RecordedCaseNames.Contains(ExpectedCase);
		}
		const bool bCompleteStageTelemetry = StageStarts.Contains(TEXT("ParryActive"))
			&& StageStarts.Contains(TEXT("CounterActive"))
			&& StageStarts.Contains(TEXT("FinisherActive"))
			&& StageTransitions.Contains(TEXT("CounterWindow"))
			&& CounterDamageEvents == 1
			&& FinisherDamageEvents == 1
			&& FinisherCompletedEvents == 1;
		if (!bFatalFailure)
		{
			Test->TestTrue(TEXT("Gate A evidence contains the complete unique case ledger"),
				bCompleteCaseLedger);
			Test->TestTrue(TEXT("Every recorded Gate A evidence case passed"), bAllCasesPassed);
			Test->TestTrue(TEXT("Normal block retained canonical resolution telemetry"),
				bNormalResolutionTelemetry);
			Test->TestTrue(TEXT("Normal block retained committed presentation telemetry"),
				bNormalPresentationTelemetry);
			Test->TestTrue(TEXT("Defense telemetry proves every paired stage and exactly-once damage"),
				bCompleteStageTelemetry);
			Test->TestTrue(TEXT("Defense telemetry contains evaluated alignment frames"),
				AlignmentFrameCount > 0);
			Test->TestTrue(TEXT("Final per-frame yaw stays within rate * simulation delta + 0.1 degrees"),
				MaxYawOverBudget <= 0.1f + KINDA_SMALL_NUMBER);
			Test->TestTrue(TEXT("Unexpected per-frame displacement stays within 10 cm"),
				MaxUnexpectedDisplacement <= 10.0f + KINDA_SMALL_NUMBER);
			Test->TestTrue(TEXT("Pelvis discontinuity stays within 15 cm"),
				MaxPelvisDelta <= 15.0f + KINDA_SMALL_NUMBER);
		}

		FString ResolvedTelemetryPath;
		FString TelemetryError;
		const FString TelemetryPath = FPaths::Combine(
			EvidenceDirectory, TEXT("defense-gate-a-telemetry.csv"));
		const bool bTelemetryWritten = DefenseTelemetry::WriteCsv(
			TelemetryPath, Telemetry, ResolvedTelemetryPath, TelemetryError);
		Test->TestTrue(TEXT("Gate A telemetry CSV is written"), bTelemetryWritten);
		if (!bTelemetryWritten)
		{
			Test->AddError(TelemetryError);
		}

		TArray<FString> RenderedFrames;
		IFileManager::Get().FindFiles(RenderedFrames, *FPaths::Combine(FramesDirectory, TEXT("*.png")), true, false);
		const bool bRenderedFrameCountComplete = !FApp::CanEverRender()
			|| (RequestedFrameCount > 0 && RenderedFrames.Num() == RequestedFrameCount);
		int32 DecodedFrameCount = 0;
		int32 NontrivialFrameCount = 0;
		for (const TSharedPtr<FJsonValue>& FrameValue : CapturedFrames)
		{
			const TSharedPtr<FJsonObject> Frame = FrameValue.IsValid()
				? FrameValue->AsObject()
				: nullptr;
			if (!Frame.IsValid())
			{
				continue;
			}

			FString Filename;
			Frame->TryGetStringField(TEXT("file"), Filename);
			FRenderedFrameValidation Validation;
			const bool bDecoded = AnalyzeRenderedFrameFile(
				FPaths::Combine(FramesDirectory, Filename), Validation);
			DecodedFrameCount += bDecoded && Validation.bDecoded ? 1 : 0;
			NontrivialFrameCount += Validation.bHasNontrivialPixels ? 1 : 0;
			Frame->SetBoolField(TEXT("image_decoded"), bDecoded && Validation.bDecoded);
			Frame->SetBoolField(TEXT("nontrivial_pixels"), Validation.bHasNontrivialPixels);
			Frame->SetNumberField(TEXT("image_width"), Validation.Width);
			Frame->SetNumberField(TEXT("image_height"), Validation.Height);
			Frame->SetNumberField(TEXT("channel_range"), Validation.ChannelRange);
			Frame->SetNumberField(TEXT("average_brightness"), Validation.AverageBrightness);
		}
		const bool bRenderedFramePixelsComplete = !FApp::CanEverRender()
			|| (RequestedFrameCount > 0
				&& DecodedFrameCount == RequestedFrameCount
				&& NontrivialFrameCount == RequestedFrameCount);
		const bool bRenderedFrameFramingComplete = !FApp::CanEverRender()
			|| (RequestedFrameCount > 0
				&& CapturedFrames.Num() == RequestedFrameCount
				&& bAllCapturedFramesFramed);
		if (FApp::CanEverRender() && !bFatalFailure)
		{
			Test->TestTrue(TEXT("Every requested Gate A frame was rendered"),
				bRenderedFrameCountComplete);
			Test->TestTrue(TEXT("Every rendered Gate A frame decodes with nontrivial pixels"),
				bRenderedFramePixelsComplete);
			Test->TestTrue(TEXT("Every Gate A frame contains its required combat participants"),
				bRenderedFrameFramingComplete);
		}
		TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 2);
		Root->SetStringField(TEXT("gate"), TEXT("A"));
		Root->SetStringField(TEXT("map"), GateAMapPackage);
		Root->SetStringField(TEXT("attack"), GateAAttackPath);
		Root->SetBoolField(TEXT("fatal_failure"), bFatalFailure);
		Root->SetBoolField(TEXT("complete_case_ledger"), bCompleteCaseLedger);
		Root->SetBoolField(TEXT("all_cases_passed"), bCompleteCaseLedger && bAllCasesPassed);
		Root->SetStringField(TEXT("proof_case_authority"), GateAManifestRelativePath);
		Root->SetNumberField(TEXT("manifest_proof_case_count"), ExpectedCaseNames.Num());
		Root->SetStringField(TEXT("execution_mode"),
			FApp::CanEverRender() ? TEXT("Rendered") : TEXT("Headless"));
		Root->SetBoolField(TEXT("render_capture_available"), FApp::CanEverRender());
		Root->SetNumberField(TEXT("requested_frames"), RequestedFrameCount);
		Root->SetNumberField(TEXT("rendered_frames"), RenderedFrames.Num());
		Root->SetBoolField(TEXT("rendered_frame_count_complete"), bRenderedFrameCountComplete);
		Root->SetNumberField(TEXT("decoded_frames"), DecodedFrameCount);
		Root->SetNumberField(TEXT("nontrivial_pixel_frames"), NontrivialFrameCount);
		Root->SetBoolField(TEXT("rendered_frame_pixels_complete"), bRenderedFramePixelsComplete);
		Root->SetBoolField(TEXT("rendered_frame_framing_complete"), bRenderedFrameFramingComplete);
		Root->SetNumberField(TEXT("resolution_count"), ResolutionCount);
		Root->SetStringField(TEXT("normal_contact_transport"), TEXT("physical weapon trace"));
		Root->SetArrayField(TEXT("cases"), Cases);
		Root->SetArrayField(TEXT("frames"), CapturedFrames);

		TSharedPtr<FJsonObject> Metrics = MakeShared<FJsonObject>();
		Metrics->SetNumberField(TEXT("normal_block_drift_cm"), NormalBlockDriftCm);
		Metrics->SetNumberField(TEXT("bridge_defender_distance_cm"), BridgeDefenderDistanceCm);
		Metrics->SetNumberField(TEXT("bridge_attacker_distance_cm"), BridgeAttackerDistanceCm);
		Metrics->SetNumberField(TEXT("max_yaw_over_budget_degrees"), MaxYawOverBudget);
		Metrics->SetNumberField(TEXT("max_unexpected_displacement_cm"), MaxUnexpectedDisplacement);
		Metrics->SetNumberField(TEXT("max_pelvis_delta_cm"), MaxPelvisDelta);
		Metrics->SetNumberField(TEXT("alignment_frame_count"), AlignmentFrameCount);
		Metrics->SetNumberField(TEXT("counter_stage_damage_events"), CounterDamageEvents);
		Metrics->SetNumberField(TEXT("finisher_stage_damage_events"), FinisherDamageEvents);
		Metrics->SetNumberField(TEXT("finisher_completed_cleanup_events"), FinisherCompletedEvents);
		Metrics->SetNumberField(TEXT("telemetry_record_count"), Telemetry.Num());
		Root->SetObjectField(TEXT("metrics"), Metrics);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
		const FString EvidencePath = FPaths::Combine(
			EvidenceDirectory, TEXT("defense-gate-a-evidence.json"));
		const bool bEvidenceWritten = FFileHelper::SaveStringToFile(
			Json, *EvidencePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test->TestTrue(TEXT("Gate A structured evidence JSON is written"), bEvidenceWritten);
	}

	FAutomationTestBase* Test = nullptr;
	double CommandStart = 0.0;
	double StageStart = 0.0;
	double LastContinuousCapture = -1.0;
	EGateAProofStage Stage = EGateAProofStage::WaitForPIE;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerCharacter> Player;
	TArray<TWeakObjectPtr<AEnemyCharacter>> Enemies;
	TWeakObjectPtr<UAttackData> Attack;
	TWeakObjectPtr<UCombatComponent> PlayerCombat;
	TWeakObjectPtr<UPairedAnimationComponent> PlayerPaired;
	TWeakObjectPtr<UCombatTokenSubsystem> TokenSubsystem;
	TWeakObjectPtr<AEnemyCharacter> ExpectedResolutionAttacker;
	TWeakObjectPtr<AEnemyCharacter> CaptureOpponent;
	FVector BaseLocation = FVector::ZeroVector;
	FDefenseResolution LastResolution;
	FDefenseResolution OutOfConeInputResolution;
	FDefenseResolution AlignmentDowngradeResolution;
	FDefenseResolution PerfectResolution;
	FDefenseContactReceipt NormalDuplicateReceipt;
	FDefenseInteractionId NormalInteractionId;
	bool bHasResolution = false;
	bool bResolutionBound = false;
	bool bNormalContactBinding = false;
	bool bImpactSoundBinding = false;
	bool bDefenseDebugOverridden = false;
	bool bFatalFailure = false;
	bool bEvidenceFinalized = false;
	bool bAllCapturedFramesFramed = true;
	bool bHandsOffTokenPolicyValid = true;
	bool bNormalAudioPlaybackInvoked = false;
	bool bNormalDuplicateChecked = false;
	bool bNormalDuplicateCached = false;
	bool bNormalDuplicateSameWindow = false;
	bool bNormalObserved = false;
	bool bAllCasesPassed = true;
	float NormalInitialHealth = 0.0f;
	float OutOfConeInitialHealth = 0.0f;
	float PerfectEnemyInitialHealth = 0.0f;
	float PerfectFinisherInitialHealth = 0.0f;
	float NormalBlockDriftCm = 0.0f;
	float BridgeDefenderDistanceCm = 0.0f;
	float BridgeAttackerDistanceCm = 0.0f;
	int32 NormalAcceptedBeforeDuplicate = 0;
	int32 NormalAcceptedAfterDuplicate = 0;
	int32 HandsOffConfiguredEnemyCount = 0;
	int32 HandsOffReviewedAttackCount = 0;
	int32 HandsOffControllerCount = 0;
	int32 HandsOffTargetingPlayerCount = 0;
	int32 HandsOffActiveStateCount = 0;
	int32 HandsOffMaxAttackGeneration = 0;
	int32 HandsOffMaxConcurrentTokens = 0;
	int32 HandsOffMaxQueueLength = 0;
	int32 PreviousDefenseDebugValue = 0;
	int32 NormalAttackGeneration = 0;
	int32 SecondAttackGeneration = 0;
	int32 PerfectAttackGeneration = 0;
	int32 PerfectFinisherStageGeneration = 0;
	int32 ExpectedResolutionGeneration = 0;
	int32 PerfectTokenReleaseBefore = 0;
	int32 ResolutionCount = 0;
	int32 RequestedFrameCount = 0;
	FVector NormalBlockStartLocation = FVector::ZeroVector;
	FVector BridgeDefenderStart = FVector::ZeroVector;
	FVector BridgeAttackerStart = FVector::ZeroVector;
	TWeakObjectPtr<UAnimMontage> NormalBlockMontage;
	TWeakObjectPtr<USoundBase> NormalPlayedSound;
	FString EvidenceDirectory;
	FString FramesDirectory;
	TArray<TSharedPtr<FJsonValue>> Cases;
	TArray<TSharedPtr<FJsonValue>> CapturedFrames;
	TArray<FString> ExpectedCaseNames;
	TSet<FString> RecordedCaseNames;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseRenderedFrameValidationTest,
	"KatanaCombat.Defense.GateA.RenderedFrameValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseRenderedFrameValidationTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FString Directory = FPaths::Combine(
		FPaths::ProjectSavedDir(), TEXT("Automation/DefenseRenderedFrameValidation"));
	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	IFileManager::Get().MakeDirectory(*Directory, true);

	FImage UniformImage(64, 64, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	FMemory::Memzero(UniformImage.RawData.GetData(), UniformImage.RawData.Num());
	const FString UniformPath = FPaths::Combine(Directory, TEXT("uniform.png"));
	TestTrue(TEXT("The uniform PNG fixture should serialize"),
		FImageUtils::SaveImageByExtension(*UniformPath, UniformImage));
	FRenderedFrameValidation UniformValidation;
	TestTrue(TEXT("A uniform PNG should still decode"),
		AnalyzeRenderedFrameFile(UniformPath, UniformValidation));
	TestFalse(TEXT("A uniform PNG must not count as rendered proof"),
		UniformValidation.bHasNontrivialPixels);

	FImage PatternImage(64, 64, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	FColor* PatternPixels = reinterpret_cast<FColor*>(PatternImage.RawData.GetData());
	for (int32 Y = 0; Y < PatternImage.SizeY; ++Y)
	{
		for (int32 X = 0; X < PatternImage.SizeX; ++X)
		{
			PatternPixels[Y * PatternImage.SizeX + X] = (X + Y) % 2 == 0
				? FColor(220, 40, 20)
				: FColor(10, 80, 210);
		}
	}
	const FString PatternPath = FPaths::Combine(Directory, TEXT("pattern.png"));
	TestTrue(TEXT("The patterned PNG fixture should serialize"),
		FImageUtils::SaveImageByExtension(*PatternPath, PatternImage));
	FRenderedFrameValidation PatternValidation;
	TestTrue(TEXT("A patterned PNG should decode"),
		AnalyzeRenderedFrameFile(PatternPath, PatternValidation));
	TestTrue(TEXT("A patterned PNG should count as nontrivial rendered proof"),
		PatternValidation.bHasNontrivialPixels);

	const FString MalformedPath = FPaths::Combine(Directory, TEXT("malformed.png"));
	TestTrue(TEXT("The malformed fixture should serialize"),
		FFileHelper::SaveStringToFile(TEXT("not a png"), *MalformedPath));
	FRenderedFrameValidation MalformedValidation;
	TestFalse(TEXT("Malformed PNG bytes must be rejected"),
		AnalyzeRenderedFrameFile(MalformedPath, MalformedValidation));

	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseGateAPIEProofTest,
	"KatanaCombat.Defense.GateA.PIEProof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseGateAPIEProofTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> ManifestErrors;
	const FString ManifestPath = FPaths::Combine(
		FPaths::ProjectDir(), GateAManifestRelativePath);
	if (!FDefenseAssetValidationService::LoadManifestFile(
		ManifestPath, Manifest, ManifestErrors))
	{
		for (const FString& Error : ManifestErrors)
		{
			AddError(Error);
		}
		return false;
	}
	if (FPackageName::ObjectPathToPackageName(Manifest.Map) != GateAMapPackage
		|| !Manifest.Attacks.ContainsByPredicate([](const FDefenseProofAttackEntry& Entry)
		{
			return Entry.AttackData == GateAAttackPath;
		})
		|| Manifest.ProofCases.IsEmpty())
	{
		AddError(TEXT("Gate A PIE proof constants do not match the canonical manifest"));
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(GateAMapPackage));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FDefenseGateAPIEProofCommand(this, Manifest.ProofCases));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
