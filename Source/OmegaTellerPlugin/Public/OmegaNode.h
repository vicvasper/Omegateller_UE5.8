// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "OmegaNode.generated.h"

/**
 * Represents a narrative event node in the story tree
 * Can be connected to other nodes to form branching narratives
 */
UCLASS(BlueprintType, Blueprintable)
class OMEGATELLERPLUGIN_API UOmegaNode : public UObject
{
	GENERATED_BODY()

public:
	UOmegaNode();

	// Node identification
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FString NodeID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FString NodeName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FText Description;

	// Node type classification
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Node")
	FString NodeType; // "Start", "Event", "Decision", "End", "Branch"

	// Visual properties
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FLinearColor NodeColor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector2D Position;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Visual")
	FVector2D Size;

	// Connections
	UPROPERTY()
	TArray<UOmegaNode*> InputNodes;

	UPROPERTY()
	TArray<UOmegaNode*> OutputNodes;

	// Narrative data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Narrative")
	TMap<FString, FString> NarrativeData;

	// AI-generated content
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	FString AIGeneratedContent;

	// Connection management
	UFUNCTION(BlueprintCallable, Category = "Node|Connections")
	bool ConnectToNode(UOmegaNode* TargetNode);

	UFUNCTION(BlueprintCallable, Category = "Node|Connections")
	bool DisconnectFromNode(UOmegaNode* TargetNode);

	UFUNCTION(BlueprintCallable, Category = "Node|Connections")
	bool IsConnectedTo(UOmegaNode* TargetNode) const;

	// Node operations
	UFUNCTION(BlueprintCallable, Category = "Node")
	void UpdatePosition(FVector2D NewPosition);

	UFUNCTION(BlueprintCallable, Category = "Node")
	FString GetNodeSummary() const;

	// AI integration
	UFUNCTION(BlueprintCallable, Category = "Node|AI")
	void GenerateContentFromAI(const FString& Context);

protected:
	// Internal connection validation
	bool CanConnectTo(UOmegaNode* TargetNode) const;
};