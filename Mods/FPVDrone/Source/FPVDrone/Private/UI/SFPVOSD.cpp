#include "UI/SFPVOSD.h"

#include "Engine/Texture2D.h"
#include "FPVDroneActor.h"
#include "FPVSettings.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

namespace FPVOSDLocal
{
	const FLinearColor White(1.0f, 1.0f, 1.0f, 0.95f);
	const FLinearColor Warn(1.0f, 0.75f, 0.1f, 1.0f);
	const FLinearColor Alarm(1.0f, 0.2f, 0.15f, 1.0f);
	const FLinearColor Shadow(0.0f, 0.0f, 0.0f, 0.8f);

	const TCHAR* ModeName(EFPVFlightMode Mode)
	{
		switch (Mode)
		{
		case EFPVFlightMode::Angle: return TEXT("ANGL");
		case EFPVFlightMode::Horizon: return TEXT("HOR");
		default: return TEXT("ACRO");
		}
	}
}

using namespace FPVOSDLocal;

void SFPVOSD::Construct(const FArguments& InArgs)
{
	Drone = InArgs._Drone;
	SetVisibility(EVisibility::HitTestInvisible);

	Font = FCoreStyle::GetDefaultFontStyle("Bold", 18);
	LargeFont = FCoreStyle::GetDefaultFontStyle("Bold", 30);

	if (InArgs._Vignette != nullptr)
	{
		VignetteBrush.SetResourceObject(InArgs._Vignette);
		VignetteBrush.DrawAs = ESlateBrushDrawType::Image;
		bHasVignette = true;
	}
	if (InArgs._StaticNoise != nullptr)
	{
		StaticBrush.SetResourceObject(InArgs._StaticNoise);
		StaticBrush.DrawAs = ESlateBrushDrawType::Image;
		bHasStatic = true;
	}
}

void SFPVOSD::DrawText(FSlateWindowElementList& Out, int32 Layer, const FGeometry& Geometry, const FVector2D& Position,
	const FString& Text, const FLinearColor& Color, bool bCentered, bool bLarge) const
{
	const FSlateFontInfo& UseFont = bLarge ? LargeFont : Font;
	FVector2D Pos = Position;
	if (bCentered)
	{
		const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		Pos.X -= Measure->Measure(Text, UseFont).X * 0.5;
	}
	const FVector2D Size(2000.0, 60.0);
	FSlateDrawElement::MakeText(Out, Layer, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos + FVector2D(2.0, 2.0))), Text, UseFont, ESlateDrawEffect::None, Shadow);
	FSlateDrawElement::MakeText(Out, Layer + 1, Geometry.ToPaintGeometry(Size, FSlateLayoutTransform(Pos)), Text, UseFont, ESlateDrawEffect::None, Color);
}

