// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOmegaModernEditor.h"
#include "OmegaTellerManager.h"
#include "OmegaCharacter.h"
#include "OmegaNode.h"
#include "OmegaTree.h"
#include "OmegaAIInterface.h"
#include "OmegaLocalAI.h"
#include "WebBrowserModule.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "OmegaTeller"

// Helper function for JSON escaping
static FString EscapeForJson(const FString& InStr)
{
	FString Out = InStr;
	Out.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	Out.ReplaceInline(TEXT("\""), TEXT("\\\""));
	Out.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	Out.ReplaceInline(TEXT("\r"), TEXT(""));
	Out.ReplaceInline(TEXT("\t"), TEXT("\\t"));
	return Out;
}

// Construction

void SOmegaModernEditor::Construct(const FArguments& InArgs)
{
	UOmegaTellerManager* Mgr = UOmegaTellerManager::Get();
	CurrentTree.Reset(Mgr->CreateNewTree(TEXT("New Narrative Tree")));
	bReady = false;

	// Boot the bundled local LLM so it is warm by the time the user parses or merges
	FOmegaLocalAI::Get().EnsureServerRunning();

	HTMLFilePath = ResolveHTMLPath();

	const bool bHTMLExists = FPaths::FileExists(HTMLFilePath);
	UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] HTML path: %s (exists: %s)"),
		*HTMLFilePath, bHTMLExists ? TEXT("YES") : TEXT("NO"));

	// SWebBrowser silently produces an empty widget when the WebBrowser module (CEF) is
	// unavailable or the HTML is missing; surface the problem instead of a blank tab
	const bool bBrowserAvailable = IWebBrowserModule::IsAvailable()
		&& IWebBrowserModule::Get().IsWebModuleAvailable();

	if (!bBrowserAvailable || !bHTMLExists)
	{
		const FText ErrorText = !bBrowserAvailable
			? NSLOCTEXT("OmegaTeller", "NoWebBrowser",
				"OmegaTeller could not initialize the embedded web browser (CEF).\n"
				"The 'WebBrowser' module is not available in this editor session.")
			: FText::Format(NSLOCTEXT("OmegaTeller", "NoHTML",
				"OmegaTeller could not find its UI file:\n{0}"), FText::FromString(HTMLFilePath));

		UE_LOG(LogTemp, Error, TEXT("[OmegaTeller] %s"), *ErrorText.ToString());

		ChildSlot
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(ErrorText)
				.Justification(ETextJustify::Center)
				.AutoWrapText(true)
			]
		];
		return;
	}

	ChildSlot
	[
		SAssignNew(WebBrowser, SWebBrowser)
		.InitialURL(TEXT("file:///") + HTMLFilePath)
		.ShowControls(false)
		.ShowErrorMessage(true)
		.SupportsTransparency(false)
		.OnConsoleMessage(this, &SOmegaModernEditor::OnConsoleMessage)
		.OnBeforeNavigation(this, &SOmegaModernEditor::OnBeforeNavigation)
		.OnLoadCompleted(this, &SOmegaModernEditor::OnLoadComplete)
	];
}

SOmegaModernEditor::~SOmegaModernEditor()
{
	CurrentTree.Reset();
}

// Tree

void SOmegaModernEditor::SetTree(UOmegaTree* InTree)
{
	CurrentTree.Reset(InTree);
	if (bReady && CurrentTree.IsValid())
	{
		SyncTreeToBrowser();
	}
}

// HTML path

FString SOmegaModernEditor::ResolveHTMLPath() const
{
	const FString RelPath = TEXT("Content/Web/OmegaGraphEditor.html");

	// Try IPluginManager first, it gives us the actual on-disk plugin root
	TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("OmegaTellerPlugin"));
	if (Plugin.IsValid())
	{
		FString Path = FPaths::ConvertRelativePathToFull(Plugin->GetBaseDir() / RelPath);
		if (FPaths::FileExists(Path))
		{
			return Path;
		}
	}

	// Fallback: project plugins dir
	FString Fallback = FPaths::ConvertRelativePathToFull(
		FPaths::ProjectPluginsDir() / TEXT("OmegaTellerPlugin") / RelPath);
	if (FPaths::FileExists(Fallback))
	{
		return Fallback;
	}

	// Last resort: search relative to this module's DLL
	UE_LOG(LogTemp, Warning, TEXT("[OmegaTeller] Could not find HTML at: %s"), *Fallback);
	return Fallback;
}

// Browser callbacks

