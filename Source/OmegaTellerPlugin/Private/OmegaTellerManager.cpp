// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaTellerManager.h"
#include "OmegaTree.h"
#include "OmegaCharacter.h"
#include "OmegaAIInterface.h"
#include "RorkToolkitAdapter.h"
#include "Misc/Guid.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UOmegaTellerManager* UOmegaTellerManager::Instance = nullptr;

UOmegaTellerManager::UOmegaTellerManager()
{
	// Do not call InitializeAISystem here - UObject is not fully constructed yet
}

UOmegaTellerManager* UOmegaTellerManager::Get()
{
	if (!Instance)
	{
		Instance = NewObject<UOmegaTellerManager>();
		Instance->AddToRoot(); // Prevent garbage collection
		Instance->InitializeAISystem();
	}
	return Instance;
}

void UOmegaTellerManager::BeginDestroy()
{
	// Clear the static pointer so a late Get() (e.g. during shutdown) never returns a dying object
	if (Instance == this)
	{
		Instance = nullptr;
	}
	Super::BeginDestroy();
}

void UOmegaTellerManager::InitializeAISystem()
{
	// Create Rork Toolkit adapter (outer = manager, so it shares the rooted lifetime)
	RorkToolkitAdapter = NewObject<URorkToolkitAdapter>(this);
	
	// Configure based on settings
	if (AIType == TEXT("RorkToolkit"))
	{
		AIInterface = TScriptInterface<IOmegaAIInterface>(RorkToolkitAdapter);
		
		// Configure adapter
		if (!APIKey.IsEmpty())
		{
			RorkToolkitAdapter->SetAPIKey(APIKey);
		}
		
		// Initialize the adapter using Execute_ form for BlueprintNativeEvent
		IOmegaAIInterface::Execute_Initialize(RorkToolkitAdapter, TEXT(""));
	}
	else
	{
		// For other AI types, we'll use the Rork Toolkit adapter as a template fallback
		AIInterface = TScriptInterface<IOmegaAIInterface>(RorkToolkitAdapter);
		IOmegaAIInterface::Execute_Initialize(RorkToolkitAdapter, TEXT(""));
	}
}

TScriptInterface<IOmegaAIInterface> UOmegaTellerManager::GetAIInterface()
{
	if (!AIInterface)
	{
		InitializeAISystem();
	}
	return AIInterface;
}

UOmegaTree* UOmegaTellerManager::CreateNewTree(const FString& TreeName)
{
	UOmegaTree* NewTree = NewObject<UOmegaTree>(this);
	NewTree->TreeID = FGuid::NewGuid().ToString();
	NewTree->TreeName = TreeName.IsEmpty() ? TEXT("New Tree") : TreeName;
	NewTree->Description = FText::FromString(TEXT("New narrative tree"));

	ManagedTrees.Add(NewTree);
	return NewTree;
}

TArray<UOmegaTree*> UOmegaTellerManager::GetAllTrees() const
{
	TArray<UOmegaTree*> Result;
	Result.Reserve(ManagedTrees.Num());
	for (UOmegaTree* Tree : ManagedTrees)
	{
		if (Tree)
		{
			Result.Add(Tree);
		}
	}
	return Result;
}

UOmegaTree* UOmegaTellerManager::FindTreeByID(const FString& TreeID) const
{
	for (UOmegaTree* Tree : ManagedTrees)
	{
		if (Tree && Tree->TreeID == TreeID)
		{
			return Tree;
		}
	}
	return nullptr;
}

UOmegaCharacter* UOmegaTellerManager::CreateCharacter(const FString& CharacterName)
{
	UOmegaCharacter* NewCharacter = NewObject<UOmegaCharacter>(this);
	NewCharacter->CharacterID = FGuid::NewGuid().ToString();
	NewCharacter->CharacterName = CharacterName.IsEmpty() ? TEXT("New Character") : CharacterName;
	NewCharacter->Description = FText::FromString(TEXT("New character"));

	ManagedCharacters.Add(NewCharacter);
	return NewCharacter;
}

