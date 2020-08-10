#ifndef __SDL_GAME_H__
#define __SDL_GAME_H__


/* TODO: USE SDL2 stdc implementations */
#include <SDL2/SDL.h>

/* TODO: SDL Assert configuration before include */
#include <SDL2/SDL_assert.h>

/*
 * Plataform dependant code
 */

struct sdl_offscreen_buffer
{
    SDL_Texture *texture;
    void *memory;
    i32 width;
    i32 height;
    i32 pitch;
};

/* TODO: This isn't really SDL coupled */
struct sdl_window_dimension
{
    i32 width;
    i32 height;
};

struct sdl_audio_ring_buffer
{
    i32 size;
    i32 write_cursor;
    i32 play_cursor;
    void * data;
};

struct sdl_sound_output
{
    SDL_AudioDeviceID device;
    i32 samples_per_second;
    f32 toneHz;
    f32 amplitude;  /* ToneVolume */
    size_t running_sample_index;
    f32 wave_period;
    i32 bytes_per_sample;
    i32 secondary_buffer_size; /* todo: size_t? */
    f32 t_sine;
    i32 latency_sample_count;
};

SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];

#endif /* __SDL_GAME_H__ */
