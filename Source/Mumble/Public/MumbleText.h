// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "MumbleTypes.h"

/** One syllable of a line, and the character that sets it off. */
struct FMumblePlannedSyllable
{
	/** The character whose arrival starts this syllable, counted in code points */
	int32 TriggerChar = 0;

	/** Characters until the next syllable or the end of the phrase, which sets how long it is held */
	int32 CharSpan = 1;

	/** Pitches here are semitones from the voice's pitch, not Hz */
	FMumbleSyllable Sound;

	/** Stress and phrase-final lengthening */
	float DurationScale = 1.f;
};

/**
 * A line read for its sound: which characters start a syllable, what each syllable is, and the tune.
 *
 * English is read with spelling rules (vowel teams, silent and magic e, r-coloured vowels, word
 * stress, reduced function words). Kana are read as the syllables they are. Anything else, kanji
 * included, makes a syllable chosen by the character, so the same character always sounds the same.
 *
 * Every character index counts code points, which is how Folio counts a `<text>`.
 */
struct MUMBLE_API FMumbleLine
{
	TArray<FMumblePlannedSyllable> Syllables;
	int32 NumChars = 0;

	/** @param Seed Mixed with the text, so two voices saying one line still differ */
	static FMumbleLine Analyze(const FString& Text, const FMumbleProsody& Prosody, uint32 Seed);

	/**
	 * Turns a planned syllable into one the synth can play, in this voice at this typing speed.
	 *
	 * @param CharInterval  Seconds per character the text is arriving at
	 */
	static FMumbleSyllable Realise(const FMumblePlannedSyllable& Planned, const FMumbleVoiceSynth& Synth, const FMumbleProsody& Prosody, float CharInterval);
};

namespace MumbleText
{
	/** The code points of a string, with surrogate pairs joined */
	MUMBLE_API void ToCodepoints(const FString& Text, TArray<UTF32CHAR>& Out);

	/**
	 * When each character of a line is due, in seconds from the start, for a line Mumble types itself.
	 *
	 * The pause for a punctuation mark lands on the character after it, and only where the mark
	 * ends something: "3.5" and "e.g." do not pause mid-token, and an ellipsis pauses once.
	 */
	MUMBLE_API void BuildSchedule(TConstArrayView<UTF32CHAR> Chars, const FMumbleTiming& Timing, TArray<float>& OutTimes);
}