void SOmegaModernEditor::OnConsoleMessage(const FString& Message, const FString& Source, int32 Line, EWebBrowserConsoleLogSeverity Severity)
{
	if (Message.StartsWith(TEXT("OMEGA:")))
	{
		HandleBridgeMessage(Message.Mid(6));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[OmegaBrowser] %s"), *Message);
	}
}

bool SOmegaModernEditor::OnBeforeNavigation(const FString& URL, const FWebNavigationRequest& Request)
{
	// false = allow, true = cancel
	return !(URL.StartsWith(TEXT("file:///")) || URL.StartsWith(TEXT("data:")) || URL.StartsWith(TEXT("about:")));
}

void SOmegaModernEditor::OnLoadComplete()
{
	bReady = true;

	// Hand the AI configuration (local endpoint, optional cloud keys) to the page
	// before anything else runs, so callAI() prefers the bundled local model
	FString ConfigJSON = FOmegaLocalAI::Get().BuildJSConfigJSON();
	ConfigJSON.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	ConfigJSON.ReplaceInline(TEXT("'"), TEXT("\\'"));
	ExecJS(FString::Printf(TEXT("if (typeof applyAIConfig === 'function') applyAIConfig('%s');"), *ConfigJSON));

	FlushPending();
	if (CurrentTree.IsValid())
	{
		SyncTreeToBrowser();
	}
}

// JS bridge

void SOmegaModernEditor::ExecJS(const FString& Script)
{
	if (bReady && WebBrowser.IsValid())
	{
		WebBrowser->ExecuteJavascript(Script);
	}
	else
	{
		Pending.Add(Script);
	}
}

void SOmegaModernEditor::FlushPending()
{
	for (const FString& Cmd : Pending)
	{
		if (WebBrowser.IsValid())
		{
			WebBrowser->ExecuteJavascript(Cmd);
		}
	}
	Pending.Empty();
}

void SOmegaModernEditor::SyncTreeToBrowser()
{
	if (!CurrentTree.IsValid()) return;

	ExecJS(TEXT("clearGraph();"));

	for (UOmegaNode* Node : CurrentTree->AllNodes)
	{
		if (!Node) continue;
		FString Safe = Node->NodeName.ReplaceCharWithEscapedChar();
		Safe.ReplaceInline(TEXT("'"), TEXT("\\'"));
		ExecJS(FString::Printf(TEXT("addNode('%s','%s',%f,%f,'all',%d);"),
			*Node->NodeType, *Safe,
			Node->Position.X, Node->Position.Y,
			NodeIndex(Node)));
	}

	for (UOmegaNode* Node : CurrentTree->AllNodes)
	{
		if (!Node) continue;
		int32 From = NodeIndex(Node);
		for (UOmegaNode* Out : Node->OutputNodes)
		{
			if (!Out) continue;
			int32 To = NodeIndex(Out);
			if (From != INDEX_NONE && To != INDEX_NONE)
			{
				ExecJS(FString::Printf(TEXT("connectNodes(%d,%d);"), From, To));
			}
		}
	}
}

// Incoming bridge messages

