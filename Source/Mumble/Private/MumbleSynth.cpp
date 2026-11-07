// Copyright (c) Jared Taylor

#include "MumbleSynth.h"

#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace MumbleSynth
{
	enum class EKind : uint8 { None, Stop, Fricative, Aspirate, Affricate, Nasal, Approximant };

	struct FConsonantInfo
	{
		EKind Kind;
		bool bVoiced;
		/** Centre and Q of the noise band, for the consonants that hiss or burst */
		float Hz;
		float Q;
		float Gain;
		/** Fraction of the noise sent through the vocal tract rather than straight out */
		float Tract;
		/** Where the tract is at the consonant */
		float F1, F2, F3;
		/** How far a neighbouring vowel's formants are pulled toward F1-F3 */
		float Locus;
	};

	using enum EKind;

	/** In EMumbleConsonant order. Formants are an adult man's. */
	const FConsonantInfo Consonants[] =
	{
		{ None,			false,	0.f,	0.f,	0.f,	0.f,	0.f,	0.f,	0.f,	0.f },
		{ Stop,			false,	1100.f,	0.7f,	0.5f,	0.2f,	250.f,	800.f,	2200.f,	0.5f },		// P
		{ Stop,			true,	900.f,	0.7f,	0.3f,	0.2f,	250.f,	800.f,	2200.f,	0.5f },		// B
		{ Stop,			false,	4200.f,	1.5f,	0.7f,	0.05f,	250.f,	1750.f,	2700.f,	0.5f },		// T
		{ Stop,			true,	3600.f,	1.5f,	0.45f,	0.05f,	250.f,	1700.f,	2650.f,	0.5f },		// D
		{ Stop,			false,	2200.f,	2.5f,	0.7f,	0.3f,	250.f,	2000.f,	2300.f,	0.4f },		// K
		{ Stop,			true,	1900.f,	2.5f,	0.45f,	0.3f,	250.f,	1900.f,	2300.f,	0.4f },		// G
		{ Fricative,	false,	7000.f,	0.6f,	0.3f,	0.25f,	300.f,	900.f,	2200.f,	0.5f },		// F
		{ Fricative,	true,	6000.f,	0.6f,	0.15f,	0.25f,	300.f,	900.f,	2200.f,	0.5f },		// V
		{ Fricative,	false,	6000.f,	0.6f,	0.25f,	0.25f,	300.f,	1400.f,	2600.f,	0.5f },		// TH
		{ Fricative,	true,	5000.f,	0.6f,	0.1f,	0.25f,	300.f,	1400.f,	2600.f,	0.5f },		// DH
		{ Fricative,	false,	6200.f,	3.0f,	0.8f,	0.05f,	300.f,	1700.f,	2700.f,	0.5f },		// S
		{ Fricative,	true,	5800.f,	3.0f,	0.5f,	0.05f,	300.f,	1700.f,	2700.f,	0.5f },		// Z
		{ Fricative,	false,	3000.f,	2.0f,	0.8f,	0.25f,	300.f,	1900.f,	2400.f,	0.5f },		// SH
		{ Fricative,	true,	2900.f,	2.0f,	0.5f,	0.25f,	300.f,	1900.f,	2400.f,	0.5f },		// ZH
		{ Aspirate,		false,	0.f,	0.f,	0.6f,	1.f,	0.f,	0.f,	0.f,	0.f },		// H
		{ Affricate,	false,	3000.f,	2.0f,	0.8f,	0.25f,	250.f,	1900.f,	2400.f,	0.5f },		// CH
		{ Affricate,	true,	2900.f,	2.0f,	0.5f,	0.25f,	250.f,	1900.f,	2400.f,	0.5f },		// J
		{ Nasal,		true,	0.f,	0.f,	0.f,	0.f,	280.f,	1000.f,	2200.f,	0.85f },	// M
		{ Nasal,		true,	0.f,	0.f,	0.f,	0.f,	280.f,	1500.f,	2500.f,	0.85f },	// N
		{ Nasal,		true,	0.f,	0.f,	0.f,	0.f,	280.f,	2000.f,	2500.f,	0.85f },	// NG
		{ Approximant,	true,	0.f,	0.f,	0.f,	0.f,	360.f,	1050.f,	2900.f,	0.85f },	// L
		{ Approximant,	true,	0.f,	0.f,	0.f,	0.f,	420.f,	1250.f,	1650.f,	0.85f },	// R
		{ Approximant,	true,	0.f,	0.f,	0.f,	0.f,	300.f,	650.f,	2300.f,	0.9f },		// W
		{ Approximant,	true,	0.f,	0.f,	0.f,	0.f,	270.f,	2250.f,	3000.f,	0.9f },		// Y
	};
	static_assert(UE_ARRAY_COUNT(Consonants) == int32(EMumbleConsonant::Num));

	/** Adult male formants, in EMumbleVowel order (Peterson & Barney, Hillenbrand) */
	const float Vowels[][3] =
	{
		{ 300.f,	2300.f,	3000.f },	// IY
		{ 420.f,	2000.f,	2650.f },	// IH
		{ 480.f,	2050.f,	2650.f },	// EY
		{ 580.f,	1800.f,	2600.f },	// EH
		{ 660.f,	1720.f,	2410.f },	// AE
		{ 750.f,	1200.f,	2500.f },	// AA
		{ 620.f,	950.f,	2550.f },	// AO
		{ 500.f,	900.f,	2450.f },	// OW
		{ 450.f,	1100.f,	2350.f },	// UH
		{ 330.f,	950.f,	2300.f },	// UW
		{ 640.f,	1200.f,	2450.f },	// AH
		{ 480.f,	1350.f,	1700.f },	// ER
		{ 520.f,	1450.f,	2500.f },	// AX
	};
	static_assert(UE_ARRAY_COUNT(Vowels) == int32(EMumbleVowel::Num));

	constexpr float BaseBandwidths[] = { 70.f, 95.f, 150.f, 220.f, 300.f };
	constexpr float F4 = 3500.f;
	constexpr float F5 = 4500.f;

	/** Puts ordinary speech near -20 dBFS RMS at unit gain, which leaves a shout its headroom */
	constexpr float OutputGain = 0.2f;

	/** Below this every amplitude is treated as off */
	constexpr float Inaudible = 1e-5f;

	FORCEINLINE float SoftClip(float X)
	{
		X = FMath::Clamp(X, -3.f, 3.f);
		return X * (27.f + X * X) / (27.f + 9.f * X * X);
	}

	FORCEINLINE float SmoothStep(float T)
	{
		T = FMath::Clamp(T, 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}

	FORCEINLINE void KlattCoefficients(float Hz, float Bandwidth, float SampleRate, float& A, float& B, float& C)
	{
		const float T = 1.f / SampleRate;
		Hz = FMath::Clamp(Hz, 50.f, SampleRate * 0.45f);
		Bandwidth = FMath::Max(Bandwidth, 20.f);
		C = -FMath::Exp(-2.f * UE_PI * Bandwidth * T);
		B = 2.f * FMath::Exp(-UE_PI * Bandwidth * T) * FMath::Cos(2.f * UE_PI * Hz * T);
		A = 1.f - B - C;
	}
}

void FMumbleSynth::Init(float InSampleRate, uint32 Seed)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleSynth::Init);

	SampleRate = FMath::Max(InSampleRate, 8000.f);
	RandomState = Seed != 0 ? Seed : 0x9E3779B9u;

	const float Scale = 1.f / FMath::Max(Voice.TractSize, 0.1f);
	const float* Schwa = MumbleSynth::Vowels[int32(EMumbleVowel::AX)];
	Formants[0] = Schwa[0] * Scale;
	Formants[1] = Schwa[1] * Scale;
	Formants[2] = Schwa[2] * Scale;
	Formants[3] = MumbleSynth::F4 * Scale;
	Formants[4] = MumbleSynth::F5 * Scale;
	for (int32 Index = 0; Index < NumFormants; ++Index)
	{
		Bandwidths[Index] = MumbleSynth::BaseBandwidths[Index];
	}

	LogPitch = FMath::Log2(FMath::Max(Voice.PitchHz, 40.f));
	bIdle = true;
	NumSegments = 0;
	ControlCountdown = 0;
	StartPulse();
}

