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
	 * Leave empty to use one built at runtime: a Mumble Voice node and nothing else. To add effects to
	 * every voice, press Generate Default Sound for an editable copy, or point this at your own. It
	 * needs an Int32 input named "Mumble Stream" wired to a Mumble Voice node.
	 */
	UPROPERTY(Config, EditAnywhere, Category = Mumble)
	TSoftObjectPtr<USoundBase> DefaultSound;

	/** Where Generate Default Sound writes its MetaSound */
	UPROPERTY(Config, EditAnywhere, Category = Mumble, meta = (ContentDir, LongPackageName))
	FDirectoryPath GeneratedSoundFolder;

#if WITH_EDITOR
	/** Writes the runtime MetaSound out as an asset in Generated Sound Folder, and makes it the Default Sound */
	UFUNCTION(CallInEditor, Category = Mumble)
	void GenerateDefaultSound();

	/** Builds the asset. Installed by the MumbleEditor module, which owns the editor-only MetaSound API. */
	static TFunction<USoundBase*(const FString& Folder)> GenerateSoundHook;
#endif
};
