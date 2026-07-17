#include "Misc/AutomationTest.h"

#include "Core/CombatComponent.h"
#include "Data/AttackData.h"
#include "Debug/DefenseTelemetry.h"
#include "HAL/IConsoleManager.h"

namespace DefenseTelemetryTests
{
class FScopedDefenseTelemetryDebug
{
public:
	explicit FScopedDefenseTelemetryDebug(const int32 NewValue)
	{
		Variable = IConsoleManager::Get().FindConsoleVariable(TEXT("Combat.Defense.Debug"));
		if (Variable)
		{
			PreviousValue = Variable->GetInt();
			Variable->Set(NewValue, ECVF_SetByCode);
		}
	}

	~FScopedDefenseTelemetryDebug()
	{
		if (Variable)
		{
			Variable->Set(PreviousValue, ECVF_SetByCode);
		}
	}

	bool IsValid() const { return Variable != nullptr; }

private:
	IConsoleVariable* Variable = nullptr;
	int32 PreviousValue = 0;
};

int32 CountCsvFields(const FString& Line)
{
	bool bQuoted = false;
	int32 FieldCount = 1;
	for (int32 Index = 0; Index < Line.Len(); ++Index)
	{
		if (Line[Index] == TEXT('"'))
		{
			if (bQuoted && Index + 1 < Line.Len() && Line[Index + 1] == TEXT('"'))
			{
				++Index;
				continue;
			}
			bQuoted = !bQuoted;
		}
		else if (Line[Index] == TEXT(',') && !bQuoted)
		{
			++FieldCount;
		}
	}
	return FieldCount;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseTelemetryBoundedRingTest,
	"KatanaCombat.Defense.Telemetry.BoundedRing",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseTelemetryBoundedRingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;
	using namespace DefenseTelemetryTests;

	UCombatComponent* Combat = NewObject<UCombatComponent>();
	TestNotNull(TEXT("Combat component should be created"), Combat);
	if (!Combat)
	{
		return false;
	}

	FScopedDefenseTelemetryDebug Disabled(0);
	TestTrue(TEXT("Defense telemetry CVar should be registered"), Disabled.IsValid());
	TestNotNull(TEXT("Telemetry dump command should be registered"),
		IConsoleManager::Get().FindConsoleObject(TEXT("Combat.Defense.DumpTelemetry")));
	TestNotNull(TEXT("Telemetry clear command should be registered"),
		IConsoleManager::Get().FindConsoleObject(TEXT("Combat.Defense.ClearTelemetry")));
	FDefenseTelemetryRecord Record;
	Record.Event = EDefenseTelemetryEvent::Resolution;
	Record.SimulationTimestamp = 1.0;
	Combat->AppendDefenseTelemetry(Record);
	TestEqual(TEXT("Disabled telemetry should not retain records"), Combat->GetDefenseTelemetry().Num(), 0);

	{
		FScopedDefenseTelemetryDebug Enabled(1);
		const int32 Capacity = UCombatComponent::GetDefenseTelemetryCapacity();
		for (int32 Index = 0; Index < Capacity + 2; ++Index)
		{
			Record.SimulationTimestamp = static_cast<double>(Index);
			Combat->AppendDefenseTelemetry(Record);
		}

		const TArray<FDefenseTelemetryRecord>& Records = Combat->GetDefenseTelemetry();
		TestEqual(TEXT("Telemetry ring should retain only its configured capacity"), Records.Num(), Capacity);
		if (Records.Num() == Capacity)
		{
			TestEqual(TEXT("Oldest records should be evicted"), Records[0].SimulationTimestamp, 2.0);
			TestEqual(TEXT("Sequence should remain monotonic across eviction"), Records[0].Sequence, uint64(3));
			TestEqual(TEXT("Newest record should retain the latest sequence"), Records.Last().Sequence,
				static_cast<uint64>(Capacity + 2));
		}
	}

	Combat->ClearDefenseTelemetry();
	TestEqual(TEXT("Clear should empty the component-owned ring"), Combat->GetDefenseTelemetry().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseTelemetryCsvTest,
	"KatanaCombat.Defense.Telemetry.StableCsv",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseTelemetryCsvTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FDefenseTelemetryRecord Later;
	Later.Sequence = 2;
	Later.Event = EDefenseTelemetryEvent::Cleanup;
	Later.UnscaledTimestamp = 20.0;
	Later.SimulationTimestamp = 10.0;
	Later.DefenderStableId.Value = 22;
	Later.SelectedPresentationRow = TEXT("Row,Two");
	Later.CleanupReason = TEXT("Interrupted\"ByOwner");

	FDefenseTelemetryRecord Earlier;
	Earlier.Sequence = 7;
	Earlier.Event = EDefenseTelemetryEvent::Resolution;
	Earlier.UnscaledTimestamp = 10.0;
	Earlier.SimulationTimestamp = 5.0;
	Earlier.DefenderStableId.Value = 11;
	Earlier.Outcome = EDefenseOutcome::PerfectParry;
	Earlier.Reason = EDefenseReason::None;
	Earlier.SelectedPresentationRow = TEXT("ExactParry");

	const TArray<FDefenseTelemetryRecord> Unordered = {Later, Earlier};
	const FString Csv = DefenseTelemetry::BuildCsv(Unordered);
	TestTrue(TEXT("CSV should use the versioned stable header"),
		Csv.StartsWith(TEXT("schema_version,sequence,event,simulation_timestamp,unscaled_timestamp")));
	TestTrue(TEXT("CSV should escape commas"), Csv.Contains(TEXT("\"Row,Two\"")));
	TestTrue(TEXT("CSV should double embedded quotes"),
		Csv.Contains(TEXT("\"Interrupted\"\"ByOwner\"")));

	const int32 EarlierOffset = Csv.Find(TEXT("ExactParry"));
	const int32 LaterOffset = Csv.Find(TEXT("Row,Two"));
	TestTrue(TEXT("CSV should sort by timestamps and stable identity"),
		EarlierOffset != INDEX_NONE && LaterOffset != INDEX_NONE && EarlierOffset < LaterOffset);
	TestTrue(TEXT("CSV should emit enum names, not ordinal values"),
		Csv.Contains(TEXT("PerfectParry")) && Csv.Contains(TEXT("Resolution")));
	TArray<FString> Lines;
	Csv.ParseIntoArrayLines(Lines, false);
	TestEqual(TEXT("CSV fixture should contain one header and two rows"), Lines.Num(), 3);
	if (Lines.Num() == 3)
	{
		const int32 HeaderFields = DefenseTelemetryTests::CountCsvFields(Lines[0]);
		TestEqual(TEXT("First data row should match header cardinality"),
			DefenseTelemetryTests::CountCsvFields(Lines[1]), HeaderFields);
		TestEqual(TEXT("Second data row should match header cardinality"),
			DefenseTelemetryTests::CountCsvFields(Lines[2]), HeaderFields);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FDefenseTelemetryResolutionMappingTest,
	"KatanaCombat.Defense.Telemetry.ResolutionMapping",
	EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDefenseTelemetryResolutionMappingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	AActor* Defender = NewObject<AActor>();
	AActor* Attacker = NewObject<AActor>();
	UAttackData* AttackData = NewObject<UAttackData>();
	FDefenseResolution Resolution;
	Resolution.Stage = EDefenseQueryStage::Contact;
	Resolution.InteractionId.Epoch = 17;
	Resolution.InteractionId.Key.Stage = EDefenseQueryStage::Contact;
	Resolution.InteractionId.Key.Defender = Defender;
	Resolution.InteractionId.Key.ContactInstance.bUsesAttackWindow = true;
	Resolution.InteractionId.Key.ContactInstance.AttackWindow.AttackInstance.Attacker = Attacker;
	Resolution.InteractionId.Key.ContactInstance.AttackWindow.AttackInstance.AttackGeneration = 9;
	Resolution.InteractionId.Key.ContactInstance.AttackWindow.Kind = EAttackWindowKind::Hit;
	Resolution.InteractionId.Key.ContactInstance.AttackWindow.WindowGeneration = 4;
	Resolution.Decision.Outcome = EDefenseOutcome::NormalBlock;
	Resolution.Decision.Reason = EDefenseReason::None;
	Resolution.Decision.SelectedAttack = AttackData;
	Resolution.Decision.AttackInstance =
		Resolution.InteractionId.Key.ContactInstance.AttackWindow.AttackInstance;
	Resolution.Decision.Height = EAttackHeight::High;
	Resolution.Decision.Lane = EIncomingAttackLane::Left;
	Resolution.Decision.SwingShape = ESwingDirection::Vertical;
	Resolution.PredictedContact.Height = EAttackHeight::Middle;
	Resolution.PredictedContact.Lane = EIncomingAttackLane::Center;
	Resolution.PredictedContact.PathDirection = FVector(1.0, 2.0, 0.0);
	Resolution.bHasActualContact = true;
	Resolution.ActualContact.bIsValid = true;
	Resolution.ActualContact.Height = EAttackHeight::High;
	Resolution.ActualContact.Lane = EIncomingAttackLane::Left;
	Resolution.ActualContact.IncomingTrajectory = FVector(-1.0, 0.0, 0.0);
	Resolution.PresentationRow = TEXT("BlockHighLeft");
	Resolution.PresentationFallback = EDefensePresentationFallbackLevel::Exact;

	const FDefenseTelemetryRecord Record = DefenseTelemetry::FromResolution(
		Resolution,
		EDefenseTelemetryEvent::PresentationStart);
	TestEqual(TEXT("Event should be preserved"), Record.Event, EDefenseTelemetryEvent::PresentationStart);
	TestEqual(TEXT("Interaction epoch should be preserved"), Record.InteractionId.Epoch, uint64(17));
	TestEqual(TEXT("Attack generation should be preserved"), Record.AttackInstance.AttackGeneration, 9);
	TestEqual(TEXT("Contact window identity should be preserved"), Record.AttackWindow.WindowGeneration, 4);
	TestEqual(TEXT("Defender should come from the committed interaction"), Record.Defender.Get(), Defender);
	TestEqual(TEXT("Attacker should come from the committed attack identity"), Record.Attacker.Get(), Attacker);
	TestEqual(TEXT("Outcome should be preserved"), Record.Outcome, EDefenseOutcome::NormalBlock);
	TestEqual(TEXT("Predicted axis should be preserved"), Record.PredictedAxis, FVector(1.0, 2.0, 0.0));
	TestEqual(TEXT("Actual axis should be preserved"), Record.ActualAxis, FVector(-1.0, 0.0, 0.0));
	TestEqual(TEXT("Actual height should use contact evidence"), Record.ActualHeight, EAttackHeight::High);
	TestEqual(TEXT("Presentation row should be preserved"), Record.SelectedPresentationRow,
		FName(TEXT("BlockHighLeft")));
	TestEqual(TEXT("Attack asset path should be preserved"), Record.AttackDataPath,
		FSoftObjectPath(AttackData));
	return true;
}
