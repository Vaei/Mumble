// Copyright (c) Jared Taylor

#include "MumbleComponent.h"

#include "HAL/PlatformTime.h"
#include "MumbleStream.h"
#include "MumbleSubsystem.h"
#include "MumbleVoice.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MumbleComponent)

DEFINE_LOG_CATEGORY_STATIC(LogMumbleComponent, Log, All);

namespace MumbleComponent
{
	const FMumbleVoiceSynth DefaultSynth;
	const FMumbleProsody DefaultProsody;
	const FMumbleTiming DefaultTiming;

	/** A gap longer than this between characters is a pause, not the typing speed */
	constexpr float LongestCharInterval = 0.2f;
}

UMumbleComponent::UMumbleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bAutoActivate = false;
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void UMumbleComponent::SetVoice(UMumbleVoice* NewVoice)
{
	if (Voice == NewVoice)
	{
		return;
	}

	Voice = NewVoice;

	const UMumbleVoice* Effective = GetEffectiveVoice();
	if (Stream.IsValid())
	{
		Stream->SetVoice(Effective ? Effective->Synth : MumbleComponent::DefaultSynth);
	}

	// A voice with its own MetaSound needs the component restarted on it
	if (IsPlaying() && Effective && Effective->Sound && Effective->Sound != Sound)
	{
		Stop();
		if (bLineActive)
		{
			EnsurePlaying();
		}
	}
}

UMumbleVoice* UMumbleComponent::GetEffectiveVoice() const
{
	if (Voice)
	{
		return Voice;
	}
	UMumbleSubsystem* Subsystem = UMumbleSubsystem::Get();
	return Subsystem ? Subsystem->GetDefaultVoice() : nullptr;
}

void UMumbleComponent::EnsureStream()
{
	if (StreamId == 0)
	{
		StreamId = MumbleStreams::Create();
		Stream = MumbleStreams::Find(StreamId);
	}
}

void UMumbleComponent::ReleaseStream()
{
	if (StreamId != 0)
	{
		MumbleStreams::Release(StreamId);
		StreamId = 0;
		Stream.Reset();
	}
}

void UMumbleComponent::EnsurePlaying()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleComponent::EnsurePlaying);

	if (IsPlaying())
	{
		return;
	}

	const UMumbleVoice* Effective = GetEffectiveVoice();
	USoundBase* Desired = Effective && Effective->Sound ? Effective->Sound.Get() : nullptr;
	if (!Desired)
	{
		UMumbleSubsystem* Subsystem = UMumbleSubsystem::Get();
		Desired = Subsystem ? Subsystem->GetDefaultSound() : nullptr;
	}

	if (!Desired)
	{
		UE_LOG(LogMumbleComponent, Warning, TEXT("%s has nothing to speak with: no MetaSound could be found or built."), *GetPathName());
		return;
	}

	if (Sound != Desired)
	{
		SetSound(Desired);
	}
	SetIntParameter(MumbleStreams::ParameterName(), StreamId);
	Play();
}

float UMumbleComponent::Speak(const FString& Text)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleComponent::Speak);

	BeginLine(Text);

	const UMumbleVoice* Effective = GetEffectiveVoice();
	const FMumbleTiming& Timing = Effective ? Effective->Timing : MumbleComponent::DefaultTiming;

	TArray<UTF32CHAR> Chars;
	MumbleText::ToCodepoints(Text, Chars);
	MumbleText::BuildSchedule(Chars, Timing, Schedule);

	if (Schedule.Num() == 0)
	{
		FinishSelfTimed();
		return 0.f;
	}

	const float Speed = FMath::Max(SpeedScale, 0.1f);
	bSelfTimed = true;
	SelfTimedStart = FPlatformTime::Seconds();
	CharInterval = 1.f / (FMath::Max(Timing.CharactersPerSecond, 1.f) * Speed);

	TickComponent(0.f, LEVELTICK_All, nullptr);
	return Schedule.Last() / Speed;
}

void UMumbleComponent::Skip()
{
	if (!bLineActive)
	{
		return;
	}

	const bool bWasSelfTimed = bSelfTimed;
	if (bWasSelfTimed && Line.NumChars > 0 && LastRevealed < Line.NumChars - 1)
	{
		OnCharacterRevealed.Broadcast(Line.NumChars - 1);
	}

	EndLine(true);

	if (bWasSelfTimed)
	{
		OnLineFinished.Broadcast();
	}
}

