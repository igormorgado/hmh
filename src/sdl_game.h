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
};

/* TODO: This isn't really SDL coupled */
struct sdl_window_dimension
{
    int Width;
    int Height;
};

struct sdl_audio_ring_buffer
{
    int Size;
    int WriteCursor;
    int PlayCursor;
    void *Data;
};

struct sdl_sound_output
{
    SDL_AudioDeviceID AudioDevice;
    int SamplesPerSecond;
    u32 RunningSampleIndex;
    int BytesPerSample;
    int SecondaryBufferSize; /* todo: size_t? */
    int LatencySampleCount;
};

SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];

#endif /* __SDL_GAME_H__ */
