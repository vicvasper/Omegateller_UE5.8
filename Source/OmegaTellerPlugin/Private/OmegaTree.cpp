// Copyright Epic Games, Inc. All Rights Reserved.

#include "OmegaTree.h"
#include "Misc/Guid.h"
#include "OmegaTellerManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

UOmegaTree::UOmegaTree()
{
	TreeID = FGuid::NewGuid().ToString();
	TreeName = TEXT("New Tree");
	Description = FText::FromString(TEXT("Narrative tree"));
	RootNode = nullptr;
	bIsLinear = false;
	MaxBranches = 5;
	MaxDepth = 10;
}

UOmegaNode* UOmegaTree::CreateNode(const FString& NodeName, const FString& NodeType)
{
	// Outer = this tree, so nodes share the tree's lifetime instead of floating in the transient package
	UOmegaNode* NewNode = NewObject<UOmegaNode>(this);
	NewNode->NodeName = NodeName.IsEmpty() ? TEXT("Unnamed Node") : NodeName;
	NewNode->NodeType = NodeType.IsEmpty() ? TEXT("Event") : NodeType;
	NewNode->NodeID = FGuid::NewGuid().ToString();

	// Set default size based on type
	if (NodeType == TEXT("Start") || NodeType == TEXT("End"))
	{
		NewNode->Size = FVector2D(80.0f, 40.0f);
	}
	else
	{
		NewNode->Size = FVector2D(120.0f, 60.0f);
	}

	AddNode(NewNode);

	// If this is the first node and it's a Start node, set as root
	if (!RootNode && NodeType == TEXT("Start"))
	{
		RootNode = NewNode;
	}

	// If it's an End node, add to EndNodes
	if (NodeType == TEXT("End"))
	{
		EndNodes.Add(NewNode);
	}

	return NewNode;
}

bool UOmegaTree::RemoveNode(UOmegaNode* NodeToRemove)
{
	if (!NodeToRemove || !AllNodes.Contains(NodeToRemove))
	{
		return false;
	}

	RemoveNodeInternal(NodeToRemove);
	return true;
}

UOmegaNode* UOmegaTree::FindNodeByID(const FString& NodeID) const
{
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node && Node->NodeID == NodeID)
		{
			return Node;
		}
	}
	return nullptr;
}

bool UOmegaTree::ConnectNodes(UOmegaNode* SourceNode, UOmegaNode* TargetNode)
{
	if (!SourceNode || !TargetNode || 
		!AllNodes.Contains(SourceNode) || 
		!AllNodes.Contains(TargetNode))
	{
		return false;
	}

	return SourceNode->ConnectToNode(TargetNode);
}

bool UOmegaTree::DisconnectNodes(UOmegaNode* SourceNode, UOmegaNode* TargetNode)
{
	if (!SourceNode || !TargetNode)
	{
		return false;
	}

	return SourceNode->DisconnectFromNode(TargetNode);
}

TArray<UOmegaNode*> UOmegaTree::FindPath(UOmegaNode* StartNode, UOmegaNode* EndNode) const
{
	TArray<UOmegaNode*> Path;
	
	if (!StartNode || !EndNode || 
		!AllNodes.Contains(StartNode) || 
		!AllNodes.Contains(EndNode))
	{
		return Path;
	}

	// Simple BFS to find path
	TMap<UOmegaNode*, UOmegaNode*> ParentMap;
	TQueue<UOmegaNode*> Queue;
	TSet<UOmegaNode*> Visited;

	Queue.Enqueue(StartNode);
	Visited.Add(StartNode);

	while (!Queue.IsEmpty())
	{
		UOmegaNode* Current = nullptr;
		Queue.Dequeue(Current);

		if (!Current)
		{
			continue;
		}

		if (Current == EndNode)
		{
			// Reconstruct path
			UOmegaNode* Node = EndNode;
			while (Node)
			{
				Path.Insert(Node, 0);
				Node = ParentMap.FindRef(Node);
			}
			break;
		}

		for (UOmegaNode* Neighbor : Current->OutputNodes)
		{
			if (Neighbor && !Visited.Contains(Neighbor))
			{
				Visited.Add(Neighbor);
				ParentMap.Add(Neighbor, Current);
				Queue.Enqueue(Neighbor);
			}
		}
	}

	return Path;
}

