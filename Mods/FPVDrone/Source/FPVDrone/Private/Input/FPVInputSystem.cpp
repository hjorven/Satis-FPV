#include "Input/FPVInputSystem.h"

#include "FPVSettings.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

FFPVInputSystem& FFPVInputSystem::Get()
{
	static FFPVInputSystem Instance;
	return Instance;
}

float FFPVInputSystem::CalibrateAxis(const FFPVRadioSettings& Radio, int32 AxisIndex, float Raw)
{
	if (!Radio.Calibration.IsValidIndex(AxisIndex))
	{
		return FMath::Clamp(Raw, -1.0f, 1.0f);
	}
	const FFPVAxisCalibration& Cal = Radio.Calibration[AxisIndex];
	if (Raw >= Cal.Center)
	{
		const float Span = FMath::Max(1e-3f, Cal.Max - Cal.Center);
		return FMath::Clamp((Raw - Cal.Center) / Span, -1.0f, 1.0f);
	}
	const float Span = FMath::Max(1e-3f, Cal.Center - Cal.Min);
	return FMath::Clamp((Raw - Cal.Center) / Span, -1.0f, 1.0f);
}

float FFPVInputSystem::ReadChannel(const FFPVRadioSettings& Radio, const FFPVHIDSnapshot& Snapshot, EFPVChannel Channel)
{
	const int32 Index = static_cast<int32>(Channel);
	if (!Radio.Channels.IsValidIndex(Index))
	{
		return -1.0f;
	}
	const FFPVChannelMapping& Map = Radio.Channels[Index];
	float Value = -1.0f;
	if (Snapshot.Axes.IsValidIndex(Map.Axis))
	{
		Value = CalibrateAxis(Radio, Map.Axis, Snapshot.Axes[Map.Axis]);
	}
	else if (Snapshot.Buttons.IsValidIndex(Map.Button))
	{
		Value = Snapshot.Buttons[Map.Button] ? 1.0f : -1.0f;
	}
	else
	{
		// Unmapped: sticks centred, switches off, throttle low.
		return (Channel == EFPVChannel::Roll || Channel == EFPVChannel::Pitch || Channel == EFPVChannel::Yaw) ? 0.0f : -1.0f;
	}
	return Map.bInvert ? -Value : Value;
}

EFPVFlightMode FFPVInputSystem::ModeFromValue(const FFPVRadioSettings& Radio, float Value) const
{
	if (Value < -0.33f)
	{
		return Radio.ModeLow;
	}
	return Value > 0.33f ? Radio.ModeHigh : Radio.ModeMid;
}

void FFPVInputSystem::SampleHID(const FFPVRadioSettings& Radio, FFPVPilotInput& Out)
{
	FFPVHIDSnapshot Snapshot;
	if (!FFPVHIDManager::Get().GetSnapshot(Snapshot))
	{
		return;
	}
	UpdateCalibration(Snapshot);

	Out.Roll = ReadChannel(Radio, Snapshot, EFPVChannel::Roll);
	Out.Pitch = ReadChannel(Radio, Snapshot, EFPVChannel::Pitch);
	Out.Yaw = ReadChannel(Radio, Snapshot, EFPVChannel::Yaw);
	Out.Throttle = (ReadChannel(Radio, Snapshot, EFPVChannel::Throttle) + 1.0f) * 0.5f;
	Out.bArmSwitch = ReadChannel(Radio, Snapshot, EFPVChannel::Arm) > Radio.SwitchThreshold;
	Out.Mode = ModeFromValue(Radio, ReadChannel(Radio, Snapshot, EFPVChannel::FlightMode));
	Out.b3D = ReadChannel(Radio, Snapshot, EFPVChannel::Mode3D) > Radio.SwitchThreshold;
	Out.bTurtle = ReadChannel(Radio, Snapshot, EFPVChannel::Turtle) > Radio.SwitchThreshold;
	Out.bValid = true;
	Out.SourceName = Snapshot.DeviceName;
}

