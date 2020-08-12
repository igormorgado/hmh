
#include "sdl_game.h"
internal inline u32
SafeTruncateUInt64(u64 Value)
{
    /* TODO: Assert (value <= 0xFFFFFFFF) */
    u32 Result = (u32)Value;
    return Result;
}

internal i32
GameOutputSound(struct game_sound_output_buffer *SoundBuffer, f32 ToneHz)
{
    local_persist f32 TSine;
    i16 amplitude = 3000;
    f32 wave_period = SoundBuffer->SamplesPerSecond / ToneHz;

    i16 *sample_out = SoundBuffer->Samples;
    for(size_t sample_index = 0; sample_index < SoundBuffer->SampleCount; ++sample_index)
    {
        f32 sine_value = sinf(TSine);
        i16 sample_value = (i16)(roundf(sine_value * amplitude));
        *sample_out++ = sample_value;
        *sample_out++ = sample_value;
        TSine += TAU / wave_period;
    }
    return RETURN_SUCCESS;
}

internal i32
RenderWeirdGradient (struct game_offscreen_buffer *ScreenBuffer,
                     i16 BlueOffset, i16 GreenOffset, i16 RedOffset)
{

    // TODO: ASSERT WINDOW NOT NULL
    // TODO: ASSERT 0 < BlueOffset GREEN OFFSET < 255
    u8 *row = (u8 *)ScreenBuffer->Memory;
    for(int y = 0; y < ScreenBuffer->Height; ++y)
    {
        u32 *pixel = (u32*)row;
        for(int x = 0; x < ScreenBuffer->Width; ++x)
        {
            u8 blue = (x + BlueOffset);
            u8 green = (y + GreenOffset);
            u8 red = RedOffset;
            *pixel++ = ((red << 16)  |
                        (green << 8) |
                        (blue << 0));
        }
        row += ScreenBuffer->Pitch;
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


internal void
GameUpdateAndRender(struct game_memory *Memory,
                    struct game_input *Input,
                    struct game_offscreen_buffer *ScreenBuffer,
                    struct game_sound_output_buffer *SoundBuffer)
{
    /* TODO: Assert(sizeof(struct GameState) <= Memory->PermanentStorageSize)
     * Assert((&Input->Controllers[0].Terminator - &Input->Controllers[0].Buttons[0]) ==
     *      (ArrayCount(Input->Controllers[0].Buttons)));
     */

    struct game_state *GameState = (struct game_state *)Memory->PersistentStorage;

    if(!Memory->IsInitialized)
    {

#if DEBUG
        const char *Filename = __FILE__;
        struct debug_read_file_result File = DEBUGPlataformReadEntireFile(Filename);
        if(File.Contents)
        {
            DEBUGPlataformWriteEntireFile("test.out", File.ContentsSize, File.Contents);
            DEBUGPlataformFreeFileMemory(File.Contents);
        }
#endif
        GameState->ToneHz = 256.0f;
        Memory->IsInitialized = true;
    }

    for(size_t i = 0; i < ArrayCount(Input->Controllers); ++i)
    {
        struct game_controller_input *Controller = GetController(Input, i);
        if(Controller->IsAnalog)
        {
            /* TODO: Tune ananlog movement */
            GameState->BlueOffset += (i16)(4.0f * Controller->StickAverageX);
            GameState->ToneHz = 256.0f + 128.0f * Controller->StickAverageY;
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
    GameOutputSound(SoundBuffer, GameState->ToneHz);
    RenderWeirdGradient(ScreenBuffer, GameState->BlueOffset, GameState->GreenOffset, GameState->RedOffset);
}
