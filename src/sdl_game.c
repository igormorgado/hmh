/* Plataform independant code */
#include "game.h"
#include "game.c"

/* Plataform dependent code */
#include "sdl_game.h"

/* Since we are using SDL Textures we still use SDL dependant code */
global_variable struct sdl_offscreen_buffer GlobalBackBuffer;
global_variable struct sdl_audio_ring_buffer AudioRingBuffer;
#if 0
global_variable struct sdl_sound_output global_sound_output;
global_variable SDL_AudioDeviceID adev;
#endif

internal void
sdl_AudioCallback(void *userdata, u8 *audiodata, i32 length)
{
    struct sdl_audio_ring_buffer *ringbuffer = (struct sdl_audio_ring_buffer *)userdata;

    i32 region1size = (i32)length;
    i32 region2size = (i32)0;
    /* Not enough space on ring buffer region */
    if ( (ringbuffer->play_cursor + length) > ringbuffer->size)
    {
        region1size = ringbuffer->size - ringbuffer->play_cursor;
        region2size = length - region1size;
    }

    memcpy(audiodata, (u8*)(ringbuffer->data) + ringbuffer->play_cursor, region1size);
    memcpy(audiodata + region1size, ringbuffer->data, region2size);
    ringbuffer->play_cursor = (ringbuffer->play_cursor + length) % ringbuffer->size;
    ringbuffer->write_cursor = (ringbuffer->play_cursor + 2048) % ringbuffer->size;
}

internal void
sdl_InitAudio(struct sdl_sound_output *sound_output)
{
    SDL_AudioSpec AudioSettings = {0};
    AudioSettings.freq = sound_output->samples_per_second;
    AudioSettings.format = AUDIO_S16LSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = 1024;
    AudioSettings.callback = &sdl_AudioCallback;
    AudioSettings.userdata = &AudioRingBuffer;

    AudioRingBuffer.size = sound_output->secondary_buffer_size;
    AudioRingBuffer.play_cursor = 0;
    AudioRingBuffer.write_cursor = 0;
    AudioRingBuffer.data = malloc(sound_output->secondary_buffer_size);
    if(!AudioRingBuffer.data)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: malloc(AudioRingBuffer.data)\n", __func__);
        exit(EXIT_FAILURE);
    }

    sound_output->device = SDL_OpenAudioDevice(NULL, 0, &AudioSettings, NULL, 0);
    SDL_Log("%s: Audio opened: freq: %d, %d channels, %d bufsz\n",
            __func__, AudioSettings.freq, AudioSettings.channels, AudioSettings.samples);

    if (AudioSettings.format != AUDIO_S16LSB)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_OpenAudio(): Could not get S16LE format\n",
                __func__);
        SDL_CloseAudioDevice(sound_output->device);
        sound_output->device = 0;
    }
}

internal void
sdl_SoundGetCursors(struct sdl_sound_output *sound_output,
                    i32 *byte_to_lock,
                    i32 *bytes_to_write)
{
    SDL_LockAudioDevice(sound_output->device);
    *byte_to_lock = ((sound_output->running_sample_index
                     * sound_output->bytes_per_sample)
                    % sound_output->secondary_buffer_size);

    i32 target_cursor = ((( sound_output->latency_sample_count
                          * sound_output->bytes_per_sample)
                         + AudioRingBuffer.play_cursor)
                        % sound_output->secondary_buffer_size);

    if (*byte_to_lock > target_cursor)
        *bytes_to_write = sound_output->secondary_buffer_size - *byte_to_lock + target_cursor;
    else
        *bytes_to_write = target_cursor - *byte_to_lock;
    SDL_UnlockAudioDevice(sound_output->device);
}

internal void
sdl_FillSoundBuffer(struct sdl_sound_output *sound_output,
                    i32 byte_to_lock,
                    i32 bytes_to_write,
                    struct game_sound_output_buffer *sound_buffer)
{
    /* Calculate region sizes */
    void *region1 = (u8*)AudioRingBuffer.data + byte_to_lock;
    i32 region1_size = bytes_to_write;
    if(region1_size + byte_to_lock > sound_output->secondary_buffer_size)
        region1_size = sound_output->secondary_buffer_size - byte_to_lock;

    void *region2 = AudioRingBuffer.data;
    i32 region2_size = bytes_to_write - region1_size;

    i16 *samples = sound_buffer->samples;
    i16 *sample_out = NULL;

    /* Fill region 1 */
    i32 region1_sample_count = region1_size / sound_output->bytes_per_sample;
    sample_out = (i16*)region1;
    for (i32 sample_index = 0; sample_index < region1_sample_count; ++sample_index)
    {
        *sample_out++ = *samples++;     // L
        *sample_out++ = *samples++;     // R
    }

