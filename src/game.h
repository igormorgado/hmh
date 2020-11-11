#ifndef __GAME_H__
#define __GAME_H__

#include "game_platform.h"


struct game_state
{
    i16 blue_offset;
    i16 green_offset;
    i16 red_offset;
    f32 tone_hz;
    i16 amplitude;
    f32 t_sine;

    i16 player_x;
    i16 player_y;
    f32 t_jump;
};

#if !defined(CASEYDEFS)
internal void game_update_and_render_f (struct game_memory              *memory,
                                        struct game_input               *input,
                                        struct game_offscreen_buffer    *screen_buffer);

internal void game_get_sound_samples_f (struct game_memory              *memory,
                                        struct game_sound_output_buffer *sound_buffer);
#endif

#endif /* __GAME_H__ */
