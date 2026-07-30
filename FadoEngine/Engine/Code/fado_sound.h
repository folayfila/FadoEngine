// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef FADO_SOUND_H
#define FADO_SOUND_H

#include "fado_types.h"
#include "fado_math.h"

// ──────────────────────────────────────────────────────────────────────────────────────────
// --Sound--
/*
* 2D audio instances gets mixed each frame by the sound manager and played in one buffer.
* 3D audio instances hold a voice slot index that gets used and updated in a 3D voice pool in the platform layer and played by its own.
*/

// Number of allowed loaded sound assets.
#define FMAX_SOUND_ASSETS 100

// How many instances of one sound can be played simultaneously.
#define FMAX_SOUND_ASSET_INSTANCES_AT_ONCE 5

// How many total sound instances can be played/mixed simultaneously.
#define FMAX_SOUND_INSTANCES 64

#define SOUND_SAMPLE_RATE 48000
#define SOUND_CHANNELS 2

enum ESoundCategory
{
    Sound_Music,
    Sound_SFX,
    Sound_UI,
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
// The instance holds a handle to the sound asset buffer in the sound assets bank "bufferIndex".
// This allows us to play one sound asset multiple times simultaneously.
// 2D audio instances gets mixed each frame by the sound manager and played in one buffer.
// 3D audio instances hold a voice slot index that gets used and updated in a 3D voice pool in the platform layer and played by its own.
struct FSoundInstance
{
    HSound bufferIndex;         // index into SoundAssetBank.assets
    ESoundCategory category;
    u32 cursor;                 // current playback position (sample index) ONLY 2D
    f32 volume;                 // 0.0 - 1.0
    b8 playing;
    b8 loop;
    b8 active;

    // 3d audio only:
    b8 is3D;
    HEntity attachedTo;         // The entity this sound is attached to. INVALID_HANDLE for none. Used for postion update in 3D sounds.
    v3 position;
    f32 minDistance;
    f32 maxDistance;
    i32 voiceSlot;              // index into platform voice pool, INVALID_HANDLE if unassigned.
};

// What the sound manager produces each frame for the platform layer (2D).
struct FSoundOutput
{
    i16* samples;       // All active sounds added together into one buffer
    u32 sampleCount;    // how many samples to fill this frame, ~1000-4000 samples (a few ms of audio)
};

// Used for 3D audio. 
struct FSoundListener
{
    v3 position;
    v3 forward;
    v3 up;
};

// Holds all sound assets (pcm) and instances.
struct FSoundAssetBank
{
    FSoundBuffer assets[FMAX_SOUND_ASSETS];             // loaded assets
    u32 assetInstanceCount[FMAX_SOUND_ASSETS];          // how many instances currently use assets[i]
    FSoundInstance instances[FMAX_SOUND_INSTANCES];     // the actual sound instances being played this frame. 
    u32 assetsCount;
};

// The sound manager, lives in the permanent arena and is used to manage and access all sound assets and instances.
struct FSoundManager
{
    FSoundAssetBank* assetBank;
    f32 categoryVolume[SOUND_CATEGORY_COUNT];
    f32 masterVolume;
    FSoundListener listener;
    i16* mixBuffer;         // the 2D mix buffer that get used to output the 2D audio mix.

