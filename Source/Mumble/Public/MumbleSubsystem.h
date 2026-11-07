// Copyright (c) Jared Taylor

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "MumbleSubsystem.generated.h"

class USoundBase;
class UMumbleVoice;

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

private:
	USoundBase* BuildDefaultSound();

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> DefaultSound;

	UPROPERTY(Transient)
	TObjectPtr<UMumbleVoice> DefaultVoice;

	bool bBuildFailed = false;
};