TArray<UOmegaNode*> UOmegaTree::GetAllEndNodes() const
{
	TArray<UOmegaNode*> Ends;
	
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node && (Node->NodeType == TEXT("End") || Node->OutputNodes.Num() == 0))
		{
			Ends.Add(Node);
		}
	}
	
	return Ends;
}

int32 UOmegaTree::CalculateTreeComplexity() const
{
	int32 Complexity = 0;
	
	// Count nodes
	Complexity += AllNodes.Num();
	
	// Count connections
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node)
		{
			Complexity += Node->OutputNodes.Num();
		}
	}
	
	// Add bonus for branching
	if (!bIsLinear)
	{
		Complexity *= 2;
	}
	
	return Complexity;
}

bool UOmegaTree::ValidateTree() const
{
	// Cycle detection with three-color DFS. A node in the current DFS path (gray)
	// reached again means a cycle; a node already fully explored (black) reached
	// again is just converging branches (e.g. a decision diamond) and is valid.
	TSet<UOmegaNode*> Gray;
	TSet<UOmegaNode*> Black;

	for (UOmegaNode* StartNode : AllNodes)
	{
		if (!StartNode || Black.Contains(StartNode))
		{
			continue;
		}

		// Stack entries: node + whether we are entering (true) or leaving (false) it
		TArray<TPair<UOmegaNode*, bool>> Stack;
		Stack.Push(TPair<UOmegaNode*, bool>(StartNode, true));

		while (Stack.Num() > 0)
		{
			TPair<UOmegaNode*, bool> Entry = Stack.Pop();
			UOmegaNode* Current = Entry.Key;

			if (!Entry.Value)
			{
				Gray.Remove(Current);
				Black.Add(Current);
				continue;
			}

			if (Black.Contains(Current))
			{
				continue;
			}

			Gray.Add(Current);
			Stack.Push(TPair<UOmegaNode*, bool>(Current, false));

			for (UOmegaNode* Output : Current->OutputNodes)
			{
				if (!Output || Black.Contains(Output))
				{
					continue;
				}
				if (Gray.Contains(Output))
				{
					return false; // Cycle detected
				}
				Stack.Push(TPair<UOmegaNode*, bool>(Output, true));
			}
		}
	}

	// Check for orphaned nodes (no connections)
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node && Node != RootNode && Node->InputNodes.Num() == 0 && Node->OutputNodes.Num() == 0)
		{
			// Orphaned node (except root which might not have inputs)
			return false;
		}
	}

	return true;
}

TArray<FString> UOmegaTree::GetValidationErrors() const
{
	TArray<FString> Errors;
	
	// Check for self-connections
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node && Node->OutputNodes.Contains(Node))
		{
			Errors.Add(FString::Printf(TEXT("Node '%s' connects to itself"), *Node->NodeName));
		}
	}

	// Check for orphaned nodes
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node && Node != RootNode && Node->InputNodes.Num() == 0 && Node->OutputNodes.Num() == 0)
		{
			Errors.Add(FString::Printf(TEXT("Node '%s' is orphaned (no connections)"), *Node->NodeName));
		}
	}
	
	// Check depth
	if (AllNodes.Num() > MaxDepth * 10) // Rough estimate
	{
		Errors.Add(FString::Printf(TEXT("Tree may be too deep (max recommended: %d)"), MaxDepth));
	}
	
	return Errors;
}