    // Set to false when we want to stop all 2D and 3D soudns.
    // Used so that the platform layer can stop 3D audios.
    b32 active;
};

// ──────────────────────────────────────────────────────────────────────────────────────────

// Returns the first valid instance slot, INVALID_HANDLE on failure.
inline i32 GetFirstFreeInstanceSlot(FSoundManager* manager)
{
    for (i32 i = 0; i < FMAX_SOUND_INSTANCES; ++i)
    {
        // Return the index of the empty instance slot.
        if (!manager->assetBank->instances[i].active)
        {
            return i;
        }
    }
    return INVALID_HANDLE;
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// 3D calculations

inline f32 SoundCalculateAttenuation(f32 distance, f32 minDist, f32 maxDist)
{
    if (distance <= minDist)
    {
        return 1.0f;
    }
    if (distance >= maxDist)
    {
        return 0.0f;
    }

    f32 result = 1.0f - ((distance - minDist) / (maxDist - minDist));
    return result;
}

// Project the direction to the source onto the listener's right vector.
inline f32 SoundCalculatePan(FSoundListener* listener, v3 sourcePos)
{
    v3 toSource = sourcePos - listener->position;
    toSource = V3Normalize(toSource);

    v3 right = V3Cross(listener->up, listener->forward);
    right = V3Normalize(right);

    f32 pan = V3Dot(toSource, right); // -1 = left, 0 = center, 1 = right
    return pan;
}

// Pan + Volume -> L / R Matrix(Equal - Power Pan)
inline void SoundCalculateStereoMatrix(f32 pan, f32 volume, f32* outMatrix /* [2] */)
{
    f32 angle = (pan + 1.0f) * 0.25f * Pi32; // maps -1..1 to 0..PI/2

    f32 leftGain = cosf(angle) * volume;
    f32 rightGain = sinf(angle) * volume;

    outMatrix[0] = leftGain;
    outMatrix[1] = rightGain;
}

// ──────────────────────────────────────────────────────────────────────────────────────────
// Public API

// Starts playing a 2D sound by its handle. Assuming the buffer has been already filled.
inline HSound SoundPlay2D(FSoundManager* manager, HSound bufferIndex, ESoundCategory category, f32 volume, b8 loop)
{
    FSoundAssetBank* bank = manager->assetBank;
    if (bank->assetInstanceCount[bufferIndex] >= FMAX_SOUND_ASSET_INSTANCES_AT_ONCE)
    {
        return INVALID_HANDLE; // This sound asset has maximum simultaneous playing instances.
    }

    i32 instanceSlot = GetFirstFreeInstanceSlot(manager);
    if (instanceSlot == INVALID_HANDLE)
    {
        return INVALID_HANDLE;
    }

    FSoundInstance* instance = &bank->instances[instanceSlot];
    instance->attachedTo = INVALID_HANDLE;
    instance->bufferIndex = bufferIndex;
    instance->category = category;
    instance->cursor = 0;
    instance->volume = volume;
    instance->loop = loop;
    instance->playing = true;
    instance->active = true;
    // we don't care about the rest, they are 0 by default.

    bank->assetInstanceCount[bufferIndex]++;

    return instanceSlot;
}

// Starts playing a 3D sound by its handle. Assuming the buffer has been already filled.
inline i32 SoundPlay3D(FSoundManager* manager, HEntity attachTo, HSound bufferIndex, ESoundCategory category, f32 volume, b8 loop,
    v3 position, f32 minDist, f32 maxDist)
{
    FSoundAssetBank* bank = manager->assetBank;
    if (bank->assetInstanceCount[bufferIndex] >= FMAX_SOUND_ASSET_INSTANCES_AT_ONCE)
    {
        return INVALID_HANDLE; // This sound asset has maximum simultaneous playing instances.
    }

    i32 instanceSlot = GetFirstFreeInstanceSlot(manager);
    if (instanceSlot == INVALID_HANDLE)
    {
        return INVALID_HANDLE;
    }

    FSoundInstance* instance = &bank->instances[instanceSlot];
    instance->attachedTo = attachTo;
    instance->bufferIndex = bufferIndex;
    instance->category = category;
    instance->cursor = 0;
    instance->volume = volume;
    instance->loop = loop;
    instance->playing = true;
    instance->active = true;
    instance->is3D = true;
    instance->position = position;
    instance->minDistance = minDist;
    instance->maxDistance = maxDist;
    instance->voiceSlot = INVALID_HANDLE;   // assigned lazily on first update

    bank->assetInstanceCount[bufferIndex]++;

    return instanceSlot;
}

inline void SoundStop(FSoundManager* manager, HSound handle)
{
    if (handle >= FMAX_SOUND_INSTANCES)
    {
        return;
    }

    FSoundInstance* instance = &manager->assetBank->instances[handle];
    instance->active = false;
    instance->playing = false;
    instance->voiceSlot = INVALID_HANDLE;
    manager->assetBank->assetInstanceCount[instance->bufferIndex]--;
}

inline void SoundPause(FSoundManager* manager, HSound handle)
{
    if (handle >= FMAX_SOUND_INSTANCES)
    {
        return;
    }

    manager->assetBank->instances[handle].playing = false;
}

inline void SoundResume(FSoundManager* manager, HSound handle)
{
    if (handle >= FMAX_SOUND_INSTANCES)
    {
        return;
    }

    manager->assetBank->instances[handle].playing = true;
}

// Stops all sounds of a category.
inline void SoundStopCategory(FSoundManager* manager, ESoundCategory category)
{
    for (u32 i = 0; i < FMAX_SOUND_INSTANCES; ++i)
    {
        if (manager->assetBank->instances[i].category == category)
        {
            SoundStop(manager, i);
        }
    }
}

inline void SoundStopAll(FSoundManager* manager)
{
    FadoZeroArray(manager->assetBank->instances);
    FadoZeroArray(manager->assetBank->assetInstanceCount);
    manager->listener = {};
    manager->active = false;
}

/*
* Mixes one instance into the passed output buffer for one audio frame.
* interleaved stereo
- The output buffer looks like this:
  Index:   0   1   2   3   4   5   6   7
  Data :   L0  R0  L1  R1  L2  R2  L3  R3
*/
inline void SoundMixInstance(FSoundManager* manager, FSoundInstance* instance, i32 instanceHandle, FSoundOutput* output)
{
    // Final playback volume.
    f32 volume = (instance->volume * manager->categoryVolume[instance->category] * manager->masterVolume);

    FSoundBuffer* buffer = &manager->assetBank->assets[instance->bufferIndex];

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
                SoundStop(manager, instanceHandle);
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
        output->samples[outIdx] = Clamp(left, I16_MIN_VALUE, I16_MAX_VALUE);
        output->samples[outIdx + 1] = Clamp(right, I16_MIN_VALUE, I16_MAX_VALUE);

        // Advance to the next sample in this sound.
        instance->cursor++;
    }
}

inline void Update3DSoundsPositions(FSoundAssetBank* assetBank, FSharedStuff* shared)
{
    for (u32 i = 0; i < FMAX_SOUND_INSTANCES; ++i)
    {
        FSoundInstance* inst = &assetBank->instances[i];
        if (!inst->active || !inst->playing || (inst->attachedTo == INVALID_HANDLE))
        {
            continue;
        }
        inst->position = shared->transforms.positions[inst->attachedTo];
    }
}

// ────────────────────────────────────────────────────────────────────────

#endif	// FADO_SOUND_H