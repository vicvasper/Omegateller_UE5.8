// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaTellerPlugin.h"

#define LOCTEXT_NAMESPACE "FOmegaTellerPluginModule"

FOmegaTellerPluginModule& FOmegaTellerPluginModule::Get()
{
	return FModuleManager::LoadModuleChecked<FOmegaTellerPluginModule>("OmegaTellerPlugin");
}

void FOmegaTellerPluginModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	UE_LOG(LogTemp, Log, TEXT("OmegaTeller Plugin Module starting up..."));
	
	bModuleLoaded = true;
	
	// Log module initialization
	UE_LOG(LogTemp, Log, TEXT("OmegaTeller Plugin Module initialized successfully"));
}

void FOmegaTellerPluginModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
	
	UE_LOG(LogTemp, Log, TEXT("OmegaTeller Plugin Module shutting down..."));
	
	bModuleLoaded = false;
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOmegaTellerPluginModule, OmegaTellerPlugin)