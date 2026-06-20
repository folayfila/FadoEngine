// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_MATH_H
#define FADO_MATH_H

#include "fado_types.h"
#include <math.h>

// ────────────────────────────────────────────────────────────────────────
#define Deg2Rad(d) ((d) * (Pi32 / 180.0f))

// ────────────────────────────────────────────────────────────────────────
// ──────────────── General ───────────────────────────────────────────────

inline f32 Clampf32(f32 value, f32 min, f32 max)
{
	if (value < min)
	{
		return min;
	}
	else if (value > max)
	{
		return max;
	}
	else
	{
		return value;
	}
}

inline i32 Clampi32(i32 value, i32 min, i32 max)
{
	if (value < min)
	{
		return min;
	}
	else if (value > max)
	{
		return max;
	}
	else
	{
		return value;
	}
}

inline f32 Absf32(f32 value)
{
	f32 result = (value < 0.0f) ? -value : value;
	return result;
}

// ────────────────────────────────────────────────────────────────────────
// ──────────────── Vectors ───────────────────────────────────────────────

struct mat3
{
	f32 m[9];
};

struct mat4
{
	f32 m[16];
};


inline b8 operator==(const v4& va, const v4& vb)
{
	return
		(va.x == vb.x) &&
		(va.y == vb.y) &&
		(va.z == vb.z) &&
		(va.w == vb.w);
}

inline v3 operator*(v3 a, f32 b)
{
	v3 result;

	result.x = a.x * b;
	result.y = a.y * b;
	result.z = a.z * b;

	return result;
}

inline v3 V3One()
{
	v3 v = { 1.0f, 1.0f, 1.0f };
	return v;
}

inline f32 V3Dot(v3 a, v3 b)
{
    return (a.x * b.x) + (a.y * b.y) + (a.z * b.z);
}

inline v3 V3Cross(v3 a, v3 b)
{
    v3 result;
    result.x = (a.y * b.z) - (a.z * b.y);
    result.y = (a.z * b.x) - (a.x * b.z);
    result.z = (a.x * b.y) - (a.y * b.x);
    return result;
}

inline v3 V3Sub(v3 a, v3 b)
{
    v3 result = { (a.x - b.x), (a.y - b.y), (a.z - b.z) };
    return result;
}

inline f32 V3Length(v3 a)
{
	f32 result = sqrtf(V3Dot(a, a));
    return result;
}

inline v3 V3Normalize(v3 a)
{
	v3 result = { 0.0f, 0.0f, 0.0f };

    f32 len = V3Length(a);
    if (len > 0.00001f)
    {
		result = { (a.x / len), (a.y / len), (a.z / len) };
    }
    return result;
}

// > TODO: Replace with our own rand()
#include <stdlib.h>
inline f32 Randomf32InRange(f32 min, f32 max)
{
	f32 result = min + ((max - min) * ((f32)rand() / (f32)RAND_MAX));
	return result;
}

inline v4 GetRandomColor()
{
	v4 color = { 0 , 0, 0, 1 };
	color.r = Randomf32InRange(0.0f, 1.0f);
	color.g = Randomf32InRange(0.0f, 1.0f);
	color.b = Randomf32InRange(0.0f, 1.0f);
	return color;
}

// ────────────────────────────────────────────────────────────────────────
// ──────────────── Quaternions ───────────────────────────────────────────
// 
// Quaternion layout: v4 q = { x, y, z, w }
// Identity = { 0, 0, 0, 1 }
// ────────────────────────────────────────────────────────────────────────

inline quat QuatIdentity()
{
	quat q = { 0.0f, 0.0f, 0.0f, 1.0f };
	return q;
}

inline b8 IsQuatIdentity(quat& q)
{
	b8 result = (q == QuatIdentity());
	return result;
}

// Build a quaternion from a single axis + angle (radians).
// This function simply follows the math formula to building a quat based on an axis and an angle.
inline quat QuatFromAxisAngle(v3 axis, f32 angleRad)
{
	f32 half = angleRad * 0.5f;	// half angle
	f32 s = sinf(half);	// sin(angle/2)

	quat q;
	q.x = axis.x * s;
	q.y = axis.y * s;
	q.z = axis.z * s;
	q.w = cosf(half);

	return q;
}

// Combine two rotations: first apply a, then apply b, following the quaternion product formula.
// Result = a * b (Order matters!).
inline quat QuatMultiply(quat a, quat b)
{
	quat result = QuatIdentity();

	result.x = (a.w * b.x) + (a.x * b.w) + (a.y * b.z) - (a.z * b.y);
	result.y = (a.w * b.y) - (a.x * b.z) + (a.y * b.w) + (a.z * b.x);
	result.z = (a.w * b.z) + (a.x * b.y) - (a.y * b.x) + (a.z * b.w);
	result.w = (a.w * b.w) - (a.x * b.x) - (a.y * b.y) - (a.z * b.z);

	return result;
}

// Normalize: keeps the quaternion valid after many multiplications.
// Floating point drift will slowly break it without this.
inline void QuatNormalize(quat* q)
{
	f32 length = sqrtf((q->x * q->x) + (q->y * q->y) + (q->z * q->z) + (q->w * q->w));
	if (length > 0.00001f)
	{
		q->x /= length;
		q->y /= length;
		q->z /= length;
		q->w /= length;
	}
}

