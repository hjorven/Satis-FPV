// The simple settings in Satisfactory's own mod menu (ESC -> Mods -> FPV Drone).
//
// The full setup (rates, PID, radio calibration, Betaflight import) stays in the F10 menu,
// because SML config pages only offer plain switches and number fields. Both menus point to
// each other. Values are synced both ways with FFPVSettings, which stays the single source
// the drone reads from.

#pragma once

#include "CoreMinimal.h"
#include "Configuration/ModConfiguration.h"
#include "FPVModConfig.generated.h"

class UGameInstance;

UCLASS()
class FPVDRONE_API UFPVModConfiguration : public UModConfiguration
{
	GENERATED_BODY()

public:
	UFPVModConfiguration();
};

namespace FPVModConfig
{
	// Registers the config page once per game instance.
	void Register(UGameInstance* GameInstance);

	// Copies changes from the ESC menu into FFPVSettings and the other way round.
	void Sync(UGameInstance* GameInstance);
}
