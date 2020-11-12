#ifndef __GAME_PLATFORM_H__
#define __GAME_PLATFORM_H__



/*
 * GAME_INTERNAL
 *  0 - For public release
 *  1 - For development
 *
 * GAME_SLOW
 *  0 - No slow code
 *  1 - Slow code
 */

/*
 * TODO(Igor):
 *   - Replace bool with sdl_bool?
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMPILER_MSVC
# define COMPILER_MSVC 0
#endif

#ifndef COMPILER_LLVM
# define COMPILER_LLVM 0
#endif

#ifndef COMPILER_GNUC
# define COMPILER_GNUC 0
#endif

#if !COMPILER_MSVC && \
    !COMPILER_LLVM && \
    !COMPILER_GNUC

# if _MSC_VER
#  undef COMPILER_MSVC
#  define COMPILER_MSVC 1
# endif

# if __GNUC__
#  undef COMPILER_GNUC
#  define COMPILER_GNUC 1
# endif

# if __llvm__
#  undef COMPILER_LLVM
#  define COMPILER_LLVM 1
# endif

#endif

#if COMPILER_MSVC
# include <intrin.h>
#elif COMPILER_GNUC || COMPILER_LLVM
# include <x86intrin.h>
#else
# error SSE/NEON optimizations not available.
#endif


/*
 * TYPES
 */
#include <stdint.h>
#include <stddef.h>
#include <limits.h>
#include <float.h>

#ifndef __cplusplus
# include <stdbool.h>
#endif

typedef int8_t          int8;
typedef int8_t          int08;
typedef int16_t         int16;
typedef int32_t         int32;
typedef int64_t         int64;
typedef int32_t         b32;

typedef uint8_t         uint8;
typedef uint8_t         uint08;
typedef uint16_t        uint16;
typedef uint32_t        uint32;
typedef uint64_t        uint64;

typedef float           real32;
typedef double          real64;

typedef intptr_t        intptr;
typedef uintptr_t       uintptr;

typedef unsigned int    uint;

typedef size_t          memory_index;

typedef int8              s8;
typedef int8              s08;
typedef int16             s16;
typedef int32             s32;
typedef int64             s64;
typedef int8              i8;
typedef int8              i08;
typedef int16             i16;
typedef int32             i32;
typedef int64             i64;

typedef uint8             u8;
typedef uint8             u08;
typedef uint16            u16;
typedef uint32            u32;
typedef uint64            u64;

typedef real32            r32;
typedef real64            r64;

typedef real32            f32;
typedef real64            f64;


#if !defined(internal)
# define internal static
#endif
#define local_persist static
#define global_variable static

/*
 * Mathematical constants and types
 */

/* sinf */
#include <math.h>

#define PI          3.14159265358979323846f  /* pi */
#define PI_2        1.57079632679489661923f  /* pi/2 */
#define PI_4        0.78539816339744830962f  /* pi/4 */
#define TAU         6.28318530717958623199f  /* Tau = 2*pi */

/*
 * TODO: Use SDL assertions is adviced
 */
#if GAME_SLOW
# define Assert(Expression) if(!(Expression)) {*(volatile int *)0 = 0;}
#else
# define Assert(Expression)
#endif

enum RETURN_STATUS
{
    RETURN_FAILURE = -1,
    RETURN_SUCCESS = 0,
    RETURN_EXIT = 255
};


/*
 * Helpful custom inline macros
 */

#define MININT(a, b)    ((a) < (b) ? (a) : (b))
#define MAXINT(a, b)    ((a) > (b) ? (a) : (b))

#define Kilobytes(value) ((value)*1024LL)
#define Megabytes(value) (Kilobytes(value)*1024LL)
#define Gigabytes(value) (Megabytes(value)*1024LL)
#define Terabytes(value) (Gigabytes(value)*1024LL)

#define ArrayCount(array) (sizeof((array)) / sizeof((array)[0]))

#define AlignPow2(Value, Alignment) (((Value) + ((Alignment) - 1)) & ~((Alignment) - 1))
#define Align4(value)  (((value) +  3) &  ~3)
#define Align8(value)  (((value) +  7) &  ~7)
#define Align16(value) (((value) + 15) & ~15)

