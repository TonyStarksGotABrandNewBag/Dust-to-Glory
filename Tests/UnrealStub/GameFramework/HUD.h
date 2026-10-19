#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

class UCanvas;
class APlayerController;

class AHUD : public AActor
{
public:
	virtual void DrawHUD() {}
	void DrawLine(float X0, float Y0, float X1, float Y1, const FLinearColor& Colour, float Thickness = 0.0f)
	{
		(void)X0; (void)Y0; (void)X1; (void)Y1; (void)Colour; (void)Thickness;
	}
	void DrawRect(const FLinearColor& Colour, float X, float Y, float Width, float Height)
	{
		(void)Colour; (void)X; (void)Y; (void)Width; (void)Height;
	}
	void DrawText(const FString& Text, const FLinearColor& Colour, float X, float Y,
		class UFont* Font = nullptr, float Scale = 1.0f, bool bScalePosition = false)
	{
		(void)Text; (void)Colour; (void)X; (void)Y; (void)Font; (void)Scale; (void)bScalePosition;
	}
	APlayerController* GetOwningPlayerController() const { return PlayerOwner; }
	class APawn* GetOwningPawn() const;
	UCanvas* Canvas = nullptr;
	APlayerController* PlayerOwner = nullptr;
	/** NOTE: a uint8 bitfield in the real engine; assigning true/false works the same on both. */
	bool bShowHUD = true;
};