void UOmegaTree::GenerateTreeFromGDD(const FString& GDDText)
{
	// TODO: Implement AI-based tree generation from GDD
	// This will use the OmegaTellerManager's AI system
	
	// For now, create a simple example tree
	ClearGraph();
	
	UOmegaNode* StartNode = CreateNode(TEXT("Start"), TEXT("Start"));
	StartNode->Position = FVector2D(100.0f, 300.0f);
	
	UOmegaNode* Event1 = CreateNode(TEXT("First Event"), TEXT("Event"));
	Event1->Position = FVector2D(300.0f, 200.0f);
	
	UOmegaNode* Event2 = CreateNode(TEXT("Second Event"), TEXT("Event"));
	Event2->Position = FVector2D(300.0f, 400.0f);
	
	UOmegaNode* Decision = CreateNode(TEXT("Decision Point"), TEXT("Decision"));
	Decision->Position = FVector2D(500.0f, 300.0f);
	
	UOmegaNode* End1 = CreateNode(TEXT("Good Ending"), TEXT("End"));
	End1->Position = FVector2D(700.0f, 200.0f);
	
	UOmegaNode* End2 = CreateNode(TEXT("Bad Ending"), TEXT("End"));
	End2->Position = FVector2D(700.0f, 400.0f);
	
	// Connect nodes
	ConnectNodes(StartNode, Event1);
	ConnectNodes(StartNode, Event2);
	ConnectNodes(Event1, Decision);
	ConnectNodes(Event2, Decision);
	ConnectNodes(Decision, End1);
	ConnectNodes(Decision, End2);
	
	// Generate AI content for nodes
	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	if (Manager)
	{
		StartNode->Description = FText::FromString(Manager->GenerateNodeDescription(TEXT("Story beginning")));
		Event1->Description = FText::FromString(Manager->GenerateNodeDescription(TEXT("First major event")));
		Event2->Description = FText::FromString(Manager->GenerateNodeDescription(TEXT("Alternative first event")));
		Decision->Description = FText::FromString(Manager->GenerateNodeDescription(TEXT("Critical decision point")));
		End1->Description = FText::FromString(Manager->GenerateNodeDescription(TEXT("Positive conclusion")));
		End2->Description = FText::FromString(Manager->GenerateNodeDescription(TEXT("Negative conclusion")));
	}
}

void UOmegaTree::PopulateNodeContent()
{
	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	if (!Manager)
	{
		return;
	}
	
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node && Node->AIGeneratedContent.IsEmpty())
		{
			Node->GenerateContentFromAI(Node->NodeName);
		}
	}
}

FString UOmegaTree::ExportToJSON() const
{
	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("TreeID"), TreeID);
	Root->SetStringField(TEXT("TreeName"), TreeName);
	Root->SetStringField(TEXT("Description"), Description.ToString());
	Root->SetBoolField(TEXT("bIsLinear"), bIsLinear);
	Root->SetNumberField(TEXT("MaxBranches"), MaxBranches);
	Root->SetNumberField(TEXT("MaxDepth"), MaxDepth);
	if (RootNode)
	{
		Root->SetStringField(TEXT("RootNodeID"), RootNode->NodeID);
	}

	TArray<TSharedPtr<FJsonValue>> NodeValues;
	for (UOmegaNode* Node : AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedRef<FJsonObject> NodeObject = MakeShared<FJsonObject>();
		NodeObject->SetStringField(TEXT("NodeID"), Node->NodeID);
		NodeObject->SetStringField(TEXT("NodeName"), Node->NodeName);
		NodeObject->SetStringField(TEXT("Description"), Node->Description.ToString());
		NodeObject->SetStringField(TEXT("NodeType"), Node->NodeType);
		NodeObject->SetStringField(TEXT("AIGeneratedContent"), Node->AIGeneratedContent);
		NodeObject->SetNumberField(TEXT("PositionX"), Node->Position.X);
		NodeObject->SetNumberField(TEXT("PositionY"), Node->Position.Y);
		NodeObject->SetNumberField(TEXT("SizeX"), Node->Size.X);
		NodeObject->SetNumberField(TEXT("SizeY"), Node->Size.Y);
		NodeObject->SetStringField(TEXT("Color"), Node->NodeColor.ToString());

		TSharedRef<FJsonObject> DataObject = MakeShared<FJsonObject>();
		for (const auto& Pair : Node->NarrativeData)
		{
			DataObject->SetStringField(Pair.Key, Pair.Value);
		}
		NodeObject->SetObjectField(TEXT("NarrativeData"), DataObject);

		TArray<TSharedPtr<FJsonValue>> OutputIDs;
		for (UOmegaNode* Output : Node->OutputNodes)
		{
			if (Output)
			{
				OutputIDs.Add(MakeShared<FJsonValueString>(Output->NodeID));
			}
		}
		NodeObject->SetArrayField(TEXT("Outputs"), OutputIDs);

		NodeValues.Add(MakeShared<FJsonValueObject>(NodeObject));
	}
	Root->SetArrayField(TEXT("Nodes"), NodeValues);

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root, Writer);
	return Output;
}