void SOmegaModernEditor::HandleBridgeMessage(const FString& Payload)
{
	TArray<FString> P;

	if (Payload.StartsWith(TEXT("NODE_ADD:")))
	{
		Payload.Mid(9).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 4 && CurrentTree.IsValid())
		{
			UOmegaNode* N = CurrentTree->CreateNode(P[1], P[0]);
			if (N)
			{
				N->Position = FVector2D(FCString::Atof(*P[2]), FCString::Atof(*P[3]));
			}
		}
	}
	else if (Payload.StartsWith(TEXT("NODE_MOVE:")))
	{
		Payload.Mid(10).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 3)
		{
			UOmegaNode* N = NodeAt(FCString::Atoi(*P[0]));
			if (N) N->Position = FVector2D(FCString::Atof(*P[1]), FCString::Atof(*P[2]));
		}
	}
	else if (Payload.StartsWith(TEXT("NODE_DEL:")))
	{
		UOmegaNode* N = NodeAt(FCString::Atoi(*Payload.Mid(9)));
		if (N && CurrentTree.IsValid()) CurrentTree->RemoveNode(N);
	}
	else if (Payload.StartsWith(TEXT("NODE_TEXT:")))
	{
		// Split only on the first ':' so node names containing colons survive intact
		const FString Rest = Payload.Mid(10);
		int32 Sep = INDEX_NONE;
		if (Rest.FindChar(TEXT(':'), Sep) && Sep > 0)
		{
			UOmegaNode* N = NodeAt(FCString::Atoi(*Rest.Left(Sep)));
			if (N) N->NodeName = Rest.Mid(Sep + 1);
		}
	}
	else if (Payload.StartsWith(TEXT("CONN_ADD:")))
	{
		Payload.Mid(9).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 2 && CurrentTree.IsValid())
		{
			UOmegaNode* A = NodeAt(FCString::Atoi(*P[0]));
			UOmegaNode* B = NodeAt(FCString::Atoi(*P[1]));
			if (A && B) CurrentTree->ConnectNodes(A, B);
		}
	}
	else if (Payload.StartsWith(TEXT("GDD_PARSE_REQUEST:")))
	{
		// ── Delegate GDD parsing to the Rork Toolkit AI ──
		FString GDDText = Payload.Mid(18);
		HandleGDDParseRequest(GDDText);
	}
	else if (Payload.StartsWith(TEXT("GDD_PARSED:")))
	{
		UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] GDD parsed with %s characters"), *Payload.Mid(11));
	}
	else if (Payload.StartsWith(TEXT("DRAG_MERGE:")))
	{
		// Drag-merge: targetNodeIdx:targetCharName:sourceCharName:sourceNodeIdx
		Payload.Mid(11).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 4)
		{
			UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Drag-merge at node %s: '%s' + '%s' (from source node %s)"),
				*P[0], *P[1], *P[2], *P[3]);
		}
	}
	else if (Payload.StartsWith(TEXT("DRAG_CONNECT:")))
	{
		// Drag-connect (same character): targetNodeIdx:draggedNodeIdx
		Payload.Mid(13).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 2 && CurrentTree.IsValid())
		{
			UOmegaNode* A = NodeAt(FCString::Atoi(*P[0]));
			UOmegaNode* B = NodeAt(FCString::Atoi(*P[1]));
			if (A && B) CurrentTree->ConnectNodes(A, B);
		}
	}
	else if (Payload.StartsWith(TEXT("CHAR_INSERT:")))
	{
		// Cross-character insert: sourceNodeId:sourceCharName:newNodeName:targetCharName
		Payload.Mid(12).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 4)
		{
			UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Cross-character insert: node %s from '%s' -> new node %s in '%s'"),
				*P[0], *P[1], *P[2], *P[3]);
				
			// Process the cross-character insertion
			int32 SourceNodeId = FCString::Atoi(*P[0]);
			FString SourceChar = P[1];
			FString NewNodeName = P[2];
			FString TargetChar = P[3];
			
			UOmegaNode* SourceNode = NodeAt(SourceNodeId);
			if (SourceNode && CurrentTree.IsValid())
			{
				// Create a new node in the target character with the source node's content
				UOmegaNode* NewNode = CurrentTree->CreateNode(NewNodeName, TEXT("Event"));
				if (NewNode)
				{
					NewNode->NodeName = SourceNode->NodeName;
					NewNode->Position = SourceNode->Position + FVector2D(50.0f, 50.0f); // Offset slightly
					
					UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Created cross-character node '%s' for character '%s'"), 
						*NewNodeName, *TargetChar);
				}
			}
		}
	}
	else if (Payload.StartsWith(TEXT("TIMELINE_MERGE:")))
	{
		// Timeline merge: anchorNid:targetChar:sourceChar:sourceNodeIdx[:lang]
		Payload.Mid(15).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 4)
		{
			UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Timeline merge at node %s: '%s' + '%s' (from source node %s)"),
				*P[0], *P[1], *P[2], *P[3]);

			// Process the merge
			int32 AnchorNid = FCString::Atoi(*P[0]);
			FString TargetChar = P[1];
			FString SourceChar = P[2];
			int32 SourceNid = FCString::Atoi(*P[3]);
			// Idioma del texto importado, enviado por la UI. Sin él no se puede
			// garantizar que el texto generado conserve el idioma original.
			FString Lang = (P.Num() >= 5) ? P[4] : FString();

			UOmegaNode* AnchorNode = NodeAt(AnchorNid);
			UOmegaNode* SourceNode = NodeAt(SourceNid);

			if (AnchorNode && SourceNode && CurrentTree.IsValid())
			{
				// Create story merge request
				FString AnchorText = AnchorNode->NodeName;
				FString SourceText = SourceNode->NodeName;

				// Send merge request to AI
				FString MergeJson = TEXT("{");
				MergeJson += FString::Printf(TEXT("\"charA\":\"%s\",\"charB\":\"%s\",\"textA\":\"%s\",\"textB\":\"%s\",\"lang\":\"%s\",\"anchorNodeId\":%d}"),
					*EscapeForJson(TargetChar),
					*EscapeForJson(SourceChar),
					*EscapeForJson(AnchorText),
					*EscapeForJson(SourceText),
					*EscapeForJson(Lang),
					AnchorNid);

				UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Sending merge request JSON: %s"), *MergeJson);
				HandleStoryMergeRequest(MergeJson);
				
				// Don't remove source node immediately - let the merge complete first
				// The source node will be removed after the merge response is processed
				// This prevents issues with node references during merge processing
			}
		}
	}
	else if (Payload.StartsWith(TEXT("TIMELINE_SPLIT:")))
	{
		// Timeline split: nodeId:ownerChar:charToSplit:newBranchId
		Payload.Mid(15).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 4)
		{
			UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Timeline split at node %s: '%s' splits off '%s' (new branch at %s)"),
				*P[0], *P[1], *P[2], *P[3]);
				
			// Process the split
			int32 NodeId = FCString::Atoi(*P[0]);
			FString OwnerChar = P[1];
			FString CharToSplit = P[2];
			int32 NewBranchId = FCString::Atoi(*P[3]);
			
			UOmegaNode* SplitNode = NodeAt(NodeId);
			if (SplitNode && CurrentTree.IsValid())
			{
				// In a full implementation, we would:
				// 1. Create a new character tree branch
				// 2. Move appropriate nodes to the new branch
				// 3. Update visual representation
				// For now, just log the split request
				UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Processed timeline split for character '%s' from node %d"), 
					*CharToSplit, NodeId);
			}
		}
	}
	else if (Payload.StartsWith(TEXT("MERGE_COMPLETE:")))
	{
		// Merge complete: anchorNodeId:sourceNodeId
		Payload.Mid(15).ParseIntoArray(P, TEXT(":"));
		if (P.Num() >= 2)
		{
			int32 AnchorNid = FCString::Atoi(*P[0]);
			int32 SourceNid = FCString::Atoi(*P[1]);
			
			UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Merge complete: anchor %d, source %d"), AnchorNid, SourceNid);
			
			// Now remove the source node after merge is complete
			UOmegaNode* SourceNode = NodeAt(SourceNid);
			if (SourceNode && CurrentTree.IsValid())
			{
				CurrentTree->RemoveNode(SourceNode);
				UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Removed source node %d after merge"), SourceNid);
			}
		}
	}
	else if (Payload.StartsWith(TEXT("STORY_MERGE_REQUEST:")))
	{
		// Story merge request: JSON with charA, charB, textA, textB
		FString JsonPayload = Payload.Mid(20);
		HandleStoryMergeRequest(JsonPayload);
	}
	else if (Payload.StartsWith(TEXT("REPOPULATE_REQUEST:")))
	{
		// Repopulate request: JSON with startNodeId, startText, character,
		// mergedChars, upstreamContext, downstreamIds, downstreamTexts
		FString JsonPayload = Payload.Mid(19);
		HandleRepopulateRequest(JsonPayload);
	}
}

