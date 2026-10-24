#include "UI/SFPVConfigMenu.h"

#include "FPVSettings.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Input/FPVHIDDevice.h"
#include "Input/FPVInputSystem.h"
#include "Rendering/DrawElements.h"
#include "Sim/FPVRates.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

namespace FPVMenuLocal
{
	const FLinearColor Accent(1.0f, 0.55f, 0.05f, 1.0f);
	const FLinearColor Panel(0.03f, 0.035f, 0.045f, 0.94f);
	const FLinearColor AxisColors[3] = { FLinearColor(1.0f, 0.35f, 0.3f), FLinearColor(0.35f, 1.0f, 0.4f), FLinearColor(0.4f, 0.6f, 1.0f) };

	FSlateFontInfo BodyFont() { return FCoreStyle::GetDefaultFontStyle("Regular", 12); }
	FSlateFontInfo BoldFont(int32 Size = 13) { return FCoreStyle::GetDefaultFontStyle("Bold", Size); }

	const FFPVAxisRates& AxisRates(const FFPVTune& Tune, int32 Axis)
	{
		return Axis == 0 ? Tune.RollRates : (Axis == 1 ? Tune.PitchRates : Tune.YawRates);
	}

	// Plots the stick -> deg/s curve for all three axes.
	class SRateGraph : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SRateGraph) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&) {}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(420.0, 260.0); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
			int32 LayerId, const FWidgetStyle&, bool) const override
		{
			const FFPVTune& Tune = FFPVSettings::Get().Tune;
			const FVector2D Size = Geometry.GetLocalSize();
			const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
			FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(), White, ESlateDrawEffect::None, FLinearColor(0, 0, 0, 0.5f));

			const FPVSim::RatesType Type = static_cast<FPVSim::RatesType>(Tune.RatesType);
			float MaxRate = 200.0f;
			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const FFPVAxisRates& R = AxisRates(Tune, Axis);
				MaxRate = FMath::Max(MaxRate, FPVSim::MaxRate(Type, { R.A, R.B, R.C }));
			}
			MaxRate = FMath::CeilToFloat(MaxRate / 200.0f) * 200.0f;

			for (int32 Grid = 1; Grid < MaxRate / 200.0f; ++Grid)
			{
				const double Y = Size.Y * (1.0 - Grid * 200.0 / MaxRate);
				TArray<FVector2D> Line = { FVector2D(0.0, Y), FVector2D(Size.X, Y) };
				FSlateDrawElement::MakeLines(Out, LayerId + 1, Geometry.ToPaintGeometry(), Line, ESlateDrawEffect::None, FLinearColor(1, 1, 1, 0.12f));
			}

			for (int32 Axis = 0; Axis < 3; ++Axis)
			{
				const FFPVAxisRates& R = AxisRates(Tune, Axis);
				TArray<FVector2D> Points;
				for (int32 Index = 0; Index <= 64; ++Index)
				{
					const float Stick = Index / 64.0f;
					const float Rate = FPVSim::ApplyRates(Type, { R.A, R.B, R.C }, Stick);
					Points.Add(FVector2D(Size.X * Stick, Size.Y * (1.0 - Rate / MaxRate)));
				}
				FSlateDrawElement::MakeLines(Out, LayerId + 2, Geometry.ToPaintGeometry(), Points, ESlateDrawEffect::None, AxisColors[Axis], true, 2.0f);

				const FString Label = FString::Printf(TEXT("%s max %.0f deg/s"), Axis == 0 ? TEXT("Roll") : (Axis == 1 ? TEXT("Pitch") : TEXT("Yaw")),
					FPVSim::MaxRate(Type, { R.A, R.B, R.C }));
				FSlateDrawElement::MakeText(Out, LayerId + 3, Geometry.ToPaintGeometry(FVector2D(300.0, 20.0), FSlateLayoutTransform(FVector2D(8.0, 6.0 + Axis * 18.0))),
					Label, BodyFont(), ESlateDrawEffect::None, AxisColors[Axis]);
			}
			return LayerId + 4;
		}
	};

	// Live view of the radio: raw HID axes, buttons and the resulting channels.
	class SChannelView : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SChannelView) {}
		SLATE_END_ARGS()

		void Construct(const FArguments&) {}

		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(460.0, 330.0); }

		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& Geometry, const FSlateRect&, FSlateWindowElementList& Out,
			int32 LayerId, const FWidgetStyle&, bool) const override
		{
			const FSlateBrush* White = FCoreStyle::Get().GetBrush("WhiteBrush");
			const FFPVRadioSettings& Radio = FFPVSettings::Get().Radio;
			FFPVHIDSnapshot Snapshot;
			FFPVHIDManager::Get().GetSnapshot(Snapshot);
			const FFPVPilotInput& Last = FFPVInputSystem::Get().GetLast();

			auto Bar = [&](int32 Row, const FString& Label, float Value, const FLinearColor& Color)
			{
				const double Y = 4.0 + Row * 19.0;
				FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(120.0, 18.0), FSlateLayoutTransform(FVector2D(0.0, Y))),
					Label, BodyFont(), ESlateDrawEffect::None, FLinearColor::White);
				const double X0 = 130.0, Width = 260.0;
				FSlateDrawElement::MakeBox(Out, LayerId, Geometry.ToPaintGeometry(FVector2D(Width, 14.0), FSlateLayoutTransform(FVector2D(X0, Y + 2.0))),
					White, ESlateDrawEffect::None, FLinearColor(1, 1, 1, 0.1f));
				const double Mid = X0 + Width * 0.5;
				const double End = X0 + Width * (FMath::Clamp(Value, -1.0f, 1.0f) * 0.5 + 0.5);
				FSlateDrawElement::MakeBox(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(FMath::Abs(End - Mid) + 1.0, 14.0),
					FSlateLayoutTransform(FVector2D(FMath::Min(Mid, End), Y + 2.0))), White, ESlateDrawEffect::None, Color);
				FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(60.0, 18.0), FSlateLayoutTransform(FVector2D(X0 + Width + 8.0, Y))),
					FString::Printf(TEXT("%4d"), FMath::RoundToInt(1500.0f + Value * 500.0f)), BodyFont(), ESlateDrawEffect::None, FLinearColor::White);
			};

			int32 Row = 0;
			if (!Snapshot.bConnected)
			{
				FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(440.0, 18.0), FSlateLayoutTransform(FVector2D(0.0, 4.0))),
					FString(TEXT("No HID radio connected (shows keyboard/gamepad values)")), BodyFont(), ESlateDrawEffect::None, Accent);
				++Row;
			}
			else
			{
				for (int32 Index = 0; Index < Snapshot.Axes.Num() && Index < 10; ++Index)
				{
					const FString Name = FString::Printf(TEXT("Axis %d %s"), Index + 1, Snapshot.AxisNames.IsValidIndex(Index) ? *Snapshot.AxisNames[Index] : TEXT(""));
					Bar(Row++, Name, FFPVInputSystem::CalibrateAxis(Radio, Index, Snapshot.Axes[Index]), FLinearColor(0.6f, 0.6f, 0.6f));
				}
				FString Buttons = TEXT("Buttons ");
				for (int32 Index = 0; Index < Snapshot.Buttons.Num() && Index < 24; ++Index)
				{
					Buttons += Snapshot.Buttons[Index] ? TEXT("#") : TEXT(".");
				}
				FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(440.0, 18.0), FSlateLayoutTransform(FVector2D(0.0, 4.0 + Row * 19.0))),
					Buttons, BodyFont(), ESlateDrawEffect::None, FLinearColor::White);
				++Row;
			}

			Bar(Row++, TEXT("= Roll"), Last.Roll, Accent);
			Bar(Row++, TEXT("= Pitch"), Last.Pitch, Accent);
			Bar(Row++, TEXT("= Yaw"), Last.Yaw, Accent);
			Bar(Row++, TEXT("= Throttle"), Last.Throttle * 2.0f - 1.0f, Accent);
			const FString Switches = FString::Printf(TEXT("ARM %s   MODE %d   3D %s   TURTLE %s"),
				Last.bArmSwitch ? TEXT("ON") : TEXT("off"), static_cast<int32>(Last.Mode), Last.b3D ? TEXT("ON") : TEXT("off"), Last.bTurtle ? TEXT("ON") : TEXT("off"));
			FSlateDrawElement::MakeText(Out, LayerId + 1, Geometry.ToPaintGeometry(FVector2D(440.0, 18.0), FSlateLayoutTransform(FVector2D(0.0, 4.0 + Row * 19.0))),
				Switches, BodyFont(), ESlateDrawEffect::None, FLinearColor::White);
			return LayerId + 2;
		}
	};
}

