/* Plataform independant header/code */
#include "game.h"
#include "game.c"

/* Plataform dependent header */
#include "sdl_game.h"

/* Since we are using SDL Textures we still use SDL dependant code */
/* TODO: MOVE GLOBAL TO LOCAL */
global_variable struct sdl_offscreen_buffer GlobalBackBuffer;
global_variable struct sdl_audio_ring_buffer GlobalAudioRingBuffer;
global_variable bool GlobalPaused;

#if DEBUG
internal struct debug_read_file_result
DEBUG_plataform_read_entire_file(const char *filename)
{
    struct debug_read_file_result result = {};

    int file_handle = open(filename, O_RDONLY);
    if(file_handle == -1)
    {
        return result;
    }

    struct stat file_status;
    if(fstat(file_handle, &file_status) == -1)
    {
        close(file_handle);
        return result;
    }
    result.contents_size = safe_truncate_u64(file_status.st_size);

    /* TODO: Use a internal game malloc */
    result.contents = malloc(result.contents_size);
    if(!result.contents)
    {
        close(file_handle);
        result.contents_size = 0;
        return result;
    }

    u32 bytes_to_read = result.contents_size;
    u8 *next_byte_location = (u8 *)result.contents;
    while(bytes_to_read)
    {
        ssize_t bytes_read = read(file_handle, next_byte_location, bytes_to_read);

        if(bytes_read == -1)
        {
            free(result.contents);
            result.contents = 0;
            result.contents_size = 0;
            close(file_handle);
            return result;
        }
        bytes_to_read -= bytes_read;
        next_byte_location += bytes_read;
    }
    close (file_handle);
    return result;
}

internal void
DEBUG_plataform_free_file_memory(void *memory)
{
    free(memory);
}

internal bool
DEBUG_plataform_write_entire_file(const char *filename, size_t memory_size, void *memory)
{
    int file_handle = open(filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (!file_handle)
    {
        return false;
    }

    size_t bytes_to_write = memory_size;
    u8 *next_byte_location = (u8*)memory;
    while(bytes_to_write)
    {
        ssize_t bytes_written = write(file_handle, next_byte_location, bytes_to_write);
        if (bytes_written == -1)
        {
            close(file_handle);
            return false;
        }
        bytes_to_write -= bytes_written;
        next_byte_location += bytes_written;
    }
    close(file_handle);
    return true;
}
#endif

internal void
sdl_audio_callback(void *user_data, u8 *audio_data, int length)
{
    struct sdl_audio_ring_buffer *ring_buffer = (struct sdl_audio_ring_buffer *)user_data;

    i32 region_1_size = length;
    i32 region_2_size = 0;
    /* Not enough space on ring buffer region */
    if ( (ring_buffer->play_cursor + length) > ring_buffer->size)
    {
        region_1_size = ring_buffer->size - ring_buffer->play_cursor;
        region_2_size = length - region_1_size;
    }

    memcpy(audio_data, (u8*)(ring_buffer->data) + ring_buffer->play_cursor, region_1_size);
    memcpy(audio_data + region_1_size, ring_buffer->data, region_2_size);
    ring_buffer->play_cursor =  (ring_buffer->play_cursor + length) % ring_buffer->size;
    ring_buffer->write_cursor = (ring_buffer->play_cursor + length) % ring_buffer->size;
}

internal void
sdl_init_audio(struct sdl_sound_output *sound_output)
{
    SDL_AudioSpec audio_settings = {};

    audio_settings.freq = sound_output->samples_per_second;
    audio_settings.format = AUDIO_S16LSB;
    audio_settings.channels = 2;
    audio_settings.samples = 512;
    audio_settings.callback = &sdl_audio_callback;
    audio_settings.userdata = &GlobalAudioRingBuffer;

    GlobalAudioRingBuffer.size = sound_output->secondary_buffer_size;
    GlobalAudioRingBuffer.play_cursor = 0;
    GlobalAudioRingBuffer.write_cursor = 0;
    //GlobalAudioRingBuffer.data = calloc(sound_output->buffer_size, 1);
    GlobalAudioRingBuffer.data = mmap(NULL,
                                      sound_output->secondary_buffer_size,
                                      PROT_READ | PROT_WRITE,
                                      MAP_ANONYMOUS | MAP_PRIVATE,
                                      -1,
                                      0);

    if(!GlobalAudioRingBuffer.data)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: mmap(GlobalAudioRingBuffer.data)\n", __func__);
        exit(EXIT_FAILURE);
    }

    sound_output->audio_device = SDL_OpenAudioDevice(NULL, 0, &audio_settings, NULL, 0);
    SDL_Log("%s: Audio opened ID (%d): freq: %d, channels: %d, samples: %d, bufsz: %d\n",
            __func__,
            sound_output->audio_device,
            audio_settings.freq,
            audio_settings.channels,
            audio_settings.samples,
            audio_settings.size);

    if (audio_settings.format != AUDIO_S16LSB)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_OpenAudio(): Could not get S16LE format\n",
                __func__);
        SDL_CloseAudioDevice(sound_output->audio_device);
        sound_output->audio_device = 0;
    }
}

