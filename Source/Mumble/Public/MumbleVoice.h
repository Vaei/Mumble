// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MumbleTypes.h"
#include "MumbleVoice.generated.h"

class USoundBase;

/**
 * A character's voice: how they sound, how they move through a line, and how fast they talk.
 *
 * Giving a character a voice is pointing their Mumble component at one of these.
 */
UCLASS(BlueprintType)
class MUMBLE_API UMumbleVoice : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice, meta = (ShowOnlyInnerProperties))
	FMumbleVoiceSynth Synth;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Prosody, meta = (ShowOnlyInnerProperties))
	FMumbleProsody Prosody;

	/** Only for lines Mumble types out itself with Speak */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Timing, meta = (ShowOnlyInnerProperties))
	FMumbleTiming Timing;

	/** Two voices with the same settings but different seeds say the same line differently */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice)
	int32 Seed = 0;

	/**
	 * A MetaSound to voice this with instead of the default, to add reverb, a radio filter or anything else.
	 *
	 * It needs an Int32 input named "Mumble Stream" wired to a Mumble Voice node.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Voice)
	TObjectPtr<USoundBase> Sound;
};
