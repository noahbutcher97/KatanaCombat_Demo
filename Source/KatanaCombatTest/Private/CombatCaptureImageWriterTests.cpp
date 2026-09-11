// Copyright Epic Games, Inc. All Rights Reserved.
#include "Misc/AutomationTest.h"
#include "Analysis/CombatCaptureImageWriter.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureImageWriterTest,
	"KatanaCombat.Capture.ImageWriter.LosslessAndBounded", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCaptureImageWriterTest::RunTest(const FString&)
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CaptureImageWriterTests") / FGuid::NewGuid().ToString();
	IFileManager::Get().MakeDirectory(*Directory, true);
	const TArray<FColor> Original = {FColor(1, 2, 3, 255), FColor(255, 0, 123, 255), FColor(27, 198, 61, 255), FColor(0, 0, 0, 255)};
	FCombatCaptureImageWriter Writer; FString Error;
	TestFalse(TEXT("Invalid dimensions rejected before allocating"), Writer.CanEnqueue(FIntPoint(-1, 2), Error));
	TestFalse(TEXT("Oversized viewport rejected before readback"), Writer.CanEnqueue(FIntPoint(8192, 8192), Error));
	TestFalse(TEXT("Mismatched pixel buffer rejected"), Writer.Enqueue(Directory / TEXT("bad.png"), FIntPoint(3, 3), TArray<FColor>(Original), Error));
	for (int32 I = 0; I < FCombatCaptureImageWriter::MaxPendingFrames; ++I)
	{
		TestTrue(TEXT("Bounded submission accepted"), Writer.Enqueue(Directory / FString::Printf(TEXT("%d.png"), I), FIntPoint(2, 2), TArray<FColor>(Original), Error));
	}
	// Completed-but-uncollected work still owns a slot, so no artificial sleep is needed.
	TestFalse(TEXT("Queue cannot grow past the frame bound"), Writer.CanEnqueue(FIntPoint(2, 2), Error));
	TestEqual(TEXT("All pending file bytes are reserved"), Writer.GetReservedFileBytes(), FCombatCaptureImageWriter::FileByteReservation(FIntPoint(2, 2)) * 4);
	FCombatCaptureImageWriter::FResult Result; int32 Count = 0;
	while (Writer.Collect(Result, true))
	{
		TestTrue(TEXT("PNG export succeeded"), Result.Error.IsEmpty());
		TestEqual(TEXT("Results retain submission order"), Result.File, Directory / FString::Printf(TEXT("%d.png"), Count));
		TestEqual(TEXT("Written byte count is exact"), Result.BytesWritten, IFileManager::Get().FileSize(*Result.File));
		FImage Decoded;
		if (TestTrue(TEXT("Written PNG decodes"), FImageUtils::LoadImage(*Result.File, Decoded)))
		{
			Decoded.ChangeFormat(ERawImageFormat::BGRA8, EGammaSpace::sRGB);
			TestEqual(TEXT("Decoded dimensions"), Decoded.SizeX * Decoded.SizeY, Original.Num());
			TestTrue(TEXT("Background export preserves every pixel byte"), Decoded.RawData.Num() == Original.Num() * sizeof(FColor)
				&& FMemory::Memcmp(Decoded.RawData.GetData(), Original.GetData(), Decoded.RawData.Num()) == 0);
		}
		++Count;
	}
	TestEqual(TEXT("All images drained"), Count, 4);
	TestEqual(TEXT("No reserved file bytes remain"), Writer.GetReservedFileBytes(), int64(0));
	TestTrue(TEXT("Slots reusable after collection"), Writer.CanEnqueue(FIntPoint(2, 2), Error));
	const FIntPoint LargeSize(2048, 2048);
	TArray<FColor> LargePixels; LargePixels.SetNumZeroed(LargeSize.X * LargeSize.Y);
	TestTrue(TEXT("One bounded large image accepted"), Writer.Enqueue(Directory / TEXT("large.png"), LargeSize, MoveTemp(LargePixels), Error));
	TestFalse(TEXT("Byte bound is independent of the frame bound"), Writer.CanEnqueue(LargeSize, Error));
	TestTrue(TEXT("Large image drains"), Writer.Collect(Result, true));
	TestTrue(TEXT("Large image export succeeded"), Result.Error.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatCaptureImageWriterFailureTest,
	"KatanaCombat.Capture.ImageWriter.FailureAndTeardown", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCombatCaptureImageWriterFailureTest::RunTest(const FString&)
{
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("CaptureImageWriterTests") / FGuid::NewGuid().ToString();
	IFileManager::Get().MakeDirectory(*Directory, true);
	FString Error; FCombatCaptureImageWriter::FResult Result;
	FCombatCaptureImageWriter Writer;
	const FString Obstruction = Directory / TEXT("blocked.png"); IFileManager::Get().MakeDirectory(*Obstruction);
	TestTrue(TEXT("Write-failure fixture submits"), Writer.Enqueue(Obstruction, FIntPoint(1, 1), {FColor::Red}, Error));
	TestTrue(TEXT("Failed write returns a result"), Writer.Collect(Result, true));
	TestFalse(TEXT("Write failure cannot appear successful"), Result.Error.IsEmpty());
	TestEqual(TEXT("Failed file has no claimed bytes"), Result.BytesWritten, int64(0));
	TestEqual(TEXT("Failure releases reservation"), Writer.GetReservedFileBytes(), int64(0));
	const FString FinalFile = Directory / TEXT("teardown.png");
	{
		FCombatCaptureImageWriter Scoped;
		TestTrue(TEXT("Teardown fixture submits"), Scoped.Enqueue(FinalFile, FIntPoint(1, 1), {FColor::Blue}, Error));
	}
	FImage Decoded;
	TestTrue(TEXT("Destruction finishes outstanding PNG before returning"), FImageUtils::LoadImage(*FinalFile, Decoded));
	return true;
}