void UMumbleComponent::BeginLine(const FString& Text)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleComponent::BeginLine);

	EnsureStream();

	const UMumbleVoice* Effective = GetEffectiveVoice();
	const FMumbleProsody& Prosody = Effective ? Effective->Prosody : MumbleComponent::DefaultProsody;
	const uint32 Seed = Effective ? uint32(Effective->Seed) : 0u;

	Line = FMumbleLine::Analyze(Text, Prosody, Seed);
	NextSyllable = 0;
	LastRevealed = INDEX_NONE;
	bLineActive = true;
	bSelfTimed = false;
	LastRevealTime = FPlatformTime::Seconds();
	LingerUntil = 0.0;

	Stream->Reset();
	Stream->SetVoice(Effective ? Effective->Synth : MumbleComponent::DefaultSynth);

	EnsurePlaying();
	SetComponentTickEnabled(true);
}

void UMumbleComponent::RevealTo(int32 CharIndex)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleComponent::RevealTo);

	if (!bLineActive || CharIndex <= LastRevealed)
	{
		return;
	}

	const double Now = FPlatformTime::Seconds();
	if (!bSelfTimed && LastRevealed != INDEX_NONE)
	{
		const float PerChar = float(Now - LastRevealTime) / float(CharIndex - LastRevealed);
		if (PerChar > 0.002f && PerChar < MumbleComponent::LongestCharInterval)
		{
			CharInterval = FMath::Lerp(CharInterval, PerChar, 0.3f);
		}
	}
	LastRevealTime = Now;

	// Several syllables due at once means the text outran the frame: only the newest is worth saying
	int32 Due = INDEX_NONE;
	while (Line.Syllables.IsValidIndex(NextSyllable) && Line.Syllables[NextSyllable].TriggerChar <= CharIndex)
	{
		Due = NextSyllable++;
	}

	LastRevealed = CharIndex;

	if (Due != INDEX_NONE)
	{
		Emit(Line.Syllables[Due]);
	}
}

void UMumbleComponent::Emit(const FMumblePlannedSyllable& Planned)
{
	if (!Stream.IsValid())
	{
		return;
	}

	const UMumbleVoice* Effective = GetEffectiveVoice();
	const FMumbleVoiceSynth& Synth = Effective ? Effective->Synth : MumbleComponent::DefaultSynth;
	const FMumbleProsody& Prosody = Effective ? Effective->Prosody : MumbleComponent::DefaultProsody;

	Stream->Push(FMumbleLine::Realise(Planned, Synth, Prosody, CharInterval));
}

void UMumbleComponent::EndLine(bool bSkipped)
{
	if (!bLineActive)
	{
		return;
	}

	if (bSkipped && Stream.IsValid())
	{
		Stream->Silence();
	}

	bLineActive = false;
	bSelfTimed = false;
	LingerUntil = FPlatformTime::Seconds() + LingerTime + 0.5;
}

void UMumbleComponent::FinishSelfTimed()
{
	EndLine(false);
	OnLineFinished.Broadcast();
}

void UMumbleComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleComponent::TickComponent);

	if (ThisTickFunction)
	{
		Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	}

	const double Now = FPlatformTime::Seconds();

	if (bLineActive && bSelfTimed)
	{
		const float Elapsed = float(Now - SelfTimedStart) * FMath::Max(SpeedScale, 0.1f);

		int32 Reached = LastRevealed;
		while (Reached + 1 < Schedule.Num() && Schedule[Reached + 1] <= Elapsed)
		{
			++Reached;
		}

		if (Reached > LastRevealed)
		{
			RevealTo(Reached);
			OnCharacterRevealed.Broadcast(Reached);
		}

		if (Reached >= Schedule.Num() - 1)
		{
			FinishSelfTimed();
		}
	}

	if (!bLineActive && LingerUntil > 0.0 && Now >= LingerUntil)
	{
		LingerUntil = 0.0;
		Stop();
		SetComponentTickEnabled(false);
	}
}

void UMumbleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	EndLine(true);
	Stop();
	ReleaseStream();
	Super::EndPlay(EndPlayReason);
}

void UMumbleComponent::BeginDestroy()
{
	ReleaseStream();
	Super::BeginDestroy();
}