void FMumbleSynth::GetVowelFormants(EMumbleVowel Vowel, float OutF[3])
{
	const float* F = MumbleSynth::Vowels[FMath::Clamp(int32(Vowel), 0, int32(EMumbleVowel::Num) - 1)];
	OutF[0] = F[0];
	OutF[1] = F[1];
	OutF[2] = F[2];
}

void FMumbleSynth::VowelTargets(EMumbleVowel Vowel, float Reduction, float OutF[3]) const
{
	GetVowelFormants(Vowel, OutF);

	// A closed jaw and an unstressed syllable both pull a vowel toward the middle of the mouth
	const float* Schwa = MumbleSynth::Vowels[int32(EMumbleVowel::AX)];
	const float Closed = 1.f - FMath::Clamp(Voice.MouthOpenness, 0.f, 1.f);
	const float Centralise = FMath::Clamp(Reduction + Closed * 0.6f, 0.f, 0.9f);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		OutF[Index] += (Schwa[Index] - OutF[Index]) * Centralise;
	}
	OutF[0] *= 1.f - 0.2f * Closed;
}

FMumbleSynth::FSegment& FMumbleSynth::AddSegment()
{
	check(NumSegments < MaxSegments);
	FSegment& Segment = Timeline[NumSegments++];
	Segment = FSegment();
	return Segment;
}

