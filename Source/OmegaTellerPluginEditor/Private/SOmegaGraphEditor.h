// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "OmegaNode.h"
#include "OmegaTree.h"
#include "SOmegaNodeWidget.h"

class SOverlay;
class SScrollBox;
class SCanvas;

/**
 * Main graph editor widget for OmegaTeller
 * Provides Infinite Craft-style node dragging and connection
 */
class SOmegaGraphEditor : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOmegaGraphEditor) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Tree management
	void SetTree(UOmegaTree* InTree);
	UOmegaTree* GetCurrentTree() const { return CurrentTree; }

	// Node operations
	void AddNodeAtPosition(const FVector2D& Position, const FString& NodeType = "Event");
	void RemoveSelectedNodes();
	void ConnectSelectedNodes();
	void CreateConnection(UOmegaNode* FromNode, UOmegaNode* ToNode);

	// Visual operations
	void ZoomToFit();
	void ClearGraph();
	void RefreshGraph();

	// Status text
	FText GetStatusText() const;

	// Selection
	void SelectNode(UOmegaNode* Node, bool bClearSelection = true);
	void DeselectNode(UOmegaNode* Node);
	void ClearSelection();
	TArray<UOmegaNode*> GetSelectedNodes() const { return SelectedNodes; }

protected:
	// Internal widget creation
	TSharedRef<SWidget> CreateGraphEditor();
	TSharedRef<SWidget> CreateToolbar();

	// Event handlers
	FReply OnAddNodeClicked();
	FReply OnRemoveNodesClicked();
	FReply OnConnectNodesClicked();
	FReply OnParseGDDClicked();
	FReply OnZoomToFitClicked();
	FReply OnClearGraphClicked();
	FReply OnSaveTreeClicked();
	FReply OnLoadTreeClicked();
	FReply OnBackgroundClicked();

	// Mouse events
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseWheel(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;

	// Node widget events (bound with the node as payload; the widget pointer is
	// not yet assigned while SAssignNew builds its argument list)
	void OnNodeSelected(UOmegaNode* Node);
	void OnNodePositionChanged(UOmegaNode* Node);

	// Graph painting
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// Drawing helpers
	void DrawGrid(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;
	void DrawConnections(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;

	// Utility functions
	FVector2D ScreenToGraph(const FVector2D& ScreenPoint) const;
	FVector2D GraphToScreen(const FVector2D& GraphPoint) const;
	TSharedPtr<SOmegaNodeWidget> FindNodeWidget(UOmegaNode* Node) const;
	TSharedPtr<SOmegaNodeWidget> FindNodeAtPosition(const FVector2D& GraphPosition) const;

private:
	// Current tree being edited
	UOmegaTree* CurrentTree;

	// Node widgets
	TArray<TSharedPtr<SOmegaNodeWidget>> NodeWidgets;

	// Selected nodes
	TArray<UOmegaNode*> SelectedNodes;

	// UI components
	TSharedPtr<SCanvas> GraphCanvas;
	TSharedPtr<SOverlay> GraphOverlay;
	TSharedPtr<SScrollBox> HorizontalScroll;
	TSharedPtr<SScrollBox> VerticalScroll;

	// Visual state
	FVector2D ViewOffset;
	float ZoomAmount;
	FVector2D PanStartPosition;
	bool bIsPanning;

	// Constants
	static const float GRID_SIZE;
	static const float MIN_ZOOM;
	static const float MAX_ZOOM;
	static const FLinearColor GRID_COLOR;
	static const FLinearColor CONNECTION_COLOR;
};