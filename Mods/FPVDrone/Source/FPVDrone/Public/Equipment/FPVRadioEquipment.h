// The radio (transmitter + goggles) as a hand tool like the Chainsaw or Xeno-Zapper.
// Holding it, left click launches the drone from the inventory or puts the goggles on/off.
// The click itself is read in UFPVLocalSubsystem, so this class needs no input setup.
//
// The C++ class has no mesh. For a visible radio in the hand create a Blueprint child
// (see docs/SETUP.md) at FPVContentPaths::RadioEquipmentBlueprint; it is picked up automatically.

#pragma once

#include "CoreMinimal.h"
#include "Equipment/FGEquipment.h"
#include "FPVRadioEquipment.generated.h"

UCLASS()
class FPVDRONE_API AFPVRadioEquipment : public AFGEquipment
{
	GENERATED_BODY()

public:
	AFPVRadioEquipment();
};
