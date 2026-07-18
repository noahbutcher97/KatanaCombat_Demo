// Copyright Epic Games, Inc. All Rights Reserved.

#include "Misc/AutomationTest.h"

#include "AI/EnemyCombatAIComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequenceBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Characters/EnemyCharacter.h"
#include "Characters/PlayerCharacter.h"
#include "Components/ActorComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackData.h"
#include "Data/PairedAnimationData.h"
#include "Defense/DefenseResolver.h"
#include "DrawDebugHelpers.h"
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
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
constexpr TCHAR PreviewMapPackage[] = TEXT("/Game/ProjectFiles/Levels/Lvl_ThirdPerson1");
constexpr TCHAR AttackCatalogRoot[] = TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/");
constexpr float LaneCenterHalfAngle = 12.0f;
constexpr float LaneProofSignedAngle = 30.0f;
constexpr float LaneProofAnchorRadius = 185.0f;
constexpr float ProofFrameRate = 60.0f;
constexpr TCHAR MontageProbePrefix[] = TEXT("MontageProbe|");
constexpr TCHAR SequenceProbePrefix[] = TEXT("SequenceProbe|");

struct FGateBInventorySelection
{
	const TCHAR* ObjectPath = nullptr;
	EAttackHeight IntendedHeight = EAttackHeight::Middle;
};

constexpr FGateBInventorySelection GateBInventorySelections[] = {
	{TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_7.LightAttack_7"),
		EAttackHeight::High},
	{TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_6.LightAttack_6"),
		EAttackHeight::Middle},
	{TEXT("/Game/ProjectFiles/Data/PDA/Attack/AttackData/Light/New/LightAttack_2.LightAttack_2"),
		EAttackHeight::Low}};

const FGateBInventorySelection* FindGateBInventorySelection(const FString& ObjectPath)
{
	for (const FGateBInventorySelection& Selection : GateBInventorySelections)
	{
		if (ObjectPath == Selection.ObjectPath)
		{
			return &Selection;
		}
	}
	return nullptr;
}

struct FActiveTipSample
{
	FString Label;
	float MontagePosition = 0.0f;
	FVector ActorLocalPosition = FVector::ZeroVector;
};

struct FPreviewImageValidation
{
	bool bDecoded = false;
	bool bHasNontrivialPixels = false;
	int32 Width = 0;
	int32 Height = 0;
	int32 ChannelRange = 0;
	float AverageBrightness = 0.0f;
};

bool AnalyzePreviewImage(const FString& Filename, FPreviewImageValidation& OutValidation)
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

FString EnumName(const UEnum* Enum, const int64 Value)
{
	return Enum ? Enum->GetNameStringByValue(Value) : TEXT("Unknown");
}

void SetVectorFields(
	const TSharedRef<FJsonObject>& Object,
	const FString& Prefix,
	const FVector& Value)
{
	Object->SetNumberField(Prefix + TEXT("_x"), Value.X);
	Object->SetNumberField(Prefix + TEXT("_y"), Value.Y);
	Object->SetNumberField(Prefix + TEXT("_z"), Value.Z);
}

bool ResolveSourceAnimation(
	const UAnimMontage* Montage,
	const float MontagePosition,
	FString& OutSlot,
	FString& OutAnimation,
	float& OutAnimationPosition)
{
	OutSlot.Reset();
	OutAnimation.Reset();
	OutAnimationPosition = 0.0f;
	if (!Montage)
	{
		return false;
	}

	for (const FSlotAnimationTrack& Track : Montage->SlotAnimTracks)
	{
		for (const FAnimSegment& Segment : Track.AnimTrack.AnimSegments)
		{
			if (!Segment.IsInRange(MontagePosition))
			{
				continue;
			}

			UAnimSequenceBase* Source = Segment.GetAnimationData(
				MontagePosition, OutAnimationPosition);
			OutSlot = Track.SlotName.ToString();
			OutAnimation = GetPathNameSafe(Source);
			return Source != nullptr;
		}
	}
	return false;
}

TArray<FString> DiscoverPreviewCandidates(
	FAutomationTestBase& Test,
	TArray<FString>& OutExcludedHeavyAttacks)
{
	FAssetRegistryModule& AssetRegistryModule =
		FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AssetDataList;
	AssetRegistryModule.Get().GetAssetsByClass(
		UAttackData::StaticClass()->GetClassPathName(), AssetDataList, true);
	AssetDataList.Sort([](const FAssetData& Left, const FAssetData& Right)
	{
		return Left.GetObjectPathString() < Right.GetObjectPathString();
	});

	TArray<FString> CandidatePaths;
	for (const FAssetData& AssetData : AssetDataList)
	{
		const FString ObjectPath = AssetData.GetObjectPathString();
		if (!ObjectPath.StartsWith(AttackCatalogRoot))
		{
			continue;
		}

		UAttackData* AttackData = Cast<UAttackData>(AssetData.GetAsset());
		if (!AttackData)
		{
			Test.AddError(FString::Printf(
				TEXT("Gate B preview could not load AttackData '%s'"), *ObjectPath));
			continue;
		}
		if (AttackData->AttackType == EAttackType::Heavy)
		{
			OutExcludedHeavyAttacks.Add(ObjectPath);
			continue;
		}
		if (!AttackData->AttackMontage)
		{
			Test.AddWarning(FString::Printf(
				TEXT("Gate B preview excluded '%s' because it has no montage"), *ObjectPath));
			continue;
		}
		CandidatePaths.Add(ObjectPath);
	}
	return CandidatePaths;
}

TArray<FString> BuildMontageProbeCandidates(
	FAutomationTestBase& Test,
	const FString& MontagePath)
{
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
	if (!Montage)
	{
		Test.AddError(FString::Printf(
			TEXT("Gate B montage probe could not load '%s'"), *MontagePath));
		return {};
	}
	TArray<FString> CandidatePaths;
	for (const FCompositeSection& Section : Montage->CompositeSections)
	{
		if (!Section.SectionName.IsNone())
		{
			CandidatePaths.Add(FString::Printf(
				TEXT("%s%s|%s"), MontageProbePrefix,
				*MontagePath, *Section.SectionName.ToString()));
		}
	}
	if (CandidatePaths.IsEmpty())
	{
		Test.AddError(FString::Printf(
			TEXT("Gate B montage probe found no named sections in '%s'"), *MontagePath));
	}
	return CandidatePaths;
}

TArray<FString> BuildSequenceProbeCandidates(
	FAutomationTestBase& Test,
	const FString& SequencePath)
{
	UAnimSequenceBase* Sequence = LoadObject<UAnimSequenceBase>(nullptr, *SequencePath);
	if (!Sequence)
	{
		Test.AddError(FString::Printf(
			TEXT("Gate B sequence probe could not load '%s'"), *SequencePath));
		return {};
	}
	return {FString::Printf(TEXT("%s%s"), SequenceProbePrefix, *SequencePath)};
}

bool ParseMontageProbeCandidate(
	const FString& Candidate,
	FString& OutMontagePath,
	FName& OutSection)
{
	if (!Candidate.StartsWith(MontageProbePrefix))
	{
		return false;
	}
	TArray<FString> Tokens;
	Candidate.ParseIntoArray(Tokens, TEXT("|"), false);
	if (Tokens.Num() != 3 || Tokens[1].IsEmpty() || Tokens[2].IsEmpty())
	{
		return false;
	}
	OutMontagePath = Tokens[1];
	OutSection = FName(*Tokens[2]);
	return true;
}

bool ParseSequenceProbeCandidate(
	const FString& Candidate,
	FString& OutSequencePath)
{
	if (!Candidate.StartsWith(SequenceProbePrefix))
	{
		return false;
	}
	OutSequencePath = Candidate.RightChop(FCString::Strlen(SequenceProbePrefix));
	return !OutSequencePath.IsEmpty();
}

enum class ECatalogPreviewStage : uint8
{
	WaitForPIE,
	SettleFixture,
	PreparePose,
	WaitForPose,
	WaitForScreenshot,
	Finalize,
	Done
};

