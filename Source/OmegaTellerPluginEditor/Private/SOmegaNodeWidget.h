// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "OmegaNode.h"

class SImage;
class STextBlock;

/**
 * Visual widget for an OmegaNode in the graph editor
 * Supports dragging, dropping, and visual customization
 */
class SOmegaNodeWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SOmegaNodeWidget) {}
	
	/** The node this widget represents */
	SLATE_ARGUMENT(UOmegaNode*, Node)
	
	/** Called when node is selected */
	SLATE_EVENT(FSimpleDelegate, OnSelected)
	
	/** Called when node position changes */
	SLATE_EVENT(FSimpleDelegate, OnPositionChanged)
	
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	// Get the underlying node
	UOmegaNode* GetNode() const { return Node; }

	// Update visual appearance
	void UpdateVisuals();

	// Check if point is inside node
	bool IsPointInside(const FVector2D& GraphPoint) const;

	// Get node bounds in graph coordinates
	FSlateRect GetNodeBounds() const;

	// Selection
	void SetSelected(bool bInSelected);
	bool IsSelected() const { return bSelected; }

	// Drag operations
	FVector2D GetDragOffset() const { return DragOffset; }
	void SetDragOffset(const FVector2D& InOffset) { DragOffset = InOffset; }

protected:
	// Mouse event handlers
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override;

	// Drag and drop
	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	virtual void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;
	virtual void OnDragLeave(const FDragDropEvent& DragDropEvent) override;
	virtual FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent) override;

	// Paint override for custom rendering
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
		int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	// Create node content
	TSharedRef<SWidget> CreateNodeContent();

	// Get color based on node type
	FLinearColor GetNodeColor() const;

	// Get border color based on selection state
	FSlateColor GetBorderColor() const;

	// Connection point helpers
	FVector2D GetInputConnectionPoint() const;
	FVector2D GetOutputConnectionPoint() const;

	// Draw connection points
	void DrawConnectionPoints(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;

	// Draw connections to other nodes
	void DrawConnections(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const;

private:
	// The underlying node
	UOmegaNode* Node;

	// Visual components
	TSharedPtr<SImage> BackgroundImage;
	TSharedPtr<STextBlock> TitleText;
	TSharedPtr<STextBlock> DescriptionText;

	// Events
	FSimpleDelegate OnSelectedEvent;
	FSimpleDelegate OnPositionChangedEvent;

	// State
	bool bSelected;
	bool bHovered;
	bool bDragging;
	FVector2D DragOffset;
	FVector2D DragStartPosition;

	// Visual settings
public:
	static const float NODE_WIDTH;
	static const float NODE_HEIGHT;
private:
	static const float CONNECTION_RADIUS;
	static const float BORDER_WIDTH;
	static const FLinearColor SELECTED_COLOR;
	static const FLinearColor HOVERED_COLOR;
	static const FLinearColor CONNECTION_COLOR;
};