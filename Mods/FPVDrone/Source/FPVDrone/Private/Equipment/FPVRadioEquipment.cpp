#include "Equipment/FPVRadioEquipment.h"

#include "CharacterAnimationTypes.h"
#include "Components/SceneComponent.h"

AFPVRadioEquipment::AFPVRadioEquipment()
{
	mEquipmentSlot = EEquipmentSlot::ES_ARMS;
	// Hold the radio with both hands instead of leaving the arms in the default empty pose, which
	// is why the radio looked "unheld". A visible radio mesh in the hands additionally needs an
	// FGEquipmentChild (set up as the Equip_FPVRadio blueprint); the pose already helps on its own.
	mArmAnimation = EArmEquipment::AE_Generic2Hand;
	// A C++ AActor has no RootComponent by default, unlike a Blueprint which always gets
	// a DefaultSceneRoot. AFGEquipment::Equip() attaches the equipment through its root, so
	// without one the game dereferences a null pointer and crashes when the radio is equipped.
	// Provide an (invisible) scene root; a Blueprint child can still add the radio mesh under it.
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent")));
}
