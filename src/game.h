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

#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value)*1024LL)
#define Gigabytes(value) (Megabytes(value)*1024LL)
#define Terabytes(value) (Gigabytes(value)*1024LL)

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
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    size_t SampleCount;
    i16 *Samples;
};

struct game_button_state
{
    int HalfTransientCount;
    bool EndedDown;
};

struct game_controller_input
{
    bool IsAnalog;

    f32 StartX;
    f32 StartY;

    f32 MinX;
    f32 MinY;

    f32 MaxX;
    f32 MaxY;

    f32 EndX;
    f32 EndY;

    union
    {
        struct game_button_state Buttons[6];
        struct
        {
            struct game_button_state Up;
            struct game_button_state Down;
            struct game_button_state Left;
            struct game_button_state Right;
            struct game_button_state Lshoulder;
            struct game_button_state Rshoulder;
        };
    };
};

#define MAX_CONTROLLERS 4
struct game_input
{
    struct game_controller_input Controllers[MAX_CONTROLLERS];
};


struct game_state
{
    i16 BlueOffset;
    i16 GreenOffset;
    i16 RedOffset;
    f32 ToneHz;
};

struct game_memory
{
    bool IsInitialized;

    size_t PersistentStorageSize;
    void *PersistentStorage;

    size_t TransientStorageSize;
    void *TransientStorage;
};


#if DEBUG
struct debug_read_file_result
{
    u32 ContentsSize;
    void *Contents;
};

internal struct debug_read_file_result DEBUGPlataformReadEntireFile(char *filename);
internal void DEBUGPlataformFreeFileMemory(void *Memory);
internal bool DEBUGPlataformWriteEntireFile(char *Filename, u32 MemorySize, void *Memory);
#endif


internal void GameUpdateAndRender(struct game_memory *Memory,
                                  struct game_input *Input,
                                  struct game_offscreen_buffer *ScreenBuffer,
                                  struct game_sound_output_buffer *SoundBuffer);

#endif /* __GAME_H__ */
