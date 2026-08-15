// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaCharacter.h"
#include "Misc/Guid.h"
#include "OmegaTellerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UOmegaCharacter::UOmegaCharacter()
{
	InitializeDefaults();
}

void UOmegaCharacter::InitializeDefaults()
{
	CharacterID = FGuid::NewGuid().ToString();
	CharacterName = TEXT("New Character");
	Description = FText::FromString(TEXT("Character description"));
	NarrativeTree = nullptr;
	
	// Set default attributes
	Attributes.Add(TEXT("Role"), TEXT("Unknown"));
	Attributes.Add(TEXT("Personality"), TEXT("Neutral"));
	Attributes.Add(TEXT("Goals"), TEXT("Undefined"));
	Attributes.Add(TEXT("Arc"), TEXT("Standard"));
	
	// Add default tags
	Tags.Add(TEXT("Character"));
	Tags.Add(TEXT("Protagonist"));
}

UOmegaTree* UOmegaCharacter::CreateNarrativeTree(const FString& TreeName)
{
	if (NarrativeTree)
	{
		// Already has a tree
		return NarrativeTree;
	}
	
	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	if (!Manager)
	{
		return nullptr;
	}
	
	NarrativeTree = Manager->CreateNewTree(TreeName);
	return NarrativeTree;
}

bool UOmegaCharacter::HasNarrativeTree() const
{
	return NarrativeTree != nullptr;
}

void UOmegaCharacter::UpdateAttribute(const FString& Key, const FString& Value)
{
	Attributes.Add(Key, Value);
}

FString UOmegaCharacter::GetAttribute(const FString& Key) const
{
	const FString* Value = Attributes.Find(Key);
	return Value ? *Value : TEXT("");
}

void UOmegaCharacter::AddTag(const FString& Tag)
{
	if (!Tags.Contains(Tag))
	{
		Tags.Add(Tag);
	}
}

bool UOmegaCharacter::HasTag(const FString& Tag) const
{
	return Tags.Contains(Tag);
}

void UOmegaCharacter::AddRelationship(UOmegaCharacter* OtherCharacter, const FString& RelationshipType)
{
	if (!OtherCharacter || OtherCharacter == this)
	{
		return;
	}

	// Apply both sides directly instead of recursing (mutual calls would never terminate)
	RelatedCharacters.AddUnique(OtherCharacter);
	RelationshipTypes.Add(OtherCharacter->CharacterID, RelationshipType);

	OtherCharacter->RelatedCharacters.AddUnique(this);
	OtherCharacter->RelationshipTypes.Add(CharacterID, RelationshipType);
}

void UOmegaCharacter::RemoveRelationship(UOmegaCharacter* OtherCharacter)
{
	if (!OtherCharacter || OtherCharacter == this)
	{
		return;
	}

	RelatedCharacters.Remove(OtherCharacter);
	RelationshipTypes.Remove(OtherCharacter->CharacterID);

	OtherCharacter->RelatedCharacters.Remove(this);
	OtherCharacter->RelationshipTypes.Remove(CharacterID);
}

FString UOmegaCharacter::GetRelationshipType(UOmegaCharacter* OtherCharacter) const
{
	if (!OtherCharacter)
	{
		return TEXT("");
	}
	
	const FString* Type = RelationshipTypes.Find(OtherCharacter->CharacterID);
	return Type ? *Type : TEXT("");
}

