// Copyright (c) Jared Taylor

#include "MumbleEditor.h"

#include "AssetToolsModule.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "MetasoundBuilderSubsystem.h"
#include "MetasoundEditorSubsystem.h"
#include "MumbleSettings.h"
#include "MumbleSubsystem.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogMumbleEditor, Log, All);

namespace MumbleEditor
{
	USoundBase* GenerateSound(const FString& Folder)
	{
		UMetaSoundSourceBuilder* Builder = UMumbleSubsystem::CreateVoiceGraph(TEXT("MumbleGeneratedVoice"));
		if (!Builder)
		{
			return nullptr;
		}

		// A fresh name every time: building over a package the registry already knows hangs the editor
		FString PackageName;
		FString AssetName;
		const FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
		AssetTools.Get().CreateUniqueAssetName(Folder / TEXT("MS_MumbleVoice"), FString(), PackageName, AssetName);

		EMetaSoundBuilderResult Result = EMetaSoundBuilderResult::Failed;
		const TScriptInterface<IMetaSoundDocumentInterface> Built = UMetaSoundEditorSubsystem::GetChecked().BuildToAsset(
			Builder, TEXT("Mumble"), AssetName, Folder, Result);

		USoundBase* Sound = Cast<USoundBase>(Built.GetObject());
		if (Result != EMetaSoundBuilderResult::Succeeded || !Sound)
		{
			UE_LOG(LogMumbleEditor, Error, TEXT("Could not generate %s/%s."), *Folder, *AssetName);
			return nullptr;
		}

		UEditorLoadingAndSavingUtils::SavePackages({ Sound->GetPackage() }, false);
		if (GEditor)
		{
			TArray<UObject*> Generated{ Sound };
			GEditor->SyncBrowserToObjects(Generated);
		}

		UE_LOG(LogMumbleEditor, Display, TEXT("Generated %s and made it Mumble's Default Sound."), *Sound->GetPathName());
		return Sound;
	}
}

void FMumbleEditorModule::StartupModule()
{
	UMumbleSettings::GenerateSoundHook = &MumbleEditor::GenerateSound;
}

void FMumbleEditorModule::ShutdownModule()
{
	UMumbleSettings::GenerateSoundHook = nullptr;
}

IMPLEMENT_MODULE(FMumbleEditorModule, MumbleEditor)