internal void
sdl_SoundGetCursors(struct sdl_audio_ring_buffer *ring_buffer,
                    struct sdl_sound_output *sound_output,
                    struct sdl_performance_counters *perf,
                    struct sdl_audio_counters *audio_perf,
                    u64 *byte_to_lock, u64 *bytes_to_write)
{

    SDL_LockAudioDevice(sound_output->audio_device);
    *byte_to_lock = (sound_output->running_sample_index * sound_output->bytes_per_sample) % sound_output->secondary_buffer_size;

#if 0
    u64 expected_sound_bytes_per_frame = (sound_output->SamplesPerSecond * sound_output->bytes_per_sample)/ perf->game_update_hz;
    f64 seconds_left_until_flip = perf->target_seconds_per_frame - audio_perf->from_begin_to_audio_seconds;
    u64 expected_bytes_until_flip = (size_t)(( seconds_left_until_flip / perf->target_seconds_per_frame )*(f64)expected_sound_bytes_per_frame);
    u64 expected_frame_boundary_byte = ring_buffer->play_cursor + expected_sound_bytes_per_frame;

    u64 safe_write_cursor = ring_buffer->write_cursor;
    if(safe_write_cursor < ring_buffer->PlayCursor)
    {
        safe_write_cursor += sound_output->secondary_buffer_size;
    }

    /* TODO: Assert(safe_write_cursor >= PlayCursor); */
    safe_write_cursor += sound_output->safety_bytes;

    bool audio_card_is_low_latency = (safe_write_cursor < expected_frame_boundary_byte);
#endif

    u64 target_cursor = 0;
#if 0
    if(audio_card_is_low_latency)
    {
        target_cursor = (expected_frame_boundary_byte + expected_sound_bytes_per_frame);
    }
    else
    {
        target_cursor = (ring_buffer->write_cursor + expected_sound_bytes_per_frame +
                        sound_output->safety_bytes);
    }
    target_cursor = (target_cursor % sound_output->secondary_buffer_size);
#endif
    target_cursor = (ring_buffer->play_cursor + (sound_output->safety_bytes * sound_output->bytes_per_sample)) % sound_output->secondary_buffer_size;


    if(*byte_to_lock > target_cursor)
    {
        *bytes_to_write =  (sound_output->secondary_buffer_size - *byte_to_lock);
        *bytes_to_write += target_cursor;
    }
    else
    {
        *bytes_to_write = target_cursor - *byte_to_lock;
    }
    SDL_UnlockAudioDevice(sound_output->audio_device);

#if 0
    audio_perf->target_cursor = target_cursor;
    audio_perf->expected_frame_boundary_byte = expected_frame_boundary_byte;
#endif
}

internal void
sdl_FillSoundBuffer(struct sdl_sound_output *sound_output,
                    int byte_to_lock,
                    int bytes_to_write,
                    struct game_sound_output_buffer *sound_buffer)
{
    i16 *samples = sound_buffer->samples;
    i16 *sample_out = NULL;

    /* Calculate region sizes */
    void *region1 = (u8*)GlobalAudioRingBuffer.data + byte_to_lock;
    u64 region1_size = bytes_to_write;

    if(region1_size + byte_to_lock > sound_output->secondary_buffer_size)
        region1_size = sound_output->secondary_buffer_size - byte_to_lock;

    void *region2 = GlobalAudioRingBuffer.data;
    u64 region2_size = bytes_to_write - region1_size;

    /* Fill region 1 */
    u64 region1_sample_count = region1_size / sound_output->bytes_per_sample;
    sample_out = (i16*)region1;
    for (u64 i = 0; i < region1_sample_count; ++i)
    {
        *sample_out++ = *samples++;     // L
        *sample_out++ = *samples++;     // R
        ++sound_output->running_sample_index;
    }

    /* Fill region 2 */
    u64 region2_sample_count = region2_size / sound_output->bytes_per_sample;
    sample_out = (i16*)region2;
    for (u64 i = 0; i < region2_sample_count; ++i)
    {
        *sample_out++ = *samples;     // L
        *sample_out++ = *samples;     // R
        ++sound_output->running_sample_index;
    }
}

internal int
sdl_GetWindowRefreshRate(SDL_Window *Window)
{
    SDL_DisplayMode Mode;
    int DisplayIndex = SDL_GetWindowDisplayIndex(Window);
    int DefaultRefreshRate = 60;
    if (SDL_GetDesktopDisplayMode(DisplayIndex, &Mode) != 0)
    {
        return DefaultRefreshRate;
    }
    if (Mode.refresh_rate == 0)
    {
        return DefaultRefreshRate;
    }
    return Mode.refresh_rate;
}

#if DEBUG
internal void
sdl_DebugDrawVertical(struct sdl_offscreen_buffer *BackBuffer,
                      int X,
                      int Top,
                      int Bottom,
                      u32 Color)
{
    u8 *Pixel = ((u8*)BackBuffer->Memory +
                 X * BackBuffer->BytesPerPixel +
                 Top * BackBuffer->Pitch);

    for(int y = Top; y < Bottom; ++y)
    {
        *(u32*)Pixel = Color;
        Pixel += BackBuffer->Pitch;
    }
}

