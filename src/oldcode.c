/*
 *
 * ABANDONED
 *
 *
 */



#if 0 /* CODE NOT USED ANYMORE */

#if 0


#if 0
            char FPSBuffer[256];
            snprintf (FPSBuffer, sizeof(FPSBuffer),
                      "%6.02fms/f, %6.02ff/s %6.02fmc/f\n",
                      perf.ms_per_frame,
                      perf.fps,
                      perf.mcpf);
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", FPSBuffer);
#endif

#if DEBUG
            ++debug_time_marker_index;
            debug_time_marker_index %= time_marker_count;
#endif
        }
    }

    exitval = EXIT_SUCCESS;
#endif



#if 0  /* REDO LATER */

void
sdl_HandleController(struct game_input *new_input, struct game_input *old_input)
{
    for(int i = 0; i< MAX_CONTROLLERS; i++)
    {
        if(controller_handles[i] != NULL && SDL_GameControllerGetAttached(controller_handles[i]))
        {
            /* Controllers start from 1, since 0 is the keyboard. Ugly. But ok */
            struct game_controller_input * old_controller = get_controller(old_input, i + 1);
            struct game_controller_input * new_controller = get_controller(new_input, i + 1);

            new_controller->IsConnected = true;
            new_controller->is_analog = true;

            sdl_ProcessGameControllerButton(&(old_controller->Lshoulder),
                                            &(new_controller->Lshoulder),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_LEFTSHOULDER));

            sdl_ProcessGameControllerButton(&(old_controller->Rshoulder),
                                            &(new_controller->Rshoulder),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));

            sdl_ProcessGameControllerButton(&(old_controller->ActionDown),
                                            &(new_controller->ActionDown),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_A));

            sdl_ProcessGameControllerButton(&(old_controller->ActionRight),
                                            &(new_controller->ActionRight),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_B));

            sdl_ProcessGameControllerButton(&(old_controller->ActionLeft),
                                            &(new_controller->ActionLeft),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_X));

            sdl_ProcessGameControllerButton(&(old_controller->ActionUp),
                                            &(new_controller->ActionUp),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_Y));

            sdl_ProcessGameControllerButton(&(old_controller->Back),
                                            &(new_controller->Back),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_BACK));

            sdl_ProcessGameControllerButton(&(old_controller->Start),
                                            &(new_controller->Start),
                                            SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_START));

            new_controller->StickAverageX  = sdl_ProcessGameControllerAxisValue (
                    SDL_GameControllerGetAxis (controller_handles[i], SDL_CONTROLLER_AXIS_LEFTX), 1);

            new_controller->StickAverageY  = sdl_ProcessGameControllerAxisValue (
                    SDL_GameControllerGetAxis (controller_handles[i], SDL_CONTROLLER_AXIS_LEFTY), 1);

            /* Possible branchless with sum and or */
            if((new_controller->StickAverageX != 0.0f) || (new_controller->StickAverageY != 0.0f))
            {
                new_controller->is_analog = true;
            }

            // if(new_controller->Start.EndedDown) GlobalPaused = !GlobalPaused;
            if(SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_START))
            {
                GlobalPaused = !GlobalPaused;
            }

            if(SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_UP))
            {
                new_controller->StickAverageY = 1.0f;
                new_controller->is_analog = false;
            }

            if(SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN))
            {
                new_controller->StickAverageY = -1.0f;
                new_controller->is_analog = false;
            }

            if(SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT))
            {
                new_controller->StickAverageX = -1.0f;
                new_controller->is_analog = false;
            }

            if(SDL_GameControllerGetButton(controller_handles[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
            {
                new_controller->StickAverageX = 1.0f;
                new_controller->is_analog = false;
            }

            f32 Threshold = 0.5f;
            sdl_ProcessGameControllerButton(&(old_controller->MoveLeft),
                                           &(new_controller->MoveLeft),
                                           new_controller->StickAverageX < -Threshold);
            sdl_ProcessGameControllerButton(&(old_controller->MoveRight),
                                           &(new_controller->MoveRight),
                                           new_controller->StickAverageX > Threshold);
            sdl_ProcessGameControllerButton(&(old_controller->MoveUp),
                                           &(new_controller->MoveUp),
                                           new_controller->StickAverageY < -Threshold);
            sdl_ProcessGameControllerButton(&(old_controller->MoveDown),
                                           &(new_controller->MoveDown),
                                           new_controller->StickAverageY > Threshold);
        } else {
            // TODO: Controller not plugged in (CLOSE IT!)
        }
    }
    return;
}

