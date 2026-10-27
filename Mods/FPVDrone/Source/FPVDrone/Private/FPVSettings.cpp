#include "FPVSettings.h"

#include "FPVDroneModule.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

bool FFPVSettings::bLoaded = false;
FSimpleMulticastDelegate FFPVSettings::OnChanged;

FFPVTune::FFPVTune()
{
	YawRates.C = 0.0f;
	PitchPid.P = 47.0f; PitchPid.I = 84.0f; PitchPid.D = 46.0f; PitchPid.F = 125.0f;
	YawPid.D = 0.0f;
}

FFPVRadioSettings::FFPVRadioSettings()
{
	Channels.SetNum(FPVChannelCount);
	// EdgeTX/OpenTX expose the USB joystick axes sorted X, Y, Z, Rx, Ry, Rz, Slider, Dial, which in
	// the default AETR channel order is Roll, Pitch, Throttle, Yaw on the first four axes. Our logical
	// channel enum is ordered Roll, Pitch, Yaw, Throttle, so the old "channel index == axis index"
	// default put Yaw and Throttle on each other's axis. Map AETR explicitly, then AUX1..AUX4.
	// (Radios that route gimbals to other usages still need the in-game Detect per channel.)
	auto Set = [this](EFPVChannel Ch, int32 Axis) { Channels[static_cast<int32>(Ch)].Axis = Axis; };
	Set(EFPVChannel::Roll, 0);
	Set(EFPVChannel::Pitch, 1);
	Set(EFPVChannel::Throttle, 2);
	Set(EFPVChannel::Yaw, 3);
	Set(EFPVChannel::Arm, 4);
	Set(EFPVChannel::FlightMode, 5);
	Set(EFPVChannel::Mode3D, 6);
	Set(EFPVChannel::Turtle, 7);
}

FFPVConfig& FFPVSettings::Get()
{
	static FFPVConfig Config;
	if (!bLoaded)
	{
		bLoaded = true;
		const FString Path = GetFilePath();
		FString Json;
		if (FFileHelper::LoadFileToString(Json, *Path))
		{
			FFPVConfig Loaded;
			if (FJsonObjectConverter::JsonObjectStringToUStruct(Json, &Loaded, 0, 0))
			{
				Config = Loaded;
				UE_LOG(LogFPVDrone, Log, TEXT("Loaded settings from %s"), *Path);
			}
			else
			{
				UE_LOG(LogFPVDrone, Warning, TEXT("Could not parse %s, using defaults"), *Path);
			}
		}
		Sanitize(Config);
	}
	return Config;
}

void FFPVSettings::Load()
{
	bLoaded = false;
	Get();
	OnChanged.Broadcast();
}

void FFPVSettings::Save()
{
	FFPVConfig& Config = Get();
	Sanitize(Config);
	FString Json;
	if (FJsonObjectConverter::UStructToJsonObjectString(Config, Json, 0, 0))
	{
		FFileHelper::SaveStringToFile(Json, *GetFilePath());
	}
	OnChanged.Broadcast();
}

FString FFPVSettings::GetFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("FPVDrone"), TEXT("Settings.json"));
}

void FFPVSettings::Sanitize(FFPVConfig& Config)
{
	FFPVRadioSettings Defaults;
	if (Config.Radio.Channels.Num() != FPVChannelCount)
	{
		const int32 Old = Config.Radio.Channels.Num();
		Config.Radio.Channels.SetNum(FPVChannelCount);
		for (int32 Index = Old; Index < FPVChannelCount; ++Index)
		{
			Config.Radio.Channels[Index] = Defaults.Channels[Index];
		}
	}
	Config.Craft.BatteryCells = FMath::Clamp(Config.Craft.BatteryCells, 1, 8);
	Config.Craft.MassKg = FMath::Max(0.01f, Config.Craft.MassKg);
	Config.Craft.MotorMaxThrustN = FMath::Max(0.05f, Config.Craft.MotorMaxThrustN);
	Config.Camera.FieldOfView = FMath::Clamp(Config.Camera.FieldOfView, 60.0f, 170.0f);
	Config.Camera.UptiltDeg = FMath::Clamp(Config.Camera.UptiltDeg, -20.0f, 80.0f);
	Config.Tune.ThrottleMid = FMath::Clamp(Config.Tune.ThrottleMid, 0.05f, 0.95f);
}