internal inline void
sdl_DrawSoundBufferMarker(struct sdl_offscreen_buffer *BackBuffer,
                          struct sdl_sound_output *SoundOutput,
                          f32 C, int PadX, int Top,
                          int Bottom, int Value, u32 Color)
{
    /* TODO: Assert(value < soundOutput -> Secondary buffer size); */
    f32 x32 = C * (f32)Value;
    int X = PadX + (int)x32;
    sdl_DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void
sdl_DebugSyncDisplay(struct sdl_offscreen_buffer *BackBuffer,
                     int MarkerCount,
                     struct sdl_debug_time_marker *Markers,
                     int CurrentMarkerIndex,
                     struct sdl_sound_output *SoundOutput,
                     f32 TargetSecondsPerFrame)
{
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    f32 C = (f32)(BackBuffer->Width - 2*PadX) / (f32)SoundOutput->secondary_buffer_size;
    for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
    {
        struct sdl_debug_time_marker *ThisMarker = Markers + MarkerIndex;
        // TODO: Assert(ThisMarker->OutputPlayCursor < SoundOutput->secondary_buffer_size);
        // TODO: Assert(ThisMarker->OutputWriteCursor < SoundOutput->secondary_buffer_size);
        // TODO: Assert(ThisMarker->OutputLocation < SoundOutput->secondary_buffer_size);
        // TODO: Assert(ThisMarker->OutputByteCount < SoundOutput->secondary_buffer_size);
        // TODO: Assert(ThisMarker->FlipPlayCursor < SoundOutput->secondary_buffer_size);
        // TODO: Assert(ThisMarker->FlipWriteCursor < SoundOutput->secondary_buffer_size);
        u32 PlayColor         = 0xFFFFFFFF;
        u32 WriteColor        = 0xFFFF0000;
        u32 ExpectedFlipColor = 0xFFFFFF00;
        u32 PlayWindowColor   = 0xFFFF00FF;

        int Top = PadY;
        int Bottom = PadY + LineHeight;
        if(MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            int FirstTop = Top;

            sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputPlayCursor,  PlayColor);
            sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputWriteCursor, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation,  PlayColor);
            sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->OutputLocation + ThisMarker->OutputByteCount, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, FirstTop, Bottom, ThisMarker->ExpectedFlipPlayCursor,  ExpectedFlipColor);
        }

        sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor,  PlayColor);
        sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipPlayCursor + 480*SoundOutput->bytes_per_sample,  PlayWindowColor);
        sdl_DrawSoundBufferMarker(BackBuffer, SoundOutput, C, PadX, Top, Bottom, ThisMarker->FlipWriteCursor,  WriteColor);
    }
}
#endif

internal struct sdl_window_dimension
sdl_WindowGetDimension(SDL_Window *window)
{
    struct sdl_window_dimension result;
    SDL_GetWindowSize(window, &result.Width, &result.Height);
    return result;
}

/* NOTE: Name in hmh is SDLResizeTexture */
internal int
sdl_ResizeBackBuffer(struct sdl_offscreen_buffer *screenbuffer, SDL_Window *window, u16 width, u16 height)
{
    // TODO: ASSERT WINDOW not null
    // TODO: ASSERT 0 < width/height <= 4096

    int bytes_per_pixel = SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888);
    if(screenbuffer->Memory)
    {
        // free(screenbuffer->Memory);
        munmap(screenbuffer->Memory, screenbuffer->Width * screenbuffer->Height * bytes_per_pixel);
    }

    if(screenbuffer->Texture)
    {
        SDL_DestroyTexture(screenbuffer->Texture);
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    screenbuffer->Texture = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        width,
                                        height);

    if(!screenbuffer->Texture)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: SDL_CreateTexture(GlobalBackBuffer): %s\n",
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
    screenbuffer->Width = width;
    screenbuffer->Height = height;
    screenbuffer->Pitch = width * bytes_per_pixel;
    screenbuffer->BytesPerPixel = bytes_per_pixel;
    // screenbuffer->Memory = malloc(screenbuffer->Width * screenbuffer->Height * bytes_per_pixel);
    screenbuffer->Memory = mmap(NULL,
                        screenbuffer->Width * screenbuffer->Height * bytes_per_pixel,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if(!screenbuffer->Memory)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: malloc(screenbuffer->Memory): %s\n",
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
    return RETURN_SUCCESS;
}

internal int
sdl_UpdateWindow(SDL_Window *window, struct sdl_offscreen_buffer *screenbuffer)
{
    // TODO: ASSERT WINDOW not null
    // TODO: Assert Handle all those "ifs"  with SDL_Asserts.
    int retval = RETURN_FAILURE;
    if(!screenbuffer->Texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: GlobalBackBuffer Texture NULL, nothing to update\n",
                __func__);
        return retval;
    }

    retval = SDL_UpdateTexture(screenbuffer->Texture,
                               NULL,
                               screenbuffer->Memory,
                               screenbuffer->Pitch);

    if(retval < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_UpdateTexture(GlobalBackBuffer): %s\n",
                __func__, SDL_GetError());
        return retval;
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if(!renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_GetRenderer(): %s\n",
                __func__, SDL_GetError());
        return RETURN_FAILURE;
    }

    retval = SDL_RenderCopy(renderer, screenbuffer->Texture, NULL, NULL);
    if(retval < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_RenderCopy(GlobalBackBuffer.texture): %s\n",
                __func__, SDL_GetError());
        return retval;
    }

    SDL_RenderPresent(renderer);
    return RETURN_SUCCESS;
}

