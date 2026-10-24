#pragma once

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "Modules/ModuleManager.h"

FPVDRONE_API DECLARE_LOG_CATEGORY_EXTERN(LogFPVDrone, Log, All);

class FFPVDroneModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

private:
	void OnWorldInitializedActors(const UWorld::FActorsInitializedParams& Params);

	FDelegateHandle WorldInitHandle;
};
