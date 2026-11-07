// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "MumbleTypes.h"

/**
 * What one speaker hands to the MetaSound voicing it: the voice, and syllables as they are due.
 *
 * A side channel rather than MetaSound parameters because a syllable is a dozen values that must
 * arrive together, and two syllables set in the same frame would overwrite each other as
 * parameters. The graph only carries the stream's id, as the "Mumble Stream" Int32 input.
 *
 * Written on the game thread, read on the audio render thread. The lock is held for a copy at most.
 */
class MUMBLE_API FMumbleStream
{
public:
	void SetVoice(const FMumbleVoiceSynth& InVoice);
	void Push(const FMumbleSyllable& Syllable);

	/** Lets go of whatever is sounding, quickly. What skipping a line does. */
	void Silence();

	/** Drops anything not yet played, for a stream about to be reused */
	void Reset();

	/**
	 * Takes everything waiting, for the audio thread.
	 *
	 * @param InOutVoiceSerial  The serial of the voice the caller already has; OutVoice is only written if it changed
	 * @param InOutSilenceSerial  Same, for silence requests
	 * @return True if a silence was requested since the caller last asked
	 */
	bool Drain(TArray<FMumbleSyllable>& OutSyllables, FMumbleVoiceSynth& OutVoice, uint32& InOutVoiceSerial, uint32& InOutSilenceSerial);

private:
	FCriticalSection Lock;
	TArray<FMumbleSyllable> Pending;
	FMumbleVoiceSynth Voice;
	uint32 VoiceSerial = 0;
	uint32 SilenceSerial = 0;
};

using FMumbleStreamRef = TSharedRef<FMumbleStream, ESPMode::ThreadSafe>;
using FMumbleStreamPtr = TSharedPtr<FMumbleStream, ESPMode::ThreadSafe>;

/** Every live stream, by the id a MetaSound is given. */
namespace MumbleStreams
{
	/** @return A new stream's id, never zero */
	MUMBLE_API int32 Create();

	MUMBLE_API void Release(int32 Id);

	MUMBLE_API FMumbleStreamPtr Find(int32 Id);

	/** Name of the Int32 input a Mumble MetaSound reads its stream id from */
	MUMBLE_API const FName& ParameterName();
}
