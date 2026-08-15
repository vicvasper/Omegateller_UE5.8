// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FOmegaTellerPluginModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	
	/** Get the singleton instance */
	static FOmegaTellerPluginModule& Get();
	
	/** Check if module is fully loaded */
	bool IsModuleLoaded() const { return bModuleLoaded; }

private:
	/** Internal module state */
	bool bModuleLoaded = false;
};
