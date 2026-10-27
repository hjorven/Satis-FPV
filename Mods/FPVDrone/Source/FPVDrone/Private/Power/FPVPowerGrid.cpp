#include "Power/FPVPowerGrid.h"

#include "Buildables/FGBuildablePowerPole.h"
#include "Buildables/FGBuildableWire.h"
#include "EngineUtils.h"
#include "FGCircuitConnectionComponent.h"
#include "FGPowerCircuit.h"
#include "FGPowerConnectionComponent.h"

namespace FPVPowerGridLocal
{
	bool IsPowered(const UFGCircuitConnectionComponent* Connection)
	{
		const UFGPowerConnectionComponent* PowerConnection = Cast<UFGPowerConnectionComponent>(Connection);
		if (PowerConnection == nullptr)
		{
			return false;
		}
		UFGPowerCircuit* Circuit = PowerConnection->GetPowerCircuit();
		if (Circuit == nullptr || Circuit->IsFuseTriggered())
		{
			return false;
		}
		FPowerCircuitStats Stats;
		Circuit->GetStats(Stats);
		return Stats.PowerProductionCapacity > 0.0f;
	}

	bool AnyPowered(const AActor* Actor)
	{
		TInlineComponentArray<UFGPowerConnectionComponent*> Connections(Actor);
		for (const UFGPowerConnectionComponent* Connection : Connections)
		{
			if (IsPowered(Connection))
			{
				return true;
			}
		}
		return false;
	}
}

using namespace FPVPowerGridLocal;

bool FPVPowerGrid::IsNearPoweredGrid(UWorld* World, const FVector& Location, float RadiusCm)
{
	if (World == nullptr)
	{
		return false;
	}
	const double RadiusSquared = FMath::Square(static_cast<double>(RadiusCm));

	// Poles and wall outlets.
	for (TActorIterator<AFGBuildablePowerPole> It(World); It; ++It)
	{
		if (FVector::DistSquared(It->GetActorLocation(), Location) <= RadiusSquared && AnyPowered(*It))
		{
			return true;
		}
	}

	// Power lines: distance to the straight segment between both ends (sag is ignored).
	for (TActorIterator<AFGBuildableWire> It(World); It; ++It)
	{
		const UFGCircuitConnectionComponent* A = It->GetConnection(0);
		const UFGCircuitConnectionComponent* B = It->GetConnection(1);
		if (A == nullptr || B == nullptr)
		{
			continue;
		}
		const FVector Closest = FMath::ClosestPointOnSegment(Location, A->GetComponentLocation(), B->GetComponentLocation());
		if (FVector::DistSquared(Closest, Location) <= RadiusSquared && (IsPowered(A) || IsPowered(B)))
		{
			return true;
		}
	}
	return false;
}