using namespace FPVMenuLocal;

void SFPVConfigMenu::NotifyChanged()
{
	FFPVSettings::OnChanged.Broadcast();
}

void SFPVConfigMenu::Construct(const FArguments& InArgs)
{
	OnClose = InArgs._OnClose;

	const TArray<FText> TabNames = {
		LOCTEXT("TabRates", "Rates"), LOCTEXT("TabTune", "PID Tuning"), LOCTEXT("TabRadio", "Radio"),
		LOCTEXT("TabCraft", "Craft & Battery"), LOCTEXT("TabCamera", "Camera & Game"), LOCTEXT("TabPresets", "Presets & Import")
	};

	TSharedRef<SHorizontalBox> Tabs = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < TabNames.Num(); ++Index)
	{
		Tabs->AddSlot().AutoWidth().Padding(2.0f)
		[
			SNew(SButton)
			.OnClicked_Lambda([this, Index]() { ActiveTab = Index; return FReply::Handled(); })
			.ButtonColorAndOpacity_Lambda([this, Index]() { return FSlateColor(ActiveTab == Index ? Accent : FLinearColor(0.2f, 0.2f, 0.22f)); })
			[
				SNew(STextBlock).Text(TabNames[Index]).Font(BoldFont())
			]
		];
	}

	ChildSlot
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SBox)
		.WidthOverride(1120.0f)
		.HeightOverride(780.0f)
		[
			SNew(SBorder)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(Panel)
			.Padding(16.0f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("MenuTitle", "FPV DRONE  -  Configuration"))
					.Font(BoldFont(20))
					.ColorAndOpacity(Accent)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
				[
					Tabs
				]
				+ SVerticalBox::Slot().FillHeight(1.0f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SNew(SWidgetSwitcher)
						.WidgetIndex_Lambda([this]() { return ActiveTab; })
						+ SWidgetSwitcher::Slot()[BuildRatesTab()]
						+ SWidgetSwitcher::Slot()[BuildTuneTab()]
						+ SWidgetSwitcher::Slot()[BuildRadioTab()]
						+ SWidgetSwitcher::Slot()[BuildCraftTab()]
						+ SWidgetSwitcher::Slot()[BuildCameraTab()]
						+ SWidgetSwitcher::Slot()[BuildPresetsTab()]
					]
				]
				+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right).Padding(0.0f, 8.0f, 0.0f, 0.0f)
				[
					SNew(SButton)
					.OnClicked_Lambda([this]() { FFPVSettings::Save(); OnClose.ExecuteIfBound(); return FReply::Handled(); })
					[
						SNew(STextBlock).Text(LOCTEXT("SaveClose", "Save & Close (F10)")).Font(BoldFont())
					]
				]
			]
		]
	];
}

