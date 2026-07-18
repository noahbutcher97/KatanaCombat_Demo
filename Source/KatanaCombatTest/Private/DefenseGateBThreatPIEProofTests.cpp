// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/TargetingComponent.h"
#include "Data/DefenseConfiguration.h"
#include "Debug/DefenseMatrixProofDirector.h"
#include "Debug/DefenseTelemetry.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"

namespace
{
constexpr TCHAR GateBThreatMapPackage[] =
	TEXT("/Game/ProjectFiles/Levels/Test/Lvl_DefenseMatrix");
constexpr TCHAR PlayerFixtureTag[] = TEXT("DefenseMatrix.Player");
const FName LeftAnchorTag = TEXT("DefenseMatrix.Anchor.Left");
const FName RightAnchorTag = TEXT("DefenseMatrix.Anchor.Right");
const FName FirstThreatCase = TEXT("NormalBlockMiddleLeft");
const FName SecondThreatCase = TEXT("NormalBlockMiddleRight");

enum class EGateBThreatStage : uint8
{
	WaitForPIE,
	Settle,
	StartPair,
	AwaitActiveAttacks,
	CameraSettle,
	AwaitCapture,
	RunScenarios,
	Finalize,
	Done
};

const TCHAR* ThreatStageName(const EGateBThreatStage Stage)
{
	switch (Stage)
	{
	case EGateBThreatStage::WaitForPIE: return TEXT("WaitForPIE");
	case EGateBThreatStage::Settle: return TEXT("Settle");
	case EGateBThreatStage::StartPair: return TEXT("StartPair");
	case EGateBThreatStage::AwaitActiveAttacks: return TEXT("AwaitActiveAttacks");
	case EGateBThreatStage::CameraSettle: return TEXT("CameraSettle");
	case EGateBThreatStage::AwaitCapture: return TEXT("AwaitCapture");
	case EGateBThreatStage::RunScenarios: return TEXT("RunScenarios");
	case EGateBThreatStage::Finalize: return TEXT("Finalize");
	case EGateBThreatStage::Done: return TEXT("Done");
	default: return TEXT("Unknown");
	}
}

bool IsNontrivialImage(const FString& Filename)
{
	FImage Image;
	if (!FImageUtils::LoadImage(*Filename, Image)
		|| Image.SizeX < 32 || Image.SizeY < 32 || Image.NumSlices != 1)
	{
		return false;
	}
	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const int64 PixelCount = static_cast<int64>(Image.SizeX) * Image.SizeY;
	if (Image.RawData.Num() != PixelCount * static_cast<int64>(sizeof(FColor)))
	{
		return false;
	}
	const FColor* Pixels = reinterpret_cast<const FColor*>(Image.RawData.GetData());
	const int64 Stride = FMath::Max<int64>(1, PixelCount / 4096);
	int32 Minimum = 255;
	int32 Maximum = 0;
	for (int64 Index = 0; Index < PixelCount; Index += Stride)
	{
		Minimum = FMath::Min(Minimum, FMath::Min3(
			static_cast<int32>(Pixels[Index].R),
			static_cast<int32>(Pixels[Index].G),
			static_cast<int32>(Pixels[Index].B)));
		Maximum = FMath::Max(Maximum, FMath::Max3(
			static_cast<int32>(Pixels[Index].R),
			static_cast<int32>(Pixels[Index].G),
			static_cast<int32>(Pixels[Index].B)));
	}
	return Maximum - Minimum >= 8;
}

class FDefenseGateBThreatPIEProofCommand final : public IAutomationLatentCommand
{
public:
	explicit FDefenseGateBThreatPIEProofCommand(FAutomationTestBase* InTest)
		: Test(InTest)
		, CommandStart(FPlatformTime::Seconds())
		, StageStart(CommandStart)
	{
		EvidenceDirectory = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("DefenseProof/GateB/Threat"),
			FApp::CanEverRender() ? TEXT("Rendered") : TEXT("Headless"));
		FramesDirectory = FPaths::Combine(EvidenceDirectory, TEXT("frames"));
		ScreenshotPath = FPaths::Combine(FramesDirectory, TEXT("two_active_threats.png"));
	}

	virtual ~FDefenseGateBThreatPIEProofCommand() override
	{
		Cleanup();
	}