internal int
sdl_HandleQuit()
{
    return RETURN_EXIT;
}

internal int
sdl_HandleEvent(struct game_input *input)
{
    int retval = RETURN_SUCCESS;

    /* Easier naming */
    struct game_input *new_input = &input[0];
    struct game_input *old_input = &input[1];

    /* Initialize and rotate keyboard events */
    struct game_controller_input *keyboard_controller = get_controller(new_input,0);
    struct game_controller_input *OldKeyboardController = get_controller(old_input,0);
    memset(keyboard_controller, 0, sizeof(*keyboard_controller));
    for(size_t i = 0; i < ArrayCount(keyboard_controller->Buttons); ++i)
    {
        keyboard_controller->Buttons[i].EndedDown =
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
            case SDL_KEYUP:         retval = sdl_HandleKey(event, keyboard_controller); break;
            case SDL_KEYDOWN:       retval = sdl_HandleKey(event, keyboard_controller); break;
            default: break;
        }
    }
    /* TODO: Use SDL Event pool for Controllers */
    sdl_HandleController(new_input, old_input);
    return retval;
}

internal int
sdl_HandleKey(const SDL_Event event,
              struct game_controller_input *keyboard_controller)
{
    SDL_Keycode key_code = event.key.keysym.sym;
    SDL_Keymod  KeyMod  = event.key.keysym.mod;
    // SDL_Keymod  KeyMod = SDL_GetModState()  // Or this?

