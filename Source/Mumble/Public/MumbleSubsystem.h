// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "MumbleSubsystem.generated.h"

class USoundBase;
class UMumbleVoice;
class UMetaSoundSourceBuilder;

/** Holds what every Mumble component shares: the MetaSound to voice with, when none is given. */
UCLASS()
class MUMBLE_API UMumbleSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	static UMumbleSubsystem* Get();

	/** The project's default MetaSound, or one built at runtime from a single Mumble Voice node */
	USoundBase* GetDefaultSound();

	/** The project's default voice, if it has one */
	UMumbleVoice* GetDefaultVoice();

	/** Forgets the default sound, so a changed Default Sound setting is picked up by the next line */
	void ResetDefaultSound() { DefaultSound = nullptr; bBuildFailed = false; }

	/**
	 * A builder holding the default voice graph: a "Mumble Stream" Int32 input wired through a Mumble
	 * Voice node to the mono output. Null on failure, which is logged.
	 */
	static UMetaSoundSourceBuilder* CreateVoiceGraph(FName BuilderName);

private:
	USoundBase* BuildDefaultSound();

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> DefaultSound;

	UPROPERTY(Transient)
	TObjectPtr<UMumbleVoice> DefaultVoice;

	bool bBuildFailed = false;
};