float FMumbleSynth::AddConsonant(EMumbleConsonant Consonant, bool bOnset, float TempoScale, const float VowelF[3])
{
	using namespace MumbleSynth;

	constexpr float DefaultTau = 0.035f;
	if (Consonant == EMumbleConsonant::None || int32(Consonant) >= int32(EMumbleConsonant::Num))
	{
		return DefaultTau;
	}

	const FConsonantInfo& Info = Consonants[int32(Consonant)];
	const float A = FMath::Clamp(Voice.Articulation, 0.f, 1.f);
	const float Slack = 1.f - A;
	const float Speed = TempoScale * (0.6f + 0.4f * A) * (bOnset ? 1.f : 0.8f);

	auto Locus = [&Info, VowelF](FSegment& Segment, float Pull)
	{
		const float Target[3] = { Info.F1, Info.F2, Info.F3 };
		for (int32 Index = 0; Index < 3; ++Index)
		{
			Segment.F[Index] = VowelF[Index] + (Target[Index] - VowelF[Index]) * Pull;
		}
	};

	auto Noise = [&Info, A](FSegment& Segment, float Gain)
	{
		// Squared, so a mumble's consonants are a breath and only a crisp voice's are a hiss
		Segment.Frication = Info.Gain * Gain * (0.05f + 0.95f * A * A);
		Segment.FricationHz = Info.Hz;
		Segment.FricationQ = Info.Q;
		Segment.FricationTract = Info.Tract;
	};

	// A mumble never closes all the way: low articulation leaves voice running through the stops
	const float ObstruentPull = Info.Locus * (0.5f + 0.5f * A);

	switch (Info.Kind)
	{
	case Stop:
	case Affricate:
	{
		FSegment& Closure = AddSegment();
		Closure.Duration = (Info.bVoiced ? 0.035f : 0.045f) * Speed * (Info.Kind == Affricate ? 0.7f : 1.f);
		Closure.Voice = Info.bVoiced ? 0.15f + 0.25f * Slack : 0.45f * Slack;
		Closure.FormantTau = 0.015f;
		Closure.AmpTau = 0.008f;
		Locus(Closure, ObstruentPull);

		if (Info.Kind == Affricate)
		{
			FSegment& Frication = AddSegment();
			Frication.Duration = 0.045f * Speed;
			Frication.Voice = Info.bVoiced ? 0.45f : 0.3f * Slack;
			Frication.FormantTau = 0.02f;
			Frication.AmpTau = 0.004f;
			Locus(Frication, ObstruentPull);
			Noise(Frication, 1.f);
			return 0.03f;
		}

		// A coda stop is unreleased unless the speaker is enunciating
		if (bOnset || A > 0.6f)
		{
			FSegment& Burst = AddSegment();
			Burst.Duration = 0.008f;
			Burst.Voice = Closure.Voice;
			Burst.FormantTau = 0.015f;
			Burst.AmpTau = 0.003f;
			Locus(Burst, ObstruentPull);
			Noise(Burst, bOnset ? 1.f : 0.5f);
		}

		if (bOnset && !Info.bVoiced)
		{
			FSegment& Aspiration = AddSegment();
			Aspiration.Duration = 0.03f * Speed * (0.4f + 0.6f * A);
			Aspiration.Aspiration = 0.35f * (0.3f + 0.7f * A);
			Aspiration.Voice = 0.3f * Slack;
			Aspiration.bVowel = true;
			Aspiration.FormantTau = 0.02f;
			Aspiration.AmpTau = 0.004f;
		}
		return 0.03f;
	}

	case Fricative:
	{
		const bool bSibilant = Consonant == EMumbleConsonant::S || Consonant == EMumbleConsonant::SH
			|| Consonant == EMumbleConsonant::Z || Consonant == EMumbleConsonant::ZH;
		const float Base = Consonant == EMumbleConsonant::DH ? 0.035f : (bSibilant ? 0.075f : 0.06f);

		FSegment& Frication = AddSegment();
		Frication.Duration = Base * Speed * (Info.bVoiced ? 0.85f : 1.f);
		Frication.Voice = Info.bVoiced ? 0.5f : 0.35f * Slack;
		Frication.FormantTau = 0.02f;
		Frication.AmpTau = 0.012f;
		Locus(Frication, ObstruentPull);
		Noise(Frication, 1.f);
		return 0.03f;
	}

	case Aspirate:
	{
		FSegment& Breath = AddSegment();
		Breath.Duration = 0.05f * Speed;
		Breath.Aspiration = Info.Gain * (0.5f + 0.5f * A);
		Breath.Voice = 0.2f * Slack;
		Breath.bVowel = true;
		Breath.FormantTau = 0.02f;
		Breath.AmpTau = 0.012f;
		return DefaultTau;
	}

	case Nasal:
	{
		FSegment& Murmur = AddSegment();
		Murmur.Duration = 0.055f * Speed;
		Murmur.Voice = 0.5f;
		Murmur.Nasal = 1.f;
		Murmur.BandwidthScale = 2.5f;
		Murmur.FormantTau = 0.012f;
		Murmur.AmpTau = 0.012f;
		Locus(Murmur, Info.Locus);

		// The mouth opens all at once when a nasal releases
		return 0.012f;
	}

	case Approximant:
	{
		FSegment& Glide = AddSegment();
		Glide.Duration = 0.045f * Speed;
		Glide.Voice = 0.8f;
		Glide.BandwidthScale = 1.4f;
		Glide.FormantTau = 0.02f;
		Glide.AmpTau = 0.015f;
		Locus(Glide, Info.Locus);
		return 0.045f;
	}

	default:
		return DefaultTau;
	}
}

