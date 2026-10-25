// Standalone tests for the FPVDrone flight simulation (no Unreal needed).
// Build and run: tests/run_sim_tests.sh

#include "Sim/FPVFlightSim.h"

#include <cstdio>
#include <cstdlib>
#include <cmath>

using namespace FPVSim;

static int GFailures = 0;

#define EXPECT(Cond, ...) \
	do { \
		if (!(Cond)) { ++GFailures; std::printf("  FAIL %s:%d: ", __FILE__, __LINE__); std::printf(__VA_ARGS__); std::printf("\n"); } \
	} while (0)

static void Run(const Params& P, const Input& In, State& S, float Seconds)
{
	const int Steps = static_cast<int>(Seconds * StepRate + 0.5f);
	for (int I = 0; I < Steps; ++I)
	{
		Step(P, In, S);
	}
}

static State MakeState(const Params& P)
{
	State S;
	ChargeBattery(P, S);
	S.Pos = { 0, 0, 100 };
	return S;
}

static void TestRates()
{
	std::printf("rates\n");
	AxisRates Bf{ 1.0f, 0.7f, 0.0f };
	const float BfMax = MaxRate(RatesType::Betaflight, Bf);
	EXPECT(std::fabs(BfMax - 666.67f) < 1.0f, "Betaflight 1.0/0.7 max %.1f, expected 666.7", BfMax);
	EXPECT(std::fabs(ApplyRates(RatesType::Betaflight, Bf, 0.5f) - 153.8f) < 1.0f, "Betaflight half stick %.1f", ApplyRates(RatesType::Betaflight, Bf, 0.5f));

	AxisRates Actual{ 70.0f, 670.0f, 0.54f };
	EXPECT(std::fabs(MaxRate(RatesType::Actual, Actual) - 670.0f) < 0.5f, "Actual max %.1f", MaxRate(RatesType::Actual, Actual));
	const float Small = ApplyRates(RatesType::Actual, Actual, 0.001f);
	EXPECT(std::fabs(Small / 0.001f - 70.0f) < 2.0f, "Actual center sensitivity %.1f", Small / 0.001f);
	EXPECT(ApplyRates(RatesType::Actual, Actual, -1.0f) < -669.0f, "Actual symmetric");

	AxisRates Quick{ 1.0f, 670.0f, 0.0f };
	EXPECT(std::fabs(MaxRate(RatesType::Quick, Quick) - 670.0f) < 1.0f, "Quick max %.1f", MaxRate(RatesType::Quick, Quick));

	AxisRates Kiss{ 1.0f, 0.7f, 0.0f };
	EXPECT(std::fabs(MaxRate(RatesType::Kiss, Kiss) - 666.67f) < 1.0f, "KISS max %.1f", MaxRate(RatesType::Kiss, Kiss));

	EXPECT(std::fabs(ApplyThrottleCurve(0.5f, 0.5f, 0.6f) - 0.5f) < 1e-4f, "throttle mid fixed point");
	EXPECT(ApplyThrottleCurve(0.4f, 0.5f, 0.6f) > 0.4f, "throttle expo flattens around mid");
	EXPECT(ApplyDeadband(0.005f, 0.01f) == 0.0f, "deadband");
}

static void TestHover()
{
	std::printf("hover\n");
	Params P;
	State S = MakeState(P);
	Input In;
	In.Armed = true;
	In.Throttle = HoverThrottle(P);
	Run(P, In, S, 3.0f);
	const float RateDeg = S.Omega.Size() * RadToDeg;
	EXPECT(RateDeg < 5.0f, "hover rotation %.2f deg/s", RateDeg);
	EXPECT(std::fabs(S.Vel.Z) < 1.5f, "hover vertical speed %.2f m/s (throttle %.3f)", S.Vel.Z, In.Throttle);
	float Roll, Pitch;
	S.Rot.ToRollPitch(Roll, Pitch);
	EXPECT(std::fabs(Roll * RadToDeg) < 3.0f && std::fabs(Pitch * RadToDeg) < 3.0f, "hover attitude roll %.2f pitch %.2f", Roll * RadToDeg, Pitch * RadToDeg);
}

