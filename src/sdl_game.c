/* Plataform independant code */
#include "game.h"
#include "game.c"

/* Plataform dependent code */
#include "sdl_game.h"

/* Since we are using SDL Textures we still use SDL dependant code */
global_variable struct sdl_offscreen_buffer GlobalBackBuffer;
global_variable struct sdl_audio_ring_buffer AudioRingBuffer;


#if DEBUG
internal struct debug_read_file_result
DEBUGPlataformReadEntireFile(const char *Filename)
{
    struct debug_read_file_result Result = {0};

    int FileHandle = open(Filename, O_RDONLY);
    if(FileHandle == -1)
    {
        return Result;
    }

    struct stat FileStatus;
    if(fstat(FileHandle, &FileStatus) == -1)
    {
        close(FileHandle);
        return Result;
    }
    Result.ContentsSize = SafeTruncateUInt64(FileStatus.st_size);

    Result.Contents = malloc(Result.ContentsSize);
    if(!Result.Contents)
    {
        close(FileHandle);
        Result.ContentsSize = 0;
        return Result;
    }

    u32 BytesToRead = Result.ContentsSize;
    u8 *NextByteLocation = (u8*)Result.Contents;
    while(BytesToRead)
    {
        ssize_t BytesRead = read(FileHandle, NextByteLocation, BytesToRead);
        if(BytesRead == -1)
        {
            free(Result.Contents);
            Result.Contents = 0;
            Result.ContentsSize = 0;
            close(FileHandle);
            return Result;
        }
        BytesToRead -= BytesRead;
        NextByteLocation += BytesToRead;
    }
    close (FileHandle);
    return Result;
}

internal void
DEBUGPlataformFreeFileMemory(void *Memory)
{
    free(Memory);
}

internal bool
DEBUGPlataformWriteEntireFile(const char *Filename, u32 MemorySize, void *Memory)
{
    int FileHandle = open(Filename, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

    if (!FileHandle)
    {
        return false;
    }

    u32 BytesToWrite = MemorySize;
    u8 *NextByteLocation = (u8*)Memory;
    while(BytesToWrite)
    {
        ssize_t BytesWritten = write(FileHandle, NextByteLocation, BytesToWrite);
        if (BytesWritten == -1)
        {
            close(FileHandle);
            return false;
        }
        BytesToWrite -= BytesWritten;
        NextByteLocation +=BytesWritten;
    }
    close(FileHandle);
    return true;
}
#endif

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

internal void
sdl_AudioCallback(void *userdata, u8 *audiodata, int length)
{
    struct sdl_audio_ring_buffer *ringbuffer = (struct sdl_audio_ring_buffer *)userdata;

    int region1size = (int)length;
    int region2size = (int)0;
    /* Not enough space on ring buffer region */
    if ( (ringbuffer->PlayCursor + length) > ringbuffer->Size)
    {
        region1size = ringbuffer->Size - ringbuffer->PlayCursor;
        region2size = length - region1size;
    }

    memcpy(audiodata, (u8*)(ringbuffer->Data) + ringbuffer->PlayCursor, region1size);
    memcpy(audiodata + region1size, ringbuffer->Data, region2size);
    ringbuffer->PlayCursor = (ringbuffer->PlayCursor + length) % ringbuffer->Size;
    ringbuffer->WriteCursor = (ringbuffer->PlayCursor + 2048) % ringbuffer->Size;
}

internal void
sdl_InitAudio(struct sdl_sound_output *sound_output)
{
    SDL_AudioSpec AudioSettings = {0};
    AudioSettings.freq = sound_output->SamplesPerSecond;
    AudioSettings.format = AUDIO_S16LSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = 1024;
    AudioSettings.callback = &sdl_AudioCallback;
    AudioSettings.userdata = &AudioRingBuffer;

    AudioRingBuffer.Size = sound_output->SecondaryBufferSize;
    AudioRingBuffer.PlayCursor = 0;
    AudioRingBuffer.WriteCursor = 0;
    AudioRingBuffer.Data = malloc(sound_output->SecondaryBufferSize);
    if(!AudioRingBuffer.Data)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: malloc(AudioRingBuffer.data)\n", __func__);
        exit(EXIT_FAILURE);
    }

    sound_output->AudioDevice = SDL_OpenAudioDevice(NULL, 0, &AudioSettings, NULL, 0);
    SDL_Log("%s: Audio opened: freq: %d, %d channels, %d bufsz\n",
            __func__, AudioSettings.freq, AudioSettings.channels, AudioSettings.samples);

    if (AudioSettings.format != AUDIO_S16LSB)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_OpenAudio(): Could not get S16LE format\n",
                __func__);
        SDL_CloseAudioDevice(sound_output->AudioDevice);
        sound_output->AudioDevice = 0;
    }
}