// GDD parsing

namespace OmegaJsonUtil
{
	// This namespace is kept for compatibility but the main function is now global
}

void SOmegaModernEditor::HandleGDDParseRequest(const FString& GDDText)
{
	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	if (!Manager)
	{
		UE_LOG(LogTemp, Error, TEXT("[OmegaTeller] Manager not available for GDD parsing"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Parsing GDD via Rork Toolkit (length: %d)"), GDDText.Len());

	// Use the Rork Toolkit adapter to extract characters
	TArray<UOmegaCharacter*> Characters = Manager->ExtractCharactersWithAI(GDDText);

	if (Characters.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[OmegaTeller] Rork Toolkit found no characters"));
		return;
	}

	// Extract all story beats ONCE (not per-character)
	TScriptInterface<IOmegaAIInterface> AI = Manager->GetAIInterface();
	UObject* AIObject = AI.GetObject();

	TArray<FString> AllBeats;
	if (AIObject && IOmegaAIInterface::Execute_IsAvailable(AIObject))
	{
		AllBeats = IOmegaAIInterface::Execute_ExtractStoryBeatsFromGDD(AIObject, GDDText);
	}

	// Build a JSON response for the browser
	FString JSON = TEXT("{\"characters\":[");

	for (int32 i = 0; i < Characters.Num(); ++i)
	{
		UOmegaCharacter* Character = Characters[i];
		if (!Character) continue;

		// Determine beats for this character:
		// First, try to find beats that mention this character's name
		TArray<FString> RelevantBeats;

		FString Role = Character->GetAttribute(TEXT("Role"));
		bool bIsProtagonist = Role.Equals(TEXT("Protagonist"), ESearchCase::IgnoreCase)
			|| Role.Equals(TEXT("Hero"), ESearchCase::IgnoreCase);

		for (const FString& Beat : AllBeats)
		{
			if (Beat.Contains(Character->CharacterName))
			{
				RelevantBeats.Add(Beat);
			}
		}

		// Protagonists also get unassigned beats (beats that don't mention any character)
		if (bIsProtagonist)
		{
			for (const FString& Beat : AllBeats)
			{
				if (RelevantBeats.Contains(Beat))
				{
					continue;
				}
				// Check if this beat mentions ANY character
				bool bMentionsAny = false;
				for (UOmegaCharacter* OtherChar : Characters)
				{
					if (OtherChar && Beat.Contains(OtherChar->CharacterName))
					{
						bMentionsAny = true;
						break;
					}
				}
				if (!bMentionsAny)
				{
					RelevantBeats.Add(Beat);
				}
			}
		}

		// If still no beats and only one character, give them all beats
		if (RelevantBeats.Num() == 0 && Characters.Num() == 1)
		{
			RelevantBeats = AllBeats;
		}

		// Build JSON for this character
		if (i > 0) JSON += TEXT(",");

		FString SafeName = EscapeForJson(Character->CharacterName);
		JSON += FString::Printf(TEXT("{\"name\":\"%s\",\"beats\":["), *SafeName);

		for (int32 j = 0; j < RelevantBeats.Num(); ++j)
		{
			if (j > 0) JSON += TEXT(",");
			FString SafeBeat = EscapeForJson(RelevantBeats[j]);
			JSON += FString::Printf(TEXT("\"%s\""), *SafeBeat);
		}

		JSON += TEXT("]}");

		// Also generate a tree in the backend for this character
		Manager->GenerateTreeWithAI(Character, GDDText);
	}

	JSON += TEXT("]}");

	UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Sending Rork result to browser: %d characters"), Characters.Num());

	// Send result back to browser
	// The JSON is already properly escaped by EscapeJsonString, so we just
	// need to escape single quotes for JS string wrapper
	FString SafeJSON = JSON;
	SafeJSON.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	SafeJSON.ReplaceInline(TEXT("'"), TEXT("\\'"));
	SafeJSON.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	SafeJSON.ReplaceInline(TEXT("\r"), TEXT(""));
	SafeJSON.ReplaceInline(TEXT("\t"), TEXT("\\t"));
	ExecJS(FString::Printf(TEXT("handleRorkResult('%s');"), *SafeJSON));
}

// Story merge: generates combined narrative for merged nodes

void SOmegaModernEditor::HandleStoryMergeRequest(const FString& JsonPayload)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OmegaTeller] Failed to parse story merge request JSON"));
		return;
	}

	FString CharA, CharB, TextA, TextB, Lang;
	JsonObject->TryGetStringField(TEXT("charA"), CharA);
	JsonObject->TryGetStringField(TEXT("charB"), CharB);
	JsonObject->TryGetStringField(TEXT("textA"), TextA);
	JsonObject->TryGetStringField(TEXT("textB"), TextB);
	JsonObject->TryGetStringField(TEXT("lang"), Lang);

	int32 AnchorNodeId = 0;
	if (!JsonObject->TryGetNumberField(TEXT("anchorNodeId"), AnchorNodeId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OmegaTeller] Story merge request missing 'anchorNodeId'"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Story merge (lang=%s): '%s' (%s) + '%s' (%s)"),
		Lang.IsEmpty() ? TEXT("?") : *Lang, *TextA, *CharA, *TextB, *CharB);

	// Nexos por idioma. El texto fusionado conserva siempre el idioma del texto
	// que el usuario importo, nunca se traduce ni se mezclan idiomas.
	// Se componen por fragmentos porque FString::Printf exige un literal como
	// formato y no puede recibir la plantilla desde una variable.
	struct FMergeWords
	{
		const TCHAR* Meanwhile;    // nexo entre los dos textos
		const TCHAR* CrossJoin;    // "A" + esto + "B"
		const TCHAR* CrossTail;    // ... + esto
		const TCHAR* JoinsTail;    // "A" + esto
	};
	auto WordsFor = [](const FString& Code) -> FMergeWords
	{
		if (Code == TEXT("en")) return { TEXT("Meanwhile,"),        TEXT(" and "), TEXT(" cross paths"),        TEXT(" joins") };
		if (Code == TEXT("pt")) return { TEXT("Enquanto isso,"),    TEXT(" e "),   TEXT(" se cruzam"),          TEXT(" se junta") };
		if (Code == TEXT("fr")) return { TEXT("Pendant ce temps,"), TEXT(" et "),  TEXT(" se croisent"),        TEXT(" rejoint") };
		if (Code == TEXT("it")) return { TEXT("Nel frattempo,"),    TEXT(" e "),   TEXT(" si incrociano"),      TEXT(" si unisce") };
		if (Code == TEXT("de")) return { TEXT("Unterdessen,"),      TEXT(" und "), TEXT(" treffen aufeinander"),TEXT(" kommt dazu") };
		return { TEXT("Mientras tanto,"), TEXT(" y "), TEXT(" se cruzan"), TEXT(" se une") };   // es por defecto
	};
	const FMergeWords Words = WordsFor(Lang.ToLower());

	FString MergedText;
	if (TextA.IsEmpty() && TextB.IsEmpty())
	{
		MergedText = CharA + Words.CrossJoin + CharB + Words.CrossTail;
	}
	else if (TextA.IsEmpty())
	{
		MergedText = CharA + Words.JoinsTail + TEXT(" \u2014 ") + TextB;
	}
	else if (TextB.IsEmpty())
	{
		MergedText = TextA + TEXT(" \u2014 ") + CharB + Words.JoinsTail;
	}
	else
	{
		// Une los dos textos literales con el nexo del idioma correspondiente
		FString Head = TextA.TrimEnd();
		if (!Head.IsEmpty())
		{
			const TCHAR Last = Head[Head.Len() - 1];
			if (Last != TEXT('.') && Last != TEXT('!') && Last != TEXT('?') && Last != TEXT('\u2026'))
			{
				Head.AppendChar(TEXT('.'));
			}
		}
		MergedText = FString::Printf(TEXT("%s %s %s"), *Head, Words.Meanwhile, *TextB);
	}

	UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Merged story: '%s'"), *MergedText);

	// Build JSON response
	FString JSON = TEXT("{");
	JSON += FString::Printf(TEXT("\"anchorNodeId\":%d,"), AnchorNodeId);
	JSON += FString::Printf(TEXT("\"mergedText\":\"%s\""), *EscapeForJson(MergedText));
	JSON += TEXT("}");

	// Properly escape JSON for JavaScript string literal
	FString SafeJSON = JSON;
	SafeJSON.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	SafeJSON.ReplaceInline(TEXT("'"), TEXT("\\'"));
	SafeJSON.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	SafeJSON.ReplaceInline(TEXT("\r"), TEXT(""));
	SafeJSON.ReplaceInline(TEXT("\t"), TEXT("\\t"));
	ExecJS(FString::Printf(TEXT("handleMergeResult('%s');"), *SafeJSON));
}