void FMumbleSynth::Speak(const FMumbleSyllable& InSyllable)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleSynth::Speak);

	Syllable = InSyllable;
	Syllable.Duration = FMath::Clamp(Syllable.Duration, 0.03f, 2.f);

	NumSegments = 0;
	SegmentIndex = 0;
	SegmentTime = 0.f;
	bIdle = false;
	SilentFor = 0.f;

	// Fast text shortens the consonants with it, or they would eat the vowels
	const float Tempo = FMath::Clamp(Syllable.Duration / 0.14f, 0.6f, 1.3f);

	float StartF[3];
	float EndF[3];
	VowelTargets(Syllable.VowelStart, Syllable.Reduction, StartF);
	VowelTargets(Syllable.VowelEnd, Syllable.Reduction, EndF);

	float NucleusTau = 0.035f;
	for (const EMumbleConsonant Onset : Syllable.Onset)
	{
		NucleusTau = AddConsonant(Onset, true, Tempo, StartF);
	}

	NucleusIndex = NumSegments;
	FSegment& Nucleus = AddSegment();
	Nucleus.Duration = Syllable.Duration;
	Nucleus.Voice = 1.f;
	Nucleus.bVowel = true;
	Nucleus.bNucleus = true;
	Nucleus.FormantTau = NucleusTau;
	Nucleus.AmpTau = 0.014f;

	AddConsonant(Syllable.Coda, false, Tempo, EndF);

	FSegment& Release = AddSegment();
	Release.Duration = Syllable.bPhraseFinal ? 0.22f : 0.07f;
	Release.bVowel = true;
	Release.FormantTau = 0.05f;
	Release.AmpTau = Syllable.bPhraseFinal ? 0.06f : 0.025f;
	Release.bFinalRelease = Syllable.bPhraseFinal;
}

