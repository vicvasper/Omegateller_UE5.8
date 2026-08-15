// Copyright Epic Games, Inc. All Rights Reserved.

#include "SOmegaNodeWidget.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Framework/Application/SlateApplication.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Rendering/DrawElements.h"

#define LOCTEXT_NAMESPACE "OmegaTeller"

// Visual constants
const float SOmegaNodeWidget::NODE_WIDTH = 150.0f;
const float SOmegaNodeWidget::NODE_HEIGHT = 80.0f;
const float SOmegaNodeWidget::CONNECTION_RADIUS = 6.0f;
const float SOmegaNodeWidget::BORDER_WIDTH = 2.0f;
const FLinearColor SOmegaNodeWidget::SELECTED_COLOR(0.0f, 0.5f, 1.0f, 1.0f);
const FLinearColor SOmegaNodeWidget::HOVERED_COLOR(0.8f, 0.8f, 0.8f, 0.3f);
const FLinearColor SOmegaNodeWidget::CONNECTION_COLOR(0.2f, 0.2f, 0.2f, 1.0f);

// Custom drag operation for nodes
class FOmegaNodeDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FOmegaNodeDragDropOp, FDecoratedDragDropOp)

	static TSharedRef<FOmegaNodeDragDropOp> New(TSharedPtr<SOmegaNodeWidget> InNodeWidget)
	{
		TSharedRef<FOmegaNodeDragDropOp> Operation = MakeShareable(new FOmegaNodeDragDropOp());
		Operation->NodeWidget = InNodeWidget;
		Operation->MouseCursor = EMouseCursor::GrabHandClosed;
		Operation->Construct();
		return Operation;
	}

	TSharedPtr<SOmegaNodeWidget> GetNodeWidget() const { return NodeWidget; }

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		const UOmegaNode* Node = (NodeWidget.IsValid()) ? NodeWidget->GetNode() : nullptr;
		const FString Label = Node ? Node->NodeName : TEXT("Node");

		return SNew(SBox)
			.WidthOverride(SOmegaNodeWidget::NODE_WIDTH)
			.HeightOverride(SOmegaNodeWidget::NODE_HEIGHT)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FSlateColor(FLinearColor::Blue))
				.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(STextBlock)
					.Text(FText::FromString(Label))
					.Justification(ETextJustify::Center)
				]
			];
	}

private:
	TSharedPtr<SOmegaNodeWidget> NodeWidget;
};

void SOmegaNodeWidget::Construct(const FArguments& InArgs)
{
	Node = InArgs._Node;
	OnSelectedEvent = InArgs._OnSelected;
	OnPositionChangedEvent = InArgs._OnPositionChanged;

	// Initialize state
	bSelected = false;
	bHovered = false;
	bDragging = false;
	DragOffset = FVector2D::ZeroVector;
	DragStartPosition = FVector2D::ZeroVector;

	// Create widget
	ChildSlot
	[
		SNew(SBox)
		.WidthOverride(NODE_WIDTH)
		.HeightOverride(NODE_HEIGHT)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("ToolPanel.GroupBorder"))
			.BorderBackgroundColor(this, &SOmegaNodeWidget::GetBorderColor)
			.Padding(4.0f)
			[
				CreateNodeContent()
			]
		]
	];
}

TSharedRef<SWidget> SOmegaNodeWidget::CreateNodeContent()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(2.0f)
		[
			SAssignNew(TitleText, STextBlock)
			.Text(FText::FromString(Node ? Node->NodeName : TEXT("Unnamed")))
			.Font(FCoreStyle::Get().GetFontStyle("Bold"))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
		]
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(2.0f)
		[
			SAssignNew(DescriptionText, STextBlock)
			.Text(FText::FromString(Node ? Node->Description.ToString() : TEXT("No description")))
			.Font(FCoreStyle::Get().GetFontStyle("Small"))
			.Justification(ETextJustify::Center)
			.AutoWrapText(true)
			.WrapTextAt(NODE_WIDTH - 16.0f)
		];
}

void SOmegaNodeWidget::UpdateVisuals()
{
	if (TitleText.IsValid() && Node)
	{
		TitleText->SetText(FText::FromString(Node->NodeName));
	}
	
	if (DescriptionText.IsValid() && Node)
	{
		DescriptionText->SetText(FText::FromString(Node->Description.ToString()));
	}
}

bool SOmegaNodeWidget::IsPointInside(const FVector2D& GraphPoint) const
{
	if (!Node)
	{
		return false;
	}

	FVector2D NodePos = Node->Position;
	FVector2D NodeSize(NODE_WIDTH, NODE_HEIGHT);
	
	return GraphPoint.X >= NodePos.X && 
		   GraphPoint.X <= NodePos.X + NodeSize.X &&
		   GraphPoint.Y >= NodePos.Y && 
		   GraphPoint.Y <= NodePos.Y + NodeSize.Y;
}

FSlateRect SOmegaNodeWidget::GetNodeBounds() const
{
	if (!Node)
	{
		return FSlateRect();
	}

	return FSlateRect(
		Node->Position.X,
		Node->Position.Y,
		Node->Position.X + NODE_WIDTH,
		Node->Position.Y + NODE_HEIGHT
	);
}

void SOmegaNodeWidget::SetSelected(bool bInSelected)
{
	bSelected = bInSelected;
}

FLinearColor SOmegaNodeWidget::GetNodeColor() const
{
	if (!Node)
	{
		return FLinearColor::Gray;
	}

	return Node->NodeColor;
}

FSlateColor SOmegaNodeWidget::GetBorderColor() const
{
	if (bSelected)
	{
		return FSlateColor(SELECTED_COLOR);
	}
	
	if (bHovered)
	{
		return FSlateColor(HOVERED_COLOR);
	}
	
	return FSlateColor(GetNodeColor());
}

