// Betaflight-style on-screen display drawn directly with Slate draw elements.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class AFPVDroneActor;
class UTexture2D;

class FPVDRONE_API SFPVOSD : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SFPVOSD) {}
		SLATE_ARGUMENT(AFPVDroneActor*, Drone)
		SLATE_ARGUMENT(UTexture2D*, Vignette)
		SLATE_ARGUMENT(UTexture2D*, StaticNoise)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1.0, 1.0); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	void DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position,
		const FString& Text, const FLinearColor& Color, bool bCentered = false, bool bLarge = false) const;

	TWeakObjectPtr<AFPVDroneActor> Drone;
	FSlateBrush VignetteBrush;
	FSlateBrush StaticBrush;
	bool bHasVignette = false;
	bool bHasStatic = false;
	FSlateFontInfo Font;
	FSlateFontInfo LargeFont;
	mutable uint32 NoiseSeed = 1;
};
