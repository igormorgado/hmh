/*
 *  TODO:  THIS IS NOT A FINAL PLATFORM LAYER!!!
 *
 *  - Make the right calls so Windows doesn't think we're "still loading" for a bit after we actually start
 *  - Saved game locations
 *  - Getting a handle to our own executable file
 *  - Asset loading path
 *  - Threading (launch a thread)
 *  - Raw Input (support for multiple keyboards)
 *  - ClipCursor() (for multimonitor support)
 *  - QueryCancelAutoplay
 *  - WM_ACTIVATEAPP (for when we are not the active application)
 *  - Blit speed improvements (BitBlt)
 *  - Hardware acceleration (OpenGL or Direct3D or BOTH??)
 *  - GetKeyboardLayout (for French keyboards, international WASD support)
 *  - ChangeDisplaySettings option if we detect slow fullscreen blit??
 *
 *   Just a partial list of stuff!!
*/


#include "game_platform.h"

#if 0  /* Not anymore?:Plataform independant header/code */
#include "game.h"
#endif

/* Plataform dependent header */
#include "sdl_game.h"

/* NOTE(Igor) Isn't this better on sdl_game.h ? */
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif


global_variable bool global_running;
global_variable bool global_pause;
global_variable bool debug_global_show_cursor;

global_variable struct sdl_offscreen_buffer global_back_buffer;
global_variable struct sdl_performance_counters global_performance_counters;
global_variable struct sdl_audio_counters global_audio_counters;

global_variable SDL_GameController *controller_handles[MAX_CONTROLLERS];
global_variable SDL_Haptic *rumble_handles[MAX_CONTROLLERS];


internal void
cat_strings(size_t src_a_sz, char *src_a,
            size_t src_b_sz, char *src_b,
            size_t dst_sz,   char *dst)
{
    for (size_t i = 0; i < src_a_sz; ++i)
        *dst++ = *src_a++;

    for (size_t i = 0; i < src_b_sz; ++i)
        *dst++ = *src_b++;

    *dst++ = 0;
}

/* TODO(Igor): Find a non linux only way to do it */
internal void
sdl_get_exe_filename(struct sdl_state *state)
{
    memset(state->exe_filename, 0, sizeof(state->exe_filename));

    /* TODO(Igor): Why not get from argv[0]? */
    ssize_t size_of_filename = readlink ("/proc/self/exe", state->exe_filename, sizeof (state->exe_filename) - 1);
    state->exe_basename = state->exe_filename;
    for(char *scan = state->exe_filename; *scan; ++scan)
    {
        if(*scan == '/')
        {
            state->exe_basename = scan + 1;
        }
    }
}

internal int
string_length(char *string)
{
    int count = 0;
    while(*string++) count++;
    return (count);
}

internal void
sdl_build_exe_path_filename(struct sdl_state *state,
                            char *filename,
                            char *dst,
                            int dst_sz)
{
    /* Note(Igor): Hackish stuff: char a[] = "..."; char *b = &a[X]; ==> b - a = X */
    cat_strings(state->exe_basename - state->exe_filename, state->exe_filename,
                string_length(filename), filename,
                dst_sz, dst);
}

#if GAME_INTERNAL
void
debug_plataform_free_file_memory(void *memory)
{
    if(memory)
    {
        free(memory);
    }
}

struct debug_read_file_result
debug_plataform_read_entire_file(const char *filename)
{
    struct debug_read_file_result result = {0};

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

    result.contents = malloc(result.contents_size);
    if(!result.contents)
    {
        close(file_handle);
        result.contents_size = 0;
        return result;
    }

    size_t bytes_to_read = result.contents_size;
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

bool
debug_plataform_write_entire_file(const char *filename, size_t memory_size, void *memory)
{
    int file_handle = open(filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

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


/* Note(Igor): This function should be name get_filename_write_time */
inline time_t
sdl_get_last_write_time(char *filename)
{
    time_t last_write_time = 0;
    struct stat file_status;
    if (stat (filename, &file_status) == 0)
    {
        last_write_time = file_status.st_mtime;
    }
    return last_write_time;
}

internal struct sdl_game_code
sdl_load_game_code(char *source_dll_name)
{
    struct sdl_game_code result = {0};

    result.dll_last_write_time = sdl_get_last_write_time (source_dll_name);

    if (result.dll_last_write_time)
    {
        SDL_Log("%s: %s reloading\n", __FILE__, source_dll_name);
        /* TODO(Igor): Use SDL dynamic loading */
        result.game_code_dll = dlopen(source_dll_name, RTLD_LAZY);
        if (result.game_code_dll)
        {
#if !defined(CASEYDEFS)
            result.update_and_render = (game_update_and_render *)
                dlsym (result.game_code_dll, "game_update_and_render_f");
            if(!result.update_and_render)
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "%s: %s\n",
                        __func__,
                        dlerror());
            }

            result.get_sound_samples = (game_get_sound_samples *)
                dlsym (result.game_code_dll, "game_get_sound_samples_f");
            if(!result.update_and_render)
            {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                        "%s: %s\n",
                        __func__,
                        dlerror());
            }
#else
            result.update_and_render = (game_update_and_render *)
                dlsym (result.game_code_dll, "GameUpdateAndRender");

            result.get_sound_samples = (game_get_sound_samples *)
                dlsym (result.game_code_dll, "GameGetSoundSamples");
#endif

            result.is_valid = (result.update_and_render &&
                    result.get_sound_samples);

        }
        else
        {
            /* TODO(Igor): Use SDL_LogError */
        }
    }

    if(result.is_valid)
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "%s: %s RELOADED!\n",
                __FILE__, source_dll_name);
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: Failed to load some libraries. Disabling them.\n",
                __func__);
        result.update_and_render = 0;
        result.get_sound_samples = 0;
    }
    return (result);
}

