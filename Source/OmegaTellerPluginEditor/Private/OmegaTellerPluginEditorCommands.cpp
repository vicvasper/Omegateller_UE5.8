// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaTellerPluginEditorCommands.h"

#define LOCTEXT_NAMESPACE "FOmegaTellerPluginEditorCommands"

void FOmegaTellerPluginEditorCommands::RegisterCommands()
{
	UI_COMMAND(OpenPluginWindow, "OmegaTeller", "Open OmegaTeller narrative tree editor", EUserInterfaceActionType::Button, FInputChord());
}

#undef LOCTEXT_NAMESPACE