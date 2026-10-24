// Turns raw HID / gamepad / keyboard state into calibrated pilot commands.
// Also hosts the calibration and "move the stick" detection wizards used by the menu.

#pragma once

#include "CoreMinimal.h"
#include "FPVTypes.h"
#include "Input/FPVHIDDevice.h"

class APlayerController;

class FPVDRONE_API FFPVInputSystem
{
public:
	static FFPVInputSystem& Get();

	// Reads the configured source. Call once per frame on the game thread.
	FFPVPilotInput Sample(APlayerController* PC);

	// Last sampled values, for the menu's live view.
	const FFPVPilotInput& GetLast() const { return Last; }

	// Calibrated value (-1..1) of a logical channel from a HID snapshot.
	static float ReadChannel(const FFPVRadioSettings& Radio, const FFPVHIDSnapshot& Snapshot, EFPVChannel Channel);
	static float CalibrateAxis(const FFPVRadioSettings& Radio, int32 AxisIndex, float Raw);

	// --- Calibration wizard -------------------------------------------------
	// Start, move all sticks and switches to their ends, then Finish (with sticks centred).
	void BeginCalibration();
	void FinishCalibration();
	void CancelCalibration();
	bool IsCalibrating() const { return bCalibrating; }

	// --- Channel detection ----------------------------------------------------
	// Start, then the user moves the stick in the positive direction (right / forward /
	// throttle up / switch on). PollDetection() assigns the mapping once movement is seen.
	void BeginDetection(EFPVChannel Channel);
	bool PollDetection(FText& OutStatus);
	void CancelDetection();
	bool IsDetecting() const { return DetectChannel.IsSet(); }

private:
	FFPVInputSystem() = default;

	void SampleHID(const FFPVRadioSettings& Radio, FFPVPilotInput& Out);
	void SampleGamepadAndKeyboard(APlayerController* PC, const FFPVConfig& Config, FFPVPilotInput& Out, bool bUseGamepad, bool bUseKeyboard);
	void UpdateCalibration(const FFPVHIDSnapshot& Snapshot);
	EFPVFlightMode ModeFromValue(const FFPVRadioSettings& Radio, float Value) const;

	FFPVPilotInput Last;

	// Keyboard/gamepad latched switches
	bool bLatchedArm = false;
	bool bLatched3D = false;
	int32 LatchedMode = 0;
	float KeyboardThrottle = 0.0f;
	double LastGamepadActivity = -100.0;
	double LastSampleTime = 0.0;

	bool bCalibrating = false;
	TArray<float> CalMin;
	TArray<float> CalMax;

	TOptional<EFPVChannel> DetectChannel;
	TArray<float> DetectBaselineAxes;
	TArray<bool> DetectBaselineButtons;
};