void FFPVInputSystem::SampleGamepadAndKeyboard(APlayerController* PC, const FFPVConfig& Config, FFPVPilotInput& Out, bool bUseGamepad, bool bUseKeyboard)
{
	const double Now = FPlatformTime::Seconds();
	const float Dt = static_cast<float>(FMath::Clamp(Now - LastSampleTime, 0.0, 0.1));
	const FFPVKeyBindings& Keys = Config.Keys;

	auto Down = [PC](const FKey& Key) { return PC->IsInputKeyDown(Key); };
	auto Pressed = [PC](const FKey& Key) { return PC->WasInputKeyJustPressed(Key); };
	auto Analog = [PC](const FKey& Key) { return PC->GetInputAnalogKeyState(Key); };

	float Roll = 0.0f, Pitch = 0.0f, Yaw = 0.0f;
	bool bGamepadActive = false;

	if (bUseGamepad)
	{
		// Mode 2: left stick throttle/yaw, right stick pitch/roll.
		const float LX = Analog(EKeys::Gamepad_LeftX);
		const float LY = Analog(EKeys::Gamepad_LeftY);
		const float RX = Analog(EKeys::Gamepad_RightX);
		const float RY = Analog(EKeys::Gamepad_RightY);
		if (FMath::Abs(LX) + FMath::Abs(LY) + FMath::Abs(RX) + FMath::Abs(RY) > 0.15f)
		{
			LastGamepadActivity = Now;
		}
		bGamepadActive = Now - LastGamepadActivity < 30.0;
		if (bGamepadActive)
		{
			Roll += RX;
			Pitch += RY;
			Yaw += LX;
			KeyboardThrottle = Config.Radio.bGamepadFullRangeThrottle ? (LY + 1.0f) * 0.5f : FMath::Max(0.0f, LY);
		}
		if (Pressed(EKeys::Gamepad_FaceButton_Bottom)) { bLatchedArm = !bLatchedArm; }
		if (Pressed(EKeys::Gamepad_FaceButton_Right)) { LatchedMode = (LatchedMode + 1) % 3; }
		if (Pressed(EKeys::Gamepad_FaceButton_Top)) { bLatched3D = !bLatched3D; }
		Out.bTurtle |= Down(EKeys::Gamepad_LeftShoulder);
	}

	if (bUseKeyboard)
	{
		Roll += (Down(EKeys::D) ? 1.0f : 0.0f) - (Down(EKeys::A) ? 1.0f : 0.0f);
		Pitch += (Down(EKeys::W) ? 1.0f : 0.0f) - (Down(EKeys::S) ? 1.0f : 0.0f);
		Yaw += (Down(EKeys::E) ? 1.0f : 0.0f) - (Down(EKeys::Q) ? 1.0f : 0.0f);
		Roll += (Down(EKeys::Right) ? 1.0f : 0.0f) - (Down(EKeys::Left) ? 1.0f : 0.0f);
		Pitch += (Down(EKeys::Up) ? 1.0f : 0.0f) - (Down(EKeys::Down) ? 1.0f : 0.0f);
		if (!bGamepadActive)
		{
			const float ThrottleRate = 0.8f; // full range in ~1.2 s
			if (Down(EKeys::LeftShift) || Down(EKeys::SpaceBar)) { KeyboardThrottle += ThrottleRate * Dt; }
			if (Down(EKeys::LeftControl) || Down(EKeys::C)) { KeyboardThrottle -= ThrottleRate * Dt; }
			if (Down(EKeys::X)) { KeyboardThrottle = 0.0f; }
		}
		if (Pressed(FFPVSettings::ParseKey(Keys.KeyboardArm))) { bLatchedArm = !bLatchedArm; }
		if (Pressed(FFPVSettings::ParseKey(Keys.KeyboardMode))) { LatchedMode = (LatchedMode + 1) % 3; }
		if (Pressed(FFPVSettings::ParseKey(Keys.Keyboard3D))) { bLatched3D = !bLatched3D; }
		Out.bTurtle |= Down(FFPVSettings::ParseKey(Keys.KeyboardTurtle));
	}

	KeyboardThrottle = FMath::Clamp(KeyboardThrottle, 0.0f, 1.0f);
	Out.Roll = FMath::Clamp(Roll, -1.0f, 1.0f);
	Out.Pitch = FMath::Clamp(Pitch, -1.0f, 1.0f);
	Out.Yaw = FMath::Clamp(Yaw, -1.0f, 1.0f);
	Out.Throttle = KeyboardThrottle;
	Out.bArmSwitch = bLatchedArm;
	Out.Mode = static_cast<EFPVFlightMode>(LatchedMode);
	Out.b3D = bLatched3D;
	Out.bValid = true;
	Out.SourceName = bGamepadActive ? TEXT("Gamepad") : TEXT("Keyboard");
}

FFPVPilotInput FFPVInputSystem::Sample(APlayerController* PC)
{
	const FFPVConfig& Config = FFPVSettings::Get();
	FFPVHIDManager::Get().Tick();

	FFPVPilotInput Out;
	const EFPVInputSource Source = Config.Radio.Source;

	if (Source == EFPVInputSource::HID || Source == EFPVInputSource::Auto)
	{
		SampleHID(Config.Radio, Out);
	}
	if (!Out.bValid && PC != nullptr && Source != EFPVInputSource::HID)
	{
		SampleGamepadAndKeyboard(PC, Config, Out, Source != EFPVInputSource::Keyboard, Source != EFPVInputSource::Gamepad);
	}

	LastSampleTime = FPlatformTime::Seconds();
	Last = Out;
	return Out;
}

void FFPVInputSystem::BeginCalibration()
{
	bCalibrating = true;
	CalMin.Reset();
	CalMax.Reset();
}

void FFPVInputSystem::UpdateCalibration(const FFPVHIDSnapshot& Snapshot)
{
	if (!bCalibrating)
	{
		return;
	}
	if (CalMin.Num() != Snapshot.Axes.Num())
	{
		CalMin = Snapshot.Axes;
		CalMax = Snapshot.Axes;
	}
	for (int32 Index = 0; Index < Snapshot.Axes.Num(); ++Index)
	{
		CalMin[Index] = FMath::Min(CalMin[Index], Snapshot.Axes[Index]);
		CalMax[Index] = FMath::Max(CalMax[Index], Snapshot.Axes[Index]);
	}
}