internal void
sdl_unload_game_code(struct sdl_game_code *game_code)
{
    if (game_code->game_code_dll)
    {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                "%s: unloading game code!\n",
                __FILE__);
        dlclose(game_code->game_code_dll);
        game_code->game_code_dll = 0;
    }
    game_code->is_valid = false;
    game_code->update_and_render = 0;
    game_code->get_sound_samples = 0;
}

/* Original name: SDLOpenGameControllers() */
internal void
sdl_game_controllers_open()
{
    int njoys = SDL_NumJoysticks();
    int controller_index = 0;
    for (int joystick_index = 0; joystick_index < njoys; ++joystick_index)
    {
        if (!SDL_IsGameController(joystick_index))
        {
            continue;
        }
        if(controller_index >= MAX_CONTROLLERS)
        {
            break;
        }
        controller_handles[controller_index] = SDL_GameControllerOpen (joystick_index);
        SDL_Joystick *joystick_handle = SDL_GameControllerGetJoystick (controller_handles[controller_index]);
        rumble_handles[controller_index] = SDL_HapticOpenFromJoystick (joystick_handle);
        if (SDL_HapticRumbleInit(rumble_handles[controller_index]) != 0)
        {
            SDL_HapticClose(rumble_handles[controller_index]);
            rumble_handles[controller_index] = NULL;
        }
        controller_index++;
    }
}


/* Original: SDLcloseGameControllers */
internal void
sdl_game_controllers_close()
{
    for (int controller_index = 0; controller_index < MAX_CONTROLLERS; controller_index++)
    {
        SDL_Log("%s: %d Controller @%p \t Rumble @%p\n", __func__, controller_index, (void*)controller_handles[controller_index], (void*)rumble_handles[controller_index]);
        if (controller_handles[controller_index])
        {
            SDL_Log("%s: Closing Controller %d @%p\n", __func__, controller_index, (void*)controller_handles[controller_index]);
            SDL_GameControllerClose(controller_handles[controller_index]);
            controller_handles[controller_index] = NULL;
        }
        if (rumble_handles[controller_index] != NULL)
        {
            SDL_Log("%s: Closing Haptic %d @%p\n", __func__, controller_index, (void*)rumble_handles[controller_index]);
            SDL_HapticClose(rumble_handles[controller_index]);
            rumble_handles[controller_index] = NULL;
        }
    }
}


internal void
sdl_clear_sound_buffer(struct sdl_sound_output *sound_output)
{
    SDL_ClearQueuedAudio(sound_output->audio_device);
}


internal void
sdl_init_audio(struct sdl_sound_output *sound_output)
{
    SDL_AudioSpec audio_settings = {0};

    audio_settings.freq = sound_output->samples_per_second;
    audio_settings.format = AUDIO_S16LSB;
    audio_settings.channels = 2;
    audio_settings.samples = 512;

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

    sdl_clear_sound_buffer(sound_output);

    SDL_PauseAudioDevice(sound_output->audio_device, SDL_FALSE);
}


internal struct sdl_window_dimension
sdl_window_get_dimension(SDL_Window *window)
{
    struct sdl_window_dimension result;
    SDL_GetWindowSize(window, &result.width, &result.height);
    return result;
}


internal int
sdl_resize_texture(struct sdl_offscreen_buffer *screen_buffer, SDL_Window *window, int width, int height)
{
    // TODO(casey): Bulletproof this.
    // Maybe don't free first, free after, then free first if that fails.

    if(screen_buffer->memory)
    {
        munmap (screen_buffer->memory, screen_buffer->width * screen_buffer->height * screen_buffer->bytes_per_pixel);
    }


    if(screen_buffer->texture)
    {
        SDL_DestroyTexture(screen_buffer->texture);
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    screen_buffer->texture = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        width,
                                        height);

    if(!screen_buffer->texture)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: SDL_CreateTexture(screen_buffer): %s\n",
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }

    int bytes_per_pixel = SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888);

    screen_buffer->width = width;
    screen_buffer->height = height;
    screen_buffer->bytes_per_pixel = bytes_per_pixel;
    screen_buffer->pitch = Align16(width * bytes_per_pixel);
    screen_buffer->memory = mmap(NULL,
                                screen_buffer->width * screen_buffer->height * screen_buffer->bytes_per_pixel,
                                PROT_READ | PROT_WRITE,
                                MAP_PRIVATE | MAP_ANONYMOUS,
                                -1,
                                0);
    if(!screen_buffer->memory)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: malloc(screen_buffer->Memory): %s\n",
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }

    /* TODO: Clear screen to black */
    return RETURN_SUCCESS;
}


internal int
sdl_display_buffer_in_window(struct sdl_offscreen_buffer *screen_buffer,
                             SDL_Window *window,
                             int window_width,
                             int window_height)
{
    int retval = RETURN_FAILURE;
    if(!screen_buffer->texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: GlobalBackBuffer Texture NULL, nothing to update\n",
                __func__);
        return retval;
    }

    retval = SDL_UpdateTexture(screen_buffer->texture,
                               NULL,
                               screen_buffer->memory,
                               screen_buffer->pitch);
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

    if ((window_width >= screen_buffer->width * 2) && (window_height >= screen_buffer->height * 2))
    {
        SDL_Rect src_rect = {0, 0, screen_buffer->width, screen_buffer->height};
        SDL_Rect dst_rect = {0, 0, 2 * screen_buffer->width, 2 * screen_buffer->height};
        retval = SDL_RenderCopy(renderer, screen_buffer->texture, &src_rect, &dst_rect);
    }
    else
    {
        int offset_x = 0;
        int offset_y = 0;
        SDL_Rect src_rect = {0, 0, screen_buffer->width, screen_buffer->height};
        SDL_Rect dst_rect = {offset_x, offset_y, screen_buffer->width, screen_buffer->height};
        retval = SDL_RenderCopy(renderer, screen_buffer->texture, &src_rect, &dst_rect);
    }

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

