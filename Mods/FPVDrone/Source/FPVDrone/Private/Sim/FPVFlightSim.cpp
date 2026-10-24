#include "Sim/FPVFlightSim.h"

namespace FPVSim
{
	namespace
	{
		// Betaflight PID scaling (src/main/flight/pid.h)
		constexpr float PTermScale = 0.032029f;
		constexpr float ITermScale = 0.244381f;
		constexpr float DTermScale = 0.000529f;
		constexpr float FeedforwardScale = 0.013754f;
		constexpr float PidMixerScaling = 1000.0f;

		// Motor positions as sign of (x, y) and the sign of the reaction torque around +Z.
		// CCW props push the frame clockwise (negative Z torque).
		constexpr float MotorX[4] = { 1, 1, -1, -1 };
		constexpr float MotorY[4] = { 1, -1, 1, -1 };
		constexpr float MotorSpin[4] = { -1, 1, 1, -1 };

		// Per-cell open circuit voltage over state of charge (0, 0.1, ..., 1.0).
		constexpr float CellOcv[11] = { 3.30f, 3.60f, 3.68f, 3.73f, 3.77f, 3.81f, 3.86f, 3.92f, 4.00f, 4.09f, 4.20f };

		float MotorThrustFromOutput(const Params& P, float Output, float VoltageScale)
		{
			const float Effective = Output * VoltageScale;
			const float Thrust = P.MotorMaxThrust * Effective * std::fabs(Effective);
			return Thrust < 0.0f ? Thrust * P.ReverseThrustEfficiency : Thrust;
		}

		// Setpoints in deg/s on the stick axes (roll right, pitch forward, yaw right).
		void ComputeSetpoints(const Params& P, const Input& In, State& S)
		{
			const float Raw[3] = {
				ApplyDeadband(In.Roll, P.StickDeadband),
				ApplyDeadband(In.Pitch, P.StickDeadband),
				ApplyDeadband(In.Yaw, P.YawDeadband)
			};

			// RC smoothing: the radio link updates at a few hundred Hz, Betaflight smooths the steps.
			float Sticks[3];
			for (int A = 0; A < 3; ++A)
			{
				const float First = S.RcSmoothing[A][0].Apply(Raw[A], P.RcSmoothingHz, StepDt);
				Sticks[A] = S.RcSmoothing[A][1].Apply(First, P.RcSmoothingHz, StepDt);
			}

			for (int A = 0; A < 3; ++A)
			{
				S.Setpoint[A] = ApplyRates(P.Rates, P.AxisRate[A], Sticks[A]);
			}

			if (In.Mode == FlightMode::Acro)
			{
				return;
			}

			float RollAngle, PitchAngle;
			S.Rot.ToRollPitch(RollAngle, PitchAngle);
			const float Current[2] = { RollAngle * RadToDeg, PitchAngle * RadToDeg };

			if (In.Mode == FlightMode::Angle)
			{
				const float Gain = P.AngleStrength / 10.0f;
				for (int A = 0; A < 2; ++A)
				{
					const float Target = Sticks[A] * P.AngleLimitDeg;
					S.Setpoint[A] = Clamp((Target - Current[A]) * Gain, -SetpointRateLimit, SetpointRateLimit);
				}
			}
			else // Horizon: acro rates plus levelling that fades out with stick deflection
			{
				const float Deflection = Max(std::fabs(Sticks[0]), std::fabs(Sticks[1]));
				const float Level = Clamp(1.0f - Deflection / Max(P.HorizonTransition, 0.01f), 0.0f, 1.0f);
				const float Gain = P.HorizonStrength / 10.0f;
				for (int A = 0; A < 2; ++A)
				{
					S.Setpoint[A] += -Current[A] * Gain * Level;
				}
			}
		}

