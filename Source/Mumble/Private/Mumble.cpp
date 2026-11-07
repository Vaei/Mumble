// Copyright (c) Jared Taylor

#include "Mumble.h"

#include "MetasoundFrontendModuleRegistrationMacros.h"

METASOUND_IMPLEMENT_MODULE_REGISTRATION_LIST

void FMumbleModule::StartupModule()
{
	FModuleManager::Get().LoadModuleChecked("MetasoundFrontend");
	FModuleManager::Get().LoadModuleChecked("MetasoundEngine");

	METASOUND_REGISTER_ITEMS_IN_MODULE
}

void FMumbleModule::ShutdownModule()
{
	METASOUND_UNREGISTER_ITEMS_IN_MODULE
}

IMPLEMENT_MODULE(FMumbleModule, Mumble)
