// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OmegaTree.h"
#include "OmegaCharacter.generated.h"

/**
 * Represents a character with their own narrative tree
 * Characters are extracted from GDD analysis
 */
UCLASS(BlueprintType, Blueprintable)
class OMEGATELLERPLUGIN_API UOmegaCharacter : public UObject
{
	GENERATED_BODY()

public:
	UOmegaCharacter();

	// Character identification
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FString CharacterID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FString CharacterName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	FText Description;

	// Character properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	TMap<FString, FString> Attributes; // Role, Personality, Goals, etc.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character")
	TArray<FString> Tags;

	// Narrative tree
	UPROPERTY()
	UOmegaTree* NarrativeTree;

	// Relationships with other characters
	UPROPERTY()
	TArray<UOmegaCharacter*> RelatedCharacters;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Character|Relationships")
	TMap<FString, FString> RelationshipTypes; // CharacterID -> RelationshipType

	// Tree management
	UFUNCTION(BlueprintCallable, Category = "Character|Tree")
	UOmegaTree* CreateNarrativeTree(const FString& TreeName);

	UFUNCTION(BlueprintCallable, Category = "Character|Tree")
	bool HasNarrativeTree() const;

	// Character operations
	UFUNCTION(BlueprintCallable, Category = "Character")
	void UpdateAttribute(const FString& Key, const FString& Value);

	UFUNCTION(BlueprintCallable, Category = "Character")
	FString GetAttribute(const FString& Key) const;

	UFUNCTION(BlueprintCallable, Category = "Character")
	void AddTag(const FString& Tag);

	UFUNCTION(BlueprintCallable, Category = "Character")
	bool HasTag(const FString& Tag) const;

	// Relationship management
	UFUNCTION(BlueprintCallable, Category = "Character|Relationships")
	void AddRelationship(UOmegaCharacter* OtherCharacter, const FString& RelationshipType);

	UFUNCTION(BlueprintCallable, Category = "Character|Relationships")
	void RemoveRelationship(UOmegaCharacter* OtherCharacter);

	UFUNCTION(BlueprintCallable, Category = "Character|Relationships")
	FString GetRelationshipType(UOmegaCharacter* OtherCharacter) const;

	// AI operations
	UFUNCTION(BlueprintCallable, Category = "Character|AI")
	void GenerateFromGDD(const FString& GDDText);

	UFUNCTION(BlueprintCallable, Category = "Character|AI")
	void PopulateTreeFromAttributes();

	// Serialization
	UFUNCTION(BlueprintCallable, Category = "Character|Serialization")
	FString ExportToJSON() const;

	UFUNCTION(BlueprintCallable, Category = "Character|Serialization")
	bool ImportFromJSON(const FString& JSONString);

	// Analysis
	UFUNCTION(BlueprintCallable, Category = "Character|Analysis")
	TArray<UOmegaNode*> GetCharacterArc() const;

	UFUNCTION(BlueprintCallable, Category = "Character|Analysis")
	FString GetCharacterSummary() const;

protected:
	// Initialize with default values
	void InitializeDefaults();
};