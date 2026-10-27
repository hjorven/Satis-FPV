#include "Content/FPVContent.h"

#include "Content/FPVContentPaths.h"
#include "Equipment/FPVRadioEquipment.h"
#include "Engine/Texture2D.h"
#include "FPVDroneModule.h"
#include "ItemAmount.h"

#define LOCTEXT_NAMESPACE "FPVDrone"

namespace FPVContentLocal
{
	TSubclassOf<UFGItemDescriptor> LoadItem(const TCHAR* Path)
	{
		UClass* Class = LoadClass<UFGItemDescriptor>(nullptr, Path);
		if (Class == nullptr)
		{
			UE_LOG(LogFPVDrone, Error, TEXT("Missing FactoryGame item %s - check Content/FPVContentPaths.h"), Path);
		}
		return Class;
	}

	UTexture2D* LoadTexture(const TCHAR* Path)
	{
		return LoadObject<UTexture2D>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	struct FIngredient { const TCHAR* Path; int32 Amount; };

	void SetIngredients(TArray<FItemAmount>& Out, std::initializer_list<FIngredient> Ingredients)
	{
		if (Out.Num() > 0)
		{
			return;
		}
		for (const FIngredient& Ingredient : Ingredients)
		{
			if (const TSubclassOf<UFGItemDescriptor> Item = LoadItem(Ingredient.Path))
			{
				Out.Add(FItemAmount(Item, Ingredient.Amount));
			}
		}
	}
}

using namespace FPVContentLocal;

// --- Item -----------------------------------------------------------------------------

UFPVItemDescriptor_Drone::UFPVItemDescriptor_Drone()
{
	mDisplayName = LOCTEXT("DroneName", "FPV Drone");
	mDescription = LOCTEXT("DroneDesc", "A 5\" freestyle quadcopter.\n\nHold the FPV Radio and left click to launch it and put the goggles on, left click again to take them off (shortcut: K). Look at the drone and press E to pick it up. F10 opens the setup (rates, tune, radio).\n\nThe battery charges near powered power poles and power lines, like the Hoverpack.");
	mStackSize = EStackSize::SS_ONE;
	mForm = EResourceForm::RF_SOLID;
	// Pure C++ descriptors never run PostLoad(), which is what caches the stack size
	// from mStackSize. Without it GetStackSize() returns -1 and the inventory rejects
	// the item with "not enough space". Cache the SS_ONE value (1) directly.
	mCachedStackSize = 1;
}

void UFPVItemDescriptor_Drone::PatchDefaults()
{
	UFPVItemDescriptor_Drone* CDO = GetMutableDefault<UFPVItemDescriptor_Drone>();
	if (UTexture2D* Big = LoadTexture(FPVContentPaths::IconBig))
	{
		CDO->mPersistentBigIcon = Big;
	}
	if (UTexture2D* Small = LoadTexture(FPVContentPaths::IconSmall))
	{
		CDO->mSmallIcon = Small;
	}
}

// --- Radio (hand tool) --------------------------------------------------------------------

UFPVItemDescriptor_Radio::UFPVItemDescriptor_Radio()
{
	mDisplayName = LOCTEXT("RadioName", "FPV Radio");
	mDescription = LOCTEXT("RadioDesc", "Radio and goggles for the FPV Drone. Equip it in the hand slot.\n\nLeft click: launch a drone from the inventory, or put the goggles on/off.");
	mStackSize = EStackSize::SS_ONE;
	mForm = EResourceForm::RF_SOLID;
	mEquipmentClass = AFPVRadioEquipment::StaticClass();
	// See UFPVItemDescriptor_Drone: cache the SS_ONE stack size for the C++ CDO.
	mCachedStackSize = 1;
}

void UFPVItemDescriptor_Radio::PatchDefaults()
{
	UFPVItemDescriptor_Radio* CDO = GetMutableDefault<UFPVItemDescriptor_Radio>();
	if (UClass* Blueprint = LoadClass<AFPVRadioEquipment>(nullptr, FPVContentPaths::RadioEquipmentBlueprint, nullptr, LOAD_NoWarn | LOAD_Quiet))
	{
		CDO->mEquipmentClass = Blueprint;
	}
	UTexture2D* Big = LoadTexture(FPVContentPaths::IconRadioBig);
	UTexture2D* Small = LoadTexture(FPVContentPaths::IconRadioSmall);
	// Until the radio has its own icons, use the drone's.
	CDO->mPersistentBigIcon = Big != nullptr ? Big : LoadTexture(FPVContentPaths::IconBig);
	CDO->mSmallIcon = Small != nullptr ? Small : LoadTexture(FPVContentPaths::IconSmall);
}

UFPVRecipe_Radio::UFPVRecipe_Radio()
{
	mDisplayName = LOCTEXT("RadioRecipeName", "FPV Radio");
	mProduct.Add(FItemAmount(UFPVItemDescriptor_Radio::StaticClass(), 1));
	mManufactoringDuration = 40.0f;
	mProducedIn.Add(TSoftClassPtr<UObject>(FSoftObjectPath(FPVContentPaths::EquipmentWorkshop)));
}

void UFPVRecipe_Radio::PatchDefaults()
{
	UFPVRecipe_Radio* CDO = GetMutableDefault<UFPVRecipe_Radio>();
	SetIngredients(CDO->mIngredients, {
		{ FPVContentPaths::ItemCircuitBoard, 1 },
		{ FPVContentPaths::ItemQuickwire, 10 },
		{ FPVContentPaths::ItemRubber, 4 },
	});
}

// --- Recipe -----------------------------------------------------------------------------

UFPVRecipe_Drone::UFPVRecipe_Drone()
{
	mDisplayName = LOCTEXT("RecipeName", "FPV Drone");
	mProduct.Add(FItemAmount(UFPVItemDescriptor_Drone::StaticClass(), 1));
	mManufactoringDuration = 60.0f;
	mProducedIn.Add(TSoftClassPtr<UObject>(FSoftObjectPath(FPVContentPaths::EquipmentWorkshop)));
}

void UFPVRecipe_Drone::PatchDefaults()
{
	UFPVRecipe_Drone* CDO = GetMutableDefault<UFPVRecipe_Drone>();
	// Flight controller + VTX, wiring, motors, frame/props/LiPo pouch. Mid-game parts only, so the
	// drone is buildable as soon as Caterium and Rubber run (no Battery parts needed).
	SetIngredients(CDO->mIngredients, {
		{ FPVContentPaths::ItemCircuitBoard, 3 },
		{ FPVContentPaths::ItemQuickwire, 25 },
		{ FPVContentPaths::ItemRotor, 2 },
		{ FPVContentPaths::ItemRubber, 10 },
	});
}

// --- Unlock ----------------------------------------------------------------------------

UFPVUnlockRecipe_Drone::UFPVUnlockRecipe_Drone()
{
	mRecipes.Add(UFPVRecipe_Drone::StaticClass());
	mRecipes.Add(UFPVRecipe_Radio::StaticClass());
}

// --- Schematic ---------------------------------------------------------------------------

UFPVSchematic_Drone::UFPVSchematic_Drone()
{
	mType = ESchematicType::EST_ResourceSink;
	mDisplayName = LOCTEXT("SchematicName", "FPV Drone");
	mTimeToComplete = 0.0f;
	mMenuPriority = 100.0f;
	mUnlocks.Add(CreateDefaultSubobject<UFPVUnlockRecipe_Drone>(TEXT("UnlockDroneRecipe")));
}

void UFPVSchematic_Drone::PatchDefaults()
{
	UFPVSchematic_Drone* CDO = GetMutableDefault<UFPVSchematic_Drone>();
	if (CDO->mCost.Num() == 0)
	{
		if (const TSubclassOf<UFGItemDescriptor> Coupon = LoadItem(FPVContentPaths::ItemCoupon))
		{
			CDO->mCost.Add(FItemAmount(Coupon, 10));
		}
	}
	if (UTexture2D* Icon = LoadTexture(FPVContentPaths::IconBig))
	{
		CDO->mSchematicIcon.SetResourceObject(Icon);
		CDO->mSchematicIcon.ImageSize = FVector2D(256.0, 256.0);
	}
}

void FPVContent::PatchDefaults()
{
	UFPVItemDescriptor_Drone::PatchDefaults();
	UFPVItemDescriptor_Radio::PatchDefaults();
	UFPVRecipe_Drone::PatchDefaults();
	UFPVRecipe_Radio::PatchDefaults();
	UFPVSchematic_Drone::PatchDefaults();
}

#undef LOCTEXT_NAMESPACE