internal void
sdl_fill_sound_buffer(struct sdl_sound_output *sound_output,
                      struct game_sound_output_buffer *sound_buffer,
                      u32 bytes_to_write)
{
    SDL_QueueAudio(sound_output->audio_device, sound_buffer->samples, bytes_to_write);
}


internal void
sdl_process_keyboard_event(struct game_button_state *new_state, bool is_down)
{
    new_state->ended_down = is_down;
    ++new_state->half_transition_count;
}


internal void
sdl_process_game_controller_button(struct game_button_state *old_state,
                                   struct game_button_state *new_state,
                                   bool value)
{
    new_state->ended_down = value;
    new_state->half_transition_count += ((new_state->ended_down == old_state->ended_down) ? 0 : 1);
}


internal f32
sdl_process_game_controller_axis_values(i16 value, i16 dead_zone_threshold)
{
    f32 result = 0;
    if (value < -dead_zone_threshold)
    {
        result = (f32)((value + dead_zone_threshold) / (32768.0f - dead_zone_threshold));
    }
    else if (value > dead_zone_threshold)
    {
        result = (f32)((value - dead_zone_threshold) / (32767.0f - dead_zone_threshold));
    }
    return result;
}

internal void
sdl_get_input_file_location(struct sdl_state *state,
                            bool input_stream,
                            int slot_index,
                            int dst_count,
                            char *dst)
{
    char tmp[64];
    sprintf(tmp, "loop_edit_%d_%s.hmi", slot_index, input_stream ? "input" : "state");
    sdl_build_exe_path_filename(state, tmp, dst, dst_count);
}

internal struct sdl_replay_buffer *
sdl_get_replay_buffer(struct sdl_state *state, uint index)
{
    /* TODO(Igor): Use sdl assertions */
    Assert(index > 0);
    Assert(index < ArrayCount(state->replay_buffers));
    struct sdl_replay_buffer *result = &state->replay_buffers[index];
    return result;
}


internal void
sdl_begin_recording_input(struct sdl_state *state, int input_recording_index)
{
    struct sdl_replay_buffer *replay_buffer = sdl_get_replay_buffer(state, input_recording_index);

    if(replay_buffer->memory_block)
    {
        state->input_recording_index = input_recording_index;

        char filename[SDL_STATE_FILE_NAME_MAX_SIZE];
        sdl_get_input_file_location (state, true, input_recording_index, sizeof(filename), filename);
        state->recording_handle = open(filename,
                                       O_WRONLY | O_CREAT | O_TRUNC,
                                       S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
#if 0
        lseek(state->recording_handle, state->total_size, SEEK_SET);
#endif
        memcpy(replay_buffer->memory_block, state->game_memory_block, state->total_size);
    }
}


internal void
sdl_end_recording(struct sdl_state *state)
{
    close(state->recording_handle);
    state->input_recording_index = 0;
}


internal void
sdl_begin_input_playback(struct sdl_state *state, int input_playing_index)
{
    struct sdl_replay_buffer *replay_buffer = sdl_get_replay_buffer(state, input_playing_index);
    if(replay_buffer->memory_block)
    {
        state->input_playing_index = input_playing_index;
        char filename[SDL_STATE_FILE_NAME_MAX_SIZE];
        sdl_get_input_file_location (state, true, input_playing_index, sizeof(filename), filename);
        state->playing_handle = open(filename, O_RDONLY);
#if 0
        lseek(state->playing_handle, state->total_size, SEEK_SET);
#endif
        memcpy(state->game_memory_block, replay_buffer->memory_block, state->total_size);
    }
}

internal void
sdl_end_input_playback(struct sdl_state *state)
{
    close(state->playing_handle);
    state->input_playing_index = 0;
}

internal void
sdl_record_input(struct sdl_state *state, struct game_input *input)
{
    ssize_t bytes_written = write(state->recording_handle, input, sizeof(*input));
}

internal void
sdl_playback_input(struct sdl_state *state, struct game_input *input)
{
    ssize_t bytes_read = read(state->playing_handle, input, sizeof(*input));
    if(bytes_read == 0)
    {
        /* NOTE: Weve hit end of stream, go back to beginning */
        int playing_index = state->input_playing_index;
        sdl_end_input_playback(state);
        sdl_begin_input_playback(state, playing_index);
        read(state->playing_handle, input, sizeof(*input));
    }
}

internal void
sdl_toggle_fullscreen(SDL_Window *window)
{
    u32 flags = SDL_GetWindowFlags(window);
    if (flags & SDL_WINDOW_FULLSCREEN_DESKTOP)
    {
        SDL_SetWindowFullscreen(window, 0);
    }
    else
    {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
}

internal void
sdl_process_pending_events(struct sdl_state *state,
                           struct game_controller_input * keyboard_controller)
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_QUIT:
            {
                    global_running = false;
            } break;

            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                SDL_Keycode key_code = event.key.keysym.sym;
                SDL_Keymod  key_mod  = event.key.keysym.mod;
                bool is_down = (event.key.state == SDL_PRESSED);
                bool  alt_key_was_down = (key_mod & KMOD_ALT);

                if (event.key.repeat == 0)
                {
                    if(key_code == SDLK_w)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->move_up, is_down);
                    }
                    else if(key_code == SDLK_a)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->move_left, is_down);
                    }
                    else if(key_code == SDLK_s)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->move_down, is_down);
                    }
                    else if(key_code == SDLK_d)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->move_right, is_down);
                    }
                    else if(key_code == SDLK_q)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->left_shoulder, is_down);
                    }
                    else if(key_code == SDLK_e)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->right_shoulder, is_down);
                    }
                    else if(key_code == SDLK_UP)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->action_up, is_down);
                    }
                    else if(key_code == SDLK_DOWN)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->action_down, is_down);
                    }
                    else if(key_code == SDLK_LEFT)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->action_left, is_down);
                    }
                    else if(key_code == SDLK_RIGHT)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->action_right, is_down);
                    }
                    else if(key_code == SDLK_ESCAPE)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->back, is_down);
                    }
                    else if(key_code == SDLK_SPACE)
                    {
                        sdl_process_keyboard_event(&keyboard_controller->start, is_down);
                    }