internal void
sdl_SoundGetCursors(struct sdl_sound_output *sound_output,
                    int *byte_to_lock,
                    int *bytes_to_write)
{
    SDL_LockAudioDevice(sound_output->AudioDevice);
    *byte_to_lock = ((sound_output->RunningSampleIndex
                     * sound_output->BytesPerSample)
                    % sound_output->SecondaryBufferSize);

    int target_cursor = ((( sound_output->LatencySampleCount
                          * sound_output->BytesPerSample)
                         + AudioRingBuffer.PlayCursor)
                        % sound_output->SecondaryBufferSize);

    if (*byte_to_lock > target_cursor)
        *bytes_to_write = sound_output->SecondaryBufferSize - *byte_to_lock + target_cursor;
    else
        *bytes_to_write = target_cursor - *byte_to_lock;
    SDL_UnlockAudioDevice(sound_output->AudioDevice);
}

internal void
sdl_FillSoundBuffer(struct sdl_sound_output *sound_output,
                    int byte_to_lock,
                    int bytes_to_write,
                    struct game_sound_output_buffer *sound_buffer)
{
    /* Calculate region sizes */
    void *region1 = (u8*)AudioRingBuffer.Data + byte_to_lock;
    int region1_size = bytes_to_write;
    if(region1_size + byte_to_lock > sound_output->SecondaryBufferSize)
        region1_size = sound_output->SecondaryBufferSize - byte_to_lock;

    void *region2 = AudioRingBuffer.Data;
    int region2_size = bytes_to_write - region1_size;

    i16 *samples = sound_buffer->Samples;
    i16 *sample_out = NULL;

    /* Fill region 1 */
    int region1_sample_count = region1_size / sound_output->BytesPerSample;
    sample_out = (i16*)region1;
    for (int sample_index = 0; sample_index < region1_sample_count; ++sample_index)
    {
        *sample_out++ = *samples++;     // L
        *sample_out++ = *samples++;     // R
    }

    /* Fill region 2 */
    int region2_sample_count = region2_size / sound_output->BytesPerSample;
    sample_out = (i16*)region2;
    for (int sample_index = 0; sample_index < region2_sample_count; ++ sample_index)
    {
        *sample_out++ = *samples;     // L
        *sample_out++ = *samples;     // R
    }
}

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
    // TODO: Handle all those "ifs"  with SDL_Asserts.
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
            //SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
            //sdl_ResizeBackBuffer(window, event.window.data1, event.window.data2);
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
        else if(KeyCode == SDLK_SPACE)
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
            // This closes the program
            // return RETURN_EXIT;
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

#if 0
            bool Up    = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_UP);
            bool Down  = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            bool Left  = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            bool Right = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            bool Start = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_START);
            bool Back  = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_BACK);
#endif

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

            NewController->StickAverageX  = sdl_ProcessGameControllerAxisValue (
                    SDL_GameControllerGetAxis (ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTX), 1);

            NewController->StickAverageY  = sdl_ProcessGameControllerAxisValue (
                    SDL_GameControllerGetAxis (ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTY), 1);

            /* Possible branchless with sum and or */
            if((NewController->StickAverageX != 0.0f) || (NewController->StickAverageY != 0.0f))
            {
                NewController->IsAnalog = true;
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
    // TODO: If mmap need to munmap
    //if (GlobalBackBuffer.memory)    free(GlobalBackBuffer.memory);
    munmap(GlobalBackBuffer.Memory, GlobalBackBuffer.Width * GlobalBackBuffer.Height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));
    if (GlobalBackBuffer.Texture)   SDL_DestroyTexture(GlobalBackBuffer.Texture);
    if (renderer)                   SDL_DestroyRenderer(renderer);
    if (window)                     SDL_DestroyWindow(window);
#if 0
    if (AudioRingBuffer.data)            free(AudioRingBuffer.data);
#endif
    SDL_Quit();
    return;
}

/* TODO BUG HERE: u64 to f32 */
internal f64
sdl_GetSecondsElapsed(u64 OldCounter, u64 CurrentCounter, u64 PerfCountFrequency)
{
    return ((f64)(CurrentCounter - OldCounter) / (f64)PerfCountFrequency);
}


