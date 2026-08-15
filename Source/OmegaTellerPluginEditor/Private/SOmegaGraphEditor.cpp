// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOmegaGraphEditor.h"
#include "SGDDParseDialog.h"
#include "OmegaTellerManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Rendering/DrawElements.h"

#define LOCTEXT_NAMESPACE "OmegaTeller"

// Visual constants
const float SOmegaGraphEditor::GRID_SIZE = 50.0f;
const float SOmegaGraphEditor::MIN_ZOOM = 0.25f;
const float SOmegaGraphEditor::MAX_ZOOM = 4.0f;
const FLinearColor SOmegaGraphEditor::GRID_COLOR(0.1f, 0.1f, 0.1f, 0.3f);
const FLinearColor SOmegaGraphEditor::CONNECTION_COLOR(0.5f, 0.5f, 0.5f, 1.0f);

void SOmegaGraphEditor::Construct(const FArguments& InArgs)
{
	// Create a default tree
	UOmegaTellerManager* Manager = UOmegaTellerManager::Get();
	CurrentTree = Manager->CreateNewTree(TEXT("New Narrative Tree"));

	// Initialize visual state
	ViewOffset = FVector2D::ZeroVector;
	ZoomAmount = 1.0f;
	bIsPanning = false;
	PanStartPosition = FVector2D::ZeroVector;

	// Create toolbar and graph editor
	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreateToolbar()
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			CreateGraphEditor()
		]
	];

	// Refresh to show initial nodes
	RefreshGraph();
}

