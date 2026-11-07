// Copyright (c) Jared Taylor

#if WITH_DEV_AUTOMATION_TESTS

#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "MumbleSubsystem.h"
#include "MumbleSynth.h"
#include "MumbleText.h"
#include "MumbleVoice.h"
#include "Sound/SoundBase.h"

namespace MumbleTests
{
	constexpr int32 SampleRate = 48000;
	constexpr int32 Block = 256;

	const TCHAR* Lines[] =
	{
		TEXT("Sister Agnes, you must go to the mountain temple before the pass closes."),
		TEXT("Did you really think I wouldn't notice?"),
		TEXT("STOP! Put that down right now!"),
		TEXT("I... I don't know anymore..."),
	};

	/**
	 * Speaks a line the way UMumbleComponent::Speak does, offline.
	 *
	 * @return The rendered samples
	 */
	TArray<float> Render(const FString& Text, const FMumbleVoiceSynth& Synth, const FMumbleProsody& Prosody, const FMumbleTiming& Timing, uint32 Seed)
	{
		const FMumbleLine Line = FMumbleLine::Analyze(Text, Prosody, Seed);

		TArray<UTF32CHAR> Chars;
		MumbleText::ToCodepoints(Text, Chars);
		TArray<float> Schedule;
		MumbleText::BuildSchedule(Chars, Timing, Schedule);

		FMumbleSynth Voice;
		Voice.SetVoice(Synth);
		Voice.Init(float(SampleRate), 1234u + Seed);

		const float CharInterval = 1.f / FMath::Max(Timing.CharactersPerSecond, 1.f);
		const float Length = (Schedule.Num() > 0 ? Schedule.Last() : 0.f) + 1.f;

		TArray<float> Out;
		Out.SetNumZeroed(int32(Length * SampleRate) / Block * Block);

		int32 Revealed = INDEX_NONE;
		int32 Next = 0;
		for (int32 Start = 0; Start < Out.Num(); Start += Block)
		{
			const float Time = float(Start) / SampleRate;
			int32 Reached = Revealed;
			while (Reached + 1 < Schedule.Num() && Schedule[Reached + 1] <= Time)
			{
				++Reached;
			}
			if (Reached > Revealed)
			{
				int32 Due = INDEX_NONE;
				while (Line.Syllables.IsValidIndex(Next) && Line.Syllables[Next].TriggerChar <= Reached)
				{
					Due = Next++;
				}
				if (Due != INDEX_NONE)
				{
					Voice.Speak(FMumbleLine::Realise(Line.Syllables[Due], Synth, Prosody, CharInterval));
				}
				Revealed = Reached;
			}
			Voice.Render(Out.GetData() + Start, Block);
		}
		return Out;
	}