int
main (void)
{
    int exitval = EXIT_FAILURE;
    int retval = RETURN_SUCCESS;

    /*
     * Game Window Initialization
     */
    SDL_Window *window = sdl_InitGame(640, 480);
    if(!window) goto __EXIT__;

    SDL_Log("Refresh rate is %d Hz\n", sdl_GetWindowRefreshRate(window));
    int GameUpdateHz = 30;
    f64 TargetSecondsPerFrame = 1.0f / (f64)GameUpdateHz;

    struct sdl_window_dimension wdim = sdl_WindowGetDimension(window);
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
    sound_output.AudioDevice = 0;
    sound_output.SamplesPerSecond = 48000;
    sound_output.RunningSampleIndex = 0;
    sound_output.BytesPerSample = sizeof(i16) * 2;
    sound_output.SecondaryBufferSize = sound_output.SamplesPerSecond * sound_output.BytesPerSample;
    sound_output.LatencySampleCount = sound_output.SamplesPerSecond / 15;

    /* Initialize sound paused*/
    sdl_InitAudio(&sound_output);
    i16 *samples = calloc(sound_output.SamplesPerSecond, sound_output.BytesPerSample);
    SDL_PauseAudioDevice(sound_output.AudioDevice, 0);

    /*
     * Game Memory Initialization
     */
#if GAME_INTERNAL
    void *base_address = (void*)Terabytes(2);
#else
    void *base_address = (void*)(0);
#endif

    struct game_memory game_memory = {0};
    game_memory.PersistentStorageSize = Megabytes(64);
    game_memory.TransientStorageSize = Gigabytes(2);
    size_t total_storage_size = game_memory.PersistentStorageSize + game_memory.TransientStorageSize;
    game_memory.PersistentStorage = mmap(base_address, total_storage_size,
                                          PROT_READ | PROT_WRITE,
                                          MAP_ANONYMOUS | MAP_PRIVATE,
                                          -1, 0);

    /* TODO: SDL_Assert with break for debug chekcing game memory allocation*/

    /*
     * Game Performance initialization
     */
    u64 LastCounter = SDL_GetPerformanceCounter();
    u64 LastCycleCount = _rdtsc();
    u64 PerfCountFrequency = SDL_GetPerformanceFrequency();

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

        /* Prepare render buffers */
        int byte_to_lock  = 0;
        int bytes_to_write = 0;
        sdl_SoundGetCursors(&sound_output, &byte_to_lock, &bytes_to_write);

        struct game_sound_output_buffer sound_buffer = {0};
        sound_buffer.SamplesPerSecond = sound_output.SamplesPerSecond;
        sound_buffer.SampleCount = bytes_to_write / sound_output.BytesPerSample;
        sound_buffer.Samples = samples;

        struct game_offscreen_buffer screen_buffer = {0};
        screen_buffer.Memory = GlobalBackBuffer.Memory;
        screen_buffer.Width = GlobalBackBuffer.Width;
        screen_buffer.Height = GlobalBackBuffer.Height;
        screen_buffer.Pitch = GlobalBackBuffer.Pitch;

        /* Here we fill Screen and Sound buffers */
        GameUpdateAndRender(&game_memory, new_input, &screen_buffer, &sound_buffer);

        /* Swap Inputs */
        struct game_input *temp_input = new_input;
        new_input = old_input;
        old_input = temp_input;

        /* "Blit" Video and Audio*/
        sdl_FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);

        if(sdl_GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter(), PerfCountFrequency) < TargetSecondsPerFrame)
        {
            i32 TimeToSleep = ((TargetSecondsPerFrame - sdl_GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter(), PerfCountFrequency)) * 1000) - 1;
            if (TimeToSleep > 0)
            {
                SDL_Delay(TimeToSleep);
            }
            // Assert(SDLGetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter()) < TargetSecondsPerFrame)
            while (sdl_GetSecondsElapsed(LastCounter, SDL_GetPerformanceCounter(), PerfCountFrequency) < TargetSecondsPerFrame)
            {
                // Waiting...
            }
        }

        /* Vefore Update Window to no skip vblanks */
        u64 EndCounter = SDL_GetPerformanceCounter();

        sdl_UpdateWindow(window, &GlobalBackBuffer);

        /* Do the rendering math */
        u64 EndCycleCount = _rdtsc();
        u64 CounterElapsed = EndCounter - LastCounter;
        u64 CyclesElapsed = EndCycleCount - LastCycleCount;
        f64 MSPerFrame = 1000.0 * (f64)CounterElapsed / (f64)PerfCountFrequency;
        f64 FPS = (f64)PerfCountFrequency / (f64)CounterElapsed;
        f64 MCPF = (f64)CyclesElapsed / 1000000.0;

        fprintf(stdout, "%6.02fms/f, %6.02ff/s %6.02fmc/f\r", MSPerFrame, FPS, MCPF);

        LastCycleCount = EndCycleCount;
        LastCounter = EndCounter;
    }

    exitval = EXIT_SUCCESS;
__EXIT__:
    if (sound_output.AudioDevice > 0) {
        free(samples);
        SDL_CloseAudioDevice(sound_output.AudioDevice);
    }
    sdl_ExitGame(window);
    return exitval;
}