	virtual bool Update() override
	{
		switch (Stage)
		{
		case EGateBThreatStage::WaitForPIE: return UpdateWaitForPIE();
		case EGateBThreatStage::Settle: return UpdateSettle();
		case EGateBThreatStage::StartPair: return UpdateStartPair();
		case EGateBThreatStage::AwaitActiveAttacks: return UpdateAwaitActiveAttacks();
		case EGateBThreatStage::CameraSettle: return UpdateCameraSettle();
		case EGateBThreatStage::AwaitCapture: return UpdateAwaitCapture();
		case EGateBThreatStage::RunScenarios: return UpdateRunScenarios();
		case EGateBThreatStage::Finalize: return UpdateFinalize();
		case EGateBThreatStage::Done: return true;
		default: Fail(TEXT("invalid latent stage")); return false;
		}
	}

private:
	bool UpdateWaitForPIE()
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!PIEWorld)
		{
			if (Elapsed(CommandStart) > 20.0)
			{
				Fail(TEXT("PIE world did not start within 20 seconds"));
			}
			return false;
		}

		TArray<APlayerCharacter*> Players;
		TArray<ADefenseMatrixProofDirector*> Directors;
		for (TActorIterator<APlayerCharacter> It(PIEWorld); It; ++It)
		{
			if (It->ActorHasTag(PlayerFixtureTag))
			{
				Players.Add(*It);
			}
		}
		for (TActorIterator<ADefenseMatrixProofDirector> It(PIEWorld); It; ++It)
		{
			Directors.Add(*It);
		}
		if (Players.Num() != 1 || Directors.Num() != 1)
		{
			Fail(FString::Printf(
				TEXT("expected exactly one tagged player and director (players=%d directors=%d)"),
				Players.Num(), Directors.Num()));
			return false;
		}

		World = PIEWorld;
		Player = Players[0];
		Director = Directors[0];
		PlayerCombat = Player->GetCombatComponent();
		Targeting = Player->GetTargetingComponent();
		Tokens = PIEWorld->GetGameInstance()
			? PIEWorld->GetGameInstance()->GetSubsystem<UCombatTokenSubsystem>()
			: nullptr;
		if (!PlayerCombat.IsValid() || !Targeting.IsValid() || !Tokens.IsValid())
		{
			Fail(TEXT("fixture lacks combat, targeting, or token authority"));
			return false;
		}

		IFileManager::Get().DeleteDirectory(*EvidenceDirectory, false, true);
		IFileManager::Get().MakeDirectory(*FramesDirectory, true);
		if (IConsoleVariable* DefenseDebug = IConsoleManager::Get().FindConsoleVariable(
			TEXT("Combat.Defense.Debug")))
		{
			PreviousDefenseDebug = DefenseDebug->GetInt();
			DefenseDebug->SetWithCurrentPriority(1);
			bDefenseDebugCaptured = true;
		}
		Director->ResetFixture();
		if (!Director->WasLastResetComplete())
		{
			Fail(TEXT("initial fixture reset did not complete"));
			return false;
		}
		PlayerCombat->ClearDefenseTelemetry();
		SetStage(EGateBThreatStage::Settle);
		return false;
	}

	bool UpdateSettle()
	{
		if (StageElapsed() < 0.25)
		{
			return false;
		}
		SetStage(EGateBThreatStage::StartPair);
		return false;
	}

	bool UpdateStartPair()
	{
		if (!Director->StartNamedThreatPair(FirstThreatCase, SecondThreatCase))
		{
			Fail(TEXT("director rejected the reviewed two-threat pair"));
			return false;
		}
		FirstEnemy = Director->GetFixtureEnemy(LeftAnchorTag);
		SecondEnemy = Director->GetFixtureEnemy(RightAnchorTag);
		FirstCombat = FirstEnemy.IsValid() ? FirstEnemy->GetCombatComponent() : nullptr;
		SecondCombat = SecondEnemy.IsValid() ? SecondEnemy->GetCombatComponent() : nullptr;
		FirstAI = FirstEnemy.IsValid() ? FirstEnemy->GetCombatAIComponent() : nullptr;
		SecondAI = SecondEnemy.IsValid() ? SecondEnemy->GetCombatAIComponent() : nullptr;
		if (!FirstCombat.IsValid() || !SecondCombat.IsValid()
			|| !FirstAI.IsValid() || !SecondAI.IsValid())
		{
			Fail(TEXT("two-threat pair did not resolve both combatants"));
			return false;
		}
		OriginalFirstId = FirstCombat->GetCombatantStableId();
		OriginalSecondId = SecondCombat->GetCombatantStableId();
		bStableIdsCaptured = true;
		PlayerStartHealth = Player->CurrentHealth;
		bDamageSuppressionObserved = Player->HitReactionComponent
			&& FMath::IsNearlyZero(Player->HitReactionComponent->DamageResistance)
			&& Player->HitReactionComponent->bHasSuperArmor;
		Test->TestTrue(TEXT("Two-threat fixture suppresses damage without disabling defense"),
			bDamageSuppressionObserved);
		SetStage(EGateBThreatStage::AwaitActiveAttacks);
		return false;
	}

	bool UpdateAwaitActiveAttacks()
	{
		const FAttackExecutionSnapshot FirstSnapshot =
			FirstCombat->BuildAttackExecutionSnapshot();
		const FAttackExecutionSnapshot SecondSnapshot =
			SecondCombat->BuildAttackExecutionSnapshot();
		const bool bReady = FirstAI->HasAttackToken() && SecondAI->HasAttackToken()
			&& FirstAI->IsAttacking() && SecondAI->IsAttacking()
			&& FirstSnapshot.AttackInstance.IsValid()
			&& SecondSnapshot.AttackInstance.IsValid()
			&& FirstSnapshot.AttackPhase != EAttackPhase::None
			&& SecondSnapshot.AttackPhase != EAttackPhase::None
			&& FirstSnapshot.ActiveMontage.IsValid()
			&& SecondSnapshot.ActiveMontage.IsValid()
			&& Tokens->GetActiveAttackerCount() == 2;
		if (!bReady)
		{
			if (StageElapsed() > 5.0)
			{
				Fail(TEXT("both physical attacks did not become active under the two-token fixture"));
			}
			return false;
		}

		FirstGeneration = FirstSnapshot.AttackInstance.AttackGeneration;
		SecondGeneration = SecondSnapshot.AttackInstance.AttackGeneration;
		FCombatantStableId FirstId;
		FirstId.Value = 20;
		FCombatantStableId SecondId;
		SecondId.Value = 5;
		FirstCombat->SetCombatantStableIdForTesting(FirstId);
		SecondCombat->SetCombatantStableIdForTesting(SecondId);
		bPreviousGamePaused = UGameplayStatics::IsGamePaused(World.Get());
		bWorldPausedForArbitration = UGameplayStatics::SetGamePaused(World.Get(), true);
		bPauseNeedsRestore = bWorldPausedForArbitration;
		if (!bWorldPausedForArbitration)
		{
			Fail(TEXT("could not pause the physical attacks for deterministic arbitration"));
			return false;
		}
		const FVector PlayerLocation = Player->GetActorLocation();
		const FVector FirstLocation = PlayerLocation + FVector(220.0f, -150.0f, 0.0f);
		const FVector SecondLocation = PlayerLocation + FVector(220.0f, 150.0f, 0.0f);
		FirstEnemy->SetActorLocationAndRotation(
			FirstLocation, (PlayerLocation - FirstLocation).Rotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
		SecondEnemy->SetActorLocationAndRotation(
			SecondLocation, (PlayerLocation - SecondLocation).Rotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
		bTwoPhysicalAttacksObserved = true;
		Test->TestTrue(TEXT("Two distinct physical attacks are active concurrently"), true);

		if (FApp::CanEverRender())
		{
			if (!ConfigureCamera())
			{
				Fail(TEXT("could not configure a three-actor proof camera"));
				return false;
			}
			SetStage(EGateBThreatStage::CameraSettle);
		}
		else
		{
			SetStage(EGateBThreatStage::RunScenarios);
		}
		return false;
	}

	bool UpdateCameraSettle()
	{
		if (StageElapsed() < 0.20 || FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		if (!CaptureThreatFrame())
		{
			Fail(TEXT("player and both active attackers were not visibly framed"));
			return false;
		}
		SetStage(EGateBThreatStage::AwaitCapture);
		return false;
	}

	bool UpdateAwaitCapture()
	{
		if (FScreenshotRequest::IsScreenshotRequested() || !FPaths::FileExists(ScreenshotPath))
		{
			if (StageElapsed() > 5.0)
			{
				Fail(TEXT("two-threat screenshot was not written"));
			}
			return false;
		}
		SetStage(EGateBThreatStage::RunScenarios);
		return false;
	}

	bool UpdateRunScenarios()
	{
		const UDefenseConfiguration* Configuration =
			PlayerCombat->GetEffectiveDefenseConfiguration();
		if (!Configuration || Configuration->ThreatLockMinSeconds <= 0.0f
			|| Configuration->ThreatSwitchLeadSeconds <= 0.0f
			|| Configuration->MaximumHighConfidencePredictionAge <= 0.0f)
		{
			Fail(TEXT("authored defense configuration lacks positive threat hysteresis values"));
			return false;
		}
		LockMinimum = Configuration->ThreatLockMinSeconds;
		SwitchLead = Configuration->ThreatSwitchLeadSeconds;
		MaximumPredictionAge = Configuration->MaximumHighConfidencePredictionAge;
		PlayerCombat->EndBlock();

		const double Start = World->GetTimeSeconds();
		PlayerCombat->ClearGuardThreat(EThreatClearReason::NoCandidates);
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), Start + 1.0, Start,
			EDefensePredictionConfidence::High);
		PublishThreat(SecondCombat.Get(), SecondEnemy.Get(), Start + 1.0, Start,
			EDefensePredictionConfidence::High);
		Targeting->ResetAllTargetsInRangeCallCountForTesting();
		FDefenseThreatSelectionResult Result = PlayerCombat->SelectDefenseThreat(Start);
		RecordScenario(TEXT("StableTie"), Result.bFound
			&& Result.SelectedThreat.StableId.Value == 5
			&& Targeting->GetAllTargetsInRangeCallCountForTesting() == 1,
			Result, Start + 1.0, Start + 1.0,
			Targeting->GetAllTargetsInRangeCallCountForTesting(), TEXT("InitialLock"));

		const double LockStart = Start + 0.01;
		PlayerCombat->ClearGuardThreat(EThreatClearReason::NoCandidates);
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), LockStart + 1.0, LockStart,
			EDefensePredictionConfidence::High);
		PublishThreat(SecondCombat.Get(), SecondEnemy.Get(), LockStart + 1.4, LockStart,
			EDefensePredictionConfidence::High);
		Targeting->ResetAllTargetsInRangeCallCountForTesting();
		Result = PlayerCombat->SelectDefenseThreat(LockStart);
		RecordScenario(TEXT("EarlierInitialDeadline"), Result.bFound
			&& Result.SelectedThreat.StableId.Value == 20
			&& Targeting->GetAllTargetsInRangeCallCountForTesting() == 1,
			Result, LockStart + 1.0, LockStart + 1.4,
			Targeting->GetAllTargetsInRangeCallCountForTesting(), TEXT("InitialLock"));

		const double InsideLock = LockStart + LockMinimum * 0.5;
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), InsideLock + 1.0, InsideLock,
			EDefensePredictionConfidence::High);
		PublishThreat(SecondCombat.Get(), SecondEnemy.Get(), InsideLock + 0.2, InsideLock,
			EDefensePredictionConfidence::High);
		Targeting->ResetAllTargetsInRangeCallCountForTesting();
		Result = PlayerCombat->SelectDefenseThreat(InsideLock);
		RecordScenario(TEXT("MinimumLockRetains"), Result.bFound
			&& Result.SelectedThreat.StableId.Value == 20
			&& Targeting->GetAllTargetsInRangeCallCountForTesting() == 1,
			Result, InsideLock + 1.0, InsideLock + 0.2,
			Targeting->GetAllTargetsInRangeCallCountForTesting(), TEXT("LockRetained"));

		const double AfterLock = LockStart + LockMinimum + 0.02;
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), AfterLock + 1.0, AfterLock,
			EDefensePredictionConfidence::High);
		PublishThreat(SecondCombat.Get(), SecondEnemy.Get(),
			AfterLock + 1.0 - SwitchLead * 0.5, AfterLock,
			EDefensePredictionConfidence::High);
		Targeting->ResetAllTargetsInRangeCallCountForTesting();
		Result = PlayerCombat->SelectDefenseThreat(AfterLock);
		RecordScenario(TEXT("InsufficientLeadRetains"), Result.bFound
			&& Result.SelectedThreat.StableId.Value == 20
			&& Targeting->GetAllTargetsInRangeCallCountForTesting() == 1,
			Result, AfterLock + 1.0, AfterLock + 1.0 - SwitchLead * 0.5,
			Targeting->GetAllTargetsInRangeCallCountForTesting(), TEXT("LockRetained"));

		const double SwitchTime = AfterLock + 0.01;
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), SwitchTime + 1.0, SwitchTime,
			EDefensePredictionConfidence::High);
		PublishThreat(SecondCombat.Get(), SecondEnemy.Get(),
			SwitchTime + 1.0 - SwitchLead - 0.05, SwitchTime,
			EDefensePredictionConfidence::High);
		Targeting->ResetAllTargetsInRangeCallCountForTesting();
		Result = PlayerCombat->SelectDefenseThreat(SwitchTime);
		RecordScenario(TEXT("SufficientLeadSwitches"), Result.bFound
			&& Result.SelectedThreat.StableId.Value == 5
			&& Targeting->GetAllTargetsInRangeCallCountForTesting() == 1,
			Result, SwitchTime + 1.0,
			SwitchTime + 1.0 - SwitchLead - 0.05,
			Targeting->GetAllTargetsInRangeCallCountForTesting(), TEXT("EarlierDeadline"));

		PlayerCombat->ClearGuardThreat(EThreatClearReason::NoCandidates);
		const double GuardTime = World->GetTimeSeconds();
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), GuardTime + 0.60, GuardTime,
			EDefensePredictionConfidence::Low);
		PublishThreat(SecondCombat.Get(), SecondEnemy.Get(), GuardTime + 0.25,
			GuardTime - MaximumPredictionAge - 0.05,
			EDefensePredictionConfidence::High);
		Targeting->ResetAllTargetsInRangeCallCountForTesting();
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		const FDefenseResolution GuardResolution =
			PlayerCombat->GetLastInputDefenseResolutionForTesting();
		const FAttackExecutionSnapshot GuardThreat =
			PlayerCombat->GetLockedDefenseThreat();
		const bool bGuardFallback = GuardResolution.Decision.Outcome
				== EDefenseOutcome::GuardEntered
			&& GuardResolution.Decision.LockedThreatId.Value == 5
			&& GuardThreat.StableId.Value == 5
			&& GuardThreat.PredictedContact.Confidence
				== EDefensePredictionConfidence::Low
			&& !GuardThreat.bHasCredibleIntent
			&& Targeting->GetAllTargetsInRangeCallCountForTesting() == 1;
		FDefenseThreatSelectionResult GuardResult;
		GuardResult.bFound = GuardThreat.AttackInstance.IsValid();
		GuardResult.SelectedThreat = GuardThreat;
		RecordScenario(TEXT("StalePredictionGuardFallback"), bGuardFallback,
			GuardResult,
			GuardTime + 0.60, GuardTime + 0.25,
			Targeting->GetAllTargetsInRangeCallCountForTesting(), TEXT("InitialLock"));
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);

		const double InvalidTime = World->GetTimeSeconds();
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), InvalidTime + 0.5, InvalidTime,
			EDefensePredictionConfidence::High);
		PublishThreat(SecondCombat.Get(), SecondEnemy.Get(), InvalidTime + 0.2, InvalidTime,
			EDefensePredictionConfidence::High);
		PlayerCombat->ClearGuardThreat(EThreatClearReason::NoCandidates);
		Result = PlayerCombat->SelectDefenseThreat(InvalidTime);
		const bool bSecondLocked = Result.bFound && Result.SelectedThreat.StableId.Value == 5;
		SecondAI->AbortAttack();
		PublishThreat(FirstCombat.Get(), FirstEnemy.Get(), InvalidTime + 0.4,
			InvalidTime + 0.01, EDefensePredictionConfidence::High);
		Targeting->ResetAllTargetsInRangeCallCountForTesting();
		Result = PlayerCombat->SelectDefenseThreat(InvalidTime + 0.01);
		RecordScenario(TEXT("CurrentInvalidSwitches"), bSecondLocked && Result.bFound
			&& Result.SelectedThreat.StableId.Value == 20
			&& Targeting->GetAllTargetsInRangeCallCountForTesting() == 1,
			Result, InvalidTime + 0.4, InvalidTime + 0.2,
			Targeting->GetAllTargetsInRangeCallCountForTesting(), TEXT("CurrentInvalid"));

		Test->TestTrue(TEXT("All two-active-threat arbitration scenarios pass"),
			bAllScenariosPassed);
		bPlayerHealthPreserved = FMath::IsNearlyEqual(
			Player->CurrentHealth, PlayerStartHealth);
		Test->TestTrue(TEXT("Threat proof causes no player damage"),
			bPlayerHealthPreserved);
		SetStage(EGateBThreatStage::Finalize);
		return false;
	}

	bool UpdateFinalize()
	{
		if (FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		const bool bRenderedFrameValid = !FApp::CanEverRender()
			|| (bFrameFramed && FPaths::FileExists(ScreenshotPath)
				&& IsNontrivialImage(ScreenshotPath));
		if (FApp::CanEverRender() && !bFatalFailure)
		{
			Test->TestTrue(TEXT("Two-threat frame is decoded, nontrivial, and frames all actors"),
				bRenderedFrameValid);
		}

		TArray<FDefenseTelemetryRecord> Telemetry;
		if (PlayerCombat.IsValid())
		{
			Telemetry = PlayerCombat->GetDefenseTelemetry();
		}
		FString CsvPath;
		FString CsvError;
		const bool bCsvWritten = DefenseTelemetry::WriteCsv(
			FPaths::Combine(EvidenceDirectory, TEXT("defense-gate-b-threat-telemetry.csv")),
			Telemetry, CsvPath, CsvError);
		Test->TestTrue(TEXT("Two-threat telemetry CSV is written"), bCsvWritten);
		if (!bCsvWritten)
		{
			Test->AddError(CsvError);
		}

		RestoreFixture();
		const bool bDefaultPolicyCaptured = Director.IsValid()
			&& Director->GetCapturedMaxConcurrentAttackers() == 1;
		Test->TestTrue(TEXT("Proof director captured the default one-attacker policy"),
			bDefaultPolicyCaptured);
		const bool bPassed = !bFatalFailure && bTwoPhysicalAttacksObserved
			&& bAllScenariosPassed && bFixtureRestored && bRenderedFrameValid
			&& bDefaultPolicyCaptured && bDamageSuppressionObserved
			&& bPlayerHealthPreserved && bWorldPausedForArbitration;
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 1);
		Root->SetStringField(TEXT("gate"), TEXT("B"));
		Root->SetStringField(TEXT("proof"), TEXT("TwoActiveThreat"));
		Root->SetStringField(TEXT("map"), GateBThreatMapPackage);
		Root->SetStringField(TEXT("execution_mode"),
			FApp::CanEverRender() ? TEXT("Rendered") : TEXT("Headless"));
		Root->SetBoolField(TEXT("visual_capture_applicable"), FApp::CanEverRender());
		Root->SetBoolField(TEXT("frames_complete"),
			FApp::CanEverRender() && bRenderedFrameValid);
		Root->SetBoolField(TEXT("passed"), bPassed);
		Root->SetBoolField(TEXT("two_physical_attacks_observed"),
			bTwoPhysicalAttacksObserved);
		Root->SetBoolField(TEXT("damage_suppressed_for_arbitration"),
			bDamageSuppressionObserved);
		Root->SetBoolField(TEXT("player_health_preserved"), bPlayerHealthPreserved);
		Root->SetBoolField(TEXT("world_paused_after_attacks_became_active"),
			bWorldPausedForArbitration);
		Root->SetBoolField(TEXT("mirrored_threat_geometry_applied"), true);
		Root->SetNumberField(TEXT("first_attack_generation"), FirstGeneration);
		Root->SetNumberField(TEXT("second_attack_generation"), SecondGeneration);
		Root->SetNumberField(TEXT("first_stable_id"), 20);
		Root->SetNumberField(TEXT("second_stable_id"), 5);
		Root->SetNumberField(TEXT("threat_lock_min_seconds"), LockMinimum);
		Root->SetNumberField(TEXT("threat_switch_lead_seconds"), SwitchLead);
		Root->SetNumberField(TEXT("maximum_prediction_age_seconds"), MaximumPredictionAge);
		Root->SetBoolField(TEXT("fixture_restored"), bFixtureRestored);
		Root->SetBoolField(TEXT("default_token_policy_captured"),
			bDefaultPolicyCaptured);
		Root->SetArrayField(TEXT("scenarios"), ScenarioEvidence);
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		Test->TestTrue(TEXT("Two-threat structured evidence JSON is written"),
			FFileHelper::SaveStringToFile(Json,
				*FPaths::Combine(EvidenceDirectory,
					TEXT("defense-gate-b-threat-evidence.json")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
		SetStage(EGateBThreatStage::Done);
		return false;
	}

	void PublishThreat(
		UCombatComponent* Combat,
		AEnemyCharacter* Enemy,
		const double ContactTime,
		const double PublishedTime,
		const EDefensePredictionConfidence Confidence) const
	{
		FAttackThreatPrediction Prediction;
		Prediction.IntendedTarget = Player.Get();
		Prediction.PathOrigin = Enemy->GetActorLocation();
		Prediction.PathDirection = (Player->GetActorLocation()
			- Enemy->GetActorLocation()).GetSafeNormal();
		Prediction.PredictedContactPoint = Player->GetActorLocation()
			+ FVector(0.0f, 0.0f, 100.0f);
		Prediction.SourceSocket = TEXT("weapon_end");
		Prediction.DefenderTargetBone = TEXT("spine_03");
		Prediction.PredictionSimulationTimestamp = PublishedTime;
		Prediction.PredictedContactSimulationTime = ContactTime;
		Prediction.Lane = Enemy == FirstEnemy.Get()
			? EIncomingAttackLane::Left : EIncomingAttackLane::Right;
		Prediction.Height = EAttackHeight::Middle;
		Prediction.Confidence = Confidence;
		Prediction.bPathIntersectsThreatVolume = true;
		Combat->PublishAttackThreatPrediction(Prediction);
	}

	FName LatestThreatSwitchReason() const
	{
		if (!PlayerCombat.IsValid())
		{
			return NAME_None;
		}
		const TArray<FDefenseTelemetryRecord>& Records =
			PlayerCombat->GetDefenseTelemetry();
		for (int32 Index = Records.Num() - 1; Index >= 0; --Index)
		{
			if (Records[Index].Event == EDefenseTelemetryEvent::ThreatSelection)
			{
				return Records[Index].ThreatSwitchReason;
			}
		}
		return NAME_None;
	}

	void RecordScenario(
		const FString& Name,
		const bool bPassed,
		const FDefenseThreatSelectionResult& Result,
		const double FirstDeadline,
		const double SecondDeadline,
		const int32 EnumerationCount,
		const FName ExpectedReason)
	{
		const FName ActualReason = LatestThreatSwitchReason();
		const bool bReason = ActualReason == ExpectedReason;
		const bool bScenarioPassed = bPassed && bReason;
		Test->TestTrue(*FString::Printf(TEXT("%s arbitration result"), *Name), bPassed);
		Test->TestEqual(*FString::Printf(TEXT("%s switch reason"), *Name),
			ActualReason, ExpectedReason);
		bAllScenariosPassed = bAllScenariosPassed && bScenarioPassed;
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Name);
		Entry->SetBoolField(TEXT("passed"), bScenarioPassed);
		Entry->SetNumberField(TEXT("selected_stable_id"),
			Result.bFound ? Result.SelectedThreat.StableId.Value : 0);
		Entry->SetNumberField(TEXT("first_deadline"), FirstDeadline);
		Entry->SetNumberField(TEXT("second_deadline"), SecondDeadline);
		Entry->SetNumberField(TEXT("candidate_enumerations"), EnumerationCount);
		Entry->SetStringField(TEXT("switch_reason"), ActualReason.ToString());
		Entry->SetStringField(TEXT("expected_switch_reason"), ExpectedReason.ToString());
		Entry->SetStringField(TEXT("prediction_confidence"),
			Result.bFound
				? StaticEnum<EDefensePredictionConfidence>()->GetNameStringByValue(
					static_cast<int64>(Result.SelectedThreat.PredictedContact.Confidence))
				: TEXT("None"));
		ScenarioEvidence.Add(MakeShared<FJsonValueObject>(Entry));
	}

	bool ConfigureCamera()
	{
		APlayerController* Controller = Cast<APlayerController>(Player->GetController());
		if (!Controller || !World.IsValid())
		{
			return false;
		}
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		ACameraActor* Camera = World->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(), FTransform::Identity, SpawnParameters);
		if (!Camera)
		{
			return false;
		}
		OriginalViewTarget = Controller->GetViewTarget();
		ProofCamera = Camera;
		UCameraComponent* CameraComponent = Camera->GetCameraComponent();
		CameraComponent->SetFieldOfView(65.0f);
		CameraComponent->bConstrainAspectRatio = false;
		CameraComponent->PostProcessBlendWeight = 1.0f;
		CameraComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
		CameraComponent->PostProcessSettings.MotionBlurAmount = 0.0f;
		CameraComponent->PostProcessSettings.bOverride_MotionBlurMax = true;
		CameraComponent->PostProcessSettings.MotionBlurMax = 0.0f;
		const FVector Center = (Player->GetActorLocation()
			+ FirstEnemy->GetActorLocation() + SecondEnemy->GetActorLocation()) / 3.0f;
		const FVector Focus = Center + FVector(0.0f, 0.0f, 90.0f);
		const FVector Location = Center + FVector(-550.0f, 0.0f, 300.0f);
		Camera->SetActorLocationAndRotation(Location, (Focus - Location).Rotation());
		Controller->SetViewTarget(Camera);
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
		}
		return true;
	}

	bool CaptureThreatFrame()
	{
		APlayerController* Controller = Cast<APlayerController>(Player->GetController());
		int32 Width = 0;
		int32 Height = 0;
		if (Controller)
		{
			Controller->GetViewportSize(Width, Height);
		}
		auto Project = [Controller, Width, Height](const AActor* Actor, FVector2D& Out)
		{
			return Controller && Actor && Width > 0 && Height > 0
				&& Controller->ProjectWorldLocationToScreen(
					Actor->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f), Out, true)
				&& Out.X >= 0.0f && Out.X <= Width
				&& Out.Y >= 0.0f && Out.Y <= Height;
		};
		FVector2D PlayerScreen;
		FVector2D FirstScreen;
		FVector2D SecondScreen;
		const bool bProjected = Project(Player.Get(), PlayerScreen)
			&& Project(FirstEnemy.Get(), FirstScreen)
			&& Project(SecondEnemy.Get(), SecondScreen);
		bFrameFramed = bProjected
			&& FVector2D::Distance(FirstScreen, SecondScreen) >= 64.0f
			&& FVector2D::Distance(PlayerScreen, FirstScreen) >= 32.0f
			&& FVector2D::Distance(PlayerScreen, SecondScreen) >= 32.0f;
		if (!bFrameFramed)
		{
			return false;
		}
		FScreenshotRequest::RequestScreenshot(ScreenshotPath, false, false);
		return true;
	}

	void RestoreFixture()
	{
		Cleanup();
		bFixtureRestored = Director.IsValid() && Director->WasLastResetComplete();
		Test->TestTrue(TEXT("Two-threat fixture restores all runtime ownership"),
			bFixtureRestored);
	}

	void Cleanup()
	{
		if (PlayerCombat.IsValid())
		{
			PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Release);
		}
		if (bStableIdsCaptured && FirstCombat.IsValid())
		{
			FirstCombat->SetCombatantStableIdForTesting(OriginalFirstId);
		}
		if (bStableIdsCaptured && SecondCombat.IsValid())
		{
			SecondCombat->SetCombatantStableIdForTesting(OriginalSecondId);
		}
		bStableIdsCaptured = false;
		if (APlayerController* Controller = Player.IsValid()
			? Cast<APlayerController>(Player->GetController()) : nullptr)
		{
			if (OriginalViewTarget.IsValid())
			{
				Controller->SetViewTarget(OriginalViewTarget.Get());
			}
		}
		if (ProofCamera.IsValid())
		{
			ProofCamera->Destroy();
			ProofCamera.Reset();
		}
		if (bPauseNeedsRestore && World.IsValid())
		{
			UGameplayStatics::SetGamePaused(World.Get(), bPreviousGamePaused);
			bPauseNeedsRestore = false;
		}
		if (Director.IsValid())
		{
			Director->ResetFixture();
		}
		if (bDefenseDebugCaptured)
		{
			if (IConsoleVariable* DefenseDebug = IConsoleManager::Get().FindConsoleVariable(
				TEXT("Combat.Defense.Debug")))
			{
				DefenseDebug->SetWithCurrentPriority(PreviousDefenseDebug);
			}
			bDefenseDebugCaptured = false;
		}
	}

	void SetStage(const EGateBThreatStage NewStage)
	{
		Stage = NewStage;
		StageStart = FPlatformTime::Seconds();
	}

	double Elapsed(const double Start) const
	{
		return FPlatformTime::Seconds() - Start;
	}

	double StageElapsed() const
	{
		return Elapsed(StageStart);
	}

	void Fail(const FString& Message)
	{
		if (!bFatalFailure)
		{
			bFatalFailure = true;
			bAllScenariosPassed = false;
			Test->AddError(FString::Printf(TEXT("Gate B threat %s: %s"),
				ThreatStageName(Stage), *Message));
		}
		if (Stage != EGateBThreatStage::Finalize
			&& Stage != EGateBThreatStage::Done)
		{
			SetStage(EGateBThreatStage::Finalize);
		}
	}

	FAutomationTestBase* Test = nullptr;
	double CommandStart = 0.0;
	double StageStart = 0.0;
	EGateBThreatStage Stage = EGateBThreatStage::WaitForPIE;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<ADefenseMatrixProofDirector> Director;
	TWeakObjectPtr<AEnemyCharacter> FirstEnemy;
	TWeakObjectPtr<AEnemyCharacter> SecondEnemy;
	TWeakObjectPtr<UCombatComponent> PlayerCombat;
	TWeakObjectPtr<UCombatComponent> FirstCombat;
	TWeakObjectPtr<UCombatComponent> SecondCombat;
	TWeakObjectPtr<UTargetingComponent> Targeting;
	TWeakObjectPtr<UEnemyCombatAIComponent> FirstAI;
	TWeakObjectPtr<UEnemyCombatAIComponent> SecondAI;
	TWeakObjectPtr<UCombatTokenSubsystem> Tokens;
	TWeakObjectPtr<AActor> OriginalViewTarget;
	TWeakObjectPtr<ACameraActor> ProofCamera;
	FCombatantStableId OriginalFirstId;
	FCombatantStableId OriginalSecondId;
	float PlayerStartHealth = 0.0f;
	float LockMinimum = 0.0f;
	float SwitchLead = 0.0f;
	float MaximumPredictionAge = 0.0f;
	int32 FirstGeneration = 0;
	int32 SecondGeneration = 0;
	bool bTwoPhysicalAttacksObserved = false;
	bool bAllScenariosPassed = true;
	bool bFrameFramed = false;
	bool bFixtureRestored = false;
	bool bFatalFailure = false;
	bool bDefenseDebugCaptured = false;
	bool bWorldPausedForArbitration = false;
	bool bPauseNeedsRestore = false;
	bool bStableIdsCaptured = false;
	bool bPreviousGamePaused = false;
	bool bDamageSuppressionObserved = false;
	bool bPlayerHealthPreserved = false;
	int32 PreviousDefenseDebug = 0;
	FString EvidenceDirectory;
	FString FramesDirectory;
	FString ScreenshotPath;
	TArray<TSharedPtr<FJsonValue>> ScenarioEvidence;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseGateBThreatPIEProofTest,
	"KatanaCombat.Defense.GateB.ThreatPIEProof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseGateBThreatPIEProofTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(GateBThreatMapPackage));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FDefenseGateBThreatPIEProofCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
