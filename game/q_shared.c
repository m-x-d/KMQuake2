/*
Copyright (C) 1997-2001 Id Software, Inc.
Copyright (C) 2000-2002 Mr. Hyde and Mad Dog

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include <ctype.h>
#include "q_shared.h"

vec2_t vec2_origin = { 0, 0 };
vec3_t vec3_origin = { 0, 0, 0 };
vec4_t vec4_origin = { 0, 0, 0, 0 };

#pragma region ======================= Math functions

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, const float degrees)
{
	float	m[3][3];
	float	im[3][3];
	float	zrot[3][3];
	float	tmpmat[3][3];
	float	rot[3][3];
	vec3_t	vr, vup, vf;

	vf[0] = dir[0];
	vf[1] = dir[1];
	vf[2] = dir[2];

	PerpendicularVector(vr, dir);
	CrossProduct(vr, vf, vup);

	m[0][0] = vr[0];
	m[1][0] = vr[1];
	m[2][0] = vr[2];

	m[0][1] = vup[0];
	m[1][1] = vup[1];
	m[2][1] = vup[2];

	m[0][2] = vf[0];
	m[1][2] = vf[1];
	m[2][2] = vf[2];

	memcpy(im, m, sizeof(im));

	im[0][1] = m[1][0];
	im[0][2] = m[2][0];
	im[1][0] = m[0][1];
	im[1][2] = m[2][1];
	im[2][0] = m[0][2];
	im[2][1] = m[1][2];

	memset(zrot, 0, sizeof(zrot));
	zrot[0][0] = zrot[1][1] = zrot[2][2] = 1.0f;

	zrot[0][0] = cos(DEG2RAD(degrees));
	zrot[0][1] = sin(DEG2RAD(degrees));
	zrot[1][0] = -sin(DEG2RAD(degrees));
	zrot[1][1] = cos(DEG2RAD(degrees));

	R_ConcatRotations(m, zrot, tmpmat);
	R_ConcatRotations(tmpmat, im, rot);

	for (int i = 0; i < 3; i++)
		dst[i] = rot[i][0] * point[0] + rot[i][1] * point[1] + rot[i][2] * point[2];
}

// Rotates input angles [in] by delta angles around the local coordinate system, returns new angles in [out].
void RotateAngles(const vec3_t in, const vec3_t delta, vec3_t out)
{
	vec3_t x = { 1, 0, 0 };
	vec3_t y = { 0, 1, 0 };
	float angle, c, s;
	float xtemp, ytemp, ztemp;

	if (delta[ROLL] != 0)
	{
		// Rotate about the X axis by delta roll
		angle = DEG2RAD(delta[ROLL]);
		c = cosf(angle);
		s = sinf(angle);
		ytemp = c * x[1] - s * x[2];
		ztemp = c * x[2] + s * x[1];
		x[1] = ytemp;
		x[2] = ztemp;
		ytemp = c * y[1] - s * y[2];
		ztemp = c * y[2] + s * y[1];
		y[1] = ytemp;
		y[2] = ztemp;
	}

	if (delta[PITCH] != 0)
	{
		// Rotate about the Y axis by delta yaw
		angle = -DEG2RAD(delta[PITCH]);
		c = cosf(angle);
		s = sinf(angle);
		ztemp = c * x[2] - s * x[0];
		xtemp = c * x[0] + s * x[2];
		x[0] = xtemp;
		x[2] = ztemp;
		ztemp = c * y[2] - s * y[0];
		xtemp = c * y[0] + s * y[2];
		y[0] = xtemp;
		y[2] = ztemp;
	}

	if (delta[YAW] != 0)
	{
		// Rotate about the Z axis by delta yaw
		angle = DEG2RAD(delta[YAW]);
		c = cosf(angle);
		s = sinf(angle);
		xtemp = c * x[0] - s * x[1];
		ytemp = c * x[1] + s * x[0];
		x[0] = xtemp;
		x[1] = ytemp;
		xtemp = c * y[0] - s * y[1];
		ytemp = c * y[1] + s * y[0];
		y[0] = xtemp;
		y[1] = ytemp;
	}

	if (in[ROLL] != 0)
	{
		// Rotate about X axis by input roll
		angle = DEG2RAD(in[ROLL]);
		c = cosf(angle);
		s = sinf(angle);
		ytemp = c * x[1] - s * x[2];
		ztemp = c * x[2] + s * x[1];
		x[1] = ytemp;
		x[2] = ztemp;
		ytemp = c * y[1] - s * y[2];
		ztemp = c * y[2] + s * y[1];
		y[1] = ytemp;
		y[2] = ztemp;
	}

	if (in[PITCH] != 0)
	{
		// Rotate about Y axis by input pitch
		angle = -DEG2RAD(in[PITCH]);
		c = cosf(angle);
		s = sinf(angle);
		ztemp = c * x[2] - s * x[0];
		xtemp = c * x[0] + s * x[2];
		x[0] = xtemp;
		x[2] = ztemp;
		ztemp = c * y[2] - s * y[0];
		xtemp = c * y[0] + s * y[2];
		y[0] = xtemp;
		y[2] = ztemp;
	}

	if (in[YAW] != 0)
	{
		// Rotate about Z axis by input yaw
		angle = DEG2RAD(in[YAW]);
		c = cosf(angle);
		s = sinf(angle);
		xtemp = c * x[0] - s * x[1];
		ytemp = c * x[1] + s * x[0];
		x[0] = xtemp;
		x[1] = ytemp;
		xtemp = c * y[0] - s * y[1];
		ytemp = c * y[1] + s * y[0];
		y[0] = xtemp;
		y[1] = ytemp;
	}

	out[YAW] = (180.0f / M_PI) * atan2(x[1], x[0]);
	if (out[YAW] != 0)
	{
		angle = -DEG2RAD(out[YAW]);
		c = cosf(angle);
		s = sinf(angle);
		xtemp = c * x[0] - s * x[1];
		ytemp = c * x[1] + s * x[0];
		x[0] = xtemp;
		x[1] = ytemp;
		xtemp = c * y[0] - s * y[1];
		ytemp = c * y[1] + s * y[0];
		y[0] = xtemp;
		y[1] = ytemp;
	}

	out[PITCH] = (180.0f / M_PI) * atan2(x[2], x[0]);
	if (out[PITCH] != 0)
	{
		angle = DEG2RAD(out[PITCH]);
		c = cosf(angle);
		s = sinf(angle);
		ztemp = c * y[2] - s * y[0];
		xtemp = c * y[0] + s * y[2];
		y[0] = xtemp;
		y[2] = ztemp;
	}

	out[ROLL] = (180.0f / M_PI) * atan2(y[2], y[1]);
}

void AngleVectors(const vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	if (!angles)
		return;

	float angle = angles[YAW] * (M_PI2 / 360);
	const float sy = sinf(angle);
	const float cy = cosf(angle);

	angle = angles[PITCH] * (M_PI2 / 360);
	const float sp = sinf(angle);
	const float cp = cosf(angle);

	angle = angles[ROLL] * (M_PI2 / 360);
	const float sr = sinf(angle);
	const float cr = cosf(angle);

	if (forward)
	{
		forward[0] = cp * cy;
		forward[1] = cp * sy;
		forward[2] = -sp;
	}

	if (right)
	{
		right[0] = (-1 * sr * sp * cy) + (-1 * cr * -sy);
		right[1] = (-1 * sr * sp * sy) + (-1 * cr * cy);
		right[2] = -1 * sr * cp;
	}

	if (up)
	{
		up[0] = (cr * sp * cy) + (-sr * -sy);
		up[1] = (cr * sp * sy) + (-sr * cy);
		up[2] = cr * cp;
	}
}

//mxd
void VectorsToAngles(const vec3_t forward, const vec3_t right, const vec3_t up, vec3_t ang)
{
	double sp = forward[2];

	// Cap off our sin value so that we don't get any NANs
	sp = clamp(sp, -1.0, 1.0);

	const double theta = -asin(sp);
	const double cp = cos(theta);

	ang[0] = theta * 180 / M_PI;

	if (cp > 0.000977) // 0.000977 = 8192 * FLT_EPSILON
	{
		ang[1] = atan2(forward[1], forward[0]) * 180 / M_PI;
		ang[2] = atan2(-right[2], up[2]) * 180 / M_PI;
	}
	else
	{
		ang[1] = -atan2(right[0], right[1]) * 180 / M_PI;
		ang[2] = 0;
	}
}

void MakeNormalVectors(const vec3_t forward, vec3_t right, vec3_t up)
{
	// This rotate and negate guarantees a vector not colinear with the original
	right[1] = -forward[0];
	right[2] = forward[1];
	right[0] = forward[2];

	const float d = DotProduct(right, forward);
	VectorMA(right, -d, forward, right);
	VectorNormalize(right);
	CrossProduct(right, forward, up);
}

void VecToAngleRolled(vec3_t value1, float angleyaw, vec3_t angles)
{
	const float yaw = (int)(atan2(value1[1], value1[0]) * 180 / M_PI);
	const float forward = sqrtf(value1[0] * value1[0] + value1[1] * value1[1]);
	float pitch = (int)(atan2(value1[2], forward) * 180 / M_PI);

	if (pitch < 0)
		pitch += 360;

	angles[PITCH] = -pitch;
	angles[YAW] =  yaw;
	angles[ROLL] = -angleyaw;
}

void ProjectPointOnPlane(vec3_t dst, const vec3_t p, const vec3_t normal)
{
	const float inv_denom = 1.0f / DotProduct(normal, normal);
	const float d = DotProduct(normal, p) * inv_denom;

	for (int c = 0; c < 3; c++)
	{
		const float n = normal[c] * inv_denom;
		dst[c] = p[c] - d * n;
	}
}

// Assumes "src" is normalized
void PerpendicularVector(vec3_t dst, const vec3_t src)
{
	int pos = 0;
	float minelem = 1.0f;
	vec3_t tempvec;

	// Find the smallest magnitude axially aligned vector
	for (int i = 0; i < 3; i++)
	{
		if (fabs(src[i]) < minelem)
		{
			pos = i;
			minelem = fabs(src[i]);
		}
	}

	VectorClear(tempvec);
	tempvec[pos] = 1.0f;

	// Project the point onto the plane defined by src
	ProjectPointOnPlane(dst, tempvec, src);

	// Normalize the result
	VectorNormalize(dst);
}

void R_ConcatRotations(float in1[3][3], float in2[3][3], float out[3][3])
{
	for(int c = 0; c < 3; c++)
	{
		out[0][c] = in1[0][0] * in2[0][c] + in1[0][1] * in2[1][c] + in1[0][2] * in2[2][c];
		out[1][c] = in1[1][0] * in2[0][c] + in1[1][1] * in2[1][c] + in1[1][2] * in2[2][c];
		out[2][c] = in1[2][0] * in2[0][c] + in1[2][1] * in2[1][c] + in1[2][2] * in2[2][c];
	}
}

void R_ConcatTransforms(float in1[3][4], float in2[3][4], float out[3][4])
{
	out[0][0] = in1[0][0] * in2[0][0] + in1[0][1] * in2[1][0] + in1[0][2] * in2[2][0];
	out[0][1] = in1[0][0] * in2[0][1] + in1[0][1] * in2[1][1] + in1[0][2] * in2[2][1];
	out[0][2] = in1[0][0] * in2[0][2] + in1[0][1] * in2[1][2] + in1[0][2] * in2[2][2];
	out[0][3] = in1[0][0] * in2[0][3] + in1[0][1] * in2[1][3] + in1[0][2] * in2[2][3] + in1[0][3];

	out[1][0] = in1[1][0] * in2[0][0] + in1[1][1] * in2[1][0] + in1[1][2] * in2[2][0];
	out[1][1] = in1[1][0] * in2[0][1] + in1[1][1] * in2[1][1] + in1[1][2] * in2[2][1];
	out[1][2] = in1[1][0] * in2[0][2] + in1[1][1] * in2[1][2] + in1[1][2] * in2[2][2];
	out[1][3] = in1[1][0] * in2[0][3] + in1[1][1] * in2[1][3] + in1[1][2] * in2[2][3] + in1[1][3];

	out[2][0] = in1[2][0] * in2[0][0] + in1[2][1] * in2[1][0] + in1[2][2] * in2[2][0];
	out[2][1] = in1[2][0] * in2[0][1] + in1[2][1] * in2[1][1] + in1[2][2] * in2[2][1];
	out[2][2] = in1[2][0] * in2[0][2] + in1[2][1] * in2[1][2] + in1[2][2] * in2[2][2];
	out[2][3] = in1[2][0] * in2[0][3] + in1[2][1] * in2[1][3] + in1[2][2] * in2[2][3] + in1[2][3];
}

//============================================================================

float LerpAngle(float a1, float a2, float frac)
{
	if (a2 - a1 > 180)
		a2 -= 360;
	else if (a2 - a1 < -180)
		a2 += 360;
	return a1 + frac * (a2 - a1);
}

float anglemod(float a)
{
	a = (360.0 / 65536) * ((int)(a * (65536 / 360.0)) & 65535);
	return a;
}

int BoxOnPlaneSide2(vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	vec3_t corners[2];

	for (int i = 0; i < 3; i++)
	{
		if (p->normal[i] < 0)
		{
			corners[0][i] = emins[i];
			corners[1][i] = emaxs[i];
		}
		else
		{
			corners[1][i] = emins[i];
			corners[0][i] = emaxs[i];
		}
	}

	const float dist1 = DotProduct(p->normal, corners[0]) - p->dist;
	const float dist2 = DotProduct(p->normal, corners[1]) - p->dist;
	int sides = 0;

	if (dist1 >= 0)
		sides = 1;
	if (dist2 < 0)
		sides |= 2;

	return sides;
}

// Returns 1, 2, or 1 + 2
int BoxOnPlaneSide(vec3_t emins, vec3_t emaxs, struct cplane_s *plane)
{
	float dist1, dist2;

	// Fast axial cases
	if (plane->type < 3)
	{
		if (plane->dist <= emins[plane->type])
			return 1;
		if (plane->dist >= emaxs[plane->type])
			return 2;
		return 3;
	}
	
	// General case
	switch (plane->signbits)
	{
		case 0:
			dist1 = plane->normal[0] * emaxs[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emaxs[2];
			dist2 = plane->normal[0] * emins[0] + plane->normal[1] * emins[1] + plane->normal[2] * emins[2];
			break;

		case 1:
			dist1 = plane->normal[0] * emins[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emaxs[2];
			dist2 = plane->normal[0] * emaxs[0] + plane->normal[1] * emins[1] + plane->normal[2] * emins[2];
			break;

		case 2:
			dist1 = plane->normal[0] * emaxs[0] + plane->normal[1] * emins[1] + plane->normal[2] * emaxs[2];
			dist2 = plane->normal[0] * emins[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emins[2];
			break;

		case 3:
			dist1 = plane->normal[0] * emins[0] + plane->normal[1] * emins[1] + plane->normal[2] * emaxs[2];
			dist2 = plane->normal[0] * emaxs[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emins[2];
			break;

		case 4:
			dist1 = plane->normal[0] * emaxs[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emins[2];
			dist2 = plane->normal[0] * emins[0] + plane->normal[1] * emins[1] + plane->normal[2] * emaxs[2];
			break;

		case 5:
			dist1 = plane->normal[0] * emins[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emins[2];
			dist2 = plane->normal[0] * emaxs[0] + plane->normal[1] * emins[1] + plane->normal[2] * emaxs[2];
			break;

		case 6:
			dist1 = plane->normal[0] * emaxs[0] + plane->normal[1] * emins[1] + plane->normal[2] * emins[2];
			dist2 = plane->normal[0] * emins[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emaxs[2];
			break;

		case 7:
			dist1 = plane->normal[0] * emins[0] + plane->normal[1] * emins[1] + plane->normal[2] * emins[2];
			dist2 = plane->normal[0] * emaxs[0] + plane->normal[1] * emaxs[1] + plane->normal[2] * emaxs[2];
			break;

		default: // Shut up compiler
			dist1 = 0;
			dist2 = 0; 
			break;
	}

	int sides = 0;
	if (dist1 >= plane->dist)
		sides = 1;
	if (dist2 < plane->dist)
		sides |= 2;

	return sides;
}

void ClearBounds(vec3_t mins, vec3_t maxs)
{
	VectorSetAll(mins, 99999);
	VectorSetAll(maxs, -99999);
}

void AddPointToBounds(const vec3_t v, vec3_t mins, vec3_t maxs)
{
	for (int i = 0; i < 3; i++)
	{
		mins[i] = min(v[i], mins[i]);
		maxs[i] = max(v[i], maxs[i]);
	}
}

int VectorCompare(const vec3_t v1, const vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
		return 0;
			
	return 1;
}

float VectorNormalize(vec3_t v)
{
	const float length = VectorLength(v);

	if (length)
	{
		const float ilength = 1 / length;
		VectorScale(v, ilength, v);
	}

	return length;
}

float VectorNormalize2(const vec3_t v, vec3_t out)
{
	const float length = VectorLength(v);

	if (length)
	{
		const float ilength = 1 / length;
		VectorScale(v, ilength, out);
	}
		
	return length;
}

// From Q2E
void VectorNormalizeFast(vec3_t v)
{
	const float ilength = Q_rsqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

void VectorMA(const vec3_t veca, const float scale, const vec3_t vecb, vec3_t vecc)
{
	for(int c = 0; c < 3; c++)
		vecc[c] = veca[c] + scale * vecb[c];
}

float _DotProduct(const vec3_t v1, const vec3_t v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void _VectorSubtract(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = veca[c] - vecb[c];
}

void _VectorAdd(const vec3_t veca, const vec3_t vecb, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = veca[c] + vecb[c];
}

void _VectorCopy(const vec3_t in, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = in[c];
}

void CrossProduct(const vec3_t v1, const vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

float VectorLength(const vec3_t v)
{
	float length = 0;
	for (int i = 0; i < 3; i++)
		length += v[i] * v[i];

	return sqrtf(length); // FIXME
}

void VectorInverse(vec3_t v)
{
	for (int c = 0; c < 3; c++)
		v[c] = -v[c];
}

void VectorScale(const vec3_t in, const vec_t scale, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = in[c] * scale;
}

// From Q2E
void VectorRotate(const vec3_t v, const vec3_t matrix[3], vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = v[0] * matrix[c][0] + v[1] * matrix[c][1] + v[2] * matrix[c][2];
}

/*
=======================================================================
	Matrix operations (mxd)
=======================================================================
*/