#if 0
#if GAME_INTERNAL
                    else if(key_code == SDLK_p)
                    {
                        if(is_down)
                        {
                            global_pause = !global_pause;
                        }
                    }
                    else if(key_code == SDLK_l)
                    {
                        if(is_down)
                        {
                            if(state->input_playing_index == 0)
                            {
                                if(state->input_recording_index == 0)
                                {
                                    sdl_begin_recording_input (state, 1);
                                }
                                else
                                {
                                    sdl_end_recording_input (state, 1)
                                }
                                global_pause = !global_pause;
                            }
                        }
                    }
#endif
#endif
                    else if(key_code == SDLK_x)
                    {
                        SDL_Log("Exiting...\n");
                    }
                    else if ((key_code == SDLK_F4) && alt_key_was_down)
                    {
                        global_running = false;
                    }
                    else if ((key_code == SDLK_RETURN) && alt_key_was_down)
                    {
                        SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
                        if (window)
                        {
                            sdl_toggle_fullscreen(window);
                        }
                    }
                } /* event.key.repeat == 0 */
            } break; /* case SDL_KEYUP | SDL_KEYDOWN */

            case SDL_WINDOWEVENT:
            {
                switch(event.window.event)
                {
                    case SDL_WINDOWEVENT_MOVED:
                        { /* pass */ } break;

                    case SDL_WINDOWEVENT_RESIZED:
                        { /* handled in SIZE_CHANGED */ } break;

                    case SDL_WINDOWEVENT_SIZE_CHANGED:
                        {
                            SDL_Window *window = SDL_GetWindowFromID(event.window.windowID);
                            SDL_Renderer *renderer = SDL_GetRenderer(window);
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
                            struct sdl_window_dimension dimension = sdl_window_get_dimension(window);
                            sdl_display_buffer_in_window (&global_back_buffer, window, dimension.width, dimension.height);
                        } break;

                    case SDL_WINDOWEVENT_CLOSE:
                        {
                            SDL_Log("%s: Window Close requested\n", __func__);
                            //return RETURN_EXIT;
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
                } /* switch (window.event.type) */
            } break; /* case SDL_WINDOWEVENT */
        } /* switch (event.type) */
    } /* while (SDL_PoolEvent(event) */
}


inline u64
sdl_get_wall_clock(void)
{
    u64 result = SDL_GetPerformanceCounter();
    return result;
}

internal f32
sdl_get_seconds_elapsed(u64 start, u64 end)
{
    f32 result = (f32)(end - start) / (f32)global_performance_counters.frequency;
    return result;
}

internal void
sdl_handle_debug_cycle_counters(struct game_memory *memory)
{
#if GAME_INTERNAL
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                 "%s: Cycle counts: \n", __func__);
    /* TODO(Igor): why not use DEBUG_CC_COUNT enum value? */
    for(size_t counter_index = 0; counter_index < ArrayCount(memory->counters); ++counter_index)
    {
        struct debug_cycle_counter *counter = memory->counters + counter_index;
        if(counter->hit_count)
        {
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                    "%s: %zu: %zu cy %zu h %zu cy/h\n",
                    __func__,
                    counter_index,
                    counter->cycle_count,
                    counter->hit_count,
                    counter->cycle_count / counter->hit_count);
            counter->hit_count = 0;
            counter->cycle_count = 0;
        }
    }
#endif
}


internal int
sdl_get_window_refresh_rate(SDL_Window *window)
{
    SDL_DisplayMode mode;
    int display_index = SDL_GetWindowDisplayIndex(window);
    int default_refresh_rate = 60;
    if (SDL_GetDesktopDisplayMode(display_index, &mode) != 0)
    {
        return default_refresh_rate;
    }
    if (mode.refresh_rate == 0)
    {
        return default_refresh_rate;
    }
    return mode.refresh_rate;
}

internal SDL_Window *
sdl_init_game(u16 screen_width, u16 screen_height)
{
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

    struct sdl_window_dimension dimension = sdl_window_get_dimension(window);
    sdl_resize_texture(&global_back_buffer, window, dimension.width, dimension.height);


    return window;
}


