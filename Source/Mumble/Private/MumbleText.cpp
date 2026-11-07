// Copyright (c) Jared Taylor

#include "MumbleText.h"

#include "Math/RandomStream.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace MumbleText
{
	using EC = EMumbleConsonant;
	using EV = EMumbleVowel;

	enum class EPunct : uint8 { None, Comma, Period, Question, Exclaim, Ellipsis, Dash, Break };

	enum class EPhrase : uint8 { Statement, Continuation, Question, Exclaim, Trail };

	struct FVowelSound
	{
		EV Start = EV::AX;
		EV End = EV::AX;
	};

	/** A syllable before the tune is put on it */
	struct FRawSyllable
	{
		int32 Trigger = 0;
		EC Onset[2] = { EC::None, EC::None };
		EC Coda = EC::None;
		FVowelSound Vowel;
		float Reduction = 0.f;
		bool bStressed = false;
		bool bShout = false;
		bool bLong = false;
		int32 Phrase = 0;
	};

	struct FPhrase
	{
		EPhrase Type = EPhrase::Statement;
		/** One past the last character that belongs to the phrase, before its punctuation */
		int32 EndChar = 0;
		/** Which phrase of its sentence this is, counting from zero */
		int32 IndexInSentence = 0;
	};

	UTF32CHAR FoldLatin(UTF32CHAR C)
	{
		if (C >= 'A' && C <= 'Z')
		{
			return C - 'A' + 'a';
		}
		if (C < 0xC0)
		{
			return C;
		}

		// Accented Latin down to its base letter, macrons included: names in this kind of game use them
		struct FFold { UTF32CHAR From; UTF32CHAR To; char Base; };
		static const FFold Folds[] =
		{
			{ 0xC0, 0xC5, 'a' }, { 0xE0, 0xE5, 'a' }, { 0x100, 0x105, 'a' },
			{ 0xC8, 0xCB, 'e' }, { 0xE8, 0xEB, 'e' }, { 0x112, 0x11B, 'e' },
			{ 0xCC, 0xCF, 'i' }, { 0xEC, 0xEF, 'i' }, { 0x12A, 0x12F, 'i' },
			{ 0xD2, 0xD6, 'o' }, { 0xF2, 0xF6, 'o' }, { 0x14C, 0x151, 'o' }, { 0xD8, 0xD8, 'o' }, { 0xF8, 0xF8, 'o' },
			{ 0xD9, 0xDC, 'u' }, { 0xF9, 0xFC, 'u' }, { 0x168, 0x173, 'u' },
			{ 0xD1, 0xD1, 'n' }, { 0xF1, 0xF1, 'n' },
			{ 0xC7, 0xC7, 'c' }, { 0xE7, 0xE7, 'c' },
			{ 0xDD, 0xDD, 'y' }, { 0xFD, 0xFD, 'y' }, { 0xFF, 0xFF, 'y' },
		};
		for (const FFold& Fold : Folds)
		{
			if (C >= Fold.From && C <= Fold.To)
			{
				return UTF32CHAR(Fold.Base);
			}
		}
		return C;
	}

	bool IsLatin(UTF32CHAR C)
	{
		const UTF32CHAR F = FoldLatin(C);
		return F >= 'a' && F <= 'z';
	}

	bool IsUpperLatin(UTF32CHAR C)
	{
		return (C >= 'A' && C <= 'Z') || (C >= 0xC0 && C <= 0xDD && C != 0xD7);
	}

	bool IsApostrophe(UTF32CHAR C)
	{
		return C == '\'' || C == 0x2019;
	}

	bool IsHiragana(UTF32CHAR C) { return C >= 0x3041 && C <= 0x3096; }
	bool IsKatakana(UTF32CHAR C) { return C >= 0x30A1 && C <= 0x30F6; }
	bool IsKanaLengthMark(UTF32CHAR C) { return C == 0x30FC; }
	bool IsHangul(UTF32CHAR C) { return C >= 0xAC00 && C <= 0xD7A3; }

	bool IsIdeograph(UTF32CHAR C)
	{
		return (C >= 0x4E00 && C <= 0x9FFF) || (C >= 0x3400 && C <= 0x4DBF) || (C >= 0xF900 && C <= 0xFAFF);
	}

	bool IsSpace(UTF32CHAR C)
	{
		return C == ' ' || C == '\t' || C == 0xA0 || C == 0x3000;
	}

	bool IsLetterLike(UTF32CHAR C)
	{
		return C < 0x10000 ? FChar::IsAlnum(TCHAR(C)) : true;
	}

	/** Closing quotes and brackets, which a pause looks back past */
	bool IsCloser(UTF32CHAR C)
	{
		return C == '"' || C == '\'' || C == ')' || C == ']' || C == 0x201D || C == 0x2019 || C == 0x300D || C == 0x300F || C == 0xFF09;
	}

	EPunct Punctuation(UTF32CHAR C)
	{
		switch (C)
		{
		case ',': case ';': case ':': case 0x3001: case 0xFF0C: case 0xFF1B: case 0xFF1A:
			return EPunct::Comma;
		case '.': case 0x3002: case 0xFF0E:
			return EPunct::Period;
		case '?': case 0xFF1F:
			return EPunct::Question;
		case '!': case 0xFF01:
			return EPunct::Exclaim;
		case 0x2026: case 0x22EF:
			return EPunct::Ellipsis;
		case 0x2014: case 0x2013: case 0x2015:
			return EPunct::Dash;
		case '\n': case '\r':
			return EPunct::Break;
		default:
			return EPunct::None;
		}
	}

	/**
	 * The punctuation that ends at Index, if it ends something.
	 *
	 * A run of full stops is one ellipsis; a stop between letters or digits ("3.5", "e.g.") is not
	 * a pause; a hyphen only counts doubled or standing between spaces.
	 */
	EPunct EndingPunctuation(TConstArrayView<UTF32CHAR> Chars, int32 Index)
	{
		const int32 Num = Chars.Num();
		const UTF32CHAR C = Chars[Index];
		const UTF32CHAR Next = Index + 1 < Num ? Chars[Index + 1] : 0;

		if (C == '-')
		{
			const bool bDoubled = Next == '-' || (Index > 0 && Chars[Index - 1] == '-');
			const bool bSpaced = Index > 0 && IsSpace(Chars[Index - 1]) && (Next == 0 || IsSpace(Next));
			if ((bDoubled && Next != '-') || bSpaced)
			{
				return EPunct::Dash;
			}
			return EPunct::None;
		}

		EPunct Type = Punctuation(C);
		if (Type == EPunct::None || Type == EPunct::Break)
		{
			return Type;
		}

		// Mid-token: the mark is followed straight away by more of the word
		if (Next != 0 && IsLetterLike(Next) && Next < 0x3000)
		{
			return EPunct::None;
		}

		if (Type == EPunct::Period)
		{
			if (Next == '.' || Next == 0x3002)
			{
				return EPunct::None;
			}
			if (Index > 0 && Chars[Index - 1] == '.')
			{
				return EPunct::Ellipsis;
			}
		}

		// "?!" and "!?" pause once, on the last of them
		if (Next != 0 && Punctuation(Next) >= EPunct::Period && Punctuation(Next) <= EPunct::Ellipsis)
		{
			return EPunct::None;
		}

		return Type;
	}

	void ToCodepoints(const FString& Text, TArray<UTF32CHAR>& Out)
	{
		Out.Reset(Text.Len());
		for (int32 Index = 0; Index < Text.Len(); ++Index)
		{
			const uint32 C = uint32(Text[Index]);
			if (C >= 0xD800 && C <= 0xDBFF && Index + 1 < Text.Len())
			{
				const uint32 Low = uint32(Text[Index + 1]);
				if (Low >= 0xDC00 && Low <= 0xDFFF)
				{
					Out.Add(UTF32CHAR(0x10000 + ((C - 0xD800) << 10) + (Low - 0xDC00)));
					++Index;
					continue;
				}
			}
			Out.Add(UTF32CHAR(C));
		}
	}

	void BuildSchedule(TConstArrayView<UTF32CHAR> Chars, const FMumbleTiming& Timing, TArray<float>& OutTimes)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(MumbleText::BuildSchedule);

		const float Step = 1.f / FMath::Max(Timing.CharactersPerSecond, 1.f);
		OutTimes.SetNumUninitialized(Chars.Num());

		float Time = 0.f;
		EPunct Owed = EPunct::None;
		for (int32 Index = 0; Index < Chars.Num(); ++Index)
		{
			float Wait = Index > 0 ? Step : 0.f;

			// The pause is paid by the first character that is not a closing quote or space
			if (Owed != EPunct::None && !IsCloser(Chars[Index]) && !IsSpace(Chars[Index]))
			{
				switch (Owed)
				{
				case EPunct::Comma:		Wait += Timing.CommaPause; break;
				case EPunct::Period:	Wait += Timing.PeriodPause; break;
				case EPunct::Question:	Wait += Timing.QuestionPause; break;
				case EPunct::Exclaim:	Wait += Timing.ExclamationPause; break;
				case EPunct::Ellipsis:	Wait += Timing.EllipsisPause; break;
				case EPunct::Dash:		Wait += Timing.DashPause; break;
				case EPunct::Break:		Wait += Timing.PeriodPause; break;
				default: break;
				}
				Owed = EPunct::None;
			}

			Time += Wait;
			OutTimes[Index] = Time;

			const EPunct Ending = EndingPunctuation(Chars, Index);
			if (Ending != EPunct::None)
			{
				Owed = Ending;
			}
		}
	}

	// ---- English ---------------------------------------------------------------------------------

	const TCHAR* FunctionWords[] =
	{
		TEXT("a"), TEXT("an"), TEXT("the"), TEXT("and"), TEXT("or"), TEXT("but"), TEXT("if"), TEXT("of"), TEXT("to"),
		TEXT("in"), TEXT("on"), TEXT("at"), TEXT("by"), TEXT("for"), TEXT("with"), TEXT("from"), TEXT("as"), TEXT("is"),
		TEXT("are"), TEXT("was"), TEXT("were"), TEXT("be"), TEXT("been"), TEXT("am"), TEXT("do"), TEXT("does"), TEXT("did"),
		TEXT("has"), TEXT("have"), TEXT("had"), TEXT("i"), TEXT("you"), TEXT("he"), TEXT("she"), TEXT("it"), TEXT("we"),
		TEXT("they"), TEXT("me"), TEXT("him"), TEXT("her"), TEXT("us"), TEXT("them"), TEXT("my"), TEXT("your"), TEXT("his"),
		TEXT("its"), TEXT("our"), TEXT("their"), TEXT("this"), TEXT("that"), TEXT("these"), TEXT("those"), TEXT("so"),
		TEXT("than"), TEXT("then"), TEXT("there"), TEXT("will"), TEXT("would"), TEXT("can"), TEXT("could"), TEXT("shall"),
		TEXT("should"), TEXT("may"), TEXT("might"), TEXT("must"), TEXT("just"), TEXT("up"), TEXT("into"), TEXT("some"),
		TEXT("any"), TEXT("i'm"), TEXT("it's"), TEXT("you're"), TEXT("don't"), TEXT("let"),
	};

	const TCHAR* VoicedThWords[] =
	{
		TEXT("the"), TEXT("this"), TEXT("that"), TEXT("these"), TEXT("those"), TEXT("they"), TEXT("them"), TEXT("their"),
		TEXT("there"), TEXT("then"), TEXT("than"), TEXT("though"), TEXT("thus"), TEXT("thee"), TEXT("thou"), TEXT("thy"),
	};

	template <int32 N>
	bool IsOneOf(const FString& Word, const TCHAR* const (&List)[N])
	{
		for (const TCHAR* Entry : List)
		{
			if (Word.Equals(Entry, ESearchCase::CaseSensitive))
			{
				return true;
			}
		}
		return false;
	}

	bool IsVowelLetter(TCHAR C)
	{
		return C == 'a' || C == 'e' || C == 'i' || C == 'o' || C == 'u';
	}

	/** One consonant sound, spelled by one or more letters */
	struct FConsonantUnit
	{
		int32 Letter = 0;
		int32 Length = 1;
		EC Sounds[2] = { EC::None, EC::None };
	};

	struct FVowelGroup
	{
		int32 Start = 0;
		int32 End = 0;
	};

	/** Spelling to sound for the consonants between two vowel groups, or at either end of a word */
	void ReadConsonants(const FString& W, int32 From, int32 To, bool bVoicedTh, TArray<FConsonantUnit>& Out)
	{
		const int32 Len = W.Len();
		auto At = [&W, Len](int32 I) -> TCHAR { return I >= 0 && I < Len ? W[I] : TCHAR(0); };

		int32 I = From;
		while (I < To)
		{
			FConsonantUnit Unit;
			Unit.Letter = I;
			const TCHAR C = W[I];
			const TCHAR N = I + 1 < To ? W[I + 1] : TCHAR(0);
			const TCHAR After = At(To);

			auto Two = [&](EC Sound) { Unit.Length = 2; Unit.Sounds[0] = Sound; };

			if (C == 'c' && N == 'h') { Two(EC::CH); }
			else if (C == 's' && N == 'h') { Two(EC::SH); }
			else if (C == 't' && N == 'h') { Two(bVoicedTh ? EC::DH : EC::TH); }
			else if (C == 'p' && N == 'h') { Two(EC::F); }
			else if (C == 'w' && N == 'h') { Two(EC::W); }
			else if (C == 'c' && N == 'k') { Two(EC::K); }
			else if (C == 'n' && N == 'g') { Two(EC::NG); }
			else if (C == 'g' && N == 'h') { Unit.Length = 2; Unit.Sounds[0] = I == 0 ? EC::G : EC::None; }
			else if (C == 'k' && N == 'n' && I == 0) { Two(EC::N); }
			else if (C == 'w' && N == 'r' && I == 0) { Two(EC::R); }
			else if (C == 'm' && N == 'b' && I + 2 == Len) { Two(EC::M); }
			else if (C == 't' && N == 'c' && At(I + 2) == 'h') { Unit.Length = 3; Unit.Sounds[0] = EC::CH; }
			else if (C == 'q') { Unit.Sounds[0] = EC::K; Unit.Sounds[1] = EC::W; }
			else if (C == 'x') { Unit.Sounds[0] = I == 0 ? EC::Z : EC::K; Unit.Sounds[1] = I == 0 ? EC::None : EC::S; }
			else
			{
				// The next letter that decides a soft c or g may be the vowel after this cluster
				const TCHAR Soft = N != 0 ? N : After;
				const bool bFront = Soft == 'e' || Soft == 'i' || Soft == 'y';
				switch (C)
				{
				case 'b': Unit.Sounds[0] = EC::B; break;
				case 'c': Unit.Sounds[0] = bFront ? EC::S : EC::K; break;
				case 'd': Unit.Sounds[0] = EC::D; break;
				case 'f': Unit.Sounds[0] = EC::F; break;
				case 'g': Unit.Sounds[0] = (bFront && I > 0) ? EC::J : EC::G; break;
				case 'h': Unit.Sounds[0] = EC::H; break;
				case 'j': Unit.Sounds[0] = EC::J; break;
				case 'k': Unit.Sounds[0] = EC::K; break;
				case 'l': Unit.Sounds[0] = EC::L; break;
				case 'm': Unit.Sounds[0] = EC::M; break;
				case 'n': Unit.Sounds[0] = EC::N; break;
				case 'p': Unit.Sounds[0] = EC::P; break;
				case 'r': Unit.Sounds[0] = EC::R; break;
				case 's':
				{
					// Between vowels, and at the end after a voiced sound, s is a z
					const bool bBetween = I > 0 && IsVowelLetter(At(I - 1)) && IsVowelLetter(At(I + 1));
					const bool bFinal = I == Len - 1 && I > 0 && At(I - 1) != 's' && At(I - 1) != 'f'
						&& At(I - 1) != 'k' && At(I - 1) != 'p' && At(I - 1) != 't';
					Unit.Sounds[0] = (bBetween || bFinal) ? EC::Z : EC::S;
					break;
				}
				case 't': Unit.Sounds[0] = EC::T; break;
				case 'v': Unit.Sounds[0] = EC::V; break;
				case 'w': Unit.Sounds[0] = EC::W; break;
				case 'y': Unit.Sounds[0] = EC::Y; break;
				case 'z': Unit.Sounds[0] = EC::Z; break;
				default: Unit.Sounds[0] = EC::None; break;
				}
			}

			Unit.Length = FMath::Min(Unit.Length, To - I);
			Out.Add(Unit);
			I += Unit.Length;
		}
	}

	bool IsLegalOnsetPair(EC First, EC Second)
	{
		const bool bStopOrF = First == EC::P || First == EC::B || First == EC::T || First == EC::D
			|| First == EC::K || First == EC::G || First == EC::F || First == EC::TH || First == EC::SH;
		if (bStopOrF && (Second == EC::L || Second == EC::R || Second == EC::W))
		{
			return !((First == EC::T || First == EC::D) && Second == EC::L);
		}
		if (First == EC::S)
		{
			return Second == EC::P || Second == EC::T || Second == EC::K || Second == EC::L || Second == EC::M
				|| Second == EC::N || Second == EC::W;
		}
		return false;
	}

	/** The first two consonant sounds of the units that open a syllable, favouring the ones nearest the vowel */
	void FillOnset(TConstArrayView<FConsonantUnit> Units, EC (&Onset)[2])
	{
		TArray<EC, TInlineAllocator<6>> Sounds;
		for (const FConsonantUnit& Unit : Units)
		{
			for (const EC Sound : Unit.Sounds)
			{
				if (Sound != EC::None)
				{
					Sounds.Add(Sound);
				}
			}
		}

		const int32 Num = Sounds.Num();
		Onset[0] = Num >= 2 ? Sounds[Num - 2] : (Num == 1 ? Sounds[0] : EC::None);
		Onset[1] = Num >= 2 ? Sounds[Num - 1] : EC::None;
	}

	EC FirstCoda(TConstArrayView<FConsonantUnit> Units, bool bSkipR)
	{
		for (const FConsonantUnit& Unit : Units)
		{
			for (const EC Sound : Unit.Sounds)
			{
				// After a vowel, r colours it and h is silent
				if (Sound == EC::None || Sound == EC::H || (bSkipR && Sound == EC::R) || Sound == EC::W || Sound == EC::Y)
				{
					continue;
				}
				return Sound;
			}
		}
		return EC::None;
	}

	FVowelSound Pure(EV Vowel) { return { Vowel, Vowel }; }
	FVowelSound Glide(EV From, EV To) { return { From, To }; }

	/**
	 * Spelling to sound for one vowel group.
	 *
	 * @param bMagic  A silent e follows after one consonant, which makes the vowel say its name
	 * @param bRColoured  An r follows in the same syllable
	 * @param bOpenFinal  Nothing follows the group in the word
	 */
	FVowelSound ReadVowel(const FString& Group, bool bMagic, bool bRColoured, bool bOpenFinal, int32 NumGroups, bool bFirstGroup)
	{
		const FVowelSound AY = Glide(EV::AA, EV::IH);
		const FVowelSound AW = Glide(EV::AA, EV::UH);
		const FVowelSound OY = Glide(EV::AO, EV::IH);
		const FVowelSound EY = Glide(EV::EY, EV::IY);
		const FVowelSound OW = Glide(EV::OW, EV::UH);

		if (Group.Len() >= 2)
		{
			const FString Pair = Group.Left(2);
			if (Pair == TEXT("ee") || Pair == TEXT("ea")) { return Pure(EV::IY); }
			if (Pair == TEXT("ie")) { return (bOpenFinal && NumGroups == 1) ? AY : Pure(EV::IY); }
			if (Pair == TEXT("ei") || Pair == TEXT("ai") || Pair == TEXT("ay")) { return EY; }
			if (Pair == TEXT("ey")) { return (bOpenFinal && NumGroups > 1) ? Pure(EV::IY) : EY; }
			if (Pair == TEXT("oa") || Pair == TEXT("oe")) { return OW; }
			if (Pair == TEXT("oo")) { return Pure(EV::UW); }
			if (Pair == TEXT("ou")) { return AW; }
			if (Pair == TEXT("ow")) { return bOpenFinal ? OW : AW; }
			if (Pair == TEXT("oi") || Pair == TEXT("oy")) { return OY; }
			if (Pair == TEXT("au") || Pair == TEXT("aw")) { return Pure(EV::AO); }
			if (Pair == TEXT("ew") || Pair == TEXT("ue") || Pair == TEXT("ui") || Pair == TEXT("eu")) { return Pure(EV::UW); }
			if (Pair == TEXT("uy") || Pair == TEXT("ye")) { return AY; }
		}

		switch (Group[0])
		{
		case 'a':
			if (bMagic) { return EY; }
			if (bRColoured) { return Pure(EV::AA); }
			if (bOpenFinal && NumGroups > 1) { return Pure(EV::AX); }
			return Pure(EV::AE);
		case 'e':
			if (bMagic) { return Pure(EV::IY); }
			if (bRColoured) { return Pure(EV::ER); }
			if (bOpenFinal && NumGroups == 1) { return Pure(EV::IY); }
			return Pure(EV::EH);
		case 'i':
			if (bMagic) { return AY; }
			if (bRColoured) { return Pure(EV::ER); }
			if (bOpenFinal) { return Pure(EV::IY); }
			return Pure(EV::IH);
		case 'o':
			if (bMagic) { return OW; }
			if (bRColoured) { return Pure(EV::AO); }
			if (bOpenFinal) { return OW; }
			return Pure(EV::AA);
		case 'u':
			if (bMagic) { return Pure(EV::UW); }
			if (bRColoured) { return Pure(EV::ER); }
			return Pure(EV::AH);
		case 'y':
			if (bOpenFinal) { return NumGroups == 1 ? AY : Pure(EV::IY); }
			return bFirstGroup && bMagic ? AY : Pure(EV::IH);
		default:
			return Pure(EV::AX);
		}
	}

	int32 StressedSyllable(const FString& Word, int32 NumSyllables)
	{
		if (NumSyllables <= 1)
		{
			return 0;
		}
		if (NumSyllables == 2)
		{
			static const TCHAR* Prefixes[] = { TEXT("a"), TEXT("be"), TEXT("de"), TEXT("re"), TEXT("un"), TEXT("ex"), TEXT("pre"), TEXT("con"), TEXT("com") };
			for (const TCHAR* Prefix : Prefixes)
			{
				// "again", "before", "report": the prefix is unstressed. "apple" is not a prefix.
				if (Word.StartsWith(Prefix, ESearchCase::CaseSensitive) && Word.Len() > FCString::Strlen(Prefix) + 2
					&& !IsVowelLetter(Word[FCString::Strlen(Prefix)]) && Word[FCString::Strlen(Prefix)] != Word[FCString::Strlen(Prefix) + 1])
				{
					return 1;
				}
			}
			return 0;
		}
		if (Word.EndsWith(TEXT("tion")) || Word.EndsWith(TEXT("sion")) || Word.EndsWith(TEXT("ic")) || Word.EndsWith(TEXT("ical")))
		{
			return NumSyllables - 2;
		}
		return FMath::Max(NumSyllables - 3, 0);
	}

	/**
	 * Splits one English word into syllables.
	 *
	 * @param Positions  The character index of each letter of Word in the line
	 */
	void ReadEnglishWord(const FString& Word, TConstArrayView<int32> Positions, bool bShout, TArray<FRawSyllable>& Out)
	{
		const int32 Len = Word.Len();
		if (Len == 0)
		{
			return;
		}

		// Which letters are vowels: y is one unless it starts the word or a syllable, u after q is a w
		TArray<bool, TInlineAllocator<32>> Vowel;
		Vowel.SetNumZeroed(Len);
		for (int32 I = 0; I < Len; ++I)
		{
			const TCHAR C = Word[I];
			if (IsVowelLetter(C))
			{
				Vowel[I] = !(C == 'u' && I > 0 && Word[I - 1] == 'q');
				// The u of "guess" and "guard" is only there to keep the g hard
				if (C == 'u' && I > 0 && Word[I - 1] == 'g' && I + 1 < Len && IsVowelLetter(Word[I + 1]))
				{
					Vowel[I] = false;
				}
			}
			else if (C == 'y')
			{
				Vowel[I] = I > 0 && !(I + 1 < Len && IsVowelLetter(Word[I + 1]));
			}
		}

		TArray<FVowelGroup, TInlineAllocator<8>> Groups;
		for (int32 I = 0; I < Len;)
		{
			if (!Vowel[I])
			{
				++I;
				continue;
			}
			FVowelGroup Group;
			Group.Start = I;
			while (I < Len && Vowel[I])
			{
				// Two vowels that are two syllables: piano, video, radio, duo
				if (I > Group.Start)
				{
					const TCHAR A = Word[I - 1];
					const TCHAR B = Word[I];
					const bool bHiatus = (A == 'i' && (B == 'a' || B == 'o' || B == 'u')) || (A == 'e' && B == 'o')
						|| (A == 'u' && (B == 'a' || B == 'o'));
					if (bHiatus)
					{
						break;
					}
				}
				++I;
			}
			Group.End = I;
			Groups.Add(Group);
		}

		if (Groups.Num() == 0)
		{
			// "hmm", "shh", "Mr": one murmured syllable on the first letter
			FRawSyllable Syllable;
			Syllable.Trigger = Positions[0];
			Syllable.Vowel = Pure(EV::AX);
			Syllable.Reduction = 0.5f;
			Syllable.bShout = bShout;
			TArray<FConsonantUnit> Units;
			ReadConsonants(Word, 0, Len, false, Units);
			Syllable.Onset[0] = Units.Num() > 0 ? Units[0].Sounds[0] : EC::None;
			Out.Add(Syllable);
			return;
		}

		// Silent final e, and the magic e that lengthens the vowel before it
		bool bMagicLast = false;
		{
			const FVowelGroup Last = Groups.Last();
			const bool bLoneE = Last.End == Len && Last.End - Last.Start == 1 && Word[Last.Start] == 'e';
			const bool bEs = Len >= 3 && Word.EndsWith(TEXT("es")) && Last.Start == Len - 2 && Last.End == Len - 1;
			const bool bEd = Len >= 3 && Word.EndsWith(TEXT("ed")) && Last.Start == Len - 2 && Last.End == Len - 1;
			if (Groups.Num() >= 2 && (bLoneE || bEs || bEd))
			{
				const TCHAR Before = Word[Last.Start - 1];
				const bool bAfterConsonant = !IsVowelLetter(Before);
				const bool bSyllabicLe = bLoneE && Before == 'l' && Last.Start >= 2 && !IsVowelLetter(Word[Last.Start - 2]);
				const bool bVoicedEs = bEs && (Before == 's' || Before == 'x' || Before == 'z' || Before == 'h' || Before == 'c' || Before == 'g');
				const bool bVoicedEd = bEd && (Before == 't' || Before == 'd');
				// A final -es is only silent where a magic e would be: "times", "hopes", but not "Agnes"
				const FVowelGroup Prev = Groups[Groups.Num() - 2];
				const bool bMagicShape = Last.Start - Prev.End == 1 && Prev.End - Prev.Start == 1;
				if (bAfterConsonant && !bSyllabicLe && !bVoicedEs && !bVoicedEd && (!bEs || bMagicShape))
				{
					bMagicLast = bMagicShape && !bEd;
					Groups.Pop();
				}
			}
		}

		const int32 NumGroups = Groups.Num();
		const bool bFunction = IsOneOf(Word, FunctionWords);
		const bool bVoicedTh = IsOneOf(Word, VoicedThWords);
		const int32 Stressed = bFunction ? INDEX_NONE : StressedSyllable(Word, NumGroups);

		// Where each syllable's onset starts: consonants between groups split per the onset rules
		TArray<int32, TInlineAllocator<8>> OnsetStart;
		OnsetStart.SetNum(NumGroups);
		OnsetStart[0] = 0;
		for (int32 G = 1; G < NumGroups; ++G)
		{
			const int32 From = Groups[G - 1].End;
			const int32 To = Groups[G].Start;

			TArray<FConsonantUnit> Units;
			ReadConsonants(Word, From, To, bVoicedTh, Units);

			int32 Split = To;
			if (Units.Num() == 1)
			{
				Split = Units[0].Letter;
			}
			else if (Units.Num() >= 2)
			{
				const FConsonantUnit& A = Units[Units.Num() - 2];
				const FConsonantUnit& B = Units[Units.Num() - 1];
				const bool bPair = A.Sounds[1] == EC::None && B.Sounds[1] == EC::None && IsLegalOnsetPair(A.Sounds[0], B.Sounds[0]);
				Split = bPair ? A.Letter : B.Letter;
			}
			OnsetStart[G] = Split;
		}

		for (int32 G = 0; G < NumGroups; ++G)
		{
			const FVowelGroup& Group = Groups[G];
			const int32 CodaEnd = G + 1 < NumGroups ? OnsetStart[G + 1] : Len;

			TArray<FConsonantUnit> OnsetUnits;
			ReadConsonants(Word, OnsetStart[G], Group.Start, bVoicedTh, OnsetUnits);
			TArray<FConsonantUnit> CodaUnits;
			ReadConsonants(Word, Group.End, CodaEnd, bVoicedTh, CodaUnits);

			const bool bRColoured = Group.End < CodaEnd && Word[Group.End] == 'r';
			const bool bMagic = bMagicLast && G == NumGroups - 1;
			const bool bOpenFinal = G == NumGroups - 1 && Group.End == Len;

			FRawSyllable Syllable;
			Syllable.Trigger = Positions[OnsetStart[G]];
			FillOnset(OnsetUnits, Syllable.Onset);
			Syllable.Coda = FirstCoda(CodaUnits, bRColoured);
			Syllable.Vowel = ReadVowel(Word.Mid(Group.Start, Group.End - Group.Start), bMagic, bRColoured, bOpenFinal, NumGroups, G == 0);
			Syllable.bStressed = G == Stressed;
			Syllable.bShout = bShout;

			if (bFunction)
			{
				Syllable.Reduction = 0.3f;
			}
			else if (!Syllable.bStressed && NumGroups > 1)
			{
				Syllable.Reduction = 0.45f;
			}
			Out.Add(Syllable);
		}
	}

	// ---- Kana and everything else ----------------------------------------------------------------

	/** Romaji for U+3041 to U+3096. An x marks a small kana that modifies the one before it. */
	const TCHAR* const KanaRomaji[] =
	{
		TEXT("xa"), TEXT("a"), TEXT("xi"), TEXT("i"), TEXT("xu"), TEXT("u"), TEXT("xe"), TEXT("e"), TEXT("xo"), TEXT("o"),
		TEXT("ka"), TEXT("ga"), TEXT("ki"), TEXT("gi"), TEXT("ku"), TEXT("gu"), TEXT("ke"), TEXT("ge"), TEXT("ko"), TEXT("go"),
		TEXT("sa"), TEXT("za"), TEXT("shi"), TEXT("ji"), TEXT("su"), TEXT("zu"), TEXT("se"), TEXT("ze"), TEXT("so"), TEXT("zo"),
		TEXT("ta"), TEXT("da"), TEXT("chi"), TEXT("ji"), TEXT("xtsu"), TEXT("tsu"), TEXT("zu"), TEXT("te"), TEXT("de"), TEXT("to"), TEXT("do"),
		TEXT("na"), TEXT("ni"), TEXT("nu"), TEXT("ne"), TEXT("no"),
		TEXT("ha"), TEXT("ba"), TEXT("pa"), TEXT("hi"), TEXT("bi"), TEXT("pi"), TEXT("fu"), TEXT("bu"), TEXT("pu"),
		TEXT("he"), TEXT("be"), TEXT("pe"), TEXT("ho"), TEXT("bo"), TEXT("po"),
		TEXT("ma"), TEXT("mi"), TEXT("mu"), TEXT("me"), TEXT("mo"),
		TEXT("xya"), TEXT("ya"), TEXT("xyu"), TEXT("yu"), TEXT("xyo"), TEXT("yo"),
		TEXT("ra"), TEXT("ri"), TEXT("ru"), TEXT("re"), TEXT("ro"),
		TEXT("xwa"), TEXT("wa"), TEXT("wi"), TEXT("we"), TEXT("wo"), TEXT("n"), TEXT("vu"), TEXT("xka"), TEXT("xke"),
	};
	static_assert(UE_ARRAY_COUNT(KanaRomaji) == 0x3096 - 0x3041 + 1);

	EV KanaVowel(TCHAR C)
	{
		switch (C)
		{
		case 'a': return EV::AA;
		case 'i': return EV::IY;
		case 'u': return EV::UH;
		case 'e': return EV::EH;
		case 'o': return EV::OW;
		default: return EV::AX;
		}
	}

	void ReadRomaji(const FString& Romaji, EC (&Onset)[2], EV& Vowel)
	{
		Onset[0] = EC::None;
		Onset[1] = EC::None;
		Vowel = KanaVowel(Romaji.Len() > 0 ? Romaji[Romaji.Len() - 1] : 'a');

		const FString Head = Romaji.LeftChop(1);
		if (Head == TEXT("sh")) { Onset[0] = EC::SH; }
		else if (Head == TEXT("ch")) { Onset[0] = EC::CH; }
		else if (Head == TEXT("ts")) { Onset[0] = EC::T; Onset[1] = EC::S; }
		else if (Head.Len() == 1)
		{
			switch (Head[0])
			{
			case 'k': Onset[0] = EC::K; break;
			case 'g': Onset[0] = EC::G; break;
			case 's': Onset[0] = EC::S; break;
			case 'z': Onset[0] = EC::Z; break;
			case 'j': Onset[0] = EC::J; break;
			case 't': Onset[0] = EC::T; break;
			case 'd': Onset[0] = EC::D; break;
			case 'n': Onset[0] = EC::N; break;
			case 'h': Onset[0] = EC::H; break;
			case 'f': Onset[0] = EC::F; break;
			case 'b': Onset[0] = EC::B; break;
			case 'p': Onset[0] = EC::P; break;
			case 'm': Onset[0] = EC::M; break;
			case 'y': Onset[0] = EC::Y; break;
			// The Japanese r is a tap, nearer a quick d or l than an English r
			case 'r': Onset[0] = EC::L; break;
			case 'w': Onset[0] = EC::W; break;
			case 'v': Onset[0] = EC::V; break;
			default: break;
			}
		}
	}

	/** Reads a kana character into Out, merging small kana and marks into the syllable before */
	void ReadKana(UTF32CHAR C, int32 Position, TArray<FRawSyllable>& Out, int32 FirstOfRun)
	{
		FRawSyllable* Previous = Out.Num() > FirstOfRun ? &Out.Last() : nullptr;

		if (IsKanaLengthMark(C))
		{
			if (Previous)
			{
				Previous->bLong = true;
			}
			return;
		}

		const UTF32CHAR Hira = IsKatakana(C) ? C - 0x60 : C;
		if (!IsHiragana(Hira))
		{
			return;
		}

		const FString Romaji = KanaRomaji[Hira - 0x3041];
		if (Romaji == TEXT("n"))
		{
			if (Previous && Previous->Coda == EC::None)
			{
				Previous->Coda = EC::N;
				Previous->bLong = true;
				return;
			}
		}
		else if (Romaji == TEXT("xtsu"))
		{
			// A doubled consonant: the syllable before closes and holds
			if (Previous)
			{
				Previous->Coda = EC::T;
			}
			return;
		}
		else if (Romaji[0] == 'x' && Previous)
		{
			// ゃ, ゅ, ょ and the small vowels rewrite the vowel of the kana they follow: きゃ is kya
			EC Onset[2];
			EV Vowel;
			ReadRomaji(Romaji.RightChop(1), Onset, Vowel);
			if (Onset[0] == EC::Y && Previous->Onset[1] == EC::None && Previous->Onset[0] != EC::SH
				&& Previous->Onset[0] != EC::CH && Previous->Onset[0] != EC::J)
			{
				Previous->Onset[1] = EC::Y;
			}
			Previous->Vowel = Pure(Vowel);
			return;
		}

		FRawSyllable Syllable;
		Syllable.Trigger = Position;
		EV Vowel;
		ReadRomaji(Romaji[0] == 'x' ? Romaji.RightChop(1) : Romaji, Syllable.Onset, Vowel);
		Syllable.Vowel = Pure(Vowel);
		if (Romaji == TEXT("n"))
		{
			Syllable.Onset[0] = EC::N;
			Syllable.Vowel = Pure(EV::UH);
			Syllable.Reduction = 0.6f;
		}
		Out.Add(Syllable);
	}

	/** A syllable chosen by the character, for scripts with no reading here */
	FRawSyllable HashedSyllable(UTF32CHAR C, int32 Position)
	{
		static const EC Onsets[] = { EC::K, EC::S, EC::T, EC::N, EC::H, EC::M, EC::Y, EC::L, EC::W, EC::G, EC::Z, EC::D, EC::B, EC::None, EC::SH, EC::CH };
		static const EV Vowels[] = { EV::AA, EV::IY, EV::UH, EV::EH, EV::OW, EV::AA, EV::OW };

		uint32 Hash = uint32(C) * 2654435761u;
		Hash ^= Hash >> 15;

		FRawSyllable Syllable;
		Syllable.Trigger = Position;
		Syllable.Onset[0] = Onsets[Hash % UE_ARRAY_COUNT(Onsets)];
		const EV Start = Vowels[(Hash >> 8) % UE_ARRAY_COUNT(Vowels)];
		const EV End = Vowels[(Hash >> 16) % UE_ARRAY_COUNT(Vowels)];

		// An ideograph is often two morae: glide to a second vowel and hold it longer
		const bool bTwo = IsIdeograph(C) && ((Hash >> 24) & 1);
		Syllable.Vowel = { Start, bTwo ? End : Start };
		Syllable.bLong = bTwo;
		return Syllable;
	}

	// ---- The tune --------------------------------------------------------------------------------

	void ApplyProsody(TArray<FRawSyllable>& Raw, const TArray<FPhrase>& Phrases, const FMumbleProsody& Prosody,
		FRandomStream& Random, int32 NumChars, TArray<FMumblePlannedSyllable>& Out)
	{
		const float Expr = Prosody.Expressiveness;
		Out.Reset(Raw.Num());

		int32 First = 0;
		while (First < Raw.Num())
		{
			const int32 PhraseIndex = Raw[First].Phrase;
			int32 Last = First;
			while (Last + 1 < Raw.Num() && Raw[Last + 1].Phrase == PhraseIndex)
			{
				++Last;
			}

			const FPhrase Phrase = Phrases.IsValidIndex(PhraseIndex) ? Phrases[PhraseIndex] : FPhrase();
			const int32 Count = Last - First + 1;

			// The nuclear accent: the last stressed syllable of the phrase
			int32 Nuclear = Last;
			for (int32 I = Last; I >= First; --I)
			{
				if (Raw[I].bStressed)
				{
					Nuclear = I;
					break;
				}
			}

			// A phrase restarts the tune, a little lower each time within one sentence
			float Start = -FMath::Min(0.5f * Phrase.IndexInSentence, 1.5f);
			if (Phrase.Type == EPhrase::Exclaim)
			{
				Start += Prosody.ExclamationLift * Expr;
			}

			for (int32 I = First; I <= Last; ++I)
			{
				const FRawSyllable& R = Raw[I];
				const float Along = Count > 1 ? float(I - First) / float(Count - 1) : 0.f;

				float Pitch = Start - Prosody.Declination * Along;
				float Glide = 0.f;
				float Loud = 1.f;
				float Length = R.bLong ? 1.6f : 1.f;
				float Effort = 0.f;

				if (R.bStressed)
				{
					Pitch += Prosody.PitchRange * 0.5f;
					Loud = 1.1f;
					Length *= 1.15f;
				}
				else
				{
					Pitch -= 0.5f;
					Loud = R.Reduction > 0.f ? 0.8f : 0.92f;
					Length *= R.Reduction > 0.f ? 0.85f : 1.f;
				}

				if (I == Nuclear && R.bStressed)
				{
					Pitch += Prosody.PitchRange * 0.25f;
					if (Phrase.Type == EPhrase::Statement || Phrase.Type == EPhrase::Exclaim || Phrase.Type == EPhrase::Trail)
					{
						Glide = -Prosody.PitchRange * 0.6f;
					}
				}

				if (Phrase.Type == EPhrase::Question && I >= Nuclear)
				{
					// Up from the last stressed word to the end
					const float Steps = float(Last - Nuclear + 1);
					const float Rise = Prosody.QuestionRise * Expr;
					Pitch += Rise * float(I - Nuclear) / Steps;
					Glide = Rise / Steps;
				}
				else if (Phrase.Type == EPhrase::Continuation && I == Last)
				{
					// A comma leaves the tune hanging, a little up
					Glide = 1.f * Expr;
				}

				if (Phrase.Type == EPhrase::Exclaim)
				{
					Loud *= 1.15f;
					Effort = 0.45f;
				}

				if (Phrase.Type == EPhrase::Trail && I > Last - 3)
				{
					// The voice gives up as it trails off
					const float Fade = float(I - (Last - 3));
					Loud *= 1.f - 0.15f * Fade;
					Pitch -= 0.8f * Fade;
					Length *= 1.15f;
				}

				if (R.bShout)
				{
					Pitch += Prosody.ShoutLift * FMath::Max(Expr, 0.5f);
					Loud *= 1.35f;
					Effort = 1.f;
				}

				Pitch += Random.FRandRange(-1.f, 1.f) * Prosody.Randomness;

				FMumblePlannedSyllable Planned;
				Planned.TriggerChar = R.Trigger;
				Planned.Sound.Onset[0] = R.Onset[0];
				Planned.Sound.Onset[1] = R.Onset[1];
				Planned.Sound.Coda = R.Coda;
				Planned.Sound.VowelStart = R.Vowel.Start;
				Planned.Sound.VowelEnd = R.Vowel.End;
				Planned.Sound.PitchStartHz = Pitch;
				Planned.Sound.PitchEndHz = Pitch + Glide;
				Planned.Sound.Loudness = Loud;
				Planned.Sound.Effort = Effort;
				Planned.Sound.Reduction = R.Reduction;

				if (I == Last)
				{
					Planned.Sound.bPhraseFinal = true;
					Length *= 1.4f;
					switch (Phrase.Type)
					{
					case EPhrase::Statement:	Planned.Sound.Creak = 1.f; break;
					case EPhrase::Trail:		Planned.Sound.Creak = 1.f; break;
					case EPhrase::Continuation:	Planned.Sound.Creak = 0.3f; break;
					default:					Planned.Sound.Creak = 0.f; break;
					}
				}

				Planned.DurationScale = Length;

				const int32 NextTrigger = I < Last ? Raw[I + 1].Trigger : FMath::Max(Phrase.EndChar, R.Trigger + 1);
				Planned.CharSpan = FMath::Clamp(NextTrigger - R.Trigger, 1, 12);
				Out.Add(Planned);
			}

			First = Last + 1;
		}
	}
}

