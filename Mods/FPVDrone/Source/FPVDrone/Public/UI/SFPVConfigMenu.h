// In-game configuration menu (F10): rates, tune, radio, craft, camera, presets and Betaflight import.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SComboBox.h"

class FPVDRONE_API SFPVConfigMenu : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFPVConfigMenu) {}
		SLATE_EVENT(FSimpleDelegate, OnClose)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;

private:
	TSharedRef<SWidget> BuildRatesTab();
	TSharedRef<SWidget> BuildTuneTab();
	TSharedRef<SWidget> BuildRadioTab();
	TSharedRef<SWidget> BuildCraftTab();
	TSharedRef<SWidget> BuildCameraTab();
	TSharedRef<SWidget> BuildPresetsTab();

	TSharedRef<SWidget> MakeHeader(const FText& Text) const;
	TSharedRef<SWidget> MakeFloatRow(const FText& Label, float* Value, float Min, float Max, float Delta, const FText& Tooltip = FText::GetEmpty());
	TSharedRef<SWidget> MakeIntRow(const FText& Label, int32* Value, int32 Min, int32 Max);
	TSharedRef<SWidget> MakeBoolRow(const FText& Label, bool* Value, const FText& Tooltip = FText::GetEmpty());
	TSharedRef<SWidget> MakeTextRow(const FText& Label, FString* Value);
	TSharedRef<SWidget> MakeChoiceRow(const FText& Label, const TArray<FText>& Names, TFunction<int32()> Get, TFunction<void(int32)> Set);
	TSharedRef<SWidget> MakeLabel(const FText& Text) const;

	void RefreshDevices();
	static void NotifyChanged();

	FSimpleDelegate OnClose;
	int32 ActiveTab = 0;

	TArray<TSharedPtr<TArray<TSharedPtr<int32>>>> ChoiceSources;
	TArray<TSharedPtr<FString>> DeviceOptions;
	TSharedPtr<SComboBox<TSharedPtr<FString>>> DeviceCombo;

	FText RadioStatus;
	FText ImportReport;
};
