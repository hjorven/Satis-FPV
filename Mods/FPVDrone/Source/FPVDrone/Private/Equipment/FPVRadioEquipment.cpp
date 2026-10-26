#include "Equipment/FPVRadioEquipment.h"

#include "Components/SceneComponent.h"

AFPVRadioEquipment::AFPVRadioEquipment()
{
	mEquipmentSlot = EEquipmentSlot::ES_ARMS;
	// A C++ AActor has no RootComponent by default, unlike a Blueprint which always gets
	// a DefaultSceneRoot. AFGEquipment::Equip() attaches the equipment through its root, so
	// without one the game dereferences a null pointer and crashes when the radio is equipped.
	// Provide an (invisible) scene root; a Blueprint child can still add the radio mesh under it.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent")));
}