class FDefenseCatalogPreviewCommand final : public IAutomationLatentCommand
{
public:
	FDefenseCatalogPreviewCommand(
		FAutomationTestBase* InTest,
		TArray<FString> InCandidatePaths,
		TArray<FString> InExcludedHeavyAttacks,
		const bool bInProbeMode,
		const bool bInSequenceProbeMode,
		const float InProbeActiveStartOffset,
		const float InProbeActiveDuration)
		: Test(InTest)
		, CandidatePaths(MoveTemp(InCandidatePaths))
		, ExcludedHeavyAttacks(MoveTemp(InExcludedHeavyAttacks))
		, CommandStart(FPlatformTime::Seconds())
		, StageStart(CommandStart)
	{
		bProbeMode = bInProbeMode;
		bSequenceProbeMode = bInSequenceProbeMode;
		ProbeActiveStartOffset = InProbeActiveStartOffset;
		ProbeActiveDuration = InProbeActiveDuration;
		const FDateTime Now = FDateTime::UtcNow();
		const FString RunId = FString::Printf(
			TEXT("%s-%03d"), *Now.ToString(TEXT("%Y%m%d-%H%M%S")), Now.GetMillisecond());
		EvidenceDirectory = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("DefenseProof/GateB/Inventory"), RunId);
		FramesDirectory = FPaths::Combine(EvidenceDirectory, TEXT("frames"));
	}

	virtual ~FDefenseCatalogPreviewCommand() override
	{
		CleanupFixture();
	}

	virtual bool Update() override
	{
		if (!Test)
		{
			return true;
		}
		if (FPlatformTime::Seconds() - CommandStart > 300.0
			&& Stage != ECatalogPreviewStage::Done)
		{
			Fail(TEXT("Gate B catalog preview exceeded its 300-second watchdog"));
		}
		if (Stage != ECatalogPreviewStage::WaitForPIE
			&& Stage != ECatalogPreviewStage::Finalize
			&& Stage != ECatalogPreviewStage::Done
			&& !World.IsValid())
		{
			Fail(TEXT("PIE world became invalid during Gate B catalog preview"));
		}

		switch (Stage)
		{
		case ECatalogPreviewStage::WaitForPIE: return UpdateWaitForPIE();
		case ECatalogPreviewStage::SettleFixture: return UpdateSettleFixture();
		case ECatalogPreviewStage::PreparePose: return UpdatePreparePose();
		case ECatalogPreviewStage::WaitForPose: return UpdateWaitForPose();
		case ECatalogPreviewStage::WaitForScreenshot: return UpdateWaitForScreenshot();
		case ECatalogPreviewStage::Finalize: return UpdateFinalize();
		case ECatalogPreviewStage::Done: return true;
		default:
			Fail(TEXT("Unknown Gate B catalog preview stage"));
			return false;
		}
	}