TSharedRef<SWidget> SOmegaGraphEditor::CreateToolbar()
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
		.Padding(4.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddNode", "Add Node"))
				.OnClicked(this, &SOmegaGraphEditor::OnAddNodeClicked)
				.ToolTipText(LOCTEXT("AddNodeTooltip", "Add a new narrative node"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("RemoveNodes", "Remove Selected"))
				.OnClicked(this, &SOmegaGraphEditor::OnRemoveNodesClicked)
				.ToolTipText(LOCTEXT("RemoveNodesTooltip", "Remove selected nodes"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ConnectNodes", "Connect Selected"))
				.OnClicked(this, &SOmegaGraphEditor::OnConnectNodesClicked)
				.ToolTipText(LOCTEXT("ConnectNodesTooltip", "Connect selected nodes"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ParseGDD", "Parse GDD"))
				.OnClicked(this, &SOmegaGraphEditor::OnParseGDDClicked)
				.ToolTipText(LOCTEXT("ParseGDDTooltip", "Parse Game Design Document"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ZoomToFit", "Zoom to Fit"))
				.OnClicked(this, &SOmegaGraphEditor::OnZoomToFitClicked)
				.ToolTipText(LOCTEXT("ZoomToFitTooltip", "Zoom to fit all nodes"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("ClearGraph", "Clear Graph"))
				.OnClicked(this, &SOmegaGraphEditor::OnClearGraphClicked)
				.ToolTipText(LOCTEXT("ClearGraphTooltip", "Clear the entire graph"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("SaveTree", "Save Tree"))
				.OnClicked(this, &SOmegaGraphEditor::OnSaveTreeClicked)
				.ToolTipText(LOCTEXT("SaveTreeTooltip", "Save tree to file"))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("LoadTree", "Load Tree"))
				.OnClicked(this, &SOmegaGraphEditor::OnLoadTreeClicked)
				.ToolTipText(LOCTEXT("LoadTreeTooltip", "Load tree from file"))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SNew(SSpacer)
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2.0f)
			[
				SNew(STextBlock)
				.Text(this, &SOmegaGraphEditor::GetStatusText)
			]
		];
}

TSharedRef<SWidget> SOmegaGraphEditor::CreateGraphEditor()
{
	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(0.0f)
		[
			SAssignNew(HorizontalScroll, SScrollBox)
			.Orientation(Orient_Horizontal)
			+ SScrollBox::Slot()
			[
				SAssignNew(VerticalScroll, SScrollBox)
				.Orientation(Orient_Vertical)
				+ SScrollBox::Slot()
				[
					SNew(SBox)
					.WidthOverride(2000.0f)
					.HeightOverride(2000.0f)
					[
						SAssignNew(GraphCanvas, SCanvas)
					]
				]
			]
		];
}

FText SOmegaGraphEditor::GetStatusText() const
{
	if (!CurrentTree)
	{
		return LOCTEXT("NoTree", "No tree loaded");
	}

	int32 NodeCount = CurrentTree->AllNodes.Num();
	int32 SelectedCount = SelectedNodes.Num();
	int32 ConnectionCount = 0;
	
	for (UOmegaNode* Node : CurrentTree->AllNodes)
	{
		if (Node)
		{
			ConnectionCount += Node->OutputNodes.Num();
		}
	}

	return FText::FromString(FString::Printf(TEXT("Nodes: %d | Selected: %d | Connections: %d"), 
		NodeCount, SelectedCount, ConnectionCount));
}

void SOmegaGraphEditor::SetTree(UOmegaTree* InTree)
{
	CurrentTree = InTree;
	RefreshGraph();
}

void SOmegaGraphEditor::AddNodeAtPosition(const FVector2D& Position, const FString& NodeType)
{
	if (!CurrentTree)
	{
		return;
	}

	// Convert screen position to graph position
	FVector2D GraphPosition = ScreenToGraph(Position);
	
	FString NodeName = FString::Printf(TEXT("%s Node"), *NodeType);
	UOmegaNode* NewNode = CurrentTree->CreateNode(NodeName, NodeType);
	
	if (NewNode)
	{
		NewNode->Position = GraphPosition;
		NewNode->UpdatePosition(GraphPosition);
		
		// Set color based on node type
		if (NodeType == "Start")
		{
			NewNode->NodeColor = FLinearColor::Green;
		}
		else if (NodeType == "End")
		{
			NewNode->NodeColor = FLinearColor::Red;
		}
		else if (NodeType == "Decision")
		{
			NewNode->NodeColor = FLinearColor::Yellow;
		}
		else if (NodeType == "Branch")
		{
			NewNode->NodeColor = FLinearColor(0.8f, 0.2f, 0.8f, 1.0f); // Purple
		}
		else
		{
			NewNode->NodeColor = FLinearColor(0.2f, 0.5f, 0.8f, 1.0f); // Blue
		}

		// Create widget for the new node
		RefreshGraph();
		
		// Select the new node
		SelectNode(NewNode, true);
	}
}

void SOmegaGraphEditor::RemoveSelectedNodes()
{
	for (UOmegaNode* Node : SelectedNodes)
	{
		if (CurrentTree)
		{
			CurrentTree->RemoveNode(Node);
		}
	}
	SelectedNodes.Empty();
	RefreshGraph();
}

void SOmegaGraphEditor::ConnectSelectedNodes()
{
	if (SelectedNodes.Num() >= 2)
	{
		// Connect nodes in selection order
		for (int32 i = 0; i < SelectedNodes.Num() - 1; i++)
		{
			UOmegaNode* SourceNode = SelectedNodes[i];
			UOmegaNode* TargetNode = SelectedNodes[i + 1];
			
			CreateConnection(SourceNode, TargetNode);
		}
	}
}

void SOmegaGraphEditor::CreateConnection(UOmegaNode* FromNode, UOmegaNode* ToNode)
{
	if (!CurrentTree || !FromNode || !ToNode)
	{
		return;
	}

	if (CurrentTree->ConnectNodes(FromNode, ToNode))
	{
		// Refresh to show the new connection
		RefreshGraph();
	}
}

void SOmegaGraphEditor::ZoomToFit()
{
	if (!CurrentTree || CurrentTree->AllNodes.Num() == 0)
	{
		ZoomAmount = 1.0f;
		ViewOffset = FVector2D::ZeroVector;
		return;
	}

	// Simple implementation for now
	ZoomAmount = 0.8f;
	ViewOffset = FVector2D(100.0f, 100.0f);
	
	// In a full implementation, we would calculate bounds and adjust zoom/offset
}

void SOmegaGraphEditor::ClearGraph()
{
	if (CurrentTree)
	{
		CurrentTree->ClearGraph();
		SelectedNodes.Empty();
		NodeWidgets.Empty();
		
		if (GraphCanvas.IsValid())
		{
			GraphCanvas->ClearChildren();
		}
	}
}

void SOmegaGraphEditor::RefreshGraph()
{
	if (!CurrentTree || !GraphCanvas.IsValid())
	{
		return;
	}

	// Clear existing widgets
	NodeWidgets.Empty();
	GraphCanvas->ClearChildren();

	// Create widgets for all nodes
	for (UOmegaNode* Node : CurrentTree->AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		TSharedPtr<SOmegaNodeWidget> NodeWidget;

		// Bind with the node as payload: the NodeWidget shared pointer is still null
		// while the SAssignNew argument list is being evaluated
		SAssignNew(NodeWidget, SOmegaNodeWidget)
			.Node(Node)
			.OnSelected(FSimpleDelegate::CreateSP(this, &SOmegaGraphEditor::OnNodeSelected, Node))
			.OnPositionChanged(FSimpleDelegate::CreateSP(this, &SOmegaGraphEditor::OnNodePositionChanged, Node));

		NodeWidgets.Add(NodeWidget);

		// Add to canvas at node position
		FVector2D ScreenPosition = GraphToScreen(Node->Position);
		GraphCanvas->AddSlot()
			.Position(ScreenPosition)
			.Size(FVector2D(150.0f, 80.0f))
			[
				NodeWidget.ToSharedRef()
			];
	}
}

void SOmegaGraphEditor::SelectNode(UOmegaNode* Node, bool bClearSelection)
{
	if (!Node)
	{
		return;
	}

	if (bClearSelection)
	{
		ClearSelection();
	}

	// Add to selection if not already selected
	if (!SelectedNodes.Contains(Node))
	{
		SelectedNodes.Add(Node);
		
		// Update widget selection state
		TSharedPtr<SOmegaNodeWidget> NodeWidget = FindNodeWidget(Node);
		if (NodeWidget.IsValid())
		{
			NodeWidget->SetSelected(true);
		}
	}
}

void SOmegaGraphEditor::DeselectNode(UOmegaNode* Node)
{
	if (!Node)
	{
		return;
	}

	SelectedNodes.Remove(Node);
	
	// Update widget selection state
	TSharedPtr<SOmegaNodeWidget> NodeWidget = FindNodeWidget(Node);
	if (NodeWidget.IsValid())
	{
		NodeWidget->SetSelected(false);
	}
}

void SOmegaGraphEditor::ClearSelection()
{
	// Deselect all nodes
	for (UOmegaNode* Node : SelectedNodes)
	{
		TSharedPtr<SOmegaNodeWidget> NodeWidget = FindNodeWidget(Node);
		if (NodeWidget.IsValid())
		{
			NodeWidget->SetSelected(false);
		}
	}
	
	SelectedNodes.Empty();
}

FReply SOmegaGraphEditor::OnAddNodeClicked()
{
	// Add node at center of view
	AddNodeAtPosition(FVector2D(500.0f, 300.0f), "Event");
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnRemoveNodesClicked()
{
	RemoveSelectedNodes();
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnConnectNodesClicked()
{
	ConnectSelectedNodes();
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnParseGDDClicked()
{
	// Create and show the GDD parse dialog
	TSharedRef<SWindow> ParseWindow = SNew(SWindow)
		.Title(LOCTEXT("ParseGDDWindowTitle", "Parse Game Design Document"))
		.ClientSize(FVector2D(800, 600))
		.SupportsMinimize(false)
		.SupportsMaximize(false)
		.SizingRule(ESizingRule::UserSized);

	TSharedRef<SGDDParseDialog> ParseDialog = SNew(SGDDParseDialog)
		.ParentWindow(ParseWindow);

	ParseWindow->SetContent(ParseDialog);

	// Show the window
	FSlateApplication::Get().AddWindow(ParseWindow);

	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnZoomToFitClicked()
{
	ZoomToFit();
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnClearGraphClicked()
{
	ClearGraph();
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnSaveTreeClicked()
{
	// TODO: Implement save file dialog
	UE_LOG(LogTemp, Log, TEXT("Save tree requested"));
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnLoadTreeClicked()
{
	// TODO: Implement load file dialog
	UE_LOG(LogTemp, Log, TEXT("Load tree requested"));
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnBackgroundClicked()
{
	// Click on background clears selection
	ClearSelection();
	return FReply::Handled();
}

FReply SOmegaGraphEditor::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	// Middle mouse (or Ctrl+Left) starts panning from anywhere on the canvas
	if (MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton ||
		(MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && MouseEvent.IsControlDown()))
	{
		bIsPanning = true;
		PanStartPosition = MouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(AsShared());
	}

	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Check if we clicked on a node
		FVector2D GraphPosition = ScreenToGraph(MouseEvent.GetScreenSpacePosition());
		TSharedPtr<SOmegaNodeWidget> ClickedNode = FindNodeAtPosition(GraphPosition);

		if (ClickedNode.IsValid())
		{
			// Node was clicked - selection will be handled by the node widget
			return FReply::Handled();
		}

		// Background click - clear selection
		ClearSelection();
	}

	return FReply::Unhandled();
}

FReply SOmegaGraphEditor::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton || 
		MouseEvent.GetEffectingButton() == EKeys::MiddleMouseButton)
	{
		if (bIsPanning)
		{
			bIsPanning = false;
			if (HasMouseCapture())
			{
				return FReply::Handled().ReleaseMouseCapture();
			}
		}
	}
	
	return FReply::Unhandled();
}

FReply SOmegaGraphEditor::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (bIsPanning && HasMouseCapture())
	{
		FVector2D CurrentPosition = MouseEvent.GetScreenSpacePosition();
		FVector2D Delta = CurrentPosition - PanStartPosition;
		
		// Adjust view offset
		ViewOffset += Delta / ZoomAmount;
		PanStartPosition = CurrentPosition;
		
		// Update node positions
		RefreshGraph();
		
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

FReply SOmegaGraphEditor::OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	float WheelDelta = MouseEvent.GetWheelDelta();
	
	// Adjust zoom
	float OldZoom = ZoomAmount;
	ZoomAmount = FMath::Clamp(ZoomAmount + WheelDelta * 0.1f, MIN_ZOOM, MAX_ZOOM);
	
	// Adjust view offset to zoom around mouse position
	if (OldZoom != ZoomAmount)
	{
		FVector2D MouseGraphPos = ScreenToGraph(MouseEvent.GetScreenSpacePosition());
		ViewOffset = MouseGraphPos - (MouseEvent.GetScreenSpacePosition() - ViewOffset) * (OldZoom / ZoomAmount);
		
		// Update node positions
		RefreshGraph();
	}
	
	return FReply::Handled();
}

void SOmegaGraphEditor::OnNodeSelected(UOmegaNode* Node)
{
	if (!Node)
	{
		return;
	}

	// Toggle selection
	if (SelectedNodes.Contains(Node))
	{
		DeselectNode(Node);
	}
	else
	{
		bool bCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
		SelectNode(Node, !bCtrlDown);
	}
}

void SOmegaGraphEditor::OnNodePositionChanged(UOmegaNode* Node)
{
	if (!Node || !GraphCanvas.IsValid())
	{
		return;
	}

	// Position is already updated by the node widget
	// Just refresh the graph to update connections
	RefreshGraph();
}

int32 SOmegaGraphEditor::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// First paint the widget normally
	int32 MaxLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	
	// Draw grid
	DrawGrid(AllottedGeometry, OutDrawElements, MaxLayerId);
	
	// Draw connections
	DrawConnections(AllottedGeometry, OutDrawElements, MaxLayerId);
	
	return MaxLayerId;
}

void SOmegaGraphEditor::DrawGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	// Simple grid drawing
	const FVector2D GridStep = FVector2D(GRID_SIZE, GRID_SIZE) * ZoomAmount;
	const FVector2D LocalSize = AllottedGeometry.GetLocalSize();
	
	// Draw vertical lines
	for (float X = 0; X < LocalSize.X; X += GridStep.X)
	{
		TArray<FVector2D> LinePoints;
		LinePoints.Add(FVector2D(X, 0));
		LinePoints.Add(FVector2D(X, LocalSize.Y));
		
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			GRID_COLOR,
			true,
			1.0f
		);
	}
	
	// Draw horizontal lines
	for (float Y = 0; Y < LocalSize.Y; Y += GridStep.Y)
	{
		TArray<FVector2D> LinePoints;
		LinePoints.Add(FVector2D(0, Y));
		LinePoints.Add(FVector2D(LocalSize.X, Y));
		
		FSlateDrawElement::MakeLines(
			OutDrawElements,
			LayerId++,
			AllottedGeometry.ToPaintGeometry(),
			LinePoints,
			ESlateDrawEffect::None,
			GRID_COLOR,
			true,
			1.0f
		);
	}
}

void SOmegaGraphEditor::DrawConnections(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	if (!CurrentTree)
	{
		return;
	}

	// Draw connections between nodes
	for (UOmegaNode* Node : CurrentTree->AllNodes)
	{
		if (!Node)
		{
			continue;
		}

		FVector2D StartPos = GraphToScreen(Node->Position + FVector2D(150.0f, 40.0f)); // Right side of node

		for (UOmegaNode* OutputNode : Node->OutputNodes)
		{
			if (!OutputNode)
			{
				continue;
			}

			FVector2D EndPos = GraphToScreen(OutputNode->Position + FVector2D(0.0f, 40.0f)); // Left side of node
			
			TArray<FVector2D> LinePoints;
			LinePoints.Add(StartPos);
			LinePoints.Add(EndPos);
			
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(),
				LinePoints,
				ESlateDrawEffect::None,
				CONNECTION_COLOR,
				true,
				2.0f
			);
			
			// Draw arrow head
			FVector2D Direction = (EndPos - StartPos).GetSafeNormal();
			FVector2D ArrowBase = EndPos - Direction * 10.0f;
			FVector2D Perpendicular = FVector2D(-Direction.Y, Direction.X) * 5.0f;
			
			TArray<FVector2D> ArrowPoints;
			ArrowPoints.Add(EndPos);
			ArrowPoints.Add(ArrowBase + Perpendicular);
			ArrowPoints.Add(ArrowBase - Perpendicular);
			ArrowPoints.Add(EndPos);
			
			FSlateDrawElement::MakeLines(
				OutDrawElements,
				LayerId++,
				AllottedGeometry.ToPaintGeometry(),
				ArrowPoints,
				ESlateDrawEffect::None,
				CONNECTION_COLOR,
				true,
				2.0f
			);
		}
	}
}

FVector2D SOmegaGraphEditor::ScreenToGraph(const FVector2D& ScreenPoint) const
{
	// Convert screen coordinates to graph coordinates
	return (ScreenPoint - ViewOffset) / ZoomAmount;
}

FVector2D SOmegaGraphEditor::GraphToScreen(const FVector2D& GraphPoint) const
{
	// Convert graph coordinates to screen coordinates
	return GraphPoint * ZoomAmount + ViewOffset;
}

TSharedPtr<SOmegaNodeWidget> SOmegaGraphEditor::FindNodeWidget(UOmegaNode* Node) const
{
	for (TSharedPtr<SOmegaNodeWidget> Widget : NodeWidgets)
	{
		if (Widget.IsValid() && Widget->GetNode() == Node)
		{
			return Widget;
		}
	}
	
	return nullptr;
}

TSharedPtr<SOmegaNodeWidget> SOmegaGraphEditor::FindNodeAtPosition(const FVector2D& GraphPosition) const
{
	for (TSharedPtr<SOmegaNodeWidget> Widget : NodeWidgets)
	{
		if (Widget.IsValid() && Widget->IsPointInside(GraphPosition))
		{
			return Widget;
		}
	}
	
	return nullptr;
}

#undef LOCTEXT_NAMESPACE