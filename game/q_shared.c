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

#include "q_shared.h"
#include "laz_misc.h"

#ifdef _WIN32
#include "../win32/winquake.h"
#endif

vec2_t vec2_origin = { 0, 0 };
vec3_t vec3_origin = { 0, 0, 0 };
vec4_t vec4_origin = { 0, 0, 0, 0 };

//============================================================================

#ifdef _WIN32
#pragma optimize( "", off )
#endif

void RotatePointAroundVector(vec3_t dst, const vec3_t dir, const vec3_t point, float degrees)
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

#ifdef _WIN32
#pragma optimize( "", on )
#endif


void AngleVectors (vec3_t angles, vec3_t forward, vec3_t right, vec3_t up)
{
	static float sr, sp, sy, cr, cp, cy;
	// static to help MS compiler fp bugs

	if (!angles)
		return;

	float angle = angles[YAW] * (M_PI2 / 360);
	sy = sinf(angle);
	cy = cosf(angle);

	angle = angles[PITCH] * (M_PI2 / 360);
	sp = sinf(angle);
	cp = cosf(angle);

	angle = angles[ROLL] * (M_PI2 / 360);
	sr = sinf(angle);
	cr = cosf(angle);

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

void MakeNormalVectors (vec3_t forward, vec3_t right, vec3_t up)
{
	// this rotate and negate guarantees a vector not colinear with the original
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

	// find the smallest magnitude axially aligned vector
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

	// project the point onto the plane defined by src
	ProjectPointOnPlane(dst, tempvec, src);

	// normalize the result
	VectorNormalize(dst);
}


/*
================
R_ConcatRotations
================
*/
void R_ConcatRotations (float in1[3][3], float in2[3][3], float out[3][3])
{
	for(int c = 0; c < 3; c++)
	{
		out[0][c] = in1[0][0] * in2[0][c] + in1[0][1] * in2[1][c] + in1[0][2] * in2[2][c];
		out[1][c] = in1[1][0] * in2[0][c] + in1[1][1] * in2[1][c] + in1[1][2] * in2[2][c];
		out[2][c] = in1[2][0] * in2[0][c] + in1[2][1] * in2[1][c] + in1[2][2] * in2[2][c];
	}
}


/*
================
R_ConcatTransforms
================
*/
void R_ConcatTransforms (float in1[3][4], float in2[3][4], float out[3][4])
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


float Q_fabs (float f)
{
	int tmp = *(int *)&f;
	tmp &= 0x7FFFFFFF;
	return *(float *)&tmp;
}

#if defined _M_IX86 && !defined C_ONLY
#pragma warning (disable:4035)
__declspec(naked) long Q_ftol(float f)
{
	static int tmp;
	__asm fld dword ptr [esp+4]
	__asm fistp tmp
	__asm mov eax, tmp
	__asm ret
}
#pragma warning (default:4035)
#endif


/*
===============
LerpAngle
===============
*/
float LerpAngle (float a2, float a1, float frac)
{
	if (a1 - a2 > 180)
		a1 -= 360;
	if (a1 - a2 < -180)
		a1 += 360;
	return a2 + frac * (a1 - a2);
}


float anglemod(float a)
{
	a = (360.0 / 65536) * ((int)(a * (65536 / 360.0)) & 65535);
	return a;
}


// this is the slow, general version
int BoxOnPlaneSide2 (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
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

/*
==================
BoxOnPlaneSide

Returns 1, 2, or 1 + 2
==================
*/
//#if !id386 || defined __linux__ 
//#ifndef id386
#ifndef _WIN32
int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	float dist1, dist2;

	// fast axial cases
	if (p->type < 3)
	{
		if (p->dist <= emins[p->type])
			return 1;
		if (p->dist >= emaxs[p->type])
			return 2;
		return 3;
	}
	
// general case
	switch (p->signbits)
	{
	case 0:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		break;

	case 1:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		break;

	case 2:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;

	case 3:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		break;

	case 4:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;

	case 5:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emaxs[2];
		break;

	case 6:
		dist1 = p->normal[0] * emaxs[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emins[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		break;

	case 7:
		dist1 = p->normal[0] * emins[0] + p->normal[1] * emins[1] + p->normal[2] * emins[2];
		dist2 = p->normal[0] * emaxs[0] + p->normal[1] * emaxs[1] + p->normal[2] * emaxs[2];
		break;

	default:
		dist1 = dist2 = 0; // shut up compiler
		break;
	}

	int sides = 0;
	if (dist1 >= p->dist)
		sides = 1;
	if (dist2 < p->dist)
		sides |= 2;

	assert(sides != 0);

	return sides;
}
#else
#pragma warning( disable: 4035 )

__declspec(naked) int BoxOnPlaneSide (vec3_t emins, vec3_t emaxs, struct cplane_s *p)
{
	static int bops_initialized;
	static int Ljmptab[8];

	__asm {

		push ebx
			
		cmp bops_initialized, 1
		je  initialized
		mov bops_initialized, 1
		
		mov Ljmptab[0*4], offset Lcase0
		mov Ljmptab[1*4], offset Lcase1
		mov Ljmptab[2*4], offset Lcase2
		mov Ljmptab[3*4], offset Lcase3
		mov Ljmptab[4*4], offset Lcase4
		mov Ljmptab[5*4], offset Lcase5
		mov Ljmptab[6*4], offset Lcase6
		mov Ljmptab[7*4], offset Lcase7
			
initialized:

		mov edx,ds:dword ptr[4+12+esp]
		mov ecx,ds:dword ptr[4+4+esp]
		xor eax,eax
		mov ebx,ds:dword ptr[4+8+esp]
		mov al,ds:byte ptr[17+edx]
		cmp al,8
		jge Lerror
		fld ds:dword ptr[0+edx]
		fld st(0)
		jmp dword ptr[Ljmptab+eax*4]
Lcase0:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase1:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase2:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase3:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase4:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase5:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ebx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase6:
		fmul ds:dword ptr[ebx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ecx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
		jmp LSetSides
Lcase7:
		fmul ds:dword ptr[ecx]
		fld ds:dword ptr[0+4+edx]
		fxch st(2)
		fmul ds:dword ptr[ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[4+ecx]
		fld ds:dword ptr[0+8+edx]
		fxch st(2)
		fmul ds:dword ptr[4+ebx]
		fxch st(2)
		fld st(0)
		fmul ds:dword ptr[8+ecx]
		fxch st(5)
		faddp st(3),st(0)
		fmul ds:dword ptr[8+ebx]
		fxch st(1)
		faddp st(3),st(0)
		fxch st(3)
		faddp st(2),st(0)
LSetSides:
		faddp st(2),st(0)
		fcomp ds:dword ptr[12+edx]
		xor ecx,ecx
		fnstsw ax
		fcomp ds:dword ptr[12+edx]
		and ah,1
		xor ah,1
		add cl,ah
		fnstsw ax
		and ah,1
		add ah,ah
		add cl,ah
		pop ebx
		mov eax,ecx
		ret
Lerror:
		int 3
	}
}
#pragma warning( default: 4035 )
#endif

void ClearBounds(vec3_t mins, vec3_t maxs)
{
	VectorSetAll(mins, 99999);
	VectorSetAll(maxs, -99999);
}

void AddPointToBounds (vec3_t v, vec3_t mins, vec3_t maxs)
{
	for (int i = 0; i < 3; i++)
	{
		mins[i] = min(v[i], mins[i]);
		maxs[i] = max(v[i], maxs[i]);
	}
}


int VectorCompare (vec3_t v1, vec3_t v2)
{
	if (v1[0] != v2[0] || v1[1] != v2[1] || v1[2] != v2[2])
		return 0;
			
	return 1;
}


vec_t VectorNormalize (vec3_t v)
{
	const float length = VectorLength(v);

	if (length)
	{
		const float ilength = 1 / length;
		VectorScale(v, ilength, v);
	}

	return length;
}

vec_t VectorNormalize2 (vec3_t v, vec3_t out)
{
	const float length = VectorLength(v);

	if (length)
	{
		const float ilength = 1 / length;
		VectorScale(v, ilength, out);
	}
		
	return length;
}

/*
=================
VectorNormalizeFast
From Q2E
=================
*/
void VectorNormalizeFast (vec3_t v)
{
	const float ilength = Q_rsqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] *= ilength;
	v[1] *= ilength;
	v[2] *= ilength;
}

void VectorMA (vec3_t veca, float scale, vec3_t vecb, vec3_t vecc)
{
	for(int c = 0; c < 3; c++)
		vecc[c] = veca[c] + scale * vecb[c];
}


vec_t _DotProduct (vec3_t v1, vec3_t v2)
{
	return v1[0] * v2[0] + v1[1] * v2[1] + v1[2] * v2[2];
}

void _VectorSubtract (vec3_t veca, vec3_t vecb, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = veca[c] - vecb[c];
}

void _VectorAdd (vec3_t veca, vec3_t vecb, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = veca[c] + vecb[c];
}

void _VectorCopy(vec3_t in, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = in[c];
}

void CrossProduct (vec3_t v1, vec3_t v2, vec3_t cross)
{
	cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
	cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
	cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

vec_t VectorLength(vec3_t v)
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

void VectorScale(vec3_t in, vec_t scale, vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = in[c] * scale;
}

/*
=================
VectorRotate
From Q2E
=================
*/
void VectorRotate(const vec3_t v, const vec3_t matrix[3], vec3_t out)
{
	for (int c = 0; c < 3; c++)
		out[c] = v[0] * matrix[c][0] + v[1] * matrix[c][1] + v[2] * matrix[c][2];
}


/*
=================
Matrix operations (mxd)
=================
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


/*
=================
Q_rsqrt
From Q2E
=================
*/
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

/*
=================
AnglesToAxis
From Q2E
=================
*/
void AnglesToAxis (const vec3_t angles, vec3_t axis[3])
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

/*
=================
AxisClear
From Q2E
=================
*/
void AxisClear (vec3_t axis[3])
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

/*
=================
AxisCopy
From Q2E
=================
*/
void AxisCopy (const vec3_t in[3], vec3_t out[3])
{
	for(int i = 0; i < 3; i++)
		for (int c = 0; c < 3; c++)
			out[i][c] = in[i][c];
}

/*
=================
AxisCompare
From Q2E
=================
*/
qboolean AxisCompare (const vec3_t axis1[3], const vec3_t axis2[3])
{
	if (axis1[0][0] != axis2[0][0] || axis1[0][1] != axis2[0][1] || axis1[0][2] != axis2[0][2])
		return false;

	if (axis1[1][0] != axis2[1][0] || axis1[1][1] != axis2[1][1] || axis1[1][2] != axis2[1][2])
		return false;

	if (axis1[2][0] != axis2[2][0] || axis1[2][1] != axis2[2][1] || axis1[2][2] != axis2[2][2])
		return false;

	return true;
}


//====================================================================================

/*
============
COM_SkipPath
============
*/
char *COM_SkipPath (char *pathname)
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

/*
============
COM_StripExtension
============
*/
void COM_StripExtension (char *in, char *out)
{
	while (*in && *in != '.')
		*out++ = *in++;

	*out = 0;
}

/*
============
COM_FileExtension
============
*/
char *COM_FileExtension (char *in)
{
	static char exten[8];
	int	i;

	while (*in && *in != '.')
		in++;

	if (!*in)
		return "";

	in++;
	for (i = 0; i < 7 && *in ; i++, in++)
		exten[i] = *in;

	exten[i] = 0;

	return exten;
}

/*
============
COM_FileBase
============
*/
void COM_FileBase (char *in, char *out)
{
	char *s2;
	char *s = in + strlen(in) - 1;
	
	while (s != in && *s != '.')
		s--;
	
	for (s2 = s; s2 != in && *s2 != '/'; s2--)
	;
	
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

/*
============
COM_FilePath

Returns the path up to, but not including the last /
============
*/
void COM_FilePath (char *in, char *out)
{
	char *s = in + strlen(in) - 1;
	
	while (s != in && *s != '/')
		s--;

	strncpy(out, in, s - in);
	out[s - in] = 0;
}


/*
==================
COM_DefaultExtension
==================
*/
void COM_DefaultExtension (char *path, char *extension)
{
	// if path doesn't have a .EXT, append extension (extension should include the .)
	char *src = path + strlen(path) - 1;

	while (*src != '/' && src != path)
	{
		if (*src == '.')
			return; // it has an extension

		src--;
	}

	strcat(path, extension);
}

/*
============================================================================
	BYTE ORDER FUNCTIONS
============================================================================
*/

qboolean bigendien;

// can't just use function pointers, or dll linkage can mess up when qcommon is included in multiple places
// Knightmare- made these static
static short	(*_BigShort) (short l);
static short	(*_LittleShort) (short l);
static int		(*_BigLong) (int l);
static int		(*_LittleLong) (int l);
static qint64	(*_BigLong64) (qint64 l);
static qint64	(*_LittleLong64) (qint64 l);
static float	(*_BigFloat) (float l);
static float	(*_LittleFloat) (float l);

short	BigShort(short l){return _BigShort(l);}
short	LittleShort(short l) {return _LittleShort(l);}
int		BigLong (int l) {return _BigLong(l);}
int		LittleLong (int l) {return _LittleLong(l);}
qint64	BigLong64 (qint64 l) {return _BigLong64(l);}
qint64	LittleLong64 (qint64 l) {return _LittleLong64(l);}
float	BigFloat (float l) {return _BigFloat(l);}
float	LittleFloat (float l) {return _LittleFloat(l);}

short ShortSwap (short l)
{
	const byte b1 = l & 255;
	const byte b2 = (l >> 8) & 255;

	return (b1 << 8) + b2;
}

short ShortNoSwap (short l)
{
	return l;
}

int LongSwap (int l)
{
	const byte b1 = l & 255;
	const byte b2 = (l >> 8) & 255;
	const byte b3 = (l >> 16) & 255;
	const byte b4 = (l >> 24) & 255;

	return ((int)b1 << 24) + ((int)b2 << 16) + ((int)b3 << 8) + b4;
}

int	LongNoSwap (int l)
{
	return l;
}

qint64 Long64Swap (qint64 ll)
{
	const byte b1 = ll & 255;
	const byte b2 = (ll >> 8) & 255;
	const byte b3 = (ll >> 16) & 255;
	const byte b4 = (ll >> 24) & 255;
	const byte b5 = (ll >> 32) & 255;
	const byte b6 = (ll >> 40) & 255;
	const byte b7 = (ll >> 48) & 255;
	const byte b8 = (ll >> 56) & 255;

	return ((qint64)b1 << 56) + ((qint64)b2 << 48) + ((qint64)b3 << 40) + ((qint64)b4 << 32)
		 + ((qint64)b5 << 24) + ((qint64)b6 << 16) + ((qint64)b7 << 8) + (qint64)b8;
}

qint64	Long64NoSwap (qint64 ll)
{
	return ll;
}

float FloatSwap (float f)
{
	union
	{
		float	f;
		byte	b[4];
	} dat1, dat2;
	
	
	dat1.f = f;
	dat2.b[0] = dat1.b[3];
	dat2.b[1] = dat1.b[2];
	dat2.b[2] = dat1.b[1];
	dat2.b[3] = dat1.b[0];

	return dat2.f;
}

float FloatNoSwap (float f)
{
	return f;
}

/*
================
Swap_Init
================
*/
void Swap_Init (void)
{
	byte swaptest[2] = { 1, 0 };

	// set the byte swapping variables in a portable manner	
	if (*(short *)swaptest == 1)
	{
		bigendien = false;
		_BigShort = ShortSwap;
		_LittleShort = ShortNoSwap;
		_BigLong = LongSwap;
		_LittleLong = LongNoSwap;
		_BigLong64 = Long64Swap;
		_LittleLong64 = Long64NoSwap;
		_BigFloat = FloatSwap;
		_LittleFloat = FloatNoSwap;
	}
	else
	{
		bigendien = true;
		_BigShort = ShortNoSwap;
		_LittleShort = ShortSwap;
		_BigLong = LongNoSwap;
		_LittleLong = LongSwap;
		_BigLong64 = Long64NoSwap;
		_LittleLong64 = Long64Swap;
		_BigFloat = FloatNoSwap;
		_LittleFloat = FloatSwap;
	}
}


/*
============
va

does a varargs printf into a temp buffer, so I don't need to have varargs versions of all text functions.
============
*/
char *va (char *format, ...)
{
	va_list		argptr;
	static char	string[1024];
	
	va_start(argptr, format);
	Q_vsnprintf(string, sizeof(string), format, argptr);
	va_end(argptr);

	return string;
}

/*
=======================================================================
	TEXT PARSING
=======================================================================
*/

char com_token[MAX_TOKEN_CHARS];

/*
=================
COM_SkipWhiteSpace
=================
*/
char *COM_SkipWhiteSpace (char *data_p, qboolean *hasNewLines)
{
	int	c;

	while ((c = *data_p) <= ' ')
	{
		if (!c)
			return NULL;

		if (c == '\n')
			*hasNewLines = true;

		data_p++;
	}

	return data_p;
}


/*
=================
COM_SkipBracedSection

Skips until a matching close brace is found.
Internal brace depths are properly skipped.
=================
*/
void COM_SkipBracedSection (char **data_p, int depth)
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


/*
=================
COM_SkipRestOfLine

Skips until a new line is found
=================
*/
void COM_SkipRestOfLine (char **data_p)
{
	while (true)
	{
		char *tok = COM_ParseExt(data_p, false);
		if (!tok[0])
			break;
	}
}


/*
==============
COM_Parse

Parse a token out of a string
==============
*/
char *COM_Parse (char **data_p)
{
	int c;

	char *data = *data_p;
	int len = 0;
	com_token[0] = 0;
	
	if (!data)
	{
		*data_p = NULL;
		return "";
	}
		
	// skip whitespace
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
	
	// skip // comments
	if (c == '/' && data[1] == '/')
	{
		while (*data && *data != '\n')
			data++;

		goto skipwhite;
	}

	// handle quoted strings specially
	if (c == '\"')
	{
		data++;
		while (true)
		{
			c = *data++;
			if (c == '\"' || !c)
			{
				if (len == MAX_TOKEN_CHARS)	// Knightmare- discard if > MAX_TOKEN_CHARS-1
					len = 0;
				
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

	// parse a regular word
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
		len = 0;

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
=================
Com_ParseExt

Parse a token out of a string
From Quake2Evolved
=================
*/
char *COM_ParseExt (char **data_p, qboolean allowNewLines)
{
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
		else if (c == '/' && data[1] == '*')	// Skip /* */ comments
		{
			data += 2;

			while (*data && (*data != '*' || data[1] != '/'))
				data++;

			if (*data)
				data += 2;
		}
		else	// An actual token
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
					len = 0;

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
		len = 0;

	com_token[len] = 0;

	*data_p = data;
	return com_token;
}


/*
===============
Com_PageInMemory
===============
*/
int	paged_total;

void Com_PageInMemory(byte *buffer, int size)
{
	for (int i = size - 1; i > 0; i -= 4096)
		paged_total += buffer[i];
}



/*
============================================================================
	LIBRARY REPLACEMENT FUNCTIONS
============================================================================
*/

// FIXME: replace all Q_stricmp with Q_strcasecmp
int Q_stricmp (char *s1, char *s2)
{
#if defined(WIN32)
	return _stricmp(s1, s2);
#else
	return strcasecmp(s1, s2);
#endif
}


//mxd. Case-insensitive version of strstr. https://stackoverflow.com/questions/27303062/strstr-function-like-that-ignores-upper-or-lower-case
char *Q_strcasestr(const char *haystack, const char *needle)
{
#if defined(WIN32)
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


int Q_strncasecmp(char *s1, char *s2, int n)
{
	int c1, c2;
	
	do
	{
		c1 = *s1++;
		c2 = *s2++;

		if (!n--)
			return 0;	// strings are equal until end point
		
		if (c1 != c2)
		{
			if (c1 >= 'a' && c1 <= 'z')
				c1 -= ('a' - 'A');

			if (c2 >= 'a' && c2 <= 'z')
				c2 -= ('a' - 'A');

			if (c1 != c2)
				return -1;	// strings not equal
		}
	} while (c1);
	
	return 0;		// strings are equal
}


int Q_strcasecmp(char *s1, char *s2)
{
	return Q_strncasecmp(s1, s2, 99999);
}


/*
=================
Q_strncpyz

Safe strncpy that ensures a trailing zero
=================
*/
void Q_strncpyz(char *dst, const char *src, int dstSize)
{
	if (!src || !dst || dstSize < 1) 
		return;
	
	strncpy(dst, src, dstSize - 1);
	dst[dstSize - 1] = 0;
}


/*
=================
Q_strncatz

Safe strncat that ensures a trailing zero
=================
*/
void Q_strncatz (char *dst, const char *src, int dstSize)
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


/*
=================
Q_snprintfz

Safe snprintf that ensures a trailing zero
=================
*/
void Q_snprintfz (char *dst, int dstSize, const char *fmt, ...)
{
	if (!dst || dstSize < 1) 
		return;

	va_list argPtr;
	va_start(argPtr, fmt);
	Q_vsnprintf(dst, dstSize, fmt, argPtr);
	va_end(argPtr);

	dst[dstSize - 1] = 0;
}


char *Q_strlwr (char *string)
{
	char *s = string;

	while (*s)
	{
		*s = tolower(*s);
		s++;
	}

	return string;
}


char *Q_strupr (char *string)
{
	char *s = string;

	while (*s)
	{
		*s = toupper(*s);
		s++;
	}

	return string;
}


void Com_sprintf(char *dest, int size, char *fmt, ...)
{
	char	bigbuffer[0x10000];
	va_list	argptr;

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


/*
=============
Com_HashFileName
=============
*/
long Com_HashFileName (const char *fname, int hashSize, qboolean sized)
{
	int		i = 0;
	long	hash = 0;

	if (fname[0] == '/' || fname[0] == '\\')
		i++; // skip leading slash

	while (fname[i] != '\0')
	{
		char letter = tolower(fname[i]);

		if (letter == '\\')
			letter = '/'; // fix filepaths

		hash += (long)letter * (i + 119);
		i++;
	}
	hash = (hash ^ (hash >> 10) ^ (hash >> 20));

	if (sized) 
		hash &= (hashSize - 1);

	return hash;
}

/*
=====================================================================
	INFO STRINGS
=====================================================================
*/

/*
===============
Info_ValueForKey

Searches the string for the given key and returns the associated value, or an empty string.
===============
*/
char *Info_ValueForKey (char *s, char *key)
{
	char	pkey[512];
	static	char value[2][512];	// use two buffers so compares work without stomping on each other
	static	int	valueindex;

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

		while (*s != '\\' && *s)
		{
			if (!*s)
				return "";
			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp (key, pkey) )
			return value[valueindex];

		if (!*s)
			return "";

		s++;
	}
}

void Info_RemoveKey (char *s, char *key)
{
	char pkey[512];
	char value[512];

	if (strstr(key, "\\"))
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
		while (*s != '\\' && *s)
		{
			if (!*s)
				return;

			*o++ = *s++;
		}
		*o = 0;

		if (!strcmp(key, pkey) )
		{
			strcpy(start, s);	// remove this part
			return;
		}

		if (!*s)
			return;
	}
}


/*
==================
Info_Validate

Some characters are illegal in info strings because they can mess up the server's parsing
==================
*/
qboolean Info_Validate (char *s)
{
	if (strstr (s, "\""))
		return false;

	if (strstr (s, ";"))
		return false;

	return true;
}

void Info_SetValueForKey (char *s, char *key, char *value)
{
	char newi[MAX_INFO_STRING];

	if (strstr(key, "\\") || strstr(value, "\\"))
	{
		Com_Printf("Can't use keys or values with a \\\n");
		return;
	}

	if (strstr(key, ";"))
	{
		Com_Printf("Can't use keys or values with a semicolon\n");
		return;
	}

	if (strstr(key, "\"") || (value && strstr(value, "\"")))
	{
		Com_Printf("Can't use keys or values with a \"\n");
		return;
	}

	if (strlen(key) > MAX_INFO_KEY - 1 || (value && strlen(value) > MAX_INFO_KEY - 1))
	{
		Com_Printf("Keys and values must be < 64 characters.\n");
		return;
	}

	Info_RemoveKey(s, key);
	if (!value || !strlen(value))
		return;

	Com_sprintf(newi, sizeof(newi), "\\%s\\%s", key, value);

	if (strlen(newi) + strlen(s) > MAX_INFO_STRING)
	{
		Com_Printf("Info string length exceeded\n");
		return;
	}

	// only copy ascii values
	s += strlen(s);
	char *v = newi;
	while (*v)
	{
		int c = *v++;
		c &= 127; // strip high bits

		if (c >= 32 && c < 127)
			*s++ = c;
	}

	*s = 0;
}