// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/ActorComponent.h"
#include "Core/CombatComponent.h"
#include "Core/HitReactionComponent.h"
#include "Core/PairedAnimationComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
#include "Debug/DefenseMatrixProofDirector.h"
#include "Debug/DefenseTelemetry.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
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
constexpr TCHAR GateBSemanticMapPackage[] =
	TEXT("/Game/ProjectFiles/Levels/Test/Lvl_DefenseMatrix");
constexpr TCHAR SemanticPlayerFixtureTag[] = TEXT("DefenseMatrix.Player");
const FName SemanticCenterAnchorTag = TEXT("DefenseMatrix.Anchor.Center");
const FName UnblockableCase = TEXT("UnblockableMiddleCenter");
const FName PerfectParryCase = TEXT("PerfectParryGateARegression");
constexpr TCHAR UnblockableAttackPath[] =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_11.LightAttack_11");
constexpr TCHAR PerfectParryAttackPath[] =
	TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_1.LightAttack_1");

struct FSemanticControllerTickState
{
	TWeakObjectPtr<AController> Controller;
	bool bActorTickEnabled = false;
	TMap<TWeakObjectPtr<UActorComponent>, bool> ComponentTicks;
};

enum class EGateBSemanticStage : uint8
{
	WaitForPIE,
	SettleFixture,
	StartUnblockable,
	AwaitUnblockableResolution,
	AwaitUnblockableCleanup,
	SettleAfterUnblockable,
	StartPerfectParry,
	AwaitParryWindow,
	AwaitCounterWindow,
	Finalize,
	Done
};

const TCHAR* SemanticStageName(const EGateBSemanticStage Stage)
{
	switch (Stage)
	{
	case EGateBSemanticStage::WaitForPIE: return TEXT("WaitForPIE");
	case EGateBSemanticStage::SettleFixture: return TEXT("SettleFixture");
	case EGateBSemanticStage::StartUnblockable: return TEXT("StartUnblockable");
	case EGateBSemanticStage::AwaitUnblockableResolution: return TEXT("AwaitUnblockableResolution");
	case EGateBSemanticStage::AwaitUnblockableCleanup: return TEXT("AwaitUnblockableCleanup");
	case EGateBSemanticStage::SettleAfterUnblockable: return TEXT("SettleAfterUnblockable");
	case EGateBSemanticStage::StartPerfectParry: return TEXT("StartPerfectParry");
	case EGateBSemanticStage::AwaitParryWindow: return TEXT("AwaitParryWindow");
	case EGateBSemanticStage::AwaitCounterWindow: return TEXT("AwaitCounterWindow");
	case EGateBSemanticStage::Finalize: return TEXT("Finalize");
	case EGateBSemanticStage::Done: return TEXT("Done");
	default: return TEXT("Unknown");
	}
}

bool IsNontrivialSemanticImage(const FString& Filename)
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

