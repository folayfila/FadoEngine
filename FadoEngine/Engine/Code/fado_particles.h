// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_PARTICLES_H
#define FADO_PARTICLES_H

#include "fado_types.h"
#include "fado_math.h"

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Particles --
/*
* CPU-simulated, GPU-instanced. One shared quad mesh + one lightweight shader, all particles in an emitter update on CPU each frame, 
* uploaded as an instance buffer, drawn with a single draw call per emitter.
*/

// ──────────────────────────────────────────────────────────────────────────────────────────

// -- Range types -- 
struct FRangeF32 { f32 min; f32 max; };
struct FRangeV3 { v3  min; v3  max; };
struct FRangeV4 { v4  min; v4  max; };

// Roll a concrete value. If min == max, this is effectively constant — no branching needed.
internal f32 RollRange(FRangeF32 r) { return (r.min == r.max) ? r.min : RandomF32InRange(r.min, r.max); }
internal v3  RollRange(FRangeV3  r) { return (r.min == r.max) ? r.min : RandomV3InRange(r.min, r.max); }
internal v4  RollRange(FRangeV4  r) { return (r.min == r.max) ? r.min : RandomV4InRange(r.min, r.max); }

// Convenience: build a non-random ("constant") range from a single value.
internal FRangeF32 ConstRange(f32 v) { return { v, v }; }
internal FRangeV3  ConstRange(v3  v) { return { v, v }; }
internal FRangeV4  ConstRange(v4  v) { return { v, v }; }

// -- Curves --
// A property that can optionally change over a particle's lifetime.
// If enabled == false, 'start' is used for the particle's entire life.

struct FParticleCurveF32
{ 
	FRangeF32 start;
	FRangeF32 end;
	b32 enabled;
};

struct FParticleCurveV3
{
	FRangeV3  start;
	FRangeV3  end;
	b32 enabled;
};

struct FParticleCurveV4
{
	FRangeV4  start;
	FRangeV4  end;
	b32 enabled;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

// -- Runtime Particles --
// CPU-only, never touches GPU directly
struct FParticle
{
	v3  position;       // integrated velocity-only position (spawn origin + motion)
	v3  posOffsetEnd;   // this particle's resolved random position-curve end
	v3  velocity;
	f32 age;
	f32 lifetime;

	f32 sizeStart, sizeEnd, size;
	v4  colorStart, colorEnd, color;
	f32 speedStart, speedEnd;

	b32 alive;
};
// -- Particle Instance --
// GPU-facing instance data. Small, only what the shader needs.
struct FParticleInstance
{
	v3 position;
	f32 size;
	v4 color;
};

#define FMAX_PARTICLE_INSTANCES 4096 // shared cap across all emitters drawn in one frame

// -- Particle Emitter --
// config + owned particle pool

#define FMAX_PARTICLES_PER_EMITTER 128

// A property that can optionally change over a particle's lifetime.
// If enabled == false, 'start' is used for the particle's entire life.
struct FParticleEmitter
{
	// --- Spawn config ---
	f32 lifetime;						// base lifetime per particle (seconds)

	f32 spawnRate;						// particles per second (0 = burst-only, spawn 'count' once)
	FParticleCurveF32 count;			// concurrent particles count

	FParticleCurveV3 position;			 // emitter origin and optional offset applied over particle life

	FParticleCurveF32 size;				// size

	FParticleCurveV4 color;				// color

	FParticleCurveF32 speed;			// speed

	v3 direction;						// base emit direction (normalized)
	HTexture texture;