void UOmegaCharacter::GenerateFromGDD(const FString& GDDText)
{
	// TODO: Implement AI-based character extraction from GDD
	// This will use the OmegaTellerManager's AI system
	
	// For now, parse simple patterns
	if (GDDText.Contains(TEXT("protagonist"), ESearchCase::IgnoreCase) ||
		GDDText.Contains(TEXT("main character"), ESearchCase::IgnoreCase))
	{
		Attributes.Add(TEXT("Role"), TEXT("Protagonist"));
		AddTag(TEXT("Protagonist"));
		AddTag(TEXT("MainCharacter"));
	}
	
	if (GDDText.Contains(TEXT("antagonist"), ESearchCase::IgnoreCase) ||
		GDDText.Contains(TEXT("villain"), ESearchCase::IgnoreCase))
	{
		Attributes.Add(TEXT("Role"), TEXT("Antagonist"));
		AddTag(TEXT("Antagonist"));
		AddTag(TEXT("Villain"));
	}
	
	if (GDDText.Contains(TEXT("mentor"), ESearchCase::IgnoreCase))
	{
		Attributes.Add(TEXT("Role"), TEXT("Mentor"));
		AddTag(TEXT("Mentor"));
		AddTag(TEXT("Supporting"));
	}
	
	// Extract name if possible
	int32 NameStart = GDDText.Find(TEXT("Name:"));
	if (NameStart != INDEX_NONE)
	{
		int32 NameEnd = GDDText.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, NameStart);
		if (NameEnd != INDEX_NONE)
		{
			FString ExtractedName = GDDText.Mid(NameStart + 5, NameEnd - NameStart - 5).TrimStartAndEnd();
			if (!ExtractedName.IsEmpty())
			{
				CharacterName = ExtractedName;
			}
		}
	}
	
	// Create a narrative tree based on character attributes
	PopulateTreeFromAttributes();
}

void UOmegaCharacter::PopulateTreeFromAttributes()
{
	if (!NarrativeTree)
	{
		CreateNarrativeTree(CharacterName + TEXT("'s Story"));
	}
	
	if (!NarrativeTree)
	{
		return;
	}
	
	// Generate tree based on character attributes
	FString Role = GetAttribute(TEXT("Role"));
	FString Arc = GetAttribute(TEXT("Arc"));
	
	if (Role == TEXT("Protagonist"))
	{
		// Protagonist typically has complex branching tree
		NarrativeTree->bIsLinear = false;
		NarrativeTree->MaxBranches = 5;
		
		// Generate a hero's journey template
		NarrativeTree->GenerateTreeFromGDD(TEXT("Hero's journey for ") + CharacterName);
	}
	else if (Role == TEXT("Antagonist"))
	{
		// Antagonist might have simpler, more direct path
		NarrativeTree->bIsLinear = true;
		NarrativeTree->MaxBranches = 2;
		
		// Generate antagonist template
		NarrativeTree->GenerateTreeFromGDD(TEXT("Villain arc for ") + CharacterName);
	}
	else
	{
		// Default template
		NarrativeTree->bIsLinear = true;
		NarrativeTree->MaxBranches = 3;
		
		NarrativeTree->GenerateTreeFromGDD(TEXT("Character story for ") + CharacterName);
	}
}