// Repopulate: regenerates downstream story beats from a node

void SOmegaModernEditor::HandleRepopulateRequest(const FString& JsonPayload)
{
	TSharedPtr<FJsonObject> JsonObject;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonPayload);
	if (!FJsonSerializer::Deserialize(Reader, JsonObject) || !JsonObject.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[OmegaTeller] Failed to parse repopulate request JSON"));
		return;
	}

	int32 StartNodeId = 0;
	if (!JsonObject->TryGetNumberField(TEXT("startNodeId"), StartNodeId))
	{
		UE_LOG(LogTemp, Warning, TEXT("[OmegaTeller] Repopulate request missing 'startNodeId'"));
		return;
	}
	FString StartText;
	JsonObject->TryGetStringField(TEXT("startText"), StartText);
	FString Character;
	JsonObject->TryGetStringField(TEXT("character"), Character);
	FString UpstreamContext;
	JsonObject->TryGetStringField(TEXT("upstreamContext"), UpstreamContext);
	FString Lang;
	JsonObject->TryGetStringField(TEXT("lang"), Lang);

	// Get merged characters if any
	TArray<FString> MergedChars;
	const TArray<TSharedPtr<FJsonValue>>* MergedCharsArray;
	if (JsonObject->TryGetArrayField(TEXT("mergedChars"), MergedCharsArray) && MergedCharsArray)
	{
		for (const auto& Val : *MergedCharsArray)
		{
			if (Val.IsValid() && Val->Type == EJson::String)
			{
				MergedChars.Add(Val->AsString());
			}
		}
	}

	// Get downstream IDs and current texts
	TArray<int32> DownstreamIds;
	TArray<FString> DownstreamTexts;
	const TArray<TSharedPtr<FJsonValue>>* IdsArray;
	const TArray<TSharedPtr<FJsonValue>>* TextsArray;

	if (JsonObject->TryGetArrayField(TEXT("downstreamIds"), IdsArray) && IdsArray)
	{
		for (const auto& Val : *IdsArray)
		{
			double Num = 0.0;
			if (Val.IsValid() && Val->TryGetNumber(Num))
			{
				DownstreamIds.Add(static_cast<int32>(Num));
			}
		}
	}
	if (JsonObject->TryGetArrayField(TEXT("downstreamTexts"), TextsArray) && TextsArray)
	{
		for (const auto& Val : *TextsArray)
		{
			FString Text;
			if (Val.IsValid() && Val->TryGetString(Text))
			{
				DownstreamTexts.Add(Text);
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Repopulate from node %d ('%s'), %d downstream nodes, character: '%s'"),
		StartNodeId, *StartText, DownstreamIds.Num(), *Character);

	// Build character label
	FString CharLabel = Character;
	if (MergedChars.Num() > 1)
	{
		CharLabel = FString::Join(MergedChars, TEXT(" & "));
	}

	// Try AI-powered repopulation via Rork Toolkit
	TArray<FString> NewBeats;
	bool bUsedAI = false;

	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	TScriptInterface<IOmegaAIInterface> AI = Manager ? Manager->GetAIInterface() : TScriptInterface<IOmegaAIInterface>();
	UObject* AIObject = AI.GetObject();

	if (AIObject && IOmegaAIInterface::Execute_IsAvailable(AIObject) && DownstreamIds.Num() > 0)
	{
		// Build a prompt that gives the AI full context
		FString RepopPrompt = FString::Printf(
			TEXT("Continue this story for character(s) %s. "
			     "Use the following context for coherence, but do not include it in your output: %s. "
			     "Current node: \"%s\". "
			     "Generate exactly %d story beats that continue coherently from this point. "
			     "Each beat should be a single concise sentence."),
			*CharLabel,
			UpstreamContext.IsEmpty() ? TEXT("(start of story)") : *UpstreamContext,
			*StartText,
			DownstreamIds.Num());

		TArray<FString> AIBeats = IOmegaAIInterface::Execute_ExtractStoryBeatsFromGDD(AIObject, RepopPrompt);
		if (AIBeats.Num() > 0)
		{
			bUsedAI = true;
			// Use AI beats, padding or trimming to match downstream count
			for (int32 i = 0; i < DownstreamIds.Num(); ++i)
			{
				if (i < AIBeats.Num())
				{
					NewBeats.Add(AIBeats[i]);
				}
				else
				{
					// Repeat last beat pattern if AI gave fewer than needed
					NewBeats.Add(AIBeats[AIBeats.Num() - 1]);
				}
			}
			UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] AI repopulated %d beats (requested %d)"),
				AIBeats.Num(), DownstreamIds.Num());
		}
	}

	// Fallback if AI not available
	if (!bUsedAI)
	{
		bool bIsMerged = MergedChars.Num() > 1;

		// Frases de relleno en el idioma del texto importado (nunca se traduce
		// ni se mezcla con otro idioma).
		auto PhrasesFor = [](const FString& Code) -> TArray<FString>
		{
			if (Code == TEXT("en"))
			{
				return { TEXT("moves forward"), TEXT("reacts to what happened"),
				         TEXT("sees the consequences"), TEXT("makes a decision"),
				         TEXT("finds something unexpected"), TEXT("faces an obstacle"),
				         TEXT("prepares for what is coming"), TEXT("reaches a turning point"),
				         TEXT("uncovers the truth"), TEXT("changes course") };
			}
			if (Code == TEXT("pt"))
			{
				return { TEXT("avança"), TEXT("reage ao ocorrido"),
				         TEXT("observa as consequências"), TEXT("toma uma decisão"),
				         TEXT("encontra algo inesperado"), TEXT("enfrenta um obstáculo"),
				         TEXT("prepara-se para o que vem"), TEXT("chega a um ponto de virada"),
				         TEXT("descobre a verdade"), TEXT("muda de rumo") };
			}
			if (Code == TEXT("fr"))
			{
				return { TEXT("avance"), TEXT("réagit à ce qui est arrivé"),
				         TEXT("observe les conséquences"), TEXT("prend une décision"),
				         TEXT("trouve quelque chose d'inattendu"), TEXT("affronte un obstacle"),
				         TEXT("se prépare à la suite"), TEXT("atteint un tournant"),
				         TEXT("découvre la vérité"), TEXT("change de cap") };
			}
			if (Code == TEXT("it"))
			{
				return { TEXT("avanza"), TEXT("reagisce a quanto accaduto"),
				         TEXT("osserva le conseguenze"), TEXT("prende una decisione"),
				         TEXT("trova qualcosa di inatteso"), TEXT("affronta un ostacolo"),
				         TEXT("si prepara a ciò che verrà"), TEXT("raggiunge un punto di svolta"),
				         TEXT("scopre la verità"), TEXT("cambia rotta") };
			}
			if (Code == TEXT("de"))
			{
				return { TEXT("geht weiter"), TEXT("reagiert auf das Geschehene"),
				         TEXT("sieht die Folgen"), TEXT("trifft eine Entscheidung"),
				         TEXT("findet etwas Unerwartetes"), TEXT("stellt sich einem Hindernis"),
				         TEXT("bereitet sich auf das Kommende vor"), TEXT("erreicht einen Wendepunkt"),
				         TEXT("entdeckt die Wahrheit"), TEXT("ändert den Kurs") };
			}
			return { TEXT("avanza"), TEXT("reacciona ante lo ocurrido"),
			         TEXT("observa las consecuencias"), TEXT("toma una decisión"),
			         TEXT("encuentra algo inesperado"), TEXT("se enfrenta a un obstáculo"),
			         TEXT("se prepara para lo que viene"), TEXT("llega a un punto de inflexión"),
			         TEXT("descubre la verdad"), TEXT("cambia de rumbo") };   // es por defecto
		};
		const TArray<FString> ProgressionPhrases = PhrasesFor(Lang.ToLower());

		for (int32 i = 0; i < DownstreamIds.Num(); ++i)
		{
			FString OldText = (i < DownstreamTexts.Num()) ? DownstreamTexts[i] : TEXT("");
			FString NewText;

			if (OldText.IsEmpty() || OldText.Len() < 5 || OldText.StartsWith(TEXT("[")))
			{
				FString Phrase = ProgressionPhrases[i % ProgressionPhrases.Num()];
				NewText = FString::Printf(TEXT("%s %s"), *CharLabel, *Phrase);
			}
			else if (bIsMerged)
			{
				bool bAllMentioned = true;
				for (const FString& Char : MergedChars)
				{
					if (!OldText.Contains(Char))
					{
						bAllMentioned = false;
						break;
					}
				}

				if (!bAllMentioned)
				{
					NewText = FString::Printf(TEXT("%s: %s"), *CharLabel, *OldText);
				}
				else
				{
					NewText = OldText;
				}
			}
			else
			{
				NewText = OldText;
			}

			NewBeats.Add(NewText);
		}
	}

	// Build JSON response
	FString JSON = TEXT("{");
	JSON += FString::Printf(TEXT("\"startNodeId\":%d,\"beats\":["), StartNodeId);
	for (int32 i = 0; i < NewBeats.Num(); ++i)
	{
		if (i > 0) JSON += TEXT(",");
		FString SafeBeat = EscapeForJson(NewBeats[i]);
		JSON += FString::Printf(TEXT("\"%s\""), *SafeBeat);
	}
	JSON += TEXT("]}");

	UE_LOG(LogTemp, Log, TEXT("[OmegaTeller] Sending repopulate result: %d beats (AI: %s)"),
		NewBeats.Num(), bUsedAI ? TEXT("YES") : TEXT("NO"));

	// Properly escape JSON for JavaScript string literal
	FString SafeJSON = JSON;
	SafeJSON.ReplaceInline(TEXT("\\"), TEXT("\\\\"));
	SafeJSON.ReplaceInline(TEXT("'"), TEXT("\\'"));
	SafeJSON.ReplaceInline(TEXT("\n"), TEXT("\\n"));
	SafeJSON.ReplaceInline(TEXT("\r"), TEXT(""));
	SafeJSON.ReplaceInline(TEXT("\t"), TEXT("\\t"));
	ExecJS(FString::Printf(TEXT("handleRepopulateResult('%s');"), *SafeJSON));
}

// Helpers

int32 SOmegaModernEditor::NodeIndex(UOmegaNode* N) const
{
	if (!CurrentTree.IsValid() || !N) return INDEX_NONE;
	return CurrentTree->AllNodes.IndexOfByKey(N);
}

UOmegaNode* SOmegaModernEditor::NodeAt(int32 Idx) const
{
	if (!CurrentTree.IsValid() || !CurrentTree->AllNodes.IsValidIndex(Idx)) return nullptr;
	return CurrentTree->AllNodes[Idx];
}

#undef LOCTEXT_NAMESPACE