FMumbleLine FMumbleLine::Analyze(const FString& Text, const FMumbleProsody& Prosody, uint32 Seed)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleLine::Analyze);

	using namespace MumbleText;

	FMumbleLine Line;

	TArray<UTF32CHAR> Chars;
	ToCodepoints(Text, Chars);
	Line.NumChars = Chars.Num();

	TArray<FRawSyllable> Raw;
	TArray<FPhrase> Phrases;
	Phrases.AddDefaulted();

	int32 SentencePhrase = 0;
	auto ClosePhrase = [&](EPunct Ending, int32 EndChar)
	{
		FPhrase& Phrase = Phrases.Last();
		Phrase.EndChar = EndChar;
		Phrase.IndexInSentence = SentencePhrase;
		switch (Ending)
		{
		case EPunct::Comma:
		case EPunct::Dash:		Phrase.Type = EPhrase::Continuation; break;
		case EPunct::Question:	Phrase.Type = EPhrase::Question; break;
		case EPunct::Exclaim:	Phrase.Type = EPhrase::Exclaim; break;
		case EPunct::Ellipsis:	Phrase.Type = EPhrase::Trail; break;
		default:				Phrase.Type = EPhrase::Statement; break;
		}

		const bool bSentenceEnds = Ending != EPunct::Comma && Ending != EPunct::Dash;
		SentencePhrase = bSentenceEnds ? 0 : SentencePhrase + 1;
		Phrases.AddDefaulted();
	};

	const int32 Num = Chars.Num();
	int32 LastLetter = 0;
	int32 Index = 0;
	while (Index < Num)
	{
		const UTF32CHAR C = Chars[Index];
		const int32 PhraseIndex = Phrases.Num() - 1;
		const int32 Before = Raw.Num();

		if (IsLatin(C))
		{
			// One word: letters with apostrophes inside it
			FString Word;
			TArray<int32, TInlineAllocator<32>> Positions;
			int32 Upper = 0;
			int32 Letters = 0;
			while (Index < Num && (IsLatin(Chars[Index]) || (IsApostrophe(Chars[Index]) && Index + 1 < Num && IsLatin(Chars[Index + 1]))))
			{
				if (IsLatin(Chars[Index]))
				{
					Word.AppendChar(TCHAR(FoldLatin(Chars[Index])));
					Positions.Add(Index);
					++Letters;
					Upper += IsUpperLatin(Chars[Index]) ? 1 : 0;
				}
				else
				{
					Word.AppendChar('\'');
					Positions.Add(Index);
				}
				++Index;
			}

			// Contractions are read without the apostrophe, but checked against the word list with it
			const FString Bare = Word.Replace(TEXT("'"), TEXT(""));
			TArray<int32, TInlineAllocator<32>> BarePositions;
			for (int32 I = 0; I < Word.Len(); ++I)
			{
				if (Word[I] != '\'')
				{
					BarePositions.Add(Positions[I]);
				}
			}

			const bool bShout = Letters >= 2 && Upper == Letters;
			const int32 First = Raw.Num();
			ReadEnglishWord(Bare, BarePositions, bShout, Raw);

			// A listed contraction is still a function word once its apostrophe is gone
			if (IsOneOf(Word, FunctionWords))
			{
				for (int32 I = First; I < Raw.Num(); ++I)
				{
					Raw[I].bStressed = false;
					Raw[I].Reduction = FMath::Max(Raw[I].Reduction, 0.3f);
				}
			}
			LastLetter = Index;
		}
		else if (IsHiragana(C) || IsKatakana(C) || IsKanaLengthMark(C))
		{
			const int32 FirstOfRun = Raw.Num();
			while (Index < Num && (IsHiragana(Chars[Index]) || IsKatakana(Chars[Index]) || IsKanaLengthMark(Chars[Index])))
			{
				ReadKana(Chars[Index], Index, Raw, FirstOfRun);
				++Index;
			}

			// Japanese carries its accent on the second mora of a phrase more often than not
			if (Raw.Num() > FirstOfRun + 1)
			{
				Raw[FirstOfRun + 1].bStressed = true;
			}
			LastLetter = Index;
		}
		else if (IsIdeograph(C) || IsHangul(C) || FChar::IsDigit(TCHAR(C)))
		{
			Raw.Add(HashedSyllable(C, Index));
			++Index;
			LastLetter = Index;
		}
		else if (C >= 0x370 && C < 0x10000 && FChar::IsAlpha(TCHAR(C)))
		{
			// Another alphabet: a syllable every second letter of a word
			int32 InWord = 0;
			while (Index < Num && Chars[Index] >= 0x370 && Chars[Index] < 0x10000 && FChar::IsAlpha(TCHAR(Chars[Index])))
			{
				if ((InWord++ & 1) == 0)
				{
					Raw.Add(HashedSyllable(Chars[Index], Index));
				}
				++Index;
			}
			LastLetter = Index;
		}
		else
		{
			const EPunct Ending = EndingPunctuation(Chars, Index);
			const bool bHasSyllables = Raw.Num() > 0 && Raw.Last().Phrase == PhraseIndex;
			if (Ending != EPunct::None && bHasSyllables)
			{
				ClosePhrase(Ending, LastLetter);
			}
			++Index;
		}

		for (int32 I = Before; I < Raw.Num(); ++I)
		{
			Raw[I].Phrase = PhraseIndex;
		}
	}

	if (Raw.Num() > 0 && Raw.Last().Phrase == Phrases.Num() - 1)
	{
		ClosePhrase(EPunct::Period, LastLetter);
	}

	// The same line in the same voice always says itself the same way
	FRandomStream Random(int32(GetTypeHash(Text) ^ Seed));
	ApplyProsody(Raw, Phrases, Prosody, Random, Num, Line.Syllables);
	return Line;
}

FMumbleSyllable FMumbleLine::Realise(const FMumblePlannedSyllable& Planned, const FMumbleVoiceSynth& Synth, const FMumbleProsody& Prosody, float CharInterval)
{
	FMumbleSyllable Sound = Planned.Sound;
	Sound.PitchStartHz = Synth.PitchHz * FMath::Pow(2.f, Planned.Sound.PitchStartHz / 12.f);
	Sound.PitchEndHz = Synth.PitchHz * FMath::Pow(2.f, Planned.Sound.PitchEndHz / 12.f);
	Sound.Duration = FMath::Clamp(Planned.CharSpan * CharInterval * Prosody.SyllableLength * Planned.DurationScale, 0.06f, 0.5f);
	return Sound;
}
