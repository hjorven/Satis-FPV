// Per-player actor that carries the drone RPCs (launch, goggles on/off, pick up).
// Spawned by AFPVServerManager for every player controller, only relevant to its owner,
// so the mod does not need to hook FactoryGame's player controller.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FPVPilotLink.generated.h"

class AFPVDroneActor;
class APlayerController;

UCLASS(NotPlaceable)
class FPVDRONE_API AFPVPilotLink : public AActor
{
	GENERATED_BODY()

public:
	AFPVPilotLink();

	// Launch a drone from the inventory, or put the goggles on/off for your drone.
	UFUNCTION(Server, Reliable)
	void Server_ToggleGoggles();

	// Put the nearest drone (within reach) back into the inventory.
	UFUNCTION(Server, Reliable)
	void Server_PickUp();

	UFUNCTION(Client, Reliable)
	void Client_Notify(const FText& Message);

	static AFPVPilotLink* FindLocal(const UWorld* World);

private:
	APlayerController* GetPC() const;
	AFPVDroneActor* FindOwnedDrone() const;
	AFPVDroneActor* FindNearestDrone(float MaxDistanceCm, bool bOnlyFree) const;
	void LaunchFromInventory();
};

// Server only: makes sure every player controller has a pilot link.
UCLASS(NotPlaceable, Transient)
class FPVDRONE_API AFPVServerManager : public AActor
{
	GENERATED_BODY()

public:
	AFPVServerManager();
	virtual void Tick(float DeltaSeconds) override;

private:
	// Links are kept alive by the world; weak pointers on both sides are enough.
	TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<AFPVPilotLink>> Links;
};
