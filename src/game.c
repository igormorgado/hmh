#include <sys/cdefs.h>

internal i32
GameOutputSound(struct game_sound_output_buffer *sound_buffer, f32 tonehz)
{
    local_persist f32 t_sine;
    i16 amplitude = 3000;
    f32 wave_period = sound_buffer->samples_per_second / tonehz;

    i16 *sample_out = sound_buffer->samples;
    for(size_t sample_index = 0; sample_index < sound_buffer->sample_count; ++sample_index)
    {
        f32 sine_value = sinf(t_sine);
        i16 sample_value = (i16)(roundf(sine_value * amplitude));
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;
        t_sine += TAU / wave_period;
    }
    return RETURN_SUCCESS;
}

internal i32
RenderWeirdGradient (struct game_offscreen_buffer *screenbuffer,
                     i16 blueoffset, i16 greenoffset, i16 redoffset)
{

    // TODO: ASSERT WINDOW NOT NULL
    // TODO: ASSERT 0 < BLUEOFFSET GREEN OFFSET < 255
    u8 *row = (u8 *)screenbuffer->memory;
    for(int y = 0; y < screenbuffer->height; ++y)
    {
        u32 *pixel = (u32*)row;
        for(int x = 0; x < screenbuffer->width; ++x)
        {
            u8 blue = (x + blueoffset);
            u8 green = (y + greenoffset);
            u8 red = redoffset;
            *pixel++ = ((red << 16)  |
                        (green << 8) |
                        (blue << 0));
        }
        row += screenbuffer->pitch;
    }
    return RETURN_SUCCESS;
}

internal void
GameUpdateAndRender(struct game_input *input,
                    struct game_offscreen_buffer *screen_buffer,
                    struct game_sound_output_buffer *sound_buffer)
{
    local_persist i16 redoffset = 0;
    local_persist i16 blueoffset = 0;
    local_persist i16 greenoffset = 0;
    local_persist f32 tonehz = 256.0f;


    struct game_controller_input *input0 = &(input->controllers[0]);
    if(input0->is_analog)
    {
        blueoffset += (i16)(4.0f * input0->end_x);
        tonehz = 256.0f + 128.0f * input0->end_y;
    }
    else
    {
        /* TODO: Do digital... */
    }

    if(input0->down.ended_down)
    {
        greenoffset += 1;
    }

    GameOutputSound(sound_buffer, tonehz);
    RenderWeirdGradient(screen_buffer, blueoffset, greenoffset, redoffset);
}