class FDefenseGateBSemanticPIEProofCommand final : public IAutomationLatentCommand
{
public:
	explicit FDefenseGateBSemanticPIEProofCommand(FAutomationTestBase* InTest)
		: Test(InTest)
		, CommandStart(FPlatformTime::Seconds())
		, StageStart(CommandStart)
	{
		EvidenceDirectory = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("DefenseProof/GateB/Semantic"),
			FApp::CanEverRender() ? TEXT("Rendered") : TEXT("Headless"));
		FramesDirectory = FPaths::Combine(EvidenceDirectory, TEXT("frames"));
	}

	virtual ~FDefenseGateBSemanticPIEProofCommand() override
	{
		Cleanup();
	}

	virtual bool Update() override
	{
		if (!Test)
		{
			return true;
		}
		if (Stage != EGateBSemanticStage::WaitForPIE
			&& Stage != EGateBSemanticStage::Done
			&& !World.IsValid())
		{
			Fail(TEXT("PIE world became invalid"));
		}

		switch (Stage)
		{
		case EGateBSemanticStage::WaitForPIE: return UpdateWaitForPIE();
		case EGateBSemanticStage::SettleFixture: return UpdateSettleFixture();
		case EGateBSemanticStage::StartUnblockable: return UpdateStartUnblockable();
		case EGateBSemanticStage::AwaitUnblockableResolution:
			return UpdateAwaitUnblockableResolution();
		case EGateBSemanticStage::AwaitUnblockableCleanup:
			return UpdateAwaitUnblockableCleanup();
		case EGateBSemanticStage::SettleAfterUnblockable:
			return UpdateSettleAfterUnblockable();
		case EGateBSemanticStage::StartPerfectParry: return UpdateStartPerfectParry();
		case EGateBSemanticStage::AwaitParryWindow: return UpdateAwaitParryWindow();
		case EGateBSemanticStage::AwaitCounterWindow: return UpdateAwaitCounterWindow();
		case EGateBSemanticStage::Finalize: return UpdateFinalize();
		case EGateBSemanticStage::Done: return true;
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
		TArray<AEnemyCharacter*> CenterEnemies;
		for (TActorIterator<APlayerCharacter> It(PIEWorld); It; ++It)
		{
			if (It->ActorHasTag(SemanticPlayerFixtureTag))
			{
				Players.Add(*It);
			}
		}
		for (TActorIterator<ADefenseMatrixProofDirector> It(PIEWorld); It; ++It)
		{
			Directors.Add(*It);
		}
		for (TActorIterator<AEnemyCharacter> It(PIEWorld); It; ++It)
		{
			if (It->ActorHasTag(SemanticCenterAnchorTag))
			{
				CenterEnemies.Add(*It);
			}
		}
		if (Players.Num() != 1 || Directors.Num() != 1 || CenterEnemies.Num() != 1)
		{
			Fail(FString::Printf(
				TEXT("expected one player, director, and center attacker (players=%d directors=%d center=%d)"),
				Players.Num(), Directors.Num(), CenterEnemies.Num()));
			return false;
		}

		World = PIEWorld;
		Player = Players[0];
		Director = Directors[0];
		Enemy = CenterEnemies[0];
		PlayerCombat = Player->GetCombatComponent();
		EnemyCombat = Enemy->GetCombatComponent();
		EnemyAI = Enemy->GetCombatAIComponent();
		Tokens = PIEWorld->GetGameInstance()
			? PIEWorld->GetGameInstance()->GetSubsystem<UCombatTokenSubsystem>()
			: nullptr;
		if (!PlayerCombat.IsValid() || !EnemyCombat.IsValid() || !EnemyAI.IsValid()
			|| !Tokens.IsValid() || !Player->PairedAnimationComponent
			|| !Player->HitReactionComponent || !Enemy->WeaponComponent)
		{
			Fail(TEXT("semantic fixture lacks combat, token, paired, hit, or weapon authority"));
			return false;
		}

		IFileManager::Get().DeleteDirectory(*EvidenceDirectory, false, true);
		IFileManager::Get().MakeDirectory(*FramesDirectory, true);
		if (IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(
			TEXT("Combat.Defense.Debug")))
		{
			PreviousDefenseDebug = Debug->GetInt();
			Debug->SetWithCurrentPriority(1);
			bDefenseDebugCaptured = true;
		}
		else
		{
			Fail(TEXT("Combat.Defense.Debug is not registered"));
			return false;
		}

		Director->ResetFixture();
		if (!Director->WasLastResetComplete())
		{
			Fail(TEXT("initial fixture reset did not complete"));
			return false;
		}
		SuspendControllerLogic();
		PlayerCombat->OnDefenseResolvedNative.AddRaw(
			this, &FDefenseGateBSemanticPIEProofCommand::HandleDefenseResolved);
		bResolutionBound = true;
		SetStage(EGateBSemanticStage::SettleFixture);
		return false;
	}

	bool UpdateSettleFixture()
	{
		if (StageElapsed() < 0.25)
		{
			return false;
		}
		SetStage(EGateBSemanticStage::StartUnblockable);
		return false;
	}

	bool UpdateStartUnblockable()
	{
		if (FApp::CanEverRender() && FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		ResetCaseCapture();
		ClearCaseTelemetry();
		if (!Director->StartNamedCase(UnblockableCase))
		{
			Fail(TEXT("director rejected UnblockableMiddleCenter"));
			return false;
		}
		if (!PrepareStartedAttack(UnblockableAttackPath))
		{
			return false;
		}
		Test->TestTrue(TEXT("Unblockable proof begins with held guard"),
			PlayerCombat->IsBlocking());
		SetStage(EGateBSemanticStage::AwaitUnblockableResolution);
		return false;
	}

	bool UpdateAwaitUnblockableResolution()
	{
		if (!bHasContactResolution)
		{
			if (StageElapsed() > 8.0)
			{
				Fail(TEXT("unblockable attack produced no physical contact resolution"));
			}
			return false;
		}
		if (Elapsed(ResolutionObservedAt) < 0.05)
		{
			return false;
		}
		if (!bUnblockableValidated)
		{
			bUnblockableSemanticPassed = ValidateUnblockableResolution();
			bUnblockableValidated = true;
		}
		if (FApp::CanEverRender() && FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		if (!bUnblockableFrameRequested
			&& !CaptureFrame(TEXT("unblockable_contact"), bUnblockableFrameRequested))
		{
			Fail(TEXT("unblockable contact frame was not validly framed"));
			return false;
		}
		SetStage(EGateBSemanticStage::AwaitUnblockableCleanup);
		return false;
	}

	bool UpdateAwaitUnblockableCleanup()
	{
		const bool bOwnershipClean = EnemyAI.IsValid()
			&& !EnemyAI->HasAttackToken()
			&& !EnemyAI->IsWaitingForToken()
			&& !EnemyAI->IsAttacking()
			&& Tokens->GetActiveAttackerCount() == 0
			&& Tokens->GetQueueLength() == 0;
		if (!bOwnershipClean)
		{
			if (StageElapsed() > 10.0)
			{
				Fail(TEXT("unblockable attack did not naturally release attack/token ownership"));
			}
			return false;
		}

		UnblockableTokenReleaseDelta = EnemyAI->GetTokenReleaseCountForTesting()
			- CaseStartTokenReleaseCount;
		UnblockableAttackEndDelta = EnemyAI->GetAttackEndBroadcastCountForTesting()
			- CaseStartAttackEndCount;
		const int32 ResolutionRows = CountTelemetry(
			EDefenseTelemetryEvent::Resolution, ContactResolution.InteractionId);
		const int32 PresentationRows = CountTelemetry(
			EDefenseTelemetryEvent::PresentationStart, ContactResolution.InteractionId);
		const int32 CommittedHitPresentationRows = CountTelemetry(
			EDefenseTelemetryEvent::PresentationStart,
			ContactResolution.InteractionId,
			TEXT("CommittedHitReaction"));
		Test->TestEqual(TEXT("Unblockable contact has one resolution telemetry row"),
			ResolutionRows, 1);
		Test->TestEqual(TEXT("Unblockable contact has one presentation telemetry row"),
			PresentationRows, 1);
		Test->TestEqual(TEXT("Unblockable presentation is the committed hit reaction"),
			CommittedHitPresentationRows, 1);
		Test->TestEqual(TEXT("Unblockable cleanup releases its token once"),
			UnblockableTokenReleaseDelta, 1);
		Test->TestEqual(TEXT("Unblockable cleanup ends its attack once"),
			UnblockableAttackEndDelta, 1);
		bUnblockableSemanticPassed = bUnblockableSemanticPassed
			&& ResolutionRows == 1 && PresentationRows == 1
			&& CommittedHitPresentationRows == 1
			&& UnblockableTokenReleaseDelta == 1
			&& UnblockableAttackEndDelta == 1;
		CollectCaseTelemetry();

		Director->ResetFixture();
		bUnblockableFixtureRestored = Director->WasLastResetComplete()
			&& Player->PairedAnimationComponent->GetChainState() == EChainCounterState::None
			&& !PlayerCombat->IsBlocking();
		Test->TestTrue(TEXT("Unblockable case restores the complete fixture"),
			bUnblockableFixtureRestored);
		bUnblockableSemanticPassed = bUnblockableSemanticPassed
			&& bUnblockableFixtureRestored;
		BuildUnblockableEvidence(ResolutionRows, PresentationRows);
		SetStage(EGateBSemanticStage::SettleAfterUnblockable);
		return false;
	}

	bool UpdateSettleAfterUnblockable()
	{
		if (StageElapsed() < 0.25
			|| (FApp::CanEverRender() && FScreenshotRequest::IsScreenshotRequested()))
		{
			return false;
		}
		SetStage(EGateBSemanticStage::StartPerfectParry);
		return false;
	}

	bool UpdateStartPerfectParry()
	{
		ResetCaseCapture();
		ClearCaseTelemetry();
		if (!Director->StartNamedCase(PerfectParryCase))
		{
			Fail(TEXT("director rejected PerfectParryGateARegression"));
			return false;
		}
		if (!PrepareStartedAttack(PerfectParryAttackPath))
		{
			return false;
		}
		Test->TestFalse(TEXT("Perfect-parry proof begins without held guard"),
			PlayerCombat->IsBlocking());
		EnemyCombat->OnAttackConsumedInternal.AddRaw(
			this, &FDefenseGateBSemanticPIEProofCommand::HandleAttackConsumed);
		bConsumedBound = true;
		SetStage(EGateBSemanticStage::AwaitParryWindow);
		return false;
	}

	bool UpdateAwaitParryWindow()
	{
		const FAttackExecutionSnapshot Snapshot = EnemyCombat->BuildAttackExecutionSnapshot();
		const double SimulationNow = World->GetTimeSeconds();
		const UDefenseConfiguration* Configuration =
			PlayerCombat->GetEffectiveDefenseConfiguration();
		const double MaximumAge = Configuration
			? Configuration->MaximumHighConfidencePredictionAge : 0.10;
		const double PredictionAge = Snapshot.PredictedContact.bIsValid
			? SimulationNow - Snapshot.PredictedContact.PredictionSimulationTimestamp
			: TNumericLimits<double>::Max();
		const bool bFreshPrediction = Snapshot.PredictedContact.bIsValid
			&& Snapshot.PredictedContact.Confidence == EDefensePredictionConfidence::High
			&& Snapshot.PredictedContact.IntendedTarget.Get() == Player.Get()
			&& Snapshot.PredictedContact.bPathIntersectsThreatVolume
			&& FMath::IsFinite(PredictionAge)
			&& PredictionAge >= 0.0 && PredictionAge <= MaximumAge;
		const bool bWindowReady = Snapshot.bAttackActive
			&& Snapshot.AttackInstance.IsValid()
			&& Snapshot.AttackInstance.AttackGeneration == ExpectedAttackGeneration
			&& Snapshot.ActiveParryWindow.IsValid()
			&& Snapshot.ActiveParryWindow.Kind == EAttackWindowKind::Parry
			&& Snapshot.ActiveParryWindow.AttackInstance == Snapshot.AttackInstance
			&& Snapshot.ActiveParryWindow.SimulationEndTime >= SimulationNow
			&& bFreshPrediction
			&& EnemyAI->HasAttackToken();
		if (!bWindowReady)
		{
			if (StageElapsed() > 5.0)
			{
				Fail(FString::Printf(
					TEXT("physical parry window was not observed with fresh high-confidence prediction (active=%d window=%d confidence=%d age=%.4f generation=%d)"),
					Snapshot.bAttackActive,
					Snapshot.ActiveParryWindow.IsValid(),
					static_cast<int32>(Snapshot.PredictedContact.Confidence),
					PredictionAge,
					Snapshot.AttackInstance.AttackGeneration));
			}
			return false;
		}

		ParryPredictionAge = PredictionAge;
		ParryWindow = Snapshot.ActiveParryWindow;
		ExpectedAttackInstance = Snapshot.AttackInstance;
		PlayerCombat->OnInputEvent(EInputType::Block, EInputEventType::Press);
		ParryResolution = PlayerCombat->GetLastInputDefenseResolutionForTesting();
		bParryImmediatePassed = ValidatePerfectParryCommit();
		if (!CaptureFrame(TEXT("perfect_parry_active"), bParryFrameRequested))
		{
			Fail(TEXT("perfect-parry frame was not validly framed"));
			return false;
		}
		SetStage(EGateBSemanticStage::AwaitCounterWindow);
		return false;
	}

	bool UpdateAwaitCounterWindow()
	{
		if (!FMath::IsNearlyEqual(Player->CurrentHealth, CaseStartHealth))
		{
			Fail(TEXT("player took damage after the exact attack was parried"));
			return false;
		}
		if (Player->PairedAnimationComponent->GetChainState()
			!= EChainCounterState::CounterWindow)
		{
			if (StageElapsed() > 6.0)
			{
				Fail(FString::Printf(
					TEXT("perfect-parry bridge did not reach CounterWindow (state=%s)"),
					*StaticEnum<EChainCounterState>()->GetNameStringByValue(
						static_cast<int64>(Player->PairedAnimationComponent->GetChainState()))));
			}
			return false;
		}
		if (FApp::CanEverRender() && FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		if (!CaptureFrame(TEXT("counter_window"), bCounterFrameRequested))
		{
			Fail(TEXT("counter-window frame was not validly framed"));
			return false;
		}

		const FDefenseSequenceContext& Sequence =
			Player->PairedAnimationComponent->GetActiveDefenseSequenceContext();
		const bool bContinuity = Sequence.OriginatingInteraction
			== ParryResolution.InteractionId
			&& Sequence.OriginatingResolution.Decision.AttackInstance
				== ExpectedAttackInstance
			&& Sequence.SourceAttacker.Get() == Enemy.Get()
			&& Sequence.Defender.Get() == Player.Get()
			&& Sequence.ChainState == EChainCounterState::CounterWindow
			&& Player->PairedAnimationComponent->HasActiveChainTarget();
		const int32 ResolutionRows = CountTelemetry(
			EDefenseTelemetryEvent::Resolution, ParryResolution.InteractionId);
		const int32 PresentationRows = CountTelemetry(
			EDefenseTelemetryEvent::PresentationStart, ParryResolution.InteractionId);
		const int32 ParryStarts = CountStageTelemetry(
			EDefenseTelemetryEvent::StageStart,
			ParryResolution.InteractionId,
			TEXT("ParryActive"));
		const int32 CounterTransitions = CountStageTelemetry(
			EDefenseTelemetryEvent::StageTransition,
			ParryResolution.InteractionId,
			TEXT("CounterWindow"));
		const int32 TokenReleaseDelta = EnemyAI->GetTokenReleaseCountForTesting()
			- CaseStartTokenReleaseCount;
		const int32 AttackEndDelta = EnemyAI->GetAttackEndBroadcastCountForTesting()
			- CaseStartAttackEndCount;
		Test->TestTrue(TEXT("Perfect parry retains exact context into CounterWindow"), bContinuity);
		Test->TestEqual(TEXT("Perfect parry has one resolution telemetry row"),
			ResolutionRows, 1);
		Test->TestEqual(TEXT("Perfect parry has one presentation telemetry row"),
			PresentationRows, 1);
		Test->TestEqual(TEXT("Perfect parry starts ParryActive once"), ParryStarts, 1);
		Test->TestEqual(TEXT("Perfect parry transitions to CounterWindow once"),
			CounterTransitions, 1);
		Test->TestEqual(TEXT("Perfect parry releases source token once"), TokenReleaseDelta, 1);
		Test->TestEqual(TEXT("Perfect parry ends source attack once"), AttackEndDelta, 1);
		bPerfectParrySemanticPassed = bParryImmediatePassed && bContinuity
			&& ResolutionRows == 1 && PresentationRows == 1
			&& ParryStarts == 1 && CounterTransitions == 1
			&& TokenReleaseDelta == 1 && AttackEndDelta == 1;
		CollectCaseTelemetry();
		BuildParryEvidence(
			ResolutionRows, PresentationRows, ParryStarts, CounterTransitions,
			TokenReleaseDelta, AttackEndDelta, bContinuity);

		if (bConsumedBound && EnemyCombat.IsValid())
		{
			EnemyCombat->OnAttackConsumedInternal.RemoveAll(this);
			bConsumedBound = false;
		}
		Director->ResetFixture();
		bParryFixtureRestored = Director->WasLastResetComplete()
			&& Player->PairedAnimationComponent->GetChainState() == EChainCounterState::None
			&& !Player->PairedAnimationComponent->HasActiveChainTarget()
			&& !PlayerCombat->IsBlocking();
		Test->TestTrue(TEXT("Perfect-parry case restores all Chain and fixture ownership"),
			bParryFixtureRestored);
		bPerfectParrySemanticPassed = bPerfectParrySemanticPassed
			&& bParryFixtureRestored;
		if (ParryEvidence.IsValid())
		{
			ParryEvidence->SetBoolField(TEXT("fixture_restored"), bParryFixtureRestored);
			ParryEvidence->SetBoolField(TEXT("passed"), bPerfectParrySemanticPassed);
		}
		SetStage(EGateBSemanticStage::Finalize);
		return false;
	}

	bool UpdateFinalize()
	{
		if (FApp::CanEverRender() && FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		bool bRenderedFramesComplete = true;
		if (FApp::CanEverRender())
		{
			bRenderedFramesComplete = FrameFiles.Num() == 3 && bAllFramesFramed;
			for (const FString& Frame : FrameFiles)
			{
				bRenderedFramesComplete = bRenderedFramesComplete
					&& FPaths::FileExists(Frame) && IsNontrivialSemanticImage(Frame);
			}
			Test->TestTrue(TEXT("Semantic proof frames decode and visibly frame both actors"),
				bRenderedFramesComplete);
		}

		FString CsvPath;
		FString CsvError;
		const bool bCsvWritten = DefenseTelemetry::WriteCsv(
			FPaths::Combine(EvidenceDirectory,
				TEXT("defense-gate-b-semantic-telemetry.csv")),
			CollectedTelemetry, CsvPath, CsvError);
		Test->TestTrue(TEXT("Semantic proof telemetry CSV is written"), bCsvWritten);
		if (!bCsvWritten)
		{
			Test->AddError(CsvError);
		}

		const bool bPassed = !bFatalFailure && bUnblockableSemanticPassed
			&& bPerfectParrySemanticPassed && bUnblockableFixtureRestored
			&& bParryFixtureRestored
			&& (!FApp::CanEverRender() || bRenderedFramesComplete)
			&& bCsvWritten;
		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 1);
		Root->SetStringField(TEXT("gate"), TEXT("B"));
		Root->SetStringField(TEXT("proof"), TEXT("DefenseSemantics"));
		Root->SetStringField(TEXT("map"), GateBSemanticMapPackage);
		Root->SetStringField(TEXT("execution_mode"),
			FApp::CanEverRender() ? TEXT("Rendered") : TEXT("Headless"));
		Root->SetBoolField(TEXT("visual_capture_applicable"), FApp::CanEverRender());
		Root->SetBoolField(TEXT("frames_complete"),
			FApp::CanEverRender() && bRenderedFramesComplete);
		Root->SetBoolField(TEXT("passed"), bPassed);
		TArray<TSharedPtr<FJsonValue>> Cases;
		if (UnblockableEvidence.IsValid())
		{
			Cases.Add(MakeShared<FJsonValueObject>(UnblockableEvidence));
		}
		if (ParryEvidence.IsValid())
		{
			Cases.Add(MakeShared<FJsonValueObject>(ParryEvidence));
		}
		Root->SetArrayField(TEXT("cases"), Cases);
		Root->SetArrayField(TEXT("frames"), FrameEvidence);
		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		const bool bJsonWritten = FFileHelper::SaveStringToFile(
			Json,
			*FPaths::Combine(EvidenceDirectory,
				TEXT("defense-gate-b-semantic-evidence.json")),
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		Test->TestTrue(TEXT("Semantic structured evidence JSON is written"), bJsonWritten);
		Cleanup();
		SetStage(EGateBSemanticStage::Done);
		return false;
	}

	bool PrepareStartedAttack(const TCHAR* ExpectedPath)
	{
		Enemy = Director->GetFixtureEnemy(SemanticCenterAnchorTag);
		EnemyCombat = Enemy.IsValid() ? Enemy->GetCombatComponent() : nullptr;
		EnemyAI = Enemy.IsValid() ? Enemy->GetCombatAIComponent() : nullptr;
		if (!Enemy.IsValid() || !EnemyCombat.IsValid() || !EnemyAI.IsValid())
		{
			Fail(TEXT("center semantic attacker did not resolve"));
			return false;
		}
		if (!EnemyAI->IsAttacking() && !EnemyAI->ExecuteAttack())
		{
			Fail(TEXT("semantic attacker could not execute its director-selected attack"));
			return false;
		}
		UAttackData* Attack = EnemyAI->SelectedAttack;
		ExpectedAttackGeneration = EnemyAI->GetActiveAttackGeneration();
		if (!Attack || Attack->GetPathName() != ExpectedPath
			|| ExpectedAttackGeneration <= 0 || !EnemyAI->HasAttackToken())
		{
			Fail(FString::Printf(
				TEXT("semantic case attack identity mismatch (expected=%s actual=%s generation=%d token=%d)"),
				ExpectedPath, *GetPathNameSafe(Attack), ExpectedAttackGeneration,
				EnemyAI->HasAttackToken()));
			return false;
		}
		CaseAttack = Attack;
		CaseStartHealth = Player->CurrentHealth;
		CaseStartDamageResistance = Player->HitReactionComponent->DamageResistance;
		CaseStartTokenReleaseCount = EnemyAI->GetTokenReleaseCountForTesting();
		CaseStartAttackEndCount = EnemyAI->GetAttackEndBroadcastCountForTesting();
		if (!ConfigureProofCamera())
		{
			Fail(TEXT("semantic proof camera could not be configured"));
			return false;
		}
		return true;
	}

	bool ValidateUnblockableResolution()
	{
		const FDefenseDecision& Decision = ContactResolution.Decision;
		const float WeaponMultiplier = Enemy->WeaponComponent->GetDamageMultiplier();
		UnblockableExpectedDamage = CaseAttack.IsValid()
			? CaseAttack->BaseDamage * WeaponMultiplier * CaseStartDamageResistance
			: 0.0f;
		UnblockableObservedDamage = FMath::Max(
			0.0f, CaseStartHealth - Player->CurrentHealth);
		const bool bIdentity = ContactResolution.Stage == EDefenseQueryStage::Contact
			&& ContactResolution.InteractionId.IsValid()
			&& Decision.AttackInstance.Attacker.Get() == Enemy.Get()
			&& Decision.AttackInstance.AttackGeneration == ExpectedAttackGeneration
			&& Decision.SelectedAttack.Get() == CaseAttack.Get();
		const bool bDecision = Decision.Outcome == EDefenseOutcome::UnblockableHit
			&& Decision.Reason == EDefenseReason::Unblockable
			&& Decision.DamageDisposition == EDefenseDamageDisposition::ApplyRequestedDamage
			&& Decision.AttackerResponse == EAttackerResponse::Continue
			&& Decision.Height == EAttackHeight::Middle
			&& Decision.Lane == EIncomingAttackLane::Center
			&& Decision.SwingShape == ESwingDirection::Vertical;
		const bool bContact = ContactResolution.bHasActualContact
			&& ContactResolution.ActualContact.bIsValid
			&& ContactResolution.ActualContact.SourceSocket == TEXT("weapon_end")
			&& ContactResolution.ActualContact.ResolvedTargetBone == TEXT("spine_03")
			&& ContactResolution.ActualContact.HitInfo.Attacker.Get() == Enemy.Get();
		const bool bDamage = UnblockableExpectedDamage > 0.0f
			&& UnblockableExpectedDamage < CaseStartHealth
			&& FMath::IsNearlyEqual(
				UnblockableObservedDamage, UnblockableExpectedDamage, 0.01f)
			&& Player->CurrentHealth > 0.0f && !Player->IsDeadOrDying();
		Test->TestTrue(TEXT("Unblockable physical contact preserves exact attack identity"), bIdentity);
		Test->TestTrue(TEXT("Unblockable physical contact matches outcome/reason/response"), bDecision);
		Test->TestTrue(TEXT("Unblockable physical contact records source socket and target bone"),
			bContact);
		Test->TestTrue(TEXT("Unblockable physical contact applies expected nonlethal damage"),
			bDamage);
		return bIdentity && bDecision && bContact && bDamage;
	}

	bool ValidatePerfectParryCommit()
	{
		const FDefenseDecision& Decision = ParryResolution.Decision;
		const bool bIdentity = ParryResolution.Stage == EDefenseQueryStage::InputIntent
			&& ParryResolution.InteractionId.IsValid()
			&& Decision.AttackInstance == ExpectedAttackInstance
			&& Decision.SelectedAttack.Get() == CaseAttack.Get();
		const bool bDecision = Decision.Outcome == EDefenseOutcome::PerfectParry
			&& Decision.Reason == EDefenseReason::None
			&& Decision.AttackerResponse == EAttackerResponse::ParryStagger
			&& Decision.bChainEligible;
		const bool bConsumed = ConsumedEventCount == 1
			&& ConsumedEvent.AttackInstance == ExpectedAttackInstance
			&& ConsumedEvent.Reason == EAttackConsumeReason::PerfectParry
			&& EnemyCombat->IsAttackConsumed(ExpectedAttackInstance);
		const bool bOwnership = !EnemyAI->HasAttackToken()
			&& !EnemyAI->IsWaitingForToken()
			&& Tokens->GetActiveAttackerCount() == 0
			&& PlayerCombat->IsBlocking();
		const bool bChainStarted = Player->PairedAnimationComponent->GetChainState()
			== EChainCounterState::ParryActive
			&& Player->PairedAnimationComponent->HasActiveChainTarget();
		const bool bNoDamage = FMath::IsNearlyEqual(Player->CurrentHealth, CaseStartHealth);
		Test->TestTrue(TEXT("Public Block Press commits the exact physical parry threat"), bIdentity);
		Test->TestTrue(TEXT("Physical parry matches outcome and Chain semantics"), bDecision);
		Test->TestTrue(TEXT("Physical parry consumes the exact generation once"), bConsumed);
		Test->TestTrue(TEXT("Physical parry releases source ownership before presentation"),
			bOwnership);
		Test->TestTrue(TEXT("Physical parry starts retained ParryActive state"), bChainStarted);
		Test->TestTrue(TEXT("Physical parry applies no player damage"), bNoDamage);
		return bIdentity && bDecision && bConsumed && bOwnership && bChainStarted && bNoDamage;
	}

	void HandleDefenseResolved(const FDefenseResolution& Resolution)
	{
		if (Stage != EGateBSemanticStage::AwaitUnblockableResolution
			|| Resolution.Stage != EDefenseQueryStage::Contact
			|| Resolution.Decision.AttackInstance.Attacker.Get() != Enemy.Get()
			|| Resolution.Decision.AttackInstance.AttackGeneration
				!= ExpectedAttackGeneration)
		{
			return;
		}
		ContactResolution = Resolution;
		bHasContactResolution = true;
		ResolutionObservedAt = FPlatformTime::Seconds();
	}

	void HandleAttackConsumed(const FAttackConsumedEvent& Event)
	{
		if (Event.AttackInstance == ExpectedAttackInstance)
		{
			++ConsumedEventCount;
			ConsumedEvent = Event;
		}
	}

	int32 CountTelemetry(
		const EDefenseTelemetryEvent Event,
		const FDefenseInteractionId& Interaction,
		const FName RequiredDisposition = NAME_None) const
	{
		int32 Count = 0;
		auto CountIn = [&](const UCombatComponent* Combat)
		{
			if (!Combat)
			{
				return;
			}
			for (const FDefenseTelemetryRecord& Record : Combat->GetDefenseTelemetry())
			{
				Count += Record.Event == Event
					&& Record.InteractionId == Interaction
					&& (RequiredDisposition.IsNone()
						|| Record.CacheDisposition == RequiredDisposition)
					? 1 : 0;
			}
		};
		CountIn(PlayerCombat.Get());
		CountIn(EnemyCombat.Get());
		return Count;
	}

	int32 CountStageTelemetry(
		const EDefenseTelemetryEvent Event,
		const FDefenseInteractionId& Interaction,
		const FName StageName) const
	{
		int32 Count = 0;
		for (const FDefenseTelemetryRecord& Record : PlayerCombat->GetDefenseTelemetry())
		{
			Count += Record.Event == Event
				&& Record.InteractionId == Interaction
				&& Record.StageName == StageName ? 1 : 0;
		}
		return Count;
	}

	void ClearCaseTelemetry()
	{
		PlayerCombat->ClearDefenseTelemetry();
		EnemyCombat->ClearDefenseTelemetry();
	}

	void CollectCaseTelemetry()
	{
		CollectedTelemetry.Append(PlayerCombat->GetDefenseTelemetry());
		CollectedTelemetry.Append(EnemyCombat->GetDefenseTelemetry());
	}

	void ResetCaseCapture()
	{
		bHasContactResolution = false;
		bUnblockableValidated = false;
		ContactResolution = {};
		ParryResolution = {};
		ExpectedAttackInstance = {};
		ParryWindow = {};
		ConsumedEvent = {};
		ConsumedEventCount = 0;
		CaseAttack.Reset();
		ExpectedAttackGeneration = 0;
	}

	bool ConfigureProofCamera()
	{
		if (!FApp::CanEverRender())
		{
			return true;
		}
		APlayerController* Controller = Cast<APlayerController>(Player->GetController());
		if (!Controller || !World.IsValid() || !Enemy.IsValid())
		{
			return false;
		}
		if (!ProofCamera.IsValid())
		{
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
		}
		FVector CombatAxis = Enemy->GetActorLocation() - Player->GetActorLocation();
		CombatAxis.Z = 0.0f;
		CombatAxis = CombatAxis.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const FVector SideAxis = FVector::CrossProduct(FVector::UpVector, CombatAxis)
			.GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
		const FVector Midpoint = (Player->GetActorLocation() + Enemy->GetActorLocation()) * 0.5f;
		const FVector Focus = Midpoint + FVector(0.0f, 0.0f, 90.0f);
		const FVector Location = Midpoint + SideAxis * 550.0f + FVector(0.0f, 0.0f, 130.0f);
		ProofCamera->SetActorLocationAndRotation(Location, (Focus - Location).Rotation());
		Controller->SetViewTarget(ProofCamera.Get());
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
		}
		return true;
	}

	bool CaptureFrame(const FString& Label, bool& bRequested)
	{
		if (bRequested || !FApp::CanEverRender())
		{
			bRequested = true;
			return true;
		}
		if (!ConfigureProofCamera())
		{
			return false;
		}
		APlayerController* Controller = Cast<APlayerController>(Player->GetController());
		int32 Width = 0;
		int32 Height = 0;
		Controller->GetViewportSize(Width, Height);
		auto Project = [Controller, Width, Height](const AActor* Actor, FVector2D& Out)
		{
			return Controller && Actor && Width > 0 && Height > 0
				&& Controller->ProjectWorldLocationToScreen(
					Actor->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f), Out, true)
				&& Out.X >= 0.0f && Out.X <= Width
				&& Out.Y >= 0.0f && Out.Y <= Height;
		};
		FVector2D PlayerScreen;
		FVector2D EnemyScreen;
		const bool bFramed = Project(Player.Get(), PlayerScreen)
			&& Project(Enemy.Get(), EnemyScreen)
			&& FVector2D::Distance(PlayerScreen, EnemyScreen) >= 32.0f;
		bAllFramesFramed = bAllFramesFramed && bFramed;
		if (!bFramed)
		{
			return false;
		}
		const FString Filename = Label + TEXT(".png");
		const FString Path = FPaths::Combine(FramesDirectory, Filename);
		FScreenshotRequest::RequestScreenshot(Path, false, false);
		bRequested = true;
		FrameFiles.Add(Path);
		TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
		Frame->SetStringField(TEXT("file"), Filename);
		Frame->SetStringField(TEXT("label"), Label);
		Frame->SetBoolField(TEXT("player_in_view"), true);
		Frame->SetBoolField(TEXT("attacker_in_view"), true);
		Frame->SetNumberField(TEXT("actor_screen_separation_px"),
			FVector2D::Distance(PlayerScreen, EnemyScreen));
		Frame->SetNumberField(TEXT("viewport_width"), Width);
		Frame->SetNumberField(TEXT("viewport_height"), Height);
		FrameEvidence.Add(MakeShared<FJsonValueObject>(Frame));
		return true;
	}

	void SuspendControllerLogic()
	{
		for (TActorIterator<AEnemyCharacter> It(World.Get()); It; ++It)
		{
			AController* Controller = It->GetController();
			if (!Controller)
			{
				continue;
			}
			FSemanticControllerTickState State;
			State.Controller = Controller;
			State.bActorTickEnabled = Controller->IsActorTickEnabled();
			TArray<UActorComponent*> Components;
			Controller->GetComponents(Components);
			for (UActorComponent* Component : Components)
			{
				if (Component)
				{
					State.ComponentTicks.Add(Component, Component->IsComponentTickEnabled());
					Component->SetComponentTickEnabled(false);
				}
			}
			Controller->SetActorTickEnabled(false);
			ControllerTickStates.Add(MoveTemp(State));
		}
	}

	void RestoreControllerLogic()
	{
		for (const FSemanticControllerTickState& State : ControllerTickStates)
		{
			if (AController* Controller = State.Controller.Get())
			{
				Controller->SetActorTickEnabled(State.bActorTickEnabled);
			}
			for (const auto& Pair : State.ComponentTicks)
			{
				if (UActorComponent* Component = Pair.Key.Get())
				{
					Component->SetComponentTickEnabled(Pair.Value);
				}
			}
		}
		ControllerTickStates.Reset();
	}

	void BuildUnblockableEvidence(const int32 ResolutionRows, const int32 PresentationRows)
	{
		UnblockableEvidence = MakeShared<FJsonObject>();
		UnblockableEvidence->SetStringField(TEXT("name"), UnblockableCase.ToString());
		UnblockableEvidence->SetBoolField(TEXT("passed"), bUnblockableSemanticPassed);
		UnblockableEvidence->SetStringField(TEXT("attack"), UnblockableAttackPath);
		UnblockableEvidence->SetNumberField(TEXT("attack_generation"),
			ExpectedAttackGeneration);
		UnblockableEvidence->SetStringField(TEXT("outcome"), TEXT("UnblockableHit"));
		UnblockableEvidence->SetStringField(TEXT("reason"), TEXT("Unblockable"));
		UnblockableEvidence->SetStringField(TEXT("attacker_response"), TEXT("Continue"));
		UnblockableEvidence->SetNumberField(TEXT("expected_damage"),
			UnblockableExpectedDamage);
		UnblockableEvidence->SetNumberField(TEXT("observed_damage"),
			UnblockableObservedDamage);
		UnblockableEvidence->SetBoolField(TEXT("player_alive"),
			Player->CurrentHealth > 0.0f && !Player->IsDeadOrDying());
		UnblockableEvidence->SetStringField(TEXT("source_socket"),
			ContactResolution.ActualContact.SourceSocket.ToString());
		UnblockableEvidence->SetStringField(TEXT("target_bone"),
			ContactResolution.ActualContact.ResolvedTargetBone.ToString());
		UnblockableEvidence->SetNumberField(TEXT("resolution_telemetry"), ResolutionRows);
		UnblockableEvidence->SetNumberField(TEXT("presentation_telemetry"), PresentationRows);
		UnblockableEvidence->SetNumberField(TEXT("token_releases"),
			UnblockableTokenReleaseDelta);
		UnblockableEvidence->SetNumberField(TEXT("attack_end_events"),
			UnblockableAttackEndDelta);
		UnblockableEvidence->SetBoolField(TEXT("fixture_restored"),
			bUnblockableFixtureRestored);
		UnblockableEvidence->SetBoolField(TEXT("passed"), bUnblockableSemanticPassed);
	}

	void BuildParryEvidence(
		const int32 ResolutionRows,
		const int32 PresentationRows,
		const int32 ParryStarts,
		const int32 CounterTransitions,
		const int32 TokenReleaseDelta,
		const int32 AttackEndDelta,
		const bool bContinuity)
	{
		ParryEvidence = MakeShared<FJsonObject>();
		ParryEvidence->SetStringField(TEXT("name"), PerfectParryCase.ToString());
		ParryEvidence->SetBoolField(TEXT("passed"), bPerfectParrySemanticPassed);
		ParryEvidence->SetStringField(TEXT("attack"), PerfectParryAttackPath);
		ParryEvidence->SetNumberField(TEXT("attack_generation"),
			ExpectedAttackInstance.AttackGeneration);
		ParryEvidence->SetStringField(TEXT("outcome"), TEXT("PerfectParry"));
		ParryEvidence->SetStringField(TEXT("attacker_response"), TEXT("ParryStagger"));
		ParryEvidence->SetNumberField(TEXT("prediction_age_seconds"), ParryPredictionAge);
		ParryEvidence->SetBoolField(TEXT("prediction_high_confidence"), true);
		ParryEvidence->SetBoolField(TEXT("physical_parry_window_observed"),
			ParryWindow.IsValid());
		ParryEvidence->SetNumberField(TEXT("attack_consumed_events"), ConsumedEventCount);
		ParryEvidence->SetNumberField(TEXT("token_releases"), TokenReleaseDelta);
		ParryEvidence->SetNumberField(TEXT("attack_end_events"), AttackEndDelta);
		ParryEvidence->SetNumberField(TEXT("resolution_telemetry"), ResolutionRows);
		ParryEvidence->SetNumberField(TEXT("presentation_telemetry"), PresentationRows);
		ParryEvidence->SetNumberField(TEXT("parry_stage_starts"), ParryStarts);
		ParryEvidence->SetNumberField(TEXT("counter_window_transitions"),
			CounterTransitions);
		ParryEvidence->SetBoolField(TEXT("counter_window_identity_continuity"),
			bContinuity);
		ParryEvidence->SetBoolField(TEXT("player_damage_suppressed"),
			FMath::IsNearlyEqual(Player->CurrentHealth, CaseStartHealth));
	}

	void Cleanup()
	{
		if (bCleanupComplete)
		{
			return;
		}
		bCleanupComplete = true;
		if (bConsumedBound && EnemyCombat.IsValid())
		{
			EnemyCombat->OnAttackConsumedInternal.RemoveAll(this);
		}
		bConsumedBound = false;
		if (bResolutionBound && PlayerCombat.IsValid())
		{
			PlayerCombat->OnDefenseResolvedNative.RemoveAll(this);
		}
		bResolutionBound = false;
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
		}
		ProofCamera.Reset();
		if (Director.IsValid())
		{
			Director->ResetFixture();
		}
		RestoreControllerLogic();
		if (bDefenseDebugCaptured)
		{
			if (IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(
				TEXT("Combat.Defense.Debug")))
			{
				Debug->SetWithCurrentPriority(PreviousDefenseDebug);
			}
		}
		bDefenseDebugCaptured = false;
	}

	void SetStage(const EGateBSemanticStage NewStage)
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
			Test->AddError(FString::Printf(TEXT("Gate B semantic %s: %s"),
				SemanticStageName(Stage), *Message));
		}
		if (Stage != EGateBSemanticStage::Finalize
			&& Stage != EGateBSemanticStage::Done)
		{
			SetStage(EGateBSemanticStage::Finalize);
		}
	}

	FAutomationTestBase* Test = nullptr;
	double CommandStart = 0.0;
	double StageStart = 0.0;
	double ResolutionObservedAt = 0.0;
	double ParryPredictionAge = 0.0;
	EGateBSemanticStage Stage = EGateBSemanticStage::WaitForPIE;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<ADefenseMatrixProofDirector> Director;
	TWeakObjectPtr<AEnemyCharacter> Enemy;
	TWeakObjectPtr<UCombatComponent> PlayerCombat;
	TWeakObjectPtr<UCombatComponent> EnemyCombat;
	TWeakObjectPtr<UEnemyCombatAIComponent> EnemyAI;
	TWeakObjectPtr<UCombatTokenSubsystem> Tokens;
	TWeakObjectPtr<UAttackData> CaseAttack;
	TWeakObjectPtr<AActor> OriginalViewTarget;
	TWeakObjectPtr<ACameraActor> ProofCamera;
	FDefenseResolution ContactResolution;
	FDefenseResolution ParryResolution;
	FAttackInstanceId ExpectedAttackInstance;
	FAttackWindowInstanceId ParryWindow;
	FAttackConsumedEvent ConsumedEvent;
	int32 ExpectedAttackGeneration = 0;
	int32 ConsumedEventCount = 0;
	int32 CaseStartTokenReleaseCount = 0;
	int32 CaseStartAttackEndCount = 0;
	int32 UnblockableTokenReleaseDelta = 0;
	int32 UnblockableAttackEndDelta = 0;
	int32 PreviousDefenseDebug = 0;
	float CaseStartHealth = 0.0f;
	float CaseStartDamageResistance = 1.0f;
	float UnblockableExpectedDamage = 0.0f;
	float UnblockableObservedDamage = 0.0f;
	bool bHasContactResolution = false;
	bool bUnblockableValidated = false;
	bool bUnblockableSemanticPassed = false;
	bool bPerfectParrySemanticPassed = false;
	bool bParryImmediatePassed = false;
	bool bUnblockableFixtureRestored = false;
	bool bParryFixtureRestored = false;
	bool bResolutionBound = false;
	bool bConsumedBound = false;
	bool bDefenseDebugCaptured = false;
	bool bFatalFailure = false;
	bool bCleanupComplete = false;
	bool bAllFramesFramed = true;
	bool bUnblockableFrameRequested = false;
	bool bParryFrameRequested = false;
	bool bCounterFrameRequested = false;
	FString EvidenceDirectory;
	FString FramesDirectory;
	TArray<FString> FrameFiles;
	TArray<TSharedPtr<FJsonValue>> FrameEvidence;
	TArray<FDefenseTelemetryRecord> CollectedTelemetry;
	TArray<FSemanticControllerTickState> ControllerTickStates;
	TSharedPtr<FJsonObject> UnblockableEvidence;
	TSharedPtr<FJsonObject> ParryEvidence;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseGateBSemanticPIEProofTest,
	"KatanaCombat.Defense.GateB.SemanticPIEProof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseGateBSemanticPIEProofTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(GateBSemanticMapPackage));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FDefenseGateBSemanticPIEProofCommand(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