    /* Fill region 2 */
    i32 region2_sample_count = region2_size / sound_output->bytes_per_sample;
    sample_out = (i16*)region2;
    for (i32 sample_index = 0; sample_index < region2_sample_count; ++ sample_index)
    {
        *sample_out++ = *samples;     // L
        *sample_out++ = *samples;     // R
    }
}

internal struct sdl_window_dimension
sdl_WindowGetDimension(SDL_Window *window)
{
    struct sdl_window_dimension result;
    SDL_GetWindowSize(window, &result.width, &result.height);
    return result;
}

/* NOTE: Name in hmh is SDLResizeTexture */
internal int
sdl_ResizeBackBuffer(struct sdl_offscreen_buffer *screenbuffer, SDL_Window *window, u16 width, u16 height)
{
    // TODO: ASSERT WINDOW not null
    // TODO: ASSERT 0 < width/height <= 4096

    int bytes_per_pixel = SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888);
    if(screenbuffer->memory)
    {
        // free(screenbuffer->memory);
        munmap(screenbuffer->memory, screenbuffer->width * screenbuffer->height * bytes_per_pixel);
    }

    if(screenbuffer->texture)
    {
        SDL_DestroyTexture(screenbuffer->texture);
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    screenbuffer->texture = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        width,
                                        height);

    if(!screenbuffer->texture)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: SDL_CreateTexture(GlobalBackBuffer): %s\n",
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
    screenbuffer->width = width;
    screenbuffer->height = height;
    screenbuffer->pitch = width * bytes_per_pixel;
    // screenbuffer->memory = malloc(screenbuffer->width * screenbuffer->height * bytes_per_pixel);
    screenbuffer->memory = mmap(NULL,
                        screenbuffer->width * screenbuffer->height * bytes_per_pixel,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if(!screenbuffer->memory)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: malloc(screenbuffer->memory): %s\n",
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
    i32 retval = RETURN_FAILURE;
    if(!screenbuffer->texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: GlobalBackBuffer Texture NULL, nothing to update\n",
                __func__);
        return retval;
    }

    retval = SDL_UpdateTexture(screenbuffer->texture,
                               NULL,
                               screenbuffer->memory,
                               screenbuffer->pitch);

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

    retval = SDL_RenderCopy(renderer, screenbuffer->texture, NULL, NULL);
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

internal i32
sdl_HandleQuit()
{
    return RETURN_EXIT;
}

internal i32
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