bool UOmegaTree::ImportFromJSON(const FString& JSONString)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JSONString);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("UOmegaTree::ImportFromJSON: invalid JSON input"));
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* NodeValues = nullptr;
	if (!Root->TryGetArrayField(TEXT("Nodes"), NodeValues) || !NodeValues)
	{
		UE_LOG(LogTemp, Warning, TEXT("UOmegaTree::ImportFromJSON: missing 'Nodes' array"));
		return false;
	}

	ClearGraph();

	FString InValue;
	if (Root->TryGetStringField(TEXT("TreeID"), InValue) && !InValue.IsEmpty())
	{
		TreeID = InValue;
	}
	if (Root->TryGetStringField(TEXT("TreeName"), InValue) && !InValue.IsEmpty())
	{
		TreeName = InValue;
	}
	if (Root->TryGetStringField(TEXT("Description"), InValue))
	{
		Description = FText::FromString(InValue);
	}
	Root->TryGetBoolField(TEXT("bIsLinear"), bIsLinear);
	Root->TryGetNumberField(TEXT("MaxBranches"), MaxBranches);
	Root->TryGetNumberField(TEXT("MaxDepth"), MaxDepth);

	// First pass: create every node and index it by ID
	TMap<FString, UOmegaNode*> NodesByID;
	for (const TSharedPtr<FJsonValue>& Value : *NodeValues)
	{
		const TSharedPtr<FJsonObject>* NodeObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(NodeObject) || !NodeObject || !NodeObject->IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *NodeObject;

		FString NodeID;
		if (!Obj->TryGetStringField(TEXT("NodeID"), NodeID) || NodeID.IsEmpty() || NodesByID.Contains(NodeID))
		{
			// Skip malformed or duplicated entries instead of aborting the whole import
			continue;
		}

		UOmegaNode* NewNode = NewObject<UOmegaNode>(this);
		NewNode->NodeID = NodeID;

		FString Field;
		if (Obj->TryGetStringField(TEXT("NodeName"), Field))
		{
			NewNode->NodeName = Field;
		}
		if (Obj->TryGetStringField(TEXT("Description"), Field))
		{
			NewNode->Description = FText::FromString(Field);
		}
		if (Obj->TryGetStringField(TEXT("NodeType"), Field) && !Field.IsEmpty())
		{
			NewNode->NodeType = Field;
		}
		Obj->TryGetStringField(TEXT("AIGeneratedContent"), NewNode->AIGeneratedContent);

		double X = 0.0, Y = 0.0;
		if (Obj->TryGetNumberField(TEXT("PositionX"), X) && Obj->TryGetNumberField(TEXT("PositionY"), Y))
		{
			NewNode->Position = FVector2D(static_cast<float>(X), static_cast<float>(Y));
		}
		if (Obj->TryGetNumberField(TEXT("SizeX"), X) && Obj->TryGetNumberField(TEXT("SizeY"), Y))
		{
			NewNode->Size = FVector2D(static_cast<float>(X), static_cast<float>(Y));
		}
		if (Obj->TryGetStringField(TEXT("Color"), Field))
		{
			FLinearColor ParsedColor;
			if (ParsedColor.InitFromString(Field))
			{
				NewNode->NodeColor = ParsedColor;
			}
		}

		const TSharedPtr<FJsonObject>* DataObject = nullptr;
		if (Obj->TryGetObjectField(TEXT("NarrativeData"), DataObject) && DataObject && DataObject->IsValid())
		{
			for (const auto& Pair : (*DataObject)->Values)
			{
				FString DataValue;
				if (Pair.Value.IsValid() && Pair.Value->TryGetString(DataValue))
				{
					NewNode->NarrativeData.Add(FString(Pair.Key), DataValue);
				}
			}
		}

		NodesByID.Add(NodeID, NewNode);
		AddNode(NewNode);

		if (NewNode->NodeType == TEXT("End"))
		{
			EndNodes.Add(NewNode);
		}
	}

	// Second pass: restore connections by ID (skip self-links and unknown targets)
	for (const TSharedPtr<FJsonValue>& Value : *NodeValues)
	{
		const TSharedPtr<FJsonObject>* NodeObject = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(NodeObject) || !NodeObject || !NodeObject->IsValid())
		{
			continue;
		}
		const TSharedPtr<FJsonObject>& Obj = *NodeObject;

		FString NodeID;
		if (!Obj->TryGetStringField(TEXT("NodeID"), NodeID))
		{
			continue;
		}
		UOmegaNode* SourceNode = NodesByID.FindRef(NodeID);
		if (!SourceNode)
		{
			continue;
		}

		const TArray<TSharedPtr<FJsonValue>>* OutputIDs = nullptr;
		if (!Obj->TryGetArrayField(TEXT("Outputs"), OutputIDs) || !OutputIDs)
		{
			continue;
		}

		for (const TSharedPtr<FJsonValue>& OutputValue : *OutputIDs)
		{
			FString TargetID;
			if (!OutputValue.IsValid() || !OutputValue->TryGetString(TargetID))
			{
				continue;
			}
			UOmegaNode* TargetNode = NodesByID.FindRef(TargetID);
			if (TargetNode && TargetNode != SourceNode && !SourceNode->OutputNodes.Contains(TargetNode))
			{
				SourceNode->OutputNodes.Add(TargetNode);
				TargetNode->InputNodes.Add(SourceNode);
			}
		}
	}

	// Restore the root: explicit ID first, then any Start node as fallback
	FString RootNodeID;
	if (Root->TryGetStringField(TEXT("RootNodeID"), RootNodeID))
	{
		RootNode = NodesByID.FindRef(RootNodeID);
	}
	if (!RootNode)
	{
		for (UOmegaNode* Node : AllNodes)
		{
			if (Node && Node->NodeType == TEXT("Start"))
			{
				RootNode = Node;
				break;
			}
		}
	}

	UE_LOG(LogTemp, Log, TEXT("UOmegaTree::ImportFromJSON: imported %d nodes into tree '%s'"), AllNodes.Num(), *TreeName);
	return true;
}