void FMumbleSynth::Silence()
{
	if (bIdle)
	{
		return;
	}

	NumSegments = 0;
	SegmentIndex = 0;
	SegmentTime = 0.f;
	NucleusIndex = 0;

	FSegment& Release = AddSegment();
	Release.Duration = 0.05f;
	Release.FormantTau = 0.05f;
	Release.AmpTau = 0.012f;
	const float Scale = FMath::Max(Voice.TractSize, 0.1f);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Release.F[Index] = Formants[Index] * Scale;
	}
}

bool FMumbleSynth::IsIdle() const
{
	return bIdle && SilentFor > 0.1f;
}

float FMumbleSynth::Random()
{
	// xorshift32
	RandomState ^= RandomState << 13;
	RandomState ^= RandomState >> 17;
	RandomState ^= RandomState << 5;
	return float(RandomState >> 8) * (1.f / 16777216.f);
}

float FMumbleSynth::RandomGaussian()
{
	// Sum of three uniforms: close enough to normal, with no tail to blow up a period
	return (Random() + Random() + Random() - 1.5f) * 2.f;
}

void FMumbleSynth::UpdateControl()
{
	using namespace MumbleSynth;

	const float Dt = float(ControlBlock) / SampleRate;
	const float Scale = 1.f / FMath::Max(Voice.TractSize, 0.1f);

	const FSegment* Segment = (!bIdle && SegmentIndex < NumSegments) ? &Timeline[SegmentIndex] : nullptr;

	float TargetF[3] = { Formants[0] / Scale, Formants[1] / Scale, Formants[2] / Scale };
	float BandwidthScale = 1.f;
	float FormantTau = 0.05f;
	float AmpTau = 0.03f;
	float TargetLogPitch = LogPitch;
	float CreakTarget = 0.f;

	const float StartLog = FMath::Log2(FMath::Max(Syllable.PitchStartHz, 30.f));
	const float EndLog = FMath::Log2(FMath::Max(Syllable.PitchEndHz, 30.f));

	if (Segment)
	{
		const float Loud = FMath::Max(Syllable.Loudness, 0.f);
		const float Progress = Segment->Duration > 0.f ? FMath::Clamp(SegmentTime / Segment->Duration, 0.f, 1.f) : 1.f;

		VoiceTarget = Segment->Voice * Loud;
		AspTarget = Segment->Aspiration * Loud;
		FricTarget = Segment->Frication * Loud;
		NasalTarget = Segment->Nasal;
		FricTract = Segment->FricationTract;
		BandwidthScale = Segment->BandwidthScale;
		FormantTau = Segment->FormantTau;
		AmpTau = Segment->AmpTau;

		if (Segment->Frication > 0.f)
		{
			// TPT state variable band-pass, normalised to unity at the centre
			const float Hz = FMath::Clamp(Segment->FricationHz * Scale, 200.f, SampleRate * 0.45f);
			const float G = FMath::Tan(UE_PI * Hz / SampleRate);
			SvfK = 1.f / FMath::Max(Segment->FricationQ, 0.3f);
			SvfA1 = 1.f / (1.f + G * (G + SvfK));
			SvfA2 = G * SvfA1;
			SvfA3 = G * SvfA2;
		}

		const bool bBeforeNucleus = SegmentIndex < NucleusIndex;
		if (Segment->bVowel)
		{
			const float Glide = Segment->bNucleus ? SmoothStep(Progress) : (bBeforeNucleus ? 0.f : 1.f);
			float StartF[3];
			float EndF[3];
			VowelTargets(Syllable.VowelStart, Syllable.Reduction, StartF);
			VowelTargets(Syllable.VowelEnd, Syllable.Reduction, EndF);
			for (int32 Index = 0; Index < 3; ++Index)
			{
				TargetF[Index] = FMath::Lerp(StartF[Index], EndF[Index], Glide);
			}
		}
		else
		{
			TargetF[0] = Segment->F[0];
			TargetF[1] = Segment->F[1];
			TargetF[2] = Segment->F[2];
		}

		if (Segment->bNucleus)
		{
			TargetLogPitch = FMath::Lerp(StartLog, EndLog, Progress);
		}
		else
		{
			TargetLogPitch = bBeforeNucleus ? StartLog : EndLog;
		}

		if (Segment->bFinalRelease)
		{
			// A phrase lets go downward, and a voice with creak in it fries as the air runs out
			const float Letting = FMath::Clamp(Syllable.Creak, 0.f, 1.f) * Progress;
			TargetLogPitch -= Letting * (1.5f / 12.f);
			CreakTarget = Voice.Creak * Letting;
		}

		SegmentTime += Dt;
		while (SegmentIndex < NumSegments && SegmentTime >= Timeline[SegmentIndex].Duration)
		{
			SegmentTime -= Timeline[SegmentIndex].Duration;
			++SegmentIndex;
		}
		if (SegmentIndex >= NumSegments)
		{
			bIdle = true;
		}
	}
	else
	{
		VoiceTarget = 0.f;
		AspTarget = 0.f;
		FricTarget = 0.f;
		NasalTarget = 0.f;
	}

	const float FormantAlpha = 1.f - FMath::Exp(-Dt / FMath::Max(FormantTau, 0.001f));
	const float TargetAll[NumFormants] = { TargetF[0] * Scale, TargetF[1] * Scale, TargetF[2] * Scale, F4 * Scale, F5 * Scale };
	const float Breathy = 1.f + 0.6f * (Voice.OpenQuotient - 0.6f);
	for (int32 Index = 0; Index < NumFormants; ++Index)
	{
		Formants[Index] += (TargetAll[Index] - Formants[Index]) * FormantAlpha;

		float Bandwidth = BaseBandwidths[Index] * BandwidthScale * Breathy * FMath::Sqrt(Scale);
		if (Index == 0)
		{
			Bandwidth *= 1.f + Voice.Nasality * 0.6f;
		}
		Bandwidths[Index] += (Bandwidth - Bandwidths[Index]) * FormantAlpha;

		KlattCoefficients(Formants[Index], Bandwidths[Index], SampleRate, ResA[Index], ResB[Index], ResC[Index]);
	}
	KlattCoefficients(270.f * Scale, 100.f, SampleRate, NasA, NasB, NasC);

	const float PitchAlpha = 1.f - FMath::Exp(-Dt / 0.03f);
	LogPitch += (TargetLogPitch - LogPitch) * PitchAlpha;

	const float SlowAlpha = 1.f - FMath::Exp(-Dt / 0.08f);
	CreakAmount += (CreakTarget - CreakAmount) * SlowAlpha;
	EffortNow += (Syllable.Effort - EffortNow) * SlowAlpha;

	// Slow random drift: a held note is never quite held
	DriftCountdown -= Dt;
	if (DriftCountdown <= 0.f)
	{
		DriftCountdown = 0.1f + 0.15f * Random();
		DriftTarget = RandomGaussian() * (0.15f + Voice.Jitter * 6.f);
	}
	Drift += (DriftTarget - Drift) * (1.f - FMath::Exp(-Dt / 0.12f));

	AmpCoeff = 1.f - FMath::Exp(-1.f / (FMath::Max(AmpTau, 0.0005f) * SampleRate));

	const float Cutoff = FMath::Clamp(Voice.ConsonantCutoff * FMath::Sqrt(Scale), 500.f, SampleRate * 0.45f);
	FricLpCoeff = 1.f - FMath::Exp(-2.f * UE_PI * Cutoff / SampleRate);
}