FPVSim::Params FFPVSettings::ToSimParams(const FFPVConfig& Config)
{
	const FFPVTune& T = Config.Tune;
	const FFPVCraft& C = Config.Craft;
	FPVSim::Params P;

	P.Rates = static_cast<FPVSim::RatesType>(T.RatesType);
	const FFPVAxisRates* Rates[3] = { &T.RollRates, &T.PitchRates, &T.YawRates };
	const FFPVPid* Pids[3] = { &T.RollPid, &T.PitchPid, &T.YawPid };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		P.AxisRate[Axis] = { Rates[Axis]->A, Rates[Axis]->B, Rates[Axis]->C };
		P.Pid[Axis] = { Pids[Axis]->P, Pids[Axis]->I, Pids[Axis]->D, Pids[Axis]->F };
	}

	P.StickDeadband = T.StickDeadband;
	P.YawDeadband = T.YawDeadband;
	P.RcSmoothingHz = T.RcSmoothingHz;
	P.ThrottleMid = T.ThrottleMid;
	P.ThrottleExpo = T.ThrottleExpo;
	P.ThrottleLimit = FMath::Clamp(T.ThrottleLimitPercent / 100.0f, 0.0f, 1.0f);
	P.MotorIdle = FMath::Clamp(T.MotorIdlePercent / 100.0f, 0.0f, 0.3f);
	P.Airmode = T.bAirmode;
	P.GyroLpfHz = T.GyroLpfHz;
	P.DtermLpfHz = T.DtermLpfHz;
	P.FeedforwardSmoothHz = T.FeedforwardSmoothHz;
	P.ItermLimit = T.ItermLimit;
	P.ItermRelaxCutoffHz = T.ItermRelaxCutoffHz;
	P.TpaRate = FMath::Clamp(T.TpaRatePercent / 100.0f, 0.0f, 1.0f);
	P.TpaBreakpoint = FMath::Clamp((T.TpaBreakpoint - 1000.0f) / 1000.0f, 0.0f, 1.0f);
	P.AngleLimitDeg = T.AngleLimitDeg;
	P.AngleStrength = T.AngleStrength;
	P.HorizonStrength = T.HorizonStrength;
	P.HorizonTransition = T.HorizonTransition;
	P.Deadband3D = T.Deadband3D;

	FPVSim::ApplyCraftScaling(P, C.MassKg, C.ArmLengthM, C.PropDiameterInch, C.DragScale);
	P.MotorMaxThrust = C.MotorMaxThrustN;
	P.MotorTimeConstantUp = FMath::Max(1.0f, C.MotorResponseMs) / 1000.0f;
	P.MotorTimeConstantDown = P.MotorTimeConstantUp * 1.25f;
	P.PropwashStrength = C.PropwashStrength;
	P.Cells = C.BatteryCells;
	P.CapacityMah = C.BatteryCapacityMah;
	P.CapacityMultiplier = FMath::Max(0.1f, C.BatteryCapacityMultiplier);
	P.CellResistance = C.CellResistanceMilliOhm / 1000.0f;
	return P;
}