internal void
sdl_wait_frame(struct sdl_performance_counters *perf)
{
    u64 now = 0;
    now = sdl_get_wall_clock();
    global_performance_counters.seconds_elapsed_for_frame = sdl_get_seconds_elapsed(global_performance_counters.last_counter, now);

    if(global_performance_counters.seconds_elapsed_for_frame < global_performance_counters.target_seconds_per_frame)
    {
        u32 time_to_sleep = (u32)( 1000.0f *
                                 (global_performance_counters.target_seconds_per_frame -
                                  global_performance_counters.seconds_elapsed_for_frame));

        if (time_to_sleep > 0)
        {
            SDL_Delay(time_to_sleep);
        }

        now = sdl_get_wall_clock();
        f32 test_seconds_elapsed_for_frame = sdl_get_seconds_elapsed(global_performance_counters.last_counter, now);

        if(test_seconds_elapsed_for_frame < global_performance_counters.target_seconds_per_frame)
        {
            /* log missed sleep */
        }

        while(global_performance_counters.seconds_elapsed_for_frame < global_performance_counters.target_seconds_per_frame)
        {
            now = sdl_get_wall_clock();
            global_performance_counters.seconds_elapsed_for_frame = sdl_get_seconds_elapsed (
                    global_performance_counters.last_counter, now);

        }

    }

    global_performance_counters.end_counter = sdl_get_wall_clock();
    global_performance_counters.ms_per_frame = 1000.0f * sdl_get_seconds_elapsed(
        global_performance_counters.last_counter,
        global_performance_counters.end_counter);

    global_performance_counters.last_counter = global_performance_counters.end_counter;
}

internal void
sdl_eval_performance()
{
    global_performance_counters.flip_wall_clock = sdl_get_wall_clock();
    global_performance_counters.end_cycle_count = _rdtsc();
    global_performance_counters.cycles_elapsed = global_performance_counters.end_cycle_count - global_performance_counters.last_cycle_count;
    global_performance_counters.last_cycle_count = global_performance_counters.end_cycle_count;
    global_performance_counters.fps = 0.0f;
    global_performance_counters.mcpf = (f64)global_performance_counters.cycles_elapsed / (1000.0f * 1000.0f);
}


#if GAME_INTERNAL
internal void
sdl_debug_draw_vertical(struct sdl_offscreen_buffer *back_buffer,
                      int X,
                      int top,
                      int bottom,
                      u32 color)
{
    if (top <= 0)
    {
        top = 0;
    }

    if (bottom > back_buffer->height)
    {
        bottom = back_buffer->height;
    }

    if ((X >= 0) && (X < back_buffer->width))
    {
        u8 *pixel = ((u8*)back_buffer->memory +
                    X * back_buffer->bytes_per_pixel +
                    top * back_buffer->pitch);

        for(int y = top; y < bottom; ++y)
        {
            *(u32*)pixel = color;
            pixel += back_buffer->pitch;
        }
    }
}

internal inline void
sdl_draw_sound_buffer_marker(struct sdl_offscreen_buffer *back_buffer,
                            struct sdl_sound_output *sound_output,
                            f32 C, int pad_x, int top,
                            int bottom, int value, u32 color)
{
    /* TODO: Assert(value < sound -> Secondary buffer size); */
    f32 x32 = C * (f32)value;
    int X = pad_x + (int)x32;
    sdl_debug_draw_vertical(back_buffer, X, top, bottom, color);
}

internal void
sdl_debug_sync_display(struct sdl_offscreen_buffer *back_buffer,
                     int marker_count,
                     struct sdl_debug_time_marker *markers,
                     int current_marker_index,
                     struct sdl_sound_output *sound_output,
                     struct sdl_performance_counters *perf)
{
    int pad_x = 16;
    int pad_y = 16;

    int line_height = 64;

    // What C stands for??!?!?!?
    f32 C = (f32)(back_buffer->width - 2* pad_x) / (f32)sound_output->secondary_buffer_size;
    for(int marker_index = 0; marker_index < marker_count; ++marker_index)
    {
        struct sdl_debug_time_marker *this_marker = markers + marker_index;

        u32 write_color           = 0x000000FF;
        u32 queued_audio_color    = 0x00FF0000;
        u32 expected_flip_color   = 0x0000FF00;

        int top = pad_y;
        int bottom = pad_y + line_height;

        if(marker_index == current_marker_index)
        {
            top    += line_height + pad_y;
            bottom += line_height + pad_y;

            int first_top = top;

            // Top bar
            sdl_draw_sound_buffer_marker(back_buffer, sound_output, C, marker_index * 30 + pad_x, top, bottom, this_marker->output_byte_count,  write_color);

            top    += line_height + pad_y;
            bottom += line_height + pad_y;

            // Mid
            sdl_draw_sound_buffer_marker(back_buffer, sound_output, C, marker_index * 30 + pad_x, top, bottom, this_marker->queued_audio_bytes,  queued_audio_color);

            top    += line_height + pad_y;
            bottom += line_height + pad_y;

            // bott
            sdl_draw_sound_buffer_marker(back_buffer, sound_output, C, marker_index * 30 + pad_x, top, bottom, this_marker->expected_bytes_until_flip,  expected_flip_color);
        }

        // Top bars...
        sdl_draw_sound_buffer_marker(back_buffer, sound_output, C, marker_index * 30 + pad_x, top, bottom, this_marker->output_byte_count,  write_color);
        sdl_draw_sound_buffer_marker(back_buffer, sound_output, C, marker_index * 30 + pad_x, top, bottom, this_marker->queued_audio_bytes,  queued_audio_color);
        sdl_draw_sound_buffer_marker(back_buffer, sound_output, C, marker_index * 30 + pad_x, top, bottom, this_marker->expected_bytes_until_flip,  expected_flip_color);
    }
}
#endif


int
main (void)
{
    global_running = true;
    int exitval = EXIT_FAILURE;
    int retval = RETURN_SUCCESS;

#if DEBUG
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_DEBUG);
#else
    SDL_LogSetPriority(SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_INFO);