		// Returns throttle in 0..1 (normal) or -1..1 (3D).
		float ComputeThrottle(const Params& P, const Input& In)
		{
			if (In.Mode3D)
			{
				float T = Clamp(In.Throttle, 0.0f, 1.0f) * 2.0f - 1.0f;
				T = ApplyDeadband(T, P.Deadband3D);
				return T * P.ThrottleLimit;
			}
			return ApplyThrottleCurve(In.Throttle, P.ThrottleMid, P.ThrottleExpo) * P.ThrottleLimit;
		}

		void RunPid(const Params& P, State& S, float Throttle, float Dt)
		{
			const float ThrottleAbs = std::fabs(Throttle);
			float Tpa = 1.0f;
			if (ThrottleAbs > P.TpaBreakpoint && P.TpaBreakpoint < 1.0f)
			{
				Tpa = 1.0f - P.TpaRate * (ThrottleAbs - P.TpaBreakpoint) / (1.0f - P.TpaBreakpoint);
			}

			// Without airmode Betaflight keeps the I-term from winding up at zero throttle.
			const bool AllowIterm = P.Airmode || ThrottleAbs > 0.05f;

			// Gyro in deg/s mapped to stick axes (yaw right = negative Z rotation).
			const float GyroRaw[3] = { S.Omega.X * RadToDeg, S.Omega.Y * RadToDeg, -S.Omega.Z * RadToDeg };

			for (int A = 0; A < 3; ++A)
			{
				const PidGains& G = P.Pid[A];
				const float Noise = P.GyroNoiseDegPerSec * S.Random.NextSigned();
				const float Gyro = S.GyroFilter[A].Apply(GyroRaw[A] + Noise, P.GyroLpfHz, Dt);
				const float Error = S.Setpoint[A] - Gyro;

				// P
				const float PTerm = G.P * PTermScale * Error * (A == Yaw ? 1.0f : Tpa);

				// D on measurement
				const float DGyro = S.DtermFilter[A].Apply(Gyro, P.DtermLpfHz, Dt);
				const float DTerm = -G.D * DTermScale * (DGyro - S.PrevDterm[A]) / Dt * Tpa;
				S.PrevDterm[A] = DGyro;

				// Feedforward on setpoint derivative, smoothed
				const float SetpointDelta = (S.Setpoint[A] - S.PrevSetpoint[A]) / Dt;
				S.PrevSetpoint[A] = S.Setpoint[A];
				const float FTerm = Clamp(G.F * FeedforwardScale * S.FeedforwardFilter[A].Apply(SetpointDelta, P.FeedforwardSmoothHz, Dt) * 0.01f,
					-P.PidSumLimit, P.PidSumLimit);

				const float Limit = A == Yaw ? P.PidSumLimitYaw : P.PidSumLimit;

				// I with I-term relax (stops wind-up during fast stick moves, avoids bounce-back)
				float ItermError = Error;
				if (P.ItermRelaxCutoffHz > 0.0f)
				{
					const float Lowpassed = S.RelaxFilter[A].Apply(S.Setpoint[A], P.ItermRelaxCutoffHz, Dt);
					const float Highpassed = std::fabs(S.Setpoint[A] - Lowpassed);
					ItermError *= Max(0.0f, 1.0f - Highpassed / P.ItermRelaxThreshold);
				}
				// Anti-windup: do not integrate further while the output is saturated in the same direction.
				const float Unclamped = PTerm + S.Iterm[A] + DTerm + FTerm;
				const bool Saturated = std::fabs(Unclamped) >= Limit && Unclamped * ItermError > 0.0f;
				if (AllowIterm && !Saturated)
				{
					S.Iterm[A] = Clamp(S.Iterm[A] + G.I * ITermScale * ItermError * Dt, -P.ItermLimit, P.ItermLimit);
				}

				S.PidSum[A] = Clamp(PTerm + S.Iterm[A] + DTerm + FTerm, -Limit, Limit);
			}
		}

