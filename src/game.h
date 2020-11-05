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

#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value)*1024LL)
#define Gigabytes(value) (Megabytes(value)*1024LL)
#define Terabytes(value) (Gigabytes(value)*1024LL)

#define ArrayCount(array) (sizeof((array)) / sizeof((array)[0]))

#define Align4(Value) ((Value + 3) & ~3)
#define Align8(Value) ((Value + 7) & ~7)
#define Align16(Value) ((Value + 15) & ~15)



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
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    int samples_per_second;
    size_t sample_count;
    i16 *samples;
};

struct game_button_state
{
    int HalfTransitionCount;
    bool EndedDown;
};

struct game_controller_input
{
    bool IsConnected;
    bool IsAnalog;

    f32 StickAverageX;
    f32 StickAverageY;

    union
    {
        struct game_button_state Buttons[12];
        struct
        {
            struct game_button_state MoveUp;
            struct game_button_state MoveDown;
            struct game_button_state MoveLeft;
            struct game_button_state MoveRight;

            struct game_button_state ActionUp;
            struct game_button_state ActionDown;
            struct game_button_state ActionLeft;
            struct game_button_state ActionRight;

            struct game_button_state Lshoulder;
            struct game_button_state Rshoulder;

            struct game_button_state Back;
            struct game_button_state Start;

            struct game_button_state Terminator;
        };
    };
};

#define MAX_CONTROLLERS 4
#define MAX_INPUT MAX_CONTROLLERS + 1
struct game_input
{
    struct game_controller_input Controllers[MAX_INPUT];
};

static inline struct game_controller_input *
GetController(struct game_input *Input, uint ControllerIndex)
{
    /* TODO: Assert(ControllerInde < ArrayCount(Input->Controllers)) */
    struct game_controller_input *Result = &Input->Controllers[ControllerIndex];
    return Result;
}

struct game_state
{
    i16 BlueOffset;
    i16 GreenOffset;
    i16 RedOffset;
    f32 ToneHz;
};

struct game_memory
{
    bool is_initialized;

    size_t permanent_storage_size;
    void *permanent_storage;

    size_t transient_storage_size;
    void *transient_storage;
};


#if DEBUG
struct debug_read_file_result
{
    size_t contents_size;
    void *contents;
};

internal struct debug_read_file_result DEBUGPlataformReadEntireFile(const char *filename);
internal void DEBUGPlataformFreeFileMemory(void *Memory);
internal bool DEBUGPlataformWriteEntireFile(const char *Filename, u32 MemorySize, void *Memory);
#endif


internal void GameUpdateAndRender(struct game_memory *Memory,
                                  struct game_input *Input,
                                  struct game_offscreen_buffer *ScreenBuffer);

internal void GameGetSoundSamples(struct game_memory *Memory,
                                  struct game_sound_output_buffer *SoundBuffer);

#endif /* __GAME_H__ */