#endif

    /*
     * Game states
     */
    struct sdl_state sdl_state = {0};
    sdl_get_exe_filename(&sdl_state);
    char source_game_code_dll_full_path[SDL_STATE_FILE_NAME_MAX_SIZE];
    sdl_build_exe_path_filename (&sdl_state,
                                 "game.so",
                                 source_game_code_dll_full_path,
                                 sizeof(source_game_code_dll_full_path));
    struct sdl_game_code game = sdl_load_game_code(source_game_code_dll_full_path);


    /*
     * Game Window Initialization
     */
    SDL_Window *window = sdl_init_game(960, 540);
    if(!window) goto __EXIT__;


    /*
     * Game Performance counters
     */
    global_performance_counters.frequency = SDL_GetPerformanceFrequency();
    f32 game_refresh_factor = .5f;
    global_performance_counters.game_update_hz = game_refresh_factor * (f32)sdl_get_window_refresh_rate(window);
    global_performance_counters.target_seconds_per_frame = 1.0f/(f32)global_performance_counters.game_update_hz;

    SDL_Log("Refresh rate is %d Hz\n", global_performance_counters.game_update_hz);
    SDL_Log("Target FPS is %f Hz\n", global_performance_counters.target_seconds_per_frame);

    /*
     * Game sound initialization
     */
    struct sdl_sound_output sound_output = {0};
    sound_output.audio_device = 0;
    sound_output.samples_per_second = 48000;
    sound_output.bytes_per_sample = sizeof(i16) * 2;
    sound_output.secondary_buffer_size = sound_output.samples_per_second * sound_output.bytes_per_sample;

    f32 sound_safety_factor = 1.0f/2.0f;
    sound_output.safety_bytes = (int)(sound_safety_factor *
                                      global_performance_counters.target_seconds_per_frame *
                                      (f32)sound_output.samples_per_second *
                                      (f32)sound_output.bytes_per_sample);
    sdl_init_audio(&sound_output);

#if 0 /* Test audio */
    /* Move it to an external function ? */
    i16 test_samples[4800] = {0}
    u64 last_counter = 0;
    while(global_running)
    {
        u32 queued_audio_bytes = SDL_GetQueuedAudioSize(sound_output->audio_device);
        if(!queued_audio_bytes)
        {
            u64 new_counter = sdl_get_wall_clock();
            i32 test_samples_sz = ArrayCount(test_samples);

            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                         "%s: %lu samples in %f time\n",
                         __func__,
                         test_samples_sz,
                         sdl_get_seconds_elapsed(last_counter, new_counter));

            SDL_QueueAudio(sount_output->audio_device,
                           test_samples,
                           test_samples_sz);

            last_counter = new_counter;
        }
    }
#endif

    u32 max_possible_overrun = 8;
    i16 *sound_samples = (i16 *)calloc(sound_output.samples_per_second + max_possible_overrun, sound_output.bytes_per_sample);

    /*
     * Game Memory Initialization
     */
#if GAME_INTERNAL
    void *base_address = (void*)Terabytes(2);
#else
    void *base_address = (void*)(0);
#endif

    struct game_memory game_memory = {0};
    game_memory.permanent_storage_size = Megabytes(256);
    game_memory.transient_storage_size = Gigabytes(1);
    size_t game_total_storage_size = game_memory.permanent_storage_size + game_memory.transient_storage_size;


    sdl_state.total_size = game_total_storage_size;
    sdl_state.game_memory_block = mmap(base_address,
                                       sdl_state.total_size,
                                       PROT_READ | PROT_WRITE,
                                       MAP_ANON | MAP_PRIVATE,
                                       -1,
                                       0);

    game_memory.permanent_storage = sdl_state.game_memory_block;
    game_memory.transient_storage = (u8 *)game_memory.permanent_storage + game_memory.permanent_storage_size;


    /*
     * Debug replay buffer
     */
    /*
     * TODO(Igor): Use REPLAY_BUFFERS_MAX istead ArrayCount?
     * maybe not, because this is callet once, hence not so expensive
     */
    for (size_t replay_index = 1; replay_index < ArrayCount(sdl_state.replay_buffers); ++replay_index)
    {
        struct sdl_replay_buffer *replay_buffer = &sdl_state.replay_buffers[replay_index];

        sdl_get_input_file_location(&sdl_state, false, replay_index, sizeof(replay_buffer->filename), replay_buffer->filename);

        replay_buffer->file_handle = open(replay_buffer->filename, O_RDWR | O_CREAT, S_IRUSR | S_IRGRP | S_IROTH);

        ftruncate(replay_buffer->file_handle, sdl_state.total_size);

        replay_buffer->memory_block = mmap(0,
                                           sdl_state.total_size,
                                           PROT_READ | PROT_WRITE,
                                           MAP_PRIVATE,
                                           replay_buffer->file_handle,
                                           0);

        if(replay_buffer->memory_block)
        {
            /* we got mmaped file */
        }
        else
        {
            /* TODO: wedont... diagnose it... */
        }
    }

    if(! (sound_samples && game_memory.permanent_storage && game_memory.transient_storage))
    {
        /* TODO: Logging */
        goto __EXIT__;
    }


    /*
     * Init game input
     */
    sdl_game_controllers_open();
    struct game_input input[2] = {0};
    struct game_input *new_input = &input[0];
    struct game_input *old_input = &input[1];

    /*
     * Fill first performance counters
     */
    global_performance_counters.last_counter = sdl_get_wall_clock();
    global_performance_counters.flip_wall_clock = sdl_get_wall_clock();
    global_performance_counters.last_cycle_count = _rdtsc();

#if GAME_INTERNAL
#define TIME_MARKER_MAX 30
    size_t debug_time_marker_index = 0;
    struct sdl_debug_time_marker debug_time_markers[TIME_MARKER_MAX] = {0};
#endif

#if 0 /* Remove if everything goes fine */
    struct sdl_game_code game = sdl_load_game_code(source_game_code_dll_full_path);