FReply SFPVConfigMenu::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape || InKeyEvent.GetKey() == FFPVSettings::ParseKey(FFPVSettings::Get().Keys.OpenMenu))
	{
		FFPVSettings::Save();
		OnClose.ExecuteIfBound();
		return FReply::Handled();
	}
	return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
}

void SFPVConfigMenu::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	FFPVInputSystem& Input = FFPVInputSystem::Get();
	// Keep the live view and calibration running while the menu is open (no drone needed).
	Input.Sample(nullptr);
	if (Input.IsDetecting())
	{
		Input.PollDetection(RadioStatus);
	}
}

// --- Row helpers --------------------------------------------------------------------------

TSharedRef<SWidget> SFPVConfigMenu::MakeHeader(const FText& Text) const
{
	return SNew(SBox).Padding(FMargin(0.0f, 12.0f, 0.0f, 4.0f))
	[
		SNew(STextBlock).Text(Text).Font(BoldFont(15)).ColorAndOpacity(Accent)
	];
}

TSharedRef<SWidget> SFPVConfigMenu::MakeLabel(const FText& Text) const
{
	return SNew(STextBlock).Text(Text).Font(BodyFont()).ColorAndOpacity(FLinearColor::White);
}

TSharedRef<SWidget> SFPVConfigMenu::MakeFloatRow(const FText& Label, float* Value, float Min, float Max, float Delta, const FText& Tooltip)
{
	return SNew(SHorizontalBox)
		.ToolTipText(Tooltip)
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)[MakeLabel(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(2.0f)
		[
			SNew(SSpinBox<float>)
			.MinValue(Min).MaxValue(Max).MinSliderValue(Min).MaxSliderValue(Max).Delta(Delta)
			.Value_Lambda([Value]() { return *Value; })
			.OnValueChanged_Lambda([Value](float NewValue) { *Value = NewValue; NotifyChanged(); })
		];
}

TSharedRef<SWidget> SFPVConfigMenu::MakeIntRow(const FText& Label, int32* Value, int32 Min, int32 Max)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)[MakeLabel(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(2.0f)
		[
			SNew(SSpinBox<int32>)
			.MinValue(Min).MaxValue(Max).MinSliderValue(Min).MaxSliderValue(Max).Delta(1)
			.Value_Lambda([Value]() { return *Value; })
			.OnValueChanged_Lambda([Value](int32 NewValue) { *Value = NewValue; NotifyChanged(); })
		];
}

TSharedRef<SWidget> SFPVConfigMenu::MakeBoolRow(const FText& Label, bool* Value, const FText& Tooltip)
{
	return SNew(SHorizontalBox)
		.ToolTipText(Tooltip)
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)[MakeLabel(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(2.0f)
		[
			SNew(SCheckBox)
			.IsChecked_Lambda([Value]() { return *Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
			.OnCheckStateChanged_Lambda([Value](ECheckBoxState State) { *Value = State == ECheckBoxState::Checked; NotifyChanged(); })
		];
}

TSharedRef<SWidget> SFPVConfigMenu::MakeTextRow(const FText& Label, FString* Value)
{
	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)[MakeLabel(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(2.0f)
		[
			SNew(SEditableTextBox)
			.Text_Lambda([Value]() { return FText::FromString(*Value); })
			.OnTextCommitted_Lambda([Value](const FText& Text, ETextCommit::Type) { *Value = Text.ToString().TrimStartAndEnd(); NotifyChanged(); })
		];
}

TSharedRef<SWidget> SFPVConfigMenu::MakeChoiceRow(const FText& Label, const TArray<FText>& Names, TFunction<int32()> Get, TFunction<void(int32)> Set)
{
	TSharedPtr<TArray<TSharedPtr<int32>>> Items = MakeShared<TArray<TSharedPtr<int32>>>();
	for (int32 Index = 0; Index < Names.Num(); ++Index)
	{
		Items->Add(MakeShared<int32>(Index));
	}
	ChoiceSources.Add(Items);

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)[MakeLabel(Label)]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(2.0f)
		[
			SNew(SComboBox<TSharedPtr<int32>>)
			.OptionsSource(Items.Get())
			.OnGenerateWidget_Lambda([Names](TSharedPtr<int32> Item) { return SNew(STextBlock).Text(Names[*Item]); })
			.OnSelectionChanged_Lambda([Set](TSharedPtr<int32> Item, ESelectInfo::Type) { if (Item.IsValid()) { Set(*Item); NotifyChanged(); } })
			[
				SNew(STextBlock).Text_Lambda([Names, Get]() { const int32 V = Get(); return Names.IsValidIndex(V) ? Names[V] : FText::GetEmpty(); })
			]
		];
}

// --- Tabs -------------------------------------------------------------------------------------

TSharedRef<SWidget> SFPVConfigMenu::BuildRatesTab()
{
	FFPVTune& Tune = FFPVSettings::Get().Tune;
	const TArray<FText> Types = { LOCTEXT("RatesBF", "Betaflight"), LOCTEXT("RatesActual", "Actual"), LOCTEXT("RatesQuick", "Quick"), LOCTEXT("RatesKiss", "KISS") };

	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
	Left->AddSlot().AutoHeight()[MakeChoiceRow(LOCTEXT("RatesType", "Rates type"), Types,
		[&Tune]() { return static_cast<int32>(Tune.RatesType); },
		[&Tune](int32 V) { Tune.RatesType = static_cast<EFPVRatesType>(V); })];

	const FText AxisNames[3] = { LOCTEXT("Roll", "Roll"), LOCTEXT("Pitch", "Pitch"), LOCTEXT("Yaw", "Yaw") };
	FFPVAxisRates* Rates[3] = { &Tune.RollRates, &Tune.PitchRates, &Tune.YawRates };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Left->AddSlot().AutoHeight()[MakeHeader(AxisNames[Axis])];
		// Labels follow the Betaflight Configurator for the selected type.
		Left->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(BodyFont()).Text_Lambda([&Tune]() {
					return Tune.RatesType == EFPVRatesType::Actual ? LOCTEXT("CenterSens", "Center sensitivity (deg/s)") : LOCTEXT("RcRate", "RC Rate");
				})
			]
			+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(2.0f)
			[
				SNew(SSpinBox<float>).MinValue(0.0f).MaxValue(2000.0f).Delta(0.01f)
				.Value_Lambda([Ptr = &Rates[Axis]->A]() { return *Ptr; })
				.OnValueChanged_Lambda([Ptr = &Rates[Axis]->A](float V) { *Ptr = V; NotifyChanged(); })
			]
		];
		Left->AddSlot().AutoHeight()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(BodyFont()).Text_Lambda([&Tune]() {
					return (Tune.RatesType == EFPVRatesType::Actual || Tune.RatesType == EFPVRatesType::Quick)
						? LOCTEXT("MaxRate", "Max rate (deg/s)")
						: (Tune.RatesType == EFPVRatesType::Kiss ? LOCTEXT("KissRate", "Rate") : LOCTEXT("SuperRate", "Super Rate"));
				})
			]
			+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(2.0f)
			[
				SNew(SSpinBox<float>).MinValue(0.0f).MaxValue(2000.0f).Delta(0.01f)
				.Value_Lambda([Ptr = &Rates[Axis]->B]() { return *Ptr; })
				.OnValueChanged_Lambda([Ptr = &Rates[Axis]->B](float V) { *Ptr = V; NotifyChanged(); })
			]
		];
		Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Expo", "Expo"), &Rates[Axis]->C, 0.0f, 1.0f, 0.01f)];
	}

	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("ThrottleHeader", "Throttle"))];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("ThrMid", "Throttle mid"), &Tune.ThrottleMid, 0.05f, 0.95f, 0.01f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("ThrExpo", "Throttle expo"), &Tune.ThrottleExpo, 0.0f, 1.0f, 0.01f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("ThrLimit", "Throttle limit (%)"), &Tune.ThrottleLimitPercent, 10.0f, 100.0f, 1.0f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Deadband", "Stick deadband"), &Tune.StickDeadband, 0.0f, 0.2f, 0.005f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("YawDeadband", "Yaw deadband"), &Tune.YawDeadband, 0.0f, 0.2f, 0.005f)];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.0f)[Left]
		+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(12.0f, 4.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[MakeHeader(LOCTEXT("RateCurve", "Rate curve (stick -> deg/s)"))]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SRateGraph)]
		];
}

