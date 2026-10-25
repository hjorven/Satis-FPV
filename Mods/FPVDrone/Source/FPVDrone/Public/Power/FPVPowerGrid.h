// Finds powered parts of the player's power grid near the drone, like the Hoverpack does.
// Server only: the result decides whether the drone battery may charge.

#pragma once

#include "CoreMinimal.h"

class UWorld;

namespace FPVPowerGrid
{
	// Charger range around power poles, wall outlets and power lines.
	constexpr float ChargeRadiusCm = 3000.0f;

	// Seconds from empty to full while in range.
	constexpr float SecondsToFull = 20.0f;

	// True if a power pole, outlet or power line within RadiusCm belongs to a circuit that has
	// generators running and an intact fuse.
	FPVDRONE_API bool IsNearPoweredGrid(UWorld* World, const FVector& Location, float RadiusCm = ChargeRadiusCm);
}
