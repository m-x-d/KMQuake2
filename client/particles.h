// Psychospaz's particle system

#pragma once

// Also defined in GL.h
#define GL_ZERO							0x0
#define GL_ONE							0x1
#define GL_SRC_COLOR					0x0300
#define GL_ONE_MINUS_SRC_COLOR			0x0301
#define GL_SRC_ALPHA					0x0302
#define GL_ONE_MINUS_SRC_ALPHA			0x0303
#define GL_DST_ALPHA					0x0304
#define GL_ONE_MINUS_DST_ALPHA			0x0305
#define GL_DST_COLOR					0x0306
#define GL_ONE_MINUS_DST_COLOR			0x0307
#define GL_SRC_ALPHA_SATURATE			0x0308

// Also defined in glext.h
#define GL_CONSTANT_COLOR				0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR		0x8002
#define GL_CONSTANT_ALPHA				0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA		0x8004

#define PARTICLE_GRAVITY		40
#define BLASTER_PARTICLE_COLOR	0xe0
#define INSTANT_PARTICLE		-10000.0

#define MAX_PARTICLE_LIGHTS		8

typedef enum
{
	particle_solid = 1,
	particle_generic,
	particle_inferno,
	particle_smoke,
	particle_blood,
	particle_blooddrop,
	particle_blooddrip,
	particle_redblood,
	particle_bubble,
	particle_blaster,
	particle_blasterblob,
	particle_beam,
	particle_beam2,
	particle_lightning,
//	particle_lensflare,
//	particle_lightflare,
//	particle_shield,
	particle_rflash,
	particle_rexplosion1,
	particle_rexplosion2,
	particle_rexplosion3,
	particle_rexplosion4,
	particle_rexplosion5,
	particle_rexplosion6,
	particle_rexplosion7,
//	particle_dexplosion1,
//	particle_dexplosion2,
//	particle_dexplosion3,
	particle_bfgmark,
	particle_burnmark,
	particle_blooddecal1,
	particle_blooddecal2,
	particle_blooddecal3,
	particle_blooddecal4,
	particle_blooddecal5,
	particle_shadow,
	particle_bulletmark,
	particle_trackermark,
//	particle_footprint,
	particle_classic, //mxd
} particle_type;

typedef struct
{
	qboolean isactive;
	vec3_t lightcol;
	float light;
	float lightvel;
} cplight_t;

typedef struct particle_s
{
	struct particle_s *next;

	cplight_t lights[MAX_PARTICLE_LIGHTS];

	float time;

	vec3_t org;
	vec3_t angle;
	vec3_t vel;
	vec3_t accel;

	vec3_t color;
	vec3_t colorvel;

	int blendfunc_src;
	int blendfunc_dst;

	float alpha;
	float alphavel;

	float size;
	float sizevel;

	particle_type type;
	int flags;

	vec3_t oldorg;
	int src_ent;
	int dst_ent;

	int decalnum;
	decalpolys_t *decal;

	void (*think)(struct particle_s *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time);
} cparticle_t;

extern cparticle_t *active_particles;
extern cparticle_t *free_particles;

void CL_ClipDecal(cparticle_t *part, const float radius, const float orient, const vec3_t origin, const vec3_t dir);
void CL_AddParticleLight(cparticle_t *p, const float light, const float lightvel, const float lcol0, const float lcol1, const float lcol2);
void CL_CalcPartVelocity(const cparticle_t *p, const float scale, const float time, vec3_t velocity);
void CL_ParticleBounceThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time);
void CL_ParticleRotateThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time);
void CL_DecalAlphaThink(cparticle_t *p, vec3_t org, vec3_t angle, float *alpha, float *size, particle_type *type, float *time);

/* Defaults:
    color = 255, 255, 255;
    image = particle_generic;
    blendfunc_src = GL_SRC_ALPHA;
    blendfunc_dst = GL_ONE;
    alpha = 1.0f;
    size = 1.0f;
    time = cl.time;
 The rest is 0 / NULL. */
cparticle_t *CL_InitParticle(); //mxd

//mxd. Must be called after CL_InitParticle() to finish particle setup...
void CL_FinishParticleInit(cparticle_t *p);