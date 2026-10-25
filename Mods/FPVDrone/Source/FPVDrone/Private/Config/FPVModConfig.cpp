#include "Config/FPVModConfig.h"

#include "Configuration/ConfigManager.h"
#include "Configuration/Properties/ConfigPropertyBool.h"
#include "Configuration/Properties/ConfigPropertyFloat.h"
#include "Configuration/Properties/ConfigPropertySection.h"
#include "Engine/GameInstance.h"
#include "FPVSettings.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

namespace FPVModConfigLocal
{
	const FConfigId ConfigId{ TEXT("FPVDrone"), TEXT("") };

	// Accessors for the mirrored fields in FFPVConfig, keyed like the config page entries.
	bool& ShowOSD(FFPVConfig& C) { return C.Camera.bShowOSD; }
	bool& Metric(FFPVConfig& C) { return C.Camera.bOSDMetric; }
	bool& Obstacles(FFPVConfig& C) { return C.Gameplay.bObstaclesReduceSignal; }
	bool& Failsafe(FFPVConfig& C) { return C.Gameplay.bFailsafe; }
	bool& Always3D(FFPVConfig& C) { return C.Gameplay.bAlways3D; }
	float& Range(FFPVConfig& C) { return C.Gameplay.MaxRangeMeters; }
	float& FlightTime(FFPVConfig& C) { return C.Craft.BatteryCapacityMultiplier; }

	struct FBoolField { const TCHAR* Key; bool& (*Get)(FFPVConfig&); };
	struct FFloatField { const TCHAR* Key; float& (*Get)(FFPVConfig&); };

	const FBoolField BoolFields[] = {
		{ TEXT("ShowOSD"), &ShowOSD },
		{ TEXT("MetricUnits"), &Metric },
		{ TEXT("WallsWeakenSignal"), &Obstacles },
		{ TEXT("Failsafe"), &Failsafe },
		{ TEXT("Always3D"), &Always3D },
	};
	const FFloatField FloatFields[] = {
		{ TEXT("MaxRangeMeters"), &Range },
		{ TEXT("FlightTimeMultiplier"), &FlightTime },
	};

	// Last values seen in the config page, to tell which side changed.
	TMap<FString, float> LastSeen;
	TWeakObjectPtr<UGameInstance> RegisteredFor;

	template <typename T>
	T* AddProperty(UObject* Outer, UConfigPropertySection* Section, const TCHAR* Key, const FString& Name, const FString& Tooltip)
	{
		T* Property = Outer->CreateDefaultSubobject<T>(FName(Key));
		Property->DisplayName = Name;
		Property->Tooltip = Tooltip;
		Section->SectionProperties.Add(Key, Property);
		return Property;
	}
}

using namespace FPVModConfigLocal;

UFPVModConfiguration::UFPVModConfiguration()
{
	// The name carries the hint too, in case the page does not show the description.
	DisplayName = LOCTEXT("ConfigName", "FPV Drone (more settings: F10 in game)");
	Description = LOCTEXT("ConfigDesc", "Basic settings. Rates, PID tuning, radio setup and Betaflight import are in the full FPV menu: press F10 in game (the key can be changed there).");

	const FFPVConfig Defaults;
	RootSection = CreateDefaultSubobject<UConfigPropertySection>(TEXT("RootSection"));

	struct FBoolText { const TCHAR* Key; const TCHAR* Name; const TCHAR* Tooltip; };
	const FBoolText BoolTexts[] = {
		{ TEXT("ShowOSD"), TEXT("Show OSD"), TEXT("On-screen display in the goggles.") },
		{ TEXT("MetricUnits"), TEXT("Metric units"), TEXT("km/h and metres on the OSD, otherwise mph and feet.") },
		{ TEXT("WallsWeakenSignal"), TEXT("Walls weaken the signal"), TEXT("Video breaks up behind buildings and terrain.") },
		{ TEXT("Failsafe"), TEXT("Failsafe on signal loss"), TEXT("The drone disarms after one second without signal.") },
		{ TEXT("Always3D"), TEXT("Always 3D mode"), TEXT("Reversible motors, throttle centre = zero thrust.") },
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(BoolTexts); ++Index)
	{
		UConfigPropertyBool* Property = AddProperty<UConfigPropertyBool>(this, RootSection, BoolTexts[Index].Key, BoolTexts[Index].Name, BoolTexts[Index].Tooltip);
		FFPVConfig Copy = Defaults;
		Property->DefaultValue = Property->Value = BoolFields[Index].Get(Copy);
	}

	struct FFloatText { const TCHAR* Key; const TCHAR* Name; const TCHAR* Tooltip; };
	const FFloatText FloatTexts[] = {
		{ TEXT("MaxRangeMeters"), TEXT("Max range (m, 0 = unlimited)"), TEXT("Distance from the pilot at which the signal is lost.") },
		{ TEXT("FlightTimeMultiplier"), TEXT("Flight time multiplier"), TEXT("1 = about 4 minutes of mixed flying.") },
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(FloatTexts); ++Index)
	{
		UConfigPropertyFloat* Property = AddProperty<UConfigPropertyFloat>(this, RootSection, FloatTexts[Index].Key, FloatTexts[Index].Name, FloatTexts[Index].Tooltip);
		FFPVConfig Copy = Defaults;
		Property->DefaultValue = Property->Value = FloatFields[Index].Get(Copy);
	}
}

