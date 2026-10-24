// Root game world module. SML only discovers Blueprint modules, so create an empty Blueprint
// child of this class named RootGameWorld_FPVDrone in the plugin content (see docs/SETUP.md).

#pragma once

#include "CoreMinimal.h"
#include "Module/GameWorldModule.h"
#include "FPVGameWorldModule.generated.h"

UCLASS(Abstract, Blueprintable)
class FPVDRONE_API UFPVGameWorldModule : public UGameWorldModule
{
	GENERATED_BODY()

public:
	UFPVGameWorldModule();

	virtual void DispatchLifecycleEvent(ELifecyclePhase Phase) override;
};
