// Quadcopter flight simulation: Betaflight-style flight controller, mixer, motor lag,
// LiPo battery and aerodynamics. Pure C++, no Unreal dependency.
//
// Motor layout (X frame, FLU frame, seen from above):
//   0 = front left  (CCW)     1 = front right (CW)
//   2 = rear left   (CW)      3 = rear right  (CCW)

#pragma once

#include "Sim/FPVRates.h"

namespace FPVSim
{
	enum class FlightMode : uint8_t
	{
		Acro = 0,
		Angle = 1,
		Horizon = 2,
	};

	enum Axis : int
	{
		Roll = 0,
		Pitch = 1,
		Yaw = 2,
	};

	struct PidGains
	{
		float P = 45.0f;
		float I = 80.0f;
		float D = 40.0f;
		float F = 120.0f;
	};

	struct Params
	{
		// --- Rates -------------------------------------------------------------
		RatesType Rates = RatesType::Actual;
		AxisRates AxisRate[3] = { { 70.0f, 670.0f, 0.54f }, { 70.0f, 670.0f, 0.54f }, { 70.0f, 670.0f, 0.0f } };
		float StickDeadband = 0.01f;
		float YawDeadband = 0.02f;
		float RcSmoothingHz = 40.0f;		// stick smoothing (2nd order), 0 disables

		// --- Throttle ----------------------------------------------------------
		float ThrottleMid = 0.5f;
		float ThrottleExpo = 0.0f;
		float ThrottleLimit = 1.0f;		// 0..1, scales the throttle stick
		float MotorIdle = 0.055f;		// dshot_idle_value equivalent (5.5 %)
		bool Airmode = true;

		// --- PID (Betaflight units) --------------------------------------------
		PidGains Pid[3] = { { 45, 80, 40, 120 }, { 47, 84, 46, 125 }, { 45, 80, 0, 120 } };
		float GyroLpfHz = 250.0f;
		float DtermLpfHz = 90.0f;
		float FeedforwardSmoothHz = 25.0f;
		float ItermLimit = 400.0f;
		float ItermRelaxCutoffHz = 15.0f;	// 0 disables I-term relax
		float ItermRelaxThreshold = 40.0f;	// deg/s of high-passed setpoint where I stops growing
		float PidSumLimit = 500.0f;
		float PidSumLimitYaw = 400.0f;
		float TpaRate = 0.65f;			// D attenuation at full throttle
		float TpaBreakpoint = 0.35f;		// throttle where TPA starts

		// --- Self-levelling ----------------------------------------------------
		float AngleLimitDeg = 60.0f;
		float AngleStrength = 50.0f;		// deg/s per 10 deg error
		float HorizonStrength = 50.0f;
		float HorizonTransition = 0.75f;	// stick deflection where levelling fades out

		// --- 3D (reversible motors) --------------------------------------------
		float Deadband3D = 0.06f;			// throttle deadband around centre
		float ReverseThrustEfficiency = 0.7f;

		// --- Turtle mode -------------------------------------------------------
		float TurtlePower = 0.8f;

		// --- Craft -------------------------------------------------------------
		float Mass = 0.65f;					// kg, incl. battery
		float ArmLength = 0.11f;			// m, centre to motor
		float MotorMaxThrust = 16.0f;		// N per motor at full battery
		float MotorTimeConstantUp = 0.020f; // s
		float MotorTimeConstantDown = 0.025f;
		float RotorInertia = 1.5e-5f;		// kg m^2 per prop + bell, gives yaw kick on motor acceleration
		float MotorMaxRadPerSec = 3500.0f;	// ~33k rpm at full battery
		float YawTorqueCoeff = 0.03f; 		// m, reaction torque per N of thrust
		Vec3 Inertia = { 0.0030f, 0.0034f, 0.0045f }; // kg m^2
		Vec3 DragArea = { 0.012f, 0.014f, 0.040f };   // Cd*A per body axis, m^2
		float RotationalDrag = 0.0015f;		// N m per rad/s
		float PropHForce = 0.02f;			// lateral drag per N of thrust per m/s
		float PropwashStrength = 1.0f;		// 0 disables
		float GyroNoiseDegPerSec = 0.4f;

		// --- Battery -----------------------------------------------------------
		int Cells = 6;
		float CapacityMah = 1300.0f;
		float CapacityMultiplier = 1.0f;	// gameplay knob, 1 = realistic
		float CellResistance = 0.004f;		// ohm per cell
		float PropDiameter = 0.127f;		// m (5 inch)
		float FigureOfMerit = 0.42f;
		float MotorEfficiency = 0.65f;
		float AvionicsCurrent = 0.8f;		// A (VTX, FC, camera)

		// --- Environment -------------------------------------------------------
		float Gravity = 9.81f;
		float AirDensity = 1.225f;
	};

	struct Input
	{
		float Roll = 0.0f;		// -1..1, + = right
		float Pitch = 0.0f;		// -1..1, + = forward (nose down)
		float Yaw = 0.0f;		// -1..1, + = right
		float Throttle = 0.0f;	// 0..1 normal, 0..1 with 0.5 = zero thrust in 3D mode
		bool Armed = false;
		FlightMode Mode = FlightMode::Acro;
		bool Mode3D = false;
		bool Turtle = false;
	};

	struct State
	{
		// Rigid body (world frame, FLU)
		Vec3 Pos;
		Vec3 Vel;
		Quat Rot;
		Vec3 Omega;				// body rates, rad/s

		// Motors: Command = mixer output, Output = spun-up value (both -1..1)
		float MotorCommand[4] = { 0, 0, 0, 0 };
		float MotorOutput[4] = { 0, 0, 0, 0 };
		float MotorThrust[4] = { 0, 0, 0, 0 };
		float MotorSpeed[4] = { 0, 0, 0, 0 };	// rad/s, signed

		// Battery
		float StateOfCharge = 1.0f;
		float MahUsed = 0.0f;
		float Voltage = 25.2f;
		float Current = 0.0f;

		// Flight controller internals
		PT1 GyroFilter[3];
		PT1 DtermFilter[3];
		PT1 FeedforwardFilter[3];
		PT1 RcSmoothing[3][2];
		PT1 RelaxFilter[3];
		PT1 PropwashFilter[3];
		float PrevDterm[3] = { 0, 0, 0 };
		float PrevSetpoint[3] = { 0, 0, 0 };
		float Iterm[3] = { 0, 0, 0 };
		float Setpoint[3] = { 0, 0, 0 };	// deg/s, stick axes
		float PidSum[3] = { 0, 0, 0 };
		float ThrottleOut = 0.0f;
		Rng Random;
		float Time = 0.0f;
	};

	// Flight controller loop rate and physics step.
	constexpr float StepRate = 1000.0f;
	constexpr float StepDt = 1.0f / StepRate;

	// Advances the simulation by exactly one StepDt.
	void Step(const Params& P, const Input& In, State& S);

	// Clears integrators/filters, e.g. on arming.
	void ResetController(State& S);

	// Pack voltage at a given state of charge with no load.
	float OpenCircuitVoltage(const Params& P, float StateOfCharge);

	// Restore a full battery.
	void ChargeBattery(const Params& P, State& S);

	// Throttle (0..1) that produces hover with a full battery, for presets and tests.
	float HoverThrottle(const Params& P);
}