#endif

    /*
     * MAIN LOOP
     */
    while(global_running)
    {
        new_input->dt_for_frame = global_performance_counters.target_seconds_per_frame;
        new_input->executable_reloaded = false;
        time_t dll_write_time = sdl_get_last_write_time(source_game_code_dll_full_path);
        if(dll_write_time != game.dll_last_write_time)
        {
            sdl_unload_game_code(&game);
            game = sdl_load_game_code(source_game_code_dll_full_path);
            new_input->executable_reloaded = true;
        }

        struct game_controller_input *old_keyboard_controller = get_controller (old_input, 0);
        struct game_controller_input *new_keyboard_controller = get_controller (new_input, 0);
        //new_keyboard_controller = {0};
        new_keyboard_controller->is_connected = true;

        for (size_t button_index = 0; button_index < ArrayCount(new_keyboard_controller->buttons); ++button_index)
        {
            new_keyboard_controller->buttons[button_index].ended_down = old_keyboard_controller->buttons[button_index].ended_down;
        }

        sdl_process_pending_events (&sdl_state, new_keyboard_controller);

        if(!global_pause)
        {
            u32 mouse_state = SDL_GetMouseState(&new_input->mouse_x, &new_input->mouse_y);
            /* TODO: Support mouse wheel */
            new_input->mouse_z = 0;

            sdl_process_keyboard_event(&new_input->mouse_buttons[0], mouse_state & SDL_BUTTON(SDL_BUTTON_LEFT));
            sdl_process_keyboard_event(&new_input->mouse_buttons[1], mouse_state & SDL_BUTTON(SDL_BUTTON_MIDDLE));
            sdl_process_keyboard_event(&new_input->mouse_buttons[2], mouse_state & SDL_BUTTON(SDL_BUTTON_RIGHT));
            sdl_process_keyboard_event(&new_input->mouse_buttons[3], mouse_state & SDL_BUTTON(SDL_BUTTON_X1));
            sdl_process_keyboard_event(&new_input->mouse_buttons[4], mouse_state & SDL_BUTTON(SDL_BUTTON_X2));

            u32 max_controller_count = MAX_CONTROLLERS;
            if(max_controller_count > (ArrayCount(new_input->controllers) - 1))
            {
                max_controller_count = ArrayCount(new_input->controllers) - 1;
            }

            for(size_t controller_index = 0; controller_index < max_controller_count; ++controller_index)
            {
                struct game_controller_input * old_controller = get_controller(old_input, controller_index + 1);
                struct game_controller_input * new_controller = get_controller(new_input, controller_index + 1);

                SDL_GameController *controller = controller_handles[controller_index];
                if (controller && SDL_GameControllerGetAttached(controller))
                {
                    new_controller->is_connected = true;
                    new_controller->is_analog = old_controller->is_analog;

                    i16 axis_lx = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTX);
                    i16 axis_ly = SDL_GameControllerGetAxis(controller, SDL_CONTROLLER_AXIS_LEFTY);

                    new_controller->stick_average_x = sdl_process_game_controller_axis_values (axis_lx, CONTROLLER_AXIS_LEFT_DEADZONE);
                    new_controller->stick_average_y = sdl_process_game_controller_axis_values (axis_ly, CONTROLLER_AXIS_LEFT_DEADZONE);

                    if((new_controller->stick_average_x != 0.0f) || (new_controller->stick_average_y != 0.0f))
                    {
                        new_controller->is_analog = true;
                    }

                    if(SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP))
                    {
                        new_controller->stick_average_y = 1.0f;
                        new_controller->is_analog = false;
                    }

                    if(SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))
                    {
                        new_controller->stick_average_y = -1.0f;
                        new_controller->is_analog = false;
                    }

                    if(SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))
                    {
                        new_controller->stick_average_x = -1.0f;
                        new_controller->is_analog = false;
                    }

                    if(SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT))
                    {
                        new_controller->stick_average_x = 1.0f;
                        new_controller->is_analog = false;
                    }

                    f32 threshold = 0.5f;

                    sdl_process_game_controller_button(&(old_controller->move_left),
                                                       &(new_controller->move_left),
                                                       new_controller->stick_average_x < -threshold);
                    sdl_process_game_controller_button(&(old_controller->move_right),
                                                       &(new_controller->move_right),
                                                       new_controller->stick_average_x >  threshold);
                    sdl_process_game_controller_button(&(old_controller->move_up),
                                                       &(new_controller->move_up),
                                                       new_controller->stick_average_y < -threshold);
                    sdl_process_game_controller_button(&(old_controller->move_down),
                                                       &(new_controller->move_down),
                                                       new_controller->stick_average_y >  threshold);

                    sdl_process_game_controller_button(&(old_controller->action_down),
                                                       &(new_controller->action_down),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_A));

                    sdl_process_game_controller_button(&(old_controller->action_right),
                                                       &(new_controller->action_right),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_B));

                    sdl_process_game_controller_button(&(old_controller->action_left),
                                                       &(new_controller->action_left),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_X));

                    sdl_process_game_controller_button(&(old_controller->action_up),
                                                       &(new_controller->action_up),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_Y));

                    sdl_process_game_controller_button(&(old_controller->left_shoulder),
                                                       &(new_controller->left_shoulder),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_LEFTSHOULDER));

                    sdl_process_game_controller_button(&(old_controller->right_shoulder),
                                                       &(new_controller->right_shoulder),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));

                    sdl_process_game_controller_button(&(old_controller->back),
                                                       &(new_controller->back),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_BACK));

                    sdl_process_game_controller_button(&(old_controller->start),
                                                       &(new_controller->start),
                                                       SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_START));
                }
                else
                {
                    /* controller disconnected */
                    new_controller->is_connected = false;
                }
            } /* loop controllers */

            struct game_offscreen_buffer screen_buffer = {0};
            // screen_buffer.memory = (u8*)global_back_buffer.memory +
            //                        global_back_buffer.width *
            //                        (global_back_buffer.height - 1) *
            //                        global_back_buffer.bytes_per_pixel;
            screen_buffer.memory = global_back_buffer.memory;
            screen_buffer.width  = global_back_buffer.width;
            screen_buffer.height = global_back_buffer.height;
            screen_buffer.pitch  = global_back_buffer.pitch;

            if (sdl_state.input_recording_index)
            {
                sdl_record_input(&sdl_state, new_input);
            }

            if (sdl_state.input_playing_index)
            {
                sdl_playback_input (&sdl_state, new_input);
            }

            if(game.update_and_render)
            {
                game.update_and_render(&game_memory, new_input, &screen_buffer);
            }

            /*
             * Audio "render"
             * TODO(Igor): Move to an external function, maybe inside game
             * TODO(Igor): Reorganize this mess...
             */


            global_audio_counters.queued_audio_bytes = SDL_GetQueuedAudioSize(sound_output.audio_device);

            global_audio_counters.expected_sound_bytes_per_frame = (int)(
                (f32)(sound_output.samples_per_second * sound_output.bytes_per_sample) /
                global_performance_counters.game_update_hz);
            // NOTE(IGor): Is equivalent to multiply for * global_performance_counters.target_Frames_per_second

            i32 bytes_to_write = global_audio_counters.expected_sound_bytes_per_frame +
                                 sound_output.safety_bytes -
                                 global_audio_counters.queued_audio_bytes;

            if (bytes_to_write < 0)
            {
                bytes_to_write = 0;
            }

