// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "OmegaAIInterface.generated.h"

// Forward declarations
class UOmegaNode;
class UOmegaTree;
class UOmegaCharacter;

/**
 * Interface for AI services used by OmegaTeller
 * Supports multiple backends: Rork Toolkit, Local AI, Template-based
 */
UINTERFACE(BlueprintType)
class UOmegaAIInterface : public UInterface
{
	GENERATED_BODY()
};

class OMEGATELLERPLUGIN_API IOmegaAIInterface
{
	GENERATED_BODY()

public:
	// AI Service Configuration
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI")
	void Initialize(const FString& ConfigPath);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI")
	bool IsAvailable() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI")
	FString GetAIType() const;

	// GDD Analysis
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|GDD")
	TArray<UOmegaCharacter*> ExtractCharactersFromGDD(const FString& GDDText);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|GDD")
	TArray<FString> ExtractStoryBeatsFromGDD(const FString& GDDText);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|GDD")
	TMap<FString, FString> ExtractCharacterAttributes(const FString& CharacterDescription);

	// Tree Generation
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Tree")
	UOmegaTree* GenerateTreeForCharacter(UOmegaCharacter* Character, const FString& GDDContext);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Tree")
	TArray<UOmegaNode*> GenerateNodesForBeat(const FString& StoryBeat, int32 BeatIndex, int32 TotalBeats);

	// Node Content Generation
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Node")
	FString GenerateNodeDescription(const FString& NodeContext, const FString& NodeType);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Node")
	TMap<FString, FString> GenerateNodeDetails(const FString& NodeContext);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Node")
	TArray<FString> GenerateBranchingOptions(const FString& DecisionContext);

	// Advanced AI Features
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Advanced")
	FString AnalyzeNarrativeStructure(const FString& TreeJSON);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Advanced")
	TArray<FString> SuggestImprovements(const FString& TreeJSON);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Advanced")
	FString PredictPlayerChoices(const FString& TreeJSON, const TMap<FString, float>& PlayerProfile);

	// Utility Functions
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Utility")
	float GetConfidenceScore() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Utility")
	FString GetLastError() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "OmegaTeller|AI|Utility")
	void ClearError();
};