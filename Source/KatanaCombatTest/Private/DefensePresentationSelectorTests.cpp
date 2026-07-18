#include "Misc/AutomationTest.h"

#include "Characters/PlayerCharacter.h"
#include "CombatTestHelpers.h"
#include "Core/CombatComponent.h"
#include "Data/CombatSettings.h"
#include "Data/DefenseConfiguration.h"
#include "Defense/DefensePresentationSelector.h"
#include "Utilities/CombatGameplayTags.h"

namespace DefensePresentationSelectorTests
{
const IDefensePresentationSelector& Selector()
{
	static const FTableDefensePresentationSelector TableSelector;
	return TableSelector;
}

FDefensePresentationSelectionContext MakeBlockContext()
{
	FDefensePresentationSelectionContext Context;
	Context.Outcome = EDefenseOutcome::NormalBlock;
	Context.AttackerResponse = EAttackerResponse::Recoil;
	Context.Height = EAttackHeight::High;
	Context.Lane = EIncomingAttackLane::Left;
	Context.SwingShape = ESwingDirection::Horizontal;
	Context.bPairedBridgeUsable = true;
	return Context;
}

FDefensePresentationRow MakeDefenderRow(const FName Name, const int32 Priority = 0)
{
	FDefensePresentationRow Row;
	Row.RowName = Name;
	Row.Outcome = EDefenseOutcome::NormalBlock;
	Row.Priority = Priority;
	return Row;
}

FAttackerResponsePresentationRow MakeAttackerRow(const FName Name)
{
	FAttackerResponsePresentationRow Row;
	Row.RowName = Name;
	Row.Response = EAttackerResponse::Recoil;
	return Row;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePresentationRankingTest,
	"KatanaCombat.Defense.Presentation.Ranking",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefensePresentationRankingTest::RunTest(const FString& Parameters)
{
	using namespace DefensePresentationSelectorTests;
	(void)Parameters;

	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>();

	FDefensePresentationRow Wildcard = MakeDefenderRow(TEXT("Wildcard"), 100);
	FDefensePresentationRow Exact = MakeDefenderRow(TEXT("Exact"), 0);
	Exact.bMatchAnyHeight = false;
	Exact.Height = EAttackHeight::High;
	Configuration->DefenderPresentationRows = {Wildcard, Exact};

	const FDefensePresentationSelectionResult ExactResult =
		Selector().SelectDefender(MakeBlockContext(), Configuration);
	TestEqual(TEXT("An exact closed field outranks wildcard priority"), ExactResult.RowName, FName(TEXT("Exact")));
	TestEqual(TEXT("Specialized row reports exact fallback level"), ExactResult.FallbackLevel,
		EDefensePresentationFallbackLevel::Exact);

	FDefensePresentationRow HighPriority = MakeDefenderRow(TEXT("HighPriority"), 100);
	FDefensePresentationRow MoreRequiredTags = MakeDefenderRow(TEXT("MoreRequiredTags"), 0);
	MoreRequiredTags.RequiredTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	Configuration->DefenderPresentationRows = {HighPriority, MoreRequiredTags};
	FDefensePresentationSelectionContext TaggedContext = MakeBlockContext();
	TaggedContext.AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	const FDefensePresentationSelectionResult RequiredResult =
		Selector().SelectDefender(TaggedContext, Configuration);
	TestEqual(TEXT("Required-tag count outranks priority"), RequiredResult.RowName,
		FName(TEXT("MoreRequiredTags")));

	FDefensePresentationRow ExactAgainstTags = MakeDefenderRow(TEXT("ExactAgainstTags"), -100);
	ExactAgainstTags.bMatchAnyHeight = false;
	ExactAgainstTags.Height = EAttackHeight::High;
	FDefensePresentationRow TagDense = MakeDefenderRow(TEXT("TagDense"), 1000);
	TagDense.RequiredTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	TagDense.RequiredTags.AddTag(KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	Configuration->DefenderPresentationRows = {TagDense, ExactAgainstTags};
	FDefensePresentationSelectionContext DenseTaggedContext = MakeBlockContext();
	DenseTaggedContext.AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseParryable());
	DenseTaggedContext.AttackTags.AddTag(KatanaCombatGameplayTags::AttackDefenseBlockInterruptible());
	const FDefensePresentationSelectionResult ExactAgainstTagsResult =
		Selector().SelectDefender(DenseTaggedContext, Configuration);
	TestEqual(TEXT("Exact-field count outranks tag count and priority"),
		ExactAgainstTagsResult.RowName, FName(TEXT("ExactAgainstTags")));

	FDefensePresentationRow LowPriority = MakeDefenderRow(TEXT("LowPriority"), 1);
	FDefensePresentationRow WinningPriority = MakeDefenderRow(TEXT("WinningPriority"), 2);
	Configuration->DefenderPresentationRows = {LowPriority, WinningPriority};
	const FDefensePresentationSelectionResult PriorityResult =
		Selector().SelectDefender(MakeBlockContext(), Configuration);
	TestEqual(TEXT("Priority breaks equal specificity"), PriorityResult.RowName,
		FName(TEXT("WinningPriority")));

	FDefensePresentationRow Excluded = MakeDefenderRow(TEXT("Excluded"), 1000);
	Excluded.ExcludedTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	FDefensePresentationRow Allowed = MakeDefenderRow(TEXT("Allowed"), 0);
	Configuration->DefenderPresentationRows = {Excluded, Allowed};
	FDefensePresentationSelectionContext ExcludedContext = MakeBlockContext();
	ExcludedContext.AttackTags.AddTag(KatanaCombatGameplayTags::AttackPropertyUnblockable());
	const FDefensePresentationSelectionResult ExcludedResult =
		Selector().SelectDefender(ExcludedContext, Configuration);
	TestEqual(TEXT("Excluded tags reject a row"), ExcludedResult.RowName, FName(TEXT("Allowed")));
	TestFalse(TEXT("A row with exclusions is not a universal generic fallback"), Excluded.IsGenericFallback());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePresentationAmbiguityTest,
	"KatanaCombat.Defense.Presentation.AmbiguityDeterministic",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefensePresentationAmbiguityTest::RunTest(const FString& Parameters)
{
	using namespace DefensePresentationSelectorTests;
	(void)Parameters;

	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>();
	Configuration->DefenderPresentationRows = {
		MakeDefenderRow(TEXT("Zulu"), 10),
		MakeDefenderRow(TEXT("Alpha"), 10)
	};

	const FDefensePresentationSelectionResult Result =
		Selector().SelectDefender(MakeBlockContext(), Configuration);
	TestTrue(TEXT("Equal ranking is reported as authoring ambiguity"), Result.bAmbiguous);
	TestEqual(TEXT("Lexical row name is deterministic safety fallback"), Result.RowName, FName(TEXT("Alpha")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseAttackerConfigurationSelectionTest,
	"KatanaCombat.Defense.Presentation.AttackerUsesAttackerConfiguration",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseAttackerConfigurationSelectionTest::RunTest(const FString& Parameters)
{
	using namespace DefensePresentationSelectorTests;
	(void)Parameters;

	UDefenseConfiguration* DefenderConfiguration = NewObject<UDefenseConfiguration>();
	UDefenseConfiguration* AttackerConfiguration = NewObject<UDefenseConfiguration>();
	DefenderConfiguration->AttackerResponseRows = {MakeAttackerRow(TEXT("WrongDefenderSet"))};
	AttackerConfiguration->AttackerResponseRows = {MakeAttackerRow(TEXT("AttackerSet"))};

	const FDefensePresentationSelectionResult Result =
		Selector().SelectAttacker(MakeBlockContext(), AttackerConfiguration);
	TestEqual(TEXT("Attacker response reads the supplied attacker configuration"), Result.RowName,
		FName(TEXT("AttackerSet")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefensePresentationFallbackTest,
	"KatanaCombat.Defense.Presentation.FallbackPreservesOutcome",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefensePresentationFallbackTest::RunTest(const FString& Parameters)
{
	using namespace DefensePresentationSelectorTests;
	(void)Parameters;

	FDefensePresentationSelectionContext ParryContext = MakeBlockContext();
	ParryContext.Outcome = EDefenseOutcome::PerfectParry;
	ParryContext.bPairedBridgeUsable = false;

	const FDefensePresentationSelectionResult Missing =
		Selector().SelectDefender(ParryContext, nullptr);
	TestFalse(TEXT("Missing configuration selects no row"), Missing.bFound);
	TestTrue(TEXT("Missing configuration returns an empty payload"), Missing.Payload.IsEmpty());
	TestEqual(TEXT("Missing presentation cannot change gameplay outcome"), Missing.Outcome,
		EDefenseOutcome::PerfectParry);

	UDefenseConfiguration* Configuration = NewObject<UDefenseConfiguration>();
	FDefensePresentationRow Bridge = MakeDefenderRow(TEXT("Bridge"), 100);
	Bridge.Outcome = EDefenseOutcome::PerfectParry;
	Bridge.bMatchAnyHeight = false;
	Bridge.Height = EAttackHeight::High;
	Bridge.Payload.bRequiresBridgePreflight = true;
	FDefensePresentationRow Generic = MakeDefenderRow(TEXT("Generic"), 0);
	Generic.Outcome = EDefenseOutcome::PerfectParry;
	Configuration->DefenderPresentationRows = {Bridge, Generic};

	const FDefensePresentationSelectionResult Fallback =
		Selector().SelectDefender(ParryContext, Configuration);
	TestEqual(TEXT("Unusable bridge falls through to generic row"), Fallback.RowName, FName(TEXT("Generic")));
	TestEqual(TEXT("Generic row reports generic fallback"), Fallback.FallbackLevel,
		EDefensePresentationFallbackLevel::Generic);
	TestEqual(TEXT("Bridge fallback cannot rewrite perfect parry"), Fallback.Outcome,
		EDefenseOutcome::PerfectParry);

	FDefensePresentationPayload ContactOverride;
	ContactOverride.TargetBoneOverride = TEXT("spine_03");
	TestFalse(TEXT("A contact override is a meaningful presentation payload"),
		ContactOverride.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseConfigurationDefaultsTest,
	"KatanaCombat.Defense.Presentation.ConfigurationDefaults",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseConfigurationDefaultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	const UDefenseConfiguration* Configuration = GetDefault<UDefenseConfiguration>();
	TestEqual(TEXT("Hard cone"), Configuration->HardGuardConeHalfAngle, 70.0f);
	TestEqual(TEXT("Maximum automatic turn"), Configuration->MaximumAutomaticTurn, 70.0f);
	TestEqual(TEXT("Defense turn rate"), Configuration->DefenseTurnRate, 180.0f);
	TestEqual(TEXT("Normal block tolerance"), Configuration->NormalBlockFinalTolerance, 35.0f);
	TestEqual(TEXT("Perfect parry tolerance"), Configuration->PerfectParryFinalTolerance, 10.0f);
	TestEqual(TEXT("Threat lock"), Configuration->ThreatLockMinSeconds, 0.15f);
	TestEqual(TEXT("Threat switch lead"), Configuration->ThreatSwitchLeadSeconds, 0.10f);
	TestEqual(TEXT("Center lane half-angle"), Configuration->CenterLaneHalfAngle, 12.0f);
	TestEqual(TEXT("Guarded threat refresh"), Configuration->GuardedThreatRefreshSeconds, 0.05f);
	TestEqual(TEXT("Maximum high-confidence prediction age"),
		Configuration->MaximumHighConfidencePredictionAge, 0.10f);
	TestEqual(TEXT("Defense threat range"), Configuration->DefenseThreatRange, 1000.0f);
	TestEqual(TEXT("Guard manual override threshold"),
		Configuration->GuardManualOverrideThreshold, 0.25f);
	TestEqual(TEXT("Guard auto-facing resume"),
		Configuration->GuardAutoFacingResumeSeconds, 0.10f);
	TestEqual(TEXT("Interaction tombstone"), Configuration->InteractionTombstoneSeconds, 1.0f);
	TestEqual(TEXT("Terminal cache cap"), Configuration->TerminalInteractionCacheCap, 128);
	TestEqual(TEXT("No-montage bridge"), Configuration->NoMontageParryBridgeSeconds, 0.15f);
	TestEqual(TEXT("Counter window"), Configuration->CounterWindowSeconds, 2.0f);
	TestEqual(TEXT("Finisher ready"), Configuration->FinisherReadySeconds, 2.0f);
	TestEqual(TEXT("Lease watchdog"), Configuration->TimeDilationLeaseWatchdogSeconds, 10.0f);
	TestEqual(TEXT("Normal-block authored translation"), Configuration->NormalBlockTranslationAllowance, 0.0f);
	TestEqual(TEXT("Normal-block numerical drift"), Configuration->NormalBlockTranslationDriftTolerance, 1.0f);
	TestEqual(TEXT("Perfect-parry per-role translation"), Configuration->PerfectParryTranslationAllowancePerRole, 75.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDefenseConfigurationPrecedenceTest,
	"KatanaCombat.Defense.Presentation.ConfigurationPrecedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDefenseConfigurationPrecedenceTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	UWorld* World = FCombatTestHelpers::CreateTestWorld();
	UCombatComponent* Combat = nullptr;
	APlayerCharacter* Player = FCombatTestHelpers::CreateTestCharacterWithCombat(World, Combat);
	if (!Player || !Combat)
	{
		AddError(TEXT("Failed to create defense-configuration fixture"));
		FCombatTestHelpers::DestroyTestWorld(World);
		return false;
	}

	UDefenseConfiguration* SettingsConfiguration = NewObject<UDefenseConfiguration>();
	UDefenseConfiguration* ComponentConfiguration = NewObject<UDefenseConfiguration>();
	UDefenseConfiguration* FirstStanceConfiguration = NewObject<UDefenseConfiguration>();
	UDefenseConfiguration* NewestStanceConfiguration = NewObject<UDefenseConfiguration>();
	UCombatSettings* Settings = NewObject<UCombatSettings>();
	Settings->DefenseConfiguration = SettingsConfiguration;
	Player->CombatSettings = Settings;

	TestTrue(TEXT("Character settings provide the base defense configuration"),
		Combat->GetEffectiveDefenseConfiguration() == SettingsConfiguration);
	Combat->DefenseConfigurationOverride = ComponentConfiguration;
	TestTrue(TEXT("Component override wins over character settings"),
		Combat->GetEffectiveDefenseConfiguration() == ComponentConfiguration);

	const FDefenseConfigurationOverrideHandle FirstHandle =
		Combat->AcquireDefenseStanceOverride(FirstStanceConfiguration);
	const FDefenseConfigurationOverrideHandle NewestHandle =
		Combat->AcquireDefenseStanceOverride(NewestStanceConfiguration);
	TestTrue(TEXT("First stance override returns an opaque handle"), FirstHandle.IsValid());
	TestTrue(TEXT("Newest stance override returns an opaque handle"), NewestHandle.IsValid());
	TestTrue(TEXT("Newest active stance override has highest precedence"),
		Combat->GetEffectiveDefenseConfiguration() == NewestStanceConfiguration);

	Combat->ReleaseDefenseStanceOverride(FirstHandle);
	TestTrue(TEXT("Out-of-order release does not disturb the newest override"),
		Combat->GetEffectiveDefenseConfiguration() == NewestStanceConfiguration);
	Combat->ReleaseDefenseStanceOverride(NewestHandle);
	TestTrue(TEXT("Releasing all stance overrides restores component precedence"),
		Combat->GetEffectiveDefenseConfiguration() == ComponentConfiguration);

	Combat->DefenseConfigurationOverride = nullptr;
	TestTrue(TEXT("Clearing the component override restores settings precedence"),
		Combat->GetEffectiveDefenseConfiguration() == SettingsConfiguration);
	Settings->DefenseConfiguration = nullptr;
	TestTrue(TEXT("Missing authored configuration falls back to C++ defaults"),
		Combat->GetEffectiveDefenseConfiguration() == GetDefault<UDefenseConfiguration>());

	FCombatTestHelpers::DestroyTestWorld(World);
	return true;
}