void Matrix4Invert(float m[16])
{
	float inv[16];

	inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15]
		+ m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
	inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15]
		- m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
	inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15]
		+ m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
	inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14]
		- m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
	inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15]
		- m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
	inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15]
		+ m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
	inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15]
		- m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
	inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14]
		+ m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
	inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15]
		+ m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
	inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15]
		- m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
	inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15]
		+ m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
	inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14]
		- m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
	inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11]
		- m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
	inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11]
		+ m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
	inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11]
		- m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
	inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10]
		+ m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

	double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
	if (det == 0)
		return;

	det = 1.0f / det;

	for (int i = 0; i < 16; i++)
		m[i] = inv[i] * det;
}

void Matrix4Multiply(const float m[16], const float v[4], float result[4])
{
	memset(result, 0, sizeof(result[0]) * 4);
	for (int i = 0; i < 4; i++) // for each row
		for (int j = 0; j < 4; j++) // for each col
			result[i] += m[j * 4 + i] * v[j];
}

void Matrix3Multiply(const float m[9], const float v[3], float result[3])
{
	memset(result, 0, sizeof(result[0]) * 3);
	for (int i = 0; i < 3; i++) // for each row
		for (int j = 0; j < 3; j++) // for each col
			result[i] += m[j * 3 + i] * v[j];
}

