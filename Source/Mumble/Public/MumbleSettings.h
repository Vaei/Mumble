// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "MumbleSettings.generated.h"

class UMumbleVoice;
class USoundBase;

/** Project-wide Mumble defaults */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Mumble"))
class MUMBLE_API UMumbleSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMumbleSettings();

	/** Used by a Mumble component that has no voice of its own */
	UPROPERTY(Config, EditAnywhere, Category = Mumble)
	TSoftObjectPtr<UMumbleVoice> DefaultVoice;

	/**
	 * The MetaSound every voice plays through unless it names its own.
	 *
	 * Leave empty to use one built at runtime: a Mumble Voice node and nothing else. Set it to add
	 * effects to every voice at once. It needs an Int32 input named "Mumble Stream" wired to a
	 * Mumble Voice node.
	 */
	UPROPERTY(Config, EditAnywhere, Category = Mumble)
	TSoftObjectPtr<USoundBase> DefaultSound;
};
