// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "AI/CombatTokenSubsystem.h"
#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimSequenceBase.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/ActorComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Data/DefenseConfiguration.h"
#include "Debug/DefenseMatrixProofDirector.h"
#include "Debug/DefenseTelemetry.h"
#include "Defense/DefenseResolver.h"
#include "DefenseAssetValidationService.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "Utilities/CinematicEffectsUtilityLibrary.h"

namespace
{
constexpr TCHAR GateBMapPackage[] =
	TEXT("/Game/ProjectFiles/Levels/Test/Lvl_DefenseMatrix");
constexpr TCHAR GateBManifestRelativePath[] =
	TEXT("Tools/Codex/manifests/defense-gate-b.json");
constexpr TCHAR PlayerFixtureTag[] = TEXT("DefenseMatrix.Player");
constexpr TCHAR LeftAnchorTag[] = TEXT("DefenseMatrix.Anchor.Left");
constexpr TCHAR CenterAnchorTag[] = TEXT("DefenseMatrix.Anchor.Center");
constexpr TCHAR RightAnchorTag[] = TEXT("DefenseMatrix.Anchor.Right");
constexpr float MinimumActorScreenSeparationPixels = 48.0f;
constexpr float MinimumFrameEdgeMarginPixels = 4.0f;
constexpr float ProofCameraFieldOfViewDegrees = 65.0f;
constexpr float ProofCameraDistanceCm = 550.0f;
constexpr float DefaultProofDefenderYawOffsetDegrees = 30.0f;
constexpr float MaximumNormalBlockDisplacementCm = 1.0f;
constexpr float MaximumFrameYawOverBudgetDegrees = 0.1f;
constexpr float MaximumUnexpectedFrameDisplacementCm = 10.0f;
constexpr float MinimumMontageObservedWeight = 0.01f;
constexpr float MinimumMontageProgressSeconds = 0.005f;

struct FGateBFrameValidation
{
	bool bDecoded = false;
	bool bNontrivial = false;
	int32 Width = 0;
	int32 Height = 0;
};

bool AnalyzeGateBFrame(const FString& Filename, FGateBFrameValidation& OutResult)
{
	OutResult = {};
	FImage Image;
	if (!FImageUtils::LoadImage(*Filename, Image))
	{
		return false;
	}
	OutResult.bDecoded = true;
	OutResult.Width = Image.SizeX;
	OutResult.Height = Image.SizeY;
	if (Image.SizeX < 32 || Image.SizeY < 32 || Image.NumSlices != 1)
	{
		return true;
	}

	Image.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
	const int64 PixelCount = static_cast<int64>(Image.SizeX) * Image.SizeY;
	if (Image.RawData.Num() != PixelCount * static_cast<int64>(sizeof(FColor)))
	{
		OutResult.bDecoded = false;
		return false;
	}

	const FColor* Pixels = reinterpret_cast<const FColor*>(Image.RawData.GetData());
	const FColor First = Pixels[0];
	const int64 Stride = FMath::Max<int64>(1, PixelCount / 8192);
	int32 MinChannel = 255;
	int32 MaxChannel = 0;
	int32 VariedSamples = 0;
	double Brightness = 0.0;
	int32 Samples = 0;
	for (int64 Index = 0; Index < PixelCount; Index += Stride)
	{
		const FColor& Pixel = Pixels[Index];
		MinChannel = FMath::Min(MinChannel,
			FMath::Min3(static_cast<int32>(Pixel.R), static_cast<int32>(Pixel.G),
				static_cast<int32>(Pixel.B)));
		MaxChannel = FMath::Max(MaxChannel,
			FMath::Max3(static_cast<int32>(Pixel.R), static_cast<int32>(Pixel.G),
				static_cast<int32>(Pixel.B)));
		Brightness += FMath::Max3(Pixel.R, Pixel.G, Pixel.B);
		VariedSamples += FMath::Abs(static_cast<int32>(Pixel.R) - First.R) > 8
			|| FMath::Abs(static_cast<int32>(Pixel.G) - First.G) > 8
			|| FMath::Abs(static_cast<int32>(Pixel.B) - First.B) > 8
			? 1 : 0;
		++Samples;
	}
	OutResult.bNontrivial = MaxChannel - MinChannel >= 8
		&& VariedSamples >= 8
		&& Samples > 0
		&& Brightness / Samples >= 2.0;
	return true;
}

FString EnumName(const UEnum* Enum, const int64 Value)
{
	return Enum ? Enum->GetNameStringByValue(Value) : TEXT("Unknown");
}

template <typename TEnum>
bool EnumMatches(const TEnum Actual, const FString& Expected)
{
	return EnumName(StaticEnum<TEnum>(), static_cast<int64>(Actual)) == Expected;
}

struct FGateBNormalCase
{
	FDefenseProofExpectedCaseEntry Expected;
	FDefenseProofPresentationEntry Presentation;
	FString AttackDataPath;
};

struct FGateBRunVariant
{
	int32 CaseIndex = INDEX_NONE;
	float MontageRate = 1.0f;
	float WorldDilation = 1.0f;

