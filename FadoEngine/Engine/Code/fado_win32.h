// (C) Copyright 2026 by Abdallah Maaliki / folayfila.

#ifndef WIN32_FADO_H
#define WIN32_FADO_H

#include "fado_d3d.h"
#include "fado.h"

// ────────────────────────────────────────────────────────────────────────

/*
** Windows Platform Layer **
* Starting point of the code, the game loop and where everything is setup and initialized
* for windows (currently the engine is only supported on windows).
*/

// ─────────────────────────────────────────────
#define WIN32_LEAN_AND_MEAN
#define VSYNC_ENABLED true
#define SCREEN_DEPTH 1000.0f
#define SCREEN_NEAR 0.3f
// ─────────────────────────────────────────────

// ─────────────────────────────────────────────
// XAudio2
#include<xaudio2.h>
#include "fado_sound.h"

// -- 2D Audio --

// Double buffer so XAudio2 always has something while we're filling the next.
#define WIN32_SOUND_BUFFER_COUNT 2
#define WIN32_SOUND_SAMPLES_PER_FRAME 4800  // 100ms at 48000hz, tweak for latency

// Holds XAudio2 state required to initialize and stream audio on windows.
struct Win32SoundState
{
	IXAudio2* xaudio2;                     // Main XAudio2 engine.
	IXAudio2MasteringVoice* masterVoice;   // Final output voice connected to the audio device.
	IXAudio2SourceVoice* sourceVoice;      // Voice that we continuously feed PCM samples to.

    // Ring of audio buffers submitted to XAudio2.
    // Double buffering prevents the CPU from writing into a buffer that's still being played.
	i16 buffers[WIN32_SOUND_BUFFER_COUNT][WIN32_SOUND_SAMPLES_PER_FRAME * SOUND_CHANNELS];
	u32 currentBuffer;

	b32 initialized;
};

// XAudio2 requires a callback object for voice events.
struct Win32VoiceCallback : IXAudio2VoiceCallback
{
    // Signaled whenever XAudio2 finishes playing a submitted buffer.
    HANDLE bufferEndEvent;

    Win32VoiceCallback()
    {
        // Auto-reset event used to synchronize buffer submission.
        bufferEndEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    }

    ~Win32VoiceCallback()
    {
        CloseHandle(bufferEndEvent);
    }

    // Called when a submitted buffer has finished playing.
    // Wake up the audio thread so it can submit another buffer.
    void STDMETHODCALLTYPE OnBufferEnd(void*) { SetEvent(bufferEndEvent); }

    // Required interface stubs.
    void STDMETHODCALLTYPE OnStreamEnd() {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() {}
    void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) {}
    void STDMETHODCALLTYPE OnBufferStart(void*) {}
    void STDMETHODCALLTYPE OnLoopEnd(void*) {}
    void STDMETHODCALLTYPE OnVoiceError(void*, HRESULT) {}
};

global_variable Win32VoiceCallback GlobalVoiceCallback;

// ────────────────────
// -- 3D Audio --
// Fixed pool of source voices used for spatial audio.

#define WIN32_MAX_3D_VOICES 32

struct Win32VoiceSlot3D
{
    IXAudio2SourceVoice* voice;
    b32 inUse;
};

// Shared pool reused by all 3D sound playback.
global_variable Win32VoiceSlot3D Global_3DVoiceSlots[WIN32_MAX_3D_VOICES];

// ─────────────────────────────────────────────

// Holds the game code dll and the main update function.
// Loaded on startup and reloaded if the dll is rebuilt during runtime.
struct Win32GameCode
{
	HMODULE gameCodeDLL;
	FILETIME dllLastWriteTime;

	FGameUpdate* gameUpdate;
	b32 isValid;
};

// Main application.
// Holds a pointer to the renderer world and the game code.
struct Win32System
{
	HINSTANCE instance;
	HWND window;
	FRenderWorld* world;
};

// ─────────────────────────────────────────────
// Globals
// ─────────────────────────────────────────────
global_variable Win32System* GlobalWin32System;
global_variable WINDOWPLACEMENT GlobalWindowPosition = { sizeof(GlobalWindowPosition) };
global_variable b32 GlobalRunning = true;

// ────────────────────────────────────────────────────────────────────────

#endif // WIN32_FADO_H