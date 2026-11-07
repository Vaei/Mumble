// Copyright (c) Jared Taylor

#include "MumbleStream.h"

#include "Misc/ScopeLock.h"

void FMumbleStream::SetVoice(const FMumbleVoiceSynth& InVoice)
{
	FScopeLock ScopeLock(&Lock);
	Voice = InVoice;
	++VoiceSerial;
}

void FMumbleStream::Push(const FMumbleSyllable& Syllable)
{
	FScopeLock ScopeLock(&Lock);
	Pending.Add(Syllable);
}

void FMumbleStream::Silence()
{
	FScopeLock ScopeLock(&Lock);
	Pending.Reset();
	++SilenceSerial;
}

void FMumbleStream::Reset()
{
	FScopeLock ScopeLock(&Lock);
	Pending.Reset();
}

bool FMumbleStream::Drain(TArray<FMumbleSyllable>& OutSyllables, FMumbleVoiceSynth& OutVoice, uint32& InOutVoiceSerial, uint32& InOutSilenceSerial)
{
	FScopeLock ScopeLock(&Lock);

	if (InOutVoiceSerial != VoiceSerial)
	{
		OutVoice = Voice;
		InOutVoiceSerial = VoiceSerial;
	}

	OutSyllables.Reset();
	if (Pending.Num() > 0)
	{
		// Swapped rather than copied, so neither side allocates once both arrays have grown
		Swap(OutSyllables, Pending);
	}

	const bool bSilence = InOutSilenceSerial != SilenceSerial;
	InOutSilenceSerial = SilenceSerial;
	return bSilence;
}

namespace MumbleStreams
{
	namespace
	{
		FCriticalSection RegistryLock;
		TMap<int32, FMumbleStreamRef> Registry;
		int32 NextId = 1;
	}

	int32 Create()
	{
		FScopeLock ScopeLock(&RegistryLock);
		const int32 Id = NextId++;
		if (NextId <= 0)
		{
			NextId = 1;
		}
		Registry.Add(Id, MakeShared<FMumbleStream, ESPMode::ThreadSafe>());
		return Id;
	}

	void Release(int32 Id)
	{
		FScopeLock ScopeLock(&RegistryLock);
		Registry.Remove(Id);
	}

	FMumbleStreamPtr Find(int32 Id)
	{
		FScopeLock ScopeLock(&RegistryLock);
		const FMumbleStreamRef* Found = Registry.Find(Id);
		return Found ? FMumbleStreamPtr(*Found) : FMumbleStreamPtr();
	}

	const FName& ParameterName()
	{
		static const FName Name = TEXT("Mumble Stream");
		return Name;
	}
}
