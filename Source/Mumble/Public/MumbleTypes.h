// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "MumbleTypes.generated.h"

/** The vowels a syllable can be built on. Formants for each live in the synth. */
enum class EMumbleVowel : uint8
{
	IY,		// beet
	IH,		// bit
	EY,		// bait
	EH,		// bet
	AE,		// bat
	AA,		// father
	AO,		// bought
	OW,		// boat
	UH,		// book
	UW,		// boot
	AH,		// but
	ER,		// bird
	AX,		// the schwa in "about"
	Num
};

/** The consonants a syllable can open or close on. */
enum class EMumbleConsonant : uint8
{
	None,
	P, B, T, D, K, G,
	F, V, TH, DH, S, Z, SH, ZH, H,
	CH, J,
	M, N, NG,
	L, R, W, Y,
	Num
};

/**
 * One syllable, as the synth plays it.
 *
 * Plain data so it can cross to the audio render thread by copy.
 */
struct FMumbleSyllable
{
	EMumbleConsonant Onset[2] = { EMumbleConsonant::None, EMumbleConsonant::None };
	EMumbleConsonant Coda = EMumbleConsonant::None;

	/** Where the vowel starts and where it glides to. The same twice for a pure vowel. */
	EMumbleVowel VowelStart = EMumbleVowel::AX;
	EMumbleVowel VowelEnd = EMumbleVowel::AX;

	float PitchStartHz = 120.f;
	float PitchEndHz = 120.f;

	/** Seconds the vowel is held, not counting the consonants either side of it */
	float Duration = 0.12f;

	/** 1 is a normal syllable */
	float Loudness = 1.f;

	/** 0 is relaxed, 1 is a shout: a tenser, brighter source */
	float Effort = 0.f;

	/** How far the vowel is pulled toward the schwa, as an unstressed syllable is */
	float Reduction = 0.f;

	/** How much of the voice's creak and fall this syllable lets go with. 1 ends a statement, 0 a question. */
	float Creak = 0.f;

	/** The last syllable before a pause, which lets go slowly */
	bool bPhraseFinal = false;
};

/**
 * How a voice sounds: the throat, the mouth, and how steady they are.
 *
 * The source is a glottal pulse rather than a fixed waveform, so its shape is a set of muscles
 * rather than a choice of oscillator: Open Quotient, Skew and Brightness move it from a pressed
 * buzz to a soft breathy hum, and Subharmonics splits every other pulse for a rough, gravelly one.
 */
USTRUCT(BlueprintType)
struct MUMBLE_API FMumbleVoiceSynth
{
	GENERATED_BODY()

	/** The speaking pitch. Roughly 100 for a man, 200 for a woman, 280 for a child. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "40", ClampMax = "600", UIMin = "60", UIMax = "450", Units = "Hz"))
	float PitchHz = 115.f;

	/**
	 * Length of the vocal tract, 1 being an adult man.
	 *
	 * Pitch alone pitches a man up into a strained man. A smaller tract moves every formant up
	 * together, which is what actually makes a different person: about 0.85 for a woman, 0.7 for
	 * a child, above 1 for someone large.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "0.5", ClampMax = "1.5", UIMin = "0.6", UIMax = "1.3"))
	float TractSize = 1.f;

	/**
	 * Fraction of each pulse the folds spend open.
	 *
	 * Low is pressed and buzzy, high is soft and breathy.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "0.3", ClampMax = "0.95"))
	float OpenQuotient = 0.6f;

	/** How much faster the folds close than they open. Higher is a harder, more nasal edge. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "1.0", ClampMax = "5.0"))
	float Skew = 2.5f;

	/** How abruptly each pulse ends. Low is dark and muffled, high is bright and cutting. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "0", ClampMax = "1"))
	float Brightness = 0.5f;

	/** Breath mixed into the voice, pulsed with it the way real aspiration is */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "0", ClampMax = "1"))
	float Breathiness = 0.15f;

	/** Every other pulse weaker and later, which drops a rough octave under the voice. Gravel, a growl. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "0", ClampMax = "1"))
	float Subharmonics = 0.f;

	/** Vocal fry as a phrase trails off */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Throat, meta = (ClampMin = "0", ClampMax = "1"))
	float Creak = 0.25f;

	/** Pulse-to-pulse pitch irregularity. A little stops it sounding like a machine; a lot is hoarse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Steadiness, meta = (ClampMin = "0", ClampMax = "0.1"))
	float Jitter = 0.012f;

	/** Pulse-to-pulse loudness irregularity */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Steadiness, meta = (ClampMin = "0", ClampMax = "0.5"))
	float Shimmer = 0.08f;

	/**
	 * A slow, steady wobble in pitch.
	 *
	 * What reads as old is not depth or randomness, it is this: around 4 to 6 times a second and
	 * half a semitone deep. Zero for everyone else.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Steadiness, meta = (ClampMin = "0", ClampMax = "12", Units = "Hz"))
	float WobbleRate = 0.f;

	/** Depth of the wobble in semitones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Steadiness, meta = (ClampMin = "0", ClampMax = "3"))
	float WobbleDepth = 0.f;

	/**
	 * How far the jaw opens.
	 *
	 * 1 says every vowel fully. Lower pulls them toward one mumbled middle vowel, which is a
	 * mutter rather than a quieter voice.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mouth, meta = (ClampMin = "0", ClampMax = "1"))
	float MouthOpenness = 0.85f;

	/** Sound let through the nose on every vowel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mouth, meta = (ClampMin = "0", ClampMax = "1"))
	float Nasality = 0.1f;

	/**
	 * How clearly consonants are said.
	 *
	 * Low is a soft, cute mumble where consonants are only dips and glides between vowels. High is
	 * crisp gibberish that sounds like a language you do not speak.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mouth, meta = (ClampMin = "0", ClampMax = "1"))
	float Articulation = 0.4f;

	/** Ceiling on the hiss of s, sh and f. Lower keeps them soft instead of clicky. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mouth, meta = (ClampMin = "1000", ClampMax = "12000", Units = "Hz"))
	float ConsonantCutoff = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Output, meta = (ClampMin = "0", ClampMax = "4"))
	float Gain = 1.f;
};

/**
 * How a voice moves through a line: its tune.
 *
 * Read off the punctuation and capitals already in the text, so no line needs authoring.
 */
