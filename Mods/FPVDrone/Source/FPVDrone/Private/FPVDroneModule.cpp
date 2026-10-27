#include "FPVDroneModule.h"

#include "Engine/World.h"
#include "FPVPilotLink.h"
#include "FPVSettings.h"
#include "Input/FPVHIDDevice.h"

DEFINE_LOG_CATEGORY(LogFPVDrone);

void FFPVDroneModule::StartupModule()
{
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddRaw(this, &FFPVDroneModule::OnWorldInitializedActors);

	if (!IsRunningDedicatedServer())
	{
		FFPVSettings::Get();
		FFPVHIDManager::Get().Start();
	}
}

void FFPVDroneModule::ShutdownModule()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	FFPVHIDManager::Get().Stop();
}

void FFPVDroneModule::OnWorldInitializedActors(const UWorld::FActorsInitializedParams& Params)
{
	UWorld* World = Params.World;
	if (World == nullptr || !World->IsGameWorld() || World->GetNetMode() == NM_Client)
	{
		return;
	}
	// One manager per server world; it hands every player controller a pilot link for RPCs.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnParams.ObjectFlags |= RF_Transient;
	World->SpawnActor<AFPVServerManager>(AFPVServerManager::StaticClass(), FTransform::Identity, SpawnParams);
}

IMPLEMENT_MODULE(FFPVDroneModule, FPVDrone)
