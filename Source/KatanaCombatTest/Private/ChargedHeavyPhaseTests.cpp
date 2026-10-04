// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimSequenceBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/CombatComponent.h"
#include "Core/WeaponComponent.h"
#include "Data/AttackConfiguration.h"
#include "Data/AttackData.h"
#include "Data/CombatSettings.h"

namespace
{
/** Explicit charged-heavy montage layout; nothing is read from shipped content. */
struct FChargedHeavyLayout
{
	float AttackActive = 0.0f;
	float AttackHold = 0.0f;
	float AttackRecovery = 0.0f;
	float LoopStart = 0.0f;
	float ReleaseStart = 0.0f;
	float ReleaseActive = 0.0f;
	float ReleaseRecovery = 0.0f;
	float TailStart = 0.0f;
	float BlendOut = 0.0f;
};

void AddMontageSection(UAnimMontage* Montage, const FName Name, const float StartTime)
{
	FCompositeSection Section;
	Section.SectionName = Name;
	Section.Link(Montage, StartTime);
	Section.NextSectionName = NAME_None;
	Montage->CompositeSections.Add(Section);
}

void AddPhaseTransition(UAnimMontage* Montage, const EAttackPhase Phase, const float Time)
{
	UAnimNotify_AttackPhaseTransition* Notify = NewObject<UAnimNotify_AttackPhaseTransition>(Montage);
	Notify->TransitionToPhase = Phase;
	FAnimNotifyEvent Event;
	Event.Notify = Notify;
	Event.Link(Montage, Time);
	Montage->Notifies.Add(Event);
}

void AddHoldWindowStart(UAnimMontage* Montage, const EInputType InputType, const float Time)
{
	UAnimNotify_HoldWindowStart* Notify = NewObject<UAnimNotify_HoldWindowStart>(Montage);
	Notify->InputType = InputType;
	FAnimNotifyEvent Event;
	Event.Notify = Notify;
	Event.Link(Montage, Time);
	Montage->Notifies.Add(Event);
}

/** A playable montage covering Length seconds by looping one compatible clip. */
UAnimMontage* CreateLoopedClipMontage(UAnimSequenceBase* Clip, const float Length, const float BlendOut)
{
	if (!Clip || Clip->GetPlayLength() <= UE_KINDA_SMALL_NUMBER)
	{
		return nullptr;
	}
	const int32 Loops = FMath::CeilToInt(Length / Clip->GetPlayLength());
	UAnimMontage* Montage = UAnimMontage::CreateSlotAnimationAsDynamicMontage(
		Clip, TEXT("DefaultSlot"), 0.0f, BlendOut, 1.0f, Loops, -1.0f, 0.0f);
	if (!Montage)
	{
		return nullptr;
	}
	Montage->BlendOut.SetBlendTime(BlendOut);
	Montage->BlendOutTriggerTime = -1.0f;
	Montage->bEnableAutoBlendOut = true;
	Montage->CompositeSections.Reset();
#if WITH_EDITORONLY_DATA
	if (Montage->AnimNotifyTracks.IsEmpty())
	{
		Montage->AnimNotifyTracks.Add(FAnimNotifyTrack(TEXT("1"), FLinearColor::White));
	}
#endif
	return Montage;
}

struct FChargedHeavyFixture
{
	UWorld* World = nullptr;
	APlayerCharacter* Player = nullptr;
	UCombatComponent* Combat = nullptr;
	UAttackData* Heavy = nullptr;
	UAttackData* Light = nullptr;

