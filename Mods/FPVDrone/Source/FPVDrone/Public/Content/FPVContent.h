// Item, recipe and AWESOME Shop schematic, defined in C++ so the mod works without
// Blueprint assets. Blueprint children can override icons, costs and the shop category.

#pragma once

#include "CoreMinimal.h"
#include "FGRecipe.h"
#include "FGSchematic.h"
#include "Resources/FGEquipmentDescriptor.h"
#include "Resources/FGItemDescriptor.h"
#include "Unlocks/FGUnlockRecipe.h"
#include "FPVContent.generated.h"

UCLASS()
class FPVDRONE_API UFPVItemDescriptor_Drone : public UFGItemDescriptor
{
	GENERATED_BODY()

public:
	UFPVItemDescriptor_Drone();
	static void PatchDefaults();
};

// The radio is the hand tool (equipment slot "arms") used to launch and fly the drone.
UCLASS()
class FPVDRONE_API UFPVItemDescriptor_Radio : public UFGEquipmentDescriptor
{
	GENERATED_BODY()

public:
	UFPVItemDescriptor_Radio();
	static void PatchDefaults();
};

UCLASS()
class FPVDRONE_API UFPVRecipe_Radio : public UFGRecipe
{
	GENERATED_BODY()

public:
	UFPVRecipe_Radio();
	static void PatchDefaults();
};

UCLASS()
class FPVDRONE_API UFPVRecipe_Drone : public UFGRecipe
{
	GENERATED_BODY()

public:
	UFPVRecipe_Drone();
	static void PatchDefaults();
};

UCLASS()
class FPVDRONE_API UFPVUnlockRecipe_Drone : public UFGUnlockRecipe
{
	GENERATED_BODY()

public:
	UFPVUnlockRecipe_Drone();
};

// Sold in the AWESOME Shop for FICSIT Coupons.
UCLASS()
class FPVDRONE_API UFPVSchematic_Drone : public UFGSchematic
{
	GENERATED_BODY()

public:
	UFPVSchematic_Drone();
	static void PatchDefaults();
};

namespace FPVContent
{
	// Loads FactoryGame classes into the CDOs above. Safe to call more than once.
	FPVDRONE_API void PatchDefaults();
}