	// --- Runtime state ---
	FParticle particles[FMAX_PARTICLES_PER_EMITTER];
	u32 aliveCount;
	f32 spawnAccumulator;
	f32 emitterAge;
	b32 active;
	b32 hasBurst;						// first frame only — one-shot burst
};

// -- Particle Emitter Pool --
#define FMAX_PARTICLE_EMITTERS 64


// ──────────────────────────────────────────────────────────────────────────────────────────

// Evaluate at t = [0,1]. Falls back to 'start' if disabled.
// Used for fields like 'count' that aren't rolled per-particle — reads
internal f32 EvaluateCurveF32(FParticleCurveF32 curve, f32 t)
{
	if (!curve.enabled)
	{
		return curve.start.min; // constant case — min == max
	}

	return Lerp(curve.start.min, curve.end.min, t);
}

// Spawns a particle by rolling values from the emitter's ranges.
// If a range has min == max, RollRange() returns that value, making it constant
// instead of random. Disabled properties reuse their start value.
internal void SpawnParticle(FParticleEmitter* e)
{
	if (e->aliveCount >= FMAX_PARTICLES_PER_EMITTER)
	{
		return; // pool full, drop this spawn
	}

	FParticle* p = &e->particles[e->aliveCount++];

	p->position = RollRange(e->position.start);
	p->posOffsetEnd = e->position.enabled ? RollRange(e->position.end) : V3Zero();

	p->age = 0.0f;
	p->lifetime = e->lifetime;

	p->sizeStart = RollRange(e->size.start);
	p->sizeEnd = e->size.enabled ? RollRange(e->size.end) : p->sizeStart;
	p->size = p->sizeStart;

	p->colorStart = RollRange(e->color.start);
	p->colorEnd = e->color.enabled ? RollRange(e->color.end) : p->colorStart;
	p->color = p->colorStart;

	p->speedStart = RollRange(e->speed.start);
	p->speedEnd = e->speed.enabled ? RollRange(e->speed.end) : p->speedStart;

	v3 dir = e->direction;
	p->velocity = { dir.x * p->speedStart, dir.y * p->speedStart, dir.z * p->speedStart };

	p->alive = true;
}

internal u32 BuildParticleInstances(FParticleEmitter* emitter, FParticleInstance* outInstances)
{
	u32 count = 0;
	for (u32 i = 0; i < emitter->aliveCount && count < FMAX_PARTICLE_INSTANCES; ++i)
	{
		FParticle* p = &emitter->particles[i];

		f32 t = (p->lifetime > 0.0f) ? (p->age / p->lifetime) : 1.0f;
		v3 posOfs = emitter->position.enabled ? LerpV3(V3Zero(), p->posOffsetEnd, t) : V3Zero();

		FParticleInstance* inst = &outInstances[count++];
		inst->position = p->position + posOfs;
		inst->size = p->size;
		inst->color = p->color;
	}
	return count;
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Public API --

// Returns particle handle into a pool, and zero-initializes the particle.
inline HParticle CreateParticleEmitter(FParticleEmitter* emitters, FParticleEmitter** outParticle)
{
	// Get the first available particle emitter slot
	for (u32 i = 0; i < FMAX_PARTICLE_EMITTERS; ++i)
	{
		if (!emitters[i].active)
		{
			*outParticle = &emitters[i];
			**outParticle = {};
			return i;
		}
	}

	// No available slots
	return INVALID_HANDLE;
}

inline void UpdateParticleEmitter(FParticleEmitter* emitter, f32 dt)
{
	if (!emitter->active)
	{
		return;
	}

	emitter->emitterAge += dt;

	// ---- Spawning ----
	if (emitter->spawnRate > 0.0f)
	{
		// Continuous emission. count optionally ramps the effective rate
		// using emitterAge as a 0..1 window over the emitter's own 'lifetime' field
		// (reused as a ramp duration when count.enabled).
		f32 rate = emitter->spawnRate;
		if (emitter->count.enabled)
		{
			f32 t = (emitter->lifetime > 0.0f) ? Saturate(emitter->emitterAge / emitter->lifetime) : 1.0f;
			rate = EvaluateCurveF32(emitter->count, t);
		}

		emitter->spawnAccumulator += dt * rate;
		while (emitter->spawnAccumulator >= 1.0f)
		{
			SpawnParticle(emitter);
			emitter->spawnAccumulator -= 1.0f;
		}
	}
	else if (!emitter->hasBurst) // first frame only — one-shot burst
	{
		for (u32 i = 0; i < emitter->count.start.min; ++i)
		{
			SpawnParticle(emitter);
		}
		emitter->hasBurst = true;
	}

	// ---- Update + kill (compact in place, swap-remove) ----
	for (u32 i = 0; i < emitter->aliveCount; )
	{
		FParticle* p = &emitter->particles[i];
		p->age += dt;

		if (p->age >= p->lifetime)
		{
			// Swap-remove: overwrite with last alive particle, shrink count, don't advance i.
			*p = emitter->particles[emitter->aliveCount - 1];
			emitter->aliveCount--;
			continue;
		}

		f32 t = (p->lifetime > 0.0f) ? (p->age / p->lifetime) : 1.0f;

		// Lerp each property from THIS particle's own rolled start/end, not the emitter's range.
		if (emitter->size.enabled)  p->size = Lerp(p->sizeStart, p->sizeEnd, t);
		if (emitter->color.enabled) p->color = LerpV4(p->colorStart, p->colorEnd, t);

		f32 speed = emitter->speed.enabled ? Lerp(p->speedStart, p->speedEnd, t) : p->speedStart;
		f32 curLen = V3Length(p->velocity);
		if (emitter->speed.enabled && (curLen > 0.0001f))
		{
			v3 dir = p->velocity / curLen;
			p->velocity = dir * speed;
		}

		// Integrate velocity only — position curve offset is applied at build-instance time.
		p->position += p->velocity * dt;

		++i;
	}

	// Kill the one bursts
	if (emitter->aliveCount <= 0 && emitter->hasBurst)
	{
		emitter->active = false;
	}
}

inline void UpdateAllEmitters(FParticleEmitter* emitters, f32 dt)
{
	for (u32 i = 0; i < FMAX_PARTICLE_EMITTERS; ++i)
	{
		UpdateParticleEmitter(&emitters[i], dt);
	}
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// -- Presets --
ForceInline HParticle MakeFireParticle(FParticleEmitter* pool, HTexture blobTexture)
{
	FParticleEmitter* fire = nullptr;
	HParticle handle = CreateParticleEmitter(pool, &fire);
	if (handle == -1)
	{
		return -1;
	}

	fire->texture = blobTexture;
	fire->lifetime = 1.0f;
	fire->active = true;
	fire->spawnRate = 50.0f;
	fire->count = { ConstRange(400.0f), ConstRange(500.0f) };
	fire->count.enabled = true;

	fire->position.start = ConstRange(V3Zero());  // shared spawn origin
	fire->position.end = { {-1.0f, 1.0f, -1.0f}, {1.0f, 2.0f, 1.0f} };  // each particle rolls its own drift target
	fire->position.enabled = true;

	fire->direction = V3Normalize({ 1.0f, 1.0f, 0.0f });

	fire->speed.start = { 1.0f, 10.0f };     // each particle rolls its own initial speed
	fire->speed.enabled = false;            // no ramp — constant per-particle speed

	fire->color.start = ConstRange(FColor::Red());
	fire->color.end = ConstRange(FColor::Orange());
	fire->color.enabled = true;

	fire->size.start = { 0.2f, 0.5f};  // each particle rolls its own starting size
	fire->size.enabled = false;

	return handle;
}

// ────────────────────────────────────────────────────────────────────────

#endif // FADO_PARTICLES_H