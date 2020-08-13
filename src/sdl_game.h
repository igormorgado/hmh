#ifndef __SDL_GAME_H__
#define __SDL_GAME_H__

#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

/* TODO: USE SDL2 stdc implementations */
#include <SDL2/SDL.h>

/* TODO: SDL Assert configuration before include */
#include <SDL2/SDL_assert.h>

/*
 * Plataform dependant code
 */

struct sdl_offscreen_buffer
{
    SDL_Texture *Texture;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    u32 BytesPerPixel;
};

/* TODO: This isn't really SDL coupled */
struct sdl_window_dimension
{
    int Width;
    int Height;
};

struct sdl_audio_ring_buffer
{
    u64 Size;
    u64 WriteCursor;
    u64 PlayCursor;
    void *Data;
};

struct sdl_sound_output
{
    SDL_AudioDeviceID AudioDevice;
    int SamplesPerSecond;
    u32 RunningSampleIndex;
    int BytesPerSample;
    size_t SecondaryBufferSize; /* Why not inside sdl_audio_ring_buffer ? */
    size_t SafetyBytes;
    int LatencySampleCount;
    f32 tSine;
};

#if DEBUG
struct sdl_debug_time_marker
{
    size_t OutputPlayCursor;
    size_t OutputWriteCursor;
    size_t OutputLocation;
    size_t OutputByteCount;
    size_t ExpectedFlipPlayCursor;
    size_t FlipPlayCursor;
    size_t FlipWriteCursor;
};
#endif

struct sdl_audio_counters
{
    // u64 ExpectedSoundBytesPerFrame;
    // f64 SecondsLeftUntilFlip;
    // u64 ExpectedBytesUntilFlip;
    // u64 SafeWriteCursor;
    // bool AudioCardIsLowLatency;
    u64 FlipWallClock;
    u64 AudioWallClock;
    f64 FromBeginToAudioSeconds;
    u64 ExpectedFrameBoundaryByte;
    u64 TargetCursor;
};

struct sdl_performance_counters
{
    int GameUpdateHz;
    f64 TargetSecondsPerFrame;
    u64 PerfCountFrequency;
    u64 LastCycleCount;
    u64 EndCycleCount;
    u64 CounterElapsed;
    u64 CyclesElapsed;
    f64 MSPerFrame;
    f64 FPS;
    f64 MCPF;
    u64 LastCounter;
    u64 EndCounter;
};

SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];

#endif /* __SDL_GAME_H__ */