	bool Initialize(const FChargedHeavyLayout& Layout)
	{
		World = FCombatTestHelpers::CreateTestWorld();
		Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
		UClass* PlayerClass = LoadClass<ABaseCombatCharacter>(
			nullptr, TEXT("/Game/ProjectFiles/Core/Actors/Character/BP_Player.BP_Player_C"));
		const ABaseCombatCharacter* Defaults = PlayerClass
			? PlayerClass->GetDefaultObject<ABaseCombatCharacter>()
			: nullptr;
		UAnimSequenceBase* Clip = LoadObject<UAnimSequenceBase>(nullptr,
			TEXT("/Game/Assets/Animations/DynamicKatana/AS_Parry_R_Seq.AS_Parry_R_Seq"));
		if (!Player || !Combat || !Defaults || !Defaults->GetMesh() || !Clip)
		{
			return false;
		}
		Player->GetMesh()->SetSkeletalMesh(Defaults->GetMesh()->GetSkeletalMeshAsset());
		Player->GetMesh()->SetAnimInstanceClass(Defaults->GetMesh()->GetAnimClass());
		if (!Player->GetMesh()->GetAnimInstance())
		{
			return false;
		}

		UAnimMontage* HeavyMontage = CreateLoopedClipMontage(
			Clip, Layout.TailStart + Layout.BlendOut, Layout.BlendOut);
		UAnimMontage* LightMontage = CreateLoopedClipMontage(
			Clip, Clip->GetPlayLength(), Layout.BlendOut);
		if (!HeavyMontage || !LightMontage)
		{
			return false;
		}
		AddMontageSection(HeavyMontage, TEXT("Attack"), 0.0f);
		AddMontageSection(HeavyMontage, TEXT("Loop"), Layout.LoopStart);
		AddMontageSection(HeavyMontage, TEXT("Release"), Layout.ReleaseStart);
		AddMontageSection(HeavyMontage, TEXT("Tail"), Layout.TailStart);
		AddPhaseTransition(HeavyMontage, EAttackPhase::Active, Layout.AttackActive);
		AddHoldWindowStart(HeavyMontage, EInputType::HeavyAttack, Layout.AttackHold);
		AddPhaseTransition(HeavyMontage, EAttackPhase::Recovery, Layout.AttackRecovery);
		AddPhaseTransition(HeavyMontage, EAttackPhase::Active, Layout.ReleaseActive);
		AddPhaseTransition(HeavyMontage, EAttackPhase::Recovery, Layout.ReleaseRecovery);
		HeavyMontage->RefreshCacheData();
		AddMontageSection(LightMontage, TEXT("Attack"), 0.0f);
		LightMontage->RefreshCacheData();

		Heavy = FCombatTestHelpers::CreateTestAttack(EAttackType::Heavy);
		Heavy->AttackMontage = HeavyMontage;
		Heavy->MontageSection = TEXT("Attack");
		Heavy->bUseSectionOnly = true;
		Heavy->ChargeLoopSection = TEXT("Loop");
		Heavy->ChargeReleaseSection = TEXT("Release");
		Light = FCombatTestHelpers::CreateTestAttack(EAttackType::Light);
		Light->AttackMontage = LightMontage;
		Light->MontageSection = TEXT("Attack");
		Light->bUseSectionOnly = true;

		UCombatSettings* Settings = FCombatTestHelpers::CreateTestCombatSettings();
		UAttackConfiguration* Attacks = Settings ? Settings->GetAttackConfiguration() : nullptr;
		if (!Attacks)
		{
			return false;
		}
		Attacks->DefaultHeavyAttack = Heavy;
		Attacks->DefaultLightAttack = Light;
		Player->CombatSettings = Settings;
		Combat->CombatSettings = Settings;
		return true;
	}

	/** Tick animation (montage weight, then position) and dispatch notifies before montage events. */
	void Advance(const float Seconds, const float Step) const
	{
		USkeletalMeshComponent* Mesh = Player->GetMesh();
		float Remaining = Seconds;
		while (Remaining > UE_KINDA_SMALL_NUMBER)
		{
			const float Delta = FMath::Min(Step, Remaining);
			Mesh->TickAnimation(Delta, false);
			if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
			{
				AnimInstance->DispatchQueuedAnimEvents();
			}
			Remaining -= Delta;
		}
	}

	FName CurrentSection() const
	{
		UAnimInstance* AnimInstance = Player->GetMesh()->GetAnimInstance();
		return AnimInstance && Heavy
			? AnimInstance->Montage_GetCurrentSection(Heavy->AttackMontage)
			: NAME_None;
	}

	bool IsHitDetectionEnabled() const
	{
		return Player->GetWeaponComponent() && Player->GetWeaponComponent()->IsHitDetectionEnabled();
	}