void FFPVSettings::ApplyPreset(FFPVConfig& Config, EFPVPreset Preset)
{
	FFPVTune Tune;
	FFPVCraft Craft;

	switch (Preset)
	{
	case EFPVPreset::Racer5:
		Tune.RollRates.A = 100.0f; Tune.RollRates.B = 850.0f; Tune.RollRates.C = 0.3f;
		Tune.PitchRates = Tune.RollRates;
		Tune.YawRates.A = 100.0f; Tune.YawRates.B = 650.0f; Tune.YawRates.C = 0.2f;
		Tune.RollPid.P = 50.0f; Tune.PitchPid.P = 54.0f;
		Craft.MassKg = 0.52f;
		Craft.MotorMaxThrustN = 17.0f;
		Craft.DragScale = 0.8f;
		Craft.BatteryCapacityMah = 1100.0f;
		break;

	case EFPVPreset::Cinewhoop3:
		Tune.RollRates.A = 60.0f; Tune.RollRates.B = 450.0f; Tune.RollRates.C = 0.35f;
		Tune.PitchRates = Tune.RollRates;
		Tune.YawRates.A = 60.0f; Tune.YawRates.B = 350.0f; Tune.YawRates.C = 0.0f;
		Tune.ThrottleMid = 0.4f; Tune.ThrottleExpo = 0.3f;
		Craft.MassKg = 0.38f;
		Craft.ArmLengthM = 0.07f;
		Craft.MotorMaxThrustN = 5.5f;
		Craft.PropDiameterInch = 3.0f;
		Craft.DragScale = 1.8f;
		Craft.PropwashStrength = 1.6f;
		Craft.BatteryCells = 4;
		Craft.BatteryCapacityMah = 850.0f;
		Craft.CrashDisarmSpeed = 12.0f;
		Craft.BounceRestitution = 0.45f;
		break;

	case EFPVPreset::Tinywhoop:
		Tune.RollRates.A = 70.0f; Tune.RollRates.B = 600.0f; Tune.RollRates.C = 0.4f;
		Tune.PitchRates = Tune.RollRates;
		Tune.YawRates.A = 70.0f; Tune.YawRates.B = 500.0f; Tune.YawRates.C = 0.0f;
		Tune.MotorIdlePercent = 7.0f;
		Craft.MassKg = 0.032f;
		Craft.ArmLengthM = 0.033f;
		Craft.MotorMaxThrustN = 0.26f;
		Craft.PropDiameterInch = 1.2f;
		Craft.MotorResponseMs = 15.0f;
		Craft.DragScale = 2.5f;
		Craft.BatteryCells = 1;
		Craft.BatteryCapacityMah = 300.0f;
		Craft.CrashDisarmSpeed = 20.0f;
		Craft.BounceRestitution = 0.5f;
		break;

	case EFPVPreset::Beginner:
		Tune.RollRates.A = 50.0f; Tune.RollRates.B = 400.0f; Tune.RollRates.C = 0.4f;
		Tune.PitchRates = Tune.RollRates;
		Tune.YawRates.A = 50.0f; Tune.YawRates.B = 300.0f; Tune.YawRates.C = 0.2f;
		Tune.ThrottleLimitPercent = 75.0f;
		Tune.ThrottleExpo = 0.3f;
		Tune.AngleLimitDeg = 45.0f;
		Craft.BatteryCapacityMultiplier = 3.0f;
		Craft.CrashDisarmSpeed = 14.0f;
		break;

	case EFPVPreset::Freestyle5:
	default:
		break;
	}

	Config.Tune = Tune;
	Config.Craft = Craft;
}

FText FFPVSettings::GetPresetName(EFPVPreset Preset)
{
	switch (Preset)
	{
	case EFPVPreset::Freestyle5: return LOCTEXT("PresetFreestyle", "5\" Freestyle (6S)");
	case EFPVPreset::Racer5: return LOCTEXT("PresetRacer", "5\" Racer (6S)");
	case EFPVPreset::Cinewhoop3: return LOCTEXT("PresetCinewhoop", "3\" Cinewhoop (4S)");
	case EFPVPreset::Tinywhoop: return LOCTEXT("PresetTinywhoop", "Tinywhoop (1S)");
	case EFPVPreset::Beginner: return LOCTEXT("PresetBeginner", "Beginner (soft rates, 3x battery)");
	default: return FText::GetEmpty();
	}
}