TSharedRef<SWidget> SFPVConfigMenu::BuildTuneTab()
{
	FFPVTune& Tune = FFPVSettings::Get().Tune;

	TSharedRef<SGridPanel> Pids = SNew(SGridPanel);
	const FText Columns[4] = { LOCTEXT("P", "P"), LOCTEXT("I", "I"), LOCTEXT("D", "D"), LOCTEXT("FF", "Feedforward") };
	const FText AxisNames[3] = { LOCTEXT("Roll", "Roll"), LOCTEXT("Pitch", "Pitch"), LOCTEXT("Yaw", "Yaw") };
	FFPVPid* Gains[3] = { &Tune.RollPid, &Tune.PitchPid, &Tune.YawPid };
	for (int32 Col = 0; Col < 4; ++Col)
	{
		Pids->AddSlot(Col + 1, 0).Padding(4.0f)[SNew(STextBlock).Text(Columns[Col]).Font(BoldFont())];
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Pids->AddSlot(0, Axis + 1).Padding(4.0f).VAlign(VAlign_Center)[SNew(STextBlock).Text(AxisNames[Axis]).Font(BoldFont()).ColorAndOpacity(AxisColors[Axis])];
		float* Values[4] = { &Gains[Axis]->P, &Gains[Axis]->I, &Gains[Axis]->D, &Gains[Axis]->F };
		for (int32 Col = 0; Col < 4; ++Col)
		{
			Pids->AddSlot(Col + 1, Axis + 1).Padding(4.0f)
			[
				SNew(SBox).WidthOverride(120.0f)
				[
					SNew(SSpinBox<float>).MinValue(0.0f).MaxValue(250.0f).Delta(1.0f)
					.Value_Lambda([Ptr = Values[Col]]() { return *Ptr; })
					.OnValueChanged_Lambda([Ptr = Values[Col]](float V) { *Ptr = FMath::RoundToFloat(V); NotifyChanged(); })
				]
			];
		}
	}

	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("PidHeader", "PID gains (Betaflight scale)"))];
	Left->AddSlot().AutoHeight()[Pids];
	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("MixerHeader", "Mixer"))];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("Airmode", "Airmode"), &Tune.bAirmode, LOCTEXT("AirmodeTip", "Keeps full PID authority at zero throttle (flips, rolls, dives)."))];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Idle", "Motor idle (%)"), &Tune.MotorIdlePercent, 0.0f, 15.0f, 0.1f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("TpaRate", "TPA rate (%)"), &Tune.TpaRatePercent, 0.0f, 100.0f, 1.0f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("TpaBp", "TPA breakpoint"), &Tune.TpaBreakpoint, 1000.0f, 2000.0f, 10.0f)];

	TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
	Right->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("FilterHeader", "Filters"))];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("GyroLpf", "Gyro lowpass (Hz)"), &Tune.GyroLpfHz, 20.0f, 1000.0f, 5.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("DtermLpf", "D-term lowpass (Hz)"), &Tune.DtermLpfHz, 20.0f, 500.0f, 5.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("FFSmooth", "Feedforward smoothing (Hz)"), &Tune.FeedforwardSmoothHz, 5.0f, 100.0f, 1.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("RcSmooth", "RC smoothing (Hz)"), &Tune.RcSmoothingHz, 0.0f, 150.0f, 1.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("ItermRelax", "I-term relax cutoff (Hz)"), &Tune.ItermRelaxCutoffHz, 0.0f, 50.0f, 1.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("ItermLimit", "I-term limit"), &Tune.ItermLimit, 50.0f, 500.0f, 10.0f)];
	Right->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("LevelHeader", "Angle / Horizon"))];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("AngleLimit", "Angle limit (deg)"), &Tune.AngleLimitDeg, 10.0f, 85.0f, 1.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("AngleStrength", "Angle strength"), &Tune.AngleStrength, 0.0f, 200.0f, 1.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("HorizonStrength", "Horizon strength"), &Tune.HorizonStrength, 0.0f, 200.0f, 1.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("HorizonTransition", "Horizon transition"), &Tune.HorizonTransition, 0.1f, 1.0f, 0.01f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Deadband3D", "3D throttle deadband"), &Tune.Deadband3D, 0.0f, 0.3f, 0.01f)];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.55f).Padding(4.0f)[Left]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(12.0f, 4.0f)[Right];
}

