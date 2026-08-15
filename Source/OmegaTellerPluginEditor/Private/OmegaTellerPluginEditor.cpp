// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaTellerPluginEditor.h"
#include "OmegaLocalAI.h"
#include "OmegaTellerPluginEditorStyle.h"
#include "OmegaTellerPluginEditorCommands.h"
#include "SOmegaModernEditor.h"
#include "Misc/MessageDialog.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "OmegaTellerManager.h"

static const FName OmegaTellerPluginTabName("OmegaTellerPlugin");

#define LOCTEXT_NAMESPACE "FOmegaTellerPluginEditorModule"

void FOmegaTellerPluginEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

	// Load the WebBrowser module explicitly: SWebBrowserView only creates a browser when
	// FModuleManager reports the module as loaded, and nothing else in a vanilla editor
	// session loads it (Epic relies on the WebBrowserWidget plugin for that). Without this,
	// the OmegaTeller tab renders as an empty window because CEF never initializes.
	FModuleManager::Get().LoadModuleChecked(TEXT("WebBrowser"));

	FOmegaTellerPluginEditorStyle::Initialize();
	FOmegaTellerPluginEditorStyle::ReloadTextures();

	FOmegaTellerPluginEditorCommands::Register();
	
	PluginCommands = MakeShareable(new FUICommandList);

	PluginCommands->MapAction(
		FOmegaTellerPluginEditorCommands::Get().OpenPluginWindow,
		FExecuteAction::CreateRaw(this, &FOmegaTellerPluginEditorModule::PluginButtonClicked),
		FCanExecuteAction());

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FOmegaTellerPluginEditorModule::RegisterMenus));
	
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(OmegaTellerPluginTabName, FOnSpawnTab::CreateRaw(this, &FOmegaTellerPluginEditorModule::OnSpawnPluginTab))
		.SetDisplayName(LOCTEXT("FOmegaTellerPluginTabTitle", "OmegaTeller"))
		.SetMenuType(ETabSpawnerMenuType::Hidden);
}

void FOmegaTellerPluginEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	// Stop the bundled local LLM server so no orphan process outlives the editor
	FOmegaLocalAI::Get().ShutdownServer();

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FOmegaTellerPluginEditorStyle::Shutdown();

	FOmegaTellerPluginEditorCommands::Unregister();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(OmegaTellerPluginTabName);
}

TSharedRef<SDockTab> FOmegaTellerPluginEditorModule::OnSpawnPluginTab(const FSpawnTabArgs& SpawnTabArgs)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SOmegaModernEditor)
		];
}

void FOmegaTellerPluginEditorModule::PluginButtonClicked()
{
	FGlobalTabmanager::Get()->TryInvokeTab(OmegaTellerPluginTabName);
}

void FOmegaTellerPluginEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FOmegaTellerPluginEditorCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}

	{
		UToolMenu* ToolbarMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		{
			FToolMenuSection& Section = ToolbarMenu->FindOrAddSection("PluginTools");
			{
				FToolMenuEntry& Entry = Section.AddEntry(FToolMenuEntry::InitToolBarButton(FOmegaTellerPluginEditorCommands::Get().OpenPluginWindow));
				Entry.SetCommandList(PluginCommands);
				// Entry.SetIcon(FOmegaTellerPluginEditorStyle::Get().GetBrush("OmegaTellerPluginEditor.OpenPluginWindow"));
			}
		}
	}

	// Add to Tools menu as well
	{
		UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Tools");
		{
			FToolMenuSection& Section = ToolsMenu->FindOrAddSection("OmegaTeller");
			Section.AddMenuEntryWithCommandList(FOmegaTellerPluginEditorCommands::Get().OpenPluginWindow, PluginCommands);
		}
	}
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FOmegaTellerPluginEditorModule, OmegaTellerPluginEditor)