USTRUCT(BlueprintType)
struct MUMBLE_API FMumbleProsody
{
	GENERATED_BODY()

	/**
	 * Size of the pitch accent on stressed words, in semitones.
	 *
	 * Small is flat and deadpan, large is animated.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0", ClampMax = "12"))
	float PitchRange = 3.f;

	/** How far pitch drifts down across a phrase before the next one resets it, in semitones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0", ClampMax = "12"))
	float Declination = 2.5f;

	/** Rise on the end of a question, in semitones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0", ClampMax = "12"))
	float QuestionRise = 5.f;

	/** How much higher an exclamation starts, in semitones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0", ClampMax = "12"))
	float ExclamationLift = 3.f;

	/** How much higher a word in capitals is, in semitones */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0", ClampMax = "12"))
	float ShoutLift = 4.f;

	/**
	 * Scales everything punctuation does to the tune.
	 *
	 * 0 ignores it, 1 is natural, 2 is theatrical.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0", ClampMax = "3"))
	float Expressiveness = 1.f;

	/**
	 * A small nudge to each syllable's pitch, in semitones.
	 *
	 * Seeded from the line, so the same line always says itself the same way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0", ClampMax = "4"))
	float Randomness = 0.6f;

	/** How long each syllable is held relative to the gap before the next. Above 1 they run together. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Prosody, meta = (ClampMin = "0.3", ClampMax = "2.5"))
	float SyllableLength = 1.15f;
};

/**
 * Pacing for a line Mumble types out itself.
 *
 * Ignored by a line another typewriter drives: that one decides when each character arrives
 * and Mumble only follows.
 */
USTRUCT(BlueprintType)
struct MUMBLE_API FMumbleTiming
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Timing, meta = (ClampMin = "1", ClampMax = "200"))
	float CharactersPerSecond = 32.f;

	/** Extra wait after a comma, semicolon or colon */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Timing, meta = (ClampMin = "0", Units = "s"))
	float CommaPause = 0.07f;

	/** Extra wait after a full stop */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Timing, meta = (ClampMin = "0", Units = "s"))
	float PeriodPause = 0.18f;

	/** Extra wait after a question mark */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Timing, meta = (ClampMin = "0", Units = "s"))
	float QuestionPause = 0.18f;

	/** Extra wait after an exclamation mark */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Timing, meta = (ClampMin = "0", Units = "s"))
	float ExclamationPause = 0.18f;

	/** Extra wait after an ellipsis, as a whole rather than per dot */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Timing, meta = (ClampMin = "0", Units = "s"))
	float EllipsisPause = 0.35f;

	/** Extra wait after a dash */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Timing, meta = (ClampMin = "0", Units = "s"))
	float DashPause = 0.1f;
};