inline u32
safe_truncate_u64(u64 value)
{
    /* TODO: Assert (value <= 0xFFFFFFFF) */
    Assert(value <= 0xFFFFFFFF)
    u32 result = (u32)value;
    return result;
}

/*
 * Game debugging
 */
#if GAME_INTERNAL
struct debug_read_file_result
{
    size_t contents_size;
    void   *contents;
};


enum
{
    DEBUG_CC_GAME_UPDATE_AND_RENDER,
    DEBUG_CC_RENDER_GROUP_TO_OUTPUT,
    DEBUG_CC_DRAW_RECTANGLE_SLOWLY,
    DEBUG_CC_PROCESS_PIXEL,
    DEBUG_CC_DRAW_RECTANGLE_QUICKLY,
    DEBUG_CC_COUNT,
};

struct debug_cycle_counter
{
    size_t cycle_count;
    size_t hit_count;
};


struct debug_read_file_result debug_platform_read_entire_file  (const char *filename);
void                          debug_platform_free_file_memory  (void       *memory);
bool                          debug_platform_write_entire_file (const char *filename,
                                                                size_t     memory_size,
                                                                void       *memory);

# if defined(COMPILER_MSVC)

extern   struct game_memory *debug_global_memory;

#  define BEGIN_TIMED_BLOCK(ID) { \
                                    u64 start_cycle_count##ID = __rdtsc();   \
                                }