static void TestStepResponse(int AxisIndex, const char* Name)
{
	std::printf("step response %s\n", Name);
	Params P;
	State S = MakeState(P);
	Input In;
	In.Armed = true;
	In.Throttle = 0.4f;
	Run(P, In, S, 0.5f);

	float* Stick = AxisIndex == Roll ? &In.Roll : (AxisIndex == Pitch ? &In.Pitch : &In.Yaw);
	*Stick = 0.7f;
	const float Target = ApplyRates(P.Rates, P.AxisRate[AxisIndex], 0.7f - 0.0f);

	auto GyroOnAxis = [&]() {
		const float G[3] = { S.Omega.X * RadToDeg, S.Omega.Y * RadToDeg, -S.Omega.Z * RadToDeg };
		return G[AxisIndex];
	};

	float Peak = 0.0f;
	float ReachTime = -1.0f;
	for (int I = 0; I < 400; ++I)
	{
		Step(P, In, S);
		const float G = GyroOnAxis();
		Peak = Max(Peak, G);
		if (ReachTime < 0.0f && G > 0.9f * S.Setpoint[AxisIndex])
		{
			ReachTime = (I + 1) * StepDt;
		}
	}
	const float Settled = GyroOnAxis();
	const float ReachLimit = AxisIndex == Yaw ? 0.25f : 0.08f;
	EXPECT(ReachTime > 0.0f && ReachTime < ReachLimit, "%s 90%% rise time %.3f s (target %.0f deg/s)", Name, ReachTime, Target);
	EXPECT(Peak < S.Setpoint[AxisIndex] * 1.2f, "%s overshoot peak %.0f vs %.0f", Name, Peak, S.Setpoint[AxisIndex]);
	EXPECT(std::fabs(Settled - S.Setpoint[AxisIndex]) < 0.05f * S.Setpoint[AxisIndex], "%s tracking %.0f vs %.0f", Name, Settled, S.Setpoint[AxisIndex]);

	// Release the stick: rate must return to zero without ringing.
	*Stick = 0.0f;
	Run(P, In, S, 0.15f);
	float MaxAfter = 0.0f;
	for (int I = 0; I < 200; ++I)
	{
		Step(P, In, S);
		MaxAfter = Max(MaxAfter, std::fabs(GyroOnAxis()));
	}
	EXPECT(MaxAfter < 15.0f, "%s residual after stop %.1f deg/s", Name, MaxAfter);
	std::printf("  rise %.0f ms, peak %.0f / %.0f deg/s, residual %.1f deg/s\n", ReachTime * 1000.0f, Peak, Target, MaxAfter);
}

static void TestAngleMode()
{
	std::printf("angle mode\n");
	Params P;
	State S = MakeState(P);
	S.Rot = Quat::FromEuler(40.0f * DegToRad, -20.0f * DegToRad, 0.0f);
	Input In;
	In.Armed = true;
	In.Mode = FlightMode::Angle;
	In.Throttle = HoverThrottle(P);
	Run(P, In, S, 1.0f);
	float Roll, Pitch;
	S.Rot.ToRollPitch(Roll, Pitch);
	EXPECT(std::fabs(Roll * RadToDeg) < 2.0f && std::fabs(Pitch * RadToDeg) < 2.0f, "self-level roll %.2f pitch %.2f", Roll * RadToDeg, Pitch * RadToDeg);

	In.Roll = 1.0f;
	Run(P, In, S, 1.0f);
	S.Rot.ToRollPitch(Roll, Pitch);
	EXPECT(std::fabs(Roll * RadToDeg - P.AngleLimitDeg) < 3.0f, "angle limit %.1f", Roll * RadToDeg);
	std::printf("  full stick roll angle %.1f deg\n", Roll * RadToDeg);

	// Horizon: centred sticks level the craft.
	State H = MakeState(P);
	H.Rot = Quat::FromEuler(30.0f * DegToRad, 0.0f, 0.0f);
	In = Input();
	In.Armed = true;
	In.Mode = FlightMode::Horizon;
	In.Throttle = HoverThrottle(P);
	Run(P, In, H, 1.5f);
	H.Rot.ToRollPitch(Roll, Pitch);
	EXPECT(std::fabs(Roll * RadToDeg) < 3.0f, "horizon level %.2f", Roll * RadToDeg);
}

static void TestEulerConvention()
{
	std::printf("conventions\n");
	// Positive roll rate must bank right: the right side (-Y) goes down.
	Quat Q = Quat::FromEuler(30.0f * DegToRad, 0.0f, 0.0f);
	const Vec3 Right = Q.Rotate({ 0, -1, 0 });
	EXPECT(Right.Z < -0.4f, "roll right lowers the right side (%.2f)", Right.Z);
	// Positive pitch is nose down.
	Q = Quat::FromEuler(0.0f, 30.0f * DegToRad, 0.0f);
	EXPECT(Q.Rotate({ 1, 0, 0 }).Z < -0.4f, "positive pitch is nose down");
	float R, Pi2;
	Quat::FromEuler(0.3f, -0.2f, 1.0f).ToRollPitch(R, Pi2);
	EXPECT(std::fabs(R - 0.3f) < 1e-3f && std::fabs(Pi2 + 0.2f) < 1e-3f, "ToRollPitch roundtrip %.3f %.3f", R, Pi2);

	// Stick roll right in acro rotates the craft to the right.
	Params P;
	State S = MakeState(P);
	Input In;
	In.Armed = true;
	In.Throttle = 0.4f;
	In.Roll = 0.5f;
	Run(P, In, S, 0.2f);
	S.Rot.ToRollPitch(R, Pi2);
	EXPECT(R > 0.2f, "roll stick right banks right (%.2f rad)", R);

	// Yaw right turns the nose to the right (-Y).
	S = MakeState(P);
	In = Input();
	In.Armed = true;
	In.Throttle = 0.4f;
	In.Yaw = 0.5f;
	Run(P, In, S, 0.3f);
	EXPECT(S.Rot.Rotate({ 1, 0, 0 }).Y < -0.1f, "yaw right turns nose right");
}

