#include "Content/FPVContent.h"

#include "Content/FPVContentPaths.h"
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
}

using namespace FPVContentLocal;

// --- Item -----------------------------------------------------------------------------

UFPVItemDescriptor_Drone::UFPVItemDescriptor_Drone()
{
	mDisplayName = LOCTEXT("DroneName", "FPV Drone");
	mDescription = LOCTEXT("DroneDesc", "A 5\" freestyle quadcopter with goggles and radio link.\n\nPress K to launch it and put the goggles on, K again to take them off. Press L next to it to pick it up. F10 opens the setup (rates, tune, radio).");
	mStackSize = EStackSize::SS_ONE;
	mForm = EResourceForm::RF_SOLID;
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

// --- Recipe -----------------------------------------------------------------------------

UFPVRecipe_Drone::UFPVRecipe_Drone()
{
	mDisplayName = LOCTEXT("RecipeName", "FPV Drone");
	mProduct.Add(FItemAmount(UFPVItemDescriptor_Drone::StaticClass(), 1));
	mManufactoringDuration = 6.0f;
	mProducedIn.Add(TSoftClassPtr<UObject>(FSoftObjectPath(FPVContentPaths::EquipmentWorkshop)));
}

void UFPVRecipe_Drone::PatchDefaults()
{
	UFPVRecipe_Drone* CDO = GetMutableDefault<UFPVRecipe_Drone>();
	if (CDO->mIngredients.Num() > 0)
	{
		return;
	}
	// Frame, 4 motors, flight controller, VTX, battery.
	struct FIngredient { const TCHAR* Path; int32 Amount; };
	const FIngredient Ingredients[] = {
		{ FPVContentPaths::ItemCircuitBoard, 2 },
		{ FPVContentPaths::ItemQuickwire, 20 },
		{ FPVContentPaths::ItemRotor, 1 },
		{ FPVContentPaths::ItemBattery, 1 },
	};
	for (const FIngredient& Ingredient : Ingredients)
	{
		if (const TSubclassOf<UFGItemDescriptor> Item = LoadItem(Ingredient.Path))
		{
			CDO->mIngredients.Add(FItemAmount(Item, Ingredient.Amount));
		}
	}
}

// --- Unlock ----------------------------------------------------------------------------

UFPVUnlockRecipe_Drone::UFPVUnlockRecipe_Drone()
{
	mRecipes.Add(UFPVRecipe_Drone::StaticClass());
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
	UFPVRecipe_Drone::PatchDefaults();
	UFPVSchematic_Drone::PatchDefaults();
}

#undef LOCTEXT_NAMESPACE
