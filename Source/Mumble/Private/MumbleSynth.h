// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "MumbleTypes.h"

/**
 * One voice: a glottal source through a vocal tract.
 *
 * The source is a Rosenberg pulse with an exponential return phase, so its shape and its spectral
 * tilt come from the voice's Open Quotient, Skew and Brightness rather than from a fixed waveform.
 * Breath is noise gated by the glottal flow, and consonant noise goes through the same tract or
 * through a place-of-articulation band and a low-pass, never raw.
 *
 * The tract is one cascade of Klatt resonators that glides from target to target. A syllable is
 * a timeline of those targets (closure, burst, aspiration, vowel, coda, release), and a new
 * syllable replaces the timeline from wherever the old one had got to. Nothing is retriggered,
 * so consecutive syllables are one continuous voice rather than a string of grains.
 *
 * Owns no UObjects and allocates nothing after Init. Safe on the audio render thread.
 */
class FMumbleSynth
{
public:
	void Init(float InSampleRate, uint32 Seed);
	void SetVoice(const FMumbleVoiceSynth& InVoice) { Voice = InVoice; }
	void Speak(const FMumbleSyllable& Syllable);

	/** Lets go of whatever is sounding, quickly */
	void Silence();

	/** Writes, never adds */
	void Render(float* Out, int32 NumFrames);

	/** True once nothing is sounding and nothing is due, so the caller can skip rendering */
	bool IsIdle() const;

	/** Every voice's formant frequencies for an adult man, before the tract is scaled */
	static void GetVowelFormants(EMumbleVowel Vowel, float OutF[3]);

private:
	struct FSegment
	{
		float Duration = 0.f;
		float Voice = 0.f;
		float Aspiration = 0.f;
		float Frication = 0.f;
		float FricationHz = 4000.f;
		float FricationQ = 1.f;
		/** Fraction of the frication that goes through the tract rather than around it */
		float FricationTract = 0.f;
		float Nasal = 0.f;
		/** Formant targets, used unless bVowel */
		float F[3] = { 500.f, 1500.f, 2500.f };
		float BandwidthScale = 1.f;
		float FormantTau = 0.03f;
		float AmpTau = 0.012f;
		/** Formants follow the syllable's vowel, gliding start to end across the segment */
		bool bVowel = false;
		/** Pitch glides start to end across the segment; otherwise it holds the start */
		bool bNucleus = false;
		/** The release of a phrase, which creaks and falls */
		bool bFinalRelease = false;
	};

	static constexpr int32 MaxSegments = 12;
	static constexpr int32 NumFormants = 5;
	static constexpr int32 ControlBlock = 32;

	/** @return How quickly the vowel after it should arrive, in seconds */
	float AddConsonant(EMumbleConsonant Consonant, bool bOnset, float TempoScale, const float VowelF[3]);
	FSegment& AddSegment();
	void VowelTargets(EMumbleVowel Vowel, float Reduction, float OutF[3]) const;
	void UpdateControl();
	float NextGlottalSample(float& OutFlow);
	void StartPulse();
	float Random();
	float RandomGaussian();

	FMumbleVoiceSynth Voice;
	FMumbleSyllable Syllable;

	FSegment Timeline[MaxSegments];
	int32 NumSegments = 0;
	int32 SegmentIndex = 0;
	int32 NucleusIndex = 0;
	float SegmentTime = 0.f;
	bool bIdle = true;

	float SampleRate = 48000.f;
	int32 ControlCountdown = 0;
	uint32 RandomState = 1;

	// Smoothed control state
	float Formants[NumFormants] = {};
	float Bandwidths[NumFormants] = {};
	float LogPitch = 0.f;
	float CreakAmount = 0.f;
	float EffortNow = 0.f;

	// Per-sample amplitude smoothing: current, target, coefficient
	float VoiceAmp = 0.f, VoiceTarget = 0.f;
	float AspAmp = 0.f, AspTarget = 0.f;
	float FricAmp = 0.f, FricTarget = 0.f;
	float NasalAmp = 0.f, NasalTarget = 0.f;
	float AmpCoeff = 0.f;
	float FricTract = 0.f;

	// Glottal oscillator
	float Phase = 0.f;
	float PeriodScale = 1.f;
	float PulseAmp = 1.f;
	bool bOddPulse = false;
	float WobblePhase = 0.f;
	float Drift = 0.f;
	float DriftTarget = 0.f;
	float DriftCountdown = 0.f;
	float Tp = 0.36f, Tn = 0.14f, Te = 0.5f, Ta = 0.02f, ReturnEnd = 0.f;

	// Klatt resonators, cascade
	float ResA[NumFormants] = {}, ResB[NumFormants] = {}, ResC[NumFormants] = {};
	float ResY1[NumFormants] = {}, ResY2[NumFormants] = {};

	// Nasal murmur, parallel
	float NasA = 0.f, NasB = 0.f, NasC = 0.f, NasY1 = 0.f, NasY2 = 0.f;

	// Frication band, a TPT state variable filter, then two one-pole low-passes
	float SvfA1 = 0.f, SvfA2 = 0.f, SvfA3 = 0.f, SvfK = 1.f, SvfIc1 = 0.f, SvfIc2 = 0.f;
	float FricLpCoeff = 0.f, FricLp = 0.f, FricLp2 = 0.f;

	// Output DC blocker
	float DcX1 = 0.f, DcY1 = 0.f;

	float SilentFor = 0.f;
};
