#pragma once
#include "CoreMinimal.h"

class UFont;

class FCanvasItem
{
public:
	virtual ~FCanvasItem() = default;
	FLinearColor SetColor;
	bool bOutlined = false;
	FLinearColor OutlineColor;
};

class FCanvasTextItem : public FCanvasItem
{
public:
	FCanvasTextItem(const FVector2D& InPosition, const FText& InText, UFont* InFont,
		const FLinearColor& InColour)
		: Position(InPosition), Text(InText), Font(InFont)
	{
		SetColor = InColour;
	}
	void SetScale(const FVector2D& InScale) { Scale = InScale; }
	void EnableShadow(const FLinearColor& Colour, const FVector2D& Offset = FVector2D(1.0, 1.0))
	{
		(void)Colour;
		(void)Offset;
	}
	void DisableShadow() {}
	FVector2D Position;
	FText Text;
	UFont* Font = nullptr;
	FVector2D Scale{1.0, 1.0};
	bool bCentreX = false;
	bool bCentreY = false;
};

class FCanvasTileItem : public FCanvasItem
{
public:
	FCanvasTileItem(const FVector2D& InPosition, const FVector2D& InSize, const FLinearColor& InColour)
		: Position(InPosition), Size(InSize)
	{
		SetColor = InColour;
	}
	FVector2D Position;
	FVector2D Size;
	uint8 BlendMode = 0;
};

class FCanvasLineItem : public FCanvasItem
{
public:
	FCanvasLineItem(const FVector2D& InStart, const FVector2D& InEnd) : Origin(InStart), EndPos(InEnd) {}
	FVector2D Origin;
	FVector2D EndPos;
	float LineThickness = 1.0f;
};

class FCanvasBoxItem : public FCanvasItem
{
public:
	FCanvasBoxItem(const FVector2D& InPosition, const FVector2D& InSize) : Position(InPosition), Size(InSize) {}
	FVector2D Position;
	FVector2D Size;
	float LineThickness = 1.0f;
};