private:
	bool UpdateWaitForPIE()
	{
		UWorld* PIEWorld = AutomationCommon::GetAnyGameWorld();
		if (!PIEWorld)
		{
			if (StageElapsed() > 20.0)
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
		if (!FoundPlayer || FoundEnemies.IsEmpty())
		{
			if (StageElapsed() > 20.0)
			{
				Fail(FString::Printf(
					TEXT("Preview map requires a player and enemy (player=%s enemies=%d)"),
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
		PreviewEnemy = FoundEnemies[0];
		BaseLocation = Player->GetActorLocation();
		PreviewLocation = BaseLocation + FVector(185.0f, 0.0f, 0.0f);

		IFileManager::Get().MakeDirectory(*FramesDirectory, true);
		ApplyRenderOverrides();
		ConfigurePlayer();
		for (int32 Index = 0; Index < FoundEnemies.Num(); ++Index)
		{
			ConfigureEnemy(FoundEnemies[Index], Index == 0);
		}
		SetStage(ECatalogPreviewStage::SettleFixture);
		return false;
	}

	bool UpdateSettleFixture()
	{
		if (StageElapsed() < 0.75)
		{
			return false;
		}
		SetStage(ECatalogPreviewStage::PreparePose);
		return false;
	}

	bool UpdatePreparePose()
	{
		if (CandidateIndex >= CandidatePaths.Num())
		{
			SetStage(ECatalogPreviewStage::Finalize);
			return false;
		}

		if (SampleIndex == 0 && !BeginCandidate())
		{
			return false;
		}
		if (!CurrentAttack.IsValid() || !PreviewEnemy.IsValid())
		{
			Fail(TEXT("Gate B preview lost its current attack or preview enemy"));
			return false;
		}

		UAnimMontage* Montage = CurrentAttack->AttackMontage;
		UAnimInstance* AnimInstance = PreviewEnemy->GetMesh()
			? PreviewEnemy->GetMesh()->GetAnimInstance()
			: nullptr;
		if (!Montage || !AnimInstance)
		{
			Fail(FString::Printf(
				TEXT("Preview candidate '%s' has no usable montage or AnimInstance"),
				*CandidatePaths[CandidateIndex]));
			return false;
		}

		PreviewEnemy->SetActorLocationAndRotation(
			PreviewLocation, FRotator(0.0f, 180.0f, 0.0f), false, nullptr,
			ETeleportType::TeleportPhysics);
		AnimInstance->StopAllMontages(0.0f);
		CurrentMontagePosition = CurrentSamplePositions[SampleIndex];
		const float PlayResult = AnimInstance->Montage_Play(
			Montage, 1.0f, EMontagePlayReturnType::MontageLength,
			CurrentMontagePosition, true);
		if (PlayResult <= 0.0f)
		{
			Fail(FString::Printf(
				TEXT("Could not play montage '%s' for preview candidate '%s'"),
				*Montage->GetPathName(), *CandidatePaths[CandidateIndex]));
			return false;
		}
		AnimInstance->Montage_SetPosition(Montage, CurrentMontagePosition);
		AnimInstance->Montage_Pause(Montage);
		SetStage(ECatalogPreviewStage::WaitForPose);
		return false;
	}

	bool UpdateWaitForPose()
	{
		if (StageElapsed() < 0.20 || FScreenshotRequest::IsScreenshotRequested())
		{
			return false;
		}
		if (!PreviewEnemy.IsValid() || !Player.IsValid())
		{
			Fail(TEXT("Preview participants became invalid before capture"));
			return false;
		}

		PreviewEnemy->SetActorLocationAndRotation(
			PreviewLocation, FRotator(0.0f, 180.0f, 0.0f), false, nullptr,
			ETeleportType::TeleportPhysics);
		TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
		const float Fraction = (CurrentMontagePosition - SectionStart)
			/ (SectionEnd - SectionStart);
		Frame->SetNumberField(TEXT("sample_index"), SampleIndex);
		Frame->SetStringField(TEXT("sample_label"), SampleLabels[SampleIndex]);
		Frame->SetNumberField(TEXT("sample_fraction"), Fraction);
		Frame->SetNumberField(TEXT("montage_position"), CurrentMontagePosition);
		Frame->SetNumberField(TEXT("simulation_time"), World->GetTimeSeconds());

		FString SlotName;
		FString SourceAnimation;
		float SourcePosition = 0.0f;
		const bool bSourceResolved = ResolveSourceAnimation(
			CurrentAttack->AttackMontage, CurrentMontagePosition,
			SlotName, SourceAnimation, SourcePosition);
		Frame->SetBoolField(TEXT("source_resolved"), bSourceResolved);
		Frame->SetStringField(TEXT("slot"), SlotName);
		Frame->SetStringField(TEXT("source_animation"), SourceAnimation);
		Frame->SetNumberField(TEXT("source_position"), SourcePosition);

		const bool bSocketsValid = RecordPoseGeometry(Frame);
		AllSamplesHaveWeaponSockets = AllSamplesHaveWeaponSockets && bSocketsValid;
		const bool bFramed = RecordFraming(Frame);
		AllSamplesFramed = AllSamplesFramed && bFramed;

		FlushDebugStrings(World.Get());
		DrawDebugString(
			World.Get(), PreviewLocation + FVector(0.0f, 0.0f, 210.0f),
			FString::Printf(TEXT("%s  %s"),
				*CurrentAttack->GetName(), *SampleLabels[SampleIndex]),
			nullptr, FColor::White, -1.0f, false, 1.0f);

		const FString Filename = FString::Printf(
			TEXT("%02d_%s_%s.png"),
			CandidateIndex + 1,
			*FPaths::MakeValidFileName(CurrentAttack->GetName()),
			*SampleLabels[SampleIndex]);
		PendingScreenshotPath = FPaths::Combine(FramesDirectory, Filename);
		Frame->SetStringField(TEXT("file"), Filename);
		CurrentFrame = Frame;
		CurrentSamples.Add(MakeShared<FJsonValueObject>(Frame));
		++RequestedFrameCount;
		FScreenshotRequest::RequestScreenshot(PendingScreenshotPath, false, false);
		SetStage(ECatalogPreviewStage::WaitForScreenshot);
		return false;
	}

	bool UpdateWaitForScreenshot()
	{
		if (FScreenshotRequest::IsScreenshotRequested()
			|| !IFileManager::Get().FileExists(*PendingScreenshotPath))
		{
			if (StageElapsed() > 8.0)
			{
				Fail(FString::Printf(
					TEXT("Screenshot was not written within 8 seconds: %s"),
					*PendingScreenshotPath));
			}
			return false;
		}

		FPreviewImageValidation Validation;
		const bool bAnalyzed = AnalyzePreviewImage(PendingScreenshotPath, Validation);
		if (CurrentFrame.IsValid())
		{
			CurrentFrame->SetBoolField(TEXT("image_decoded"),
				bAnalyzed && Validation.bDecoded);
			CurrentFrame->SetBoolField(TEXT("nontrivial_pixels"),
				Validation.bHasNontrivialPixels);
			CurrentFrame->SetNumberField(TEXT("image_width"), Validation.Width);
			CurrentFrame->SetNumberField(TEXT("image_height"), Validation.Height);
			CurrentFrame->SetNumberField(TEXT("channel_range"), Validation.ChannelRange);
			CurrentFrame->SetNumberField(TEXT("average_brightness"),
				Validation.AverageBrightness);
		}
		DecodedFrameCount += bAnalyzed && Validation.bDecoded ? 1 : 0;
		NontrivialFrameCount += Validation.bHasNontrivialPixels ? 1 : 0;
		CurrentFrame.Reset();
		PendingScreenshotPath.Reset();

		++SampleIndex;
		if (SampleIndex >= CurrentSamplePositions.Num())
		{
			FinishCandidate();
			++CandidateIndex;
			SampleIndex = 0;
		}
		SetStage(ECatalogPreviewStage::PreparePose);
		return false;
	}

	bool UpdateFinalize()
	{
		FinalizeEvidence();
		CleanupFixture();
		SetStage(ECatalogPreviewStage::Done);
		return false;
	}

	bool BeginCandidate()
	{
		FlushPersistentDebugLines(World.Get());
		FlushDebugStrings(World.Get());
		FString ProbeMontagePath;
		FName ProbeSection = NAME_None;
		const bool bCurrentCandidateIsMontageProbe = ParseMontageProbeCandidate(
			CandidatePaths[CandidateIndex], ProbeMontagePath, ProbeSection);
		FString ProbeSequencePath;
		bCurrentCandidateIsSequenceProbe = ParseSequenceProbeCandidate(
			CandidatePaths[CandidateIndex], ProbeSequencePath);
		bCurrentCandidateIsProbe = bCurrentCandidateIsMontageProbe
			|| bCurrentCandidateIsSequenceProbe;
		if (bCurrentCandidateIsProbe)
		{
			UAnimMontage* ProbeMontage = nullptr;
			if (bCurrentCandidateIsMontageProbe)
			{
				ProbeMontage = LoadObject<UAnimMontage>(nullptr, *ProbeMontagePath);
			}
			else if (UAnimSequenceBase* ProbeSequence =
				LoadObject<UAnimSequenceBase>(nullptr, *ProbeSequencePath))
			{
				ProbeMontage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
					ProbeSequence, FName(TEXT("DefaultSlot")), 0.0f, 0.0f);
			}
			UAttackData* ProbeAttack = NewObject<UAttackData>(
				GetTransientPackage(),
				*FString::Printf(TEXT("GateBProbe_%02d_%s"), CandidateIndex + 1,
					bCurrentCandidateIsSequenceProbe
						? *FPaths::GetBaseFilename(ProbeSequencePath)
						: *ProbeSection.ToString()));
			if (ProbeAttack)
			{
				ProbeAttack->AttackMontage = ProbeMontage;
				ProbeAttack->MontageSection = ProbeSection;
				ProbeAttack->bUseSectionOnly = bCurrentCandidateIsMontageProbe;
			}
			CurrentAttack.Reset(ProbeAttack);
		}
		else
		{
			CurrentAttack.Reset(LoadObject<UAttackData>(
				nullptr, *CandidatePaths[CandidateIndex]));
		}
		if (!CurrentAttack.IsValid() || !CurrentAttack->AttackMontage)
		{
			Fail(FString::Printf(TEXT("Could not load preview candidate '%s'"),
				*CandidatePaths[CandidateIndex]));
			return false;
		}

		CurrentAttack->GetSectionTimeRange(SectionStart, SectionEnd);
		if (!FMath::IsFinite(SectionStart) || !FMath::IsFinite(SectionEnd)
			|| SectionEnd - SectionStart <= KINDA_SMALL_NUMBER)
		{
			Fail(FString::Printf(
				TEXT("Preview candidate '%s' has an invalid section range %.3f..%.3f"),
				*CandidatePaths[CandidateIndex], SectionStart, SectionEnd));
			return false;
		}
		float WindupDuration = 0.0f;
		float ActiveDuration = 0.0f;
		float RecoveryDuration = 0.0f;
		const bool bAuthoredPhaseTiming = CurrentAttack->HasValidNotifyTimingInSection();
		if (bCurrentCandidateIsProbe)
		{
			WindupDuration = ProbeActiveStartOffset;
			ActiveDuration = ProbeActiveDuration;
			RecoveryDuration = FMath::Max(
				0.0f, SectionEnd - SectionStart - WindupDuration - ActiveDuration);
		}
		else
		{
			CurrentAttack->GetEffectiveTiming(
				WindupDuration, ActiveDuration, RecoveryDuration);
		}
		const float ActiveStart = SectionStart + WindupDuration;
		const float ActiveEnd = ActiveStart + ActiveDuration;
		if ((!bAuthoredPhaseTiming && !bCurrentCandidateIsProbe)
			|| WindupDuration <= KINDA_SMALL_NUMBER
			|| ActiveDuration <= KINDA_SMALL_NUMBER
			|| ActiveStart < SectionStart
			|| ActiveEnd > SectionEnd + KINDA_SMALL_NUMBER)
		{
			Fail(FString::Printf(
				TEXT("Preview candidate '%s' lacks valid authored active-window timing (windup=%.3f active=%.3f section=%.3f..%.3f)"),
				*CandidatePaths[CandidateIndex], WindupDuration, ActiveDuration,
				SectionStart, SectionEnd));
			return false;
		}

		const float ActiveInset = FMath::Min(0.01f, ActiveDuration * 0.05f);
		const float RateHalfFrame = FMath::Min(
			0.5f / ProofFrameRate, ActiveDuration - ActiveInset);
		const float RateOneFrame = FMath::Min(
			1.0f / ProofFrameRate, ActiveDuration - ActiveInset);
		const float RateTwoFrame = FMath::Min(
			2.0f / ProofFrameRate, ActiveDuration - ActiveInset);
		CurrentSamplePositions = {
			FMath::Lerp(SectionStart, ActiveStart, 0.5f),
			ActiveStart,
			ActiveStart + RateHalfFrame,
			ActiveStart + RateOneFrame,
			ActiveStart + RateTwoFrame,
			FMath::Lerp(ActiveStart, ActiveEnd, 0.5f),
			ActiveEnd - ActiveInset,
			FMath::Lerp(ActiveEnd, SectionEnd, 0.5f)};

		CurrentCandidate = MakeShared<FJsonObject>();
		CurrentCandidate->SetStringField(TEXT("attack_data"),
			bCurrentCandidateIsProbe
				? CandidatePaths[CandidateIndex] : CurrentAttack->GetPathName());
		CurrentCandidate->SetBoolField(TEXT("montage_probe"),
			bCurrentCandidateIsProbe && !bCurrentCandidateIsSequenceProbe);
		CurrentCandidate->SetBoolField(
			TEXT("sequence_probe"), bCurrentCandidateIsSequenceProbe);
		CurrentCandidate->SetStringField(TEXT("attack_type"), EnumName(
			StaticEnum<EAttackType>(), static_cast<int64>(CurrentAttack->AttackType)));
		CurrentCandidate->SetStringField(TEXT("attack_direction"), EnumName(
			StaticEnum<EAttackDirection>(), static_cast<int64>(CurrentAttack->Direction)));
		CurrentCandidate->SetStringField(TEXT("montage"), CurrentAttack->AttackMontage->GetPathName());
		CurrentCandidate->SetStringField(TEXT("section"), CurrentAttack->MontageSection.ToString());
		CurrentCandidate->SetNumberField(TEXT("section_start"), SectionStart);
		CurrentCandidate->SetNumberField(TEXT("section_end"), SectionEnd);
		CurrentCandidate->SetBoolField(TEXT("authored_phase_timing"), bAuthoredPhaseTiming);
		CurrentCandidate->SetStringField(TEXT("timing_basis"),
			bCurrentCandidateIsProbe ? TEXT("DiagnosticOverride") : TEXT("AuthoredNotifies"));
		CurrentCandidate->SetNumberField(TEXT("windup_duration"), WindupDuration);
		CurrentCandidate->SetNumberField(TEXT("active_duration"), ActiveDuration);
		CurrentCandidate->SetNumberField(TEXT("recovery_duration"), RecoveryDuration);
		CurrentCandidate->SetNumberField(TEXT("active_start"), ActiveStart);
		CurrentCandidate->SetNumberField(TEXT("active_end"), ActiveEnd);
		CurrentCandidate->SetStringField(TEXT("serialized_defense_height"), EnumName(
			StaticEnum<EAttackHeight>(), static_cast<int64>(CurrentAttack->DefenseProfile.Height)));
		CurrentCandidate->SetStringField(TEXT("serialized_nominal_lane"), EnumName(
			StaticEnum<EIncomingAttackLane>(),
			static_cast<int64>(CurrentAttack->DefenseProfile.NominalLane)));
		CurrentCandidate->SetStringField(TEXT("serialized_swing_shape"), EnumName(
			StaticEnum<ESwingDirection>(),
			static_cast<int64>(CurrentAttack->DefenseProfile.SwingShape)));
		CurrentCandidate->SetStringField(TEXT("source_socket_override"),
			CurrentAttack->DefenseProfile.SourceContactSocketOverride.ToString());
		CurrentCandidate->SetStringField(TEXT("target_bone"),
			CurrentAttack->GetDefenseTargetBoneFallback().ToString());
		CurrentCandidate->SetStringField(TEXT("attack_hand"), CurrentAttack->AttackHand.ToString());
		CurrentCandidate->SetStringField(TEXT("attack_tags"), CurrentAttack->AttackTags.ToStringSimple());
		CurrentCandidate->SetStringField(TEXT("counter_data"),
			GetPathNameSafe(CurrentAttack->CounterData.Get()));
		CurrentCandidate->SetStringField(TEXT("finisher_data"),
			GetPathNameSafe(CurrentAttack->FinisherData.Get()));
		CurrentCandidate->SetBoolField(TEXT("gate_b_selected"), false);
		CurrentSamples.Reset();
		CurrentActiveTipSamples.Reset();
		ValidTipSampleCount = 0;
		ValidActiveTipSampleCount = 0;
		MinTipLocal = FVector(FLT_MAX);
		MaxTipLocal = FVector(-FLT_MAX);
		MinActiveTipLocal = FVector(FLT_MAX);
		MaxActiveTipLocal = FVector(-FLT_MAX);
		PreviousTipWorld = FVector::ZeroVector;
		bHasPreviousTip = false;
		return true;
	}

	void FinishCandidate()
	{
		if (!CurrentCandidate.IsValid())
		{
			return;
		}
		const FVector Extent = ValidTipSampleCount > 0
			? MaxTipLocal - MinTipLocal
			: FVector::ZeroVector;
		const FVector ActiveExtent = ValidActiveTipSampleCount > 0
			? MaxActiveTipLocal - MinActiveTipLocal
			: FVector::ZeroVector;
		const bool bNondegenerate = ValidTipSampleCount >= 2 && Extent.Size() >= 5.0f;
		const bool bActiveNondegenerate = ValidActiveTipSampleCount >= 2
			&& ActiveExtent.Size() >= 5.0f;
		if (!bCurrentCandidateIsProbe)
		if (const FGateBInventorySelection* Selection = FindGateBInventorySelection(
			CurrentAttack.IsValid() ? CurrentAttack->GetPathName() : FString()))
		{
			++SelectedAttackCount;
			SelectedHeights.Add(Selection->IntendedHeight);
			CurrentCandidate->SetBoolField(TEXT("gate_b_selected"), true);
			CurrentCandidate->SetStringField(TEXT("intended_defense_height"), EnumName(
				StaticEnum<EAttackHeight>(), static_cast<int64>(Selection->IntendedHeight)));
			CurrentCandidate->SetStringField(
				TEXT("selection_basis"),
				TEXT("render-reviewed body height and strongest sampled Active-window horizontal tip segment"));
			RecordAnchorOnlyLaneResolverFeasibility(*Selection);
		}
		RecordAlignmentAdjustedEntryTrajectories();
		CurrentCandidate->SetNumberField(TEXT("valid_tip_samples"), ValidTipSampleCount);
		CurrentCandidate->SetNumberField(TEXT("valid_active_tip_samples"),
			ValidActiveTipSampleCount);
		SetVectorFields(CurrentCandidate.ToSharedRef(), TEXT("tip_trajectory_extent_cm"), Extent);
		CurrentCandidate->SetNumberField(TEXT("tip_trajectory_range_cm"), Extent.Size());
		CurrentCandidate->SetBoolField(TEXT("trajectory_nondegenerate"), bNondegenerate);
		SetVectorFields(CurrentCandidate.ToSharedRef(),
			TEXT("active_tip_trajectory_extent_cm"), ActiveExtent);
		CurrentCandidate->SetNumberField(TEXT("active_tip_trajectory_range_cm"),
			ActiveExtent.Size());
		CurrentCandidate->SetBoolField(TEXT("active_trajectory_nondegenerate"),
			bActiveNondegenerate);
		CurrentCandidate->SetArrayField(TEXT("samples"), CurrentSamples);
		CandidateRecords.Add(MakeShared<FJsonValueObject>(CurrentCandidate));
		NondegenerateTrajectoryCount += bNondegenerate ? 1 : 0;
		NondegenerateActiveTrajectoryCount += bActiveNondegenerate ? 1 : 0;
		CurrentCandidate.Reset();
		CurrentSamples.Reset();
		CurrentActiveTipSamples.Reset();
		CurrentAttack.Reset();
		bCurrentCandidateIsProbe = false;
		bCurrentCandidateIsSequenceProbe = false;
	}

	void RecordAlignmentAdjustedEntryTrajectories()
	{
		if (!CurrentCandidate.IsValid())
		{
			return;
		}

		const FActiveTipSample* Entry = CurrentActiveTipSamples.FindByPredicate(
			[](const FActiveTipSample& Sample)
			{
				return Sample.Label == TEXT("active_entry");
			});
		struct FRateSample
		{
			float MontageRate;
			const TCHAR* Label;
		};
		const FRateSample RateSamples[] = {
			{0.5f, TEXT("active_60hz_rate050")},
			{1.0f, TEXT("active_60hz_rate100")},
			{2.0f, TEXT("active_60hz_rate200")}};

		TArray<TSharedPtr<FJsonValue>> Cases;
		TSet<EIncomingAttackLane> ResolvedLanes;
		bool bComplete = Entry != nullptr;
		for (const FRateSample& RateSample : RateSamples)
		{
			const FActiveTipSample* End = CurrentActiveTipSamples.FindByPredicate(
				[&RateSample](const FActiveTipSample& Sample)
				{
					return Sample.Label == RateSample.Label;
				});
			if (!Entry || !End)
			{
				bComplete = false;
				continue;
			}

			const FVector ActorLocalTrajectory =
				End->ActorLocalPosition - Entry->ActorLocalPosition;
			const FVector DefenderLocalTrajectory =
				FRotator(0.0f, 180.0f, 0.0f).RotateVector(ActorLocalTrajectory);
			const FDefenseLaneResolution Resolution = FDefenseResolver::ResolveIncomingLane(
				DefenderLocalTrajectory,
				FVector::ZeroVector,
				FVector::ZeroVector,
				EIncomingAttackLane::Center,
				FTransform::Identity,
				LaneCenterHalfAngle);
			const float SignedAngle = FMath::RadiansToDegrees(FMath::Atan2(
				DefenderLocalTrajectory.Y, FMath::Abs(DefenderLocalTrajectory.X)));
			const bool bUsable = ActorLocalTrajectory.SizeSquared2D() >= FMath::Square(1.0f)
				&& Resolution.Provenance == EDefenseLaneProvenance::WeaponVelocity;

			TSharedRef<FJsonObject> Case = MakeShared<FJsonObject>();
			Case->SetNumberField(TEXT("montage_rate"), RateSample.MontageRate);
			Case->SetStringField(TEXT("segment_start"), Entry->Label);
			Case->SetStringField(TEXT("segment_end"), End->Label);
			Case->SetNumberField(TEXT("montage_delta_seconds"),
				End->MontagePosition - Entry->MontagePosition);
			Case->SetNumberField(TEXT("signed_angle_degrees"), SignedAngle);
			Case->SetNumberField(TEXT("horizontal_distance_cm"),
				ActorLocalTrajectory.Size2D());
			Case->SetStringField(TEXT("resolved_lane"), EnumName(
				StaticEnum<EIncomingAttackLane>(), static_cast<int64>(Resolution.Lane)));
			Case->SetStringField(TEXT("provenance"), EnumName(
				StaticEnum<EDefenseLaneProvenance>(),
				static_cast<int64>(Resolution.Provenance)));
			Case->SetBoolField(TEXT("usable"), bUsable);
			SetVectorFields(Case, TEXT("trajectory_attacker_local"), ActorLocalTrajectory);
			SetVectorFields(Case, TEXT("trajectory_defender_local"), DefenderLocalTrajectory);
			Cases.Add(MakeShared<FJsonValueObject>(Case));
			ResolvedLanes.Add(Resolution.Lane);
			bComplete = bComplete && bUsable;
			++AlignmentAdjustedEntryCaseCount;
			AlignmentAdjustedEntryUsableCount += bUsable ? 1 : 0;
		}

		const bool bRateStable = bComplete && ResolvedLanes.Num() == 1;
		CurrentCandidate->SetArrayField(
			TEXT("alignment_adjusted_entry_trajectory_cases"), Cases);
		CurrentCandidate->SetBoolField(
			TEXT("alignment_adjusted_entry_trajectory_complete"), bComplete);
		CurrentCandidate->SetBoolField(
			TEXT("alignment_adjusted_entry_lane_rate_stable"), bRateStable);
		CurrentCandidate->SetStringField(
			TEXT("alignment_adjusted_entry_lane"),
			bRateStable
				? EnumName(StaticEnum<EIncomingAttackLane>(),
					static_cast<int64>(*ResolvedLanes.CreateConstIterator()))
				: TEXT("Mixed"));
		AlignmentAdjustedEntryCompleteCount += bComplete ? 1 : 0;
		AlignmentAdjustedEntryRateStableCount += bRateStable ? 1 : 0;
	}

	void RecordAnchorOnlyLaneResolverFeasibility(const FGateBInventorySelection& Selection)
	{
		if (!CurrentCandidate.IsValid() || CurrentActiveTipSamples.Num() < 2)
		{
			return;
		}

		int32 BestSegmentIndex = INDEX_NONE;
		float BestSegmentSizeSquared = 0.0f;
		for (int32 Index = 0; Index + 1 < CurrentActiveTipSamples.Num(); ++Index)
		{
			const FVector Segment = CurrentActiveTipSamples[Index + 1].ActorLocalPosition
				- CurrentActiveTipSamples[Index].ActorLocalPosition;
			const float SegmentSizeSquared = Segment.SizeSquared2D();
			if (SegmentSizeSquared > BestSegmentSizeSquared)
			{
				BestSegmentSizeSquared = SegmentSizeSquared;
				BestSegmentIndex = Index;
			}
		}

		if (BestSegmentIndex == INDEX_NONE || BestSegmentSizeSquared < FMath::Square(5.0f))
		{
			CurrentCandidate->SetBoolField(
				TEXT("anchor_only_lane_resolver_complete"), false);
			return;
		}

		const FActiveTipSample& SegmentStart = CurrentActiveTipSamples[BestSegmentIndex];
		const FActiveTipSample& SegmentEnd = CurrentActiveTipSamples[BestSegmentIndex + 1];
		const FVector LocalTrajectory = SegmentEnd.ActorLocalPosition - SegmentStart.ActorLocalPosition;
		const float LocalTrajectoryAngle = FMath::RadiansToDegrees(
			FMath::Atan2(LocalTrajectory.Y, LocalTrajectory.X));
		CurrentCandidate->SetStringField(
			TEXT("anchor_only_segment_start"), SegmentStart.Label);
		CurrentCandidate->SetStringField(
			TEXT("anchor_only_segment_end"), SegmentEnd.Label);
		SetVectorFields(CurrentCandidate.ToSharedRef(),
			TEXT("anchor_only_trajectory_actor_local"),
			LocalTrajectory);
		CurrentCandidate->SetNumberField(
			TEXT("anchor_only_trajectory_horizontal_cm"), FMath::Sqrt(BestSegmentSizeSquared));
		CurrentCandidate->SetNumberField(
			TEXT("anchor_only_lane_center_half_angle_degrees"), LaneCenterHalfAngle);

		struct FLaneCase
		{
			EIncomingAttackLane ExpectedLane;
			float TargetSignedAngle;
		};
		const FLaneCase LaneCases[] = {
			{EIncomingAttackLane::Left, -LaneProofSignedAngle},
			{EIncomingAttackLane::Center, 0.0f},
			{EIncomingAttackLane::Right, LaneProofSignedAngle}};

		TArray<TSharedPtr<FJsonValue>> Cases;
		int32 CandidatePassCount = 0;
		for (const FLaneCase& LaneCase : LaneCases)
		{
			const float DesiredWorldAngle = 180.0f - LaneCase.TargetSignedAngle;
			const float ActorYaw = FRotator::NormalizeAxis(
				DesiredWorldAngle - LocalTrajectoryAngle);
			const float AnchorBearing = FRotator::NormalizeAxis(ActorYaw - 180.0f);
			const float AnchorRadians = FMath::DegreesToRadians(AnchorBearing);
			const FVector AnchorLocation(
				FMath::Cos(AnchorRadians) * LaneProofAnchorRadius,
				FMath::Sin(AnchorRadians) * LaneProofAnchorRadius,
				0.0f);
			const FRotator ActorRotation(0.0f, ActorYaw, 0.0f);
			const FVector WorldTrajectory = ActorRotation.RotateVector(LocalTrajectory);
			const float ActualSignedAngle = FMath::RadiansToDegrees(FMath::Atan2(
				WorldTrajectory.Y, FMath::Abs(WorldTrajectory.X)));
			const float FacingDot = FVector::DotProduct(
				ActorRotation.Vector().GetSafeNormal2D(),
				(-AnchorLocation).GetSafeNormal2D());
			const FDefenseLaneResolution Resolution = FDefenseResolver::ResolveIncomingLane(
				WorldTrajectory,
				FVector::ZeroVector,
				FVector::ZeroVector,
				EIncomingAttackLane::Center,
				FTransform::Identity,
				LaneCenterHalfAngle);
			const bool bPassed = Resolution.Lane == LaneCase.ExpectedLane
				&& Resolution.Provenance == EDefenseLaneProvenance::WeaponVelocity
				&& FacingDot >= 0.999f;

			TSharedRef<FJsonObject> Case = MakeShared<FJsonObject>();
			Case->SetStringField(TEXT("expected_lane"), EnumName(
				StaticEnum<EIncomingAttackLane>(), static_cast<int64>(LaneCase.ExpectedLane)));
			Case->SetStringField(TEXT("resolved_lane"), EnumName(
				StaticEnum<EIncomingAttackLane>(), static_cast<int64>(Resolution.Lane)));
			Case->SetStringField(TEXT("provenance"), EnumName(
				StaticEnum<EDefenseLaneProvenance>(), static_cast<int64>(Resolution.Provenance)));
			Case->SetNumberField(TEXT("target_signed_angle_degrees"),
				LaneCase.TargetSignedAngle);
			Case->SetNumberField(TEXT("actual_signed_angle_degrees"), ActualSignedAngle);
			Case->SetNumberField(TEXT("actor_yaw_degrees"), ActorYaw);
			Case->SetNumberField(TEXT("anchor_bearing_degrees"), AnchorBearing);
			Case->SetNumberField(TEXT("inward_facing_dot"), FacingDot);
			Case->SetBoolField(TEXT("passed"), bPassed);
			SetVectorFields(Case, TEXT("anchor_defender_local"), AnchorLocation);
			SetVectorFields(Case, TEXT("weapon_velocity_defender_local"), WorldTrajectory);
			Cases.Add(MakeShared<FJsonValueObject>(Case));
			++AnchorOnlyLaneResolverCaseCount;
			CandidatePassCount += bPassed ? 1 : 0;
			AnchorOnlyLaneResolverPassCount += bPassed ? 1 : 0;
		}

		CurrentCandidate->SetStringField(TEXT("intended_defense_height"), EnumName(
			StaticEnum<EAttackHeight>(), static_cast<int64>(Selection.IntendedHeight)));
		CurrentCandidate->SetArrayField(TEXT("anchor_only_lane_resolver_cases"), Cases);
		CurrentCandidate->SetBoolField(
			TEXT("anchor_only_lane_resolver_complete"),
			CandidatePassCount == UE_ARRAY_COUNT(LaneCases));
		CurrentCandidate->SetBoolField(
			TEXT("anchor_only_contact_acceptance_proven"), false);
		CurrentCandidate->SetBoolField(TEXT("contact_acceptance_proven"), false);
	}

	bool RecordPoseGeometry(const TSharedRef<FJsonObject>& Frame)
	{
		UWeaponComponent* Weapon = PreviewEnemy.IsValid()
			? PreviewEnemy->WeaponComponent.Get()
			: nullptr;
		if (!Weapon)
		{
			Frame->SetBoolField(TEXT("weapon_sockets_valid"), false);
			return false;
		}

		const FName StartSocket = Weapon->GetEffectiveStartSocketName();
		const FName EndSocket = Weapon->GetEffectiveEndSocketName();
		FVector StartWorld = FVector::ZeroVector;
		FVector EndWorld = FVector::ZeroVector;
		const bool bStartValid = Weapon->TryGetSocketLocation(StartSocket, StartWorld);
		const bool bEndValid = Weapon->TryGetSocketLocation(EndSocket, EndWorld);
		Frame->SetStringField(TEXT("weapon_start_socket"), StartSocket.ToString());
		Frame->SetStringField(TEXT("weapon_end_socket"), EndSocket.ToString());
		Frame->SetBoolField(TEXT("weapon_start_socket_valid"), bStartValid);
		Frame->SetBoolField(TEXT("weapon_end_socket_valid"), bEndValid);
		Frame->SetBoolField(TEXT("weapon_sockets_valid"), bStartValid && bEndValid);
		if (!bStartValid || !bEndValid)
		{
			return false;
		}

		const FTransform ActorTransform = PreviewEnemy->GetActorTransform();
		const FVector StartLocal = ActorTransform.InverseTransformPosition(StartWorld);
		const FVector EndLocal = ActorTransform.InverseTransformPosition(EndWorld);
		SetVectorFields(Frame, TEXT("weapon_start_world"), StartWorld);
		SetVectorFields(Frame, TEXT("weapon_end_world"), EndWorld);
		SetVectorFields(Frame, TEXT("weapon_start_actor_local"), StartLocal);
		SetVectorFields(Frame, TEXT("weapon_end_actor_local"), EndLocal);

		if (USkeletalMeshComponent* Mesh = PreviewEnemy->GetMesh())
		{
			for (const FName Bone : {FName(TEXT("pelvis")), FName(TEXT("spine_03")), FName(TEXT("head"))})
			{
				if (Mesh->GetBoneIndex(Bone) != INDEX_NONE)
				{
					SetVectorFields(Frame,
						FString::Printf(TEXT("bone_%s_actor_local"), *Bone.ToString()),
						ActorTransform.InverseTransformPosition(Mesh->GetBoneLocation(Bone)));
				}
			}
		}

		MinTipLocal.X = FMath::Min(MinTipLocal.X, EndLocal.X);
		MinTipLocal.Y = FMath::Min(MinTipLocal.Y, EndLocal.Y);
		MinTipLocal.Z = FMath::Min(MinTipLocal.Z, EndLocal.Z);
		MaxTipLocal.X = FMath::Max(MaxTipLocal.X, EndLocal.X);
		MaxTipLocal.Y = FMath::Max(MaxTipLocal.Y, EndLocal.Y);
		MaxTipLocal.Z = FMath::Max(MaxTipLocal.Z, EndLocal.Z);
		++ValidTipSampleCount;
		if (SampleLabels.IsValidIndex(SampleIndex)
			&& SampleLabels[SampleIndex].StartsWith(TEXT("active_")))
		{
			CurrentActiveTipSamples.Add(
				{SampleLabels[SampleIndex], CurrentMontagePosition, EndLocal});
			MinActiveTipLocal.X = FMath::Min(MinActiveTipLocal.X, EndLocal.X);
			MinActiveTipLocal.Y = FMath::Min(MinActiveTipLocal.Y, EndLocal.Y);
			MinActiveTipLocal.Z = FMath::Min(MinActiveTipLocal.Z, EndLocal.Z);
			MaxActiveTipLocal.X = FMath::Max(MaxActiveTipLocal.X, EndLocal.X);
			MaxActiveTipLocal.Y = FMath::Max(MaxActiveTipLocal.Y, EndLocal.Y);
			MaxActiveTipLocal.Z = FMath::Max(MaxActiveTipLocal.Z, EndLocal.Z);
			++ValidActiveTipSampleCount;
		}

		DrawDebugLine(World.Get(), StartWorld, EndWorld, FColor::Yellow, true, -1.0f, 0, 2.0f);
		DrawDebugSphere(World.Get(), EndWorld, 6.0f, 12, FColor::Red, true, -1.0f, 0, 1.5f);
		if (bHasPreviousTip)
		{
			DrawDebugLine(
				World.Get(), PreviousTipWorld, EndWorld, FColor::Cyan, true, -1.0f, 0, 2.5f);
		}
		PreviousTipWorld = EndWorld;
		bHasPreviousTip = true;
		return true;
	}

	bool RecordFraming(const TSharedRef<FJsonObject>& Frame) const
	{
		APlayerController* PlayerController = Player.IsValid()
			? Cast<APlayerController>(Player->GetController())
			: nullptr;
		USkeletalMeshComponent* Mesh = PreviewEnemy.IsValid()
			? PreviewEnemy->GetMesh()
			: nullptr;
		int32 Width = 0;
		int32 Height = 0;
		if (PlayerController)
		{
			PlayerController->GetViewportSize(Width, Height);
		}
		const auto ProjectInView = [PlayerController, Width, Height](
			const FVector& WorldLocation,
			FVector2D& OutScreen)
		{
			return PlayerController && Width > 0 && Height > 0
				&& PlayerController->ProjectWorldLocationToScreen(
					WorldLocation, OutScreen, true)
				&& OutScreen.X >= 2.0f && OutScreen.X <= Width - 2.0f
				&& OutScreen.Y >= 2.0f && OutScreen.Y <= Height - 2.0f;
		};
		FVector2D Screen = FVector2D::ZeroVector;
		FVector2D TopScreen = FVector2D::ZeroVector;
		FVector2D BottomScreen = FVector2D::ZeroVector;
		const FVector BoundsOrigin = Mesh ? Mesh->Bounds.Origin : FVector::ZeroVector;
		const float BoundsHalfHeight = Mesh ? Mesh->Bounds.BoxExtent.Z : 0.0f;
		const bool bCenterInView = Mesh && ProjectInView(BoundsOrigin, Screen);
		const bool bTopInView = Mesh && ProjectInView(
			BoundsOrigin + FVector(0.0f, 0.0f, BoundsHalfHeight), TopScreen);
		const bool bBottomInView = Mesh && ProjectInView(
			BoundsOrigin - FVector(0.0f, 0.0f, BoundsHalfHeight), BottomScreen);
		const bool bInView = bCenterInView && bTopInView && bBottomInView;
		Frame->SetNumberField(TEXT("viewport_width"), Width);
		Frame->SetNumberField(TEXT("viewport_height"), Height);
		Frame->SetNumberField(TEXT("attacker_screen_x"), Screen.X);
		Frame->SetNumberField(TEXT("attacker_screen_y"), Screen.Y);
		Frame->SetNumberField(TEXT("attacker_top_screen_x"), TopScreen.X);
		Frame->SetNumberField(TEXT("attacker_top_screen_y"), TopScreen.Y);
		Frame->SetNumberField(TEXT("attacker_bottom_screen_x"), BottomScreen.X);
		Frame->SetNumberField(TEXT("attacker_bottom_screen_y"), BottomScreen.Y);
		Frame->SetBoolField(TEXT("attacker_in_view"), bInView);
		const bool bCameraOwnerHidden = Player.IsValid() && Player->IsHidden();
		Frame->SetBoolField(TEXT("camera_owner_hidden"), bCameraOwnerHidden);
		return bInView && bCameraOwnerHidden;
	}

	void ConfigurePlayer() const
	{
		if (!Player.IsValid())
		{
			return;
		}
		ResetCharacterCombat(Player.Get());
		Player->SetActorHiddenInGame(true);
		Player->SetActorEnableCollision(false);
		Player->SetActorLocationAndRotation(
			BaseLocation, FRotator::ZeroRotator, false, nullptr,
			ETeleportType::TeleportPhysics);
		if (AController* Controller = Player->GetController())
		{
			Controller->SetControlRotation(FRotator::ZeroRotator);
		}
	}

	void ConfigureEnemy(AEnemyCharacter* Enemy, const bool bPreview) const
	{
		if (!Enemy)
		{
			return;
		}
		if (UEnemyCombatAIComponent* AI = Enemy->CombatAIComponent.Get())
		{
			if (AI->IsAttacking())
			{
				AI->OnDamaged();
			}
			AI->SetCombatTarget(nullptr);
			AI->SetComponentTickEnabled(false);
		}
		if (AController* Controller = Enemy->GetController())
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
		ResetCharacterCombat(Enemy);
		Enemy->SetActorEnableCollision(false);
		Enemy->SetActorHiddenInGame(!bPreview);
		if (bPreview)
		{
			Enemy->SetActorLocationAndRotation(
				PreviewLocation, FRotator(0.0f, 180.0f, 0.0f), false, nullptr,
				ETeleportType::TeleportPhysics);
		}
		else
		{
			Enemy->SetActorLocation(BaseLocation + FVector(2500.0f, 2500.0f, 0.0f));
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

	void FinalizeEvidence()
	{
		if (bEvidenceFinalized)
		{
			return;
		}
		bEvidenceFinalized = true;
		if (CurrentCandidate.IsValid())
		{
			FinishCandidate();
		}

		const int32 ExpectedFrameCount = CandidatePaths.Num() * SampleLabels.Num();
		TArray<TSharedPtr<FJsonValue>> SampleScheme;
		for (const FString& Label : SampleLabels)
		{
			SampleScheme.Add(MakeShared<FJsonValueString>(Label));
		}
		TArray<TSharedPtr<FJsonValue>> Excluded;
		for (const FString& Path : ExcludedHeavyAttacks)
		{
			Excluded.Add(MakeShared<FJsonValueString>(Path));
		}

		TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
		Root->SetNumberField(TEXT("schema_version"), 2);
		Root->SetStringField(TEXT("purpose"), bSequenceProbeMode
			? TEXT("Gate B read-only animation sequence probe")
			: bProbeMode
				? TEXT("Gate B read-only montage section probe")
				: TEXT("Gate B read-only attack catalog preview"));
		Root->SetBoolField(TEXT("montage_probe_mode"),
			bProbeMode && !bSequenceProbeMode);
		Root->SetBoolField(TEXT("sequence_probe_mode"), bSequenceProbeMode);
		Root->SetNumberField(TEXT("probe_active_start_offset"), ProbeActiveStartOffset);
		Root->SetNumberField(TEXT("probe_active_duration"), ProbeActiveDuration);
		Root->SetStringField(TEXT("map"), PreviewMapPackage);
		Root->SetStringField(TEXT("catalog_root"), AttackCatalogRoot);
		Root->SetStringField(TEXT("generated_utc"), FDateTime::UtcNow().ToIso8601());
		Root->SetBoolField(TEXT("fatal_failure"), bFatalFailure);
		Root->SetNumberField(TEXT("candidate_count"), CandidatePaths.Num());
		Root->SetNumberField(TEXT("excluded_heavy_count"), ExcludedHeavyAttacks.Num());
		Root->SetArrayField(TEXT("excluded_heavy_attacks"), Excluded);
		Root->SetArrayField(TEXT("sample_scheme"), SampleScheme);
		Root->SetNumberField(TEXT("expected_frames"), ExpectedFrameCount);
		Root->SetNumberField(TEXT("requested_frames"), RequestedFrameCount);
		Root->SetNumberField(TEXT("decoded_frames"), DecodedFrameCount);
		Root->SetNumberField(TEXT("nontrivial_pixel_frames"), NontrivialFrameCount);
		Root->SetNumberField(TEXT("nondegenerate_trajectories"),
			NondegenerateTrajectoryCount);
		Root->SetNumberField(TEXT("nondegenerate_active_trajectories"),
			NondegenerateActiveTrajectoryCount);
		Root->SetNumberField(TEXT("selected_attack_count"), SelectedAttackCount);
		Root->SetNumberField(TEXT("selected_height_count"), SelectedHeights.Num());
		Root->SetNumberField(TEXT("anchor_only_lane_resolver_case_count"),
			AnchorOnlyLaneResolverCaseCount);
		Root->SetNumberField(TEXT("anchor_only_lane_resolver_pass_count"),
			AnchorOnlyLaneResolverPassCount);
		Root->SetNumberField(TEXT("alignment_adjusted_entry_case_count"),
			AlignmentAdjustedEntryCaseCount);
		Root->SetNumberField(TEXT("alignment_adjusted_entry_usable_count"),
			AlignmentAdjustedEntryUsableCount);
		Root->SetNumberField(TEXT("alignment_adjusted_entry_complete_candidate_count"),
			AlignmentAdjustedEntryCompleteCount);
		Root->SetNumberField(TEXT("alignment_adjusted_entry_rate_stable_candidate_count"),
			AlignmentAdjustedEntryRateStableCount);
		Root->SetBoolField(TEXT("contact_acceptance_proven"), false);
		Root->SetBoolField(TEXT("asset_mutation_authorized"), false);
		Root->SetNumberField(TEXT("package_save_calls"), 0);
		Root->SetBoolField(TEXT("all_samples_framed"), AllSamplesFramed);
		Root->SetBoolField(TEXT("all_samples_have_weapon_sockets"),
			AllSamplesHaveWeaponSockets);
		Root->SetBoolField(TEXT("packages_saved"), false);
		Root->SetArrayField(TEXT("candidates"), CandidateRecords);

		FString Json;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
		FJsonSerializer::Serialize(Root, Writer);
		const FString LedgerPath = FPaths::Combine(
			EvidenceDirectory, TEXT("defense-gate-b-catalog-preview.json"));
		const bool bLedgerWritten = FFileHelper::SaveStringToFile(
			Json, *LedgerPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		const FString LatestPath = FPaths::Combine(
			FPaths::ProjectSavedDir(), TEXT("DefenseProof/GateB/Inventory/latest.txt"));
		const bool bLatestWritten = FFileHelper::SaveStringToFile(
			EvidenceDirectory, *LatestPath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);

		Test->TestTrue(TEXT("Gate B catalog preview ledger is written"), bLedgerWritten);
		Test->TestTrue(TEXT("Gate B catalog preview latest pointer is written"), bLatestWritten);
		if (!bFatalFailure)
		{
			Test->TestEqual(TEXT("Every catalog pose requested one screenshot"),
				RequestedFrameCount, ExpectedFrameCount);
			Test->TestEqual(TEXT("Every catalog screenshot decoded"),
				DecodedFrameCount, ExpectedFrameCount);
			Test->TestEqual(TEXT("Every catalog screenshot has nontrivial pixels"),
				NontrivialFrameCount, ExpectedFrameCount);
			Test->TestEqual(TEXT("Every catalog candidate has a nondegenerate weapon-tip trajectory"),
				NondegenerateTrajectoryCount, CandidatePaths.Num());
			Test->TestEqual(TEXT("Every catalog candidate moves its weapon tip during the authored Active window"),
				NondegenerateActiveTrajectoryCount, CandidatePaths.Num());
			Test->TestTrue(TEXT("Every catalog pose keeps the attacker in frame"),
				AllSamplesFramed);
			Test->TestTrue(TEXT("Every catalog pose resolves exact weapon sockets"),
				AllSamplesHaveWeaponSockets);
			if (!bProbeMode)
			{
				Test->TestEqual(TEXT("Reviewed Gate B inventory selects exactly three attacks"),
					SelectedAttackCount, 3);
				Test->TestEqual(TEXT("Reviewed Gate B inventory covers High, Middle, and Low"),
					SelectedHeights.Num(), 3);
				Test->TestEqual(TEXT("Selected attacks produce all nine anchor-only resolver cases"),
					AnchorOnlyLaneResolverCaseCount, 9);
				Test->TestEqual(TEXT("Every selected anchor-only resolver case resolves as expected"),
					AnchorOnlyLaneResolverPassCount, AnchorOnlyLaneResolverCaseCount);
			}
			Test->TestEqual(TEXT("Every candidate records three alignment-adjusted entry trajectories"),
				AlignmentAdjustedEntryCaseCount, CandidatePaths.Num() * 3);
			Test->TestEqual(TEXT("Every alignment-adjusted entry trajectory is usable"),
				AlignmentAdjustedEntryUsableCount, AlignmentAdjustedEntryCaseCount);
			Test->TestEqual(TEXT("Every candidate completes alignment-adjusted entry analysis"),
				AlignmentAdjustedEntryCompleteCount, CandidatePaths.Num());
		}
	}

	void CleanupFixture()
	{
		if (World.IsValid())
		{
			FlushPersistentDebugLines(World.Get());
			FlushDebugStrings(World.Get());
		}
		if (PreviewEnemy.IsValid() && PreviewEnemy->GetMesh()
			&& PreviewEnemy->GetMesh()->GetAnimInstance())
		{
			PreviewEnemy->GetMesh()->GetAnimInstance()->StopAllMontages(0.0f);
		}
		RestoreRenderOverrides();
	}

	void ApplyRenderOverrides()
	{
		OverrideConsoleVariable(
			TEXT("r.MotionBlurQuality"), 0,
			PreviousMotionBlurQuality, bMotionBlurOverridden);
		OverrideConsoleVariable(
			TEXT("r.DefaultFeature.MotionBlur"), 0,
			PreviousDefaultMotionBlur, bDefaultMotionBlurOverridden);
		OverrideConsoleVariable(
			TEXT("r.AntiAliasingMethod"), 1,
			PreviousAntiAliasingMethod, bAntiAliasingOverridden);
	}

	void OverrideConsoleVariable(
		const TCHAR* Name,
		const int32 Value,
		int32& OutPrevious,
		bool& bOutOverridden)
	{
		if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			OutPrevious = Variable->GetInt();
			Variable->SetWithCurrentPriority(Value);
			bOutOverridden = true;
		}
	}

	void RestoreRenderOverrides()
	{
		RestoreConsoleVariable(
			TEXT("r.MotionBlurQuality"), PreviousMotionBlurQuality, bMotionBlurOverridden);
		RestoreConsoleVariable(
			TEXT("r.DefaultFeature.MotionBlur"), PreviousDefaultMotionBlur,
			bDefaultMotionBlurOverridden);
		RestoreConsoleVariable(
			TEXT("r.AntiAliasingMethod"), PreviousAntiAliasingMethod,
			bAntiAliasingOverridden);
	}

	void RestoreConsoleVariable(
		const TCHAR* Name,
		const int32 Previous,
		bool& bOverridden)
	{
		if (bOverridden)
		{
			if (IConsoleVariable* Variable = IConsoleManager::Get().FindConsoleVariable(Name))
			{
				Variable->SetWithCurrentPriority(Previous);
			}
		}
		bOverridden = false;
	}

	void Fail(const FString& Message)
	{
		if (!bFatalFailure)
		{
			Test->AddError(Message);
			bFatalFailure = true;
		}
		Stage = ECatalogPreviewStage::Finalize;
		StageStart = FPlatformTime::Seconds();
	}

	void SetStage(const ECatalogPreviewStage NewStage)
	{
		Stage = NewStage;
		StageStart = FPlatformTime::Seconds();
	}

	double StageElapsed() const
	{
		return FPlatformTime::Seconds() - StageStart;
	}

	FAutomationTestBase* Test = nullptr;
	TArray<FString> CandidatePaths;
	TArray<FString> ExcludedHeavyAttacks;
	const TArray<FString> SampleLabels = {
		TEXT("windup_mid"),
		TEXT("active_entry"),
		TEXT("active_60hz_rate050"),
		TEXT("active_60hz_rate100"),
		TEXT("active_60hz_rate200"),
		TEXT("active_mid"),
		TEXT("active_end"),
		TEXT("recovery_mid")};
	TArray<float> CurrentSamplePositions;
	TArray<FActiveTipSample> CurrentActiveTipSamples;
	double CommandStart = 0.0;
	double StageStart = 0.0;
	ECatalogPreviewStage Stage = ECatalogPreviewStage::WaitForPIE;
	TWeakObjectPtr<UWorld> World;
	TWeakObjectPtr<APlayerCharacter> Player;
	TWeakObjectPtr<AEnemyCharacter> PreviewEnemy;
	TStrongObjectPtr<UAttackData> CurrentAttack;
	FVector BaseLocation = FVector::ZeroVector;
	FVector PreviewLocation = FVector::ZeroVector;
	FVector MinTipLocal = FVector::ZeroVector;
	FVector MaxTipLocal = FVector::ZeroVector;
	FVector MinActiveTipLocal = FVector::ZeroVector;
	FVector MaxActiveTipLocal = FVector::ZeroVector;
	FVector PreviousTipWorld = FVector::ZeroVector;
	float SectionStart = 0.0f;
	float SectionEnd = 0.0f;
	float CurrentMontagePosition = 0.0f;
	int32 CandidateIndex = 0;
	int32 SampleIndex = 0;
	int32 RequestedFrameCount = 0;
	int32 DecodedFrameCount = 0;
	int32 NontrivialFrameCount = 0;
	int32 NondegenerateTrajectoryCount = 0;
	int32 NondegenerateActiveTrajectoryCount = 0;
	int32 ValidTipSampleCount = 0;
	int32 ValidActiveTipSampleCount = 0;
	int32 SelectedAttackCount = 0;
	int32 AnchorOnlyLaneResolverCaseCount = 0;
	int32 AnchorOnlyLaneResolverPassCount = 0;
	int32 AlignmentAdjustedEntryCaseCount = 0;
	int32 AlignmentAdjustedEntryUsableCount = 0;
	int32 AlignmentAdjustedEntryCompleteCount = 0;
	int32 AlignmentAdjustedEntryRateStableCount = 0;
	TSet<EAttackHeight> SelectedHeights;
	int32 PreviousMotionBlurQuality = 0;
	int32 PreviousDefaultMotionBlur = 0;
	int32 PreviousAntiAliasingMethod = 0;
	bool bHasPreviousTip = false;
	bool bFatalFailure = false;
	bool bEvidenceFinalized = false;
	bool AllSamplesFramed = true;
	bool AllSamplesHaveWeaponSockets = true;
	bool bMotionBlurOverridden = false;
	bool bDefaultMotionBlurOverridden = false;
	bool bAntiAliasingOverridden = false;
	bool bProbeMode = false;
	bool bSequenceProbeMode = false;
	bool bCurrentCandidateIsProbe = false;
	bool bCurrentCandidateIsSequenceProbe = false;
	float ProbeActiveStartOffset = 0.30f;
	float ProbeActiveDuration = 0.20f;
	FString EvidenceDirectory;
	FString FramesDirectory;
	FString PendingScreenshotPath;
	TSharedPtr<FJsonObject> CurrentCandidate;
	TSharedPtr<FJsonObject> CurrentFrame;
	TArray<TSharedPtr<FJsonValue>> CurrentSamples;
	TArray<TSharedPtr<FJsonValue>> CandidateRecords;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseCatalogPreviewTest,
	"KatanaCombat.Defense.GateB.CatalogPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseCatalogPreviewTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	TArray<FString> ExcludedHeavyAttacks;
	FString ProbeMontagePath;
	const bool bMontageProbeMode = FParse::Value(
		FCommandLine::Get(), TEXT("DefenseGateBProbeMontage="), ProbeMontagePath);
	FString ProbeSequencePath;
	const bool bSequenceProbeMode = FParse::Value(
		FCommandLine::Get(), TEXT("DefenseGateBProbeSequence="), ProbeSequencePath);
	const bool bProbeMode = bMontageProbeMode || bSequenceProbeMode;
	float ProbeActiveStartOffset = 0.30f;
	float ProbeActiveDuration = 0.20f;
	FParse::Value(FCommandLine::Get(), TEXT("DefenseGateBProbeActiveStart="),
		ProbeActiveStartOffset);
	FParse::Value(FCommandLine::Get(), TEXT("DefenseGateBProbeActiveDuration="),
		ProbeActiveDuration);
	TestFalse(TEXT("Gate B preview accepts only one probe source"),
		bMontageProbeMode && bSequenceProbeMode);
	TArray<FString> CandidatePaths = bMontageProbeMode
		? BuildMontageProbeCandidates(*this, ProbeMontagePath)
		: bSequenceProbeMode
			? BuildSequenceProbeCandidates(*this, ProbeSequencePath)
			: DiscoverPreviewCandidates(*this, ExcludedHeavyAttacks);
	if (bProbeMode)
	{
		TestTrue(TEXT("Gate B montage probe start is finite and non-negative"),
			FMath::IsFinite(ProbeActiveStartOffset) && ProbeActiveStartOffset >= 0.0f);
		TestTrue(TEXT("Gate B montage probe duration is finite and positive"),
			FMath::IsFinite(ProbeActiveDuration) && ProbeActiveDuration > 0.0f);
		TestTrue(TEXT("Gate B asset probe discovers at least one candidate"),
			!CandidatePaths.IsEmpty());
	}
	else
	{
		TestTrue(TEXT("Gate B catalog has at least three non-heavy montage candidates"),
			CandidatePaths.Num() >= 3);
	}
	if (HasAnyErrors() || CandidatePaths.IsEmpty()
		|| (!bProbeMode && CandidatePaths.Num() < 3))
	{
		return false;
	}
	if (!FApp::CanEverRender())
	{
		AddInfo(FString::Printf(
			TEXT("Gate B rendered catalog preview skipped under headless RHI (%d candidates, %d excluded heavy attacks)"),
			CandidatePaths.Num(), ExcludedHeavyAttacks.Num()));
		return true;
	}

	ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(PreviewMapPackage));
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
	ADD_LATENT_AUTOMATION_COMMAND(FDefenseCatalogPreviewCommand(
		this, MoveTemp(CandidatePaths), MoveTemp(ExcludedHeavyAttacks),
		bProbeMode, bSequenceProbeMode,
		ProbeActiveStartOffset, ProbeActiveDuration));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	return true;
}