internal int
sdl_HandleQuit()
{
    return RETURN_EXIT;
}

/* TODO: It should receive global blackbuffer as argument */
internal int
sdl_HandleWindow(const SDL_Event event)
{
    // TODO: Assert eevent not null
    switch(event.window.event)
    {
        case SDL_WINDOWEVENT_MOVED:
        {} break;

        case SDL_WINDOWEVENT_RESIZED:
        { /* handled in SIZE_CHANGED */ } break;

        case SDL_WINDOWEVENT_SIZE_CHANGED:
        {
            SDL_Log("%s: Window size changed (%d, %d)\n",
                    __func__, event.window.data1, event.window.data2);
        } break;

        case SDL_WINDOWEVENT_FOCUS_GAINED:
        {
            SDL_Log("%s: Window focus gained\n", __func__);
        } break;

        case SDL_WINDOWEVENT_FOCUS_LOST:
        {
            SDL_Log("%s: Window focus lost\n", __func__);
        } break;

        case SDL_WINDOWEVENT_ENTER:
        {
            SDL_Log("%s: Mouse focus gained\n", __func__);
        } break;

        case SDL_WINDOWEVENT_LEAVE:
        {
            SDL_Log("%s: Mouse focus lost\n", __func__);
        } break;

        case SDL_WINDOWEVENT_EXPOSED:
        {
            SDL_Log("%s: Window exposed\n", __func__);
            SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
            sdl_UpdateWindow(window, &GlobalBackBuffer);
        } break;

        case SDL_WINDOWEVENT_CLOSE:
        {
            SDL_Log("%s: Window Close requested\n", __func__);
            return RETURN_EXIT;
        } break;

        case SDL_WINDOWEVENT_TAKE_FOCUS:
        {
            SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
            SDL_RaiseWindow(window);
        } break;

        default:
        {
            SDL_Log("%s: Window event not handled: %d\n", __func__, event.window.event);
        } break;
    }
    return RETURN_SUCCESS;
}

internal void
sdl_ProcessKeyPress(struct game_button_state *NewState, bool IsDown)
{
    // TODO: Assert(NewState->EndedDown != IsDown);
    NewState->EndedDown = IsDown;
    ++NewState->HalfTransitionCount;
}

/* TODO: Pass event as pointer */
internal int
sdl_HandleKey(const SDL_Event event,
              struct game_controller_input *NewKeyboardController)
{
    SDL_Keycode KeyCode = event.key.keysym.sym;
    SDL_Keymod  KeyMod  = event.key.keysym.mod;
    // SDL_Keymod  KeyMod = SDL_GetModState()  // Or this?

    bool IsDown = (event.key.state == SDL_PRESSED);
    bool WasDown = false;
    if (event.key.state == SDL_RELEASED)
    {
        WasDown = true;
    }
    else if (event.key.repeat != 0)
    {
        WasDown = true;
    }

    if (event.key.repeat == 0)
    {
        if(KeyCode == SDLK_w)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->MoveUp, IsDown);
        }
        else if(KeyCode == SDLK_a)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->MoveLeft, IsDown);
        }
        else if(KeyCode == SDLK_s)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->MoveDown, IsDown);
        }
        else if(KeyCode == SDLK_d)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->MoveRight, IsDown);
        }
        else if(KeyCode == SDLK_q)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->Lshoulder, IsDown);
        }
        else if(KeyCode == SDLK_e)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->Rshoulder, IsDown);
        }
        else if(KeyCode == SDLK_UP)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->ActionUp, IsDown);
        }
        else if(KeyCode == SDLK_DOWN)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->ActionDown, IsDown);
        }
        else if(KeyCode == SDLK_LEFT)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->ActionLeft, IsDown);
        }
        else if(KeyCode == SDLK_RIGHT)
        {
            sdl_ProcessKeyPress(&NewKeyboardController->ActionRight, IsDown);
        }
#if DEBUG
        else if(KeyCode == SDLK_SPACE)
        {
            GlobalPaused = !GlobalPaused;
        }
#endif
        else if(KeyCode == SDLK_x)
        {
            SDL_Log("Exiting...\n");
            return RETURN_EXIT;
        }
        else if(KeyCode == SDLK_ESCAPE)
        {
            if(IsDown)
            {
                SDL_Log("Escape Is down\n");
            }
            if(WasDown)
            {
                SDL_Log("Escape Was down\n");
            }
        }
        else if((KeyCode == SDLK_RETURN) && (KeyMod & KMOD_ALT))
        {
            SDL_Log("ALT-ENTER pressed\n");
        }
        else if((KeyCode == SDLK_F4) && (KeyMod & KMOD_ALT))
        {
            SDL_Log("ALT-F4 pressed\n");
            return RETURN_EXIT;
        }
        else
        {
            SDL_Log("Unmapped key pressed\n");
        }
    }
    return RETURN_SUCCESS;
}

internal void
sdl_ProcessGameControllerButton(struct game_button_state *OldState,
                                struct game_button_state *NewState,
                                bool Value)
{
    NewState->EndedDown = Value;
    NewState->HalfTransitionCount += ((NewState->EndedDown == OldState->EndedDown) ? 0:1);
}