internal i32
sdl_HandleKey(const SDL_Event event)
{
    SDL_Keycode KeyCode = event.key.keysym.sym;
    SDL_Keymod  KeyMod  = event.key.keysym.mod;
    // Should I use instead?
    // SDL_Keymod  KeyMod = SDL_GetModState()

    bool isDown = (event.key.state == SDL_PRESSED);
    bool wasDown = false;
    if (event.key.state == SDL_RELEASED)
    {
        wasDown = true;
    }
    else if (event.key.repeat != 0)
    {
        wasDown = true;
    }

    if (event.key.repeat == 0)
    {
        if(KeyCode == SDLK_w)
        {
        }
        else if(KeyCode == SDLK_a)
        {
        }
        else if(KeyCode == SDLK_s)
        {
        }
        else if(KeyCode == SDLK_d)
        {
        }
        else if(KeyCode == SDLK_q)
        {
            return RETURN_EXIT;
        }
        else if(KeyCode == SDLK_e)
        {
        }
        else if(KeyCode == SDLK_UP)
        {
        }
        else if(KeyCode == SDLK_DOWN)
        {
        }
        else if(KeyCode == SDLK_LEFT)
        {
        }
        else if(KeyCode == SDLK_RIGHT)
        {
        }
        else if(KeyCode == SDLK_SPACE)
        {
        }
        else if(KeyCode == SDLK_ESCAPE)
        {
            if(isDown)
            {
                SDL_Log("Escape Is down\n");
            }
            if(wasDown)
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

internal i32
sdl_HandleEvent(const SDL_Event event)
{
    // TODO: Assert eevent not null
    i32 retval = RETURN_SUCCESS;
    switch(event.type)
    {
        case SDL_QUIT:          retval = sdl_HandleQuit(); break;
        case SDL_WINDOWEVENT:   retval = sdl_HandleWindow(event); break;
        case SDL_KEYUP:         retval = sdl_HandleKey(event); break;
        case SDL_KEYDOWN:       retval = sdl_HandleKey(event); break;
        default: break;
    }
    return retval;
}


/* Original name: SDLOpenGameControllers() */
internal i32
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

internal void
sdl_ProcessGameControllerButton(struct game_button_state *old_state,
                                struct game_button_state *new_state,
                                SDL_GameController *controller_handle,
                                SDL_GameControllerButton button)
{
    new_state->ended_down = SDL_GameControllerGetButton(controller_handle, button);
    new_state->half_transient_count += ((new_state->ended_down == old_state->ended_down) ? 0:1);
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


void
sdl_HandleController(struct game_input *new_input, struct game_input *old_input)
{
    for(int i = 0; i< MAX_CONTROLLERS; i++)
    {
        if(ControllerHandles[i] != NULL && SDL_GameControllerGetAttached(ControllerHandles[i]))
        {
            struct game_controller_input *old_controller = &old_input->controllers[i];
            struct game_controller_input *new_controller = &new_input->controllers[i];

            new_controller->is_analog = true;

            sdl_ProcessGameControllerButton(&(old_controller->lshoulder),
                                            &(new_controller->lshoulder),
                                            ControllerHandles[i],
                                            SDL_CONTROLLER_BUTTON_LEFTSHOULDER);

            sdl_ProcessGameControllerButton(&(old_controller->rshoulder),
                                            &(new_controller->rshoulder),
                                            ControllerHandles[i],
                                            SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

            sdl_ProcessGameControllerButton(&(old_controller->down),
                                            &(new_controller->down),
                                            ControllerHandles[i],
                                            SDL_CONTROLLER_BUTTON_A);

            sdl_ProcessGameControllerButton(&(old_controller->right),
                                            &(new_controller->right),
                                            ControllerHandles[i],
                                            SDL_CONTROLLER_BUTTON_B);

            sdl_ProcessGameControllerButton(&(old_controller->left),
                                            &(new_controller->left),
                                            ControllerHandles[i],
                                            SDL_CONTROLLER_BUTTON_X);

            sdl_ProcessGameControllerButton(&(old_controller->up),
                                            &(new_controller->up),
                                            ControllerHandles[i],
                                            SDL_CONTROLLER_BUTTON_Y);

            i16 lstickX   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTX);
            i16 lstickY   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTY);

            if (lstickX < 0)
            {
                new_controller->end_x = lstickX / -32768.0f;
            }
            else
            {
                new_controller->end_x = lstickX / -32767.0f;
            }

            new_controller->min_x = new_controller->max_x = new_controller->end_x;

            if (lstickY < 0)
            {
                new_controller->end_y = lstickY / -32768.0f;
            }
            else
            {
                new_controller->end_y = lstickY / -32767.0f;
            }

            new_controller->min_y = new_controller->max_y = new_controller->end_y;

#if 0
            // rstickX   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_RIGHTX);
            rstickY   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_RIGHTY);

            if(start == true)
            {
                SDL_Log("Tone %.2f  wavePeriod %f\n", global_sound_output.toneHz, global_sound_output.wave_period);
            }

            global_sound_output.toneHz = 512.0 + 256.0f * (f32)-rstickY/20000.;
            global_sound_output.wave_period = (f32)global_sound_output.samples_per_second / global_sound_output.toneHz;

            if(up == true)
            {
                global_sound_output.amplitude +=100;
                global_sound_output.amplitude = MININT(32267, global_sound_output.amplitude);
                SDL_Log("global_sound_output.amplitude %.0f\n", global_sound_output.amplitude);
            }
            if(down == true)
            {
                global_sound_output.amplitude-=100;
                global_sound_output.amplitude = MAXINT(0, global_sound_output.amplitude);
                SDL_Log("global_sound_output.amplitude %.0f\n", global_sound_output.amplitude);
            }

            if (buttonA == true)
            {
                speed *= 2.f;
            } else if (buttonB == true) {
                speed *= .5f;
            }

            if (buttonX == true)
            {
                red -= 1;
                SDL_Log("RED-\n");
            }
            if (buttonY == true) {
                red += 1;
                SDL_Log("RED+\n");
            }

            XOffset -= lstickX/10000.f * speed;
            YOffset -= lstickY/10000.f * speed;

            // RUMBLERUMBLE
            //
            if(rshoulder && RumbleHandles[i] != NULL)
            {
                SDL_Log("RUMBLE ON!!\n");
                retval = SDL_HapticRumblePlay(RumbleHandles[i], 0.5f, 2000);

                if(retval < 0) {
                    SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                            "%s: SDL_HapticRumblePlay(%d): %s\n",
                            __func__, i, SDL_GetError());
                }
            }

            if(!rshoulder) {
                SDL_HapticRumbleStop(RumbleHandles[i]);
            }
#endif
        } else {
            // TODO: Controller not plugged in (CLOSE IT!)
        }

    }
    return;
}


internal SDL_Window *
sdl_InitGame(u16 screen_width, u16 screen_height)
{
    // TODO: Assert Window not null
    // TODO: Assert widht/height < 4096
    i32 retval;
    i32 sdl_flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_AUDIO;
    i32 wnd_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    i32 rnd_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;

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
    munmap(GlobalBackBuffer.memory, GlobalBackBuffer.width * GlobalBackBuffer.height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));
    if (GlobalBackBuffer.texture)   SDL_DestroyTexture(GlobalBackBuffer.texture);
    if (renderer)                   SDL_DestroyRenderer(renderer);
    if (window)                     SDL_DestroyWindow(window);
#if 0
    if (AudioRingBuffer.data)            free(AudioRingBuffer.data);
#endif
    SDL_Quit();
    return;
}


