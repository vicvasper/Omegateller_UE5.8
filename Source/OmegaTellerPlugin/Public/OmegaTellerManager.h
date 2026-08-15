// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OmegaAIInterface.h"
#include "OmegaTellerManager.generated.h"

// Forward declarations
class UOmegaNode;
class UOmegaTree;
class UOmegaCharacter;
class URorkToolkitAdapter;

/**
 * Singleton manager for the OmegaTeller system
 * Handles global state, AI integration, and tree management
 */
UCLASS(BlueprintType, Blueprintable)
class OMEGATELLERPLUGIN_API UOmegaTellerManager : public UObject
{
	GENERATED_BODY()

public:
	// Singleton access
	UFUNCTION(BlueprintCallable, Category = "OmegaTeller")
	static UOmegaTellerManager* Get();

	// UObject interface
	virtual void BeginDestroy() override;

	// Tree management
	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Trees")
	UOmegaTree* CreateNewTree(const FString& TreeName);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Trees")
	TArray<UOmegaTree*> GetAllTrees() const;

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Trees")
	UOmegaTree* FindTreeByID(const FString& TreeID) const;

	// Character management
	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Characters")
	UOmegaCharacter* CreateCharacter(const FString& CharacterName);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Characters")
	TArray<UOmegaCharacter*> GetAllCharacters() const;

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Characters")
	UOmegaCharacter* FindCharacterByID(const FString& CharacterID) const;

	// AI Integration
	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI")
	void ParseGDD(const FString& GDDText);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI")
	FString GenerateNodeDescription(const FString& Context);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI")
	TArray<UOmegaCharacter*> ExtractCharactersWithAI(const FString& GDDText);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI")
	UOmegaTree* GenerateTreeWithAI(UOmegaCharacter* Character, const FString& GDDContext);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI")
	void SetAIType(const FString& NewAIType);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI")
	FString GetCurrentAIType() const;

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|AI")
	bool IsAIAvailable() const;

	// File operations
	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Files")
	bool SaveTreeToFile(UOmegaTree* Tree, const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Files")
	UOmegaTree* LoadTreeFromFile(const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Files")
	bool SaveCharacterToFile(UOmegaCharacter* Character, const FString& FilePath);

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Files")
	UOmegaCharacter* LoadCharacterFromFile(const FString& FilePath);

	// Utility functions
	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Utility")
	FString GetPluginVersion() const;

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Utility")
	FString GetSystemStatus() const;

	UFUNCTION(BlueprintCallable, Category = "OmegaTeller|Utility")
	void ClearAllData();

	// Get appropriate AI interface based on configuration
	TScriptInterface<IOmegaAIInterface> GetAIInterface();

protected:
	// Singleton instance
	static UOmegaTellerManager* Instance;

	// Managed objects
	UPROPERTY()
	TArray<UOmegaTree*> ManagedTrees;

	UPROPERTY()
	TArray<UOmegaCharacter*> ManagedCharacters;

	// AI system
	UPROPERTY()
	TScriptInterface<IOmegaAIInterface> AIInterface;

	UPROPERTY()
	URorkToolkitAdapter* RorkToolkitAdapter;

	// AI configuration
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString AIType = "RorkToolkit"; // "RorkToolkit", "Local", or "Template"

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString ModelPath = "";

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString APIKey = "";

	// Initialize AI system
	void InitializeAISystem();

private:
	// Private constructor for singleton
	UOmegaTellerManager();
};