void FPVModConfig::Register(UGameInstance* GameInstance)
{
	UConfigManager* Manager = GameInstance != nullptr ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
	if (Manager == nullptr || RegisteredFor.Get() == GameInstance)
	{
		return;
	}
	Manager->RegisterModConfiguration(ConfigId, UFPVModConfiguration::StaticClass());
	RegisteredFor = GameInstance;
	LastSeen.Reset();
}

void FPVModConfig::Sync(UGameInstance* GameInstance)
{
	UConfigManager* Manager = GameInstance != nullptr ? GameInstance->GetSubsystem<UConfigManager>() : nullptr;
	UConfigPropertySection* Root = Manager != nullptr ? Manager->GetConfigurationRootSection(ConfigId) : nullptr;
	if (Root == nullptr)
	{
		return;
	}

	FFPVConfig& Config = FFPVSettings::Get();
	bool bSettingsChanged = false;
	bool bConfigChanged = false;

	auto SyncValue = [&](const TCHAR* Key, float PageValue, float& SettingValue, TFunctionRef<void(float)> WritePage)
	{
		const float* Seen = LastSeen.Find(Key);
		if (Seen == nullptr || *Seen != PageValue)
		{
			// First run or changed in the ESC menu: the page wins, unless this is the first run
			// and the page still holds its default (then the F10 settings file wins).
			if (Seen != nullptr && SettingValue != PageValue)
			{
				SettingValue = PageValue;
				bSettingsChanged = true;
			}
			else if (Seen == nullptr && SettingValue != PageValue)
			{
				WritePage(SettingValue);
				PageValue = SettingValue;
				bConfigChanged = true;
			}
		}
		else if (SettingValue != PageValue)
		{
			// Changed in the F10 menu.
			WritePage(SettingValue);
			PageValue = SettingValue;
			bConfigChanged = true;
		}
		LastSeen.Add(Key, PageValue);
	};

	for (const FBoolField& Field : BoolFields)
	{
		UConfigPropertyBool* Property = Cast<UConfigPropertyBool>(Root->SectionProperties.FindRef(Field.Key));
		if (Property == nullptr)
		{
			continue;
		}
		bool& Setting = Field.Get(Config);
		float SettingValue = Setting ? 1.0f : 0.0f;
		SyncValue(Field.Key, Property->Value ? 1.0f : 0.0f, SettingValue, [Property](float V) { Property->Value = V > 0.5f; });
		Setting = SettingValue > 0.5f;
	}
	for (const FFloatField& Field : FloatFields)
	{
		UConfigPropertyFloat* Property = Cast<UConfigPropertyFloat>(Root->SectionProperties.FindRef(Field.Key));
		if (Property == nullptr)
		{
			continue;
		}
		SyncValue(Field.Key, Property->Value, Field.Get(Config), [Property](float V) { Property->Value = V; });
	}

	if (bSettingsChanged)
	{
		FFPVSettings::Sanitize(Config);
		FFPVSettings::Save();
	}
	if (bConfigChanged)
	{
		Manager->MarkConfigurationDirty(ConfigId);
	}
}

#undef LOCTEXT_NAMESPACE
