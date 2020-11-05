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
#include <assert.h>

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
    size_t size;
    size_t write_cursor;
    size_t play_cursor;
    void *data;
};

struct sdl_sound_output
{
    SDL_AudioDeviceID audio_device;
    int samples_per_second;
    u32 running_sample_index;
    int bytes_per_sample;
    size_t secondary_buffer_size; /* Why not inside sdl_audio_ring_buffer ? */
    int safety_bytes;
    int latency_sample_count;
    f32 t_sine;
};

#if DEBUG
struct sdl_debug_time_marker
{
    size_t output_play_cursor;
    size_t output_write_cursor;
    size_t output_location;
    size_t output_byte_count;
    size_t expected_flip_play_cursor;
    size_t flip_play_cursor;
    size_t flip_write_cursor;
};
#endif

struct sdl_audio_counters
{
    // u64 ExpectedSoundBytesPerFrame;
    // f64 SecondsLeftUntilFlip;
    // u64 ExpectedBytesUntilFlip;
    // u64 SafeWriteCursor;
    // bool AudioCardIsLowLatency;
    u64 flip_wall_clock;
    u64 audio_wall_clock;
    f64 from_begin_to_audio_seconds;
    u64 expected_frame_boundary_byte;
    u64 target_cursor;
};

struct sdl_performance_counters
{
    int game_update_hz;
    f64 target_seconds_per_frame;
    u64 performance_count_frequency;
    u64 last_cycle_count;
    u64 end_cycle_count;
    u64 counter_elapsed;
    u64 cycles_elapsed;
    f64 ms_per_frame;
    f64 fps;                /* Frames per second */
    f64 mcpf;               /* ... ... per frame */
    u64 last_counter;
    u64 end_counter;
};

SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];

#endif /* __SDL_GAME_H__ */
