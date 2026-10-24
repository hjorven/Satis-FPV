// Minimal vector/quaternion math for the flight simulation.
// Deliberately free of Unreal types so the simulation can be built and tested standalone.
//
// Simulation frame: right-handed, X forward, Y left, Z up (FLU), SI units (m, s, kg, rad).
// Unreal is left-handed (X forward, Y right, Z up, cm); see FPVSimConvert.h for the mapping.

#pragma once

#include <cmath>
#include <cstdint>

namespace FPVSim
{
	constexpr float Pi = 3.14159265358979f;
	constexpr float DegToRad = Pi / 180.0f;
	constexpr float RadToDeg = 180.0f / Pi;

	inline float Clamp(float V, float Lo, float Hi) { return V < Lo ? Lo : (V > Hi ? Hi : V); }
	inline float Min(float A, float B) { return A < B ? A : B; }
	inline float Max(float A, float B) { return A > B ? A : B; }
	inline float Sign(float V) { return V > 0.0f ? 1.0f : (V < 0.0f ? -1.0f : 0.0f); }
	inline float Lerp(float A, float B, float T) { return A + (B - A) * T; }

	struct Vec3
	{
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;

		Vec3() = default;
		Vec3(float InX, float InY, float InZ) : X(InX), Y(InY), Z(InZ) {}

		Vec3 operator+(const Vec3& O) const { return { X + O.X, Y + O.Y, Z + O.Z }; }
		Vec3 operator-(const Vec3& O) const { return { X - O.X, Y - O.Y, Z - O.Z }; }
		Vec3 operator-() const { return { -X, -Y, -Z }; }
		Vec3 operator*(float S) const { return { X * S, Y * S, Z * S }; }
		Vec3 operator/(float S) const { return { X / S, Y / S, Z / S }; }
		Vec3& operator+=(const Vec3& O) { X += O.X; Y += O.Y; Z += O.Z; return *this; }
		Vec3& operator-=(const Vec3& O) { X -= O.X; Y -= O.Y; Z -= O.Z; return *this; }
		Vec3& operator*=(float S) { X *= S; Y *= S; Z *= S; return *this; }

		float operator[](int I) const { return I == 0 ? X : (I == 1 ? Y : Z); }
		float& operator[](int I) { return I == 0 ? X : (I == 1 ? Y : Z); }

		float Dot(const Vec3& O) const { return X * O.X + Y * O.Y + Z * O.Z; }
		Vec3 Cross(const Vec3& O) const { return { Y * O.Z - Z * O.Y, Z * O.X - X * O.Z, X * O.Y - Y * O.X }; }
		float SizeSquared() const { return X * X + Y * Y + Z * Z; }
		float Size() const { return std::sqrt(SizeSquared()); }
		Vec3 Normalized() const
		{
			const float S = Size();
			return S > 1e-8f ? *this / S : Vec3();
		}
		Vec3 Mul(const Vec3& O) const { return { X * O.X, Y * O.Y, Z * O.Z }; }
	};

	struct Quat
	{
		float W = 1.0f;
		float X = 0.0f;
		float Y = 0.0f;
		float Z = 0.0f;

		Quat() = default;
		Quat(float InW, float InX, float InY, float InZ) : W(InW), X(InX), Y(InY), Z(InZ) {}

		static Quat FromAxisAngle(const Vec3& Axis, float Angle)
		{
			const Vec3 N = Axis.Normalized();
			const float H = Angle * 0.5f;
			const float S = std::sin(H);
			return { std::cos(H), N.X * S, N.Y * S, N.Z * S };
		}

		// Intrinsic Z (yaw), then Y (pitch), then X (roll). Angles in radians, FLU convention:
		// positive roll = right side down, positive pitch = nose down, positive yaw = nose left.
		static Quat FromEuler(float Roll, float Pitch, float Yaw)
		{
			const Quat Qz = FromAxisAngle({ 0, 0, 1 }, Yaw);
			const Quat Qy = FromAxisAngle({ 0, 1, 0 }, Pitch);
			const Quat Qx = FromAxisAngle({ 1, 0, 0 }, Roll);
			return Qz * Qy * Qx;
		}

		Quat operator*(const Quat& O) const
		{
			return {
				W * O.W - X * O.X - Y * O.Y - Z * O.Z,
				W * O.X + X * O.W + Y * O.Z - Z * O.Y,
				W * O.Y - X * O.Z + Y * O.W + Z * O.X,
				W * O.Z + X * O.Y - Y * O.X + Z * O.W
			};
		}

		Quat Conjugate() const { return { W, -X, -Y, -Z }; }

		void Normalize()
		{
			const float S = std::sqrt(W * W + X * X + Y * Y + Z * Z);
			if (S > 1e-8f)
			{
				W /= S; X /= S; Y /= S; Z /= S;
			}
			else
			{
				*this = Quat();
			}
		}

		// Body -> world.
		Vec3 Rotate(const Vec3& V) const
		{
			const Vec3 Q(X, Y, Z);
			const Vec3 T = Q.Cross(V) * 2.0f;
			return V + T * W + Q.Cross(T);
		}

		// World -> body.
		Vec3 Unrotate(const Vec3& V) const { return Conjugate().Rotate(V); }

		// Integrate a body-frame angular velocity over Dt.
		void IntegrateBodyRate(const Vec3& Omega, float Dt)
		{
			const float Angle = Omega.Size() * Dt;
			if (Angle > 1e-9f)
			{
				*this = *this * FromAxisAngle(Omega, Angle);
				Normalize();
			}
		}

		// Roll/pitch in the same convention as FromEuler (radians).
		void ToRollPitch(float& OutRoll, float& OutPitch) const
		{
			// Third row of the rotation matrix (world Z expressed via body axes).
			const float R31 = 2.0f * (X * Z - W * Y);
			const float R32 = 2.0f * (Y * Z + W * X);
			const float R33 = 1.0f - 2.0f * (X * X + Y * Y);
			OutRoll = std::atan2(R32, R33);
			OutPitch = -std::asin(Clamp(R31, -1.0f, 1.0f));
		}
	};

	// First-order low-pass filter.
	struct PT1
	{
		float State = 0.0f;

		float Apply(float In, float CutoffHz, float Dt)
		{
			if (CutoffHz <= 0.0f)
			{
				State = In;
				return State;
			}
			const float RC = 1.0f / (2.0f * Pi * CutoffHz);
			const float K = Dt / (RC + Dt);
			State += K * (In - State);
			return State;
		}
	};

	// Small deterministic RNG (xorshift) so simulation runs are reproducible.
	struct Rng
	{
		uint32_t S = 0x12345678u;

		float NextSigned()
		{
			S ^= S << 13;
			S ^= S >> 17;
			S ^= S << 5;
			return (static_cast<float>(S & 0xFFFFFF) / static_cast<float>(0xFFFFFF)) * 2.0f - 1.0f;
		}
	};
}
