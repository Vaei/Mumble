// Copyright (c) Jared Taylor

#include "MumbleSubsystem.h"

#include "Engine/Engine.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendLiteral.h"
#include "MumbleSettings.h"
#include "MumbleStream.h"
#include "MumbleVoice.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Sound/SoundBase.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(MumbleSubsystem)

DEFINE_LOG_CATEGORY_STATIC(LogMumble, Log, All);

UMumbleSubsystem* UMumbleSubsystem::Get()
{
	return GEngine ? GEngine->GetEngineSubsystem<UMumbleSubsystem>() : nullptr;
}

USoundBase* UMumbleSubsystem::GetDefaultSound()
{
	if (DefaultSound)
	{
		return DefaultSound;
	}

	const UMumbleSettings* Settings = GetDefault<UMumbleSettings>();
	if (!Settings->DefaultSound.IsNull())
	{
		DefaultSound = Settings->DefaultSound.LoadSynchronous();
		if (DefaultSound)
		{
			return DefaultSound;
		}
		UE_LOG(LogMumble, Warning, TEXT("Default sound %s could not be loaded; building one instead."), *Settings->DefaultSound.ToString());
	}

	if (!bBuildFailed)
	{
		DefaultSound = BuildDefaultSound();
		bBuildFailed = DefaultSound == nullptr;
	}
	return DefaultSound;
}

UMumbleVoice* UMumbleSubsystem::GetDefaultVoice()
{
	if (!DefaultVoice)
	{
		const UMumbleSettings* Settings = GetDefault<UMumbleSettings>();
		if (!Settings->DefaultVoice.IsNull())
		{
			DefaultVoice = Settings->DefaultVoice.LoadSynchronous();
		}
	}
	return DefaultVoice;
}

USoundBase* UMumbleSubsystem::BuildDefaultSound()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(MumbleSubsystem::BuildDefaultSound);

	UMetaSoundBuilderSubsystem* Builders = UMetaSoundBuilderSubsystem::Get();
	if (!Builders)
	{
		UE_LOG(LogMumble, Error, TEXT("No MetaSound builder subsystem; Mumble cannot build its default voice."));
		return nullptr;
	}

	FMetaSoundBuilderNodeOutputHandle OnPlay;
	FMetaSoundBuilderNodeInputHandle OnFinished;
	TArray<FMetaSoundBuilderNodeInputHandle> AudioOuts;
	EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;

	UMetaSoundSourceBuilder* Builder = Builders->CreateSourceBuilder(TEXT("MumbleDefaultVoice"), OnPlay, OnFinished, AudioOuts,
		Result, EMetaSoundOutputAudioFormat::Mono, false);
	if (!Builder || Result != EMetaSoundBuilderResult::Succeeded || AudioOuts.Num() == 0)
	{
		UE_LOG(LogMumble, Error, TEXT("Could not create a MetaSound source builder for the default voice."));
		return nullptr;
	}

	FMetasoundFrontendLiteral NoStream;
	NoStream.Set(int32(0));
	const FMetaSoundBuilderNodeOutputHandle StreamInput = Builder->AddGraphInputNode(MumbleStreams::ParameterName(), TEXT("Int32"), NoStream, Result);
	if (Result != EMetaSoundBuilderResult::Succeeded)
	{
		UE_LOG(LogMumble, Error, TEXT("Could not add the \"%s\" input to the default voice."), *MumbleStreams::ParameterName().ToString());
		return nullptr;
	}

	const FMetaSoundNodeHandle Voice = Builder->AddNodeByClassName(FMetasoundFrontendClassName(TEXT("Mumble"), TEXT("Voice"), NAME_None), Result);
	if (Result != EMetaSoundBuilderResult::Succeeded)
	{
		UE_LOG(LogMumble, Error, TEXT("Could not add a Mumble Voice node: the node is not registered."));
		return nullptr;
	}

	Builder->ConnectNodes(StreamInput, Builder->FindNodeInputByName(Voice, TEXT("Stream"), Result), Result);
	const bool bInputConnected = Result == EMetaSoundBuilderResult::Succeeded;
	Builder->ConnectNodes(Builder->FindNodeOutputByName(Voice, TEXT("Audio"), Result), AudioOuts[0], Result);
	if (!bInputConnected || Result != EMetaSoundBuilderResult::Succeeded)
	{
		UE_LOG(LogMumble, Error, TEXT("Could not wire the default voice's graph."));
		return nullptr;
	}

	const TScriptInterface<IMetaSoundDocumentInterface> Built = Builder->BuildNewMetaSound(TEXT("MS_MumbleDefaultVoice"));
	USoundBase* Sound = Cast<USoundBase>(Built.GetObject());
	if (!Sound)
	{
		UE_LOG(LogMumble, Error, TEXT("Building the default voice MetaSound failed."));
	}
	return Sound;
}
