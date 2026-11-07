// Copyright (c) Jared Taylor

#pragma once

#include "Modules/ModuleManager.h"

class FMumbleModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