		void Mix(const Params& P, const Input& In, State& S, float Throttle)
		{
			const float U[3] = { S.PidSum[0] / PidMixerScaling, S.PidSum[1] / PidMixerScaling, S.PidSum[2] / PidMixerScaling };

			float Mixed[4];
			float MixMin = 1e9f, MixMax = -1e9f;
			for (int M = 0; M < 4; ++M)
			{
				// Roll right needs more thrust on the left (+Y), nose down needs more at the rear,
				// yaw right needs more thrust on the CCW motors.
				Mixed[M] = MotorY[M] * U[Roll] - MotorX[M] * U[Pitch] - MotorSpin[M] * U[Yaw];
				MixMin = Min(MixMin, Mixed[M]);
				MixMax = Max(MixMax, Mixed[M]);
			}

			const float Range = MixMax - MixMin;
			if (Range > 1.0f)
			{
				for (float& V : Mixed) { V /= Range; }
				MixMin /= Range;
				MixMax /= Range;
			}

			const float Lo = In.Mode3D ? -1.0f : 0.0f;
			float T = Throttle;
			if (P.Airmode)
			{
				// Shift throttle so the full differential fits into the motor range.
				T = Clamp(T, Lo - MixMin, 1.0f - MixMax);
			}

			for (int M = 0; M < 4; ++M)
			{
				float Cmd = Clamp(T + Mixed[M], Lo, 1.0f);
				if (!P.Airmode && !In.Mode3D && Throttle < 0.02f)
				{
					// Classic behaviour: no PID authority at zero throttle.
					Cmd = 0.0f;
				}
				// Spinning motors never go below idle; in 3D the deadband around zero is skipped.
				const float Mag = P.MotorIdle + (1.0f - P.MotorIdle) * std::fabs(Cmd);
				S.MotorCommand[M] = In.Mode3D ? (Cmd >= 0.0f ? Mag : -Mag) : Mag;
			}
		}

		void MixTurtle(const Params& P, const Input& In, State& S)
		{
			// Crash flip: spin individual motors in reverse to flip the craft over.
			const float RollStick = ApplyDeadband(In.Roll, 0.1f);
			const float PitchStick = ApplyDeadband(In.Pitch, 0.1f);
			for (int M = 0; M < 4; ++M)
			{
				const float Want = MotorY[M] * RollStick - MotorX[M] * PitchStick;
				// Only motors that push in reverse are used, like Betaflight's flip over after crash.
				S.MotorCommand[M] = Want < 0.0f ? Clamp(Want, -1.0f, 0.0f) * P.TurtlePower : 0.0f;
			}
		}

		void UpdateMotors(const Params& P, State& S, float Dt)
		{
			for (int M = 0; M < 4; ++M)
			{
				const float Target = S.MotorCommand[M];
				const float Current = S.MotorOutput[M];
				const bool SpinningUp = std::fabs(Target) > std::fabs(Current) && Target * Current >= 0.0f;
				const float Tau = SpinningUp ? P.MotorTimeConstantUp : P.MotorTimeConstantDown;
				S.MotorOutput[M] = Current + (Target - Current) * Min(1.0f, Dt / Max(Tau, 1e-4f));
			}
		}

		void UpdateBattery(const Params& P, State& S, float Dt)
		{
			const float DiskArea = Pi * 0.25f * P.PropDiameter * P.PropDiameter;
			const float PowerDenominator = P.FigureOfMerit * std::sqrt(2.0f * P.AirDensity * DiskArea) * P.MotorEfficiency;

			float Power = 0.0f;
			for (int M = 0; M < 4; ++M)
			{
				const float T = std::fabs(S.MotorThrust[M]);
				Power += T * std::sqrt(T) / PowerDenominator;
			}

			const float Ocv = OpenCircuitVoltage(P, S.StateOfCharge);
			const float Resistance = P.CellResistance * static_cast<float>(P.Cells);
			// Solve V = Ocv - I*R with I = Power/V + Iavionics (take the larger root).
			const float B = Ocv - Resistance * P.AvionicsCurrent;
			const float Disc = B * B - 4.0f * Resistance * Power;
			float Voltage = Disc > 0.0f ? 0.5f * (B + std::sqrt(Disc)) : 0.5f * B;
			Voltage = Max(Voltage, 0.5f * Ocv);

			S.Voltage = Voltage;
			S.Current = Power / Voltage + P.AvionicsCurrent;

			const float CapacityAh = Max(0.001f, P.CapacityMah * P.CapacityMultiplier / 1000.0f);
			S.StateOfCharge = Max(0.0f, S.StateOfCharge - S.Current * Dt / 3600.0f / CapacityAh);
			S.MahUsed += S.Current * Dt / 3.6f;
		}

