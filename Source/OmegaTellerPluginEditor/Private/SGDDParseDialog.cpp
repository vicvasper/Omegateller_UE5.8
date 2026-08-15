// Copyright Epic Games, Inc. All Rights Reserved.

#include "SGDDParseDialog.h"
#include "OmegaTellerManager.h"
#include "OmegaCharacter.h"
#include "OmegaTree.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "OmegaTeller"

void SGDDParseDialog::Construct(const FArguments& InArgs)
{
	ParentWindow = InArgs._ParentWindow;
	bConfirmed = false;
	GDDText = TEXT("");

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(16.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 8)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GDDDialogTitle", "Parse Game Design Document"))
				.Font(FCoreStyle::Get().GetFontStyle("HeadingExtraSmall"))
			]
			+ SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				CreateContent()
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 8, 0, 0)
			[
				CreateButtons()
			]
		]
	];
}

TSharedRef<SWidget> SGDDParseDialog::CreateContent()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 0, 0, 8)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GDDInstructions", "Paste your Game Design Document below. The AI will extract characters and generate narrative trees."))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SAssignNew(GDDTextBox, SMultiLineEditableTextBox)
			.HintText(LOCTEXT("GDDHint", "Paste GDD text here..."))
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SAssignNew(ResultBox, SVerticalBox)
		];
}

TSharedRef<SWidget> SGDDParseDialog::CreateButtons()
{
	return SNew(SUniformGridPanel)
		.SlotPadding(4.0f)
		+ SUniformGridPanel::Slot(0, 0)
		[
			SAssignNew(LoadExampleButton, SButton)
			.Text(LOCTEXT("LoadExample", "Load Example"))
			.OnClicked(this, &SGDDParseDialog::OnLoadExampleClicked)
			.ToolTipText(LOCTEXT("LoadExampleTooltip", "Load an example GDD for testing"))
		]
		+ SUniformGridPanel::Slot(1, 0)
		[
			SNew(SSpacer)
		]
		+ SUniformGridPanel::Slot(2, 0)
		[
			SAssignNew(CancelButton, SButton)
			.Text(LOCTEXT("Cancel", "Cancel"))
			.OnClicked(this, &SGDDParseDialog::OnCancelClicked)
		]
		+ SUniformGridPanel::Slot(3, 0)
		[
			SAssignNew(ParseButton, SButton)
			.Text(LOCTEXT("ParseGDD", "Parse GDD"))
			.OnClicked(this, &SGDDParseDialog::OnParseClicked)
			.ButtonColorAndOpacity(FLinearColor::Green)
		];
}

FReply SGDDParseDialog::OnParseClicked()
{
	if (GDDTextBox.IsValid())
	{
		GDDText = GDDTextBox->GetText().ToString();
		
		if (!GDDText.IsEmpty())
		{
			bConfirmed = true;
			
			// Clear previous results
			ResultBox->ClearChildren();
			
			// Add parsing status
			ResultBox->AddSlot()
			.AutoHeight()
			.Padding(0, 8, 0, 0)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ParsingStatus", "Parsing GDD with AI..."))
				.ColorAndOpacity(FLinearColor::Yellow)
			];
			
			// Parse with AI (this would be async in a real implementation)
			ParseWithAI();
			
			// Close window
			if (ParentWindow.IsValid())
			{
				ParentWindow.Pin()->RequestDestroyWindow();
			}
		}
	}
	
	return FReply::Handled();
}

FReply SGDDParseDialog::OnCancelClicked()
{
	bConfirmed = false;
	
	if (ParentWindow.IsValid())
	{
		ParentWindow.Pin()->RequestDestroyWindow();
	}
	
	return FReply::Handled();
}

FReply SGDDParseDialog::OnLoadExampleClicked()
{
	LoadExampleGDD();
	return FReply::Handled();
}