// From Q2E
float Q_rsqrt(float in)
{
	const float x = in * 0.5f;
	int i = *(int *)&in;
	i = 0x5f3759df - (i >> 1);
	float y = *(float *)&i;
	y = y * (1.5f - (x * y * y));

	return y;
}

int Q_log2(int val)
{
	int answer = 0;
	while (val >>= 1)
		answer++;

	return answer;
}

// From Q2E
void AnglesToAxis(const vec3_t angles, vec3_t axis[3])
{
	static float sp, sy, sr, cp, cy, cr;

	float angle = DEG2RAD(angles[PITCH]);
	sp = sinf(angle);
	cp = cosf(angle);

	angle = DEG2RAD(angles[YAW]);
	sy = sinf(angle);
	cy = cosf(angle);

	angle = DEG2RAD(angles[ROLL]);
	sr = sinf(angle);
	cr = cosf(angle);

	axis[0][0] = cp * cy;
	axis[0][1] = cp * sy;
	axis[0][2] = -sp;

	axis[1][0] = sr * sp * cy + cr * -sy;
	axis[1][1] = sr * sp * sy + cr * cy;
	axis[1][2] = sr * cp;

	axis[2][0] = cr * sp * cy + (-sr * -sy);
	axis[2][1] = cr * sp * sy + (-sr * cy);
	axis[2][2] = cr * cp;
}

