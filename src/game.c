#include "game.h"

internal i32
game_output_sound(struct game_state *game_state, struct game_sound_output_buffer *sound_buffer)
{
    int wave_period = sound_buffer->samples_per_second / game_state->tone_hz;
    i16 *sample_out = sound_buffer->samples;

    for(size_t i = 0; i < sound_buffer->sample_count; ++i)
    {

#if 1
        f32 sine_value = sinf(game_state->t_sine);
        i16 sample_value = (i16)(sine_value * game_state->amplitude);
#else
        i16 sample_value = 0;
#endif
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;

        game_state->t_sine += 2.0f * PI * 1.0f / (f32)wave_period;
        if(game_state->t_sine > 2.0f * PI)
        {
            game_state->t_sine -= 2.0f * PI;
        }
    }
    return RETURN_SUCCESS;
}

internal i32
render_weird_gradient (struct game_offscreen_buffer *screen_buffer,
                       i16 blue_offset, i16 green_offset, i16 red_offset)
{

    u8 *row = (u8 *)screen_buffer->memory;
    for(int y = 0; y < screen_buffer->height; ++y)
    {
        u32 *pixel = (u32*)row;
        for(int x = 0; x < screen_buffer->width; ++x)
        {
            u8 blue = (x + blue_offset);
            u8 green = (y + green_offset);
            u8 red = red_offset;
            *pixel++ = ((red << 16)  |
                        (green << 8) |
                        (blue << 0));
        }
        row += screen_buffer->pitch;
    }
    return RETURN_SUCCESS;
}


internal void
render_player(struct game_offscreen_buffer *screen_buffer, int player_x, int player_y)
{
    u8 *end_of_buffer = (u8*)screen_buffer->memory + screen_buffer->pitch * screen_buffer->height;

    u32 color = 0xffffff;

    int top = player_y;
    int bottom = player_y+10;
    for(int x = player_x; x < player_x+10; ++x)
    {
        u8 *pixel = (u8*)screen_buffer->memory +
                    x * screen_buffer->bytes_per_pixel +
                    top * screen_buffer->pitch;
        for(int y = top; y < bottom; ++y)
        {
            if ((pixel >= (u8*)screen_buffer->memory) && ((pixel+4) <= end_of_buffer))
            {
                *(u32*)pixel = color;
            }
            pixel += screen_buffer->pitch;
        }
    }
}


/* Renderer entry point */
void
game_update_and_render_f(struct game_memory *memory,
                         struct game_input *input,
                         struct game_offscreen_buffer *screen_buffer)
{
    /* TODO: SDL_Asserts????  or some wrapper over it? */
    Assert(sizeof(struct game_state) <= memory->permanent_storage_size)
    Assert((&input->controllers[0].terminator - &input->controllers[0].buttons[0]) ==
         (ArrayCount(input->controllers[0].buttons)));

    struct game_state *game_state = (struct game_state *)memory->permanent_storage;

    if(!memory->is_initialized)
    {

#if 0
#if GAME_INTERNAL
        const char *filename = __FILE__;
        struct debug_read_file_result file = debug_platform_read_entire_file(filename);
        if(file.contents)
        {
            debug_platform_write_entire_file("test.out", file.contents_size, file.contents);
            debug_platform_free_file_memory(file.contents);
        }
#endif
#endif
        game_state->tone_hz = 256.0f;
        game_state->amplitude = 3000;
        game_state->t_sine = 0.0f;
        game_state->t_jump = 0.0f;

        game_state->green_offset = 1;
        game_state->blue_offset = 2;
        game_state->red_offset = 4;

        game_state->player_x = 100;
        game_state->player_y = 100;

        memory->is_initialized = true;
    }

    for(size_t i = 0; i < ArrayCount(input->controllers); ++i)
    {
        struct game_controller_input *controller = get_controller(input, i);
        if(controller->is_analog)
        {
            /* TODO: Tune ananlog movement */
            game_state->blue_offset += (i16)(4.0f * controller->stick_average_x);
            game_state->tone_hz = 512.0f + (128.0f * controller->stick_average_y);
        }
        else
        {
            if(controller->move_left.ended_down)
            {
                game_state->blue_offset = -1;
            }
            if(controller->move_right.ended_down)
            {
                game_state->blue_offset = 1;
            }
        }

        game_state->player_x += (int)(4.0f * controller->stick_average_x);
        game_state->player_y += (int)(4.0f * controller->stick_average_y);

        if (game_state->t_jump > 0)
        {
            game_state->player_y += (int)(3.0f * sinf(0.5 * PI * game_state->t_jump));
        }

        if (controller->action_down.ended_down)
        {
            game_state->t_jump = 4.0f;
        }

        /* TODO(Igor): Get game fps update */
        game_state->t_jump -= 0.033f;
    } /* Scan controllers */

    render_weird_gradient(screen_buffer, game_state->blue_offset, game_state->green_offset, game_state->red_offset);
    render_player(screen_buffer, game_state->player_x, game_state->player_y);

    render_player(screen_buffer, input->mouse_x, input->mouse_y);

    for(int button_index = 0; button_index < ArrayCount(input->mouse_buttons); ++button_index)
    {
        if (input->mouse_buttons[button_index].ended_down)
        {
            render_player(screen_buffer, 10 + 20 * button_index, 10);
        }
    }
}

/* Sound entry point */
void
game_get_sound_samples_f(struct game_memory *memory,
                         struct game_sound_output_buffer *sound_buffer)
{
    struct game_state *game_state = (struct game_state *)memory->permanent_storage;
    game_output_sound(game_state, sound_buffer);
}

