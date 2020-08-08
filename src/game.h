#ifndef __GAME_H__
#define __GAME_H__

#include <stddef.h>

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

internal void GameUpdateAndRender(struct game_offscreen_buffer *screenbuffer,
                                  i16 blueoffset,
                                  i16 greenoffset,
                                  struct game_sound_output_buffer *soundbuffer,
                                  f32 tonehz);

#endif /* __GAME_H__ */