		void IntegrateBody(const Params& P, State& S, float Throttle, float Dt)
		{
			const float FullVoltage = 4.2f * static_cast<float>(P.Cells);
			const float VoltageScale = Clamp(S.Voltage / FullVoltage, 0.0f, 1.0f);
			const float Arm = P.ArmLength * 0.70710678f;

			float TotalThrust = 0.0f;
			Vec3 Torque;
			for (int M = 0; M < 4; ++M)
			{
				const float T = MotorThrustFromOutput(P, S.MotorOutput[M], VoltageScale);
				S.MotorThrust[M] = T;
				TotalThrust += T;
				// r x F with r = (x, y, 0), F = (0, 0, T)
				Torque.X += MotorY[M] * Arm * T;
				Torque.Y += -MotorX[M] * Arm * T;
				Torque.Z += MotorSpin[M] * P.YawTorqueCoeff * std::fabs(T);

				// Reaction torque from spinning the rotor up or down.
				const float Speed = S.MotorOutput[M] * VoltageScale * P.MotorMaxRadPerSec;
				const float SpeedChange = std::fabs(Speed) - std::fabs(S.MotorSpeed[M]);
				Torque.Z += MotorSpin[M] * P.RotorInertia * SpeedChange / Dt;
				S.MotorSpeed[M] = Speed;
			}

			const Vec3 VelBody = S.Rot.Unrotate(S.Vel);

			// Propwash: descending into your own wash shakes the craft.
			if (P.PropwashStrength > 0.0f)
			{
				const float Descent = -VelBody.Z;
				const float Wash = Max(0.0f, Descent - 1.5f) * Clamp(std::fabs(Throttle), 0.0f, 1.0f) * P.PropwashStrength;
				for (int A = 0; A < 3; ++A)
				{
					const float Noise = S.PropwashFilter[A].Apply(S.Random.NextSigned(), 25.0f, Dt);
					const float Amp = (A == 2 ? 0.01f : 0.035f) * Wash;
					Torque[A] += Noise * Amp;
				}
			}

			Torque -= S.Omega * P.RotationalDrag;

			// Euler's rotation equation in the body frame.
			const Vec3 IOmega = S.Omega.Mul(P.Inertia);
			const Vec3 AngAcc = (Torque - S.Omega.Cross(IOmega)).Mul({ 1.0f / P.Inertia.X, 1.0f / P.Inertia.Y, 1.0f / P.Inertia.Z });
			S.Omega += AngAcc * Dt;

			// Forces in the body frame.
			Vec3 ForceBody(0.0f, 0.0f, TotalThrust);
			ForceBody.X -= 0.5f * P.AirDensity * P.DragArea.X * VelBody.X * std::fabs(VelBody.X);
			ForceBody.Y -= 0.5f * P.AirDensity * P.DragArea.Y * VelBody.Y * std::fabs(VelBody.Y);
			ForceBody.Z -= 0.5f * P.AirDensity * P.DragArea.Z * VelBody.Z * std::fabs(VelBody.Z);
			ForceBody.X -= P.PropHForce * std::fabs(TotalThrust) * VelBody.X;
			ForceBody.Y -= P.PropHForce * std::fabs(TotalThrust) * VelBody.Y;

			const Vec3 ForceWorld = S.Rot.Rotate(ForceBody) + Vec3(0.0f, 0.0f, -P.Gravity * P.Mass);
			S.Vel += ForceWorld * (Dt / P.Mass);
			S.Pos += S.Vel * Dt;
			S.Rot.IntegrateBodyRate(S.Omega, Dt);
		}
	}

