// Copyright (c) Jared Taylor

#include "MumbleSettings.h"

#include "MumbleSubsystem.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MumbleSettings)

UMumbleSettings::UMumbleSettings()
{
	CategoryName = TEXT("Plugins");
	GeneratedSoundFolder.Path = TEXT("/Game/Audio/Mumble");
}

#if WITH_EDITOR
TFunction<USoundBase*(const FString&)> UMumbleSettings::GenerateSoundHook;

void UMumbleSettings::GenerateDefaultSound()
{
	if (!GenerateSoundHook)
	{
		return;
	}

	if (USoundBase* Generated = GenerateSoundHook(GeneratedSoundFolder.Path))
	{
		DefaultSound = Generated;
		TryUpdateDefaultConfigFile();

		if (UMumbleSubsystem* Subsystem = UMumbleSubsystem::Get())
		{
			Subsystem->ResetDefaultSound();
		}
	}
}
#endif
