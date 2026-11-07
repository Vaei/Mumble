// Copyright (c) Jared Taylor

#include "MetasoundAudioBuffer.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundParamHelper.h"
#include "MetasoundPrimitives.h"
#include "MetasoundStandardNodesCategories.h"
#include "MumbleStream.h"
#include "MumbleSynth.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

#define LOCTEXT_NAMESPACE "MumbleVoiceNode"

namespace Mumble
{
	using namespace Metasound;

	namespace VoiceVertexNames
	{
		METASOUND_PARAM(InputStream, "Stream", "The Mumble stream to voice. Wire the graph's \"Mumble Stream\" Int32 input here.");
		METASOUND_PARAM(OutputAudio, "Audio", "The voice.");
	}

	/** Voices whatever a UMumbleComponent sends down its stream */
	class FMumbleVoiceOperator : public TExecutableOperator<FMumbleVoiceOperator>
	{
	public:
		FMumbleVoiceOperator(const FBuildOperatorParams& InParams, const FInt32ReadRef& InStream)
			: Stream(InStream)
			, AudioOutput(FAudioBufferWriteRef::CreateNew(InParams.OperatorSettings))
			, SampleRate(InParams.OperatorSettings.GetSampleRate())
		{
			Syllables.Reserve(16);
			Reset(InParams);
		}

		static const FNodeClassMetadata& GetNodeInfo()
		{
			auto CreateNodeClassMetadata = []() -> FNodeClassMetadata
			{
				FNodeClassMetadata Metadata
				{
					FNodeClassName { "Mumble", "Voice", "" },
					1, // Major Version
					0, // Minor Version
					METASOUND_LOCTEXT("MumbleVoiceDisplayName", "Mumble Voice"),
					METASOUND_LOCTEXT("MumbleVoiceDesc", "Synthesises mumbled speech for the syllables a Mumble component sends."),
					TEXT("Jared Taylor"),
					PluginNodeMissingPrompt,
					DeclareVertexInterface(),
					{ NodeCategories::Generators },
					{ },
					FNodeDisplayStyle()
				};
				return Metadata;
			};

			static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
			return Metadata;
		}

		static const FVertexInterface& DeclareVertexInterface()
		{
			using namespace VoiceVertexNames;

			static const FVertexInterface Interface(
				FInputVertexInterface(
					TInputDataVertex<int32>(METASOUND_GET_PARAM_NAME_AND_METADATA(InputStream), 0)
				),
				FOutputVertexInterface(
					TOutputDataVertex<FAudioBuffer>(METASOUND_GET_PARAM_NAME_AND_METADATA(OutputAudio))
				)
			);
			return Interface;
		}

		virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
		{
			using namespace VoiceVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(InputStream), Stream);
		}

		virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
		{
			using namespace VoiceVertexNames;
			InOutVertexData.BindReadVertex(METASOUND_GET_PARAM_NAME(OutputAudio), AudioOutput);
		}

		static TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
		{
			using namespace VoiceVertexNames;
			FInt32ReadRef StreamIn = InParams.InputData.GetOrCreateDefaultDataReadReference<int32>(METASOUND_GET_PARAM_NAME(InputStream), InParams.OperatorSettings);
			return MakeUnique<FMumbleVoiceOperator>(InParams, StreamIn);
		}

		void Reset(const IOperator::FResetParams& InParams)
		{
			AudioOutput->Zero();
			BoundId = 0;
			Bound.Reset();
			VoiceSerial = 0;
			SilenceSerial = 0;
			Synth.Init(SampleRate, 0);
		}

		void Execute()
		{
			TRACE_CPUPROFILER_EVENT_SCOPE(MumbleVoiceOperator::Execute);

			if (*Stream != BoundId)
			{
				BoundId = *Stream;
				Bound = MumbleStreams::Find(BoundId);
				VoiceSerial = 0;
				Synth.Init(SampleRate, uint32(BoundId) * 2654435761u);
			}

			if (Bound.IsValid())
			{
				FMumbleVoiceSynth Voice;
				const uint32 PreviousVoice = VoiceSerial;
				const bool bSilence = Bound->Drain(Syllables, Voice, VoiceSerial, SilenceSerial);
				if (VoiceSerial != PreviousVoice)
				{
					Synth.SetVoice(Voice);
				}
				if (bSilence)
				{
					Synth.Silence();
				}

				// Only the newest matters: a syllable replaces the one before it from wherever it got to
				if (Syllables.Num() > 0)
				{
					Synth.Speak(Syllables.Last());
				}
			}

			Synth.Render(AudioOutput->GetData(), AudioOutput->Num());
		}

	private:
		FInt32ReadRef Stream;
		FAudioBufferWriteRef AudioOutput;
		float SampleRate;

		int32 BoundId = 0;
		FMumbleStreamPtr Bound;
		uint32 VoiceSerial = 0;
		uint32 SilenceSerial = 0;
		TArray<FMumbleSyllable> Syllables;
		FMumbleSynth Synth;
	};

	using FMumbleVoiceNode = TNodeFacade<FMumbleVoiceOperator>;

	METASOUND_REGISTER_NODE(FMumbleVoiceNode)
}

#undef LOCTEXT_NAMESPACE
