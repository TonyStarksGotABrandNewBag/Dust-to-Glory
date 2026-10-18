// Small 3D vector and quaternion types for the engine-independent chassis simulation.
// Axes match Unreal: X forward, Y right, Z up, in metres. The Unreal layer converts to centimetres.
#pragma once

#include <cmath>

namespace dtg::sim
{
	struct Vec3
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;

		constexpr Vec3() = default;
		constexpr Vec3(double InX, double InY, double InZ) : X(InX), Y(InY), Z(InZ) {}

		Vec3 operator+(const Vec3& Other) const { return Vec3(X + Other.X, Y + Other.Y, Z + Other.Z); }
		Vec3 operator-(const Vec3& Other) const { return Vec3(X - Other.X, Y - Other.Y, Z - Other.Z); }
		Vec3 operator-() const { return Vec3(-X, -Y, -Z); }
		Vec3 operator*(double Scale) const { return Vec3(X * Scale, Y * Scale, Z * Scale); }
		Vec3 operator/(double Scale) const { return Vec3(X / Scale, Y / Scale, Z / Scale); }
		Vec3& operator+=(const Vec3& Other)
		{
			X += Other.X;
			Y += Other.Y;
			Z += Other.Z;
			return *this;
		}
		Vec3& operator-=(const Vec3& Other)
		{
			X -= Other.X;
			Y -= Other.Y;
			Z -= Other.Z;
			return *this;
		}
		Vec3& operator*=(double Scale)
		{
			X *= Scale;
			Y *= Scale;
			Z *= Scale;
			return *this;
		}

		double Length() const { return std::sqrt(X * X + Y * Y + Z * Z); }
		double LengthSquared() const { return X * X + Y * Y + Z * Z; }
		Vec3 Normalized() const
		{
			const double Size = Length();
			return Size > 1e-12 ? *this / Size : Vec3();
		}
		bool IsFinite() const { return std::isfinite(X) && std::isfinite(Y) && std::isfinite(Z); }
	};

	inline Vec3 operator*(double Scale, const Vec3& Value) { return Value * Scale; }
	inline double Dot(const Vec3& A, const Vec3& B) { return A.X * B.X + A.Y * B.Y + A.Z * B.Z; }
	inline Vec3 Cross(const Vec3& A, const Vec3& B)
	{
		return Vec3(A.Y * B.Z - A.Z * B.Y, A.Z * B.X - A.X * B.Z, A.X * B.Y - A.Y * B.X);
	}
	/** Component of V perpendicular to the unit normal N. */
	inline Vec3 ProjectOnPlane(const Vec3& V, const Vec3& N) { return V - N * Dot(V, N); }

	/** Unit quaternion (same component convention as Unreal's FQuat). */
	struct Quat
	{
		double X = 0.0;
		double Y = 0.0;
		double Z = 0.0;
		double W = 1.0;

		constexpr Quat() = default;
		constexpr Quat(double InX, double InY, double InZ, double InW) : X(InX), Y(InY), Z(InZ), W(InW) {}

		static Quat FromAxisAngle(const Vec3& Axis, double AngleRad)
		{
			const Vec3 Unit = Axis.Normalized();
			const double Half = 0.5 * AngleRad;
			const double S = std::sin(Half);
			return Quat(Unit.X * S, Unit.Y * S, Unit.Z * S, std::cos(Half));
		}
		/** Rotation about +Z (yaw: positive turns X toward Y, i.e. to the right). */
		static Quat FromYaw(double YawRad) { return FromAxisAngle(Vec3(0.0, 0.0, 1.0), YawRad); }

		Quat operator*(const Quat& B) const
		{
			return Quat(W * B.X + X * B.W + Y * B.Z - Z * B.Y,
				W * B.Y - X * B.Z + Y * B.W + Z * B.X,
				W * B.Z + X * B.Y - Y * B.X + Z * B.W,
				W * B.W - X * B.X - Y * B.Y - Z * B.Z);
		}
		Quat Inverse() const { return Quat(-X, -Y, -Z, W); }

		Vec3 Rotate(const Vec3& V) const
		{
			// v' = v + 2w (q x v) + 2 q x (q x v)
			const Vec3 Q(X, Y, Z);
			const Vec3 T = Cross(Q, V) * 2.0;
			return V + T * W + Cross(Q, T);
		}
		Vec3 Unrotate(const Vec3& V) const { return Inverse().Rotate(V); }

		void Normalize()
		{
			const double Size = std::sqrt(X * X + Y * Y + Z * Z + W * W);
			if (Size < 1e-12)
			{
				*this = Quat();
				return;
			}
			X /= Size;
			Y /= Size;
			Z /= Size;
			W /= Size;
		}

		Vec3 Forward() const { return Rotate(Vec3(1.0, 0.0, 0.0)); }
		Vec3 Right() const { return Rotate(Vec3(0.0, 1.0, 0.0)); }
		Vec3 Up() const { return Rotate(Vec3(0.0, 0.0, 1.0)); }
		/** Heading in the ground plane, radians. */
		double Yaw() const
		{
			const Vec3 F = Forward();
			return std::atan2(F.Y, F.X);
		}
	};
}