namespace FPVSettingsCli
{
	// Betaflight stores rates as integers; convert to the units of FFPVAxisRates for a given type.
	float RcRateFromCli(EFPVRatesType Type, int32 Value)
	{
		return Type == EFPVRatesType::Actual ? Value * 10.0f : Value / 100.0f;
	}

	float SuperRateFromCli(EFPVRatesType Type, int32 Value)
	{
		return (Type == EFPVRatesType::Actual || Type == EFPVRatesType::Quick) ? Value * 10.0f : Value / 100.0f;
	}

	int32 RcRateToCli(EFPVRatesType Type, float Value)
	{
		return FMath::RoundToInt(Type == EFPVRatesType::Actual ? Value / 10.0f : Value * 100.0f);
	}

	int32 SuperRateToCli(EFPVRatesType Type, float Value)
	{
		return FMath::RoundToInt((Type == EFPVRatesType::Actual || Type == EFPVRatesType::Quick) ? Value / 10.0f : Value * 100.0f);
	}
}

int32 FFPVSettings::ImportBetaflightCli(FFPVConfig& Config, const FString& Text, FString& OutReport)
{
	using namespace FPVSettingsCli;

	TMap<FString, FString> Values;
	TOptional<bool> Airmode;

	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (FString Line : Lines)
	{
		Line.TrimStartAndEndInline();
		if (Line.StartsWith(TEXT("feature ")))
		{
			const FString Feature = Line.Mid(8).TrimStartAndEnd();
			if (Feature.Equals(TEXT("AIRMODE"), ESearchCase::IgnoreCase)) { Airmode = true; }
			if (Feature.Equals(TEXT("-AIRMODE"), ESearchCase::IgnoreCase)) { Airmode = false; }
			continue;
		}
		if (!Line.StartsWith(TEXT("set ")))
		{
			continue;
		}
		FString Name, Value;
		if (Line.Mid(4).Split(TEXT("="), &Name, &Value))
		{
			Values.Add(Name.TrimStartAndEnd().ToLower(), Value.TrimStartAndEnd());
		}
	}

	FFPVTune& T = Config.Tune;
	int32 Applied = 0;
	TArray<FString> Used;

	auto Int = [&](const TCHAR* Key, TFunctionRef<void(int32)> Apply)
	{
		if (const FString* Found = Values.Find(Key))
		{
			Apply(FCString::Atoi(**Found));
			Used.Add(Key);
			++Applied;
		}
	};

	if (const FString* Type = Values.Find(TEXT("rates_type")))
	{
		const FString Upper = Type->ToUpper();
		if (Upper == TEXT("BETAFLIGHT")) { T.RatesType = EFPVRatesType::Betaflight; }
		else if (Upper == TEXT("ACTUAL")) { T.RatesType = EFPVRatesType::Actual; }
		else if (Upper == TEXT("QUICK")) { T.RatesType = EFPVRatesType::Quick; }
		else if (Upper == TEXT("KISS")) { T.RatesType = EFPVRatesType::Kiss; }
		else { OutReport += FString::Printf(TEXT("rates_type %s is not supported, keeping %d\n"), **Type, static_cast<int32>(T.RatesType)); }
		Used.Add(TEXT("rates_type"));
		++Applied;
	}
	else if (Values.Contains(TEXT("roll_rc_rate")))
	{
		// Pre-4.2 dumps have no rates_type and always use Betaflight rates.
		T.RatesType = EFPVRatesType::Betaflight;
	}

	const TCHAR* AxisNames[3] = { TEXT("roll"), TEXT("pitch"), TEXT("yaw") };
	FFPVAxisRates* Rates[3] = { &T.RollRates, &T.PitchRates, &T.YawRates };
	FFPVPid* Pids[3] = { &T.RollPid, &T.PitchPid, &T.YawPid };
	const EFPVRatesType Type = T.RatesType;

	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		const FString A = AxisNames[Axis];
		FFPVAxisRates* R = Rates[Axis];
		FFPVPid* Pid = Pids[Axis];
		Int(*(A + TEXT("_rc_rate")), [&](int32 V) { R->A = RcRateFromCli(Type, V); });
		Int(*(A + TEXT("_srate")), [&](int32 V) { R->B = SuperRateFromCli(Type, V); });
		Int(*(A + TEXT("_expo")), [&](int32 V) { R->C = V / 100.0f; });
		Int(*(TEXT("p_") + A), [&](int32 V) { Pid->P = static_cast<float>(V); });
		Int(*(TEXT("i_") + A), [&](int32 V) { Pid->I = static_cast<float>(V); });
		Int(*(TEXT("d_") + A), [&](int32 V) { Pid->D = static_cast<float>(V); });
		Int(*(TEXT("f_") + A), [&](int32 V) { Pid->F = static_cast<float>(V); });
	}
	// Pre-4.0 dumps share rc_rate between roll and pitch.
	Int(TEXT("rc_rate"), [&](int32 V) { T.RollRates.A = T.PitchRates.A = RcRateFromCli(Type, V); });
	Int(TEXT("rc_expo"), [&](int32 V) { T.RollRates.C = T.PitchRates.C = V / 100.0f; });

	Int(TEXT("thr_mid"), [&](int32 V) { T.ThrottleMid = V / 100.0f; });
	Int(TEXT("thr_expo"), [&](int32 V) { T.ThrottleExpo = V / 100.0f; });
	Int(TEXT("throttle_limit_percent"), [&](int32 V) { T.ThrottleLimitPercent = static_cast<float>(V); });
	Int(TEXT("dshot_idle_value"), [&](int32 V) { T.MotorIdlePercent = V / 100.0f; });
	Int(TEXT("tpa_rate"), [&](int32 V) { T.TpaRatePercent = static_cast<float>(V); });
	Int(TEXT("tpa_breakpoint"), [&](int32 V) { T.TpaBreakpoint = static_cast<float>(V); });
	Int(TEXT("iterm_relax_cutoff"), [&](int32 V) { T.ItermRelaxCutoffHz = static_cast<float>(V); });
	Int(TEXT("iterm_limit"), [&](int32 V) { T.ItermLimit = static_cast<float>(V); });
	Int(TEXT("gyro_lpf1_static_hz"), [&](int32 V) { if (V > 0) { T.GyroLpfHz = static_cast<float>(V); } });
	Int(TEXT("dterm_lpf1_static_hz"), [&](int32 V) { if (V > 0) { T.DtermLpfHz = static_cast<float>(V); } });
	Int(TEXT("feedforward_smooth_factor"), [&](int32 V) { T.FeedforwardSmoothHz = FMath::Lerp(60.0f, 10.0f, FMath::Clamp(V / 75.0f, 0.0f, 1.0f)); });
	Int(TEXT("angle_limit"), [&](int32 V) { T.AngleLimitDeg = static_cast<float>(V); });
	Int(TEXT("level_limit"), [&](int32 V) { T.AngleLimitDeg = static_cast<float>(V); });
	Int(TEXT("angle_p_gain"), [&](int32 V) { T.AngleStrength = static_cast<float>(V); });
	Int(TEXT("angle_level_strength"), [&](int32 V) { T.AngleStrength = static_cast<float>(V); });
	Int(TEXT("p_level"), [&](int32 V) { T.AngleStrength = static_cast<float>(V); });
	Int(TEXT("horizon_level_strength"), [&](int32 V) { T.HorizonStrength = static_cast<float>(V); });
	Int(TEXT("deadband"), [&](int32 V) { T.StickDeadband = V / 500.0f; });
	Int(TEXT("yaw_deadband"), [&](int32 V) { T.YawDeadband = V / 500.0f; });
	Int(TEXT("3d_deadband_throttle"), [&](int32 V) { T.Deadband3D = V / 500.0f; });

	if (Airmode.IsSet())
	{
		T.bAirmode = Airmode.GetValue();
		Used.Add(TEXT("feature AIRMODE"));
		++Applied;
	}

	OutReport += FString::Printf(TEXT("Applied %d setting(s): %s\n"), Applied, *FString::Join(Used, TEXT(", ")));
	const int32 Ignored = Values.Num() - Used.Num() + (Airmode.IsSet() ? 1 : 0);
	if (Ignored > 0)
	{
		OutReport += FString::Printf(TEXT("%d other setting(s) have no equivalent in the simulation and were ignored.\n"), Ignored);
	}
	return Applied;
}