// From Q2E
void AxisClear(vec3_t axis[3])
{
	axis[0][0] = 1;
	axis[0][1] = 0;
	axis[0][2] = 0;

	axis[1][0] = 0;
	axis[1][1] = 1;
	axis[1][2] = 0;

	axis[2][0] = 0;
	axis[2][1] = 0;
	axis[2][2] = 1;
}

// From Q2E
void AxisCopy(const vec3_t in[3], vec3_t out[3])
{
	for(int i = 0; i < 3; i++)
		for (int c = 0; c < 3; c++)
			out[i][c] = in[i][c];
}

// From Q2E
qboolean AxisCompare(const vec3_t axis1[3], const vec3_t axis2[3])
{
	if (axis1[0][0] != axis2[0][0] || axis1[0][1] != axis2[0][1] || axis1[0][2] != axis2[0][2])
		return false;

	if (axis1[1][0] != axis2[1][0] || axis1[1][1] != axis2[1][1] || axis1[1][2] != axis2[1][2])
		return false;

	if (axis1[2][0] != axis2[2][0] || axis1[2][1] != axis2[2][1] || axis1[2][2] != axis2[2][2])
		return false;

	return true;
}

#pragma endregion

#pragma region ======================= File path functions 

// Returns filename ("c:/dir/file.txt" -> "file.txt") //mxd. Never used
char *COM_SkipPath(char *pathname)
{
	char *last = pathname;
	while (*pathname)
	{
		if (*pathname == '/' || *pathname == '\\')
			last = pathname + 1;

		pathname++;
	}

	return last;
}