internal f32
sdl_ProcessGameControllerAxisValue(i16 Value, i16 DeadZoneThreshold)
{
    f32 Result = 0;
    if (Value < -DeadZoneThreshold)
    {
        Result = (f32)((Value + DeadZoneThreshold) / (32768.0f - DeadZoneThreshold));
    }
    else if (Value > DeadZoneThreshold)
    {
        Result = (f32)((Value - DeadZoneThreshold) / (32767.0f - DeadZoneThreshold));
    }
    return Result;
}

void
sdl_HandleController(struct game_input *NewInput, struct game_input *OldInput)
{
    for(int i = 0; i< MAX_CONTROLLERS; i++)
    {
        if(ControllerHandles[i] != NULL && SDL_GameControllerGetAttached(ControllerHandles[i]))
        {
            /* Controllers start from 1, since 0 is the keyboard. Ugly. But ok */
            struct game_controller_input * OldController = GetController(OldInput, i + 1);
            struct game_controller_input * NewController = GetController(NewInput, i + 1);

            NewController->IsConnected = true;
            NewController->IsAnalog = true;

            sdl_ProcessGameControllerButton(&(OldController->Lshoulder),
                                            &(NewController->Lshoulder),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_LEFTSHOULDER));

            sdl_ProcessGameControllerButton(&(OldController->Rshoulder),
                                            &(NewController->Rshoulder),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));

            sdl_ProcessGameControllerButton(&(OldController->ActionDown),
                                            &(NewController->ActionDown),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_A));

            sdl_ProcessGameControllerButton(&(OldController->ActionRight),
                                            &(NewController->ActionRight),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_B));

            sdl_ProcessGameControllerButton(&(OldController->ActionLeft),
                                            &(NewController->ActionLeft),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_X));

            sdl_ProcessGameControllerButton(&(OldController->ActionUp),
                                            &(NewController->ActionUp),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_Y));

            sdl_ProcessGameControllerButton(&(OldController->Back),
                                            &(NewController->Back),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_BACK));

            sdl_ProcessGameControllerButton(&(OldController->Start),
                                            &(NewController->Start),
                                            SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_START));

            NewController->StickAverageX  = sdl_ProcessGameControllerAxisValue (
                    SDL_GameControllerGetAxis (ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTX), 1);

            NewController->StickAverageY  = sdl_ProcessGameControllerAxisValue (
                    SDL_GameControllerGetAxis (ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTY), 1);

            /* Possible branchless with sum and or */
            if((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f))
            {
                NewController->IsAnalog = true;
            }

            // if(NewController->Start.EndedDown) GlobalPaused = !GlobalPaused;
            if(SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_START))
            {
                GlobalPaused = !GlobalPaused;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_UP))
            {
                NewController->StickAverageY = 1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            {
                NewController->StickAverageY = -1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            {
                NewController->StickAverageX = -1.0f;
                NewController->IsAnalog = false;
            }

            if(SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            {
                NewController->StickAverageX = 1.0f;
                NewController->IsAnalog = false;
            }

            f32 Threshold = 0.5f;
            sdl_ProcessGameControllerButton(&(OldController->MoveLeft),
                                           &(NewController->MoveLeft),
                                           NewController->StickAverageX < -Threshold);
            sdl_ProcessGameControllerButton(&(OldController->MoveRight),
                                           &(NewController->MoveRight),
                                           NewController->StickAverageX > Threshold);
            sdl_ProcessGameControllerButton(&(OldController->MoveUp),
                                           &(NewController->MoveUp),
                                           NewController->StickAverageY < -Threshold);
            sdl_ProcessGameControllerButton(&(OldController->MoveDown),
                                           &(NewController->MoveDown),
                                           NewController->StickAverageY > Threshold);
        } else {
            // TODO: Controller not plugged in (CLOSE IT!)
        }
    }
    return;
}

/* TODO: It should receive globalbackbuffer to pss to Sdl_HandleWindow() */
internal int
sdl_HandleEvent(struct game_input *input)
{
    int retval = RETURN_SUCCESS;

    /* Easier naming */
    struct game_input *NewInput = &input[0];
    struct game_input *OldInput = &input[1];

    /* Initialize and rotate keyboard events */
    struct game_controller_input *NewKeyboardController = GetController(NewInput,0);
    struct game_controller_input *OldKeyboardController = GetController(OldInput,0);
    memset(NewKeyboardController, 0, sizeof(*NewKeyboardController));
    for(size_t i = 0; i < ArrayCount(NewKeyboardController->Buttons); ++i)
    {
        NewKeyboardController->Buttons[i].EndedDown =
        OldKeyboardController->Buttons[i].EndedDown;
    }

    /* Pool SDL Event Queue */
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_QUIT:          retval = sdl_HandleQuit(); break;
            case SDL_WINDOWEVENT:   retval = sdl_HandleWindow(event); break;
            case SDL_KEYUP:         retval = sdl_HandleKey(event, NewKeyboardController); break;
            case SDL_KEYDOWN:       retval = sdl_HandleKey(event, NewKeyboardController); break;
            default: break;
        }
    }
    /* TODO: Use SDL Event pool for Controllers */
    sdl_HandleController(NewInput, OldInput);
    return retval;
}

