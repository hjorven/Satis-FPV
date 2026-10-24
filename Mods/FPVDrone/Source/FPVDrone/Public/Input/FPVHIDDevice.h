// Raw HID joystick reader for RC transmitters (Radiomaster, Jumper, TBS, FrSky ... in USB
// joystick mode) and other DirectInput-only devices that Unreal does not see on its own.
//
// Windows only. A background thread reads input reports so the stick values are never
// limited by the game's frame rate. Other platforms get an empty implementation.

#pragma once

#include "CoreMinimal.h"

struct FFPVHIDDeviceInfo
{
	FString Name;
	FString Path;
	int32 NumAxes = 0;
	int32 NumButtons = 0;
	bool bIsJoystick = false;	// HID usage "joystick" (radios) rather than "gamepad"
};

struct FFPVHIDSnapshot
{
	bool bConnected = false;
	FString DeviceName;
	TArray<float> Axes;		// -1..1, scaled from the HID logical range, uncalibrated
	TArray<bool> Buttons;
	TArray<FString> AxisNames;	// "X", "Y", "Z", "Rx", ...
	double LastReportTime = 0.0;	// FPlatformTime::Seconds of the last report
	uint64 ReportCount = 0;
};

class FPVDRONE_API FFPVHIDManager
{
public:
	static FFPVHIDManager& Get();

	void Start();
	void Stop();

	// Call from the game thread once per frame; handles reconnects.
	void Tick();

	void RefreshDevices();
	TArray<FFPVHIDDeviceInfo> GetDevices() const;

	// Opens the device with this product name (empty = best guess). Persisted in the settings.
	void SelectDevice(const FString& Name);

	// Copies the latest values. Returns false if no device is open.
	bool GetSnapshot(FFPVHIDSnapshot& Out) const;

	bool IsSupported() const;

	~FFPVHIDManager();

private:
	FFPVHIDManager() = default;

	struct FImpl;
	TUniquePtr<FImpl> Impl;
	double LastReconnectAttempt = 0.0;
	bool bStarted = false;
};