// Returns filename without extension ("c:/dir/file.txt" -> "c:/dir/file")
void COM_StripExtension(char *in, char *out)
{
	//mxd. Let's find the last dot instead of first...
	strcpy(out, in);
	char *ext = strrchr(out, '.');
	if(ext && ext != out)
		*ext = 0;
}

// Returns file extension. Never returns NULL. ("c:/dir/file.txt" -> "txt"; "c:/dir" -> "")
const char *COM_FileExtension(const char *in)
{
	//mxd. YQ2 implementation
	const char *ext = strrchr(in, '.');
	return ((!ext || ext == in) ? "" : ext + 1);
}

// Returns filename without extension ("c:/dir/file.txt" -> "file") //mxd. Never used
void COM_FileBase(char *in, char *out)
{
	char *s = in + strlen(in) - 1;
	
	while (s != in && *s != '.')
		s--;
	
	char *s2 = s;
	while (s2 != in && *s2 != '/')
		s2--;
	
	if (s - s2 < 2)
	{
		out[0] = 0;
	}
	else
	{
		s--;
		strncpy(out, s2 + 1, s - s2);
		out[s - s2] = 0;
	}
}

// Returns the path up to, but not including the last '/' ("c:/dir/file.txt" -> "c:/dir")
void COM_FilePath(char *in, char *out)
{
	char *s = in + strlen(in) - 1;
	
	while (s != in && *s != '/')
		s--;

	strncpy(out, in, s - in);
	out[s - in] = 0;
}

