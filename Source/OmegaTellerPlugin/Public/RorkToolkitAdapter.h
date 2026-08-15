// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OmegaAIInterface.h"
#include "RorkToolkitAdapter.generated.h"

/**
 * Adapter for Rork Toolkit AI service
 * Provides advanced narrative analysis and generation
 */
UCLASS(BlueprintType, Blueprintable)
class OMEGATELLERPLUGIN_API URorkToolkitAdapter : public UObject, public IOmegaAIInterface
{
	GENERATED_BODY()

public:
	// Constructor
	URorkToolkitAdapter();
	
	// UObject interface
	virtual void BeginDestroy() override;

	// IOmegaAIInterface implementation
	virtual void Initialize_Implementation(const FString& ConfigPath) override;
	virtual bool IsAvailable_Implementation() const override;
	virtual FString GetAIType_Implementation() const override;
	
	virtual TArray<UOmegaCharacter*> ExtractCharactersFromGDD_Implementation(const FString& GDDText) override;
	virtual TArray<FString> ExtractStoryBeatsFromGDD_Implementation(const FString& GDDText) override;
	virtual TMap<FString, FString> ExtractCharacterAttributes_Implementation(const FString& CharacterDescription) override;
	
	virtual UOmegaTree* GenerateTreeForCharacter_Implementation(UOmegaCharacter* Character, const FString& GDDContext) override;
	virtual TArray<UOmegaNode*> GenerateNodesForBeat_Implementation(const FString& StoryBeat, int32 BeatIndex, int32 TotalBeats) override;
	
	virtual FString GenerateNodeDescription_Implementation(const FString& NodeContext, const FString& NodeType) override;
	virtual TMap<FString, FString> GenerateNodeDetails_Implementation(const FString& NodeContext) override;
	virtual TArray<FString> GenerateBranchingOptions_Implementation(const FString& DecisionContext) override;
	
	virtual FString AnalyzeNarrativeStructure_Implementation(const FString& TreeJSON) override;
	virtual TArray<FString> SuggestImprovements_Implementation(const FString& TreeJSON) override;
	virtual FString PredictPlayerChoices_Implementation(const FString& TreeJSON, const TMap<FString, float>& PlayerProfile) override;
	
	virtual float GetConfidenceScore_Implementation() const override;
	virtual FString GetLastError_Implementation() const override;
	virtual void ClearError_Implementation() override;

	// Rork Toolkit specific functions
	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI|RorkToolkit")
	void SetAPIKey(const FString& NewAPIKey);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI|RorkToolkit")
	void SetEndpoint(const FString& NewEndpoint);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI|RorkToolkit")
	void SetModel(const FString& NewModel);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI|RorkToolkit")
	bool TestConnection() const;

protected:
	// Internal HTTP client for API calls
	bool MakeAPIRequest(const FString& Endpoint, const FString& Payload, FString& OutResponse) const;
	
	// Template-based fallback functions
	FString GenerateTemplateDescription(const FString& NodeContext, const FString& NodeType) const;
	TArray<FString> GenerateTemplateBranchingOptions(const FString& DecisionContext) const;
	TMap<FString, FString> ExtractTemplateAttributes(const FString& CharacterDescription) const;

private:
	// Configuration
	FString APIKey;
	FString APIEndpoint;
	FString ModelName;
	
	// State
	mutable FString LastError;
	float ConfidenceScore;
	bool bInitialized;
	bool bAvailable;
	
	// Caching
	TMap<FString, FString> ResponseCache;
	TMap<FString, FDateTime> CacheTimestamps;
	
	// Constants
	static const FString DEFAULT_ENDPOINT;
	static const FString DEFAULT_MODEL;
	static const float CACHE_DURATION_SECONDS;
};