TArray<UOmegaCharacter*> UOmegaTellerManager::GetAllCharacters() const
{
	TArray<UOmegaCharacter*> Result;
	Result.Reserve(ManagedCharacters.Num());
	for (UOmegaCharacter* Character : ManagedCharacters)
	{
		if (Character)
		{
			Result.Add(Character);
		}
	}
	return Result;
}

UOmegaCharacter* UOmegaTellerManager::FindCharacterByID(const FString& CharacterID) const
{
	for (UOmegaCharacter* Character : ManagedCharacters)
	{
		if (Character && Character->CharacterID == CharacterID)
		{
			return Character;
		}
	}
	return nullptr;
}

void UOmegaTellerManager::ParseGDD(const FString& GDDText)
{
	UE_LOG(LogTemp, Log, TEXT("Parsing GDD text (length: %d)"), GDDText.Len());
	
	// Extract characters using AI
	TArray<UOmegaCharacter*> Characters = ExtractCharactersWithAI(GDDText);
	
	// Generate trees for each character
	for (UOmegaCharacter* Character : Characters)
	{
		if (Character)
		{
			GenerateTreeWithAI(Character, GDDText);
		}
	}
	
	UE_LOG(LogTemp, Log, TEXT("GDD parsing complete. Created %d characters."), Characters.Num());
}

FString UOmegaTellerManager::GenerateNodeDescription(const FString& Context)
{
	TScriptInterface<IOmegaAIInterface> AI = GetAIInterface();
	UObject* AIObject = AI.GetObject();
	if (AIObject && IOmegaAIInterface::Execute_IsAvailable(AIObject))
	{
		return IOmegaAIInterface::Execute_GenerateNodeDescription(AIObject, Context, TEXT("Event"));
	}
	
	// Fallback
	return TEXT("Node: ") + Context;
}

TArray<UOmegaCharacter*> UOmegaTellerManager::ExtractCharactersWithAI(const FString& GDDText)
{
	TArray<UOmegaCharacter*> Characters;
	
	TScriptInterface<IOmegaAIInterface> AI = GetAIInterface();
	UObject* AIObject = AI.GetObject();
	if (AIObject && IOmegaAIInterface::Execute_IsAvailable(AIObject))
	{
		Characters = IOmegaAIInterface::Execute_ExtractCharactersFromGDD(AIObject, GDDText);
	}
	else
	{
		// Fallback: create example characters
		UOmegaCharacter* Protagonist = CreateCharacter(TEXT("Protagonist"));
		Protagonist->UpdateAttribute(TEXT("Role"), TEXT("Protagonist"));
		Protagonist->UpdateAttribute(TEXT("Personality"), TEXT("Brave, Determined"));
		Protagonist->UpdateAttribute(TEXT("Goals"), TEXT("Save the kingdom"));
		
		UOmegaCharacter* Antagonist = CreateCharacter(TEXT("Antagonist"));
		Antagonist->UpdateAttribute(TEXT("Role"), TEXT("Antagonist"));
		Antagonist->UpdateAttribute(TEXT("Personality"), TEXT("Ambitious, Cunning"));
		Antagonist->UpdateAttribute(TEXT("Goals"), TEXT("Seize power"));
		
		Characters.Add(Protagonist);
		Characters.Add(Antagonist);
	}
	
	return Characters;
}

UOmegaTree* UOmegaTellerManager::GenerateTreeWithAI(UOmegaCharacter* Character, const FString& GDDContext)
{
	if (!Character)
	{
		return nullptr;
	}
	
	TScriptInterface<IOmegaAIInterface> AI = GetAIInterface();
	UObject* AIObject = AI.GetObject();
	if (AIObject && IOmegaAIInterface::Execute_IsAvailable(AIObject))
	{
		return IOmegaAIInterface::Execute_GenerateTreeForCharacter(AIObject, Character, GDDContext);
	}
	
	// Fallback: create simple tree
	return Character->CreateNarrativeTree(Character->CharacterName + TEXT("'s Story"));
}

