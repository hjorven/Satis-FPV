#include "FPVGameWorldModule.h"

#include "Content/FPVContent.h"

UFPVGameWorldModule::UFPVGameWorldModule()
{
	bRootModule = true;
	mSchematics.Add(UFPVSchematic_Drone::StaticClass());
}

void UFPVGameWorldModule::DispatchLifecycleEvent(ELifecyclePhase Phase)
{
	if (Phase == ELifecyclePhase::CONSTRUCTION)
	{
		// Fill in FactoryGame references before SML registers the schematic.
		FPVContent::PatchDefaults();
	}
	Super::DispatchLifecycleEvent(Phase);
}