/* Original name: SDLOpenGameControllers() */
internal int
sdl_GameControllersInit()
{
    int njoys = SDL_NumJoysticks();
    int ControllerIndex = 0;
    for (int i=0; i < njoys; ++i)
    {
        if(ControllerIndex >= MAX_CONTROLLERS)
        {
            break;
        }
        if(SDL_IsGameController(i))
        {
            ControllerHandles[ControllerIndex] = SDL_GameControllerOpen(i);
            RumbleHandles[ControllerIndex] = SDL_HapticOpen(i);
            if(SDL_HapticRumbleInit(RumbleHandles[ControllerIndex]) < 0)
            {
                SDL_HapticClose(RumbleHandles[ControllerIndex]);
                RumbleHandles[ControllerIndex] = NULL;
            }
            ControllerIndex++;
        }
    }
    return ControllerIndex;
}

/* Original: SDLColseGameControllers */
internal void
sdl_GameControllersQuit()
{
    for (int i = 0; i < MAX_CONTROLLERS; i++)
    {
        SDL_Log("%s: %d Controller @%p \t Rumble @%p\n", __func__, i,
                (void*)ControllerHandles[i], (void*)RumbleHandles[i]);
        if (ControllerHandles[i]) {
            SDL_Log("%s: Closing Controller %d @%p\n", __func__, i, (void*)ControllerHandles[i]);
            SDL_GameControllerClose(ControllerHandles[i]);
            ControllerHandles[i] = NULL;
        }
        if (RumbleHandles[i] != NULL) {
            SDL_Log("%s: Closing Haptic %d @%p\n", __func__, i, (void*)RumbleHandles[i]);
            SDL_HapticClose(RumbleHandles[i]);
            RumbleHandles[i] = NULL;
        }
    }
}

internal SDL_Window *
sdl_InitGame(u16 screen_width, u16 screen_height)
{
    // TODO: Assert Window not null
    // TODO: Assert widht/height < 4096
    int retval;
    int sdl_flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO;
    int wnd_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    int rnd_flags = SDL_RENDERER_ACCELERATED; // | SDL_RENDERER_PRESENTVSYNC;

    retval = SDL_Init(sdl_flags);
    if(retval < 0)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: SDL_Init(): %s\n",
                      __func__, SDL_GetError());
        return NULL;
    }

    SDL_Window *window;
    window = SDL_CreateWindow("HMH",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              screen_width,
                              screen_height,
                              wnd_flags);
    if(!window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_CreateWindow(): %s\n",
                __func__, SDL_GetError());
        return NULL;
    }

    SDL_Renderer *renderer;
    renderer = SDL_CreateRenderer(window, -1, rnd_flags);
    if(!renderer)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_CreateRenderer(): %s\n",
                __func__,
                SDL_GetError());
        return NULL;
    }

    SDL_RenderSetLogicalSize(renderer, screen_width, screen_height);

    sdl_GameControllersInit();

    return window;
}

internal void
sdl_ExitGame (SDL_Window *window)
{
    // TODO: Assert Window not null
    SDL_Log ("Exiting\n");
    sdl_GameControllersQuit();
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    //if (GlobalBackBuffer.memory)    free(GlobalBackBuffer.memory);
    munmap(GlobalBackBuffer.Memory, GlobalBackBuffer.Width * GlobalBackBuffer.Height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));
    if (GlobalBackBuffer.Texture)   SDL_DestroyTexture(GlobalBackBuffer.Texture);
    if (renderer)                   SDL_DestroyRenderer(renderer);
    if (window)                     SDL_DestroyWindow(window);
#if 0
    // No need to free since it was not malloc'd
    if (GlobalAudioRingBuffer.data)            free(GlobalAudioRingBuffer.data);
#endif
    SDL_Quit();
    return;
}

/* TODO BUG HERE: u64 to f32 */
internal f64
sdl_GetSecondsElapsed(u64 OldCounter, u64 CurrentCounter, u64 PerfCountFrequency)
{
    // WARNING: Should I call SDL_GerPerformanceFrequency instead pass as value?
    f64 result = ((f64)(CurrentCounter - OldCounter) / (f64)PerfCountFrequency);
    return result;
}

internal void
sdl_WaitFrame(struct sdl_performance_counters *perf)
{
    if(sdl_GetSecondsElapsed(perf->last_counter, SDL_GetPerformanceCounter(), perf->performance_count_frequency) < perf->target_seconds_per_frame)
    {
        i32 TimeToSleep = ((perf->target_seconds_per_frame - sdl_GetSecondsElapsed(perf->last_counter, SDL_GetPerformanceCounter(), perf->performance_count_frequency)) * 1000) - 1;
        if (TimeToSleep > 0)
        {
            SDL_Delay(TimeToSleep);
        }

        // TODO: Assert(SDLGetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame)
        while (sdl_GetSecondsElapsed(perf->last_counter, SDL_GetPerformanceCounter(), perf->performance_count_frequency) < perf->target_seconds_per_frame)
        {
            // Waiting...
        }
    }

    perf->end_counter = SDL_GetPerformanceCounter();
}