void COM_DefaultExtension(char *path, size_t pathSize, char *extension)
{
	// If path doesn't have a .EXT, append extension (extension should include the .)
	char *src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return; // It has an extension

		src--;
	}

	Q_strncatz(path, extension, pathSize);
}

#pragma endregion 

#pragma region ======================= Byte order functions

short BigShort(short l)
{
	const byte b1 = l & 255;
	const byte b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

#pragma endregion

#pragma region ======================= Utility functions

// Does a varargs printf into a temp buffer, so I don't need to have varargs versions of all text functions.
char *va(char *format, ...)
{
	va_list argptr;
	static char	string[1024];
	
	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	return string;
}

// A convenience function for making temporary vectors for function calls
float *tv(const float x, const float y, const float z) //mxd. Moved from g_utils.c
{
	static int index;
	static vec3_t vecs[8];

	// Use an array so that multiple tempvectors won't collide for a while
	float *v = vecs[index];
	index = (index + 1) & 7;
	VectorSet(v, x, y, z);

	return v;
}

// A convenience function for printing vectors
char *vtos(const vec3_t v) //mxd. Moved from g_utils.c
{
	static int index;
	static char str[8][32];

	// Use an array so that multiple vtos won't collide
	char* s = str[index];
	index = (index + 1) & 7;

	Com_sprintf(s, 32, "(%g %g %g)", v[0], v[1], v[2]); //mxd. %i -> %g

	return s;
}

#pragma endregion

#pragma region ======================= Text parsing

char *COM_SkipWhiteSpace(char *data_p, qboolean *hasNewLines)
{
	while (*data_p <= ' ')
	{
		if (!*data_p)
			return NULL;

		if (*data_p == '\n')
			*hasNewLines = true;

		data_p++;
	}

	return data_p;
}

// Skips until a matching close brace is found.
// Internal brace depths are properly skipped.
void COM_SkipBracedSection(char **data_p, int depth)
{
	do
	{
		char *tok = COM_ParseExt(data_p, true);
		if (tok[1] == 0)
		{
			if (tok[0] == '{')
				depth++;
			else if (tok[0] == '}')
				depth--;
		}
	} while (depth && *data_p);
}

// Skips until a new line is found
void COM_SkipRestOfLine(char **data_p)
{
	while (true)
	{
		char *tok = COM_ParseExt(data_p, false);
		if (!tok[0])
			break;
	}
}

// Parses a token out of a string
char *COM_Parse(char **data_p)
{
	static char com_token[MAX_TOKEN_CHARS]; //mxd. Made local
	
	int c;

	char *data = *data_p;
	int len = 0;
	com_token[0] = 0;
	
	if (!data)
	{
		*data_p = NULL;
		return "";
	}
		
	// Skip whitespace
skipwhite:
	while ((c = *data) <= ' ')
	{
		if (c == 0)
		{
			*data_p = NULL;
			return "";
		}

		data++;
	}
	
	// Skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;

		goto skipwhite;
	}

	// Handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (true)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				if (len == MAX_TOKEN_CHARS)	// Knightmare- discard if > MAX_TOKEN_CHARS-1
				{
					len = 0;

					//mxd. Print warning...
					Com_Printf(S_COLOR_YELLOW"%s: maximum token length exceeded (%i / %i chars) while parsing '%s'\n", __func__, strlen(*data_p), MAX_TOKEN_CHARS - 1, * data_p);
				}
				
				com_token[len] = 0;
				*data_p = data;

				return com_token;
			}

			if (len < MAX_TOKEN_CHARS)
			{
				com_token[len] = c;
				len++;
			}
		}
	}

	// Parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
		{
			com_token[len] = c;
			len++;
		}

		data++;
		c = *data;
	} while (c > 32);

	if (len == MAX_TOKEN_CHARS)
	{
		len = 0;

		//mxd. Print warning...
		Com_Printf(S_COLOR_YELLOW"%s: maximum token length exceeded (%i / %i chars) while parsing '%s'\n", __func__, strlen(*data_p), MAX_TOKEN_CHARS - 1, *data_p);
	}

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