FString UOmegaCharacter::ExportToJSON() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("CharacterID"), CharacterID);
	Root->SetStringField(TEXT("CharacterName"), CharacterName);
	Root->SetStringField(TEXT("Description"), Description.ToString());

	TSharedRef<FJsonObject> AttrObject = MakeShared<FJsonObject>();
	for (const auto& Attr : Attributes)
	{
		AttrObject->SetStringField(Attr.Key, Attr.Value);
	}
	Root->SetObjectField(TEXT("Attributes"), AttrObject);

	TArray<TSharedPtr<FJsonValue>> TagValues;
	for (const FString& Tag : Tags)
	{
		TagValues.Add(MakeShared<FJsonValueString>(Tag));
	}
	Root->SetArrayField(TEXT("Tags"), TagValues);

	TSharedRef<FJsonObject> RelObject = MakeShared<FJsonObject>();
	for (const auto& Rel : RelationshipTypes)
	{
		RelObject->SetStringField(Rel.Key, Rel.Value);
	}
	Root->SetObjectField(TEXT("Relationships"), RelObject);

	Root->SetBoolField(TEXT("HasTree"), NarrativeTree != nullptr);
	if (NarrativeTree)
	{
		Root->SetStringField(TEXT("TreeID"), NarrativeTree->TreeID);
		// Embed the full tree so a saved character round-trips with its narrative
		Root->SetStringField(TEXT("TreeJSON"), NarrativeTree->ExportToJSON());
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

bool UOmegaCharacter::ImportFromJSON(const FString& JSONString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UOmegaCharacter::ImportFromJSON: invalid JSON input"));
		return false;
	}

	FString InID, InName, InDescription;
	if (Root->TryGetStringField(TEXT("CharacterID"), InID) && !InID.IsEmpty())
	{
		CharacterID = InID;
	}
	if (Root->TryGetStringField(TEXT("CharacterName"), InName) && !InName.IsEmpty())
	{
		CharacterName = InName;
	}
	if (Root->TryGetStringField(TEXT("Description"), InDescription))
	{
		Description = FText::FromString(InDescription);
	}

	const TSharedPtr<FJsonObject>* AttrObject = nullptr;
	if (Root->TryGetObjectField(TEXT("Attributes"), AttrObject) && AttrObject && AttrObject->IsValid())
	{
		Attributes.Empty();
		for (const auto& Pair : (*AttrObject)->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
			{
				Attributes.Add(FString(Pair.Key), Value);
			}
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* TagValues = nullptr;
	if (Root->TryGetArrayField(TEXT("Tags"), TagValues) && TagValues)
	{
		Tags.Empty();
		for (const TSharedPtr<FJsonValue>& Value : *TagValues)
		{
			FString Tag;
			if (Value.IsValid() && Value->TryGetString(Tag) && !Tag.IsEmpty())
			{
				Tags.AddUnique(Tag);
			}
		}
	}

	const TSharedPtr<FJsonObject>* RelObject = nullptr;
	if (Root->TryGetObjectField(TEXT("Relationships"), RelObject) && RelObject && RelObject->IsValid())
	{
		RelationshipTypes.Empty();
		for (const auto& Pair : (*RelObject)->Values)
		{
			FString Value;
			if (Pair.Value.IsValid() && Pair.Value->TryGetString(Value))
			{
				RelationshipTypes.Add(FString(Pair.Key), Value);
			}
		}
	}

	FString TreeJSON;
	if (Root->TryGetStringField(TEXT("TreeJSON"), TreeJSON) && !TreeJSON.IsEmpty())
	{
		if (!NarrativeTree)
		{
			CreateNarrativeTree(CharacterName + TEXT("'s Story"));
		}
		if (NarrativeTree)
		{
			NarrativeTree->ImportFromJSON(TreeJSON);
		}
	}

	return true;
}

TArray<UOmegaNode*> UOmegaCharacter::GetCharacterArc() const
{
	TArray<UOmegaNode*> Arc;
	
	if (!NarrativeTree || !NarrativeTree->RootNode)
	{
		return Arc;
	}
	
	// Simple linear traversal from root to first end
	UOmegaNode* Current = NarrativeTree->RootNode;
	TSet<UOmegaNode*> Visited;

	while (Current && !Visited.Contains(Current))
	{
		Arc.Add(Current);
		Visited.Add(Current);

		// Take the first valid output (entries can be null after GC or partial removal)
		UOmegaNode* Next = nullptr;
		for (UOmegaNode* Output : Current->OutputNodes)
		{
			if (Output)
			{
				Next = Output;
				break;
			}
		}
		Current = Next;
	}
	
	return Arc;
}

FString UOmegaCharacter::GetCharacterSummary() const
{
	FString Summary = FString::Printf(TEXT("Character: %s\n"), *CharacterName);
	Summary += FString::Printf(TEXT("Role: %s\n"), *GetAttribute(TEXT("Role")));
	Summary += FString::Printf(TEXT("Personality: %s\n"), *GetAttribute(TEXT("Personality")));
	Summary += FString::Printf(TEXT("Goals: %s\n"), *GetAttribute(TEXT("Goals")));
	
	Summary += TEXT("Tags: ");
	for (int32 i = 0; i < Tags.Num(); i++)
	{
		Summary += Tags[i];
		if (i < Tags.Num() - 1)
		{
			Summary += TEXT(", ");
		}
	}
	Summary += TEXT("\n");
	
	if (NarrativeTree)
	{
		Summary += FString::Printf(TEXT("Narrative Tree: %s (%d nodes)\n"), 
			*NarrativeTree->TreeName, 
			NarrativeTree->AllNodes.Num());
		
		TArray<UOmegaNode*> EndNodes = NarrativeTree->GetAllEndNodes();
		Summary += FString::Printf(TEXT("Possible Endings: %d\n"), EndNodes.Num());
	}
	else
	{
		Summary += TEXT("No narrative tree created yet.\n");
	}
	
	return Summary;
}