void FMumbleSynth::StartPulse()
{
	bOddPulse = !bOddPulse;

	const float Sub = FMath::Clamp(Voice.Subharmonics, 0.f, 1.f);

	// Alternate long and short periods; the pair still averages to the intended pitch
	PeriodScale = 1.f + Voice.Jitter * RandomGaussian() + (bOddPulse ? 0.08f : -0.08f) * Sub;
	if (CreakAmount > 0.001f)
	{
		PeriodScale *= 1.f + CreakAmount * (0.3f + 1.2f * Random());
	}
	PeriodScale = FMath::Clamp(PeriodScale, 0.5f, 3.f);

	PulseAmp = FMath::Clamp(1.f + Voice.Shimmer * RandomGaussian(), 0.3f, 1.7f);
	if (bOddPulse)
	{
		PulseAmp *= 1.f - 0.65f * Sub;
		PulseAmp *= 1.f - 0.5f * CreakAmount;
	}

	// Effort presses the folds together: shorter open phase, sharper closure
	const float Effort = FMath::Clamp(EffortNow, 0.f, 1.f);
	const float OpenQuotient = FMath::Clamp(Voice.OpenQuotient - 0.2f * Effort, 0.3f, 0.92f);
	const float Skew = FMath::Max(Voice.Skew * (1.f + 0.3f * Effort), 1.f);
	Tp = OpenQuotient * Skew / (1.f + Skew);
	Tn = OpenQuotient / (1.f + Skew);
	Te = Tp + Tn;

	const float Bright = FMath::Clamp(Voice.Brightness + 0.35f * Effort, 0.f, 1.f);
	Ta = FMath::Lerp(0.06f, 0.004f, Bright);
	ReturnEnd = FMath::Exp(-(1.f - Te) / Ta);
}

