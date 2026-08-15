// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "SWebBrowser.h"
#include "OmegaTree.h"
#include "UObject/StrongObjectPtr.h"

class UOmegaNode;
class UOmegaTree;
class UOmegaTellerManager;

/**
 * Web-based narrative graph editor.
 * Uses an embedded Chromium browser to render a custom HTML5 canvas
 * with a neural-network-style node graph for character story trees.
 */
class SOmegaModernEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOmegaModernEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SOmegaModernEditor();

	void SetTree(UOmegaTree* InTree);

protected:
	TSharedPtr<SWebBrowser> WebBrowser;
	TStrongObjectPtr<UOmegaTree> CurrentTree;

	// Browser callbacks
	void OnConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity);
	bool OnBeforeNavigation(const FString& URL, const FWebNavigationRequest& Request);
	void OnLoadComplete();

	// JS bridge
	void ExecJS(const FString& Script);
	void FlushPending();
	void SyncTreeToBrowser();

	// Incoming commands from browser
	void HandleBridgeMessage(const FString& Payload);
	void HandleGDDParseRequest(const FString& GDDText);
	void HandleStoryMergeRequest(const FString& JsonPayload);
	void HandleRepopulateRequest(const FString& JsonPayload);

	// Helpers
	FString ResolveHTMLPath() const;
	int32 NodeIndex(UOmegaNode* N) const;
	UOmegaNode* NodeAt(int32 Idx) const;

private:
	FString HTMLFilePath;
	bool bReady;
	TArray<FString> Pending;
};
