// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_SOUND_H
#define FADO_SOUND_H

#include "fado_types.h"
#include "fado_math.h"

// ──────────────────────────────────────────────────────────────────────────────────────────
// --Sound--

#define FMAX_SOUNDS 64
#define SOUND_SAMPLE_RATE 48000
#define SOUND_CHANNELS 2

enum ESoundCategory
{
    Music,
    SFX,
    UI,
    SOUND_CATEGORY_COUNT  // Always last. Not an actual sound.
};

// A loaded sound asset (PCM data decoded from WAV).
struct FSoundBuffer
{
    i16* samples;       // the actual wave data, just an array of numbers
    u32 sampleCount;    // how many samples (per channel)
    u32 channels;       // 1=mono, 2=stereo
    u32 sampleRate;     // 44100, 48000, etc
};

// A playing instance of a sound.
struct FSoundInstance
{
    FSoundBuffer buffer;
    ESoundCategory category;
    u32 cursor;                 // current playback position (sample index)
    f32 volume;                 // 0.0 - 1.0
    b8 playing;
    b8 loop;
};

// What the sound manager produces each frame for the platform layer.
struct FSoundOutput
{
    i16* samples;       // All active sounds added together into one buffer
    u32 sampleCount;    // how many samples to fill this frame, ~1000-4000 samples (a few ms of audio)
};

// The sound manager, lives in the permanent arena and contains all sound instances.
struct FSoundManager
{
    FSoundInstance instances[FMAX_SOUNDS];
    f32 categoryVolume[SOUND_CATEGORY_COUNT];
    f32 masterVolume;
    i16* mixBuffer;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

// Inits the sound and prepares it, but doesn't play it.
inline void SoundInit(FSoundManager* manager, HSound hSound, ESoundCategory category, f32 volume, b8 loop)
{
    FSoundInstance* instance = &manager->instances[hSound];
    instance->category = category;
    instance->cursor = 0;
    instance->volume = volume;
    instance->loop = loop;
    instance->playing = false;
}

// Starts playing a sound by its handle. Assuming the buffer has been already filled.
inline void SoundPlay(FSoundManager* manager, HSound hSound, ESoundCategory category, f32 volume, b8 loop)
{
    FSoundInstance* instance = &manager->instances[hSound];
    instance->category = category;
    instance->cursor = 0;
    instance->volume = volume;
    instance->loop = loop;
    instance->playing = true;
}

/*
* Mixes all currently playing sounds into a single output buffer for one audio frame.

* interleaved stereo
- The output buffer looks like this:
  Index:   0   1   2   3   4   5   6   7
  Data :   L0  R0  L1  R1  L2  R2  L3  R3
*/
inline void SoundMix(FSoundManager* manager, FSoundOutput* output)
{
    // Zero the output buffer first.
    FadoZeroMemory(output->samples, (output->sampleCount * SOUND_CHANNELS * sizeof(i16)));

    for (i32 i = 0; i < FMAX_SOUNDS; ++i)
    {
        FSoundInstance* instance = &manager->instances[i];
        if (!instance->playing)
        {
            continue;
        }

        // Final playback volume.
        f32 volume = (instance->volume * manager->categoryVolume[instance->category] * manager->masterVolume);

        FSoundBuffer* buffer = &instance->buffer;

        // Mix one output sample at a time.
        for (u32 sample = 0; sample < output->sampleCount; ++sample)
        {
            if (instance->cursor >= buffer->sampleCount)
            {
                if (instance->loop)
                {
                    instance->cursor = 0;
                }
                else
                {
                    instance->playing = false;
                    break;
                }
            }

            // Stereo interleaved output index.
            u32 outIdx = sample * SOUND_CHANNELS;
            u32 srcIdx = instance->cursor * buffer->channels;

            // Add this sound's sample to whatever is already in the output.
            i32 left = output->samples[outIdx] + (i32)(buffer->samples[srcIdx] * volume);
            i32 right = output->samples[outIdx + 1] + (i32)(buffer->samples[srcIdx + 1] * volume);

            // Clamp to i16 range to avoid overflow.
            output->samples[outIdx]     = Clampi16(left, I16_MIN_VALUE, I16_MAX_VALUE);
            output->samples[outIdx + 1] = Clampi16(right, I16_MIN_VALUE, I16_MAX_VALUE);

            // Advance to the next sample in this sound.
            instance->cursor++;
        }
    }
}

// ────────────────
// Helpers

inline void SoundPause(FSoundManager* manager, HSound handle)
{
    if (handle >= FMAX_SOUNDS)
    {
        return;
    }

    manager->instances[handle].playing = false;
}

inline void SoundResume(FSoundManager* manager, HSound handle)
{
    if (handle >= FMAX_SOUNDS)
    {
        return;
    }

    manager->instances[handle].playing = true;
}

inline void SoundReplay(FSoundManager* manager, HSound handle)
{
    if (handle >= FMAX_SOUNDS)
    {
        return;
    }

    manager->instances[handle].playing = true;
    manager->instances[handle].cursor = 0;
}

inline void SoundReset(FSoundManager* manager, HSound handle)
{
    if (handle >= FMAX_SOUNDS)
    {
        return;
    }

    manager->instances[handle].cursor = 0;
}

// Stops all sounds of a category.
inline void SoundStopCategory(FSoundManager* manager, ESoundCategory category)
{
    for (u32 i = 0; i < FMAX_SOUNDS; ++i)
    {
        if (manager->instances[i].category == category)
        {
            SoundPause(manager, i);
        }
    }
}

inline void SoundStopAll(FSoundManager* manager)
{
    for (u32 i = 0; i < FMAX_SOUNDS; ++i)
    {
        SoundPause(manager, i);
    }
}

// Returns the first valid instance slot, -1 on failure.
inline i32 GetFirstFreeInstanceSlot(FSoundManager* manager)
{
    for (u32 i = 0; i < FMAX_SOUNDS; ++i)
    {   
        // Return the index of the empty instance slot.
        if (!manager->instances[i].buffer.samples)
        {
            return i;
        }
    }
    return -1;
}

#endif	// FADO_SOUND_H