	float OpenCircuitVoltage(const Params& P, float StateOfCharge)
	{
		const float X = Clamp(StateOfCharge, 0.0f, 1.0f) * 10.0f;
		const int I = static_cast<int>(Min(X, 9.0f));
		const float Cell = Lerp(CellOcv[I], CellOcv[I + 1], X - static_cast<float>(I));
		// An empty pack collapses quickly.
		const float Empty = StateOfCharge <= 0.0f ? 0.85f : 1.0f;
		return Cell * static_cast<float>(P.Cells) * Empty;
	}

	void ChargeBattery(const Params& P, State& S)
	{
		S.StateOfCharge = 1.0f;
		S.MahUsed = 0.0f;
		S.Voltage = OpenCircuitVoltage(P, 1.0f);
		S.Current = 0.0f;
	}

	void ApplyCraftScaling(Params& P, float Mass, float ArmLength, float PropInch, float DragScale)
	{
		const Params Ref;
		const float MassScale = Mass / Ref.Mass;
		const float ArmScale = ArmLength / Ref.ArmLength;
		const float PropScale = PropInch / 5.0f;
		P.Mass = Mass;
		P.ArmLength = ArmLength;
		P.Inertia = Ref.Inertia * (MassScale * ArmScale * ArmScale);
		P.RotorInertia = Ref.RotorInertia * PropScale * PropScale * PropScale * PropScale;
		P.MotorMaxRadPerSec = Ref.MotorMaxRadPerSec / Max(0.2f, PropScale);
		P.YawTorqueCoeff = Ref.YawTorqueCoeff * PropScale;
		P.DragArea = Ref.DragArea * (DragScale * MassScale);
		P.RotationalDrag = Ref.RotationalDrag * MassScale * ArmScale * ArmScale;
		P.PropDiameter = PropInch * 0.0254f;
		P.AvionicsCurrent = Ref.AvionicsCurrent * Min(1.0f, MassScale);
	}

	float HoverThrottle(const Params& P)
	{
		const float PerMotor = P.Mass * P.Gravity / 4.0f;
		const float Output = std::sqrt(Clamp(PerMotor / P.MotorMaxThrust, 0.0f, 1.0f));
		return Clamp((Output - P.MotorIdle) / (1.0f - P.MotorIdle), 0.0f, 1.0f);
	}

	void ResetController(State& S)
	{
		for (int A = 0; A < 3; ++A)
		{
			S.Iterm[A] = 0.0f;
			S.PidSum[A] = 0.0f;
			S.PrevSetpoint[A] = S.Setpoint[A];
			S.FeedforwardFilter[A].State = 0.0f;
		}
	}

	void Step(const Params& P, const Input& In, State& S)
	{
		const float Dt = StepDt;
		S.Time += Dt;

		const float Throttle = ComputeThrottle(P, In);
		S.ThrottleOut = Throttle;

		if (!In.Armed)
		{
			for (int A = 0; A < 3; ++A)
			{
				S.Setpoint[A] = 0.0f;
				S.Iterm[A] = 0.0f;
				S.PidSum[A] = 0.0f;
			}
			for (float& C : S.MotorCommand) { C = 0.0f; }
		}
		else if (In.Turtle)
		{
			for (int A = 0; A < 3; ++A)
			{
				S.Iterm[A] = 0.0f;
			}
			MixTurtle(P, In, S);
		}
		else
		{
			ComputeSetpoints(P, In, S);
			RunPid(P, S, Throttle, Dt);
			Mix(P, In, S, Throttle);
		}

		UpdateMotors(P, S, Dt);
		IntegrateBody(P, S, Throttle, Dt);
		UpdateBattery(P, S, Dt);
	}
}
