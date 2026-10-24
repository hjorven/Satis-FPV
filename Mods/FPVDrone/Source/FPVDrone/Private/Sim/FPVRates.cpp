#include "Sim/FPVRates.h"

namespace FPVSim
{
	namespace
	{
		float Power3(float V) { return V * V * V; }
		float Power5(float V) { return V * V * V * V * V; }

		// Ported from Betaflight src/main/fc/rc.c (applyBetaflightRates).
		float BetaflightRates(const AxisRates& R, float Cmd, float CmdAbs)
		{
			if (R.C != 0.0f)
			{
				Cmd = Cmd * Power3(CmdAbs) * R.C + Cmd * (1.0f - R.C);
			}
			float RcRate = R.A;
			if (RcRate > 2.0f)
			{
				RcRate += 14.54f * (RcRate - 2.0f);
			}
			float Angle = 200.0f * RcRate * Cmd;
			if (R.B != 0.0f)
			{
				Angle *= 1.0f / Clamp(1.0f - CmdAbs * R.B, 0.01f, 1.0f);
			}
			return Angle;
		}

		// applyActualRates
		float ActualRates(const AxisRates& R, float Cmd, float CmdAbs)
		{
			const float Expo = CmdAbs * (Power5(Cmd) * R.C + Cmd * (1.0f - R.C));
			const float Center = R.A;
			const float StickMovement = Max(0.0f, R.B - Center);
			return Cmd * Center + StickMovement * Expo;
		}

		// applyQuickRates
		float QuickRates(const AxisRates& R, float Cmd, float CmdAbs)
		{
			const float RcRate = R.A * 200.0f;
			const float MaxDps = Max(R.B, RcRate);
			const float SuperFactorConfig = (MaxDps / RcRate - 1.0f) / (MaxDps / RcRate);
			const float Curve = Power3(CmdAbs) * R.C + CmdAbs * (1.0f - R.C);
			const float SuperFactor = 1.0f / Clamp(1.0f - Curve * SuperFactorConfig, 0.01f, 1.0f);
			return Cmd * RcRate * SuperFactor;
		}

		// applyKissRates
		float KissRates(const AxisRates& R, float Cmd, float CmdAbs)
		{
			const float UseRates = 1.0f / Clamp(1.0f - CmdAbs * R.B, 0.01f, 1.0f);
			const float KissCmd = (Power3(Cmd) * R.C + Cmd * (1.0f - R.C)) * (R.A / 10.0f);
			return 2000.0f * UseRates * KissCmd;
		}
	}

	float ApplyRates(RatesType Type, const AxisRates& R, float Stick)
	{
		const float Cmd = Clamp(Stick, -1.0f, 1.0f);
		const float CmdAbs = std::fabs(Cmd);
		float Rate = 0.0f;
		switch (Type)
		{
		case RatesType::Actual: Rate = ActualRates(R, Cmd, CmdAbs); break;
		case RatesType::Quick: Rate = QuickRates(R, Cmd, CmdAbs); break;
		case RatesType::Kiss: Rate = KissRates(R, Cmd, CmdAbs); break;
		case RatesType::Betaflight:
		default: Rate = BetaflightRates(R, Cmd, CmdAbs); break;
		}
		return Clamp(Rate, -SetpointRateLimit, SetpointRateLimit);
	}
}
