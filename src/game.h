#ifndef __GAME_H__
#define __GAME_H__

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <x86intrin.h>

#define PI          3.14159265358979323846f  /* pi */
#define PI_2        1.57079632679489661923f  /* pi/2 */
#define PI_4        0.78539816339744830962f  /* pi/4 */
#define TAU         6.28318530717958623199f  /* Tau = 2*pi */

#define MININT(a, b)    ((a) < (b) ? (a) : (b))
#define MAXINT(a, b)    ((a) > (b) ? (a) : (b))

#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif

#define ArrayCount(array) (sizeof((array)) / sizeof((array)[0]))

typedef int8_t          i8;
typedef int16_t         i16;
typedef int32_t         i32;
typedef int64_t         i64;

typedef uint8_t         u8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef uint64_t        u64;

typedef unsigned int    uint;
typedef float           f32;
typedef double          f64;

#define internal static
#define local_persist static
#define global_variable static


enum RETURN_STATUS
{
    RETURN_FAILURE = -1,
    RETURN_SUCCESS = 0,
    RETURN_EXIT = 255
};


struct game_offscreen_buffer
{
    void *memory;
    int width;
    int height;
    int pitch;
};

struct game_sound_output_buffer
{
    int samples_per_second;
    size_t sample_count;
    i16 *samples;
};

struct game_button_state
{
    int half_transient_count;
    bool ended_down;
};

struct game_controller_input
{
    bool is_analog;

    f32 start_x;
    f32 start_y;

    f32 min_x;
    f32 min_y;

    f32 max_x;
    f32 max_y;

    f32 end_x;
    f32 end_y;

    union
    {
        struct game_button_state buttons[6];
        struct
        {
            struct game_button_state up;
            struct game_button_state down;
            struct game_button_state left;
            struct game_button_state right;
            struct game_button_state lshoulder;
            struct game_button_state rshoulder;
        };
    };
};

#define MAX_CONTROLLERS 4
struct game_input
{
    struct game_controller_input controllers[MAX_CONTROLLERS];
};

internal void GameUpdateAndRender(struct game_input *input,
                                  struct game_offscreen_buffer *screen_buffer,
                                  struct game_sound_output_buffer *soundbuffer);

#endif /* __GAME_H__ */