// Parse a token out of a string. From Q2E
char *COM_ParseExt(char **data_p, qboolean allowNewLines)
{
	static char com_token[MAX_TOKEN_CHARS]; //mxd. Made local
	
	int c;
	int len = 0;
	qboolean hasNewLines = false;
	char *data = *data_p;

	com_token[0] = 0;

	// Make sure incoming data is valid
	if (!data)
	{
		*data_p = NULL;
		return com_token;
	}

	while (true)
	{	
		// Skip whitespace
		data = COM_SkipWhiteSpace(data, &hasNewLines);
		if (!data)
		{
			*data_p = NULL;
			return com_token;
		}

		if (hasNewLines && !allowNewLines)
		{
			*data_p = data;
			return com_token;
		}

		c = *data;
	
		// Skip // comments
		if (c == '/' && data[1] == '/')
		{
			while (*data && *data != '\n')
				data++;
		}
		else if (c == '/' && data[1] == '*') // Skip /* */ comments
		{
			data += 2;

			while (*data && (*data != '*' || data[1] != '/'))
				data++;

			if (*data)
				data += 2;
		}
		else // An actual token
		{
			break;
		}
	}

	// Handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (true)
		{
			c = *data++;

			if (c == '\"' || !c)
			{
				if (len == MAX_TOKEN_CHARS)
				{
					len = 0;

					//mxd. Print warning...
					Com_Printf(S_COLOR_YELLOW"%s: maximum token length exceeded (%i / %i chars) while parsing '%s'\n", __func__, strlen(*data_p), MAX_TOKEN_CHARS - 1, *data_p);
				}

				com_token[len] = 0;

				*data_p = data;
				return com_token;
			}

			if (len < MAX_TOKEN_CHARS)
				com_token[len++] = c;
		}
	}

	// Parse a regular word
	do
	{
		if (len < MAX_TOKEN_CHARS)
			com_token[len++] = c;

		data++;
		c = *data;
	} while (c > 32);

	if (len == MAX_TOKEN_CHARS)
	{
		len = 0;

		//mxd. Print warning...
		Com_Printf(S_COLOR_YELLOW"%s: maximum token length exceeded (%i / %i chars) while parsing '%s'\n", __func__, strlen(*data_p), MAX_TOKEN_CHARS - 1, *data_p);
	}

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}

#pragma endregion

#pragma region ======================= Library replacement functions

// FIXME: replace all Q_stricmp with Q_strcasecmp
int Q_stricmp(const char *s1, const char *s2)
{
#ifdef _WIN32
	return _stricmp(s1, s2);
#else
	return strcasecmp(s1, s2);
#endif
}

//mxd. Case-insensitive version of strstr. https://stackoverflow.com/questions/27303062/strstr-function-like-that-ignores-upper-or-lower-case
char *Q_strcasestr(const char *haystack, const char *needle)
{
#ifdef _WIN32
	const int c = tolower((unsigned char)*needle);

	if (c == '\0')
		return (char *)haystack;

	for (; *haystack; haystack++)
	{
		if (tolower((unsigned char)*haystack) == c)
		{
			for (size_t i = 0;;)
			{
				if (needle[++i] == '\0')
					return (char *)haystack;

				if (tolower((unsigned char)haystack[i]) != tolower((unsigned char)needle[i]))
					break;
			}
		}
	}

	return NULL;
#else
	return strcasestr(s1, s2); // Never tested...
#endif
}

int Q_strncasecmp(const char *s1, const char *s2, int n)
{
	int c1, c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0; // Strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');

			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');

			if (c1 != c2)
				return -1; // Strings not equal
		}
	} while (c1);
	
	return 0; // Strings are equal
}

int Q_strcasecmp(const char *s1, const char *s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}

// Safe strncpy that ensures a trailing zero
void Q_strncpyz(char *dst, const char *src, size_t dstSize)
{
	if (!src || !dst || dstSize < 1) 
		return;
	
	strncpy(dst, src, dstSize - 1);
	dst[dstSize - 1] = 0;
}

// Safe strncat that ensures a trailing zero
void Q_strncatz(char *dst, const char *src, size_t dstSize)
{
	if (!src || !dst || dstSize < 1) 
		return;
	
	while (--dstSize && *dst)
		dst++;

	if (dstSize > 0)
	{
		while (--dstSize && *src)
			*dst++ = *src++;

		*dst = 0;
	}
}