static void TestTopSpeed()
{
	std::printf("top speed\n");
	Params P;
	State S = MakeState(P);
	Input In;
	In.Armed = true;
	In.Mode = FlightMode::Angle;
	P.AngleLimitDeg = 70.0f;
	In.Pitch = 1.0f;
	In.Throttle = 1.0f;
	Run(P, In, S, 12.0f);
	const float Horizontal = std::sqrt(S.Vel.X * S.Vel.X + S.Vel.Y * S.Vel.Y);
	std::printf("  horizontal %.1f m/s (%.0f km/h), vertical %.1f m/s, %.1f V, %.0f A\n", Horizontal, Horizontal * 3.6f, S.Vel.Z, S.Voltage, S.Current);
	EXPECT(Horizontal > 25.0f && Horizontal < 65.0f, "top speed %.1f m/s", Horizontal);
	EXPECT(S.Current > 60.0f && S.Current < 220.0f, "full throttle current %.0f A", S.Current);
	EXPECT(S.Voltage < 23.5f, "voltage sag under load %.1f V", S.Voltage);
}

static void TestBattery()
{
	std::printf("battery\n");
	Params P;
	State S = MakeState(P);
	Input In;
	In.Armed = true;
	In.Mode = FlightMode::Angle;
	In.Throttle = HoverThrottle(P);
	float T = 0.0f;
	while (S.StateOfCharge > 0.0f && T < 3600.0f)
	{
		Run(P, In, S, 1.0f);
		T += 1.0f;
		S.Pos = { 0, 0, 100 };
		S.Vel = { 0, 0, 0 };
	}
	std::printf("  hover flight time %.1f min, %.0f mAh, hover current %.1f A\n", T / 60.0f, S.MahUsed, S.Current);
	EXPECT(T > 4.0f * 60.0f && T < 20.0f * 60.0f, "hover flight time %.1f min", T / 60.0f);
}

// Typical freestyle/exploring mix: cruising, a punch-out now and then, some dives.
static float MixedFlightMinutes(const Params& P)
{
	State S = MakeState(P);
	Input In;
	In.Armed = true;
	In.Mode = FlightMode::Angle;
	const float Hover = HoverThrottle(P);
	float T = 0.0f;
	while (S.StateOfCharge > 0.0f && T < 3600.0f)
	{
		const int Phase = static_cast<int>(T) % 10;
		In.Throttle = Phase < 6 ? Hover * 1.35f : (Phase < 9 ? Hover * 0.6f : 1.0f);
		Run(P, In, S, 1.0f);
		T += 1.0f;
		S.Pos = { 0, 0, 100 };
		S.Vel = { 0, 0, 0 };
		S.Rot = Quat();
		S.Omega = { 0, 0, 0 };
	}
	return T / 60.0f;
}

static void TestFlightTimeAndCharging()
{
	std::printf("flight time / charging\n");
	Params P;
	const float Minutes = MixedFlightMinutes(P);
	std::printf("  mixed flight time %.1f min\n", Minutes);
	EXPECT(Minutes > 3.3f && Minutes < 5.0f, "mixed flight time %.1f min, expected about 4", Minutes);

	State S = MakeState(P);
	S.StateOfCharge = 0.25f;
	S.MahUsed = 0.75f * P.CapacityMah * P.CapacityMultiplier;
	AddCharge(P, S, 0.5f);
	EXPECT(std::fabs(S.StateOfCharge - 0.75f) < 1e-4f, "charge to %.3f", S.StateOfCharge);
	EXPECT(std::fabs(S.MahUsed - 0.25f * P.CapacityMah) < 0.5f, "mAh used after charging %.0f", S.MahUsed);
	AddCharge(P, S, 1.0f);
	EXPECT(S.StateOfCharge == 1.0f && S.MahUsed == 0.0f, "charge clamps at full (%.3f, %.0f mAh)", S.StateOfCharge, S.MahUsed);
	EXPECT(std::fabs(S.Voltage - OpenCircuitVoltage(P, 1.0f)) < 1e-3f, "voltage after charging %.2f V", S.Voltage);
}