float FMumbleSynth::NextGlottalSample(float& OutFlow)
{
	WobblePhase += Voice.WobbleRate / SampleRate;
	WobblePhase -= FMath::FloorToFloat(WobblePhase);
	const float Wobble = FMath::Sin(2.f * UE_PI * WobblePhase) * Voice.WobbleDepth;

	const float F0 = FMath::Exp2(LogPitch + (Wobble + Drift) / 12.f);
	Phase += F0 / (SampleRate * PeriodScale);
	if (Phase >= 1.f)
	{
		Phase -= FMath::FloorToFloat(Phase);
		StartPulse();
	}

	// The derivative of a Rosenberg pulse, which carries the lip radiation, normalised so the
	// closing peak is -1, then an exponential return phase that sets the spectral tilt
	float Derivative;
	if (Phase < Tp)
	{
		const float X = UE_PI * Phase / Tp;
		Derivative = FMath::Sin(X) * (Tn / Tp);
		OutFlow = 0.5f * (1.f - FMath::Cos(X));
	}
	else if (Phase < Te)
	{
		const float X = UE_HALF_PI * (Phase - Tp) / Tn;
		Derivative = -FMath::Sin(X);
		OutFlow = FMath::Cos(X);
	}
	else
	{
		const float Decay = FMath::Exp(-(Phase - Te) / Ta);
		Derivative = -(Decay - ReturnEnd) / (1.f - ReturnEnd);
		OutFlow = 0.f;
	}

	return Derivative * PulseAmp;
}