// Safe snprintf that ensures a trailing zero
void Q_snprintfz(char *dst, size_t dstSize, const char *fmt, ...)
{
	if (!dst || dstSize < 1) 
		return;

	va_list argPtr;
	va_start(argPtr, fmt);
	Q_vsnprintf(dst, dstSize, fmt, argPtr);
	va_end(argPtr);

	dst[dstSize - 1] = 0;
}

char *Q_strlwr(char *string)
{
	char *s = string;

	while (*s)
	{
		*s = tolower(*s);
		s++;
	}

	return string;
}

char *Q_strupr(char *string)
{
	char *s = string;

	while (*s)
	{
		*s = toupper(*s);
		s++;
	}

	return string;
}

void Com_sprintf(char *dest, size_t size, char *fmt, ...)
{
	static char bigbuffer[0x10000]; //mxd. +static
	va_list argptr;

	va_start(argptr, fmt);
	const int len = Q_vsnprintf(bigbuffer, sizeof(bigbuffer), fmt, argptr);
	va_end(argptr);

	if (len < 0)
		Com_Printf("Com_sprintf: overflow in temp buffer of size %i\n", sizeof(bigbuffer));
	else if (len >= size)
		Com_Printf("Com_sprintf: overflow of %i in dest buffer of size %i\n", len, size);

	strncpy(dest, bigbuffer, size - 1);
	dest[size - 1] = 0;
}

#pragma endregion

#pragma region ======================= Info strings

// Searches the string for the given key and returns the associated value, or an empty string.
char *Info_ValueForKey(char *s, char *key)
{
	static char value[2][512]; // Use two buffers so compares work without stomping on each other
	static int valueindex;

	char pkey[512];

	valueindex ^= 1;
	if (*s == '\\')
		s++;

	while (true)
	{
		char *o = pkey;

		while (*s != '\\')
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value[valueindex];

		while (*s && *s != '\\')
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey))
			return value[valueindex];

		if (!*s)
			return "";

		s++;
	}
}

void Info_RemoveKey(char *s, char *key)
{
	char pkey[512];
	char value[512];

	if (strchr(key, '\\')) //mxd. strstr -> strchr
		return;

	while (true)
	{
		char *start = s;
		if (*s == '\\')
			s++;

		char *o = pkey;
		while (*s != '\\')
		{
			if (!*s)
				return;
			*o++ = *s++;
		}
		*o = 0;
		s++;

		o = value;
		while (*s && *s != '\\')
		{
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey))
		{
			strcpy(start, s); // Remove this part
			return;
		}

		if (!*s)
			return;
	}
}

// Some characters are illegal in info strings because they can mess up the server's parsing
qboolean Info_Validate(char *s)
{
	return (!strchr(s, '\"') && !strchr(s, ';'));
}

void Info_SetValueForKey(char *s, char *key, char *value)
{
	if (strchr(key, '\\') || (value && strchr(value, '\\')))
	{
		Com_Printf(S_COLOR_YELLOW"Can't use keys or values with a '\\' character (key: '%s', value: '%s').\n", key, value);
		return;
	}

	if (strchr(key, ';'))
	{
		Com_Printf(S_COLOR_YELLOW"Can't use keys with a semicolon (key: '%s').\n", key);
		return;
	}

	if (strchr(key, '\"') || (value && strchr(value, '\"')))
	{
		Com_Printf(S_COLOR_YELLOW"Can't use keys or values with a '\"' character (key: '%s', value: '%s').\n", key, value);
		return;
	}

	if (strlen(key) > MAX_INFO_KEY - 1 || (value && strlen(value) > MAX_INFO_KEY - 1))
	{
		Com_Printf(S_COLOR_YELLOW"Keys and values must be less than %i characters long (key: '%s' [%i chars], value: '%s' [%i chars]).\n", MAX_INFO_KEY, key, strlen(key), value, strlen(value));
		return;
	}

	Info_RemoveKey(s, key);
	if (!value || !strlen(value))
		return;

	char newi[MAX_INFO_STRING];
	Com_sprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > MAX_INFO_STRING)
	{
		Com_Printf(S_COLOR_YELLOW"Info string length exceeded (%i / %i chars).\n", strlen(newi) + strlen(s), MAX_INFO_STRING);
		return;
	}

	// Only copy ascii values
	s += strlen(s);
	char *v = newi;
	while (*v)
	{
		int c = *v++;
		c &= 127; // Strip high bits

		if (c >= 32 && c < 127)
			*s++ = c;
	}

	*s = 0;
}

#pragma endregion