static void Test3D()
{
	std::printf("3D mode\n");
	Params P;
	State S = MakeState(P);
	S.Rot = Quat::FromEuler(Pi, 0.0f, 0.0f); // upside down
	Input In;
	In.Armed = true;
	In.Mode3D = true;
	In.Throttle = 0.0f; // full reverse
	Run(P, In, S, 0.5f);
	EXPECT(S.Vel.Z > 2.0f, "inverted full reverse climbs (vz %.2f)", S.Vel.Z);

	State Z = MakeState(P);
	In.Throttle = 0.5f; // centre = no thrust
	In.Mode3D = true;
	Run(P, In, Z, 0.5f);
	EXPECT(Z.Vel.Z < -3.0f, "3D centre stick has no lift (vz %.2f)", Z.Vel.Z);
}

static void TestTurtleAndDisarm()
{
	std::printf("turtle / disarm\n");
	Params P;
	State S = MakeState(P);
	Input In;
	In.Armed = true;
	In.Turtle = true;
	In.Roll = 1.0f;
	Run(P, In, S, 0.1f);
	EXPECT(std::fabs(S.Omega.X) > 1.0f, "turtle produces roll (%.2f rad/s)", S.Omega.X);

	S = MakeState(P);
	In = Input();
	In.Armed = false;
	In.Throttle = 1.0f;
	Run(P, In, S, 1.0f);
	EXPECT(S.MotorOutput[0] < 0.01f, "disarmed motors stay off");
	EXPECT(S.Vel.Z < -7.0f, "disarmed craft falls (vz %.2f)", S.Vel.Z);
}

struct CraftCase { const char* Name; float Mass, Arm, Prop, Drag, Thrust, MotorMs, Idle; int Cells; float Mah; };

static void TestCraftPresets()
{
	// Same numbers as FFPVSettings::ApplyPreset.
	const CraftCase Cases[] = {
		{ "racer 5in", 0.52f, 0.11f, 5.0f, 0.8f, 17.0f, 20.0f, 0.055f, 6, 1100.0f },
		{ "cinewhoop 3in", 0.38f, 0.07f, 3.0f, 1.8f, 5.5f, 20.0f, 0.055f, 4, 850.0f },
		{ "tinywhoop", 0.032f, 0.033f, 1.2f, 2.5f, 0.26f, 15.0f, 0.07f, 1, 300.0f },
	};
	for (const CraftCase& C : Cases)
	{
		std::printf("craft %s\n", C.Name);
		Params P;
		ApplyCraftScaling(P, C.Mass, C.Arm, C.Prop, C.Drag);
		P.MotorMaxThrust = C.Thrust;
		P.MotorTimeConstantUp = C.MotorMs / 1000.0f;
		P.MotorTimeConstantDown = P.MotorTimeConstantUp * 1.25f;
		P.MotorIdle = C.Idle;
		P.Cells = C.Cells;
		P.CapacityMah = C.Mah;

		State S = MakeState(P);
		Input In;
		In.Armed = true;
		In.Throttle = HoverThrottle(P);
		Run(P, In, S, 2.0f);
		EXPECT(S.Omega.Size() * RadToDeg < 8.0f, "%s hover rotation %.1f deg/s", C.Name, S.Omega.Size() * RadToDeg);

		In.Roll = 0.6f;
		Run(P, In, S, 0.4f);
		const float Want = S.Setpoint[Roll];
		const float Got = S.Omega.X * RadToDeg;
		EXPECT(std::fabs(Got - Want) < 0.1f * Want, "%s roll tracking %.0f vs %.0f", C.Name, Got, Want);
		In.Roll = 0.0f;
		Run(P, In, S, 0.2f);
		float Residual = 0.0f;
		for (int I = 0; I < 300; ++I)
		{
			Step(P, In, S);
			Residual = Max(Residual, std::fabs(S.Omega.X * RadToDeg));
		}
		EXPECT(Residual < 15.0f, "%s residual oscillation %.1f deg/s", C.Name, Residual);
		std::printf("  hover throttle %.2f, roll %.0f/%.0f deg/s, residual %.1f\n", HoverThrottle(P), Got, Want, Residual);
	}
}

int main()
{
	TestRates();
	TestEulerConvention();
	TestHover();
	TestStepResponse(Roll, "roll");
	TestStepResponse(Pitch, "pitch");
	TestStepResponse(Yaw, "yaw");
	TestAngleMode();
	TestTopSpeed();
	TestBattery();
	TestFlightTimeAndCharging();
	Test3D();
	TestTurtleAndDisarm();
	TestCraftPresets();

	if (GFailures == 0)
	{
		std::printf("ALL TESTS PASSED\n");
		return EXIT_SUCCESS;
	}
	std::printf("%d FAILURE(S)\n", GFailures);
	return EXIT_FAILURE;
}