	FString Name(const TArray<FGateBNormalCase>& Cases) const
	{
		return FString::Printf(TEXT("%s_Rate%03d_Time%03d"),
			*Cases[CaseIndex].Expected.Name,
			FMath::RoundToInt(MontageRate * 100.0f),
			FMath::RoundToInt(WorldDilation * 100.0f));
	}
};

struct FGateBControllerTickState
{
	TWeakObjectPtr<AController> Controller;
	bool bActorTickEnabled = false;
	TMap<TWeakObjectPtr<UActorComponent>, bool> ComponentTicks;
};

enum class EGateBProofStage : uint8
{
	WaitForPIE,
	SettleFixture,
	StartCase,
	AwaitAttack,
	AwaitResolution,
	ObservePresentation,
	AwaitNaturalCleanup,
	Finalize,
	Done
};

const TCHAR* StageName(const EGateBProofStage Stage)
{
	switch (Stage)
	{
	case EGateBProofStage::WaitForPIE: return TEXT("WaitForPIE");
	case EGateBProofStage::SettleFixture: return TEXT("SettleFixture");
	case EGateBProofStage::StartCase: return TEXT("StartCase");
	case EGateBProofStage::AwaitAttack: return TEXT("AwaitAttack");
	case EGateBProofStage::AwaitResolution: return TEXT("AwaitResolution");
	case EGateBProofStage::ObservePresentation: return TEXT("ObservePresentation");
	case EGateBProofStage::AwaitNaturalCleanup: return TEXT("AwaitNaturalCleanup");
	case EGateBProofStage::Finalize: return TEXT("Finalize");
	case EGateBProofStage::Done: return TEXT("Done");
	default: return TEXT("Unknown");
	}
}

class FDefenseGateBPIEProofCommand final : public IAutomationLatentCommand
{
public:
	FDefenseGateBPIEProofCommand(
		FAutomationTestBase* InTest,
		TArray<FGateBNormalCase> InCases,
		TArray<FGateBRunVariant> InVariants)
		: Test(InTest)
		, Cases(MoveTemp(InCases))
		, Variants(MoveTemp(InVariants))
		, CommandStart(FPlatformTime::Seconds())
		, StageStart(CommandStart)
	{
		bRadiusOverrideRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBRadius="), ProofRadius);
		const bool bOffsetXRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBOffsetX="), ProofOffset.X);
		const bool bOffsetYRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBOffsetY="), ProofOffset.Y);
		const bool bAttackerYawRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBYawOffset="), ProofYawOffset);
		bDefenderYawOverrideRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBPlayerYawOffset="), ProofPlayerYawOffset);
		bTransformOverrideRequested = bRadiusOverrideRequested
			|| bOffsetXRequested
			|| bOffsetYRequested
			|| bAttackerYawRequested
			|| bDefenderYawOverrideRequested;
		FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBDiagnosticAttack="), DiagnosticAttackPath);
		bDiagnosticMode = !DiagnosticAttackPath.IsEmpty();
		if (!bDiagnosticMode && !bDefenderYawOverrideRequested)
		{
			ProofPlayerYawOffset = DefaultProofDefenderYawOffsetDegrees;
			bDefenderYawOverrideRequested = true;
			bTransformOverrideRequested = true;
		}
		bDiagnosticMontageRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBDiagnosticMontage="),
			DiagnosticMontagePath);
		bDiagnosticSequenceRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBDiagnosticSequence="),
			DiagnosticSequencePath);
		FString DiagnosticSectionName;
		bDiagnosticSectionRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBDiagnosticSection="),
			DiagnosticSectionName);
		DiagnosticSection = FName(*DiagnosticSectionName);
		bDiagnosticMontageOverrideRequested = bDiagnosticMontageRequested
			|| bDiagnosticSectionRequested
			|| bDiagnosticSequenceRequested;
		bDiagnosticWarpOffsetXRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBWarpOffsetX="), DiagnosticWarpOffset.X);
		bDiagnosticWarpOffsetYRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBWarpOffsetY="), DiagnosticWarpOffset.Y);
		bDiagnosticWarpOffsetZRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBWarpOffsetZ="), DiagnosticWarpOffset.Z);
		bDiagnosticWarpOffsetRequested = bDiagnosticWarpOffsetXRequested
			|| bDiagnosticWarpOffsetYRequested
			|| bDiagnosticWarpOffsetZRequested;
		bDiagnosticMaxWarpDistanceRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBMaxWarpDistance="),
			DiagnosticMaxWarpDistance);
		bDiagnosticActiveStartRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBActiveStartOffset="),
			DiagnosticActiveStartOffset);
		bDiagnosticActiveDurationRequested = FParse::Value(
			FCommandLine::Get(), TEXT("DefenseGateBActiveDuration="),
			DiagnosticActiveDuration);
		bDiagnosticActiveWindowRequested = bDiagnosticActiveStartRequested
			|| bDiagnosticActiveDurationRequested;
		bDiagnosticAttackOverrideRequested = bDiagnosticWarpOffsetRequested
			|| bDiagnosticMaxWarpDistanceRequested
			|| bDiagnosticActiveWindowRequested
			|| bDiagnosticMontageOverrideRequested;
		FParse::Value(FCommandLine::Get(), TEXT("DefenseGateBFixedHz="), ProofFixedHz);
		ProofRadius = FMath::Max(80.0f, ProofRadius);
		ProofFixedHz = FMath::Clamp(ProofFixedHz, 15.0f, 240.0f);
		EvidenceDirectory = FPaths::Combine(
			FPaths::ProjectSavedDir(),
			FApp::CanEverRender()
				? TEXT("DefenseProof/GateB/Rendered")
				: TEXT("DefenseProof/GateB/Headless"));
		FramesDirectory = FPaths::Combine(EvidenceDirectory, TEXT("frames"));
	}

	virtual ~FDefenseGateBPIEProofCommand() override
	{
		Cleanup();
	}

	virtual bool Update() override
	{
		if (!Test)
		{
			return true;
		}
		if (Stage != EGateBProofStage::WaitForPIE
			&& Stage != EGateBProofStage::Done
			&& !World.IsValid())
		{
			Fail(TEXT("PIE world became invalid during Gate B proof"));
		}

		switch (Stage)
		{
		case EGateBProofStage::WaitForPIE: return UpdateWaitForPIE();
		case EGateBProofStage::SettleFixture: return UpdateSettleFixture();
		case EGateBProofStage::StartCase: return UpdateStartCase();
		case EGateBProofStage::AwaitAttack: return UpdateAwaitAttack();
		case EGateBProofStage::AwaitResolution: return UpdateAwaitResolution();
		case EGateBProofStage::ObservePresentation: return UpdateObservePresentation();
		case EGateBProofStage::AwaitNaturalCleanup: return UpdateAwaitNaturalCleanup();
		case EGateBProofStage::Finalize: return UpdateFinalize();
		case EGateBProofStage::Done: return true;
		default:
			Fail(TEXT("Unknown Gate B proof stage"));
			return false;
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

		TArray<APlayerCharacter*> FoundPlayers;
		TArray<ADefenseMatrixProofDirector*> FoundDirectors;
		TMap<FName, TArray<AEnemyCharacter*>> EnemiesByAnchor;
		for (TActorIterator<APlayerCharacter> It(PIEWorld); It; ++It)
		{
			if (It->ActorHasTag(PlayerFixtureTag))
			{
				FoundPlayers.Add(*It);
			}
		}
		for (TActorIterator<ADefenseMatrixProofDirector> It(PIEWorld); It; ++It)
		{
			FoundDirectors.Add(*It);
		}
		for (TActorIterator<AEnemyCharacter> It(PIEWorld); It; ++It)
		{
			for (const FName Anchor : {
				FName(LeftAnchorTag), FName(CenterAnchorTag), FName(RightAnchorTag)})
			{
				if (It->ActorHasTag(Anchor))
				{
					EnemiesByAnchor.FindOrAdd(Anchor).Add(*It);
				}
			}
		}
		const bool bExactFixture = FoundPlayers.Num() == 1
			&& FoundDirectors.Num() == 1
			&& EnemiesByAnchor.FindRef(FName(LeftAnchorTag)).Num() == 1
			&& EnemiesByAnchor.FindRef(FName(CenterAnchorTag)).Num() == 1
			&& EnemiesByAnchor.FindRef(FName(RightAnchorTag)).Num() == 1;
		if (!bExactFixture)
		{
			if (Elapsed(CommandStart) > 20.0)
			{
				Fail(FString::Printf(
					TEXT("Gate B map requires exactly one tagged player/director and one enemy per anchor (players=%d directors=%d left=%d center=%d right=%d)"),
					FoundPlayers.Num(), FoundDirectors.Num(),
					EnemiesByAnchor.FindRef(FName(LeftAnchorTag)).Num(),
					EnemiesByAnchor.FindRef(FName(CenterAnchorTag)).Num(),
					EnemiesByAnchor.FindRef(FName(RightAnchorTag)).Num()));
			}
			return false;
		}

		World = PIEWorld;
		PreviousGlobalTimeDilation = UGameplayStatics::GetGlobalTimeDilation(PIEWorld);
		bGlobalTimeDilationCaptured = true;
		AcquireFixedTimeStep();
		Player = FoundPlayers[0];
		Director = FoundDirectors[0];
		PlayerCombat = FoundPlayers[0]->GetCombatComponent();
		TokenSubsystem = PIEWorld->GetGameInstance()
			? PIEWorld->GetGameInstance()->GetSubsystem<UCombatTokenSubsystem>()
			: nullptr;
		Enemies.Add(EnemiesByAnchor.FindRef(FName(LeftAnchorTag))[0]);
		Enemies.Add(EnemiesByAnchor.FindRef(FName(CenterAnchorTag))[0]);
		Enemies.Add(EnemiesByAnchor.FindRef(FName(RightAnchorTag))[0]);
		if (!PlayerCombat.IsValid() || !TokenSubsystem.IsValid())
		{
			Fail(TEXT("Gate B fixture is missing combat or token authority"));
			return false;
		}

		IFileManager::Get().DeleteDirectory(*EvidenceDirectory, false, true);
		IFileManager::Get().MakeDirectory(*FramesDirectory, true);
		if (IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(
			TEXT("Combat.Defense.Debug")))
		{
			PreviousDefenseDebug = Debug->GetInt();
			Debug->SetWithCurrentPriority(1);
			bDefenseDebugOverridden = true;
		}

		Director->ResetFixture();
		SuspendControllerLogic();
		PlayerCombat->ClearDefenseTelemetry();
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			if (Enemy.IsValid() && Enemy->GetCombatComponent())
			{
				Enemy->GetCombatComponent()->ClearDefenseTelemetry();
			}
		}
		PlayerCombat->OnDefenseResolvedNative.AddRaw(
			this, &FDefenseGateBPIEProofCommand::HandleDefenseResolved);
		bResolutionBound = true;
		UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.AddRaw(
			this, &FDefenseGateBPIEProofCommand::HandleImpactSound);
		bAudioBound = true;
		UCinematicEffectsUtilityLibrary::OnImpactVFXSpawnInvokedForTesting.AddRaw(
			this, &FDefenseGateBPIEProofCommand::HandleImpactVFX);
		bVFXBound = true;
		SetStage(EGateBProofStage::SettleFixture);
		return false;
	}

	bool UpdateSettleFixture()
	{
		if (StageElapsed() < 0.25)
		{
			return false;
		}
		SetStage(EGateBProofStage::StartCase);
		return false;
	}

	bool UpdateStartCase()
	{
		if (VariantIndex >= Variants.Num())
		{
			SetStage(EGateBProofStage::Finalize);
			return false;
		}
		if (FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}

		ResetCaseCapture();
		if (!RestoreDiagnosticAttackOverrides())
		{
			Fail(TEXT("Previous diagnostic AttackData overrides were not restored"));
			return false;
		}
		AppliedDiagnosticWarpOffset = FVector::ZeroVector;
		AppliedDiagnosticMaxWarpDistance = 0.0f;
		AppliedDiagnosticActiveStartOffset = 0.0f;
		AppliedDiagnosticActiveDuration = 0.0f;
		const FGateBRunVariant& Variant = CurrentVariant();
		const FGateBNormalCase& Case = CurrentCase();
		UGameplayStatics::SetGlobalTimeDilation(World.Get(), Variant.WorldDilation);
		FDefenseMatrixProofCase* RuntimeCase = Director->Cases.FindByPredicate(
			[&Case](const FDefenseMatrixProofCase& Candidate)
			{
				return Candidate.CaseName == FName(*Case.Expected.Name);
			});
		if (!RuntimeCase)
		{
			Fail(FString::Printf(TEXT("Director has no runtime case '%s'"),
				*Case.Expected.Name));
			return false;
		}

		const bool bOriginalApplyDefenderTransform = RuntimeCase->bApplyDefenderTransform;
		const FTransform OriginalDefenderTransform = RuntimeCase->DefenderTransform;
		const bool bOriginalApplyAttackerTransform = RuntimeCase->bApplyAttackerTransform;
		const FTransform OriginalAttackerTransform = RuntimeCase->AttackerTransform;
		UAttackData* OriginalAttack = RuntimeCase->Attack;
		if (bDiagnosticMode)
		{
			if (!DiagnosticAttack.IsValid())
			{
				UAttackData* LoadedAttack = LoadObject<UAttackData>(
					nullptr, *DiagnosticAttackPath);
				if (bDiagnosticMontageOverrideRequested)
				{
					if (bDiagnosticSequenceRequested
						&& (bDiagnosticMontageRequested || bDiagnosticSectionRequested))
					{
						Fail(TEXT("Diagnostic sequence and montage overrides are mutually exclusive"));
						return false;
					}
					if (!bDiagnosticSequenceRequested
						&& (!bDiagnosticMontageRequested || !bDiagnosticSectionRequested
							|| DiagnosticSection.IsNone()))
					{
						Fail(TEXT("Diagnostic montage overrides require both montage and section"));
						return false;
					}
					UAnimMontage* ProbeMontage = nullptr;
					if (bDiagnosticSequenceRequested)
					{
						if (UAnimSequenceBase* SourceSequence = LoadObject<UAnimSequenceBase>(
							nullptr, *DiagnosticSequencePath))
						{
							ProbeMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
								SourceSequence, FName(TEXT("DefaultSlot")), 0.0f, 0.0f);
						}
					}
					else
					{
						UAnimMontage* SourceMontage = LoadObject<UAnimMontage>(
							nullptr, *DiagnosticMontagePath);
						if (SourceMontage
						&& SourceMontage->GetSectionIndex(DiagnosticSection) == INDEX_NONE)
						{
							Fail(FString::Printf(
								TEXT("Diagnostic montage has no section '%s': %s"),
								*DiagnosticSection.ToString(), *DiagnosticMontagePath));
							return false;
						}
						ProbeMontage = SourceMontage
							? DuplicateObject<UAnimMontage>(SourceMontage,
								GetTransientPackage(), TEXT("GateB_DiagnosticMontage"))
							: nullptr;
					}
					UAttackData* ProbeAttack = LoadedAttack
						? DuplicateObject<UAttackData>(LoadedAttack, GetTransientPackage(),
							TEXT("GateB_DiagnosticAttack")) : nullptr;
					if (!ProbeAttack || !ProbeMontage)
					{
						Fail(FString::Printf(
							TEXT("Diagnostic animation source or AttackData template did not load (%s, %s)"),
							*DiagnosticAttackPath,
							bDiagnosticSequenceRequested
								? *DiagnosticSequencePath : *DiagnosticMontagePath));
						return false;
					}
					ProbeAttack->AttackMontage = ProbeMontage;
					ProbeAttack->MontageSection = DiagnosticSection;
					ProbeAttack->bUseSectionOnly = !bDiagnosticSequenceRequested;
					DiagnosticAttack.Reset(ProbeAttack);
				}
				else
				{
					DiagnosticAttack.Reset(LoadedAttack);
				}
			}
			if (!DiagnosticAttack.IsValid() || !DiagnosticAttack->AttackMontage)
			{
				Fail(FString::Printf(TEXT("Diagnostic AttackData did not load: %s"),
					*DiagnosticAttackPath));
				return false;
			}
			RuntimeCase->Attack = DiagnosticAttack.Get();
			if (bDiagnosticAttackOverrideRequested)
			{
				if (bDiagnosticMontageOverrideRequested
					&& !bDiagnosticActiveWindowRequested)
				{
					Fail(TEXT("Diagnostic animation-source overrides require an explicit active window"));
					return false;
				}
				if (bDiagnosticWarpOffsetRequested && DiagnosticWarpOffset.ContainsNaN())
				{
					Fail(TEXT("Diagnostic attack warp offset must be finite"));
					return false;
				}
				if (bDiagnosticMaxWarpDistanceRequested
					&& (!FMath::IsFinite(DiagnosticMaxWarpDistance)
						|| DiagnosticMaxWarpDistance < 0.0f))
				{
					Fail(TEXT("Diagnostic maximum warp distance must be finite and non-negative"));
					return false;
				}
				if (bDiagnosticActiveWindowRequested
					&& (!bDiagnosticActiveStartRequested
						|| !bDiagnosticActiveDurationRequested))
				{
					Fail(TEXT("Diagnostic active-window overrides require both start offset and duration"));
					return false;
				}
				if (bDiagnosticActiveWindowRequested
					&& (!FMath::IsFinite(DiagnosticActiveStartOffset)
						|| !FMath::IsFinite(DiagnosticActiveDuration)
						|| DiagnosticActiveStartOffset < 0.0f
						|| DiagnosticActiveDuration <= 0.0f))
				{
					Fail(TEXT("Diagnostic active-window start must be non-negative and duration must be positive"));
					return false;
				}
				OriginalDiagnosticWarpOffset =
					DiagnosticAttack->WarpConfig.TargetRelativeOffset;
				OriginalDiagnosticMaxWarpDistance =
					DiagnosticAttack->WarpConfig.MaxWarpDistance;
				OriginalDiagnosticManualTiming = DiagnosticAttack->ManualTiming;
				AppliedDiagnosticWarpOffset = OriginalDiagnosticWarpOffset;
				AppliedDiagnosticMaxWarpDistance =
					OriginalDiagnosticMaxWarpDistance;
				if (bDiagnosticWarpOffsetXRequested)
				{
					AppliedDiagnosticWarpOffset.X = DiagnosticWarpOffset.X;
				}
				if (bDiagnosticWarpOffsetYRequested)
				{
					AppliedDiagnosticWarpOffset.Y = DiagnosticWarpOffset.Y;
				}
				if (bDiagnosticWarpOffsetZRequested)
				{
					AppliedDiagnosticWarpOffset.Z = DiagnosticWarpOffset.Z;
				}
				if (bDiagnosticMaxWarpDistanceRequested)
				{
					AppliedDiagnosticMaxWarpDistance = DiagnosticMaxWarpDistance;
				}
				UPackage* DiagnosticPackage = DiagnosticAttack->GetOutermost();
				bOriginalDiagnosticPackageDirty = DiagnosticPackage
					&& DiagnosticPackage->IsDirty();
				UPackage* DiagnosticMontagePackage =
					DiagnosticAttack->AttackMontage->GetOutermost();
				bOriginalDiagnosticMontagePackageDirty = DiagnosticMontagePackage
					&& DiagnosticMontagePackage->IsDirty();
				OriginalDiagnosticNotifyTrackCount =
					DiagnosticAttack->AttackMontage->AnimNotifyTracks.Num();
				DiagnosticAttack->WarpConfig.TargetRelativeOffset =
					AppliedDiagnosticWarpOffset;
				DiagnosticAttack->WarpConfig.MaxWarpDistance =
					AppliedDiagnosticMaxWarpDistance;
				bDiagnosticAttackOverrideApplied = true;
				if (bDiagnosticActiveWindowRequested
					&& !ApplyDiagnosticActiveWindow())
				{
					RestoreDiagnosticAttackOverrides();
					return false;
				}
			}
		}
		else if (bDiagnosticAttackOverrideRequested)
		{
			Fail(TEXT("Diagnostic AttackData overrides require DefenseGateBDiagnosticAttack"));
			return false;
		}
		if (bTransformOverrideRequested)
		{
			FTransform DefenderTransform = RuntimeCase->bApplyDefenderTransform
				? RuntimeCase->DefenderTransform
				: Player->GetActorTransform();
			if (bDefenderYawOverrideRequested)
			{
				DefenderTransform.SetRotation((
					DefenderTransform.Rotator()
					+ FRotator(0.0f, ProofPlayerYawOffset, 0.0f)).Quaternion());
			}

			const FVector DefenderLocation = DefenderTransform.GetLocation();
			FVector AttackOffset = RuntimeCase->bApplyAttackerTransform
				? RuntimeCase->AttackerTransform.GetLocation() - DefenderLocation
				: FVector::ForwardVector;
			AttackOffset.Z = 0.0f;
			if (AttackOffset.IsNearlyZero())
			{
				AttackOffset = FVector::ForwardVector;
			}
			const float ControlledRadius = bRadiusOverrideRequested
				? ProofRadius
				: AttackOffset.Size2D();
			const FVector ControlledLocation = DefenderLocation
				+ AttackOffset.GetSafeNormal2D() * ControlledRadius
				+ ProofOffset;

			RuntimeCase->bApplyDefenderTransform = true;
			RuntimeCase->DefenderTransform = DefenderTransform;
			RuntimeCase->bApplyAttackerTransform = true;
			RuntimeCase->AttackerTransform = FTransform(
				(DefenderLocation - ControlledLocation).Rotation()
					+ FRotator(0.0f, ProofYawOffset, 0.0f),
				ControlledLocation);
		}

		ClearCaseTelemetry();
		const bool bCaseStarted = Director->StartNamedCase(FName(*Case.Expected.Name));
		RuntimeCase->bApplyDefenderTransform = bOriginalApplyDefenderTransform;
		RuntimeCase->DefenderTransform = OriginalDefenderTransform;
		RuntimeCase->bApplyAttackerTransform = bOriginalApplyAttackerTransform;
		RuntimeCase->AttackerTransform = OriginalAttackerTransform;
		RuntimeCase->Attack = OriginalAttack;
		if (!bCaseStarted)
		{
			Fail(FString::Printf(TEXT("Director could not start case '%s'"),
				*Case.Expected.Name));
			return false;
		}

		const FString SelectedAttackPath = bDiagnosticMode
			? DiagnosticAttack->GetPathName()
			: Case.AttackDataPath;
		SelectedEnemy = FindSelectedEnemy(SelectedAttackPath);
		SelectedAI = SelectedEnemy.IsValid()
			? SelectedEnemy->GetCombatAIComponent()
			: nullptr;
		if (!SelectedEnemy.IsValid() || !SelectedAI.IsValid())
		{
			Fail(FString::Printf(TEXT("Case '%s' did not isolate one selected attacker"),
				*Case.Expected.Name));
			return false;
		}
		UCombatComponent* SelectedCombat = SelectedEnemy->GetCombatComponent();
		if (!SelectedCombat)
		{
			Fail(FString::Printf(TEXT("Case '%s' selected attacker has no combat component"),
				*Case.Expected.Name));
			return false;
		}
		SelectedCombat->SetAttackMontagePlayRateForTesting(Variant.MontageRate);
		RemoveTraceBinding();
		BoundTraceWeapon = SelectedEnemy->WeaponComponent.Get();
		if (!BoundTraceWeapon.IsValid())
		{
			Fail(FString::Printf(TEXT("Case '%s' selected attacker has no weapon component"),
				*Case.Expected.Name));
			return false;
		}
		BoundTraceWeapon->OnTraceFrameForTesting.AddRaw(
			this, &FDefenseGateBPIEProofCommand::HandleTraceFrame);
		bTraceBound = true;
		PlayerStartHealth = Player->CurrentHealth;
		PlayerStartTransform = Player->GetActorTransform();
		EnemyStartTransform = SelectedEnemy->GetActorTransform();
		if (!ConfigureProofCamera())
		{
			Fail(FString::Printf(TEXT("Case '%s' could not configure rendered proof camera"),
				*Case.Expected.Name));
			return false;
		}
		for (int32 Index = 0; Index < Enemies.Num(); ++Index)
		{
			AEnemyCharacter* Enemy = Enemies[Index].Get();
			if (!Enemy || Enemy == SelectedEnemy.Get())
			{
				continue;
			}
			if (UEnemyCombatAIComponent* AI = Enemy->GetCombatAIComponent())
			{
				AI->SetCombatTarget(nullptr);
			}
			Enemy->SetActorLocation(
				Player->GetActorLocation() + FVector(2000.0f + Index * 200.0f, 1500.0f, 0.0f),
				false, nullptr, ETeleportType::TeleportPhysics);
		}
		if (!SelectedAI->IsAttacking() && !SelectedAI->ExecuteAttack())
		{
			Fail(FString::Printf(TEXT("Case '%s' could not execute under director control"),
				*Case.Expected.Name));
			return false;
		}
		SetStage(EGateBProofStage::AwaitAttack);
		return false;
	}

	bool UpdateAwaitAttack()
	{
		if (SelectedAI.IsValid() && SelectedAI->IsAttacking())
		{
			ExpectedAttackGeneration = SelectedAI->GetActiveAttackGeneration();
			UAnimInstance* AnimInstance = SelectedEnemy->GetMesh()
				? SelectedEnemy->GetMesh()->GetAnimInstance()
				: nullptr;
			UAttackData* Attack = SelectedAI->SelectedAttack;
			if (ExpectedAttackGeneration <= 0 || !AnimInstance || !Attack
				|| !Attack->AttackMontage)
			{
				Fail(FString::Printf(TEXT("Case '%s' entered attack without generation or montage"),
					*CurrentCase().Expected.Name));
				return false;
			}
			ObservedMontageRate = AnimInstance->Montage_GetPlayRate(Attack->AttackMontage);
			if (!FMath::IsNearlyEqual(
				ObservedMontageRate, CurrentVariant().MontageRate, 0.001f))
			{
				Fail(FString::Printf(
					TEXT("Case '%s' started at montage rate %.3f instead of %.3f"),
					*CurrentCase().Expected.Name,
					ObservedMontageRate,
					CurrentVariant().MontageRate));
				return false;
			}
			CaptureFrame(TEXT("attack_started"));
			SetStage(EGateBProofStage::AwaitResolution);
			return false;
		}
		if (StageElapsed() > 5.0 / FMath::Max(0.1f, CurrentVariant().WorldDilation))
		{
			Fail(FString::Printf(TEXT("Case '%s' never entered Attacking"),
				*CurrentCase().Expected.Name));
		}
		return false;
	}

	bool UpdateAwaitResolution()
	{
		if (bHasResolution)
		{
			SetStage(EGateBProofStage::ObservePresentation);
			return false;
		}
		if (StageElapsed() > 8.0 / FMath::Max(0.1f,
			CurrentVariant().MontageRate * CurrentVariant().WorldDilation))
		{
			Fail(FString::Printf(
				TEXT("Case '%s' produced no physical contact resolution (state=%s generation=%d trace_frames=%d trace_radius=%.2f min_blade_target_cm=%.2f target_delta=%s capsule_separation=%.2f capsule_delta=%s)"),
				*CurrentCase().Expected.Name,
				*EnumName(StaticEnum<EEnemyAIState>(),
					static_cast<int64>(SelectedAI->CurrentState)),
				ExpectedAttackGeneration,
				TraceFrameCount,
				TraceRadius,
				MinimumBladeToTargetCm,
				*ClosestBladeDelta.ToString(),
				MinimumTraceToCapsuleSeparationCm,
				*ClosestCapsuleDelta.ToString()));
		}
		return false;
	}

	bool UpdateObservePresentation()
	{
		SamplePresentationMontages();
		if (StageElapsed() < 0.15)
		{
			return false;
		}
		const FGateBNormalCase& Case = CurrentCase();
		const FDefenseDecision& Decision = LastResolution.Decision;
		const FString SelectedAttackPath = bDiagnosticMode && DiagnosticAttack.IsValid()
			? DiagnosticAttack->GetPathName()
			: Case.AttackDataPath;
		const bool bIdentity = LastResolution.InteractionId.IsValid()
			&& Decision.AttackInstance.IsValid()
			&& Decision.AttackInstance.Attacker.Get() == SelectedEnemy.Get()
			&& Decision.AttackInstance.AttackGeneration == ExpectedAttackGeneration
			&& Decision.SelectedAttack
			&& Decision.SelectedAttack->GetPathName() == SelectedAttackPath;
		const bool bDecision = EnumMatches(Decision.Outcome, Case.Expected.Outcome)
			&& EnumMatches(Decision.Reason, Case.Expected.Reason)
			&& EnumMatches(Decision.Height, Case.Expected.ExpectedHeight)
			&& EnumMatches(Decision.Lane, Case.Expected.ExpectedLane)
			&& EnumMatches(Decision.SwingShape, Case.Expected.ExpectedSwing)
			&& EnumMatches(Decision.AttackerResponse, Case.Expected.AttackerResponse);
		const FVector ContactWeaponVelocity =
			LastResolution.ActualContact.HitInfo.WeaponVelocity;
		const bool bContactWeaponVelocityUsable =
			!ContactWeaponVelocity.ContainsNaN()
			&& !ContactWeaponVelocity.IsNearlyZero();
		const FVector NormalizedContactWeaponTrajectory =
			ContactWeaponVelocity.GetSafeNormal2D();
		const bool bTrajectorySourceProven =
			LastResolution.ActualContact.bIncomingTrajectoryRateNormalized
			|| (bContactWeaponVelocityUsable
				&& LastResolution.ActualContact.IncomingTrajectory.Equals(
					NormalizedContactWeaponTrajectory, 0.01f));
		const bool bContact = LastResolution.bHasActualContact
			&& LastResolution.ActualContact.bIsValid
			&& bTrajectorySourceProven
			&& EnumMatches(LastResolution.ActualContact.Lane,
				Case.Expected.ExpectedLane)
			&& EnumMatches(LastResolution.ActualContact.LaneProvenance,
				Case.Expected.ExpectedLaneProvenance)
			&& LastResolution.ActualContact.SourceSocket == TEXT("weapon_end")
			&& LastResolution.ActualContact.ResolvedTargetBone
				== FName(*Case.Presentation.ExpectedTargetBone);
		const bool bPhysicalTargeting = bResolutionTargetBoneValid
			&& ContactTargetVerticalDeltaCm
				<= Case.Presentation.MaxContactTargetVerticalDeltaCm;
		const bool bRows = LastResolution.PresentationRow
				== FName(*Case.Presentation.DefenderRow)
			&& LastResolution.AttackerPresentationRow
				== FName(*Case.Presentation.AttackerRow);
		const bool bPresentation = LastResolution.Presentation.Montage != nullptr
			&& LastResolution.Presentation.ImpactAudio.IsActive()
			&& LastResolution.Presentation.ImpactVFX.IsActive()
			&& (!Case.Presentation.bRequiresAttackerMontage
				|| LastResolution.AttackerPresentation.Montage != nullptr)
			&& (Case.Presentation.bRequiresAttackerMontage
				|| LastResolution.AttackerPresentation.Montage == nullptr);
		const bool bNoDamage = FMath::IsNearlyEqual(Player->CurrentHealth, PlayerStartHealth);
		const bool bEffects = AudioInvocationCount == 1
			&& (!FApp::CanEverRender() || VFXInvocationCount == 1);
		const bool bDefenderMontagePlayback =
			!Case.Presentation.bRequiresDefenderMontage
			|| (bDefenderPresentationObservedActive
				&& DefenderPresentationMaxWeight >= MinimumMontageObservedWeight
				&& DefenderPresentationMaxProgress >= MinimumMontageProgressSeconds);
		const bool bAttackerMontagePlayback =
			!Case.Presentation.bRequiresAttackerMontage
			|| (bAttackerPresentationObservedActive
				&& AttackerPresentationMaxWeight >= MinimumMontageObservedWeight
				&& AttackerPresentationMaxProgress >= MinimumMontageProgressSeconds);
		const bool bDiagnosticContact = LastResolution.bHasActualContact
			&& LastResolution.ActualContact.bIsValid
			&& LastResolution.ActualContact.LaneProvenance
				== EDefenseLaneProvenance::WeaponVelocity
			&& LastResolution.ActualContact.SourceSocket == TEXT("weapon_end");
		const bool bPassed = bDiagnosticMode
			? bIdentity && bDiagnosticContact && bNoDamage
			: bIdentity && bDecision && bContact && bPhysicalTargeting && bRows
			&& bPresentation && bDefenderMontagePlayback
			&& bAttackerMontagePlayback && bNoDamage && bEffects;

		if (bDiagnosticMode)
		{
			Test->TestTrue(FString::Printf(
				TEXT("%s records diagnostic rich contact without damage"),
				*CurrentVariant().Name(Cases)), bPassed);
		}
		else
		{
			Test->TestTrue(FString::Printf(TEXT("%s uses one canonical attack identity"),
				*CurrentVariant().Name(Cases)), bIdentity);
			Test->TestTrue(FString::Printf(TEXT("%s matches expected decision tuple"),
				*CurrentVariant().Name(Cases)), bDecision);
			Test->TestTrue(FString::Printf(TEXT("%s resolves physical contact provenance"),
				*CurrentVariant().Name(Cases)), bContact);
			Test->TestTrue(FString::Printf(
				TEXT("%s contacts its reviewed target height within %.2f cm (actual %.2f cm)"),
				*CurrentVariant().Name(Cases),
				Case.Presentation.MaxContactTargetVerticalDeltaCm,
				ContactTargetVerticalDeltaCm), bPhysicalTargeting);
			Test->TestTrue(FString::Printf(TEXT("%s selects expected presentation rows"),
				*CurrentVariant().Name(Cases)), bRows);
			Test->TestTrue(FString::Printf(TEXT("%s selects required presentation payloads"),
				*CurrentVariant().Name(Cases)), bPresentation);
			Test->TestTrue(FString::Printf(
				TEXT("%s visibly advances the defender presentation montage"),
				*CurrentVariant().Name(Cases)), bDefenderMontagePlayback);
			Test->TestTrue(FString::Printf(
				TEXT("%s visibly advances any required attacker response montage"),
				*CurrentVariant().Name(Cases)), bAttackerMontagePlayback);
			Test->TestTrue(FString::Printf(TEXT("%s suppresses blocked damage"),
				*CurrentVariant().Name(Cases)), bNoDamage);
			Test->TestTrue(FString::Printf(TEXT("%s invokes all available effects exactly once"),
				*CurrentVariant().Name(Cases)), bEffects);
		}

		CurrentCasePassed = bPassed;
		CaptureFrame(TEXT("contact_resolution"));
		SetStage(EGateBProofStage::AwaitNaturalCleanup);
		return false;
	}

	bool UpdateAwaitNaturalCleanup()
	{
		SamplePresentationMontages();
		const bool bClean = SelectedAI.IsValid()
			&& !SelectedAI->HasAttackToken()
			&& !SelectedAI->IsWaitingForToken()
			&& !SelectedAI->IsAttacking()
			&& TokenSubsystem->GetActiveAttackerCount() == 0
			&& TokenSubsystem->GetQueueLength() == 0;
		if (!bClean)
		{
			if (StageElapsed() > 10.0 / FMath::Max(0.1f,
				CurrentVariant().MontageRate * CurrentVariant().WorldDilation))
			{
				Fail(FString::Printf(TEXT("Case '%s' did not naturally release attack/token ownership"),
					*CurrentVariant().Name(Cases)));
			}
			return false;
		}

		const int32 ResolutionTelemetry = CountTelemetry(
			EDefenseTelemetryEvent::Resolution, LastResolution.InteractionId);
		const int32 PresentationTelemetry = CountTelemetry(
			EDefenseTelemetryEvent::PresentationStart, LastResolution.InteractionId);
		const bool bTelemetry = ResolutionTelemetry == 1 && PresentationTelemetry >= 1;
		Test->TestTrue(FString::Printf(TEXT("%s retains exactly one resolution telemetry row"),
			*CurrentVariant().Name(Cases)), ResolutionTelemetry == 1);
		Test->TestTrue(FString::Printf(TEXT("%s retains committed presentation telemetry"),
			*CurrentVariant().Name(Cases)), PresentationTelemetry >= 1);
		CurrentCasePassed = CurrentCasePassed && bTelemetry;
		TArray<FDefenseTelemetryRecord> CaseTelemetry;
		CollectCaseTelemetry(CaseTelemetry);
		int32 AlignmentFrameCount = 0;
		float MaxYawOverBudget = 0.0f;
		float MaxUnexpectedDisplacement = 0.0f;
		float MaxAlignmentPelvisFrameDelta = 0.0f;
		for (const FDefenseTelemetryRecord& Record : CaseTelemetry)
		{
			if (Record.Event != EDefenseTelemetryEvent::AlignmentFrame)
			{
				continue;
			}
			++AlignmentFrameCount;
			if (Record.FrameSimulationDelta > 0.0f && Record.MaximumTurnRate > 0.0f)
			{
				const float AllowedYaw = Record.MaximumTurnRate
					* Record.FrameSimulationDelta;
				MaxYawOverBudget = FMath::Max(MaxYawOverBudget,
					FMath::Max(0.0f, FMath::Abs(Record.AppliedFrameYaw) - AllowedYaw));
			}
			MaxUnexpectedDisplacement = FMath::Max(MaxUnexpectedDisplacement,
				Record.UnexpectedDisplacement.Size2D());
			MaxAlignmentPelvisFrameDelta = FMath::Max(
				MaxAlignmentPelvisFrameDelta, Record.PelvisDelta);
		}
		CollectedTelemetry.Append(CaseTelemetry);
		const float PlayerDisplacement = FVector::Dist2D(
			PlayerStartTransform.GetLocation(), Player->GetActorLocation());
		const float AttackerDisplacement = FVector::Dist2D(
			EnemyStartTransform.GetLocation(), SelectedEnemy->GetActorLocation());
		const float PlayerDamage = FMath::Max(
			0.0f, PlayerStartHealth - Player->CurrentHealth);
		const bool bNormalBlockDisplacement = PlayerDisplacement
			<= MaximumNormalBlockDisplacementCm + KINDA_SMALL_NUMBER;
		const bool bAlignmentFrames = AlignmentFrameCount > 0;
		const bool bYawBudget = MaxYawOverBudget
			<= MaximumFrameYawOverBudgetDegrees + KINDA_SMALL_NUMBER;
		const bool bUnexpectedDisplacement = MaxUnexpectedDisplacement
			<= MaximumUnexpectedFrameDisplacementCm + KINDA_SMALL_NUMBER;
		Test->TestTrue(FString::Printf(
			TEXT("%s emits evaluated alignment frames"),
			*CurrentVariant().Name(Cases)), bAlignmentFrames);
		Test->TestTrue(FString::Printf(
			TEXT("%s stays within turn-rate * simulation-delta + %.1f degrees"),
			*CurrentVariant().Name(Cases), MaximumFrameYawOverBudgetDegrees), bYawBudget);
		Test->TestTrue(FString::Printf(
			TEXT("%s keeps normal-block horizontal drift within %.1f cm"),
			*CurrentVariant().Name(Cases), MaximumNormalBlockDisplacementCm),
			bNormalBlockDisplacement);
		Test->TestTrue(FString::Printf(
			TEXT("%s keeps unexpected frame displacement within %.1f cm"),
			*CurrentVariant().Name(Cases), MaximumUnexpectedFrameDisplacementCm),
			bUnexpectedDisplacement);
		CurrentCasePassed = CurrentCasePassed && bAlignmentFrames && bYawBudget
			&& bNormalBlockDisplacement && bUnexpectedDisplacement;

		RestoreGlobalTimeDilation();
		Director->ResetFixture();
		const bool bFixtureRestored = Director->ActiveCase.IsNone()
			&& TokenSubsystem->GetActiveAttackerCount() == 0
			&& TokenSubsystem->GetQueueLength() == 0;
		const bool bDiagnosticDataRestored = RestoreDiagnosticAttackOverrides();
		const bool bRestored = bFixtureRestored && bDiagnosticDataRestored;
		Test->TestTrue(FString::Printf(TEXT("%s reset restores fixture ownership"),
			*CurrentVariant().Name(Cases)), bFixtureRestored);
		Test->TestTrue(FString::Printf(TEXT("%s restores diagnostic AttackData"),
			*CurrentVariant().Name(Cases)), bDiagnosticDataRestored);
		CurrentCasePassed = CurrentCasePassed && bRestored;
		RecordVariant(
			ResolutionTelemetry,
			PresentationTelemetry,
			PlayerDisplacement,
			AttackerDisplacement,
			PlayerDamage,
			bRestored,
			AlignmentFrameCount,
			MaxYawOverBudget,
			MaxUnexpectedDisplacement,
			MaxAlignmentPelvisFrameDelta);
		++VariantIndex;
		SetStage(EGateBProofStage::StartCase);
		return false;
	}

	bool UpdateFinalize()
	{
		if (FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		FinalizeEvidence();
		Cleanup();
		SetStage(EGateBProofStage::Done);
		return true;
	}

	void SetStage(const EGateBProofStage NewStage)
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

	const FGateBRunVariant& CurrentVariant() const
	{
		return Variants[VariantIndex];
	}

	const FGateBNormalCase& CurrentCase() const
	{
		return Cases[CurrentVariant().CaseIndex];
	}

	void ResetCaseCapture()
	{
		bHasResolution = false;
		CurrentCasePassed = false;
		ExpectedAttackGeneration = 0;
		AudioInvocationCount = 0;
		VFXInvocationCount = 0;
		MinimumBladeToTargetCm = BIG_NUMBER;
		MinimumTraceToCapsuleSeparationCm = BIG_NUMBER;
		ClosestBladeDelta = FVector::ZeroVector;
		ClosestCapsuleDelta = FVector::ZeroVector;
		TraceFrameCount = 0;
		TraceRadius = 0.0f;
		ResolutionTargetBoneLocation = FVector::ZeroVector;
		StandardTargetBoneLocations.Reset();
		ContactTargetDelta = FVector::ZeroVector;
		ContactTargetVerticalDeltaCm = BIG_NUMBER;
		SourceSocketVelocityAtResolution = FVector::ZeroVector;
		ActiveWindowEntryTipLocation = FVector::ZeroVector;
		LatestTraceTipLocation = FVector::ZeroVector;
		ActiveWindowTipTrajectoryAtResolution = FVector::ZeroVector;
		RateNormalizedTipTrajectoryAtResolution = FVector::ZeroVector;
		ShortRateNormalizedTipTrajectoryAtResolution = FVector::ZeroVector;
		SourceSocketLaneAtResolution = EIncomingAttackLane::Center;
		SourceSocketLaneProvenanceAtResolution = EDefenseLaneProvenance::None;
		ActiveWindowTipLaneAtResolution = EIncomingAttackLane::Center;
		ActiveWindowTipLaneProvenanceAtResolution = EDefenseLaneProvenance::None;
		RateNormalizedTipLaneAtResolution = EIncomingAttackLane::Center;
		RateNormalizedTipLaneProvenanceAtResolution = EDefenseLaneProvenance::None;
		ShortRateNormalizedTipLaneAtResolution = EIncomingAttackLane::Center;
		ShortRateNormalizedTipLaneProvenanceAtResolution = EDefenseLaneProvenance::None;
		SourceSocketLaneCenterHalfAngleAtResolution = 12.0f;
		bResolutionTargetBoneValid = false;
		bSourceSocketVelocityUsableAtResolution = false;
		bHasActiveWindowEntryTipLocation = false;
		bActiveWindowTipTrajectoryUsableAtResolution = false;
		bRateNormalizedTipTrajectoryUsableAtResolution = false;
		bShortRateNormalizedTipTrajectoryUsableAtResolution = false;
		PlayerYawAtResolution = 0.0f;
		ObservedMontageRate = 0.0f;
		DefenderPresentationStartPosition = -1.0f;
		AttackerPresentationStartPosition = -1.0f;
		DefenderPresentationMaxProgress = 0.0f;
		AttackerPresentationMaxProgress = 0.0f;
		DefenderPresentationMaxWeight = 0.0f;
		AttackerPresentationMaxWeight = 0.0f;
		bDefenderPresentationObservedActive = false;
		bAttackerPresentationObservedActive = false;
		DefenderPresentationMontage.Reset();
		AttackerPresentationMontage.Reset();
		LastResolution = {};
		SelectedEnemy.Reset();
		SelectedAI.Reset();
	}

	void HandleTraceFrame(
		const TArray<FVector>& PreviousPoints,
		const TArray<FVector>& CurrentPoints,
		const float Radius)
	{
		USkeletalMeshComponent* Mesh = Player.IsValid() ? Player->GetMesh() : nullptr;
		if (!Mesh || CurrentPoints.Num() < 2
			|| PreviousPoints.Num() != CurrentPoints.Num())
		{
			return;
		}
		if (!bHasActiveWindowEntryTipLocation)
		{
			ActiveWindowEntryTipLocation = PreviousPoints.Last();
			bHasActiveWindowEntryTipLocation = true;
		}
		LatestTraceTipLocation = CurrentPoints.Last();
		const FVector Target = Mesh->GetBoneLocation(
			FName(*CurrentCase().Presentation.ExpectedTargetBone));
		const UCapsuleComponent* Capsule = Player->GetCapsuleComponent();
		const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.0f;
		const float AxisHalfHeight = Capsule
			? FMath::Max(0.0f, Capsule->GetScaledCapsuleHalfHeight() - CapsuleRadius)
			: 0.0f;
		const FVector CapsuleCenter = Capsule
			? Capsule->GetComponentLocation()
			: Player->GetActorLocation();
		const FVector CapsuleAxisStart = CapsuleCenter - FVector::UpVector * AxisHalfHeight;
		const FVector CapsuleAxisEnd = CapsuleCenter + FVector::UpVector * AxisHalfHeight;
		++TraceFrameCount;
		TraceRadius = Radius;
		for (int32 Index = 0; Index < CurrentPoints.Num(); ++Index)
		{
			const FVector Closest = FMath::ClosestPointOnSegment(
				Target, PreviousPoints[Index], CurrentPoints[Index]);
			const float Distance = FVector::Distance(Target, Closest);
			if (Distance < MinimumBladeToTargetCm)
			{
				MinimumBladeToTargetCm = Distance;
				ClosestBladeDelta = Target - Closest;
			}
			FVector BladeClosest = FVector::ZeroVector;
			FVector CapsuleClosest = FVector::ZeroVector;
			FMath::SegmentDistToSegmentSafe(
				PreviousPoints[Index], CurrentPoints[Index],
				CapsuleAxisStart, CapsuleAxisEnd,
				BladeClosest, CapsuleClosest);
			const float Separation = FVector::Distance(BladeClosest, CapsuleClosest)
				- Radius - CapsuleRadius;
			if (Separation < MinimumTraceToCapsuleSeparationCm)
			{
				MinimumTraceToCapsuleSeparationCm = Separation;
				ClosestCapsuleDelta = CapsuleClosest - BladeClosest;
			}
		}
		for (int32 Index = 0; Index + 1 < CurrentPoints.Num(); ++Index)
		{
			const FVector Closest = FMath::ClosestPointOnSegment(
				Target, CurrentPoints[Index], CurrentPoints[Index + 1]);
			const float Distance = FVector::Distance(Target, Closest);
			if (Distance < MinimumBladeToTargetCm)
			{
				MinimumBladeToTargetCm = Distance;
				ClosestBladeDelta = Target - Closest;
			}
		}
	}

	void RemoveTraceBinding()
	{
		if (bTraceBound && BoundTraceWeapon.IsValid())
		{
			BoundTraceWeapon->OnTraceFrameForTesting.RemoveAll(this);
		}
		bTraceBound = false;
		BoundTraceWeapon.Reset();
	}

	AEnemyCharacter* FindSelectedEnemy(const FString& ExpectedAttackPath) const
	{
		AEnemyCharacter* Found = nullptr;
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			UEnemyCombatAIComponent* AI = Enemy.IsValid()
				? Enemy->GetCombatAIComponent()
				: nullptr;
			if (!AI || AI->AvailableAttacks.Num() != 1
				|| !AI->AvailableAttacks[0].AttackData
				|| AI->AvailableAttacks[0].AttackData->GetPathName()
					!= ExpectedAttackPath)
			{
				continue;
			}
			if (Found)
			{
				return nullptr;
			}
			Found = Enemy.Get();
		}
		return Found;
	}

	void HandleDefenseResolved(const FDefenseResolution& Resolution)
	{
		if (Resolution.Stage != EDefenseQueryStage::Contact
			|| !SelectedEnemy.IsValid()
			|| Resolution.Decision.AttackInstance.Attacker.Get() != SelectedEnemy.Get())
		{
			return;
		}
		if (ExpectedAttackGeneration > 0
			&& Resolution.Decision.AttackInstance.AttackGeneration
				!= ExpectedAttackGeneration)
		{
			return;
		}
		LastResolution = Resolution;
		DefenderPresentationMontage = Resolution.Presentation.Montage;
		AttackerPresentationMontage = Resolution.AttackerPresentation.Montage;
		SamplePresentationMontages();
		if (const UWeaponComponent* Weapon = SelectedEnemy->WeaponComponent.Get())
		{
			SourceSocketVelocityAtResolution = Weapon->GetWeaponTipVelocity();
			bSourceSocketVelocityUsableAtResolution =
				!SourceSocketVelocityAtResolution.ContainsNaN()
				&& SourceSocketVelocityAtResolution.SizeSquared2D() > KINDA_SMALL_NUMBER;
			const UDefenseConfiguration* Configuration = PlayerCombat.IsValid()
				? PlayerCombat->GetEffectiveDefenseConfiguration()
				: GetDefault<UDefenseConfiguration>();
			SourceSocketLaneCenterHalfAngleAtResolution = Configuration
				? Configuration->CenterLaneHalfAngle
				: 12.0f;
			const EIncomingAttackLane AuthoredLane = Resolution.Decision.SelectedAttack
				? Resolution.Decision.SelectedAttack->DefenseProfile.NominalLane
				: EIncomingAttackLane::Center;
			const FDefenseLaneResolution SourceSocketLane =
				FDefenseResolver::ResolveIncomingLane(
					SourceSocketVelocityAtResolution,
					FVector::ZeroVector,
					FVector::ZeroVector,
					AuthoredLane,
					Player.IsValid() ? Player->GetActorTransform() : FTransform::Identity,
					SourceSocketLaneCenterHalfAngleAtResolution);
			SourceSocketLaneAtResolution = SourceSocketLane.Lane;
			SourceSocketLaneProvenanceAtResolution = SourceSocketLane.Provenance;

			if (bHasActiveWindowEntryTipLocation)
			{
				ActiveWindowTipTrajectoryAtResolution =
					LatestTraceTipLocation - ActiveWindowEntryTipLocation;
				bActiveWindowTipTrajectoryUsableAtResolution =
					!ActiveWindowTipTrajectoryAtResolution.ContainsNaN()
					&& ActiveWindowTipTrajectoryAtResolution.SizeSquared2D()
						> KINDA_SMALL_NUMBER;
				const FDefenseLaneResolution ActiveWindowTipLane =
					FDefenseResolver::ResolveIncomingLane(
						ActiveWindowTipTrajectoryAtResolution,
						FVector::ZeroVector,
						FVector::ZeroVector,
						AuthoredLane,
						Player.IsValid()
							? Player->GetActorTransform()
							: FTransform::Identity,
						SourceSocketLaneCenterHalfAngleAtResolution);
				ActiveWindowTipLaneAtResolution = ActiveWindowTipLane.Lane;
				ActiveWindowTipLaneProvenanceAtResolution =
					ActiveWindowTipLane.Provenance;
			}

			const auto ResolveHistoryLane = [this, Weapon, AuthoredLane](
				const float WindowAnimationSeconds,
				FVector& OutTrajectory,
				EIncomingAttackLane& OutLane,
				EDefenseLaneProvenance& OutProvenance)
			{
				if (!Weapon->TryGetDefenseTrajectoryForWindowForTesting(
					WindowAnimationSeconds, OutTrajectory))
				{
					return false;
				}
				const FDefenseLaneResolution Resolution = FDefenseResolver::ResolveIncomingLane(
					OutTrajectory,
					FVector::ZeroVector,
					FVector::ZeroVector,
					AuthoredLane,
					Player.IsValid()
						? Player->GetActorTransform()
						: FTransform::Identity,
					SourceSocketLaneCenterHalfAngleAtResolution);
				OutLane = Resolution.Lane;
				OutProvenance = Resolution.Provenance;
				return true;
			};
			bRateNormalizedTipTrajectoryUsableAtResolution = ResolveHistoryLane(
				1.0f / 30.0f,
				RateNormalizedTipTrajectoryAtResolution,
				RateNormalizedTipLaneAtResolution,
				RateNormalizedTipLaneProvenanceAtResolution);
			bShortRateNormalizedTipTrajectoryUsableAtResolution = ResolveHistoryLane(
				1.0f / 60.0f,
				ShortRateNormalizedTipTrajectoryAtResolution,
				ShortRateNormalizedTipLaneAtResolution,
				ShortRateNormalizedTipLaneProvenanceAtResolution);
		}
		if (Player.IsValid())
		{
			if (const USkeletalMeshComponent* Mesh = Player->GetMesh())
			{
				static const FName StandardTargetBones[] = {
					TEXT("head"), TEXT("neck_01"), TEXT("spine_05"),
					TEXT("spine_03"), TEXT("spine_01"), TEXT("pelvis")};
				for (const FName Bone : StandardTargetBones)
				{
					if (Mesh->DoesSocketExist(Bone))
					{
						StandardTargetBoneLocations.Add(Bone, Mesh->GetBoneLocation(Bone));
					}
				}
				const FName TargetBone(*CurrentCase().Presentation.ExpectedTargetBone);
				bResolutionTargetBoneValid = Mesh->DoesSocketExist(TargetBone);
				if (bResolutionTargetBoneValid)
				{
					ResolutionTargetBoneLocation = Mesh->GetBoneLocation(TargetBone);
					ContactTargetDelta = Resolution.Decision.ContactPoint
						- ResolutionTargetBoneLocation;
					ContactTargetVerticalDeltaCm = FMath::Abs(ContactTargetDelta.Z);
				}
			}
		}
		PlayerYawAtResolution = Player.IsValid()
			? Player->GetActorRotation().Yaw
			: 0.0f;
		bHasResolution = true;
	}

	void HandleImpactSound(
		UWorld* PlayedWorld,
		USoundBase* PlayedSound,
		const FVector& ImpactLocation,
		AActor* Attacker)
	{
		(void)ImpactLocation;
		if (PlayedWorld == World.Get() && PlayedSound
			&& Attacker == SelectedEnemy.Get())
		{
			++AudioInvocationCount;
		}
	}

	void HandleImpactVFX(
		UWorld* PlayedWorld,
		UNiagaraSystem* SpawnedSystem,
		const FVector& ImpactLocation,
		const FName BoneName)
	{
		(void)ImpactLocation;
		(void)BoneName;
		if (PlayedWorld == World.Get() && SpawnedSystem)
		{
			++VFXInvocationCount;
		}
	}

	int32 CountTelemetry(
		const EDefenseTelemetryEvent Event,
		const FDefenseInteractionId& Interaction) const
	{
		int32 Count = 0;
		auto CountIn = [&Count, Event, &Interaction](const UCombatComponent* Combat)
		{
			if (!Combat)
			{
				return;
			}
			for (const FDefenseTelemetryRecord& Record : Combat->GetDefenseTelemetry())
			{
				Count += Record.Event == Event && Record.InteractionId == Interaction ? 1 : 0;
			}
		};
		CountIn(PlayerCombat.Get());
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			CountIn(Enemy.IsValid() ? Enemy->GetCombatComponent() : nullptr);
		}
		return Count;
	}

	void ClearCaseTelemetry()
	{
		if (PlayerCombat.IsValid())
		{
			PlayerCombat->ClearDefenseTelemetry();
		}
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			if (Enemy.IsValid() && Enemy->GetCombatComponent())
			{
				Enemy->GetCombatComponent()->ClearDefenseTelemetry();
			}
		}
	}

	void CollectCaseTelemetry(TArray<FDefenseTelemetryRecord>& OutRecords) const
	{
		if (PlayerCombat.IsValid())
		{
			OutRecords.Append(PlayerCombat->GetDefenseTelemetry());
		}
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			if (Enemy.IsValid() && Enemy->GetCombatComponent())
			{
				OutRecords.Append(Enemy->GetCombatComponent()->GetDefenseTelemetry());
			}
		}
	}

	void SamplePresentationMontages()
	{
		auto Sample = [](UAnimInstance* AnimInstance, UAnimMontage* Montage,
			float& StartPosition, float& MaxProgress, float& MaxWeight,
			bool& bObservedActive)
		{
			if (!AnimInstance || !Montage)
			{
				return;
			}
			const bool bActive = AnimInstance->Montage_IsActive(Montage);
			bObservedActive = bObservedActive || bActive;
			if (!bActive)
			{
				return;
			}
			const float Position = AnimInstance->Montage_GetPosition(Montage);
			if (StartPosition < 0.0f)
			{
				StartPosition = Position;
			}
			else
			{
				MaxProgress = FMath::Max(MaxProgress,
					FMath::Abs(Position - StartPosition));
			}
			if (const FAnimMontageInstance* Instance =
				AnimInstance->GetActiveInstanceForMontage(Montage))
			{
				MaxWeight = FMath::Max(MaxWeight, Instance->GetWeight());
			}
		};

		UAnimInstance* DefenderAnim = Player.IsValid() && Player->GetMesh()
			? Player->GetMesh()->GetAnimInstance()
			: nullptr;
		UAnimInstance* AttackerAnim = SelectedEnemy.IsValid()
			&& SelectedEnemy->GetMesh()
			? SelectedEnemy->GetMesh()->GetAnimInstance()
			: nullptr;
		Sample(DefenderAnim, DefenderPresentationMontage.Get(),
			DefenderPresentationStartPosition, DefenderPresentationMaxProgress,
			DefenderPresentationMaxWeight, bDefenderPresentationObservedActive);
		Sample(AttackerAnim, AttackerPresentationMontage.Get(),
			AttackerPresentationStartPosition, AttackerPresentationMaxProgress,
			AttackerPresentationMaxWeight, bAttackerPresentationObservedActive);
	}

	void SuspendControllerLogic()
	{
		if (!ControllerTickStates.IsEmpty())
		{
			return;
		}
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			AController* Controller = Enemy.IsValid() ? Enemy->GetController() : nullptr;
			if (!Controller)
			{
				continue;
			}
			FGateBControllerTickState State;
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
		for (const FGateBControllerTickState& State : ControllerTickStates)
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

	void CaptureFrame(const FString& Label)
	{
		if (!FApp::CanEverRender() || FScreenshotRequest::IsScreenshotRequested())
		{
			return;
		}
		APlayerController* Controller = Player.IsValid()
			? Cast<APlayerController>(Player->GetController())
			: nullptr;
		int32 Width = 0;
		int32 Height = 0;
		if (Controller)
		{
			Controller->GetViewportSize(Width, Height);
		}
		auto Project = [Controller, Width, Height](
			const FVector& WorldLocation,
			FVector2D& Screen)
		{
			return Controller && Width > 0 && Height > 0
				&& Controller->ProjectWorldLocationToScreen(
					WorldLocation, Screen, true)
				&& Screen.X >= MinimumFrameEdgeMarginPixels
				&& Screen.X <= Width - MinimumFrameEdgeMarginPixels
				&& Screen.Y >= MinimumFrameEdgeMarginPixels
				&& Screen.Y <= Height - MinimumFrameEdgeMarginPixels;
		};
		FVector2D PlayerScreen = FVector2D::ZeroVector;
		FVector2D EnemyScreen = FVector2D::ZeroVector;
		const bool bPlayerFramed = Player.IsValid()
			&& Project(Player->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f),
				PlayerScreen);
		const bool bEnemyFramed = SelectedEnemy.IsValid()
			&& Project(SelectedEnemy->GetActorLocation() + FVector(0.0f, 0.0f, 90.0f),
				EnemyScreen);
		const float ActorWorldSeparation = Player.IsValid() && SelectedEnemy.IsValid()
			? FVector::Dist2D(Player->GetActorLocation(), SelectedEnemy->GetActorLocation())
			: 0.0f;
		const UCapsuleComponent* PlayerCapsule = Player.IsValid()
			? Player->GetCapsuleComponent() : nullptr;
		const UCapsuleComponent* EnemyCapsule = SelectedEnemy.IsValid()
			? SelectedEnemy->GetCapsuleComponent() : nullptr;
		FVector2D PlayerCapsuleTopScreen = FVector2D::ZeroVector;
		FVector2D PlayerCapsuleBottomScreen = FVector2D::ZeroVector;
		FVector2D EnemyCapsuleTopScreen = FVector2D::ZeroVector;
		FVector2D EnemyCapsuleBottomScreen = FVector2D::ZeroVector;
		const bool bPlayerCapsuleFramed = PlayerCapsule
			&& Project(Player->GetActorLocation() + FVector(0.0f, 0.0f,
				PlayerCapsule->GetScaledCapsuleHalfHeight()), PlayerCapsuleTopScreen)
			&& Project(Player->GetActorLocation() - FVector(0.0f, 0.0f,
				PlayerCapsule->GetScaledCapsuleHalfHeight()), PlayerCapsuleBottomScreen);
		const bool bEnemyCapsuleFramed = EnemyCapsule
			&& Project(SelectedEnemy->GetActorLocation() + FVector(0.0f, 0.0f,
				EnemyCapsule->GetScaledCapsuleHalfHeight()), EnemyCapsuleTopScreen)
			&& Project(SelectedEnemy->GetActorLocation() - FVector(0.0f, 0.0f,
				EnemyCapsule->GetScaledCapsuleHalfHeight()), EnemyCapsuleBottomScreen);
		const float CombinedCapsuleRadii = PlayerCapsule && EnemyCapsule
			? PlayerCapsule->GetScaledCapsuleRadius()
				+ EnemyCapsule->GetScaledCapsuleRadius()
			: 0.0f;
		const float ActorScreenSeparation = bPlayerFramed && bEnemyFramed
			? FVector2D::Distance(PlayerScreen, EnemyScreen)
			: 0.0f;
		const bool bActorsVisiblySeparated = ActorScreenSeparation
			>= MinimumActorScreenSeparationPixels;
		const bool bCapsulesOverlapping = CombinedCapsuleRadii > 0.0f
			&& ActorWorldSeparation < CombinedCapsuleRadii;
		bAllFramesFramed = bAllFramesFramed && bPlayerFramed && bEnemyFramed
			&& bPlayerCapsuleFramed && bEnemyCapsuleFramed
			&& bActorsVisiblySeparated && !bCapsulesOverlapping;

		++RequestedFrames;
		const FString Filename = FString::Printf(TEXT("frame_%04d.png"), RequestedFrames);
		FScreenshotRequest::RequestScreenshot(
			FPaths::Combine(FramesDirectory, Filename), false, false);
		TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
		Frame->SetStringField(TEXT("file"), Filename);
		Frame->SetStringField(TEXT("label"), Label);
		Frame->SetStringField(TEXT("variant"), CurrentVariant().Name(Cases));
		Frame->SetBoolField(TEXT("player_in_view"), bPlayerFramed);
		Frame->SetBoolField(TEXT("attacker_in_view"), bEnemyFramed);
		Frame->SetBoolField(TEXT("player_capsule_framed"), bPlayerCapsuleFramed);
		Frame->SetBoolField(TEXT("attacker_capsule_framed"), bEnemyCapsuleFramed);
		Frame->SetNumberField(TEXT("player_capsule_top_screen_y"),
			PlayerCapsuleTopScreen.Y);
		Frame->SetNumberField(TEXT("player_capsule_bottom_screen_y"),
			PlayerCapsuleBottomScreen.Y);
		Frame->SetNumberField(TEXT("attacker_capsule_top_screen_y"),
			EnemyCapsuleTopScreen.Y);
		Frame->SetNumberField(TEXT("attacker_capsule_bottom_screen_y"),
			EnemyCapsuleBottomScreen.Y);
		Frame->SetNumberField(TEXT("player_screen_x"), PlayerScreen.X);
		Frame->SetNumberField(TEXT("player_screen_y"), PlayerScreen.Y);
		Frame->SetNumberField(TEXT("attacker_screen_x"), EnemyScreen.X);
		Frame->SetNumberField(TEXT("attacker_screen_y"), EnemyScreen.Y);
		Frame->SetNumberField(TEXT("actor_screen_separation_px"), ActorScreenSeparation);
		Frame->SetBoolField(TEXT("actors_visibly_separated"), bActorsVisiblySeparated);
		Frame->SetNumberField(TEXT("actor_world_separation_cm"), ActorWorldSeparation);
		Frame->SetNumberField(TEXT("combined_capsule_radii_cm"), CombinedCapsuleRadii);
		Frame->SetBoolField(TEXT("capsules_overlapping"), bCapsulesOverlapping);
		Frame->SetNumberField(TEXT("viewport_width"), Width);
		Frame->SetNumberField(TEXT("viewport_height"), Height);
		Frames.Add(MakeShared<FJsonValueObject>(Frame));
	}

	bool ConfigureProofCamera()
	{
		if (!FApp::CanEverRender())
		{
			return true;
		}
		APlayerController* Controller = Player.IsValid()
			? Cast<APlayerController>(Player->GetController())
			: nullptr;
		if (!Controller || !World.IsValid() || !SelectedEnemy.IsValid())
		{
			return false;
		}

		if (!ProofCamera.IsValid())
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.ObjectFlags |= RF_Transient;
			SpawnParameters.SpawnCollisionHandlingOverride =
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			ACameraActor* SpawnedCamera = World->SpawnActor<ACameraActor>(
				ACameraActor::StaticClass(), FTransform::Identity, SpawnParameters);
			if (!SpawnedCamera)
			{
				return false;
			}
			OriginalViewTarget = Controller->GetViewTarget();
			ProofCamera = SpawnedCamera;
			UCameraComponent* CameraComponent = SpawnedCamera->GetCameraComponent();
			CameraComponent->SetFieldOfView(ProofCameraFieldOfViewDegrees);
			CameraComponent->bConstrainAspectRatio = false;
			CameraComponent->PostProcessBlendWeight = 1.0f;
			CameraComponent->PostProcessSettings.bOverride_MotionBlurAmount = true;
			CameraComponent->PostProcessSettings.MotionBlurAmount = 0.0f;
			CameraComponent->PostProcessSettings.bOverride_MotionBlurMax = true;
			CameraComponent->PostProcessSettings.MotionBlurMax = 0.0f;
		}

		FVector CombatAxis = SelectedEnemy->GetActorLocation() - Player->GetActorLocation();
		CombatAxis.Z = 0.0f;
		CombatAxis = CombatAxis.GetSafeNormal();
		if (CombatAxis.IsNearlyZero())
		{
			CombatAxis = FVector::ForwardVector;
		}
		const FVector SideAxis = FVector::CrossProduct(FVector::UpVector, CombatAxis)
			.GetSafeNormal();
		const FVector Midpoint = (Player->GetActorLocation()
			+ SelectedEnemy->GetActorLocation()) * 0.5f;
		const FVector FocusLocation = Midpoint;
		const FVector CameraLocation = Midpoint + SideAxis * ProofCameraDistanceCm
			+ FVector(0.0f, 0.0f, 130.0f);
		ProofCamera->SetActorLocationAndRotation(
			CameraLocation,
			(FocusLocation - CameraLocation).Rotation(),
			false, nullptr, ETeleportType::TeleportPhysics);
		Controller->SetViewTarget(ProofCamera.Get());
		if (Controller->PlayerCameraManager)
		{
			Controller->PlayerCameraManager->SetGameCameraCutThisFrame();
		}
		return true;
	}

	void RecordVariant(
		const int32 ResolutionTelemetry,
		const int32 PresentationTelemetry,
		const float PlayerDisplacement,
		const float AttackerDisplacement,
		const float PlayerDamage,
		const bool bFixtureRestored,
		const int32 AlignmentFrameCount,
		const float MaxYawOverBudget,
		const float MaxUnexpectedDisplacement,
		const float MaxAlignmentPelvisFrameDelta)
	{
		const FGateBRunVariant& Variant = CurrentVariant();
		const FGateBNormalCase& Case = CurrentCase();
		const FDefenseDecision& Decision = LastResolution.Decision;
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("name"), Variant.Name(Cases));
		Entry->SetStringField(TEXT("case"), Case.Expected.Name);
		Entry->SetStringField(TEXT("attack"), GetPathNameSafe(Decision.SelectedAttack.Get()));
		Entry->SetBoolField(TEXT("diagnostic_mode"), bDiagnosticMode);
		Entry->SetStringField(TEXT("diagnostic_montage"), DiagnosticMontagePath);
		Entry->SetStringField(TEXT("diagnostic_sequence"), DiagnosticSequencePath);
		Entry->SetStringField(TEXT("diagnostic_section"), DiagnosticSection.ToString());
		Entry->SetBoolField(TEXT("diagnostic_warp_offset_requested"),
			bDiagnosticWarpOffsetRequested);
		Entry->SetNumberField(TEXT("diagnostic_warp_offset_x"),
			AppliedDiagnosticWarpOffset.X);
		Entry->SetNumberField(TEXT("diagnostic_warp_offset_y"),
			AppliedDiagnosticWarpOffset.Y);
		Entry->SetNumberField(TEXT("diagnostic_warp_offset_z"),
			AppliedDiagnosticWarpOffset.Z);
		Entry->SetBoolField(TEXT("diagnostic_max_warp_distance_requested"),
			bDiagnosticMaxWarpDistanceRequested);
		Entry->SetNumberField(TEXT("diagnostic_max_warp_distance_cm"),
			AppliedDiagnosticMaxWarpDistance);
		Entry->SetBoolField(TEXT("diagnostic_active_window_requested"),
			bDiagnosticActiveWindowRequested);
		Entry->SetNumberField(TEXT("diagnostic_active_start_offset_seconds"),
			AppliedDiagnosticActiveStartOffset);
		Entry->SetNumberField(TEXT("diagnostic_active_duration_seconds"),
			AppliedDiagnosticActiveDuration);
		Entry->SetNumberField(TEXT("montage_rate"), Variant.MontageRate);
		Entry->SetNumberField(TEXT("world_dilation"), Variant.WorldDilation);
		Entry->SetNumberField(TEXT("observed_montage_rate"), ObservedMontageRate);
		Entry->SetBoolField(TEXT("defender_montage_observed_active"),
			bDefenderPresentationObservedActive);
		Entry->SetNumberField(TEXT("defender_montage_max_weight"),
			DefenderPresentationMaxWeight);
		Entry->SetNumberField(TEXT("defender_montage_progress_seconds"),
			DefenderPresentationMaxProgress);
		Entry->SetBoolField(TEXT("attacker_montage_required"),
			Case.Presentation.bRequiresAttackerMontage);
		Entry->SetBoolField(TEXT("attacker_montage_observed_active"),
			bAttackerPresentationObservedActive);
		Entry->SetNumberField(TEXT("attacker_montage_max_weight"),
			AttackerPresentationMaxWeight);
		Entry->SetNumberField(TEXT("attacker_montage_progress_seconds"),
			AttackerPresentationMaxProgress);
		Entry->SetBoolField(TEXT("passed"), CurrentCasePassed);
		Entry->SetStringField(TEXT("outcome"), EnumName(
			StaticEnum<EDefenseOutcome>(), static_cast<int64>(Decision.Outcome)));
		Entry->SetStringField(TEXT("reason"), EnumName(
			StaticEnum<EDefenseReason>(), static_cast<int64>(Decision.Reason)));
		Entry->SetStringField(TEXT("height"), EnumName(
			StaticEnum<EAttackHeight>(), static_cast<int64>(Decision.Height)));
		Entry->SetStringField(TEXT("lane"), EnumName(
			StaticEnum<EIncomingAttackLane>(), static_cast<int64>(Decision.Lane)));
		Entry->SetStringField(TEXT("lane_provenance"), EnumName(
			StaticEnum<EDefenseLaneProvenance>(),
			static_cast<int64>(LastResolution.ActualContact.LaneProvenance)));
		Entry->SetStringField(TEXT("swing"), EnumName(
			StaticEnum<ESwingDirection>(), static_cast<int64>(Decision.SwingShape)));
		Entry->SetStringField(TEXT("source_socket"), Decision.SourceSocket.ToString());
		Entry->SetNumberField(TEXT("actual_trajectory_x"),
			LastResolution.ActualContact.IncomingTrajectory.X);
		Entry->SetNumberField(TEXT("actual_trajectory_y"),
			LastResolution.ActualContact.IncomingTrajectory.Y);
		Entry->SetNumberField(TEXT("actual_trajectory_z"),
			LastResolution.ActualContact.IncomingTrajectory.Z);
		Entry->SetBoolField(TEXT("actual_trajectory_rate_normalized"),
			LastResolution.ActualContact.bIncomingTrajectoryRateNormalized);
		Entry->SetNumberField(TEXT("contact_weapon_velocity_x"),
			LastResolution.ActualContact.HitInfo.WeaponVelocity.X);
		Entry->SetNumberField(TEXT("contact_weapon_velocity_y"),
			LastResolution.ActualContact.HitInfo.WeaponVelocity.Y);
		Entry->SetNumberField(TEXT("contact_weapon_velocity_z"),
			LastResolution.ActualContact.HitInfo.WeaponVelocity.Z);
		Entry->SetBoolField(TEXT("source_socket_velocity_usable"),
			bSourceSocketVelocityUsableAtResolution);
		Entry->SetNumberField(TEXT("source_socket_velocity_x"),
			SourceSocketVelocityAtResolution.X);
		Entry->SetNumberField(TEXT("source_socket_velocity_y"),
			SourceSocketVelocityAtResolution.Y);
		Entry->SetNumberField(TEXT("source_socket_velocity_z"),
			SourceSocketVelocityAtResolution.Z);
		Entry->SetStringField(TEXT("source_socket_velocity_lane"), EnumName(
			StaticEnum<EIncomingAttackLane>(),
			static_cast<int64>(SourceSocketLaneAtResolution)));
		Entry->SetStringField(TEXT("source_socket_velocity_lane_provenance"), EnumName(
			StaticEnum<EDefenseLaneProvenance>(),
			static_cast<int64>(SourceSocketLaneProvenanceAtResolution)));
		Entry->SetNumberField(TEXT("source_socket_lane_center_half_angle_degrees"),
			SourceSocketLaneCenterHalfAngleAtResolution);
		Entry->SetBoolField(TEXT("active_window_tip_trajectory_usable"),
			bActiveWindowTipTrajectoryUsableAtResolution);
		Entry->SetNumberField(TEXT("active_window_entry_tip_x"),
			ActiveWindowEntryTipLocation.X);
		Entry->SetNumberField(TEXT("active_window_entry_tip_y"),
			ActiveWindowEntryTipLocation.Y);
		Entry->SetNumberField(TEXT("active_window_entry_tip_z"),
			ActiveWindowEntryTipLocation.Z);
		Entry->SetNumberField(TEXT("active_window_contact_tip_x"),
			LatestTraceTipLocation.X);
		Entry->SetNumberField(TEXT("active_window_contact_tip_y"),
			LatestTraceTipLocation.Y);
		Entry->SetNumberField(TEXT("active_window_contact_tip_z"),
			LatestTraceTipLocation.Z);
		Entry->SetNumberField(TEXT("active_window_tip_trajectory_x"),
			ActiveWindowTipTrajectoryAtResolution.X);
		Entry->SetNumberField(TEXT("active_window_tip_trajectory_y"),
			ActiveWindowTipTrajectoryAtResolution.Y);
		Entry->SetNumberField(TEXT("active_window_tip_trajectory_z"),
			ActiveWindowTipTrajectoryAtResolution.Z);
		Entry->SetStringField(TEXT("active_window_tip_trajectory_lane"), EnumName(
			StaticEnum<EIncomingAttackLane>(),
			static_cast<int64>(ActiveWindowTipLaneAtResolution)));
		Entry->SetStringField(
			TEXT("active_window_tip_trajectory_lane_provenance"), EnumName(
				StaticEnum<EDefenseLaneProvenance>(),
				static_cast<int64>(ActiveWindowTipLaneProvenanceAtResolution)));
		Entry->SetBoolField(TEXT("rate_normalized_tip_trajectory_usable"),
			bRateNormalizedTipTrajectoryUsableAtResolution);
		Entry->SetNumberField(TEXT("rate_normalized_tip_window_animation_seconds"),
			1.0 / 30.0);
		Entry->SetNumberField(TEXT("rate_normalized_tip_trajectory_x"),
			RateNormalizedTipTrajectoryAtResolution.X);
		Entry->SetNumberField(TEXT("rate_normalized_tip_trajectory_y"),
			RateNormalizedTipTrajectoryAtResolution.Y);
		Entry->SetNumberField(TEXT("rate_normalized_tip_trajectory_z"),
			RateNormalizedTipTrajectoryAtResolution.Z);
		Entry->SetStringField(TEXT("rate_normalized_tip_trajectory_lane"), EnumName(
			StaticEnum<EIncomingAttackLane>(),
			static_cast<int64>(RateNormalizedTipLaneAtResolution)));
		Entry->SetStringField(
			TEXT("rate_normalized_tip_trajectory_lane_provenance"), EnumName(
				StaticEnum<EDefenseLaneProvenance>(),
				static_cast<int64>(RateNormalizedTipLaneProvenanceAtResolution)));
		Entry->SetBoolField(TEXT("short_rate_normalized_tip_trajectory_usable"),
			bShortRateNormalizedTipTrajectoryUsableAtResolution);
		Entry->SetNumberField(TEXT("short_rate_normalized_tip_window_animation_seconds"),
			1.0 / 60.0);
		Entry->SetNumberField(TEXT("short_rate_normalized_tip_trajectory_x"),
			ShortRateNormalizedTipTrajectoryAtResolution.X);
		Entry->SetNumberField(TEXT("short_rate_normalized_tip_trajectory_y"),
			ShortRateNormalizedTipTrajectoryAtResolution.Y);
		Entry->SetNumberField(TEXT("short_rate_normalized_tip_trajectory_z"),
			ShortRateNormalizedTipTrajectoryAtResolution.Z);
		Entry->SetStringField(TEXT("short_rate_normalized_tip_trajectory_lane"), EnumName(
			StaticEnum<EIncomingAttackLane>(),
			static_cast<int64>(ShortRateNormalizedTipLaneAtResolution)));
		Entry->SetStringField(
			TEXT("short_rate_normalized_tip_trajectory_lane_provenance"), EnumName(
				StaticEnum<EDefenseLaneProvenance>(),
				static_cast<int64>(ShortRateNormalizedTipLaneProvenanceAtResolution)));
		const FVector TraceTrajectory =
			LastResolution.ActualContact.TraceEnd - LastResolution.ActualContact.TraceStart;
		Entry->SetNumberField(TEXT("trace_trajectory_x"), TraceTrajectory.X);
		Entry->SetNumberField(TEXT("trace_trajectory_y"), TraceTrajectory.Y);
		Entry->SetNumberField(TEXT("trace_trajectory_z"), TraceTrajectory.Z);
		Entry->SetNumberField(TEXT("contact_animation_time"),
			LastResolution.ActualContact.HitInfo.AnimationTime);
		Entry->SetNumberField(TEXT("contact_point_x"), Decision.ContactPoint.X);
		Entry->SetNumberField(TEXT("contact_point_y"), Decision.ContactPoint.Y);
		Entry->SetNumberField(TEXT("contact_point_z"), Decision.ContactPoint.Z);
		Entry->SetNumberField(TEXT("player_start_yaw"),
			PlayerStartTransform.Rotator().Yaw);
		Entry->SetNumberField(TEXT("player_start_x"), PlayerStartTransform.GetLocation().X);
		Entry->SetNumberField(TEXT("player_start_y"), PlayerStartTransform.GetLocation().Y);
		Entry->SetNumberField(TEXT("attacker_start_x"), EnemyStartTransform.GetLocation().X);
		Entry->SetNumberField(TEXT("attacker_start_y"), EnemyStartTransform.GetLocation().Y);
		Entry->SetNumberField(TEXT("attacker_start_yaw"), EnemyStartTransform.Rotator().Yaw);
		Entry->SetNumberField(TEXT("attacker_start_radius_cm"), FVector::Dist2D(
			EnemyStartTransform.GetLocation(), PlayerStartTransform.GetLocation()));
		Entry->SetNumberField(TEXT("player_contact_yaw"), PlayerYawAtResolution);
		Entry->SetNumberField(TEXT("measured_source_yaw"), Decision.MeasuredYawDegrees);
		Entry->SetStringField(TEXT("target_bone"), Decision.TargetBone.ToString());
		Entry->SetBoolField(TEXT("resolution_target_bone_valid"),
			bResolutionTargetBoneValid);
		Entry->SetNumberField(TEXT("target_bone_world_x"),
			ResolutionTargetBoneLocation.X);
		Entry->SetNumberField(TEXT("target_bone_world_y"),
			ResolutionTargetBoneLocation.Y);
		Entry->SetNumberField(TEXT("target_bone_world_z"),
			ResolutionTargetBoneLocation.Z);
		for (const auto& Pair : StandardTargetBoneLocations)
		{
			const FString Prefix = FString::Printf(
				TEXT("standard_bone_%s"), *Pair.Key.ToString().ToLower());
			Entry->SetNumberField(Prefix + TEXT("_world_x"), Pair.Value.X);
			Entry->SetNumberField(Prefix + TEXT("_world_y"), Pair.Value.Y);
			Entry->SetNumberField(Prefix + TEXT("_world_z"), Pair.Value.Z);
			Entry->SetNumberField(Prefix + TEXT("_contact_vertical_delta_cm"),
				FMath::Abs(Decision.ContactPoint.Z - Pair.Value.Z));
		}
		Entry->SetNumberField(TEXT("contact_target_delta_x"), ContactTargetDelta.X);
		Entry->SetNumberField(TEXT("contact_target_delta_y"), ContactTargetDelta.Y);
		Entry->SetNumberField(TEXT("contact_target_delta_z"), ContactTargetDelta.Z);
		Entry->SetNumberField(TEXT("contact_target_vertical_delta_cm"),
			ContactTargetVerticalDeltaCm);
		Entry->SetNumberField(TEXT("max_contact_target_vertical_delta_cm"),
			Case.Presentation.MaxContactTargetVerticalDeltaCm);
		Entry->SetStringField(TEXT("defender_row"), LastResolution.PresentationRow.ToString());
		Entry->SetStringField(TEXT("attacker_row"),
			LastResolution.AttackerPresentationRow.ToString());
		Entry->SetNumberField(TEXT("audio_invocations"), AudioInvocationCount);
		Entry->SetNumberField(TEXT("vfx_invocations"), VFXInvocationCount);
		Entry->SetNumberField(TEXT("resolution_telemetry"), ResolutionTelemetry);
		Entry->SetNumberField(TEXT("presentation_telemetry"), PresentationTelemetry);
		Entry->SetBoolField(TEXT("attack_instance_valid"), Decision.AttackInstance.IsValid());
		Entry->SetBoolField(TEXT("interaction_id_valid"),
			LastResolution.InteractionId.IsValid());
		Entry->SetNumberField(TEXT("locked_threat_stable_id"),
			Decision.LockedThreatId.Value);
		Entry->SetBoolField(TEXT("fixture_restored"), bFixtureRestored);
		Entry->SetNumberField(TEXT("player_damage"), PlayerDamage);
		Entry->SetNumberField(TEXT("player_displacement_cm"), PlayerDisplacement);
		Entry->SetNumberField(TEXT("attacker_displacement_cm"), AttackerDisplacement);
		Entry->SetNumberField(TEXT("maximum_normal_block_displacement_cm"),
			MaximumNormalBlockDisplacementCm);
		Entry->SetNumberField(TEXT("alignment_frame_count"), AlignmentFrameCount);
		Entry->SetNumberField(TEXT("max_yaw_over_budget_degrees"), MaxYawOverBudget);
		Entry->SetNumberField(TEXT("maximum_yaw_over_budget_degrees"),
			MaximumFrameYawOverBudgetDegrees);
		Entry->SetNumberField(TEXT("max_unexpected_frame_displacement_cm"),
			MaxUnexpectedDisplacement);
		Entry->SetNumberField(TEXT("maximum_unexpected_frame_displacement_cm"),
			MaximumUnexpectedFrameDisplacementCm);
		Entry->SetNumberField(TEXT("max_alignment_pelvis_frame_delta_cm"),
			MaxAlignmentPelvisFrameDelta);
		Entry->SetNumberField(TEXT("minimum_blade_to_target_cm"), MinimumBladeToTargetCm);
		Entry->SetNumberField(TEXT("minimum_trace_to_capsule_separation_cm"),
			MinimumTraceToCapsuleSeparationCm);
		Entry->SetNumberField(TEXT("trace_frame_count"), TraceFrameCount);
		Entry->SetNumberField(TEXT("trace_radius_cm"), TraceRadius);
		Entry->SetNumberField(TEXT("closest_blade_delta_x"), ClosestBladeDelta.X);
		Entry->SetNumberField(TEXT("closest_blade_delta_y"), ClosestBladeDelta.Y);
		Entry->SetNumberField(TEXT("closest_blade_delta_z"), ClosestBladeDelta.Z);
		Entry->SetNumberField(TEXT("closest_capsule_delta_x"), ClosestCapsuleDelta.X);
		Entry->SetNumberField(TEXT("closest_capsule_delta_y"), ClosestCapsuleDelta.Y);
		Entry->SetNumberField(TEXT("closest_capsule_delta_z"), ClosestCapsuleDelta.Z);
		VariantEvidence.Add(MakeShared<FJsonValueObject>(Entry));
		bAllVariantsPassed = bAllVariantsPassed && CurrentCasePassed;
	}

	void FinalizeEvidence()
	{
		if (bEvidenceFinalized)
		{
			return;
		}
		bEvidenceFinalized = true;

		FString ResolvedCsvPath;
		FString CsvError;
		const bool bCsvWritten = DefenseTelemetry::WriteCsv(
			FPaths::Combine(EvidenceDirectory, TEXT("defense-gate-b-telemetry.csv")),
			CollectedTelemetry, ResolvedCsvPath, CsvError);
		Test->TestTrue(TEXT("Gate B telemetry CSV is written"), bCsvWritten);
		if (!bCsvWritten)
		{
			Test->AddError(CsvError);
		}

		TArray<FString> RenderedFrames;
		IFileManager::Get().FindFiles(RenderedFrames,
			*FPaths::Combine(FramesDirectory, TEXT("*.png")), true, false);
		int32 DecodedFrames = 0;
		int32 NontrivialFrames = 0;
		for (const TSharedPtr<FJsonValue>& Value : Frames)
		{
			const TSharedPtr<FJsonObject> Frame = Value.IsValid() ? Value->AsObject() : nullptr;
			if (!Frame.IsValid())
			{
				continue;
			}
			FString Filename;
			Frame->TryGetStringField(TEXT("file"), Filename);
			FGateBFrameValidation Validation;
			const bool bLoaded = AnalyzeGateBFrame(
				FPaths::Combine(FramesDirectory, Filename), Validation);
			DecodedFrames += bLoaded && Validation.bDecoded ? 1 : 0;
			NontrivialFrames += Validation.bNontrivial ? 1 : 0;
			Frame->SetBoolField(TEXT("decoded"), bLoaded && Validation.bDecoded);
			Frame->SetBoolField(TEXT("nontrivial"), Validation.bNontrivial);
			Frame->SetNumberField(TEXT("width"), Validation.Width);
			Frame->SetNumberField(TEXT("height"), Validation.Height);
		}
		const bool bVisualCaptureApplicable = FApp::CanEverRender();
		const bool bFramesComplete = bVisualCaptureApplicable
			&& (RequestedFrames > 0
				&& RenderedFrames.Num() == RequestedFrames
				&& DecodedFrames == RequestedFrames
				&& NontrivialFrames == RequestedFrames
				&& bAllFramesFramed);
		if (FApp::CanEverRender() && !bFatalFailure)
		{
			Test->TestTrue(TEXT("All Gate B proof frames are rendered, nontrivial, fully framed, distinguishable, and non-overlapping"),
				bFramesComplete);
		}
		if (!bFatalFailure)
		{
			Test->TestEqual(TEXT("Gate B proof executed every selected variant"),
				VariantEvidence.Num(), Variants.Num());
			Test->TestTrue(TEXT("Every Gate B normal-block variant passed"),
				bAllVariantsPassed);
		}

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 4);
		Root->SetStringField(TEXT("gate"), TEXT("B"));
		Root->SetStringField(TEXT("map"), GateBMapPackage);
		Root->SetStringField(TEXT("manifest"), GateBManifestRelativePath);
		Root->SetBoolField(TEXT("diagnostic_mode"), bDiagnosticMode);
		Root->SetStringField(TEXT("diagnostic_attack"), DiagnosticAttackPath);
		Root->SetStringField(TEXT("diagnostic_montage"), DiagnosticMontagePath);
		Root->SetStringField(TEXT("diagnostic_sequence"), DiagnosticSequencePath);
		Root->SetStringField(TEXT("diagnostic_section"), DiagnosticSection.ToString());
		Root->SetBoolField(TEXT("diagnostic_warp_offset_requested"),
			bDiagnosticWarpOffsetRequested);
		Root->SetNumberField(TEXT("diagnostic_warp_offset_x"), DiagnosticWarpOffset.X);
		Root->SetNumberField(TEXT("diagnostic_warp_offset_y"), DiagnosticWarpOffset.Y);
		Root->SetNumberField(TEXT("diagnostic_warp_offset_z"), DiagnosticWarpOffset.Z);
		Root->SetBoolField(TEXT("diagnostic_max_warp_distance_requested"),
			bDiagnosticMaxWarpDistanceRequested);
		Root->SetNumberField(TEXT("diagnostic_max_warp_distance_cm"),
			DiagnosticMaxWarpDistance);
		Root->SetBoolField(TEXT("diagnostic_active_window_requested"),
			bDiagnosticActiveWindowRequested);
		Root->SetNumberField(TEXT("diagnostic_active_start_offset_seconds"),
			DiagnosticActiveStartOffset);
		Root->SetNumberField(TEXT("diagnostic_active_duration_seconds"),
			DiagnosticActiveDuration);
		Root->SetStringField(TEXT("execution_mode"),
			FApp::CanEverRender() ? TEXT("Rendered") : TEXT("Headless"));
		Root->SetBoolField(TEXT("visual_capture_applicable"),
			bVisualCaptureApplicable);
		Root->SetStringField(TEXT("proof_camera"),
			FApp::CanEverRender() ? TEXT("DetachedSideOn") : TEXT("None"));
		Root->SetNumberField(TEXT("proof_camera_fov_degrees"),
			ProofCameraFieldOfViewDegrees);
		Root->SetNumberField(TEXT("proof_camera_distance_cm"),
			ProofCameraDistanceCm);
		Root->SetNumberField(TEXT("minimum_actor_screen_separation_px"),
			MinimumActorScreenSeparationPixels);
		Root->SetNumberField(TEXT("minimum_frame_edge_margin_px"),
			MinimumFrameEdgeMarginPixels);
		Root->SetBoolField(TEXT("fixed_timestep_applied"), bFixedTimeStepOverridden);
		Root->SetNumberField(TEXT("fixed_timestep_hz"), ProofFixedHz);
		Root->SetBoolField(TEXT("fatal_failure"), bFatalFailure);
		Root->SetBoolField(TEXT("all_variants_passed"),
			!bFatalFailure && bAllVariantsPassed && VariantEvidence.Num() == Variants.Num());
		Root->SetNumberField(TEXT("variant_count"), Variants.Num());
		Root->SetNumberField(TEXT("requested_frames"), RequestedFrames);
		Root->SetNumberField(TEXT("rendered_frames"), RenderedFrames.Num());
		Root->SetNumberField(TEXT("decoded_frames"), DecodedFrames);
		Root->SetNumberField(TEXT("nontrivial_frames"), NontrivialFrames);
		Root->SetBoolField(TEXT("frames_complete"), bFramesComplete);
		Root->SetArrayField(TEXT("variants"), VariantEvidence);
		Root->SetArrayField(TEXT("frames"), Frames);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		Test->TestTrue(TEXT("Gate B structured evidence JSON is written"),
			FFileHelper::SaveStringToFile(
				Json,
				*FPaths::Combine(EvidenceDirectory,
					TEXT("defense-gate-b-evidence.json")),
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM));
	}

	void Fail(const FString& Message)
	{
		if (!bFatalFailure)
		{
			bFatalFailure = true;
			Test->AddError(FString::Printf(TEXT("Gate B %s: %s"), StageName(Stage), *Message));
		}
		if (Stage != EGateBProofStage::Finalize && Stage != EGateBProofStage::Done)
		{
			SetStage(EGateBProofStage::Finalize);
		}
	}

	void Cleanup()
	{
		RestoreDiagnosticAttackOverrides();
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
		OriginalViewTarget.Reset();
		RestoreGlobalTimeDilation();
		if (Director.IsValid())
		{
			Director->ResetFixture();
		}
		for (const TWeakObjectPtr<AEnemyCharacter>& Enemy : Enemies)
		{
			if (Enemy.IsValid() && Enemy->GetCombatComponent())
			{
				Enemy->GetCombatComponent()->SetAttackMontagePlayRateForTesting(1.0f);
			}
		}
		RestoreControllerLogic();
		if (bResolutionBound && PlayerCombat.IsValid())
		{
			PlayerCombat->OnDefenseResolvedNative.RemoveAll(this);
		}
		bResolutionBound = false;
		RemoveTraceBinding();
		if (bAudioBound)
		{
			UCinematicEffectsUtilityLibrary::OnImpactSoundPlaybackInvokedForTesting.RemoveAll(this);
		}
		bAudioBound = false;
		if (bVFXBound)
		{
			UCinematicEffectsUtilityLibrary::OnImpactVFXSpawnInvokedForTesting.RemoveAll(this);
		}
		bVFXBound = false;
		if (bDefenseDebugOverridden)
		{
			if (IConsoleVariable* Debug = IConsoleManager::Get().FindConsoleVariable(
				TEXT("Combat.Defense.Debug")))
			{
				Debug->SetWithCurrentPriority(PreviousDefenseDebug);
			}
		}
		bDefenseDebugOverridden = false;
		RestoreFixedTimeStep();
	}

	void RestoreGlobalTimeDilation()
	{
		if (bGlobalTimeDilationCaptured && World.IsValid())
		{
			UGameplayStatics::SetGlobalTimeDilation(
				World.Get(), PreviousGlobalTimeDilation);
		}
	}

	bool RestoreDiagnosticAttackOverrides()
	{
		if (!bDiagnosticAttackOverrideApplied)
		{
			return true;
		}

		UAttackData* Attack = DiagnosticAttack.Get();
		if (!Attack)
		{
			bDiagnosticAttackOverrideApplied = false;
			return false;
		}

		Attack->WarpConfig.TargetRelativeOffset = OriginalDiagnosticWarpOffset;
		Attack->WarpConfig.MaxWarpDistance = OriginalDiagnosticMaxWarpDistance;
		Attack->ManualTiming = OriginalDiagnosticManualTiming;
		UAnimMontage* Montage = Attack->AttackMontage;
		bool bNotifyTimesRestored = true;
		if (Montage && (!OriginalDiagnosticPhaseNotifyTimes.IsEmpty()
			|| !DiagnosticAddedPhaseNotifyIndices.IsEmpty()
			|| bDiagnosticNotifyTrackAdded))
		{
			for (const TPair<int32, float>& Snapshot : OriginalDiagnosticPhaseNotifyTimes)
			{
				if (!Montage->Notifies.IsValidIndex(Snapshot.Key))
				{
					bNotifyTimesRestored = false;
					continue;
				}
				Montage->Notifies[Snapshot.Key].SetTime(Snapshot.Value);
			}
			DiagnosticAddedPhaseNotifyIndices.Sort(TGreater<int32>());
			for (const int32 AddedIndex : DiagnosticAddedPhaseNotifyIndices)
			{
				if (Montage->Notifies.IsValidIndex(AddedIndex))
				{
					Montage->Notifies.RemoveAt(AddedIndex);
				}
				else
				{
					bNotifyTimesRestored = false;
				}
			}
			if (bDiagnosticNotifyTrackAdded)
			{
				Montage->AnimNotifyTracks.SetNum(OriginalDiagnosticNotifyTrackCount);
			}
			Montage->RefreshCacheData();
		}
		else if (!OriginalDiagnosticPhaseNotifyTimes.IsEmpty())
		{
			bNotifyTimesRestored = false;
		}
		UPackage* DiagnosticPackage = Attack->GetOutermost();
		UPackage* DiagnosticMontagePackage = Montage
			? Montage->GetOutermost() : nullptr;
		if (DiagnosticPackage && !bOriginalDiagnosticPackageDirty)
		{
			DiagnosticPackage->ClearDirtyFlag();
		}
		if (DiagnosticMontagePackage && !bOriginalDiagnosticMontagePackageDirty)
		{
			DiagnosticMontagePackage->ClearDirtyFlag();
		}
		const bool bRestored = Attack->WarpConfig.TargetRelativeOffset.Equals(
			OriginalDiagnosticWarpOffset, KINDA_SMALL_NUMBER)
			&& FMath::IsNearlyEqual(Attack->WarpConfig.MaxWarpDistance,
				OriginalDiagnosticMaxWarpDistance)
			&& FMath::IsNearlyEqual(Attack->ManualTiming.WindupDuration,
				OriginalDiagnosticManualTiming.WindupDuration)
			&& FMath::IsNearlyEqual(Attack->ManualTiming.ActiveDuration,
				OriginalDiagnosticManualTiming.ActiveDuration)
			&& FMath::IsNearlyEqual(Attack->ManualTiming.RecoveryDuration,
				OriginalDiagnosticManualTiming.RecoveryDuration)
			&& bNotifyTimesRestored
			&& Montage
			&& Montage->AnimNotifyTracks.Num() == OriginalDiagnosticNotifyTrackCount
			&& DiagnosticPackage
			&& DiagnosticPackage->IsDirty() == bOriginalDiagnosticPackageDirty
			&& DiagnosticMontagePackage
			&& DiagnosticMontagePackage->IsDirty()
				== bOriginalDiagnosticMontagePackageDirty;
		OriginalDiagnosticPhaseNotifyTimes.Reset();
		DiagnosticAddedPhaseNotifyIndices.Reset();
		bDiagnosticNotifyTrackAdded = false;
		bDiagnosticAttackOverrideApplied = false;
		return bRestored;
	}

	bool ApplyDiagnosticActiveWindow()
	{
		UAttackData* Attack = DiagnosticAttack.Get();
		UAnimMontage* Montage = Attack ? Attack->AttackMontage : nullptr;
		if (!Attack || !Montage)
		{
			Fail(TEXT("Diagnostic active-window override requires a montage"));
			return false;
		}

		float SectionStart = 0.0f;
		float SectionEnd = 0.0f;
		Attack->GetSectionTimeRange(SectionStart, SectionEnd);
		const float ActiveStart = SectionStart + DiagnosticActiveStartOffset;
		const float RecoveryStart = ActiveStart + DiagnosticActiveDuration;
		if (SectionEnd <= SectionStart || RecoveryStart > SectionEnd + KINDA_SMALL_NUMBER)
		{
			Fail(FString::Printf(
				TEXT("Diagnostic active window %.3f..%.3f is outside section %.3f..%.3f"),
				ActiveStart, RecoveryStart, SectionStart, SectionEnd));
			return false;
		}

		int32 ActiveNotifyIndex = INDEX_NONE;
		int32 RecoveryNotifyIndex = INDEX_NONE;
		for (int32 Index = 0; Index < Montage->Notifies.Num(); ++Index)
		{
			FAnimNotifyEvent& Event = Montage->Notifies[Index];
			const float Time = Event.GetTriggerTime();
			if (Time < SectionStart - KINDA_SMALL_NUMBER
				|| Time > SectionEnd + KINDA_SMALL_NUMBER)
			{
				continue;
			}
			const UAnimNotify_AttackPhaseTransition* Transition =
				Cast<UAnimNotify_AttackPhaseTransition>(Event.Notify);
			if (!Transition)
			{
				continue;
			}
			if (Transition->TransitionToPhase == EAttackPhase::Active)
			{
				if (ActiveNotifyIndex != INDEX_NONE)
				{
					Fail(TEXT("Diagnostic section has multiple Active phase transitions"));
					return false;
				}
				ActiveNotifyIndex = Index;
			}
			else if (Transition->TransitionToPhase == EAttackPhase::Recovery)
			{
				if (RecoveryNotifyIndex != INDEX_NONE)
				{
					Fail(TEXT("Diagnostic section has multiple Recovery phase transitions"));
					return false;
				}
				RecoveryNotifyIndex = Index;
			}
		}
		auto AddPhaseTransition = [Montage](
			const EAttackPhase Phase,
			const float Time)
		{
			UAnimNotify_AttackPhaseTransition* Transition =
				NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
			Transition->TransitionToPhase = Phase;
			FAnimNotifyEvent Event;
			Event.Notify = Transition;
			Event.SetTime(Time);
			Event.TriggerTimeOffset = EAnimEventTriggerOffsets::OffsetBefore;
			Event.TrackIndex = 0;
			return Montage->Notifies.Add(MoveTemp(Event));
		};
		if (Montage->AnimNotifyTracks.IsEmpty())
		{
			FAnimNotifyTrack Track;
			Track.TrackName = FName(TEXT("DefenseProof"));
			Montage->AnimNotifyTracks.Add(MoveTemp(Track));
			bDiagnosticNotifyTrackAdded = true;
		}
		if (bDiagnosticMontageOverrideRequested && ActiveNotifyIndex == INDEX_NONE)
		{
			ActiveNotifyIndex = AddPhaseTransition(EAttackPhase::Active, ActiveStart);
			DiagnosticAddedPhaseNotifyIndices.Add(ActiveNotifyIndex);
		}
		if (bDiagnosticMontageOverrideRequested && RecoveryNotifyIndex == INDEX_NONE)
		{
			RecoveryNotifyIndex = AddPhaseTransition(EAttackPhase::Recovery, RecoveryStart);
			DiagnosticAddedPhaseNotifyIndices.Add(RecoveryNotifyIndex);
		}
		if (ActiveNotifyIndex == INDEX_NONE || RecoveryNotifyIndex == INDEX_NONE)
		{
			Fail(TEXT("Diagnostic section requires one Active and one Recovery phase transition"));
			return false;
		}

		OriginalDiagnosticPhaseNotifyTimes.Reset();
		if (!DiagnosticAddedPhaseNotifyIndices.Contains(ActiveNotifyIndex))
		{
			OriginalDiagnosticPhaseNotifyTimes.Add(
				ActiveNotifyIndex, Montage->Notifies[ActiveNotifyIndex].GetTriggerTime());
		}
		if (!DiagnosticAddedPhaseNotifyIndices.Contains(RecoveryNotifyIndex))
		{
			OriginalDiagnosticPhaseNotifyTimes.Add(
				RecoveryNotifyIndex, Montage->Notifies[RecoveryNotifyIndex].GetTriggerTime());
		}
		Montage->Notifies[ActiveNotifyIndex].SetTime(ActiveStart);
		Montage->Notifies[RecoveryNotifyIndex].SetTime(RecoveryStart);
		Montage->RefreshCacheData();
		Attack->ManualTiming.WindupDuration = DiagnosticActiveStartOffset;
		Attack->ManualTiming.ActiveDuration = DiagnosticActiveDuration;
		Attack->ManualTiming.RecoveryDuration = FMath::Max(
			0.0f, SectionEnd - RecoveryStart);
		AppliedDiagnosticActiveStartOffset = DiagnosticActiveStartOffset;
		AppliedDiagnosticActiveDuration = DiagnosticActiveDuration;
		return true;
	}

	void AcquireFixedTimeStep()
	{
		if (bFixedTimeStepOverridden)
		{
			return;
		}

		PreviousUseFixedTimeStep = FApp::UseFixedTimeStep();
		PreviousFixedDeltaTime = FApp::GetFixedDeltaTime();
		FApp::SetFixedDeltaTime(1.0 / static_cast<double>(ProofFixedHz));
		FApp::SetUseFixedTimeStep(true);
		bFixedTimeStepOverridden = true;
	}

	void RestoreFixedTimeStep()
	{
		if (!bFixedTimeStepOverridden)
		{
			return;
		}

		FApp::SetUseFixedTimeStep(PreviousUseFixedTimeStep);
		FApp::SetFixedDeltaTime(PreviousFixedDeltaTime);
		bFixedTimeStepOverridden = false;
	}

	FAutomationTestBase* Test = nullptr;
	TArray<FGateBNormalCase> Cases;
	TArray<FGateBRunVariant> Variants;
	int32 VariantIndex = 0;
	double CommandStart = 0.0;
	double StageStart = 0.0;
	EGateBProofStage Stage = EGateBProofStage::WaitForPIE;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<ADefenseMatrixProofDirector> Director;
	TWeakObjectPtr<UCombatComponent> PlayerCombat;
	TWeakObjectPtr<UCombatTokenSubsystem> TokenSubsystem;
	TWeakObjectPtr<AActor> OriginalViewTarget;
	TWeakObjectPtr<ACameraActor> ProofCamera;
	TArray<TWeakObjectPtr<AEnemyCharacter>> Enemies;
	TArray<FGateBControllerTickState> ControllerTickStates;
	TWeakObjectPtr<AEnemyCharacter> SelectedEnemy;
	TWeakObjectPtr<UEnemyCombatAIComponent> SelectedAI;
	TWeakObjectPtr<UWeaponComponent> BoundTraceWeapon;
	TStrongObjectPtr<UAttackData> DiagnosticAttack;
	TWeakObjectPtr<UAnimMontage> DefenderPresentationMontage;
	TWeakObjectPtr<UAnimMontage> AttackerPresentationMontage;
	FDefenseResolution LastResolution;
	FTransform PlayerStartTransform = FTransform::Identity;
	FTransform EnemyStartTransform = FTransform::Identity;
	float PlayerStartHealth = 0.0f;
	float MinimumBladeToTargetCm = BIG_NUMBER;
	float MinimumTraceToCapsuleSeparationCm = BIG_NUMBER;
	float ProofRadius = 185.0f;
	FVector ProofOffset = FVector::ZeroVector;
	FVector DiagnosticWarpOffset = FVector::ZeroVector;
	FVector OriginalDiagnosticWarpOffset = FVector::ZeroVector;
	FVector AppliedDiagnosticWarpOffset = FVector::ZeroVector;
	float DiagnosticMaxWarpDistance = 0.0f;
	float OriginalDiagnosticMaxWarpDistance = 0.0f;
	float AppliedDiagnosticMaxWarpDistance = 0.0f;
	float DiagnosticActiveStartOffset = 0.0f;
	float DiagnosticActiveDuration = 0.0f;
	float AppliedDiagnosticActiveStartOffset = 0.0f;
	float AppliedDiagnosticActiveDuration = 0.0f;
	FAttackPhaseTimingOverride OriginalDiagnosticManualTiming;
	TMap<int32, float> OriginalDiagnosticPhaseNotifyTimes;
	TArray<int32> DiagnosticAddedPhaseNotifyIndices;
	int32 OriginalDiagnosticNotifyTrackCount = 0;
	float ProofYawOffset = 0.0f;
	float ProofPlayerYawOffset = 0.0f;
	float ProofFixedHz = 60.0f;
	double PreviousFixedDeltaTime = 0.0;
	float PlayerYawAtResolution = 0.0f;
	float ObservedMontageRate = 0.0f;
	float PreviousGlobalTimeDilation = 1.0f;
	float DefenderPresentationStartPosition = -1.0f;
	float AttackerPresentationStartPosition = -1.0f;
	float DefenderPresentationMaxProgress = 0.0f;
	float AttackerPresentationMaxProgress = 0.0f;
	float DefenderPresentationMaxWeight = 0.0f;
	float AttackerPresentationMaxWeight = 0.0f;
	float TraceRadius = 0.0f;
	FVector ClosestBladeDelta = FVector::ZeroVector;
	FVector ClosestCapsuleDelta = FVector::ZeroVector;
	FVector ResolutionTargetBoneLocation = FVector::ZeroVector;
	TMap<FName, FVector> StandardTargetBoneLocations;
	FVector ContactTargetDelta = FVector::ZeroVector;
	FVector SourceSocketVelocityAtResolution = FVector::ZeroVector;
	FVector ActiveWindowEntryTipLocation = FVector::ZeroVector;
	FVector LatestTraceTipLocation = FVector::ZeroVector;
	FVector ActiveWindowTipTrajectoryAtResolution = FVector::ZeroVector;
	FVector RateNormalizedTipTrajectoryAtResolution = FVector::ZeroVector;
	FVector ShortRateNormalizedTipTrajectoryAtResolution = FVector::ZeroVector;
	float ContactTargetVerticalDeltaCm = BIG_NUMBER;
	float SourceSocketLaneCenterHalfAngleAtResolution = 12.0f;
	int32 TraceFrameCount = 0;
	int32 ExpectedAttackGeneration = 0;
	int32 AudioInvocationCount = 0;
	int32 VFXInvocationCount = 0;
	int32 RequestedFrames = 0;
	int32 PreviousDefenseDebug = 0;
	bool bHasResolution = false;
	bool bResolutionTargetBoneValid = false;
	bool bSourceSocketVelocityUsableAtResolution = false;
	bool bHasActiveWindowEntryTipLocation = false;
	bool bActiveWindowTipTrajectoryUsableAtResolution = false;
	bool bRateNormalizedTipTrajectoryUsableAtResolution = false;
	bool bShortRateNormalizedTipTrajectoryUsableAtResolution = false;
	EIncomingAttackLane SourceSocketLaneAtResolution = EIncomingAttackLane::Center;
	EDefenseLaneProvenance SourceSocketLaneProvenanceAtResolution =
		EDefenseLaneProvenance::None;
	EIncomingAttackLane ActiveWindowTipLaneAtResolution = EIncomingAttackLane::Center;
	EDefenseLaneProvenance ActiveWindowTipLaneProvenanceAtResolution =
		EDefenseLaneProvenance::None;
	EIncomingAttackLane RateNormalizedTipLaneAtResolution = EIncomingAttackLane::Center;
	EDefenseLaneProvenance RateNormalizedTipLaneProvenanceAtResolution =
		EDefenseLaneProvenance::None;
	EIncomingAttackLane ShortRateNormalizedTipLaneAtResolution =
		EIncomingAttackLane::Center;
	EDefenseLaneProvenance ShortRateNormalizedTipLaneProvenanceAtResolution =
		EDefenseLaneProvenance::None;
	bool CurrentCasePassed = false;
	bool bAllVariantsPassed = true;
	bool bAllFramesFramed = true;
	bool bFatalFailure = false;
	bool bEvidenceFinalized = false;
	bool bResolutionBound = false;
	bool bAudioBound = false;
	bool bVFXBound = false;
	bool bTraceBound = false;
	bool bDefenseDebugOverridden = false;
	bool PreviousUseFixedTimeStep = false;
	bool bFixedTimeStepOverridden = false;
	bool bGlobalTimeDilationCaptured = false;
	bool bDefenderPresentationObservedActive = false;
	bool bAttackerPresentationObservedActive = false;
	bool bTransformOverrideRequested = false;
	bool bRadiusOverrideRequested = false;
	bool bDefenderYawOverrideRequested = false;
	bool bDiagnosticMode = false;
	bool bDiagnosticWarpOffsetRequested = false;
	bool bDiagnosticWarpOffsetXRequested = false;
	bool bDiagnosticWarpOffsetYRequested = false;
	bool bDiagnosticWarpOffsetZRequested = false;
	bool bDiagnosticMaxWarpDistanceRequested = false;
	bool bDiagnosticActiveStartRequested = false;
	bool bDiagnosticActiveDurationRequested = false;
	bool bDiagnosticActiveWindowRequested = false;
	bool bDiagnosticMontageRequested = false;
	bool bDiagnosticSequenceRequested = false;
	bool bDiagnosticSectionRequested = false;
	bool bDiagnosticMontageOverrideRequested = false;
	bool bDiagnosticAttackOverrideRequested = false;
	bool bDiagnosticAttackOverrideApplied = false;
	bool bDiagnosticNotifyTrackAdded = false;
	bool bOriginalDiagnosticPackageDirty = false;
	bool bOriginalDiagnosticMontagePackageDirty = false;
	FString DiagnosticAttackPath;
	FString DiagnosticMontagePath;
	FString DiagnosticSequencePath;
	FName DiagnosticSection = NAME_None;
	FString EvidenceDirectory;
	FString FramesDirectory;
	TArray<FDefenseTelemetryRecord> CollectedTelemetry;
	TArray<TSharedPtr<FJsonValue>> VariantEvidence;
	TArray<TSharedPtr<FJsonValue>> Frames;
};

