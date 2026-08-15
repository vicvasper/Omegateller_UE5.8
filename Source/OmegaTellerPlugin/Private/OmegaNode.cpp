// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaNode.h"
#include "Misc/Guid.h"

UOmegaNode::UOmegaNode()
{
	NodeID = FGuid::NewGuid().ToString();
	NodeName = TEXT("New Node");
	Description = FText::FromString(TEXT("Node description"));
	NodeType = TEXT("Event");
	NodeColor = FLinearColor(0.2f, 0.5f, 0.8f, 1.0f);
	Position = FVector2D::ZeroVector;
	Size = FVector2D(100.0f, 60.0f);
}

bool UOmegaNode::ConnectToNode(UOmegaNode* TargetNode)
{
	if (!TargetNode || TargetNode == this)
	{
		return false;
	}

	if (!CanConnectTo(TargetNode))
	{
		return false;
	}

	// Check if already connected
	if (IsConnectedTo(TargetNode))
	{
		return false;
	}

	// Add connection
	OutputNodes.Add(TargetNode);
	TargetNode->InputNodes.Add(this);

	return true;
}

bool UOmegaNode::DisconnectFromNode(UOmegaNode* TargetNode)
{
	if (!TargetNode)
	{
		return false;
	}

	// Remove from outputs
	int32 RemovedFromOutput = OutputNodes.Remove(TargetNode);
	
	// Remove from inputs of target
	if (TargetNode)
	{
		TargetNode->InputNodes.Remove(this);
	}

	return RemovedFromOutput > 0;
}

bool UOmegaNode::IsConnectedTo(UOmegaNode* TargetNode) const
{
	return OutputNodes.Contains(TargetNode);
}

void UOmegaNode::UpdatePosition(FVector2D NewPosition)
{
	Position = NewPosition;
}

FString UOmegaNode::GetNodeSummary() const
{
	return FString::Printf(TEXT("%s: %s"), *NodeName, *Description.ToString());
}

void UOmegaNode::GenerateContentFromAI(const FString& Context)
{
	// TODO: Implement AI content generation
	// This will call the OmegaTellerManager's AI system
	
	AIGeneratedContent = TEXT("AI-generated content for: ") + Context;
	
	// Update description with AI content
	if (!AIGeneratedContent.IsEmpty())
	{
		Description = FText::FromString(AIGeneratedContent);
	}
}

bool UOmegaNode::CanConnectTo(UOmegaNode* TargetNode) const
{
	if (!TargetNode)
	{
		return false;
	}

	// Prevent circular connections
	if (TargetNode == this)
	{
		return false;
	}

	// Check if target is already in our output chain (prevent cycles)
	TSet<const UOmegaNode*> Visited;
	TArray<const UOmegaNode*> ToVisit;
	ToVisit.Add(this);

	while (ToVisit.Num() > 0)
	{
		const UOmegaNode* Current = ToVisit.Pop();

		if (!Current)
		{
			continue;
		}

		if (Current == TargetNode)
		{
			return false; // Cycle detected
		}

		if (!Visited.Contains(Current))
		{
			Visited.Add(Current);

			for (const UOmegaNode* Output : Current->OutputNodes)
			{
				if (Output)
				{
					ToVisit.Add(Output);
				}
			}
		}
	}

	// Type-specific connection rules
	if (NodeType == TEXT("End"))
	{
		// End nodes shouldn't have outputs
		return false;
	}

	if (TargetNode->NodeType == TEXT("Start"))
	{
		// Can't connect to start nodes (they should only have outputs)
		return false;
	}

	return true;
}