	void Destroy() const
	{
		FCombatTestHelpers::DestroyTestWorld(World);
	}
};

/**
 * Press and hold Heavy through its hold window, queue a Light during the charge loop, release, and play
 * the release section to its Recovery. Returns false only when the fixture could not run.
 */
bool RunChargedHeavy(
	FAutomationTestBase& Test,
	const FChargedHeavyLayout& Layout,
	const TCHAR* Context)
{
	constexpr float FrameSeconds = 1.0f / 60.0f;
	FChargedHeavyFixture Fixture;
	if (!Fixture.Initialize(Layout))
	{
		Test.AddError(FString::Printf(TEXT("%s: charged-heavy fixture could not be created"), Context));
		Fixture.Destroy();
		return false;
	}

	Fixture.Combat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
	Test.TestEqual(FString::Printf(TEXT("%s: the held Heavy starts the charged attack"), Context),
		Fixture.Combat->GetCurrentAttack(), Fixture.Heavy);
	Fixture.Advance(FMath::Max(Layout.AttackActive, Layout.AttackHold) + 0.05f, FrameSeconds);
	Test.TestEqual(FString::Printf(TEXT("%s: the held button enters the charge loop"), Context),
		Fixture.CurrentSection(), FName(TEXT("Loop")));
	Fixture.Advance(0.2f, FrameSeconds);
	Test.TestEqual(FString::Printf(TEXT("%s: the charge loop is not an Active phase"), Context),
		Fixture.Combat->GetCurrentPhase(), EAttackPhase::Windup);
	Test.TestFalse(FString::Printf(TEXT("%s: weapon hit detection is off through the charge loop"), Context),
		Fixture.IsHitDetectionEnabled());

	Fixture.Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	Test.TestEqual(FString::Printf(TEXT("%s: a Light pressed while charging is queued"), Context),
		Fixture.Combat->GetPendingActionCount(), 1);
	Fixture.Combat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Release);
	Test.TestEqual(FString::Printf(TEXT("%s: release plays the release section"), Context),
		Fixture.CurrentSection(), FName(TEXT("Release")));
	Fixture.Advance(Layout.ReleaseActive - Layout.ReleaseStart + 0.05f, FrameSeconds);
	Test.TestEqual(FString::Printf(TEXT("%s: the release strike is Active"), Context),
		Fixture.Combat->GetCurrentPhase(), EAttackPhase::Active);
	Test.TestTrue(FString::Printf(TEXT("%s: the release strike detects hits"), Context),
		Fixture.IsHitDetectionEnabled());

	Fixture.Advance(Layout.ReleaseRecovery - Layout.ReleaseActive, FrameSeconds);
	Test.TestEqual(FString::Printf(TEXT("%s: the release's Recovery runs the queued Light"), Context),
		Fixture.Combat->GetCurrentAttack(), Fixture.Light);
	Test.TestEqual(FString::Printf(TEXT("%s: no queued input is left behind"), Context),
		Fixture.Combat->GetPendingActionCount(), 0);
	Test.TestFalse(FString::Printf(TEXT("%s: the charged attack's hit detection ended at Recovery"), Context),
		Fixture.IsHitDetectionEnabled());

	Fixture.Destroy();
	return true;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FChargedHeavyHoldAfterActiveTest,
	"KatanaCombat.CombatInput.ChargedHeavy.HoldAfterActiveKeepsPhasesConsistent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChargedHeavyHoldAfterActiveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FChargedHeavyLayout Layout;
	Layout.AttackActive = 0.30f;
	Layout.AttackHold = 0.40f;
	Layout.AttackRecovery = 0.90f;
	Layout.LoopStart = 1.20f;
	Layout.ReleaseStart = 1.60f;
	Layout.ReleaseActive = 1.70f;
	Layout.ReleaseRecovery = 2.20f;
	Layout.TailStart = 2.60f;
	Layout.BlendOut = 0.25f;
	TestTrue(TEXT("The fixture's hold window starts after its Active transition"),
		Layout.AttackHold > Layout.AttackActive);
	return RunChargedHeavy(*this, Layout, TEXT("Hold after Active"));
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FChargedHeavyHoldBeforeActiveTest,
	"KatanaCombat.CombatInput.ChargedHeavy.HoldBeforeActiveKeepsPhasesConsistent",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChargedHeavyHoldBeforeActiveTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	FChargedHeavyLayout Layout;
	Layout.AttackHold = 0.30f;
	Layout.AttackActive = 0.40f;
	Layout.AttackRecovery = 0.90f;
	Layout.LoopStart = 1.20f;
	Layout.ReleaseStart = 1.60f;
	Layout.ReleaseActive = 1.70f;
	Layout.ReleaseRecovery = 2.20f;
	Layout.TailStart = 2.60f;
	Layout.BlendOut = 0.25f;
	TestTrue(TEXT("The fixture's hold window starts before its Active transition"),
		Layout.AttackHold < Layout.AttackActive);
	return RunChargedHeavy(*this, Layout, TEXT("Hold before Active"));
}