FString FFPVSettings::ExportBetaflightCli(const FFPVConfig& Config)
{
	using namespace FPVSettingsCli;

	const FFPVTune& T = Config.Tune;
	static const TCHAR* TypeNames[] = { TEXT("BETAFLIGHT"), TEXT("ACTUAL"), TEXT("QUICK"), TEXT("KISS") };
	FString Out = TEXT("# FPVDrone tune export\n");
	Out += FString::Printf(TEXT("set rates_type = %s\n"), TypeNames[static_cast<int32>(T.RatesType)]);

	const TCHAR* AxisNames[3] = { TEXT("roll"), TEXT("pitch"), TEXT("yaw") };
	const FFPVAxisRates* Rates[3] = { &T.RollRates, &T.PitchRates, &T.YawRates };
	const FFPVPid* Pids[3] = { &T.RollPid, &T.PitchPid, &T.YawPid };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Out += FString::Printf(TEXT("set %s_rc_rate = %d\n"), AxisNames[Axis], RcRateToCli(T.RatesType, Rates[Axis]->A));
		Out += FString::Printf(TEXT("set %s_srate = %d\n"), AxisNames[Axis], SuperRateToCli(T.RatesType, Rates[Axis]->B));
		Out += FString::Printf(TEXT("set %s_expo = %d\n"), AxisNames[Axis], FMath::RoundToInt(Rates[Axis]->C * 100.0f));
	}
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		Out += FString::Printf(TEXT("set p_%s = %d\nset i_%s = %d\nset d_%s = %d\nset f_%s = %d\n"),
			AxisNames[Axis], FMath::RoundToInt(Pids[Axis]->P), AxisNames[Axis], FMath::RoundToInt(Pids[Axis]->I),
			AxisNames[Axis], FMath::RoundToInt(Pids[Axis]->D), AxisNames[Axis], FMath::RoundToInt(Pids[Axis]->F));
	}
	Out += FString::Printf(TEXT("set thr_mid = %d\nset thr_expo = %d\nset throttle_limit_percent = %d\n"),
		FMath::RoundToInt(T.ThrottleMid * 100.0f), FMath::RoundToInt(T.ThrottleExpo * 100.0f), FMath::RoundToInt(T.ThrottleLimitPercent));
	Out += FString::Printf(TEXT("set dshot_idle_value = %d\nset tpa_rate = %d\nset tpa_breakpoint = %d\n"),
		FMath::RoundToInt(T.MotorIdlePercent * 100.0f), FMath::RoundToInt(T.TpaRatePercent), FMath::RoundToInt(T.TpaBreakpoint));
	Out += FString::Printf(TEXT("set angle_limit = %d\nset angle_p_gain = %d\nset horizon_level_strength = %d\n"),
		FMath::RoundToInt(T.AngleLimitDeg), FMath::RoundToInt(T.AngleStrength), FMath::RoundToInt(T.HorizonStrength));
	Out += T.bAirmode ? TEXT("feature AIRMODE\n") : TEXT("feature -AIRMODE\n");
	return Out;
}

FKey FFPVSettings::ParseKey(const FString& Name)
{
	const FKey Key(*Name);
	return Key.IsValid() ? Key : EKeys::Invalid;
}

#undef LOCTEXT_NAMESPACE