bool BuildGateBProofInputs(
	const FDefenseProofManifest& Manifest,
	TArray<FGateBNormalCase>& OutCases,
	TArray<FGateBRunVariant>& OutVariants,
	FString& OutError)
{
	for (const FDefenseProofExpectedCaseEntry& Expected : Manifest.ExpectedCases)
	{
		if (Expected.Outcome != TEXT("NormalBlock"))
		{
			continue;
		}
		const FDefenseProofPresentationEntry* Presentation =
			Manifest.Presentations.FindByPredicate(
				[&Expected](const FDefenseProofPresentationEntry& Candidate)
				{
					return Candidate.Name == Expected.Presentation;
				});
		if (!Presentation)
		{
			OutError = FString::Printf(TEXT("Case '%s' has no presentation '%s'"),
				*Expected.Name, *Expected.Presentation);
			return false;
		}
		if (!Presentation->bHasMaxContactTargetVerticalDeltaCm)
		{
			OutError = FString::Printf(
				TEXT("Case '%s' presentation '%s' has no reviewed contact target vertical tolerance"),
				*Expected.Name, *Expected.Presentation);
			return false;
		}
		const int32 MatchingAttackCount = Manifest.Attacks.FilterByPredicate(
			[&Expected](const FDefenseProofAttackEntry& Candidate)
			{
				return Candidate.Name == Expected.Attack;
			}).Num();
		const FDefenseProofAttackEntry* Attack = Manifest.Attacks.FindByPredicate(
			[&Expected](const FDefenseProofAttackEntry& Candidate)
			{
				return Candidate.Name == Expected.Attack;
			});
		if (MatchingAttackCount != 1 || !Attack || Attack->AttackData.IsEmpty())
		{
			OutError = FString::Printf(
				TEXT("Case '%s' must resolve exactly one attack entry '%s'"),
				*Expected.Name, *Expected.Attack);
			return false;
		}
		FGateBNormalCase Case;
		Case.Expected = Expected;
		Case.Presentation = *Presentation;
		Case.AttackDataPath = Attack->AttackData;
		OutCases.Add(MoveTemp(Case));
	}
	if (OutCases.Num() != 9)
	{
		OutError = FString::Printf(TEXT("Expected nine normal-block cases, found %d"),
			OutCases.Num());
		return false;
	}
	FString RequestedCase;
	if (FParse::Value(FCommandLine::Get(), TEXT("DefenseGateBCase="), RequestedCase))
	{
		OutCases.RemoveAll([&RequestedCase](const FGateBNormalCase& Case)
		{
			return Case.Expected.Name != RequestedCase;
		});
		if (OutCases.IsEmpty())
		{
			OutError = FString::Printf(TEXT("Requested Gate B case '%s' was not found"),
				*RequestedCase);
			return false;
		}
	}

	float RequestedRate = 0.0f;
	const bool bRateFiltered = FParse::Value(
		FCommandLine::Get(), TEXT("DefenseGateBRate="), RequestedRate);
	float RequestedDilation = 0.0f;
	const bool bDilationFiltered = FParse::Value(
		FCommandLine::Get(), TEXT("DefenseGateBDilation="), RequestedDilation);
	const TArray<float> Rates = bRateFiltered
		? TArray<float>{RequestedRate}
		: TArray<float>{0.5f, 1.0f, 2.0f};
	const TArray<float> Dilations = bDilationFiltered
		? TArray<float>{RequestedDilation}
		: TArray<float>{1.0f, 0.5f};
	for (int32 CaseIndex = 0; CaseIndex < OutCases.Num(); ++CaseIndex)
	{
		for (const float Rate : Rates)
		{
			for (const float Dilation : Dilations)
			{
				if (Rate <= 0.0f || Dilation <= 0.0f)
				{
					OutError = TEXT("Montage rates and world dilations must be positive");
					return false;
				}
				FGateBRunVariant Variant;
				Variant.CaseIndex = CaseIndex;
				Variant.MontageRate = Rate;
				Variant.WorldDilation = Dilation;
				OutVariants.Add(Variant);
			}
		}
	}
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseGateBPIEProofTest,
	"KatanaCombat.Defense.GateB.PIEProof",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseGateBPIEProofTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FDefenseProofManifest Manifest;
	TArray<FString> ManifestErrors;
	const FString ManifestPath = FPaths::Combine(
		FPaths::ProjectDir(), GateBManifestRelativePath);
	if (!FDefenseAssetValidationService::LoadManifestFile(
		ManifestPath, Manifest, ManifestErrors))
	{
		for (const FString& Error : ManifestErrors)
		{
			AddError(Error);
		}
		return false;
	}
	if (Manifest.Gate != TEXT("B")
		|| FPackageName::ObjectPathToPackageName(Manifest.Map) != GateBMapPackage)
	{
		AddError(TEXT("Gate B PIE proof constants do not match the canonical manifest"));
		return false;
	}

	TArray<FGateBNormalCase> Cases;
	TArray<FGateBRunVariant> Variants;
	FString InputError;
	if (!BuildGateBProofInputs(Manifest, Cases, Variants, InputError))
	{
		AddError(InputError);
		return false;
	}
	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(GateBMapPackage));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FDefenseGateBPIEProofCommand(
		this, MoveTemp(Cases), MoveTemp(Variants)));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
