#include "game.h"

#include "sdl_game.h"


internal i32
GameOutputSound(struct game_sound_output_buffer *SoundBuffer, f32 ToneHz)
{
    local_persist f32 tSine;
    i16 amplitude = 1000;
    f32 wave_period = SoundBuffer->samples_per_second / ToneHz;

    i16 *sample_out = SoundBuffer->samples;
    for(size_t i = 0; i < SoundBuffer->sample_count; ++i)
    {
        f32 sine_value = sinf(tSine);
        i16 sample_value = (i16)(sine_value * amplitude);
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;
        tSine += 2.0f * PI * 1.0f / (f32)wave_period;
        if(tSine > 2.0f * PI) tSine -= 2.0f * PI;
    }
    return RETURN_SUCCESS;
}

internal i32
RenderWeirdGradient (struct game_offscreen_buffer *screen_buffer,
                     i16 BlueOffset, i16 GreenOffset, i16 RedOffset)
{

    // TODO: ASSERT WINDOW NOT NULL
    // TODO: ASSERT 0 < BlueOffset GREEN OFFSET < 255
    u8 *row = (u8 *)screen_buffer->Memory;
    for(int y = 0; y < screen_buffer->Height; ++y)
    {
        u32 *pixel = (u32*)row;
        for(int x = 0; x < screen_buffer->Width; ++x)
        {
            u8 blue = (x + BlueOffset);
            u8 green = (y + GreenOffset);
            u8 red = RedOffset;
            *pixel++ = ((red << 16)  |
                        (green << 8) |
                        (blue << 0));
        }
        row += screen_buffer->Pitch;
    }
    return RETURN_SUCCESS;
}

internal struct game_state *
CreateGameState(void)
{
    struct game_state *state = malloc(sizeof *state);
    if (state)
    {
        state->BlueOffset = 0;
        state->GreenOffset = 0;
        state->RedOffset = 0;
        state->ToneHz = 256.0f;
    }
    return state;
}


/* Renderer entry point */
internal void
game_update_and_render(struct game_memory *memory,
                    struct game_input *input,
                    struct game_offscreen_buffer *screen_buffer)
{
    /* TODO: Assert(sizeof(struct GameState) <= Memory->permanent_storageSize)
     * Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
     *      (ArrayCount(Input->Controllers[0].Buttons)));
     */

    struct game_state *game_state = (struct game_state *)memory->permanent_storage;

    if(!memory->is_initialized)
    {

#if DEBUG
        const char *filename = __FILE__;
        struct debug_read_file_result File = debug_plataform_read_entire_file(filename);
        if(file.contents)
        {
            debug_plataform_write_entire_file("test.out", file.contents_size, file.contents);
            debug_plataform_free_file_memory(file.contents);
        }
#endif
        game_state->tone_hz = 512.0f;
        memory->is_initialized = true;
    }

    for(size_t i = 0; i < ArrayCount(input->controllers); ++i)
    {
        struct game_controller_input *Controller = GetController(Input, i);
        if(Controller->IsAnalog)
        {
            /* TODO: Tune ananlog movement */
            GameState->BlueOffset += (i16)(4.0f * Controller->StickAverageX);
            GameState->ToneHz = 512.0f + 256.0f * Controller->StickAverageY;
        }
        else
        {
            if(Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset = -1;
            }
            if(Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset = 1;
            }
        }

        if(Controller->ActionDown.EndedDown)
        {
            GameState->GreenOffset -= 1;
        }

        if(Controller->ActionUp.EndedDown)
        {
            GameState->GreenOffset += 1;
        }
    }
    // GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradient(screen_buffer, GameState->BlueOffset, GameState->GreenOffset, GameState->RedOffset);
}

/* Sound entry point */
internal void
GameGetSoundSamples(struct game_memory *Memory,
                    struct game_sound_output_buffer *SoundBuffer)
{
    struct game_state *GameState = (struct game_state *)Memory->permanent_storage;
    GameOutputSound(SoundBuffer, GameState->ToneHz);
}