// Convert quaternion to a 3x3 column-major rotation matrix.
// The columns of this matrix ARE the right/up/forward axes.
// After this:
// right   = { m[0], m[1], m[2] }   (column 0)
// up      = { m[3], m[4], m[5] }   (column 1)
// forward = { m[6], m[7], m[8] }   (column 2)
inline mat3 QuatToMat3(quat q)
{
	mat3 m = {};

	f32 x = q.x, y = q.y, z = q.z, w = q.w;

	// Column 0
	m.m[0] = 1 - 2 * (y * y + z * z);
	m.m[1] = 2 * (x * y + w * z);
	m.m[2] = 2 * (x * z - w * y);

	// Column 1
	m.m[3] = 2 * (x * y - w * z);
	m.m[4] = 1 - 2 * (x * x + z * z);
	m.m[5] = 2 * (y * z + w * x);

	// Column 2
	m.m[6] = 2 * (x * z + w * y);
	m.m[7] = 2 * (y * z - w * x);
	m.m[8] = 1 - 2 * (x * x + y * y);

	return m;
}

// Convert quaternion to a 4x4 column-major rotation matrix.
// This is what we pass to the GPU / view matrix math.
// The columns of this matrix ARE the right/up/forward axes.
// After this:
// right   = { m[0], m[1], m[2]  }   (column 0)
// up      = { m[4], m[5], m[6]  }   (column 1)
// forward = { m[8], m[9], m[10] }   (column 2)
inline mat4 QuatToMatrix(quat q)
{
	mat4 m = {};

	f32 x = q.x, y = q.y, z = q.z, w = q.w;	// for readablity.

	// Column 0							  	
	m.m[0] = 1 - 2 * (y * y + z * z);
	m.m[1] = 2 * (x * y + w * z);
	m.m[2] = 2 * (x * z - w * y);
	m.m[3] = 0;

	// Column 1
	m.m[4] = 2 * (x * y - w * z);
	m.m[5] = 1 - 2 * (x * x + z * z);
	m.m[6] = 2 * (y * z + w * x);
	m.m[7] = 0;

	// Column 2							
	m.m[8] = 2 * (x * z + w * y);
	m.m[9] = 2 * (y * z - w * x);
	m.m[10] = 1 - 2 * (x * x + y * y);
	m.m[11] = 0;

	// Column 3
	m.m[12] = 0;
	m.m[13] = 0;
	m.m[14] = 0;
	m.m[15] = 1;

	return m;
}

// Build a quaternion from euler angles (degrees: pitch=X, yaw=Y, roll=Z).
// Internally converts each axis to an axis-angle quaternion and combines them.
// Order: Yaw first (world Y), then Pitch (local X), then Roll (local Z).
// This order gives standard FPS behavior.
inline quat QuatFromEuler(v3 eulerDegrees)
{
	quat result = QuatIdentity();

	quat yaw =   QuatFromAxisAngle({ 0, 1, 0 }, Deg2Rad(eulerDegrees.y));
	quat pitch = QuatFromAxisAngle({ 1, 0, 0 }, Deg2Rad(eulerDegrees.x));
	quat roll =   QuatFromAxisAngle({ 0, 0, 1 }, Deg2Rad(eulerDegrees.z));

	// Combine: yaw * pitch * roll
	quat yawMultPitch = QuatMultiply(yaw, pitch);
	result = QuatMultiply(yawMultPitch, roll);

	return result;
}

inline v3 QuatToEuler(quat q)
{
	v3 euler;

	// Pitch (X)
	f32 sinp = 2.0f * (q.w * q.x + q.y * q.z);
	f32 cosp = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	euler.x = atan2f(sinp, cosp);

	// Yaw (Y)
	f32 siny = 2.0f * (q.w * q.y - q.z * q.x);
	if (fabsf(siny) >= 1.0f)
		euler.y = copysignf(Pi32 * 0.5f, siny);
	else
		euler.y = asinf(siny);

	// Roll (Z)
	f32 sinr = 2.0f * (q.w * q.z + q.x * q.y);
	f32 cosr = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	euler.z = atan2f(sinr, cosr);

	return euler;
}

// Set an absolute rotation on a transform slot (replaces current rotation).
// rot = euler angles in degrees {pitch, yaw, roll}
inline void SetRotation(FTransformTable* transforms, HTransform handle, v3 eulerDegrees)
{
	transforms->rotations[handle] = QuatFromEuler(eulerDegrees);
}

// Apply a DELTA rotation on top of the current rotation.
// rot = euler angles in degrees {pitchDelta, yawDelta, rollDelta}
inline void Rotate(FTransformTable* transforms, HTransform handle, v3 eulerDeltaDegrees)
{
	quat delta = QuatFromEuler(eulerDeltaDegrees);
	quat* current = &transforms->rotations[handle];
	*current = QuatMultiply(*current, delta);
	QuatNormalize(current);
}

inline v3 QuatForward(quat q)
{
	v3 forward;
	forward.x = 2.0f * (q.x * q.z + q.w * q.y);
	forward.y = 2.0f * (q.y * q.z - q.w * q.x);
	forward.z = 1.0f - 2.0f * (q.x * q.x + q.y * q.y);
	return forward;
}

inline v3 QuatRight(quat q)
{
	v3 right;
	right.x = 1.0f - 2.0f * (q.y * q.y + q.z * q.z);
	right.y = 2.0f * (q.x * q.y + q.w * q.z);
	right.z = 2.0f * (q.x * q.z - q.w * q.y);
	return right;
}

inline v3 QuatUp(quat q)
{
	v3 up;
	up.x = 2.0f * (q.x * q.y - q.w * q.z);
	up.y = 1.0f - 2.0f * (q.x * q.x + q.z * q.z);
	up.z = 2.0f * (q.y * q.z + q.w * q.x);
	return up;
}

#endif	// FADO_MATH_H