void FFPVInputSystem::FinishCalibration()
{
	FFPVHIDSnapshot Snapshot;
	if (!bCalibrating || !FFPVHIDManager::Get().GetSnapshot(Snapshot))
	{
		bCalibrating = false;
		return;
	}
	UpdateCalibration(Snapshot);

	FFPVRadioSettings& Radio = FFPVSettings::Get().Radio;
	Radio.Calibration.SetNum(Snapshot.Axes.Num());
	const int32 ThrottleAxis = Radio.Channels[static_cast<int32>(EFPVChannel::Throttle)].Axis;
	for (int32 Index = 0; Index < Snapshot.Axes.Num(); ++Index)
	{
		FFPVAxisCalibration& Cal = Radio.Calibration[Index];
		if (CalMax[Index] - CalMin[Index] < 0.2f)
		{
			// Axis not moved: keep the full range.
			Cal = FFPVAxisCalibration();
			continue;
		}
		Cal.Min = CalMin[Index];
		Cal.Max = CalMax[Index];
		// Sticks are centred when finishing; throttle and switches use the middle of the range.
		const bool bCentredStick = Index != ThrottleAxis && FMath::Abs(Snapshot.Axes[Index] - (Cal.Min + Cal.Max) * 0.5f) < 0.25f * (Cal.Max - Cal.Min);
		Cal.Center = bCentredStick ? Snapshot.Axes[Index] : (Cal.Min + Cal.Max) * 0.5f;
	}
	bCalibrating = false;
	FFPVSettings::Save();
}

void FFPVInputSystem::CancelCalibration()
{
	bCalibrating = false;
}

void FFPVInputSystem::BeginDetection(EFPVChannel Channel)
{
	FFPVHIDSnapshot Snapshot;
	FFPVHIDManager::Get().GetSnapshot(Snapshot);
	DetectChannel = Channel;
	DetectBaselineAxes = Snapshot.Axes;
	DetectBaselineButtons = Snapshot.Buttons;
}

void FFPVInputSystem::CancelDetection()
{
	DetectChannel.Reset();
}

bool FFPVInputSystem::PollDetection(FText& OutStatus)
{
	if (!DetectChannel.IsSet())
	{
		return false;
	}
	FFPVHIDSnapshot Snapshot;
	if (!FFPVHIDManager::Get().GetSnapshot(Snapshot))
	{
		OutStatus = LOCTEXT("DetectNoDevice", "No radio connected.");
		return false;
	}
	if (DetectBaselineAxes.Num() != Snapshot.Axes.Num())
	{
		DetectBaselineAxes = Snapshot.Axes;
		DetectBaselineButtons = Snapshot.Buttons;
	}

	int32 BestAxis = INDEX_NONE;
	float BestDelta = 0.0f;
	for (int32 Index = 0; Index < Snapshot.Axes.Num(); ++Index)
	{
		const float Delta = Snapshot.Axes[Index] - DetectBaselineAxes[Index];
		if (FMath::Abs(Delta) > FMath::Abs(BestDelta))
		{
			BestDelta = Delta;
			BestAxis = Index;
		}
	}

	FFPVRadioSettings& Radio = FFPVSettings::Get().Radio;
	FFPVChannelMapping& Map = Radio.Channels[static_cast<int32>(DetectChannel.GetValue())];

	if (BestAxis != INDEX_NONE && FMath::Abs(BestDelta) > 0.5f)
	{
		Map.Axis = BestAxis;
		Map.Button = INDEX_NONE;
		Map.bInvert = BestDelta < 0.0f;
		OutStatus = FText::Format(LOCTEXT("DetectAxis", "Assigned axis {0} ({1}){2}."), BestAxis + 1,
			FText::FromString(Snapshot.AxisNames.IsValidIndex(BestAxis) ? Snapshot.AxisNames[BestAxis] : FString()),
			Map.bInvert ? LOCTEXT("Inverted", ", inverted") : FText::GetEmpty());
		DetectChannel.Reset();
		FFPVSettings::Save();
		return true;
	}

	for (int32 Index = 0; Index < Snapshot.Buttons.Num() && Index < DetectBaselineButtons.Num(); ++Index)
	{
		if (Snapshot.Buttons[Index] != DetectBaselineButtons[Index])
		{
			Map.Axis = INDEX_NONE;
			Map.Button = Index;
			Map.bInvert = !Snapshot.Buttons[Index];
			OutStatus = FText::Format(LOCTEXT("DetectButton", "Assigned button {0}."), Index + 1);
			DetectChannel.Reset();
			FFPVSettings::Save();
			return true;
		}
	}

	OutStatus = LOCTEXT("DetectWaiting", "Move the stick or flip the switch to its positive end (right / forward / up / on)...");
	return false;
}

#undef LOCTEXT_NAMESPACE