void UOmegaTree::AddNode(UOmegaNode* NewNode)
{
	if (NewNode && !AllNodes.Contains(NewNode))
	{
		AllNodes.Add(NewNode);
	}
}

void UOmegaTree::RemoveNodeInternal(UOmegaNode* NodeToRemove)
{
	if (!NodeToRemove)
	{
		return;
	}
	
	// Remove from all node connections
	for (UOmegaNode* Node : AllNodes)
	{
		if (Node)
		{
			Node->InputNodes.Remove(NodeToRemove);
			Node->OutputNodes.Remove(NodeToRemove);
		}
	}
	
	// Remove from special lists
	if (RootNode == NodeToRemove)
	{
		RootNode = nullptr;
	}
	
	EndNodes.Remove(NodeToRemove);
	AllNodes.Remove(NodeToRemove);
	
	// TODO: Handle object destruction properly
}

void UOmegaTree::TraverseTree(UOmegaNode* CurrentNode, TSet<UOmegaNode*>& Visited) const
{
	if (!CurrentNode || Visited.Contains(CurrentNode))
	{
		return;
	}
	
	Visited.Add(CurrentNode);
	
	for (UOmegaNode* Output : CurrentNode->OutputNodes)
	{
		TraverseTree(Output, Visited);
	}
}

void UOmegaTree::ClearGraph()
{
	// Create copy of array to avoid modification during iteration
	TArray<UOmegaNode*> NodesToRemove = AllNodes;
	
	for (UOmegaNode* Node : NodesToRemove)
	{
		RemoveNodeInternal(Node);
	}
	
	RootNode = nullptr;
	EndNodes.Empty();
}