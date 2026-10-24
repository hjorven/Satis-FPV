#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "FPVTypes.h"
#include "Sim/FPVFlightSim.h"

enum class EFPVPreset : uint8
{
	Freestyle5,
	Racer5,
	Cinewhoop3,
	Tinywhoop,
	Beginner,
	Count,
};

/**
 * Local (per player) settings: rates, tune, radio mapping, camera, keys.
 * Stored as JSON in Saved/FPVDrone/Settings.json so they survive game updates and
 * can be shared between players.
 */
class FPVDRONE_API FFPVSettings
{
public:
	static FFPVConfig& Get();
	static void Load();
	static void Save();
	static FString GetFilePath();

	// Broadcast after Save() or any change applied through the menu.
	static FSimpleMulticastDelegate OnChanged;

	static FPVSim::Params ToSimParams(const FFPVConfig& Config);

	static void ApplyPreset(FFPVConfig& Config, EFPVPreset Preset);
	static FText GetPresetName(EFPVPreset Preset);

	/**
	 * Applies a Betaflight CLI "diff all" / "dump" (or just a few "set x = y" lines).
	 * Supports rates, PIDs, feedforward, throttle, TPA, filters, angle/horizon and 3D settings.
	 * @return number of settings applied; OutReport lists what was applied or ignored.
	 */
	static int32 ImportBetaflightCli(FFPVConfig& Config, const FString& Text, FString& OutReport);

	// Exports the tune as Betaflight CLI lines, e.g. to share it.
	static FString ExportBetaflightCli(const FFPVConfig& Config);

	static FKey ParseKey(const FString& Name);

	// Makes sure arrays have the expected size after loading older/hand-edited files.
	static void Sanitize(FFPVConfig& Config);

private:
	static bool bLoaded;
};
