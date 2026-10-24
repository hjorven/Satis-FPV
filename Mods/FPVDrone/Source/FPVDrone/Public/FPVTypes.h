// Settings structures. Everything here is saved to Saved/FPVDrone/Settings.json and
// edited by the in-game config menu. Units follow the Betaflight Configurator where possible.

#pragma once

#include "CoreMinimal.h"
#include "FPVTypes.generated.h"

UENUM(BlueprintType)
enum class EFPVRatesType : uint8
{
	Betaflight,
	Actual,
	Quick,
	Kiss,
};

UENUM(BlueprintType)
enum class EFPVFlightMode : uint8
{
	Acro,
	Angle,
	Horizon,
};

UENUM(BlueprintType)
enum class EFPVInputSource : uint8
{
	// HID radio if one is configured and connected, otherwise gamepad, otherwise keyboard.
	Auto,
	HID,
	Gamepad,
	Keyboard,
};

UENUM(BlueprintType)
enum class EFPVCameraMode : uint8
{
	FPV,
	Chase,
};

// Logical radio channels the flight controller understands.
UENUM(BlueprintType)
enum class EFPVChannel : uint8
{
	Roll,
	Pitch,
	Yaw,
	Throttle,
	Arm,
	FlightMode,
	Mode3D,
	Turtle,
	Count UMETA(Hidden),
};

constexpr int32 FPVChannelCount = static_cast<int32>(EFPVChannel::Count);

USTRUCT(BlueprintType)
struct FFPVAxisRates
{
	GENERATED_BODY()

	// Betaflight: RC Rate | Actual: Center Sensitivity (deg/s) | Quick: RC Rate | KISS: RC Rate
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float A = 70.0f;
	// Betaflight: Super Rate | Actual: Max Rate (deg/s) | Quick: Max Rate (deg/s) | KISS: Rate
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float B = 670.0f;
	// Expo (all types)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float C = 0.54f;
};

USTRUCT(BlueprintType)
struct FFPVPid
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float P = 45.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float I = 80.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float D = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float F = 120.0f;
};

USTRUCT(BlueprintType)
struct FFPVTune
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFPVRatesType RatesType = EFPVRatesType::Actual;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVAxisRates RollRates;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVAxisRates PitchRates;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVAxisRates YawRates;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVPid RollPid;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVPid PitchPid;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVPid YawPid;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ThrottleMid = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ThrottleExpo = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ThrottleLimitPercent = 100.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MotorIdlePercent = 5.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAirmode = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float GyroLpfHz = 250.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DtermLpfHz = 90.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FeedforwardSmoothHz = 25.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RcSmoothingHz = 40.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ItermLimit = 400.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ItermRelaxCutoffHz = 15.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TpaRatePercent = 65.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float TpaBreakpoint = 1350.0f;	// Betaflight PWM units (1000..2000)

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AngleLimitDeg = 60.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AngleStrength = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HorizonStrength = 50.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HorizonTransition = 0.75f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float StickDeadband = 0.01f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float YawDeadband = 0.02f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Deadband3D = 0.06f;

	FFPVTune();
};

USTRUCT(BlueprintType)
struct FFPVCraft
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MassKg = 0.65f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ArmLengthM = 0.11f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MotorMaxThrustN = 16.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MotorResponseMs = 20.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float DragScale = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PropwashStrength = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float PropDiameterInch = 5.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BatteryCells = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BatteryCapacityMah = 1300.0f;
	// 1 = realistic flight time (~3-12 min). Raise for a more relaxed game.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BatteryCapacityMultiplier = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CellResistanceMilliOhm = 4.0f;

	// Impact speed (m/s) above which the craft disarms like a real crash.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float CrashDisarmSpeed = 9.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float BounceRestitution = 0.25f;
};

USTRUCT(BlueprintType)
struct FFPVCameraSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFPVCameraMode Mode = EFPVCameraMode::FPV;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float UptiltDeg = 30.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FieldOfView = 120.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ChaseDistanceCm = 150.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShowOSD = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bOSDMetric = true;
};

// Where a logical channel comes from on the HID device.
USTRUCT(BlueprintType)
struct FFPVChannelMapping
{
	GENERATED_BODY()

	// Index into the device's axis list, -1 = unused.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Axis = -1;
	// Index into the device's button list, -1 = unused. Used for switches when Axis is -1.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Button = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInvert = false;
};

// Raw axis range captured by the calibration wizard (values in -1..1 after HID logical scaling).
USTRUCT(BlueprintType)
struct FFPVAxisCalibration
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Min = -1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Center = 0.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Max = 1.0f;
};

USTRUCT(BlueprintType)
struct FFPVRadioSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFPVInputSource Source = EFPVInputSource::Auto;

	// Product name of the HID device to use. Empty = first joystick found.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString DeviceName;

	// One entry per EFPVChannel. Default is AETR + AUX1 arm, AUX2 mode, AUX3 3D, AUX4 turtle,
	// which matches EdgeTX's default USB joystick mapping on the Radiomaster Pocket.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFPVChannelMapping> Channels;

	// Calibration per HID axis index.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FFPVAxisCalibration> Calibration;

	// Switch positions: channel value (-1..1) above this counts as "on".
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SwitchThreshold = 0.25f;

	// Flight modes for the low / middle / high position of the mode switch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFPVFlightMode ModeLow = EFPVFlightMode::Acro;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFPVFlightMode ModeMid = EFPVFlightMode::Horizon;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EFPVFlightMode ModeHigh = EFPVFlightMode::Angle;

	// Like Betaflight: refuse to arm when the throttle is not at the bottom.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bArmRequiresLowThrottle = true;

	// Gamepad: map the full left stick Y travel to 0..100 % throttle (otherwise only the upper half).
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bGamepadFullRangeThrottle = true;

	FFPVRadioSettings();
};

USTRUCT(BlueprintType)
struct FFPVKeyBindings
{
	GENERATED_BODY()

	// Launch the drone from the inventory, or put the goggles on/off.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ToggleGoggles = TEXT("K");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString PickUpDrone = TEXT("L");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString OpenMenu = TEXT("F10");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString ToggleCamera = TEXT("C");

	// Keyboard / gamepad flying (used when no radio is connected)
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString KeyboardArm = TEXT("R");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString KeyboardMode = TEXT("M");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Keyboard3D = TEXT("N");
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString KeyboardTurtle = TEXT("T");
};

USTRUCT(BlueprintType)
struct FFPVGameplaySettings
{
	GENERATED_BODY()

	// Video/control link range from the pilot. 0 = unlimited.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MaxRangeMeters = 1500.0f;
	// Walls between pilot and drone reduce the link quality.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bObstaclesReduceSignal = true;
	// Disarm after one second without link, like Betaflight's failsafe "drop".
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFailsafe = true;
	// Always fly in 3D mode (reversible motors) regardless of the switch.
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAlways3D = false;
};

USTRUCT(BlueprintType)
struct FFPVConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Version = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVTune Tune;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVCraft Craft;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVCameraSettings Camera;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVRadioSettings Radio;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVKeyBindings Keys;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FFPVGameplaySettings Gameplay;
};

// Final pilot command for one frame, produced by the input system.
struct FFPVPilotInput
{
	float Roll = 0.0f;
	float Pitch = 0.0f;
	float Yaw = 0.0f;
	float Throttle = 0.0f;	// 0..1
	bool bArmSwitch = false;
	EFPVFlightMode Mode = EFPVFlightMode::Acro;
	bool b3D = false;
	bool bTurtle = false;
	bool bValid = false;	// false = no input source, treated like signal loss
	FString SourceName;
};
