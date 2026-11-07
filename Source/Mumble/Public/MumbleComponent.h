// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Components/AudioComponent.h"
#include "MumbleText.h"
#include "MumbleComponent.generated.h"

class UMumbleVoice;
class FMumbleStream;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMumbleCharacterRevealed, int32, CharIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FMumbleLineFinished);

/**
 * Speaks a line of text in a voice, as it is revealed.
 *
 * Two ways to drive it, depending on who owns the timing:
 *
 * - Something else types the line (Folio, a UMG typewriter, a subtitle system): call BeginLine
 *   with the whole line, RevealTo as characters appear, and EndLine when it is done or skipped.
 *   Mumble follows and measures how fast the text is coming to size its syllables.
 * - Mumble types it: call Speak. The voice's Timing paces it, with its own pauses after
 *   punctuation, and OnCharacterRevealed reports each character so a text box can follow along.
 *
 * Is the audio component itself, so attenuation, concurrency and sound class work as on any other.
 * Leave attenuation empty for a voice in the UI.
 */
UCLASS(ClassGroup = Audio, meta = (BlueprintSpawnableComponent))
class MUMBLE_API UMumbleComponent : public UAudioComponent
{
	GENERATED_BODY()

public:
	UMumbleComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Who is talking. Falls back to the project's default voice. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Mumble)
	TObjectPtr<UMumbleVoice> Voice;

	/** Multiplies the pace of a line from Speak. Two is twice as fast. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mumble, meta = (ClampMin = "0.1"))
	float SpeedScale = 1.f;

	/** How long the MetaSound is kept alive after a line, so the next line does not restart it */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Mumble, meta = (ClampMin = "0", Units = "s"))
	float LingerTime = 2.f;

	/** A character of a line from Speak appeared. Once a frame at most, with the newest index. */
	UPROPERTY(BlueprintAssignable, Category = Mumble)
	FMumbleCharacterRevealed OnCharacterRevealed;

	/** A line from Speak has been fully revealed, or skipped */
	UPROPERTY(BlueprintAssignable, Category = Mumble)
	FMumbleLineFinished OnLineFinished;

	UFUNCTION(BlueprintCallable, Category = Mumble)
	void SetVoice(UMumbleVoice* NewVoice);

	/** @return The voice in use, which may be the project default */
	UFUNCTION(BlueprintPure, Category = Mumble)
	UMumbleVoice* GetEffectiveVoice() const;

	/**
	 * Types and speaks a line, paced by the voice's Timing.
	 *
	 * @return Seconds until the last character is revealed
	 */
	UFUNCTION(BlueprintCallable, Category = Mumble)
	float Speak(const FString& Text);

	/** Reveals the rest of a line from Speak at once and stops talking */
	UFUNCTION(BlueprintCallable, Category = Mumble)
	void Skip();

	/**
	 * Starts a line another system is typing. Call RevealTo as it types and EndLine when it is done.
	 *
	 * @param Text  The line as shown, without markup. Character indices count code points.
	 */
	UFUNCTION(BlueprintCallable, Category = Mumble)
	void BeginLine(const FString& Text);

	/** Every character up to and including this index is now showing */
	UFUNCTION(BlueprintCallable, Category = Mumble)
	void RevealTo(int32 CharIndex);

	/**
	 * The line is over.
	 *
	 * @param bSkipped  It was cut short, so whatever is sounding stops rather than finishing its syllable
	 */
	UFUNCTION(BlueprintCallable, Category = Mumble)
	void EndLine(bool bSkipped);

	/** True from BeginLine or Speak until the line ends */
	UFUNCTION(BlueprintPure, Category = Mumble)
	bool IsSpeaking() const { return bLineActive; }

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void BeginDestroy() override;

private:
	void EnsureStream();
	void EnsurePlaying();
	void ReleaseStream();
	void Emit(const FMumblePlannedSyllable& Planned);
	void FinishSelfTimed();

	FMumbleLine Line;
	int32 NextSyllable = 0;
	int32 LastRevealed = INDEX_NONE;
	bool bLineActive = false;

	/** Seconds per character, measured from how fast the text is arriving */
	float CharInterval = 1.f / 32.f;
	double LastRevealTime = 0.0;

	bool bSelfTimed = false;
	double SelfTimedStart = 0.0;
	TArray<float> Schedule;

	double LingerUntil = 0.0;

	int32 StreamId = 0;
	TSharedPtr<FMumbleStream, ESPMode::ThreadSafe> Stream;
};