void SFPVConfigMenu::RefreshDevices()
{
	FFPVHIDManager::Get().RefreshDevices();
	DeviceOptions.Reset();
	DeviceOptions.Add(MakeShared<FString>(TEXT("")));
	for (const FFPVHIDDeviceInfo& Device : FFPVHIDManager::Get().GetDevices())
	{
		DeviceOptions.Add(MakeShared<FString>(Device.Name));
	}
	if (DeviceCombo.IsValid())
	{
		DeviceCombo->RefreshOptions();
	}
}

TSharedRef<SWidget> SFPVConfigMenu::BuildRadioTab()
{
	FFPVRadioSettings& Radio = FFPVSettings::Get().Radio;
	RefreshDevices();

	const TArray<FText> Sources = { LOCTEXT("SrcAuto", "Auto"), LOCTEXT("SrcHID", "HID radio"), LOCTEXT("SrcPad", "Gamepad"), LOCTEXT("SrcKeys", "Keyboard") };
	const TArray<FText> Modes = { LOCTEXT("ModeAcro", "Acro"), LOCTEXT("ModeAngle", "Angle"), LOCTEXT("ModeHorizon", "Horizon") };

	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("DeviceHeader", "Device"))];
	Left->AddSlot().AutoHeight()[MakeChoiceRow(LOCTEXT("Source", "Input source"), Sources,
		[&Radio]() { return static_cast<int32>(Radio.Source); },
		[&Radio](int32 V) { Radio.Source = static_cast<EFPVInputSource>(V); })];
	Left->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.55f).VAlign(VAlign_Center)[MakeLabel(LOCTEXT("HidDevice", "HID device (empty = auto)"))]
		+ SHorizontalBox::Slot().FillWidth(0.35f).Padding(2.0f)
		[
			SAssignNew(DeviceCombo, SComboBox<TSharedPtr<FString>>)
			.OptionsSource(&DeviceOptions)
			.OnGenerateWidget_Lambda([](TSharedPtr<FString> Item) { return SNew(STextBlock).Text(FText::FromString(Item->IsEmpty() ? TEXT("(auto)") : *Item)); })
			.OnSelectionChanged_Lambda([&Radio](TSharedPtr<FString> Item, ESelectInfo::Type)
			{
				if (Item.IsValid())
				{
					Radio.DeviceName = *Item;
					FFPVHIDManager::Get().SelectDevice(Radio.DeviceName);
					FFPVSettings::Save();
				}
			})
			[
				SNew(STextBlock).Text_Lambda([&Radio]() { return FText::FromString(Radio.DeviceName.IsEmpty() ? TEXT("(auto)") : Radio.DeviceName); })
			]
		]
		+ SHorizontalBox::Slot().FillWidth(0.1f).Padding(2.0f)
		[
			SNew(SButton).OnClicked_Lambda([this]() { RefreshDevices(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(LOCTEXT("Refresh", "Scan"))
			]
		]
	];
	if (!FFPVHIDManager::Get().IsSupported())
	{
		Left->AddSlot().AutoHeight()[MakeLabel(LOCTEXT("NoHid", "HID radios are only supported on Windows."))];
	}

	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("CalHeader", "Calibration"))];
	Left->AddSlot().AutoHeight()
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
		[
			SNew(SButton)
			.OnClicked_Lambda([this]()
			{
				FFPVInputSystem& Input = FFPVInputSystem::Get();
				if (Input.IsCalibrating())
				{
					Input.FinishCalibration();
					RadioStatus = LOCTEXT("CalDone", "Calibration saved.");
				}
				else
				{
					Input.BeginCalibration();
					RadioStatus = LOCTEXT("CalRunning", "Move all sticks and switches to their ends, centre the sticks (throttle down), then press Finish.");
				}
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text_Lambda([]() { return FFPVInputSystem::Get().IsCalibrating() ? LOCTEXT("CalFinish", "Finish calibration") : LOCTEXT("CalStart", "Start calibration"); })
			]
		]
		+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
		[
			SNew(SButton)
			.OnClicked_Lambda([this, &Radio]() { Radio.Calibration.Reset(); FFPVInputSystem::Get().CancelCalibration(); RadioStatus = LOCTEXT("CalReset", "Calibration reset."); NotifyChanged(); return FReply::Handled(); })
			[
				SNew(STextBlock).Text(LOCTEXT("CalClear", "Reset"))
			]
		]
	];

	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("MapHeader", "Channel mapping"))];
	const FText ChannelNames[FPVChannelCount] = {
		LOCTEXT("ChRoll", "Roll"), LOCTEXT("ChPitch", "Pitch"), LOCTEXT("ChYaw", "Yaw"), LOCTEXT("ChThrottle", "Throttle"),
		LOCTEXT("ChArm", "Arm switch"), LOCTEXT("ChMode", "Mode switch"), LOCTEXT("Ch3D", "3D switch"), LOCTEXT("ChTurtle", "Turtle switch")
	};
	for (int32 Channel = 0; Channel < FPVChannelCount; ++Channel)
	{
		FFPVChannelMapping* Map = &Radio.Channels[Channel];
		Left->AddSlot().AutoHeight().Padding(0.0f, 1.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(0.25f).VAlign(VAlign_Center)[MakeLabel(ChannelNames[Channel])]
			+ SHorizontalBox::Slot().FillWidth(0.2f).VAlign(VAlign_Center)
			[
				SNew(STextBlock).Font(BodyFont()).Text_Lambda([Map]()
				{
					if (Map->Axis >= 0) { return FText::Format(LOCTEXT("AxisN", "Axis {0}"), Map->Axis + 1); }
					if (Map->Button >= 0) { return FText::Format(LOCTEXT("ButtonN", "Button {0}"), Map->Button + 1); }
					return LOCTEXT("Unmapped", "-");
				})
			]
			+ SHorizontalBox::Slot().FillWidth(0.15f).Padding(2.0f)
			[
				SNew(SSpinBox<int32>).MinValue(0).MaxValue(16).Delta(1)
				.ToolTipText(LOCTEXT("AxisTip", "Axis number, 0 = none"))
				.Value_Lambda([Map]() { return Map->Axis + 1; })
				.OnValueChanged_Lambda([Map](int32 V) { Map->Axis = V - 1; if (V > 0) { Map->Button = -1; } NotifyChanged(); })
			]
			+ SHorizontalBox::Slot().FillWidth(0.15f).Padding(2.0f).VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([Map]() { return Map->bInvert ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
				.OnCheckStateChanged_Lambda([Map](ECheckBoxState S) { Map->bInvert = S == ECheckBoxState::Checked; NotifyChanged(); })
				[
					SNew(STextBlock).Text(LOCTEXT("Invert", "Invert"))
				]
			]
			+ SHorizontalBox::Slot().FillWidth(0.25f).Padding(2.0f)
			[
				SNew(SButton)
				.OnClicked_Lambda([Channel]() { FFPVInputSystem::Get().BeginDetection(static_cast<EFPVChannel>(Channel)); return FReply::Handled(); })
				[
					SNew(STextBlock).Text(LOCTEXT("Detect", "Detect"))
				]
			]
		];
	}

	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("SwitchHeader", "Switches"))];
	Left->AddSlot().AutoHeight()[MakeChoiceRow(LOCTEXT("ModeLow", "Mode switch low"), Modes, [&Radio]() { return static_cast<int32>(Radio.ModeLow); }, [&Radio](int32 V) { Radio.ModeLow = static_cast<EFPVFlightMode>(V); })];
	Left->AddSlot().AutoHeight()[MakeChoiceRow(LOCTEXT("ModeMid", "Mode switch middle"), Modes, [&Radio]() { return static_cast<int32>(Radio.ModeMid); }, [&Radio](int32 V) { Radio.ModeMid = static_cast<EFPVFlightMode>(V); })];
	Left->AddSlot().AutoHeight()[MakeChoiceRow(LOCTEXT("ModeHigh", "Mode switch high"), Modes, [&Radio]() { return static_cast<int32>(Radio.ModeHigh); }, [&Radio](int32 V) { Radio.ModeHigh = static_cast<EFPVFlightMode>(V); })];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("SwitchThreshold", "Switch on threshold"), &Radio.SwitchThreshold, -0.9f, 0.9f, 0.05f)];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("ArmLowThr", "Arming needs low throttle"), &Radio.bArmRequiresLowThrottle)];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("PadThrottle", "Gamepad: full stick travel = throttle"), &Radio.bGamepadFullRangeThrottle)];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.55f).Padding(4.0f)[Left]
		+ SHorizontalBox::Slot().FillWidth(0.45f).Padding(12.0f, 4.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[MakeHeader(LOCTEXT("LiveHeader", "Live input"))]
			+ SVerticalBox::Slot().AutoHeight()[SNew(SChannelView)]
			+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 8.0f)
			[
				SNew(STextBlock).AutoWrapText(true).Font(BodyFont()).ColorAndOpacity(Accent).Text_Lambda([this]() { return RadioStatus; })
			]
			+ SVerticalBox::Slot().AutoHeight()
			[
				SNew(STextBlock).AutoWrapText(true).Font(BodyFont())
				.Text(LOCTEXT("RadioHelp", "Radiomaster Pocket / EdgeTX: plug in USB, choose \"USB Joystick (HID)\". Default mapping is AETR plus AUX1 = arm, AUX2 = mode, AUX3 = 3D, AUX4 = turtle. Use Detect if your model uses another order, then calibrate."))
			]
		];
}

