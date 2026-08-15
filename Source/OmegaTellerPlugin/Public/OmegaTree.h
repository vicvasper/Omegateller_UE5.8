// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OmegaNode.h"
#include "OmegaTree.generated.h"

/**
 * Represents a complete narrative tree for a character or story
 * Contains nodes and manages their connections
 */
UCLASS(BlueprintType, Blueprintable)
class OMEGATELLERPLUGIN_API UOmegaTree : public UObject
{
	GENERATED_BODY()

public:
	UOmegaTree();

	// Tree identification
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FString TreeID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FString TreeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	FText Description;

	// Tree structure
	UPROPERTY()
	UOmegaNode* RootNode;

	UPROPERTY()
	TArray<UOmegaNode*> AllNodes;

	UPROPERTY()
	TArray<UOmegaNode*> EndNodes;

	// Tree properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	bool bIsLinear = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	int32 MaxBranches = 5;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tree")
	int32 MaxDepth = 10;

	// Node management
	UFUNCTION(BlueprintCallable, Category = "Tree|Nodes")
	UOmegaNode* CreateNode(const FString& NodeName, const FString& NodeType = "Event");

	UFUNCTION(BlueprintCallable, Category = "Tree|Nodes")
	bool RemoveNode(UOmegaNode* NodeToRemove);

	UFUNCTION(BlueprintCallable, Category = "Tree|Nodes")
	UOmegaNode* FindNodeByID(const FString& NodeID) const;

	// Connection management
	UFUNCTION(BlueprintCallable, Category = "Tree|Connections")
	bool ConnectNodes(UOmegaNode* SourceNode, UOmegaNode* TargetNode);

	UFUNCTION(BlueprintCallable, Category = "Tree|Connections")
	bool DisconnectNodes(UOmegaNode* SourceNode, UOmegaNode* TargetNode);

	// Tree analysis
	UFUNCTION(BlueprintCallable, Category = "Tree|Analysis")
	TArray<UOmegaNode*> FindPath(UOmegaNode* StartNode, UOmegaNode* EndNode) const;

	UFUNCTION(BlueprintCallable, Category = "Tree|Analysis")
	TArray<UOmegaNode*> GetAllEndNodes() const;

	UFUNCTION(BlueprintCallable, Category = "Tree|Analysis")
	int32 CalculateTreeComplexity() const;

	// Tree validation
	UFUNCTION(BlueprintCallable, Category = "Tree|Validation")
	bool ValidateTree() const;

	UFUNCTION(BlueprintCallable, Category = "Tree|Validation")
	TArray<FString> GetValidationErrors() const;

	// AI operations
	UFUNCTION(BlueprintCallable, Category = "Tree|AI")
	void GenerateTreeFromGDD(const FString& GDDText);

	UFUNCTION(BlueprintCallable, Category = "Tree|AI")
	void PopulateNodeContent();

	// Serialization
	UFUNCTION(BlueprintCallable, Category = "Tree|Serialization")
	FString ExportToJSON() const;

	UFUNCTION(BlueprintCallable, Category = "Tree|Serialization")
	bool ImportFromJSON(const FString& JSONString);

	// Clear the entire graph
	UFUNCTION(BlueprintCallable, Category = "Tree")
	void ClearGraph();

protected:
	// Internal node management
	void AddNode(UOmegaNode* NewNode);
	void RemoveNodeInternal(UOmegaNode* NodeToRemove);

	// Tree traversal
	void TraverseTree(UOmegaNode* CurrentNode, TSet<UOmegaNode*>& Visited) const;
};