#if GAME_INTERNAL
            global_audio_counters.wall_clock = sdl_get_wall_clock();
            global_audio_counters.from_begin_to_audio_seconds = sdl_get_seconds_elapsed(global_performance_counters.flip_wall_clock, global_audio_counters.wall_clock);
            global_audio_counters.seconds_left_until_flip = global_performance_counters.target_seconds_per_frame - global_audio_counters.from_begin_to_audio_seconds;
            global_audio_counters.expected_bytes_until_flip = (u32)(
                     global_audio_counters.seconds_left_until_flip *
                     global_performance_counters.game_update_hz *
                     (f32)global_audio_counters.expected_sound_bytes_per_frame);
#endif

            struct game_sound_output_buffer sound_buffer = {0};
            sound_buffer.samples_per_second = sound_output.samples_per_second;
            sound_buffer.sample_count = Align8(bytes_to_write / sound_output.bytes_per_sample);
            bytes_to_write = sound_buffer.sample_count * sound_output.bytes_per_sample;
            sound_buffer.samples = sound_samples;

            if (game.get_sound_samples)
            {
                game.get_sound_samples(&game_memory, &sound_buffer);
            }

#if GAME_INTERNAL
            struct sdl_debug_time_marker *marker = &debug_time_markers[debug_time_marker_index];
            marker->queued_audio_bytes = global_audio_counters.queued_audio_bytes;
            marker->output_byte_count = bytes_to_write;
            marker->expected_bytes_until_flip = global_audio_counters.expected_bytes_until_flip;

            global_audio_counters.audio_latency_seconds =
                (f32)global_audio_counters.queued_audio_bytes /
                (f32)sound_output.bytes_per_sample /
                (f32)sound_output.samples_per_second;

#if 0
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                    "BTW: %u - Latency: %d (%fs)\n",
                    bytes_to_write,
                    global_audio_counters.queued_audio_bytes,
                    global_audio_counters.audio_latency_seconds);
#endif
#endif

            sdl_fill_sound_buffer(&sound_output, &sound_buffer, (u32)bytes_to_write);

            sdl_wait_frame(&global_performance_counters);

#if GAME_INTERNAL
            sdl_debug_sync_display(&global_back_buffer,
                    ArrayCount(debug_time_markers), debug_time_markers, debug_time_marker_index - 1,
                    &sound_output, &global_performance_counters);
#endif

            struct game_input *tmp_input = new_input;
            new_input = old_input;
            old_input = tmp_input;

            sdl_eval_performance();
#if GAME_SLOW
            SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                         "%.02f ms/f, %.02f f/s, %.02f mc/f\n",
                         global_performance_counters.ms_per_frame,
                         global_performance_counters.fps,
                         global_performance_counters.mcpf);
#endif

#if GAME_INTERNAL
            ++debug_time_marker_index;
            if (debug_time_marker_index == ArrayCount(debug_time_markers))
            {
                debug_time_marker_index = 0;
            }
#endif
        } /* if (!global_pause) */
    } /* while(global_running) */

    sdl_game_controllers_close();
    exitval = EXIT_SUCCESS;

__EXIT__:
    SDL_Log ("Exiting\n");
    /* Why gcc is returning error about unitialized ? */
#if 0
    if(sound_samples)
    {
        free(sound_samples);
    }
#endif

    if(sdl_state.game_memory_block)
        munmap(sdl_state.game_memory_block, sdl_state.total_size);

    if(sound_output.audio_device > 0)
        SDL_CloseAudioDevice(sound_output.audio_device);

    if (global_back_buffer.memory)
        munmap (global_back_buffer.memory, global_back_buffer.width * global_back_buffer.height * global_back_buffer.bytes_per_pixel);

    if (global_back_buffer.texture)
        SDL_DestroyTexture(global_back_buffer.texture);

    if (window)
    {
        SDL_Renderer *renderer = SDL_GetRenderer(window);
        if (renderer)
            SDL_DestroyRenderer(renderer);
    }

    if (window)
        SDL_DestroyWindow(window);

    SDL_Quit();

    return exitval;

} /* main() */






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