	void WriteWav(const FString& Path, const TArray<float>& Samples)
	{
		TArray<uint8> Bytes;
		auto Put32 = [&Bytes](uint32 V) { Bytes.Append(reinterpret_cast<const uint8*>(&V), 4); };
		auto Put16 = [&Bytes](uint16 V) { Bytes.Append(reinterpret_cast<const uint8*>(&V), 2); };

		const uint32 DataSize = uint32(Samples.Num()) * 2;
		Bytes.Append(reinterpret_cast<const uint8*>("RIFF"), 4);
		Put32(36 + DataSize);
		Bytes.Append(reinterpret_cast<const uint8*>("WAVEfmt "), 8);
		Put32(16);
		Put16(1);
		Put16(1);
		Put32(SampleRate);
		Put32(SampleRate * 2);
		Put16(2);
		Put16(16);
		Bytes.Append(reinterpret_cast<const uint8*>("data"), 4);
		Put32(DataSize);
		for (const float Sample : Samples)
		{
			Put16(uint16(int16(FMath::Clamp(Sample, -1.f, 1.f) * 32767.f)));
		}
		FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	int32 CountSyllables(const TCHAR* Text)
	{
		return FMumbleLine::Analyze(Text, FMumbleProsody(), 0).Syllables.Num();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMumbleTextTest, "Mumble.Text", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMumbleTextTest::RunTest(const FString& Parameters)
{
	using namespace MumbleTests;

	TestEqual(TEXT("Sister Agnes is four syllables"), CountSyllables(TEXT("Sister Agnes")), 4);
	TestEqual(TEXT("a magic e is silent"), CountSyllables(TEXT("make")), 1);
	TestEqual(TEXT("a consonant-le is its own syllable"), CountSyllables(TEXT("table")), 2);
	TestEqual(TEXT("-ed after t is a syllable"), CountSyllables(TEXT("wanted")), 2);
	TestEqual(TEXT("-ed elsewhere is not"), CountSyllables(TEXT("jumped")), 1);
	TestEqual(TEXT("-es after x is a syllable"), CountSyllables(TEXT("boxes")), 2);
	TestEqual(TEXT("a vowel team is one syllable"), CountSyllables(TEXT("boat")), 1);
	TestEqual(TEXT("radio is three"), CountSyllables(TEXT("radio")), 3);
	TestEqual(TEXT("y ending a word is a vowel"), CountSyllables(TEXT("happy")), 2);
	TestEqual(TEXT("macrons fold to their letter"), CountSyllables(TEXT("Ryūsei")), 2);
	TestEqual(TEXT("kana: n closes the mora before it"), CountSyllables(TEXT("こんにちは")), 4);
	TestEqual(TEXT("kana: small ya merges"), CountSyllables(TEXT("きゃ")), 1);
	TestEqual(TEXT("one syllable per ideograph"), CountSyllables(TEXT("月は")), 2);

	const FMumbleLine Question = FMumbleLine::Analyze(TEXT("Are you coming?"), FMumbleProsody(), 0);
	if (TestTrue(TEXT("question has syllables"), Question.Syllables.Num() > 0))
	{
		const FMumbleSyllable& Last = Question.Syllables.Last().Sound;
		TestTrue(TEXT("a question rises at the end"), Last.PitchEndHz > Question.Syllables[0].Sound.PitchStartHz);
		TestEqual(TEXT("a question does not creak"), Last.Creak, 0.f);
	}

	const FMumbleLine Same1 = FMumbleLine::Analyze(TEXT("Hello there."), FMumbleProsody(), 7);
	const FMumbleLine Same2 = FMumbleLine::Analyze(TEXT("Hello there."), FMumbleProsody(), 7);
	TestEqual(TEXT("the same line says itself the same way"), Same1.Syllables[0].Sound.PitchStartHz, Same2.Syllables[0].Sound.PitchStartHz);

	// Pauses land after the mark, once for an ellipsis, never inside "3.5"
	FMumbleTiming Timing;
	Timing.CharactersPerSecond = 10.f;
	TArray<UTF32CHAR> Chars;
	TArray<float> Times;
	MumbleText::ToCodepoints(TEXT("a, b"), Chars);
	MumbleText::BuildSchedule(Chars, Timing, Times);
	TestEqual(TEXT("comma pause lands on the next letter"), Times[3] - Times[2], 0.1f + Timing.CommaPause, 1e-4f);
	MumbleText::ToCodepoints(TEXT("3.5"), Chars);
	MumbleText::BuildSchedule(Chars, Timing, Times);
	TestEqual(TEXT("no pause inside a number"), Times[2] - Times[1], 0.1f, 1e-4f);
	MumbleText::ToCodepoints(TEXT("a... b"), Chars);
	MumbleText::BuildSchedule(Chars, Timing, Times);
	TestEqual(TEXT("an ellipsis pauses once"), Times[5] - Times[4], 0.1f + Timing.EllipsisPause, 1e-4f);
	TestEqual(TEXT("and not between its dots"), Times[3] - Times[2], 0.1f, 1e-4f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMumbleRenderTest, "Mumble.Render", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMumbleRenderTest::RunTest(const FString& Parameters)
{
	using namespace MumbleTests;

	// The shipped voices, plus the defaults so there is always one
	TArray<TPair<FString, const UMumbleVoice*>> Voices;
	Voices.Emplace(TEXT("Default"), GetDefault<UMumbleVoice>());

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.ScanPathsSynchronous({ TEXT("/Mumble/Voices") }, true);
	TArray<FAssetData> Assets;
	Registry.GetAssetsByPath(TEXT("/Mumble/Voices"), Assets, true);
	for (const FAssetData& Asset : Assets)
	{
		if (const UMumbleVoice* Loaded = Cast<UMumbleVoice>(Asset.GetAsset()))
		{
			Voices.Emplace(Asset.AssetName.ToString(), Loaded);
		}
	}

	const FString Folder = FPaths::ProjectSavedDir() / TEXT("Mumble");
	IFileManager::Get().MakeDirectory(*Folder, true);

	for (const TPair<FString, const UMumbleVoice*>& Entry : Voices)
	{
		const UMumbleVoice* Voice = Entry.Value;
		TArray<float> All;
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(Lines); ++Index)
		{
			const TArray<float> Samples = Render(Lines[Index], Voice->Synth, Voice->Prosody, Voice->Timing, uint32(Voice->Seed));

			double SumSquares = 0.0;
			int32 Active = 0;
			float Peak = 0.f;
			bool bFinite = true;
			for (const float Sample : Samples)
			{
				bFinite &= FMath::IsFinite(Sample);
				Peak = FMath::Max(Peak, FMath::Abs(Sample));
				if (FMath::Abs(Sample) > 1e-3f)
				{
					SumSquares += double(Sample) * Sample;
					++Active;
				}
			}
			const float Rms = Active > 0 ? float(FMath::Sqrt(SumSquares / Active)) : 0.f;
			const float RmsDb = 20.f * FMath::LogX(10.f, FMath::Max(Rms, 1e-6f));

			AddInfo(FString::Printf(TEXT("%s line %d: peak %.3f, active RMS %.1f dBFS, %.0f%% active"), *Entry.Key, Index, Peak, RmsDb,
				100.f * Active / FMath::Max(Samples.Num(), 1)));

			TestTrue(*FString::Printf(TEXT("%s line %d is finite"), *Entry.Key, Index), bFinite);
			TestTrue(*FString::Printf(TEXT("%s line %d is audible"), *Entry.Key, Index), RmsDb > -36.f);
			TestTrue(*FString::Printf(TEXT("%s line %d does not clip"), *Entry.Key, Index), Peak < 0.99f);

			const int32 TailStart = Samples.Num() - SampleRate / 4;
			float Tail = 0.f;
			for (int32 Sample = FMath::Max(TailStart, 0); Sample < Samples.Num(); ++Sample)
			{
				Tail = FMath::Max(Tail, FMath::Abs(Samples[Sample]));
			}
			TestTrue(*FString::Printf(TEXT("%s line %d falls silent"), *Entry.Key, Index), Tail < 1e-3f);

			All.Append(Samples);
		}
		WriteWav(Folder / (Entry.Key + TEXT(".wav")), All);
	}

	AddInfo(FString::Printf(TEXT("Wrote %d voices to %s"), Voices.Num(), *FPaths::ConvertRelativePathToFull(Folder)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMumbleDefaultSoundTest, "Mumble.DefaultSound", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FMumbleDefaultSoundTest::RunTest(const FString& Parameters)
{
	UMumbleSubsystem* Subsystem = UMumbleSubsystem::Get();
	if (!TestNotNull(TEXT("subsystem"), Subsystem))
	{
		return false;
	}
	TestNotNull(TEXT("a default MetaSound is built from the Mumble Voice node"), Subsystem->GetDefaultSound());
	return true;
}

#endif
