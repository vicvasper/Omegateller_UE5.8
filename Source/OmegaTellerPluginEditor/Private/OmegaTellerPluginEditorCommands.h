// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/Commands.h"
#include "OmegaTellerPluginEditorStyle.h"

class FOmegaTellerPluginEditorCommands : public TCommands<FOmegaTellerPluginEditorCommands>
{
public:

	FOmegaTellerPluginEditorCommands()
		: TCommands<FOmegaTellerPluginEditorCommands>(TEXT("OmegaTellerPluginEditor"), NSLOCTEXT("Contexts", "OmegaTellerPluginEditor", "OmegaTellerPlugin Editor"), NAME_None, FOmegaTellerPluginEditorStyle::GetStyleSetName())
	{
	}

	// TCommands<> interface
	virtual void RegisterCommands() override;

public:
	TSharedPtr< FUICommandInfo > OpenPluginWindow;
};