internal void
sdl_EvalPerformance(struct sdl_performance_counters *perf)
{
    perf->end_cycle_count = _rdtsc();
    perf->counter_elapsed = perf->end_counter - perf->last_counter;
    perf->cycles_elapsed = perf->end_cycle_count - perf->last_cycle_count;
    perf->ms_per_frame = 1000.0 * (f64)(perf->counter_elapsed) / (f64)(perf->performance_count_frequency);
    perf->fps = (f64)(perf->performance_count_frequency) / (f64)(perf->counter_elapsed);
    perf->fps = (f64)(perf->cycles_elapsed) / 1000000.0;
    perf->last_cycle_count = perf->end_cycle_count;
    perf->last_counter    = perf->end_counter;
}

int
main (void)
{
    int exitval = EXIT_FAILURE;
    int retval = RETURN_SUCCESS;
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);

    /*
     * Game Window Initialization
     */

    SDL_Window *window = sdl_InitGame(960, 540);
    if(!window) goto __EXIT__;

    struct sdl_performance_counters perf = {};
    perf.game_update_hz = sdl_GetWindowRefreshRate(window);
    perf.target_seconds_per_frame = 1.0f/(f64)(perf.game_update_hz);

    SDL_Log("Refresh rate is %d Hz\n", perf.game_update_hz);
    SDL_Log("Target FPS is %f Hz\n", perf.target_seconds_per_frame);

    struct sdl_window_dimension wdim = sdl_WindowGetDimension(window);
    /* Named sdl_resize_texture */
    sdl_ResizeBackBuffer(&GlobalBackBuffer, window, wdim.Width, wdim.Height);

    /*
     * Game Input initialization
     */
    struct game_input input[2] = {0};
    struct game_input *new_input = input;
    struct game_input *old_input = input + 1;

    /*
     * Game sound initialization
     */
    struct sdl_sound_output sound_output = {0};
    /* TODO: Fetch sound device from a list */
    sound_output.audio_device = 0;
    sound_output.samples_per_second = 48000;
    sound_output.running_sample_index = 0;
    sound_output.bytes_per_sample = sizeof(i16) * 2;
    sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
    sound_output.safety_bytes = (int)(((f32)sound_output.samples_per_second * (f32)sound_output.bytes_per_sample / perf.game_update_hz)/ 3.0f);

    /* WARNING: DO not exist in newer HMH codes */
    sound_output.latency_sample_count = 2 * sound_output.samples_per_second / perf.game_update_hz;

    /* Initialize sound paused*/
    sdl_init_audio(&sound_output);
    SDL_PauseAudioDevice(sound_output.audio_device, SDL_FALSE);

    //i16 *samples = calloc(sound_output.samples_per_second, sound_output.bytes_per_sample);

    // WARNING: New stuff:  why 2 * 8 ?
    size_t max_possible_overrun = (2 * 8 * sizeof(u16));
    i16 *samples = (i16 *)mmap(NULL,
                               sound_output.secondary_buffer_size + max_possible_overrun,
                               PROT_READ | PROT_WRITE,
                               MAP_ANONYMOUS | MAP_PRIVATE,
                               -1,
                               0);

    /*
     * Game Memory Initialization
     */
#if GAME_INTERNAL
    void *base_address = (void*)Terabytes(2);
#else
    void *base_address = (void*)(0);
#endif

    struct game_memory game_memory = {};
    game_memory.permanent_storage_size = Megabytes(256);
    game_memory.transient_storage_size = Gigabytes(1);
    size_t total_storage_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;
    game_memory.permanent_storage = mmap(base_address, total_storage_size,
                                          PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS | MAP_PRIVATE,
                                          -1, 0);

    /* TODO: Use SDL Assertions */
    assert(game_memory.permanent_storage != MAP_FAILED);


#if DEBUG
    int debug_time_marker_index = 0;
    int time_marker_count = perf.game_update_hz/2;
    struct sdl_debug_time_marker *debug_time_markers = calloc((size_t)(TimeMarkerCount), sizeof(*DebugTimeMarkers));