void FMumbleSynth::Render(float* Out, int32 NumFrames)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleSynth::Render);

	using namespace MumbleSynth;

	const float InvSampleRate = 1.f / SampleRate;
	const float Breathiness = FMath::Clamp(Voice.Breathiness, 0.f, 1.f);
	const float Nasality = FMath::Clamp(Voice.Nasality, 0.f, 1.f);
	const float Gain = OutputGain * Voice.Gain;

	for (int32 Frame = 0; Frame < NumFrames; ++Frame)
	{
		if (--ControlCountdown <= 0)
		{
			UpdateControl();
			ControlCountdown = ControlBlock;

			// Rung out: nothing left to render until the next syllable
			if (IsIdle())
			{
				FMemory::Memzero(Out + Frame, sizeof(float) * (NumFrames - Frame));
				return;
			}
		}

		VoiceAmp += (VoiceTarget - VoiceAmp) * AmpCoeff;
		AspAmp += (AspTarget - AspAmp) * AmpCoeff;
		FricAmp += (FricTarget - FricAmp) * AmpCoeff;
		NasalAmp += (NasalTarget - NasalAmp) * AmpCoeff;

		float Flow;
		const float Glottal = NextGlottalSample(Flow) * VoiceAmp;

		const float Noise = Random() * 2.f - 1.f;
		const float Breath = Noise * (Breathiness * 0.5f * VoiceAmp * (0.2f + 0.8f * Flow) + AspAmp * 0.6f);

		// Frication: its own noise, banded at the place it is made, then softened
		float Frication = 0.f;
		if (FricAmp > Inaudible)
		{
			const float FricNoise = Random() * 2.f - 1.f;
			const float V3 = FricNoise - SvfIc2;
			const float V1 = SvfA1 * SvfIc1 + SvfA2 * V3;
			const float V2 = SvfIc2 + SvfA2 * SvfIc1 + SvfA3 * V3;
			SvfIc1 = 2.f * V1 - SvfIc1;
			SvfIc2 = 2.f * V2 - SvfIc2;
			FricLp += (V1 * SvfK - FricLp) * FricLpCoeff;
			FricLp2 += (FricLp - FricLp2) * FricLpCoeff;
			Frication = FricLp2 * FricAmp;
		}

		float Signal = Glottal + Breath + Frication * FricTract;
		for (int32 Index = 0; Index < NumFormants; ++Index)
		{
			const float Y = ResA[Index] * Signal + ResB[Index] * ResY1[Index] + ResC[Index] * ResY2[Index];
			ResY2[Index] = ResY1[Index];
			ResY1[Index] = Y;
			Signal = Y;
		}

		const float Nasal = NasA * Glottal + NasB * NasY1 + NasC * NasY2;
		NasY2 = NasY1;
		NasY1 = Nasal;

		const float Oral = Signal * (1.f - 0.6f * NasalAmp);
		const float Murmur = Nasal * (NasalAmp * 0.8f + Nasality * 0.25f * VoiceAmp);
		const float Mixed = (Oral + Murmur) * Gain + Frication * (1.f - FricTract) * Voice.Gain;

		// DC blocker: the return phase leaves an offset in every pulse
		const float Blocked = Mixed - DcX1 + 0.995f * DcY1;
		DcX1 = Mixed;
		DcY1 = Blocked;

		Out[Frame] = SoftClip(Blocked);

		const bool bAudible = VoiceAmp > Inaudible || AspAmp > Inaudible || FricAmp > Inaudible || FMath::Abs(Blocked) > 1e-4f;
		SilentFor = (bIdle && !bAudible) ? SilentFor + InvSampleRate : 0.f;
	}
}