void SGDDParseDialog::LoadExampleGDD()
{
	// Try to load from example file
	FString ExamplePath = FPaths::ProjectPluginsDir() / TEXT("OmegaTellerPlugin/Content/ExampleGDD.txt");
	
	FString ExampleText;
	if (FFileHelper::LoadFileToString(ExampleText, *ExamplePath))
	{
		if (GDDTextBox.IsValid())
		{
			GDDTextBox->SetText(FText::FromString(ExampleText));
		}
	}
	else
	{
		// Fallback example
		FString FallbackExample = TEXT("# Example Game Design Document\n\n");
		FallbackExample += TEXT("## Characters\n\n");
		FallbackExample += TEXT("### Elara Windrider\n");
		FallbackExample += TEXT("- Role: Protagonist\n");
		FallbackExample += TEXT("- Personality: Brave, Curious\n");
		FallbackExample += TEXT("- Goals: Uncover ancient secrets\n\n");
		FallbackExample += TEXT("### Lord Valerius Blackwood\n");
		FallbackExample += TEXT("- Role: Antagonist\n");
		FallbackExample += TEXT("- Personality: Ambitious, Cunning\n");
		FallbackExample += TEXT("- Goals: Seize the throne\n\n");
		FallbackExample += TEXT("## Story Beats\n\n");
		FallbackExample += TEXT("1. Elara discovers mysterious artifact\n");
		FallbackExample += TEXT("2. Political tensions rise in the kingdom\n");
		FallbackExample += TEXT("3. Elara must choose allies carefully\n");
		FallbackExample += TEXT("4. Final confrontation with ancient evil\n");
		FallbackExample += TEXT("5. Multiple possible endings based on choices\n");
		
		if (GDDTextBox.IsValid())
		{
			GDDTextBox->SetText(FText::FromString(FallbackExample));
		}
	}
}

void SGDDParseDialog::ParseWithAI()
{
	if (GDDText.IsEmpty())
	{
		return;
	}
	
	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	if (!Manager || !ResultBox.IsValid())
	{
		return;
	}

	// Parse the GDD
	Manager->ParseGDD(GDDText);

	// Update results display
	ResultBox->ClearChildren();
	
	// Show results
	TArray<UOmegaCharacter*> Characters = Manager->GetAllCharacters();
	TArray<UOmegaTree*> Trees = Manager->GetAllTrees();
	
	ResultBox->AddSlot()
	.AutoHeight()
	.Padding(0, 8, 0, 0)
	[
		SNew(STextBlock)
		.Text(FText::Format(LOCTEXT("ParseResults", "Parsing complete! Created {0} characters and {1} trees."),
			FText::AsNumber(Characters.Num()),
			FText::AsNumber(Trees.Num())))
		.ColorAndOpacity(FLinearColor::Green)
	];
	
	// List characters
	if (Characters.Num() > 0)
	{
		ResultBox->AddSlot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("CharactersHeader", "Characters:"))
			.Font(FCoreStyle::Get().GetFontStyle("Bold"))
		];
		
		for (UOmegaCharacter* Character : Characters)
		{
			if (!Character)
			{
				continue;
			}

			ResultBox->AddSlot()
			.AutoHeight()
			.Padding(20, 2, 0, 2)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("- %s (%s)"),
					*Character->CharacterName,
					*Character->GetAttribute(TEXT("Role")))))
			];
		}
	}
	
	// List trees
	if (Trees.Num() > 0)
	{
		ResultBox->AddSlot()
		.AutoHeight()
		.Padding(0, 8, 0, 0)
		[
			SNew(STextBlock)
			.Text(LOCTEXT("TreesHeader", "Narrative Trees:"))
			.Font(FCoreStyle::Get().GetFontStyle("Bold"))
		];
		
		for (UOmegaTree* Tree : Trees)
		{
			if (!Tree)
			{
				continue;
			}

			ResultBox->AddSlot()
			.AutoHeight()
			.Padding(20, 2, 0, 2)
			[
				SNew(STextBlock)
				.Text(FText::FromString(FString::Printf(TEXT("- %s (%d nodes)"),
					*Tree->TreeName,
					Tree->AllNodes.Num())))
			];
		}
	}
}

#undef LOCTEXT_NAMESPACE