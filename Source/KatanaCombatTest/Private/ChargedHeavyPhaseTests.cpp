// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatTestHelpers.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotify_AttackPhaseTransition.h"
#include "Animation/AnimNotify_HoldWindowStart.h"
#include "Animation/AnimNotifyState_ParryWindow.h"
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
	/** Optional attacker parry window in the attack section; negative leaves it out. */
	float AttackParryStart = -1.0f;
	float AttackParryEnd = -1.0f;
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

void AddParryWindow(UAnimMontage* Montage, const float StartTime, const float EndTime)
{
	UAnimNotifyState_ParryWindow* State = NewObject<UAnimNotifyState_ParryWindow>(Montage);
	FAnimNotifyEvent Event;
	Event.NotifyStateClass = State;
	Event.Link(Montage, StartTime);
	Event.SetDuration(EndTime - StartTime);
	Event.EndLink.Link(Montage, EndTime);
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
		if (Layout.AttackParryStart >= 0.0f)
		{
			AddParryWindow(HeavyMontage, Layout.AttackParryStart, Layout.AttackParryEnd);
		}
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

	float MontagePosition() const
	{
		UAnimInstance* AnimInstance = Player->GetMesh()->GetAnimInstance();
		return AnimInstance && Heavy ? AnimInstance->Montage_GetPosition(Heavy->AttackMontage) : 0.0f;
	}

	/** Advance at Step until the heavy montage reaches MontageTime. */
	void AdvanceTo(const float MontageTime, const float Step) const
	{
		Advance(MontageTime - MontagePosition(), Step);
	}

	/** The attack notifies the last animation tick queued, in queue order (for example "Hold,Active"). */
	FString DescribeQueuedNotifies() const
	{
		TArray<FString> Names;
		UAnimInstance* AnimInstance = Player->GetMesh()->GetAnimInstance();
		if (!AnimInstance)
		{
			return FString();
		}
		for (const FAnimNotifyEventReference& Reference : AnimInstance->NotifyQueue.AnimNotifies)
		{
			const FAnimNotifyEvent* Event = Reference.GetNotify();
			if (!Event)
			{
				continue;
			}
			if (Cast<UAnimNotify_HoldWindowStart>(Event->Notify))
			{
				Names.Add(TEXT("Hold"));
			}
			else if (const UAnimNotify_AttackPhaseTransition* Transition =
				Cast<UAnimNotify_AttackPhaseTransition>(Event->Notify))
			{
				Names.Add(Transition->TransitionToPhase == EAttackPhase::Active
					? TEXT("Active")
					: (Transition->TransitionToPhase == EAttackPhase::Recovery ? TEXT("Recovery") : TEXT("Phase")));
			}
			else if (Cast<UAnimNotifyState_ParryWindow>(Event->NotifyStateClass))
			{
				Names.Add(TEXT("ParryWindow"));
			}
		}
		return FString::Join(Names, TEXT(","));
	}

	/**
	 * One animation tick from the current position to MontageTime, as a slow frame or a hitch delivers it:
	 * every notify crossed is queued before any is dispatched. Returns what that tick queued.
	 */
	FString TickOnceTo(const float MontageTime, float& OutTickSeconds) const
	{
		USkeletalMeshComponent* Mesh = Player->GetMesh();
		OutTickSeconds = MontageTime - MontagePosition();
		Mesh->TickAnimation(OutTickSeconds, false);
		const FString Queued = DescribeQueuedNotifies();
		if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
		{
			AnimInstance->DispatchQueuedAnimEvents();
		}
		return Queued;
	}

	bool HasPublishedWindow(const EAttackWindowKind Kind) const
	{
		return Combat->GetActiveAttackWindow(Kind).IsValid();
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

/** How the attack section's notifies reach the component before the charge loop. */
struct FChargedHeavyDelivery
{
	/**
	 * When positive, advance at 60 Hz to OneTickFrom, then carry the montage to OneTickThrough in a single
	 * animation tick, so every attack-section notify between them is queued before any is dispatched.
	 */
	float OneTickFrom = 0.0f;
	float OneTickThrough = 0.0f;
	/** What that single tick must queue, in queue order; proves the events share one dispatch. */
	const TCHAR* ExpectedOneTickQueue = TEXT("");
	/** Press and release Light during Windup, so a Recovery would run it, instead of pressing it mid-charge. */
	bool bQueueLightInWindup = false;
};

/**
 * Press and hold Heavy through its hold window, queue a Light, release, and play the release section to its
 * Recovery. Returns false only when the fixture could not run.
 */
bool RunChargedHeavy(
	FAutomationTestBase& Test,
	const FChargedHeavyLayout& Layout,
	const TCHAR* Context,
	const FChargedHeavyDelivery& Delivery = {})
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
	if (Delivery.bQueueLightInWindup)
	{
		Fixture.Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
		Fixture.Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Release);
		Test.TestEqual(FString::Printf(TEXT("%s: a Light tapped in Windup waits in the queue"), Context),
			Fixture.Combat->GetPendingActionCount(), 1);
	}

	if (Delivery.OneTickThrough > 0.0f)
	{
		Fixture.AdvanceTo(Delivery.OneTickFrom, FrameSeconds);
		Test.TestEqual(FString::Printf(TEXT("%s: the attack is still in Windup before the slow tick"), Context),
			Fixture.Combat->GetCurrentPhase(), EAttackPhase::Windup);
		float TickSeconds = 0.0f;
		const FString Queued = Fixture.TickOnceTo(Delivery.OneTickThrough, TickSeconds);
		Test.AddInfo(FString::Printf(TEXT("%s: one %.1f ms animation tick queued [%s]"),
			Context, TickSeconds * 1000.0f, *Queued));
		Test.TestEqual(FString::Printf(TEXT("%s: one animation tick queues these notifies together"), Context),
			Queued, FString(Delivery.ExpectedOneTickQueue));
		Test.TestEqual(FString::Printf(TEXT("%s: the charge loop is not an Active phase right after that tick"), Context),
			Fixture.Combat->GetCurrentPhase(), EAttackPhase::Windup);
		Test.TestFalse(FString::Printf(TEXT("%s: weapon hit detection is off right after that tick"), Context),
			Fixture.IsHitDetectionEnabled());
		Test.TestFalse(FString::Printf(TEXT("%s: no hit window is published for the section the charge left"), Context),
			Fixture.HasPublishedWindow(EAttackWindowKind::Hit));
		Test.TestFalse(FString::Printf(TEXT("%s: no parry window is published for the section the charge left"), Context),
			Fixture.HasPublishedWindow(EAttackWindowKind::Parry));
		Test.TestEqual(FString::Printf(TEXT("%s: the charge keeps the Heavy as the current attack"), Context),
			Fixture.Combat->GetCurrentAttack(), Fixture.Heavy);
	}
	else
	{
		Fixture.Advance(FMath::Max(Layout.AttackActive, Layout.AttackHold) + 0.05f, FrameSeconds);
	}
	Test.TestEqual(FString::Printf(TEXT("%s: the held button enters the charge loop"), Context),
		Fixture.CurrentSection(), FName(TEXT("Loop")));
	Fixture.Advance(0.2f, FrameSeconds);
	Test.TestEqual(FString::Printf(TEXT("%s: the charge loop is not an Active phase"), Context),
		Fixture.Combat->GetCurrentPhase(), EAttackPhase::Windup);
	Test.TestFalse(FString::Printf(TEXT("%s: weapon hit detection is off through the charge loop"), Context),
		Fixture.IsHitDetectionEnabled());
	Test.TestEqual(FString::Printf(TEXT("%s: the charge loop keeps playing"), Context),
		Fixture.CurrentSection(), FName(TEXT("Loop")));

	if (!Delivery.bQueueLightInWindup)
	{
		Fixture.Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	}
	Test.TestEqual(FString::Printf(TEXT("%s: one Light waits in the queue while charging"), Context),
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

namespace
{
/** A charged heavy whose release section strikes on its own Active and Recovery; attack timings vary per test. */
FChargedHeavyLayout MakeOneTickLayout(const float AttackHold, const float AttackActive, const float AttackRecovery)
{
	FChargedHeavyLayout Layout;
	Layout.AttackHold = AttackHold;
	Layout.AttackActive = AttackActive;
	Layout.AttackRecovery = AttackRecovery;
	Layout.LoopStart = 1.20f;
	Layout.ReleaseStart = 1.60f;
	Layout.ReleaseActive = 1.70f;
	Layout.ReleaseRecovery = 2.20f;
	Layout.TailStart = 2.60f;
	Layout.BlendOut = 0.25f;
	return Layout;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FChargedHeavyHoldThenActiveInOneTickTest,
	"KatanaCombat.CombatInput.ChargedHeavy.HoldThenActiveInOneTickStaysOutOfActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChargedHeavyHoldThenActiveInOneTickTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// The hold leads Active by less than one 30 FPS frame, and a parry window opens between them, so a single
	// slow tick queues all three before the hold's jump to the charge loop is dispatched.
	FChargedHeavyLayout Layout = MakeOneTickLayout(0.28f, 0.30f, 0.90f);
	Layout.AttackParryStart = 0.29f;
	Layout.AttackParryEnd = 0.60f;
	FChargedHeavyDelivery Delivery;
	Delivery.OneTickFrom = 0.275f;
	Delivery.OneTickThrough = 0.305f;
	Delivery.ExpectedOneTickQueue = TEXT("Hold,ParryWindow,Active");
	TestTrue(TEXT("The slow tick is no longer than one 30 FPS frame"),
		Delivery.OneTickThrough - Delivery.OneTickFrom <= 1.0f / 30.0f);
	TestTrue(TEXT("The fixture's hold leads Active by less than that tick"),
		Layout.AttackHold < Layout.AttackActive
		&& Layout.AttackHold > Delivery.OneTickFrom
		&& Layout.AttackActive < Delivery.OneTickThrough);
	return RunChargedHeavy(*this, Layout, TEXT("Hold then Active in one tick"), Delivery);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FChargedHeavyActiveThenHoldInOneTickTest,
	"KatanaCombat.CombatInput.ChargedHeavy.ActiveThenHoldInOneTickStaysOutOfActive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChargedHeavyActiveThenHoldInOneTickTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const FChargedHeavyLayout Layout = MakeOneTickLayout(0.30f, 0.28f, 0.90f);
	FChargedHeavyDelivery Delivery;
	Delivery.OneTickFrom = 0.275f;
	Delivery.OneTickThrough = 0.305f;
	Delivery.ExpectedOneTickQueue = TEXT("Active,Hold");
	TestTrue(TEXT("The slow tick is no longer than one 30 FPS frame"),
		Delivery.OneTickThrough - Delivery.OneTickFrom <= 1.0f / 30.0f);
	return RunChargedHeavy(*this, Layout, TEXT("Active then hold in one tick"), Delivery);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FChargedHeavyHitchAcrossAttackSectionTest,
	"KatanaCombat.CombatInput.ChargedHeavy.HitchAcrossHoldActiveRecoveryKeepsQueuedAction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChargedHeavyHitchAcrossAttackSectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// A hitch carries the attack section through its hold, Active and Recovery in one tick, with a Light already
	// queued for the next Recovery. That Light belongs to the release's Recovery, not the abandoned section's.
	const FChargedHeavyLayout Layout = MakeOneTickLayout(0.28f, 0.30f, 0.32f);
	FChargedHeavyDelivery Delivery;
	Delivery.OneTickFrom = 0.275f;
	Delivery.OneTickThrough = 0.325f;
	Delivery.ExpectedOneTickQueue = TEXT("Hold,Active,Recovery");
	Delivery.bQueueLightInWindup = true;
	return RunChargedHeavy(*this, Layout, TEXT("Hitch across hold, Active and Recovery"), Delivery);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FChargedHeavyReleaseIntoAbandonedSectionTest,
	"KatanaCombat.CombatInput.ChargedHeavy.ReleaseIntoAbandonedSectionPlaysItsPhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FChargedHeavyReleaseIntoAbandonedSectionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	// The charge ignores the abandoned section only while its hold lasts. A release that replays that same
	// section must get its Active and Recovery back.
	constexpr float FrameSeconds = 1.0f / 60.0f;
	const FChargedHeavyLayout Layout = MakeOneTickLayout(0.28f, 0.30f, 0.90f);
	FChargedHeavyFixture Fixture;
	if (!Fixture.Initialize(Layout))
	{
		AddError(TEXT("Charged-heavy fixture could not be created"));
		Fixture.Destroy();
		return false;
	}
	Fixture.Heavy->ChargeReleaseSection = TEXT("Attack");

	Fixture.Combat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Press);
	Fixture.AdvanceTo(0.275f, FrameSeconds);
	float TickSeconds = 0.0f;
	TestEqual(TEXT("One tick queues the hold and Active together"),
		Fixture.TickOnceTo(0.305f, TickSeconds), FString(TEXT("Hold,Active")));
	TestEqual(TEXT("The held button enters the charge loop"), Fixture.CurrentSection(), FName(TEXT("Loop")));
	TestEqual(TEXT("The stale Active is ignored while charging"),
		Fixture.Combat->GetCurrentPhase(), EAttackPhase::Windup);
	Fixture.Advance(0.2f, FrameSeconds);

	Fixture.Combat->OnInputEvent(EInputType::LightAttack, EInputEventType::Press);
	Fixture.Combat->OnInputEvent(EInputType::HeavyAttack, EInputEventType::Release);
	TestEqual(TEXT("Release replays the section the charge left"), Fixture.CurrentSection(), FName(TEXT("Attack")));
	Fixture.AdvanceTo(Layout.AttackActive + 0.05f, FrameSeconds);
	TestEqual(TEXT("The replayed section's Active is accepted after the hold ends"),
		Fixture.Combat->GetCurrentPhase(), EAttackPhase::Active);
	TestTrue(TEXT("The replayed strike detects hits"), Fixture.IsHitDetectionEnabled());
	Fixture.AdvanceTo(Layout.AttackRecovery + 0.05f, FrameSeconds);
	TestEqual(TEXT("The replayed section's Recovery runs the queued Light"),
		Fixture.Combat->GetCurrentAttack(), Fixture.Light);
	TestFalse(TEXT("The replayed strike's hit detection ended at Recovery"), Fixture.IsHitDetectionEnabled());

	Fixture.Destroy();
	return true;
}