FVector2D SOmegaNodeWidget::GetInputConnectionPoint() const
{
	if (!Node)
	{
		return FVector2D::ZeroVector;
	}

	return FVector2D(
		Node->Position.X,
		Node->Position.Y + NODE_HEIGHT / 2.0f
	);
}

FVector2D SOmegaNodeWidget::GetOutputConnectionPoint() const
{
	if (!Node)
	{
		return FVector2D::ZeroVector;
	}

	return FVector2D(
		Node->Position.X + NODE_WIDTH,
		Node->Position.Y + NODE_HEIGHT / 2.0f
	);
}

void SOmegaNodeWidget::DrawConnectionPoints(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	if (!Node)
	{
		return;
	}

	// Draw input connection point
	FVector2D InputPoint = GetInputConnectionPoint();
	FVector2D LocalInputPoint = AllottedGeometry.AbsoluteToLocal(InputPoint);
	
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(CONNECTION_RADIUS * 2, CONNECTION_RADIUS * 2),
			FSlateLayoutTransform(LocalInputPoint - FVector2D(CONNECTION_RADIUS, CONNECTION_RADIUS))
		),
		FCoreStyle::Get().GetBrush("FocusRectangle"),
		ESlateDrawEffect::None,
		CONNECTION_COLOR
	);

	// Draw output connection point
	FVector2D OutputPoint = GetOutputConnectionPoint();
	FVector2D LocalOutputPoint = AllottedGeometry.AbsoluteToLocal(OutputPoint);
	
	FSlateDrawElement::MakeBox(
		OutDrawElements,
		LayerId++,
		AllottedGeometry.ToPaintGeometry(
			FVector2D(CONNECTION_RADIUS * 2, CONNECTION_RADIUS * 2),
			FSlateLayoutTransform(LocalOutputPoint - FVector2D(CONNECTION_RADIUS, CONNECTION_RADIUS))
		),
		FCoreStyle::Get().GetBrush("FocusRectangle"),
		ESlateDrawEffect::None,
		CONNECTION_COLOR
	);
}

void SOmegaNodeWidget::DrawConnections(const FGeometry& AllottedGeometry, FSlateWindowElementList& OutDrawElements, int32& LayerId) const
{
	if (!Node || Node->OutputNodes.Num() == 0)
	{
		return;
	}

	FVector2D StartPoint = GetOutputConnectionPoint();
	
	for (UOmegaNode* OutputNode : Node->OutputNodes)
	{
		// In a real implementation, we'd need access to other node widgets
		// For now, this is a placeholder
	}
}

int32 SOmegaNodeWidget::OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, 
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, 
	int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// First paint the widget normally
	int32 MaxLayerId = SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bParentEnabled);
	
	// Draw connection points
	DrawConnectionPoints(AllottedGeometry, OutDrawElements, MaxLayerId);
	
	// Draw connections to other nodes
	DrawConnections(AllottedGeometry, OutDrawElements, MaxLayerId);
	
	return MaxLayerId;
}

FReply SOmegaNodeWidget::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		// Select this node
		if (OnSelectedEvent.IsBound())
		{
			OnSelectedEvent.Execute();
		}
		
		// Prepare for drag
		DragStartPosition = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		return FReply::Handled().CaptureMouse(AsShared()).DetectDrag(AsShared(), EKeys::LeftMouseButton);
	}
	
	return FReply::Unhandled();
}

FReply SOmegaNodeWidget::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (HasMouseCapture())
		{
			return FReply::Handled().ReleaseMouseCapture();
		}
	}
	
	return FReply::Unhandled();
}

FReply SOmegaNodeWidget::OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (HasMouseCapture() && !MouseEvent.GetCursorDelta().IsZero())
	{
		// We're dragging - this will be handled by OnDragDetected
		return FReply::Handled();
	}
	
	return FReply::Unhandled();
}

void SOmegaNodeWidget::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bHovered = true;
	SCompoundWidget::OnMouseEnter(MyGeometry, MouseEvent);
}

void SOmegaNodeWidget::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bHovered = false;
	SCompoundWidget::OnMouseLeave(MouseEvent);
}

FReply SOmegaNodeWidget::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bDragging = true;
	
	TSharedRef<FOmegaNodeDragDropOp> DragDropOp = FOmegaNodeDragDropOp::New(SharedThis(this));
	DragOffset = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
	
	return FReply::Handled().BeginDragDrop(DragDropOp);
}

void SOmegaNodeWidget::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	SCompoundWidget::OnDragEnter(MyGeometry, DragDropEvent);
}

void SOmegaNodeWidget::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	SCompoundWidget::OnDragLeave(DragDropEvent);
}

FReply SOmegaNodeWidget::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bDragging = false;
	
	// Check if we're dropping another node
	TSharedPtr<FOmegaNodeDragDropOp> NodeDragOp = DragDropEvent.GetOperationAs<FOmegaNodeDragDropOp>();
	if (NodeDragOp.IsValid())
	{
		TSharedPtr<SOmegaNodeWidget> DraggedNode = NodeDragOp->GetNodeWidget();
		if (DraggedNode.IsValid() && DraggedNode != SharedThis(this)
			&& DraggedNode->GetNode() && GetNode())
		{
			// In a full implementation, we would create a connection between nodes
			// For now, just log the event
			UE_LOG(LogTemp, Log, TEXT("Node %s dropped on node %s"),
				*DraggedNode->GetNode()->NodeName,
				*GetNode()->NodeName);

			return FReply::Handled();
		}
	}
	
	return FReply::Unhandled();
}

#undef LOCTEXT_NAMESPACE