int32 SFPVOSD::OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& MyCullingRect,
	FSlateWindowElementList& Out, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const AFPVDroneActor* DronePtr = Drone.Get();
	if (DronePtr == nullptr)
	{
		return LayerId;
	}
	const FFPVConfig& Config = FFPVSettings::Get();
	const FFPVTelemetry T = DronePtr->GetTelemetry();
	const FVector2D Size = Geometry.GetLocalSize();
	const double Time = FPlatformTime::Seconds();
	const bool bBlink = FMath::Fmod(Time, 0.8) < 0.5;

	int32 Layer = LayerId;

	// Analog video breakup with bad link.
	const float Noise = FMath::Clamp(1.0f - T.LinkQuality / 100.0f, 0.0f, 1.0f);
	if (bHasStatic && Noise > 0.15f)
	{
		NoiseSeed = NoiseSeed * 1664525u + 1013904223u;
		const float U = (NoiseSeed >> 8 & 1) ? 0.5f : 0.0f;
		const float V = (NoiseSeed >> 9 & 1) ? 0.5f : 0.0f;
		FSlateBrush Brush = StaticBrush;
		Brush.SetUVRegion(FBox2f(FVector2f(U, V), FVector2f(U + 0.5f, V + 0.5f)));
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), &Brush, ESlateDrawEffect::None,
			FLinearColor(1.0f, 1.0f, 1.0f, FMath::Min(1.0f, Noise * Noise * 1.3f)));
	}
	++Layer;
	if (bHasVignette)
	{
		FSlateDrawElement::MakeBox(Out, Layer, Geometry.ToPaintGeometry(), &VignetteBrush);
	}
	++Layer;

	if (!Config.Camera.bShowOSD)
	{
		return Layer;
	}

	const double Margin = 40.0;
	// Satisfactory's own HUD (health bar, hotbar/tools) lives in the bottom band of the screen, and
	// the goggles OSD is drawn on top of it, so keep the bottom rows clear of that band.
	const double BottomMargin = 150.0;
	const FVector2D Center = Size * 0.5;

	// Crosshair
	DrawText(Out, Layer, Geometry, Center - FVector2D(0.0, 12.0), TEXT("-+-"), White, true);

	// Artificial horizon (useful in Angle/Horizon, shown dimmed in Acro)
	{
		// Banking right turns the horizon counter-clockwise on screen.
		const float Roll = -FMath::DegreesToRadians(T.RollDeg);
		const double PitchOffset = FMath::Clamp(-T.PitchDeg * 4.0, -Size.Y * 0.35, Size.Y * 0.35);
		const FVector2D Dir(FMath::Cos(Roll), FMath::Sin(Roll));
		const FVector2D Mid = Center + FVector2D(-Dir.Y, Dir.X) * PitchOffset;
		TArray<FVector2D> Left = { Mid - Dir * 160.0, Mid - Dir * 50.0 };
		TArray<FVector2D> Right = { Mid + Dir * 50.0, Mid + Dir * 160.0 };
		const FLinearColor LineColor = T.Mode == EFPVFlightMode::Acro ? FLinearColor(1, 1, 1, 0.35f) : White;
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Left, ESlateDrawEffect::None, LineColor, true, 2.0f);
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Right, ESlateDrawEffect::None, LineColor, true, 2.0f);
	}

	// Top left: mode
	FString ModeText = ModeName(T.Mode);
	if (T.b3D) { ModeText += TEXT(" 3D"); }
	if (T.bAirmode) { ModeText += TEXT(" AIR"); }
	if (T.bTurtle) { ModeText = TEXT("TURTLE"); }
	DrawText(Out, Layer, Geometry, FVector2D(Margin, Margin), ModeText, White);
	DrawText(Out, Layer, Geometry, FVector2D(Margin, Margin + 26.0), T.InputSource, FLinearColor(1, 1, 1, 0.6f));

	// Top right: timer + link
	const int32 Seconds = FMath::FloorToInt(T.FlightTime);
	DrawText(Out, Layer, Geometry, FVector2D(Size.X - Margin - 110.0, Margin), FString::Printf(TEXT("%02d:%02d"), Seconds / 60, Seconds % 60), White);
	DrawText(Out, Layer, Geometry, FVector2D(Size.X - Margin - 110.0, Margin + 26.0), FString::Printf(TEXT("LQ %3d"), T.LinkQuality),
		T.LinkQuality < 30 ? Alarm : (T.LinkQuality < 60 ? Warn : White));

	// Bottom left: battery
	const FLinearColor BatteryColor = T.bLowBattery && !T.bCharging ? (bBlink ? Alarm : White) : White;
	if (T.bCharging)
	{
		const FLinearColor ChargeColor(0.35f, 1.0f, 0.45f, 1.0f);
		DrawText(Out, Layer, Geometry, FVector2D(Margin, Size.Y - BottomMargin - 104.0), FString::Printf(TEXT("CHARGING %3.0f%%"), T.StateOfCharge * 100.0f), ChargeColor);
	}
	DrawText(Out, Layer, Geometry, FVector2D(Margin, Size.Y - BottomMargin - 78.0), FString::Printf(TEXT("%.1fV  %.2fV  %3.0f%%"), T.Voltage, T.CellVoltage, T.StateOfCharge * 100.0f), BatteryColor);
	DrawText(Out, Layer, Geometry, FVector2D(Margin, Size.Y - BottomMargin - 52.0), FString::Printf(TEXT("%5.1fA  %4.0fmAh"), T.Current, T.MahUsed), White);
	DrawText(Out, Layer, Geometry, FVector2D(Margin, Size.Y - BottomMargin - 26.0), FString::Printf(TEXT("THR %3.0f%%"), T.ThrottlePercent), White);

	// Bottom right: speed, altitude, home
	const bool bMetric = Config.Camera.bOSDMetric;
	const FString Speed = bMetric ? FString::Printf(TEXT("%3.0f KM/H"), T.SpeedKmh) : FString::Printf(TEXT("%3.0f MPH"), T.SpeedKmh * 0.621371f);
	const FString Alt = bMetric ? FString::Printf(TEXT("ALT %4.0fM"), T.AltitudeM) : FString::Printf(TEXT("ALT %4.0fFT"), T.AltitudeM * 3.28084f);
	const float HomeDist = bMetric ? T.HomeDistanceM : T.HomeDistanceM * 3.28084f;
	const FString Home = bMetric ? FString::Printf(TEXT("H %4.0fM"), HomeDist) : FString::Printf(TEXT("H %4.0fFT"), HomeDist);
	DrawText(Out, Layer, Geometry, FVector2D(Size.X - Margin - 170.0, Size.Y - BottomMargin - 78.0), Speed, White);
	DrawText(Out, Layer, Geometry, FVector2D(Size.X - Margin - 170.0, Size.Y - BottomMargin - 52.0), Alt, White);
	DrawText(Out, Layer, Geometry, FVector2D(Size.X - Margin - 170.0, Size.Y - BottomMargin - 26.0), Home, White);

	// Home arrow above the crosshair
	{
		const float Angle = FMath::DegreesToRadians(T.HomeDirectionDeg);
		const FVector2D ArrowCenter = Center + FVector2D(0.0, -Size.Y * 0.32);
		const FVector2D Tip = ArrowCenter + FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * 18.0;
		const FVector2D Tail = ArrowCenter - FVector2D(FMath::Sin(Angle), -FMath::Cos(Angle)) * 18.0;
		const FVector2D Side(FMath::Cos(Angle) * 8.0, FMath::Sin(Angle) * 8.0);
		TArray<FVector2D> Arrow = { Tail, Tip, Tip - (Tip - Tail).GetSafeNormal() * 10.0 + Side, Tip, Tip - (Tip - Tail).GetSafeNormal() * 10.0 - Side };
		FSlateDrawElement::MakeLines(Out, Layer, Geometry.ToPaintGeometry(), Arrow, ESlateDrawEffect::None, White, true, 2.0f);
	}

	// Centre warnings
	FString Warning;
	FLinearColor WarningColor = Warn;
	if (T.bFailsafe)
	{
		Warning = TEXT("FAILSAFE");
		WarningColor = Alarm;
	}
	else if (!T.bArmed)
	{
		Warning = T.ArmBlockReason.IsEmpty() ? TEXT("DISARMED") : T.ArmBlockReason.ToString();
	}
	else if (T.bLowBattery && !T.bCharging)
	{
		Warning = TEXT("LOW BATTERY");
		WarningColor = Alarm;
	}
	else if (T.LinkQuality < 30)
	{
		Warning = TEXT("LOW LINK");
	}
	if (!Warning.IsEmpty() && (bBlink || !T.bArmed))
	{
		DrawText(Out, Layer, Geometry, Center + FVector2D(0.0, Size.Y * 0.18), Warning, WarningColor, true, true);
	}

	return Layer + 2;
}

#undef LOCTEXT_NAMESPACE