int
main (void)
{
    i32 exitval = EXIT_FAILURE;
    i32 retval = RETURN_SUCCESS;

    SDL_Window *window = sdl_InitGame(640, 480);
    if(!window) goto __EXIT__;
    u64 perf_count_frequency = SDL_GetPerformanceFrequency();

    struct sdl_window_dimension wdim = sdl_WindowGetDimension(window);
    sdl_ResizeBackBuffer(&GlobalBackBuffer, window, wdim.width, wdim.height);

    struct game_input input[2] = {};
//    struct game_input *new_input = input;
//    struct game_input *old_input = input + 1;
    struct game_input *new_input = &input[0];
    struct game_input *old_input = &input[1];

    struct sdl_sound_output sound_output = {};
    /* TODO: Fetch sound device from a list */
    sound_output.device = 0;
    sound_output.samples_per_second = 48000;
    sound_output.running_sample_index = 0;
    sound_output.bytes_per_sample = sizeof(i16) * 2;
    sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;
    sound_output.latency_sample_count = sound_output.samples_per_second / 15;

    /* Initialize sound paused*/
    sdl_InitAudio(&sound_output);
    i16 *samples = calloc(sound_output.samples_per_second, sound_output.bytes_per_sample);
    SDL_PauseAudioDevice(sound_output.device, 0);

    u64 last_counter = SDL_GetPerformanceCounter();
    u64 last_cycle_count = _rdtsc();

    bool running = true;
    while(running == true)
    {
        /* Handle Input */
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            /* TODO: PUT ON IF CLAUSE ? */
            retval = sdl_HandleEvent(event);
            if(retval == RETURN_EXIT)
                running = false;
        }
        sdl_HandleController(new_input, old_input);

        /* Prepare render buffers */
        i32 byte_to_lock  = 0;
        i32 bytes_to_write = 0;
        sdl_SoundGetCursors(&sound_output, &byte_to_lock, &bytes_to_write);

        struct game_sound_output_buffer sound_buffer = {0};
        sound_buffer.samples_per_second = sound_output.samples_per_second;
        sound_buffer.sample_count = bytes_to_write / sound_output.bytes_per_sample;
        sound_buffer.samples = samples;

        struct game_offscreen_buffer screen_buffer = {0};
        screen_buffer.memory = GlobalBackBuffer.memory;
        screen_buffer.width = GlobalBackBuffer.width;
        screen_buffer.height = GlobalBackBuffer.height;
        screen_buffer.pitch = GlobalBackBuffer.pitch;

        /* Here we fill Screen and Sound buffers */
        GameUpdateAndRender(new_input, &screen_buffer, &sound_buffer);

        struct game_input *temp_input = new_input;
        new_input = old_input;
        old_input = temp_input;

        /* "Blit" Video and Audio*/
        sdl_FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
        sdl_UpdateWindow(window, &GlobalBackBuffer);

        /* Do the rendering math */
        u64 end_cycle_count = _rdtsc();
        u64 end_counter = SDL_GetPerformanceCounter();
        u64 counter_elapsed = end_counter - last_counter;
        u64 cycles_elapsed = end_cycle_count - last_cycle_count;
        f64 ms_per_frame = 1000.0 * (f64)counter_elapsed / (f64)perf_count_frequency;
        f64 fps = (f64)perf_count_frequency / (f64)counter_elapsed;
        f64 mcpf = (f64)cycles_elapsed / 1000000.0;

        fprintf(stdout, "%6.02fms/f, %6.02ff/s %6.02fmc/f\r", ms_per_frame, fps, mcpf);

        last_cycle_count = end_cycle_count;
        last_counter = end_counter;
    }

    exitval = EXIT_SUCCESS;
__EXIT__:
    if (sound_output.device > 0)
        SDL_CloseAudioDevice(sound_output.device);
    sdl_ExitGame(window);
    return exitval;
}