#endif

    /* Game Performance initialization */
    perf.last_counter = SDL_GetPerformanceCounter();
    perf.last_cycle_count = _rdtsc();
    perf.performance_count_frequency = SDL_GetPerformanceFrequency();

    /* Audio Performance Counters */
    struct sdl_audio_counters audio_perf = {0};
    u64 audio_latency_bytes = 0;
    f64 audio_latency_seconds = 0.0;
    audio_perf.flip_wall_clock = SDL_GetPerformanceCounter();

    bool running = true;
    while(running == true)
    {
        /* Handle Input */
        /*
         * TODO: Remove GameController Handles from global and pass it
         *       as argument to HandleEvent
         */
        retval = sdl_HandleEvent(input);
        if(retval == RETURN_EXIT) running = false;

        if(!GlobalPaused)
        {
            /* Draw Screen buffer */
            struct game_offscreen_buffer screen_buffer = {0};
            screen_buffer.Memory = GlobalBackBuffer.Memory;
            screen_buffer.Width = GlobalBackBuffer.Width;
            screen_buffer.Height = GlobalBackBuffer.Height;
            screen_buffer.Pitch = GlobalBackBuffer.Pitch;

            /* Blit Video */
            GameUpdateAndRender(&game_memory, new_input, &screen_buffer);

            audio_perf.audio_wall_clock = SDL_GetPerformanceCounter();
            audio_perf.from_begin_to_audio_seconds =
                    sdl_GetSecondsElapsed(audio_perf.flip_wall_clock,
                                          audio_perf.audio_wall_clock,
                                          perf.performance_count_frequency);

            /* Render Audio */
            u64 byte_to_lock = 0;
            u64 bytes_to_write = 0;
            sdl_SoundGetCursors(&GlobalAudioRingBuffer,
                                &sound_output,
                                &perf,
                                &audio_perf,
                                &byte_to_lock, &bytes_to_write);

            struct game_sound_output_buffer sound_buffer = {0};
            sound_buffer.samples_per_second = sound_output.samples_per_second;
            sound_buffer.sample_count = Align8(bytes_to_write / sound_output.bytes_per_sample);
            sound_buffer.samples = samples;

            // Aligned bytes to write... TODO: Make it simpler
            //  WARNING: MAYBE ISSUE IS HERE?
            bytes_to_write =  sound_buffer.sample_count * sound_output.bytes_per_sample;

            GameGetSoundSamples(&game_memory, &sound_buffer);

#if DEBUG
            struct sdl_debug_time_marker *Marker = &DebugTimeMarkers[DebugTimeMarkerIndex];
            Marker->OutputPlayCursor = GlobalAudioRingBuffer.PlayCursor;
            Marker->OutputWriteCursor = GlobalAudioRingBuffer.WriteCursor;
            Marker->OutputLocation = byte_to_lock;
            Marker->OutputByteCount = bytes_to_write;
            Marker->ExpectedFlipPlayCursor = audio_perf.ExpectedFrameBoundaryByte;
#endif

#if 0
	    // THis code is missing in HMH SDL code.
            size_t UnwrappedWriteCursor = GlobalAudioRingBuffer.write_cursor;
            if(UnwrappedWriteCursor < GlobalAudioRingBuffer.play_cursor)
            {
                UnwrappedWriteCursor += sound_output.secondary_buffer_size;
            }

            size_t AudioLatencyBytes = UnwrappedWriteCursor - GlobalAudioRingBuffer.PlayCursor;
            f32 AudioLatencySeconds =
                (((f32)AudioLatencyBytes / (f32)sound_output.bytes_per_sample) /
                 (f32)sound_output.SamplesPerSecond);

            // char TextBuffer[256];
            // snprintf(TextBuffer, sizeof(TextBuffer),
            //         "BTL: %6lu TC: %6lu BTW: %6lu - PC: %6lu WC: %6lu - DELTA: %5lu (%9.7fs)\n",
            //         byte_to_lock, audio_perf.TargetCursor, bytes_to_write,
            //         GlobalAudioRingBuffer.PlayCursor, GlobalAudioRingBuffer.WriteCursor,
            //         AudioLatencyBytes, AudioLatencySeconds);
            // SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", TextBuffer);
#endif

            /* "Blit" Audio*/
            sdl_FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);

            /* Swap Inputs */
            struct game_input *temp_input = new_input;
            new_input = old_input;
            old_input = temp_input;

            /* Wait time frame */
            sdl_WaitFrame(&perf);

#if DEBUG
            sdl_DebugSyncDisplay(&GlobalBackBuffer,
                    TimeMarkerCount, DebugTimeMarkers, DebugTimeMarkerIndex - 1,
                    &sound_output, perf.TargetSecondsPerFrame);
#endif

            sdl_UpdateWindow(window, &GlobalBackBuffer);

            audio_perf.flip_wall_clock = SDL_GetPerformanceCounter();

#if DEBUG
            // TODO: Assert(DebugTimeMarkerIndex < ArrayCount(DebugTimeMarkers));
            struct sdl_debug_time_marker *ptrmarker = DebugTimeMarkers + DebugTimeMarkerIndex;
            ptrmarker->FlipPlayCursor = GlobalAudioRingBuffer.PlayCursor;
            ptrmarker->FlipWriteCursor = GlobalAudioRingBuffer.WriteCursor;
#endif

            /* Eval frame rate */
            sdl_EvalPerformance(&perf);

#if 0
            char FPSBuffer[256];
            snprintf (FPSBuffer, sizeof(FPSBuffer),
                      "%6.02fms/f, %6.02ff/s %6.02fmc/f\n",
                      perf.MSPerFrame,
                      perf.FPS,
                      perf.MCPF);
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", FPSBuffer);
#endif

#if DEBUG
            ++DebugTimeMarkerIndex;
            DebugTimeMarkerIndex %= TimeMarkerCount;
#endif
        }
    }

    exitval = EXIT_SUCCESS;
__EXIT__:
#if DEBUG
    free(DebugTimeMarkers);
#endif
    if (sound_output.audio_device > 0) {
        munmap(GlobalAudioRingBuffer.data, sound_output.secondary_buffer_size);
        //free(samples);
        munmap(samples, sound_output.secondary_buffer_size + max_possible_overrun);
        SDL_CloseAudioDevice(sound_output.audio_device);
    }
    sdl_ExitGame(window);
    return exitval;
}

