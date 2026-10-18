#pragma once
#include "CoreMinimal.h"
class UFont;
class UCanvas : public UObject
{
public:
	void DrawItem(class FCanvasItem& Item) { (void)Item; }
	void DrawItem(class FCanvasItem& Item, const FVector2D& Position) { (void)Item; (void)Position; }
	void DrawItem(class FCanvasItem& Item, float X, float Y) { (void)Item; (void)X; (void)Y; }
	FVector2D GetTextSize(UFont* Font, const FString& Text, float Scale = 1.0f) const
	{
		(void)Font; (void)Text; (void)Scale;
		return FVector2D();
	}
	int32 SizeX = 1920;
	int32 SizeY = 1080;
};