void UOmegaTellerManager::SetAIType(const FString& NewAIType)
{
	AIType = NewAIType;
	InitializeAISystem();
}

FString UOmegaTellerManager::GetCurrentAIType() const
{
	return AIType;
}

bool UOmegaTellerManager::IsAIAvailable() const
{
	UObject* AIObject = AIInterface.GetObject();
	return AIObject && IOmegaAIInterface::Execute_IsAvailable(AIObject);
}

bool UOmegaTellerManager::SaveTreeToFile(UOmegaTree* Tree, const FString& FilePath)
{
	if (!Tree || FilePath.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	FString JSON = Tree->ExportToJSON();
	if (!FFileHelper::SaveStringToFile(JSON, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveTreeToFile: could not write '%s'"), *FilePath);
		return false;
	}
	return true;
}

UOmegaTree* UOmegaTellerManager::LoadTreeFromFile(const FString& FilePath)
{
	FString JSON;
	if (FilePath.TrimStartAndEnd().IsEmpty() || !FFileHelper::LoadFileToString(JSON, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadTreeFromFile: could not read '%s'"), *FilePath);
		return nullptr;
	}

	UOmegaTree* NewTree = CreateNewTree(TEXT("Loaded Tree"));
	if (NewTree->ImportFromJSON(JSON))
	{
		return NewTree;
	}

	// Import failed: don't leave a half-built tree registered
	ManagedTrees.Remove(NewTree);
	return nullptr;
}

bool UOmegaTellerManager::SaveCharacterToFile(UOmegaCharacter* Character, const FString& FilePath)
{
	if (!Character || FilePath.TrimStartAndEnd().IsEmpty())
	{
		return false;
	}

	FString JSON = Character->ExportToJSON();
	if (!FFileHelper::SaveStringToFile(JSON, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("SaveCharacterToFile: could not write '%s'"), *FilePath);
		return false;
	}
	return true;
}

UOmegaCharacter* UOmegaTellerManager::LoadCharacterFromFile(const FString& FilePath)
{
	FString JSON;
	if (FilePath.TrimStartAndEnd().IsEmpty() || !FFileHelper::LoadFileToString(JSON, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("LoadCharacterFromFile: could not read '%s'"), *FilePath);
		return nullptr;
	}

	UOmegaCharacter* NewCharacter = CreateCharacter(TEXT("Loaded Character"));
	if (NewCharacter->ImportFromJSON(JSON))
	{
		return NewCharacter;
	}

	// Import failed: don't leave a half-built character registered
	ManagedCharacters.Remove(NewCharacter);
	return nullptr;
}

FString UOmegaTellerManager::GetPluginVersion() const
{
	return TEXT("1.0.0");
}

FString UOmegaTellerManager::GetSystemStatus() const
{
	FString Status;
	
	Status += FString::Printf(TEXT("OmegaTeller Plugin v%s\n"), *GetPluginVersion());
	Status += FString::Printf(TEXT("AI Type: %s\n"), *AIType);
	Status += FString::Printf(TEXT("AI Available: %s\n"), IsAIAvailable() ? TEXT("Yes") : TEXT("No"));
	Status += FString::Printf(TEXT("Managed Trees: %d\n"), ManagedTrees.Num());
	Status += FString::Printf(TEXT("Managed Characters: %d\n"), ManagedCharacters.Num());
	
	if (RorkToolkitAdapter)
	{
		Status += FString::Printf(TEXT("AI Confidence: %.2f\n"), IOmegaAIInterface::Execute_GetConfidenceScore(RorkToolkitAdapter));
	}
	
	return Status;
}

void UOmegaTellerManager::ClearAllData()
{
	ManagedTrees.Empty();
	ManagedCharacters.Empty();
	
	UE_LOG(LogTemp, Log, TEXT("Cleared all OmegaTeller data"));
}