TSharedRef<SWidget> SFPVConfigMenu::BuildCraftTab()
{
	FFPVCraft& Craft = FFPVSettings::Get().Craft;
	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("FrameHeader", "Frame & motors"))];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Mass", "All-up weight (kg)"), &Craft.MassKg, 0.02f, 3.0f, 0.01f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Arm", "Motor distance from centre (m)"), &Craft.ArmLengthM, 0.02f, 0.4f, 0.005f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Thrust", "Max thrust per motor (N)"), &Craft.MotorMaxThrustN, 0.05f, 60.0f, 0.1f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("MotorResp", "Motor response (ms)"), &Craft.MotorResponseMs, 5.0f, 100.0f, 1.0f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Prop", "Prop size (inch)"), &Craft.PropDiameterInch, 1.0f, 10.0f, 0.1f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Drag", "Drag factor"), &Craft.DragScale, 0.2f, 4.0f, 0.05f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Propwash", "Propwash strength"), &Craft.PropwashStrength, 0.0f, 3.0f, 0.05f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("CrashSpeed", "Crash disarm speed (m/s)"), &Craft.CrashDisarmSpeed, 2.0f, 50.0f, 0.5f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Bounce", "Bounce"), &Craft.BounceRestitution, 0.0f, 0.9f, 0.05f)];

	TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
	Right->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("BatteryHeader", "Battery"))];
	Right->AddSlot().AutoHeight()[MakeIntRow(LOCTEXT("Cells", "Cells (S)"), &Craft.BatteryCells, 1, 8)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Capacity", "Capacity (mAh)"), &Craft.BatteryCapacityMah, 100.0f, 10000.0f, 50.0f)];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("CapMult", "Flight time multiplier"), &Craft.BatteryCapacityMultiplier, 0.25f, 20.0f, 0.25f,
		LOCTEXT("CapMultTip", "1 = realistic. Higher values give longer flights without changing the voltage behaviour."))];
	Right->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("IR", "Internal resistance (mOhm per cell)"), &Craft.CellResistanceMilliOhm, 0.5f, 30.0f, 0.5f)];
	Right->AddSlot().AutoHeight()
	[
		SNew(STextBlock).AutoWrapText(true).Font(BodyFont())
		.Text(LOCTEXT("BatteryHelp", "Battery and craft changes apply immediately. The battery is recharged when you pick the drone up and launch it again."))
	];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.0f)[Left]
		+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(12.0f, 4.0f)[Right];
}

