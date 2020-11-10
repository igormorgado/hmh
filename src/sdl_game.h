#ifndef __SDL_GAME_H__
#define __SDL_GAME_H__


/* TODO: USE SDL2 stdc implementations */
#include <SDL2/SDL.h>

/* TODO: SDL Assert configuration before include */
#include <SDL2/SDL_assert.h>

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <glob.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>

#if 0
#include <math.h>
#endif

/*
 * Plataform dependant code
 */

#define SDL_STATE_FILE_NAME_MAX_SIZE 4096

struct sdl_offscreen_buffer
{
    // NOTE(casey): Pixels are alwasy 32-bits wide, Memory Order BB GG RR XX
    SDL_Texture *texture;
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

/* TODO: This isn't really SDL coupled */
struct sdl_window_dimension
{
    int width;
    int height;
};

struct sdl_sound_output
{
    SDL_AudioDeviceID   audio_device;
    int                 samples_per_second;
    int                 bytes_per_sample;
    size_t              running_sample_index;
    size_t              secondary_buffer_size;
    size_t              safety_bytes;

    // TODO(casey): Should running sample index be in bytes as well
    // TODO(casey): Math gets simpler if we add a "bytes per second" field?
};

#if GAME_INTERNAL
struct sdl_debug_time_marker
{
    size_t queued_audio_butees;
    size_t output_byte_count;
    size_t expected_bytes_until_flip;
};
#endif

struct sdl_game_code
{
    void *game_code_dll;
    time_t dll_last_write_time;
    game_update_and_render *update_and_render;
    game_get_sound_samples *get_sound_samples;

    bool is_valid;
};

struct sdl_replay_buffer
{
    int file_handle;
    void *memory_map;
    char filename[SDL_STATE_FILE_NAME_MAX_SIZE];
    void *memory_block;
};

#define REPLAY_BUFFERS_MAX 4
struct sdl_state
{
    size_t total_size;
    void *game_memory_block;
    struct sdl_replay_buffer replay_buffers[REPLAY_BUFFERS_MAX];

    int recording_handle;
    size_t input_recording_index;

    int playback_handle;
    size_t input_playback_index;

    char *exe_filename[SDL_STATE_FILE_NAME_MAX_SIZE];
    char *exe_basename;

};

struct sdl_performance_counters
{
    u64 frequency;
    int game_update_hz;
    f32 target_seconds_per_frame;
    u64 last_counter;
    u64 end_counter;
    u64 flip_wall_clock;
    u64 last_cycle_count;
    u64 end_cycle_count;
    f32 seconds_elapsed_for_frame;
    u64 cycles_elapsed;
    f64 fps;                /* Frames per second */
    f64 mcpf;               /* cycles per frame */
};

struct sdl_audio_counters
{
    u32 expected_sound_bytes_per_frame;
    u32 queued_audio_bytes;
#if GAME_INTERNAL
    u64 wall_clock;
    f32 from_begin_to_audio_seconds;
    f32 seconds_left_until_flip;
    u32 expected_bytes_until_flip;
    f32 audio_latency_seconds;
#endif
};

#endif /* __SDL_GAME_H__ */
