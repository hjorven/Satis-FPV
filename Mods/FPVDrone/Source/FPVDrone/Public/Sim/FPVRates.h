// Stick -> rotation rate curves, matching Betaflight's rate types.

#pragma once

#include "Sim/FPVSimMath.h"

namespace FPVSim
{
	enum class RatesType : uint8_t
	{
		Betaflight = 0,
		Actual = 1,
		Quick = 2,
		Kiss = 3,
	};

	// Meaning of A/B/C depends on the rate type (same numbers as the Betaflight Configurator shows):
	//   Betaflight: A = RC Rate (1.00), B = Super Rate (0.70), C = RC Expo (0.00)
	//   Actual:     A = Center Sensitivity in deg/s (70), B = Max Rate in deg/s (670), C = Expo (0.54)
	//   Quick:      A = RC Rate (1.00 -> 200 deg/s center), B = Max Rate in deg/s (670), C = Expo (0.00)
	//   KISS:       A = RC Rate (1.00), B = Rate (0.70), C = RC Curve (0.00)
	struct AxisRates
	{
		float A = 1.0f;
		float B = 0.7f;
		float C = 0.0f;
	};

	constexpr float SetpointRateLimit = 1998.0f;

	// Stick in [-1, 1] -> rate in deg/s.
	float ApplyRates(RatesType Type, const AxisRates& R, float Stick);

	// Rate at full stick deflection in deg/s.
	inline float MaxRate(RatesType Type, const AxisRates& R) { return ApplyRates(Type, R, 1.0f); }

	// Removes a centre deadband and rescales the rest back to [-1, 1].
	inline float ApplyDeadband(float Stick, float Deadband)
	{
		const float A = std::fabs(Stick);
		if (A <= Deadband)
		{
			return 0.0f;
		}
		return Sign(Stick) * Min(1.0f, (A - Deadband) / (1.0f - Deadband));
	}

	// Betaflight throttle curve (thr_mid / thr_expo), input and output in [0, 1].
	inline float ApplyThrottleCurve(float Throttle, float Mid, float Expo)
	{
		const float T = Clamp(Throttle, 0.0f, 1.0f);
		const float D = T - Mid;
		const float Y = D > 0.0f ? (1.0f - Mid) : Mid;
		if (Y <= 1e-6f)
		{
			return T;
		}
		return Clamp(Mid + D * (1.0f - Expo + Expo * (D * D) / (Y * Y)), 0.0f, 1.0f);
	}
}