    bool is_down = (event.key.state == SDL_PRESSED);
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
        if(key_code == SDLK_w)
        {
            sdl_process_keyboard_event(&keyboard_controller->MoveUp, is_down);
        }
        else if(key_code == SDLK_a)
        {
            sdl_process_keyboard_event(&keyboard_controller->MoveLeft, is_down);
        }
        else if(key_code == SDLK_s)
        {
            sdl_process_keyboard_event(&keyboard_controller->MoveDown, is_down);
        }
        else if(key_code == SDLK_d)
        {
            sdl_process_keyboard_event(&keyboard_controller->MoveRight, is_down);
        }
        else if(key_code == SDLK_q)
        {
            sdl_process_keyboard_event(&keyboard_controller->Lshoulder, is_down);
        }
        else if(key_code == SDLK_e)
        {
            sdl_process_keyboard_event(&keyboard_controller->Rshoulder, is_down);
        }
        else if(key_code == SDLK_UP)
        {
            sdl_process_keyboard_event(&keyboard_controller->ActionUp, is_down);
        }
        else if(key_code == SDLK_DOWN)
        {
            sdl_process_keyboard_event(&keyboard_controller->ActionDown, is_down);
        }
        else if(key_code == SDLK_LEFT)
        {
            sdl_process_keyboard_event(&keyboard_controller->ActionLeft, is_down);
        }
        else if(key_code == SDLK_RIGHT)
        {
            sdl_process_keyboard_event(&keyboard_controller->ActionRight, is_down);
        }
#if DEBUG
        else if(key_code == SDLK_SPACE)
        {
            GlobalPaused = !GlobalPaused;
        }
#endif
        else if(key_code == SDLK_x)
        {
            SDL_Log("Exiting...\n");
            return RETURN_EXIT;
        }
        else if(key_code == SDLK_ESCAPE)
        {
            if(is_down)
            {
                SDL_Log("Escape Is down\n");
            }
            if(WasDown)
            {
                SDL_Log("Escape Was down\n");
            }
        }
        else if((key_code == SDLK_RETURN) && (KeyMod & KMOD_ALT))
        {
            SDL_Log("ALT-ENTER pressed\n");
        }
        else if((key_code == SDLK_F4) && (KeyMod & KMOD_ALT))
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

internal int
sdl_handle_window(const SDL_Event event)
{
    switch(event.window.event)
    {
        case SDL_WINDOWEVENT_MOVED:
        { /* pass */ } break;

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
sdl_SoundGetCursors(struct sdl_audio_ring_buffer *ring_buffer,
                    struct sdl_sound_output *sound_output,
                    struct sdl_performance_counters *perf,
                    struct sdl_audio_counters *audio_perf,
                    u64 *byte_to_lock, u64 *bytes_to_write)
{

    SDL_LockAudioDevice(sound_output->audio_device);
    *byte_to_lock = (sound_output->running_sample_index * sound_output->bytes_per_sample) % sound_output->secondary_buffer_size;

    u64 expected_sound_bytes_per_frame = (sound_output->samples_per_second * sound_output->bytes_per_sample)/ perf->game_update_hz;
    f64 seconds_left_until_flip = perf->target_seconds_per_frame - audio_perf->from_begin_to_audio_seconds;
    u64 expected_bytes_until_flip = (size_t)(( seconds_left_until_flip / perf->target_seconds_per_frame )*(f64)expected_sound_bytes_per_frame);
    u64 expected_frame_boundary_byte = ring_buffer->play_cursor + expected_sound_bytes_per_frame;

    u64 safe_write_cursor = ring_buffer->write_cursor;
    if(safe_write_cursor < ring_buffer->play_cursor)
    {
        safe_write_cursor += sound_output->secondary_buffer_size;
    }

    /* TODO: Assert(safe_write_cursor >= play_cursor); */
    safe_write_cursor += sound_output->safety_bytes;

    bool audio_card_is_low_latency = (safe_write_cursor < expected_frame_boundary_byte);

    u64 target_cursor = 0;
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

    audio_perf->target_cursor = target_cursor;
    audio_perf->expected_frame_boundary_byte = expected_frame_boundary_byte;
}

internal void
sdl_Fillsound_buffer(struct sdl_sound_output *sound_output,
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
sdl_Drawsound_bufferMarker(struct sdl_offscreen_buffer *BackBuffer,
                          struct sdl_sound_output *Sound,
                          f32 C, int PadX, int Top,
                          int Bottom, int Value, u32 Color)
{
    /* TODO: Assert(value < sound -> Secondary buffer size); */
    f32 x32 = C * (f32)Value;
    int X = PadX + (int)x32;
    sdl_DebugDrawVertical(BackBuffer, X, Top, Bottom, Color);
}

internal void
sdl_DebugSyncDisplay(struct sdl_offscreen_buffer *BackBuffer,
                     int MarkerCount,
                     struct sdl_debug_time_marker *Markers,
                     int CurrentMarkerIndex,
                     struct sdl_sound_output *sound_output,
                     struct sdl_performance_counters *perf)
{
    int PadX = 16;
    int PadY = 16;

    int LineHeight = 64;

    // What C stands for??!?!?!?
    f32 C = (f32)(BackBuffer->Width - 2*PadX) / (f32)sound_output->secondary_buffer_size;
    for(int MarkerIndex = 0; MarkerIndex < MarkerCount; ++MarkerIndex)
    {
        struct sdl_debug_time_marker *ThisMarker = Markers + MarkerIndex;
        Assert(ThisMarker->output_play_cursor < sound_output->secondary_buffer_size);
        Assert(ThisMarker->output_write_cursor < sound_output->secondary_buffer_size);
        Assert(ThisMarker->output_location < sound_output->secondary_buffer_size);
        // Assert(ThisMarker->output_byte_count < sound_output->secondary_buffer_size);
        Assert(ThisMarker->flip_play_cursor < sound_output->secondary_buffer_size);
        Assert(ThisMarker->flip_write_cursor < sound_output->secondary_buffer_size);
        u32 PlayColor         = 0xFFFFFFFF;
        u32 WriteColor        = 0xFFFF0000;
        u32 ExpectedFlipColor = 0xFFFFFF00;  /* Yellow */
        u32 PlayWindowColor   = 0xFF00FF00;  /* Green */

        int jitter = 480;
        int Top = PadY;
        int Bottom = PadY + LineHeight;

        if(MarkerIndex == CurrentMarkerIndex)
        {
            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            int FirstTop = Top;

            // Top bar
            sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, Top, Bottom, ThisMarker->output_play_cursor,  PlayColor);
            sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, Top, Bottom, ThisMarker->output_write_cursor, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            // Mid bar
            sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, Top, Bottom, ThisMarker->output_location,  PlayColor);
            sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, Top, Bottom, ThisMarker->output_location + ThisMarker->output_byte_count, WriteColor);

            Top += LineHeight + PadY;
            Bottom += LineHeight + PadY;

            // Large bar
            sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, FirstTop, Bottom, ThisMarker->expected_flip_play_cursor,  ExpectedFlipColor);
        }

        // Top bars...
        sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, Top, Bottom, ThisMarker->flip_play_cursor,  PlayColor);
        sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, Top, Bottom, ThisMarker->flip_play_cursor + jitter*sound_output->bytes_per_sample,  PlayWindowColor);
        sdl_Drawsound_bufferMarker(BackBuffer, sound_output, C, PadX, Top, Bottom, ThisMarker->flip_write_cursor,  WriteColor);
    }
}
#endif

#endif