#  define END_TIMED_BLOCK(ID)   { \
                                    debug_global_memory->counters[debug_cycle_counter_##ID].cycle_count += __rdtsc() - start_cycle_count##ID; \
                                    ++debug_global_memory->counters[debug_cycle_counter_##ID].hit_count; \
                                }

#  define END_TIMED_BLOCK_COUNTED(ID, count) { \
                                                debug_global_memory->counters[debug_cycle_counter_##ID].cycle_count += __rdtsc() - start_cycle_count##ID; \
                                                debug_global_memory->counters[debug_cycle_counter_##ID].hit_count += (count); \
                                             }
# else
#  define BEGIN_TIMED_BLOCK(ID)
#  define END_TIMED_BLOCK(ID)
#  define END_TIMED_BLOCK_COUNTED(ID, count)
# endif

#endif

/*
 * NOTE: Services that game provides to plataform layer.
 * (this may expande in the future - sound on separate thread, etc...)
 */

/*
 * FOUR THINGS:
 * - timing,
 * - controller/keyboard input
 * - bitmap buffer to use
 * -sound buffer to use
 *
 *  NOTE(Igor): These things are all implemented direclty in SDL!
 */

/*
 * TODO: Fetch this information from SDL
 */
#define BITMAP_BYTES_PER_PIXEL 4

struct game_offscreen_buffer
{
    void *memory;
    int width;
    int height;
    int pitch;
    int bytes_per_pixel;
};

struct game_sound_output_buffer
{
    int samples_per_second;
    int sample_count;
    i16 *samples;
};

struct game_button_state
{
    int half_transition_count;
    bool ended_down;
};

struct game_controller_input
{
    bool is_connected;
    bool is_analog;
    f32 stick_average_x;
    f32 stick_average_y;

    union
    {
        struct game_button_state buttons[12];
        struct
        {
            struct game_button_state move_up;
            struct game_button_state move_down;
            struct game_button_state move_left;
            struct game_button_state move_right;

            struct game_button_state action_up;
            struct game_button_state action_down;
            struct game_button_state action_left;
            struct game_button_state action_right;

            struct game_button_state left_shoulder;
            struct game_button_state right_shoulder;

            struct game_button_state back;
            struct game_button_state start;

            /*
             * NOTE: All buttons must be up, terminator is not a button.
             * DO NOTE REMOVE UNLESS YOU KNOW WHAT YOURE DOING
             */
            struct game_button_state terminator;
        };
    };
};

#define MAX_CONTROLLERS 4
#define MAX_INPUT_DEVICES MAX_CONTROLLERS + 1
#define CONTROLLER_AXIS_LEFT_DEADZONE 7849
struct game_input
{
    struct game_button_state mouse_buttons[MAX_INPUT_DEVICES];
    i32 mouse_x;
    i32 mouse_y;
    i32 mouse_z;

    /*
     * NOTE(Igor): Why this is here???!?!? makes no sense
     */
    /* This should be in game_state */
    b32 executable_reloaded;
    /* This is most related to performance counters or game timings...  */
    f32 dt_for_frame;

    struct game_controller_input controllers[MAX_INPUT_DEVICES];
};

#if 0    /* NOT YET ON HMH: only at day 151 */
struct platform_file_handle
{
    b32 no_errors;
    void *plataform;
};

struct platform_file_group
{
    u32 file_count;
    void *platform;
};

struct platform_work_queue;

enum platform_file_type
{
    PLATFORM_FILETYPE_ASSET_FILE,
    PLATFORM_FILETYPE_SAVED_GAME_FILE,
    PLATFORM_FILETYPE_COUNT
};

#define PLATFORM_NO_FILE_ERRORS(handle)     ((handle)->no_errors)

typedef struct platform_file_group  platform_get_all_files_of_type_begin (enum   platform_file_type  type);
typedef void                        platform_get_all_files_of_type_end   (struct platform_file_group *file_group);
typedef struct platform_file_handle platform_open_next_file              (struct platform_file_group *file_group);
typedef void                        platform_read_data_from_file         (struct platform_file_handle *source,
                                                                          u64 offset,
                                                                          u64 size,
                                                                          void *dest);
typedef void                        platform_file_error                  (struct platform_file_handle *handle,
                                                                          char *message);
typedef void                        platform_work_queue_callback         (struct platform_work_queue *queue,
                                                                          void *data);
typedef void                        *platform_allocate_memory            (size_t size);
typedef void                        platform_deallocate_memory           (void *memory);
typedef void                        platform_add_entry                   (struct platform_work_queue *queue,
                                                                          platform_work_queue_callback *callback,
                                                                          void *data);
typedef void                        platform_complete_all_work           (struct platform_work_queue *queue);

typedef struct platform_api
{
    platform_add_entry                      *add_entry;
    platform_complete_all_work              *complete_all_work;
    platform_get_all_files_of_type_begin    *get_all_files_of_type_begin;
    platform_get_all_files_of_type_end      *get_all_files_of_type_end;
    platform_open_next_file                 *open_next_file;
    platform_read_data_from_file            *read_data_from_file;
    platform_file_error                     *file_error;
    platform_allocate_memory                *allocate_memory;
    platform_deallocate_memory              *deallocate_memory;
#if GAME_INTERNAL
    debug_platform_free_file_memory         *debug_free_file_memory;
    debug_platform_read_entire_file         *debug_read_entire_file;
    debug_platform_write_entire_file        *debug_write_entire_file;
#endif
};
#endif

struct game_memory
{
    size_t      permanent_storage_size;
    void        *permanent_storage;  // Must be zeroed at startup

    size_t      transient_storage_size;
    void        *transient_storage;  // Must be zeroed at stattup

    b32 is_initialized;

#if 0 /* Only after day 130 */
    struct platform_work_queue *high_priority_queue;
    struct platform_work_queue *low_priority_queue;
#endif

#if 0 /* Only after day 151 */
    struct platform_api platform_api;
#endif

#if GAME_INTERNAL
    struct debug_cycle_counter counters[DEBUG_CC_COUNT];
#endif

};

/* HERE */
typedef void game_update_and_render (struct game_memory              *memory,
                                     struct game_input               *input,
                                     struct game_offscreen_buffer    *screen_buffer);

/* This function must be very fast. no more than 1ms */
typedef void game_get_sound_samples (struct game_memory              *memory,
                                     struct game_sound_output_buffer *sound_buffer);

inline struct game_controller_input *
get_controller(struct game_input *input, uint controller_index)
{
#if GAME_SLOW
    /* TODO(Igor): SDL Assert */
    Assert(controller_index < ArrayCount(input->controllers));
#endif
    struct game_controller_input *result = &input->controllers[controller_index];
    return result;
}

#ifdef __cplusplus
}
#endif

#endif /* __GAME_PLATFORM_H__ */