TSharedRef<SWidget> SFPVConfigMenu::BuildCameraTab()
{
	FFPVConfig& Config = FFPVSettings::Get();
	const TArray<FText> CameraModes = { LOCTEXT("CamFPV", "FPV"), LOCTEXT("CamChase", "Chase (third person)") };

	TSharedRef<SVerticalBox> Left = SNew(SVerticalBox);
	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("CameraHeader", "Camera"))];
	Left->AddSlot().AutoHeight()[MakeChoiceRow(LOCTEXT("CamMode", "Camera mode"), CameraModes,
		[&Config]() { return static_cast<int32>(Config.Camera.Mode); },
		[&Config](int32 V) { Config.Camera.Mode = static_cast<EFPVCameraMode>(V); })];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Uptilt", "Camera uptilt (deg)"), &Config.Camera.UptiltDeg, -20.0f, 80.0f, 1.0f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("FOV", "Field of view (deg)"), &Config.Camera.FieldOfView, 60.0f, 170.0f, 1.0f)];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("ChaseDist", "Chase distance (cm)"), &Config.Camera.ChaseDistanceCm, 50.0f, 1000.0f, 10.0f)];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("ShowOSD", "Show OSD"), &Config.Camera.bShowOSD)];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("Metric", "Metric units"), &Config.Camera.bOSDMetric)];

	Left->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("GameHeader", "Link & gameplay"))];
	Left->AddSlot().AutoHeight()[MakeFloatRow(LOCTEXT("Range", "Max range (m, 0 = unlimited)"), &Config.Gameplay.MaxRangeMeters, 0.0f, 10000.0f, 50.0f)];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("Obstacles", "Walls weaken the signal"), &Config.Gameplay.bObstaclesReduceSignal)];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("Failsafe", "Failsafe on signal loss"), &Config.Gameplay.bFailsafe)];
	Left->AddSlot().AutoHeight()[MakeBoolRow(LOCTEXT("Always3D", "Always 3D mode"), &Config.Gameplay.bAlways3D)];

	TSharedRef<SVerticalBox> Right = SNew(SVerticalBox);
	Right->AddSlot().AutoHeight()[MakeHeader(LOCTEXT("KeysHeader", "Keys (Unreal key names, e.g. K, F10, Gamepad_Special_Left)"))];
	FFPVKeyBindings& Keys = Config.Keys;
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("KeyGoggles", "Launch / goggles on-off"), &Keys.ToggleGoggles)];
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("KeyPickUp", "Pick up drone"), &Keys.PickUpDrone)];
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("KeyMenu", "This menu"), &Keys.OpenMenu)];
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("KeyCamera", "Toggle camera mode"), &Keys.ToggleCamera)];
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("KeyArm", "Keyboard: arm"), &Keys.KeyboardArm)];
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("KeyMode", "Keyboard: flight mode"), &Keys.KeyboardMode)];
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("Key3D", "Keyboard: 3D"), &Keys.Keyboard3D)];
	Right->AddSlot().AutoHeight()[MakeTextRow(LOCTEXT("KeyTurtle", "Keyboard: turtle (hold)"), &Keys.KeyboardTurtle)];
	Right->AddSlot().AutoHeight()
	[
		SNew(STextBlock).AutoWrapText(true).Font(BodyFont())
		.Text(LOCTEXT("KeyboardHelp", "Keyboard flying: W/S pitch, A/D roll, Q/E yaw, Shift/Space throttle up, Ctrl/C down, X cut. Gamepad (mode 2): A arm, B mode, Y 3D, LB turtle."))
	];

	return SNew(SHorizontalBox)
		+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(4.0f)[Left]
		+ SHorizontalBox::Slot().FillWidth(0.5f).Padding(12.0f, 4.0f)[Right];
}

