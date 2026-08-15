// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class SMultiLineEditableTextBox;
class SButton;
class SVerticalBox;

/**
 * Dialog for parsing Game Design Documents with AI
 */
class SGDDParseDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGDDParseDialog) {}
	
	SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Get the parsed GDD text
	FString GetGDDText() const { return GDDText; }

	// Get whether user confirmed
	bool IsConfirmed() const { return bConfirmed; }

private:
	// Widget creation
	TSharedRef<SWidget> CreateContent();
	TSharedRef<SWidget> CreateButtons();

	// Event handlers
	FReply OnParseClicked();
	FReply OnCancelClicked();
	FReply OnLoadExampleClicked();

	// Internal functions
	void LoadExampleGDD();
	void ParseWithAI();

private:
	// UI elements
	TSharedPtr<SMultiLineEditableTextBox> GDDTextBox;
	TSharedPtr<SButton> ParseButton;
	TSharedPtr<SButton> CancelButton;
	TSharedPtr<SButton> LoadExampleButton;
	TSharedPtr<SVerticalBox> ResultBox;

	// State
	FString GDDText;
	bool bConfirmed;
	TWeakPtr<SWindow> ParentWindow;
};