TSharedRef<SWidget> SFPVConfigMenu::BuildPresetsTab()
{
	TSharedRef<SUniformGridPanel> Presets = SNew(SUniformGridPanel).SlotPadding(4.0f);
	for (int32 Index = 0; Index < static_cast<int32>(EFPVPreset::Count); ++Index)
	{
		const EFPVPreset Preset = static_cast<EFPVPreset>(Index);
		Presets->AddSlot(Index % 3, Index / 3)
		[
			SNew(SButton)
			.OnClicked_Lambda([this, Preset]()
			{
				FFPVSettings::ApplyPreset(FFPVSettings::Get(), Preset);
				FFPVSettings::Save();
				ImportReport = FText::Format(LOCTEXT("PresetApplied", "Preset applied: {0}"), FFPVSettings::GetPresetName(Preset));
				return FReply::Handled();
			})
			[
				SNew(STextBlock).Text(FFPVSettings::GetPresetName(Preset)).Font(BoldFont())
			]
		];
	}

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight()[MakeHeader(LOCTEXT("PresetsHeader", "Craft presets (replace rates, PIDs and craft)"))]
		+ SVerticalBox::Slot().AutoHeight()[Presets]
		+ SVerticalBox::Slot().AutoHeight()[MakeHeader(LOCTEXT("ImportHeader", "Betaflight CLI"))]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).AutoWrapText(true).Font(BodyFont())
			.Text(LOCTEXT("ImportHelp", "Copy the output of \"diff all\" from the Betaflight CLI (or just some \"set ...\" lines) and press Import. Rates, PIDs, feedforward, TPA, filters, throttle, angle/horizon and 3D settings are taken over."))
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.OnClicked_Lambda([this]()
				{
					FString Clipboard;
					FPlatformApplicationMisc::ClipboardPaste(Clipboard);
					FString Report;
					const int32 Count = FFPVSettings::ImportBetaflightCli(FFPVSettings::Get(), Clipboard, Report);
					if (Count > 0)
					{
						FFPVSettings::Save();
					}
					ImportReport = FText::FromString(Count > 0 ? Report : TEXT("Clipboard contains no Betaflight \"set\" lines."));
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("Import", "Import from clipboard"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.OnClicked_Lambda([this]()
				{
					FPlatformApplicationMisc::ClipboardCopy(*FFPVSettings::ExportBetaflightCli(FFPVSettings::Get()));
					ImportReport = LOCTEXT("Exported", "Tune copied to the clipboard as Betaflight CLI.");
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("Export", "Export to clipboard"))
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().Padding(2.0f)
			[
				SNew(SButton)
				.OnClicked_Lambda([this]()
				{
					// Assign member-wise: the menu holds pointers into the channel array.
					FFPVConfig& Config = FFPVSettings::Get();
					const FFPVConfig Defaults;
					TArray<FFPVChannelMapping> Channels = MoveTemp(Config.Radio.Channels);
					for (int32 Index = 0; Index < Channels.Num() && Index < Defaults.Radio.Channels.Num(); ++Index)
					{
						Channels[Index] = Defaults.Radio.Channels[Index];
					}
					Config = Defaults;
					Config.Radio.Channels = MoveTemp(Channels);
					FFPVSettings::Save();
					ImportReport = LOCTEXT("ResetDone", "All settings reset to defaults.");
					return FReply::Handled();
				})
				[
					SNew(STextBlock).Text(LOCTEXT("ResetAll", "Reset everything"))
				]
			]
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.0f, 6.0f)
		[
			SNew(STextBlock).AutoWrapText(true).Font(BodyFont()).ColorAndOpacity(Accent).Text_Lambda([this]() { return ImportReport; })
		]
		+ SVerticalBox::Slot().AutoHeight()
		[
			SNew(STextBlock).AutoWrapText(true).Font(BodyFont())
			.Text(FText::Format(LOCTEXT("FileHint", "Settings file: {0}"), FText::FromString(FFPVSettings::GetFilePath())))
		];
}

#undef LOCTEXT_NAMESPACE
