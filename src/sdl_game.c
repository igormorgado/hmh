#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <math.h>
#include <SDL2/SDL.h>
#include <assert.h>
#include <sys/mman.h>
#include <x86intrin.h>


#define MININT(a, b)    ((a) < (b) ? (a) : (b))
#define MAXINT(a, b)    ((a) > (b) ? (a) : (b))

#ifndef MAP_ANONYMOUS
# define MAP_ANONYMOUS MAP_ANON
#endif


#define PI          3.14159265358979323846f  /* pi */
#define PI_2        1.57079632679489661923f  /* pi/2 */
#define PI_4        0.78539816339744830962f  /* pi/4 */
#define TAU         6.28318530717958623199f  /* Tau = 2*pi */


#define internal static
#define local_persist static
#define global_variable static

typedef int8_t          i8;
typedef int16_t         i16;
typedef int32_t         i32;
typedef int64_t         i64;

typedef uint8_t         u8;
typedef uint16_t        u16;
typedef uint32_t        u32;
typedef uint64_t        u64;

typedef unsigned int    uint;
typedef float           f32;
typedef double          f64;


enum RETURN_STATUS
{
    RETURN_FAILURE = -1,
    RETURN_SUCCESS = 0,
    RETURN_EXIT = 255
};


struct game_offscreen_buffer
{
    SDL_Texture *texture;
    void *memory;
    int width;
    int height;
    int pitch;
};

struct window_dimension
{
    int width;
    int height;
};

global_variable u8 red = 0;

#include "game.c"

global_variable struct game_offscreen_buffer GlobalBackBuffer;
global_variable f32 XOffset = 0.f;
global_variable f32 YOffset = 0.f;
// global_variable i32 FramesPerSecond = 60;

/* Input data */
#define MAX_CONTROLLERS 4
SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];

struct audio_ring_buffer
{
    i32 size;
    i32 write_cursor;
    i32 play_cursor;
    void * data;
};

struct sound_output
{
    i32 samples_per_second;
    f32 toneHz;
    f32 amplitude;
    size_t running_sample_index;
    f32 wave_period;
    i32 bytes_per_sample;
    i32 latency_sample_count;
    i32 secondary_buffer_size; /* todo: size_t? */
    f32 t_sine;
};

global_variable struct audio_ring_buffer AudioRingBuffer;
global_variable struct sound_output global_sound_output;
global_variable SDL_AudioDeviceID adev;


internal void
AudioCallback(void *userdata, u8 *audiodata, i32 length)
{
    struct audio_ring_buffer *ringbuffer = (struct audio_ring_buffer *)userdata;

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


internal SDL_AudioDeviceID
InitAudioCallback(i32 SamplesPerSecond, i32 BufferSize)
{
    SDL_AudioSpec AudioSettings = {0};
    AudioSettings.freq = SamplesPerSecond;
    AudioSettings.format = AUDIO_S16LSB;
    AudioSettings.channels = 2;
    AudioSettings.samples = 1024;
    AudioSettings.callback = &AudioCallback;
    AudioSettings.userdata = &AudioRingBuffer;

    AudioRingBuffer.size = BufferSize;
    AudioRingBuffer.play_cursor = 0;
    AudioRingBuffer.write_cursor = 0;
    AudioRingBuffer.data = malloc(BufferSize);
    if(!AudioRingBuffer.data)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: malloc(AudioRingBuffer.data)\n", __func__);
        exit(EXIT_FAILURE);
    }

    adev = SDL_OpenAudioDevice(NULL, 0, &AudioSettings, NULL, 0);
    SDL_Log("%s: Audio opened: freq: %d, %d channels, %d bufsz\n",
            __func__, AudioSettings.freq, AudioSettings.channels, AudioSettings.samples);

    if (AudioSettings.format != AUDIO_S16LSB)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_OpenAudio(): Could not get S16LE format\n",
                __func__);
        SDL_CloseAudioDevice(adev);
        return 0;
    } else {
        return adev;
    }
}


internal void
sound_get_cursors(SDL_AudioDeviceID audiodevice,
                  i32 *byte_to_lock,
                  i32 *bytes_to_write)
{
    SDL_LockAudioDevice(audiodevice);
    *byte_to_lock = ((global_sound_output.running_sample_index
                     * global_sound_output.bytes_per_sample)
                    % global_sound_output.secondary_buffer_size);

    i32 target_cursor = ((( global_sound_output.latency_sample_count
                          * global_sound_output.bytes_per_sample)
                         + AudioRingBuffer.play_cursor)
                        % global_sound_output.secondary_buffer_size);

    if (*byte_to_lock > target_cursor)
        *bytes_to_write = global_sound_output.secondary_buffer_size - *byte_to_lock + target_cursor;
    else
        *bytes_to_write = target_cursor - *byte_to_lock;
    SDL_UnlockAudioDevice(audiodevice);
}

internal void
PlaySineWaveCallback(i32 byte_to_lock,
                     i32 bytes_to_write)
{

    /* Calculate region sizes */
    void *region1 = (u8*)AudioRingBuffer.data + byte_to_lock;
    i32 region1_size = bytes_to_write;
    if(region1_size + byte_to_lock > global_sound_output.secondary_buffer_size)
        region1_size = global_sound_output.secondary_buffer_size - byte_to_lock;

    void *region2 = AudioRingBuffer.data;
    i32 region2_size = bytes_to_write - region1_size;

    /* Initialize sample variables */
    i16 *sample_out = NULL;
    i16 sample_value = 0;
    f32 sine_value = 0.0f;

    /* Fill region 1 */
    i32 region1_sample_count = region1_size / global_sound_output.bytes_per_sample;
    sample_out = (i16*)region1;
    for (i32 sample_index = 0; sample_index < region1_sample_count; ++ sample_index)
    {
        sine_value = sinf(global_sound_output.t_sine);
        sample_value = (i16)(roundf(sine_value * global_sound_output.amplitude));
        *sample_out++ = sample_value;     // L
        *sample_out++ = sample_value;     // R

        global_sound_output.t_sine += TAU / global_sound_output.wave_period;
        ++global_sound_output.running_sample_index;
    }

    /* Fill region 2 */
    i32 region2_sample_count = region2_size / global_sound_output.bytes_per_sample;
    sample_out = (i16*)region2;
    for (i32 sample_index = 0; sample_index < region2_sample_count; ++ sample_index)
    {
        sine_value = sinf(global_sound_output.t_sine);
        sample_value = (i16)(roundf(sine_value * global_sound_output.amplitude));
        *sample_out++ = sample_value;     // L
        *sample_out++ = sample_value;     // R

        global_sound_output.t_sine += TAU / global_sound_output.wave_period;
        ++global_sound_output.running_sample_index;
    }
}


internal void
PlaySquareWaveCallback(i32 byte_to_lock,
                       i32 bytes_to_write)
{
    /* Calculate region sizes */
    void *region1 = (u8*)AudioRingBuffer.data + byte_to_lock;
    i32 region1_size = bytes_to_write;
    if(region1_size + byte_to_lock > global_sound_output.secondary_buffer_size)
        region1_size = global_sound_output.secondary_buffer_size - byte_to_lock;

    void *region2 = AudioRingBuffer.data;
    i32 region2_size = bytes_to_write - region1_size;

    /* Initialize sample variables */
    i16 *sample_out = NULL;
    i16 sample_value = 0;

    /* Fill region 1 */
    i32 region1_sample_count = region1_size / global_sound_output.bytes_per_sample;
    sample_out = (i16*)region1;
    for (i32 sample_index = 0; sample_index < region1_sample_count; ++ sample_index)
    {
        if ((global_sound_output.running_sample_index / (i32)(global_sound_output.wave_period / 2.0f)) % 2)
            sample_value =  global_sound_output.amplitude;
        else
            sample_value = -global_sound_output.amplitude;

        *sample_out++ = sample_value;     // L
        *sample_out++ = sample_value;     // R
        ++global_sound_output.running_sample_index;
    }

    /* Fill region 2 */
    i32 region2_sample_count = region2_size / global_sound_output.bytes_per_sample;
    sample_out = (i16*)region2;
    for (i32 sample_index = 0; sample_index < region2_sample_count; ++ sample_index)
    {
        if ((global_sound_output.running_sample_index / (i32)(global_sound_output.wave_period / 2.0f)) % 2)
            sample_value =  global_sound_output.amplitude;
        else
            sample_value = -global_sound_output.amplitude;

        *sample_out++ = sample_value;     // L
        *sample_out++ = sample_value;     // R
        ++global_sound_output.running_sample_index;
    }
}


internal struct window_dimension
WindowGetDimension(SDL_Window *window)
{
    struct window_dimension result;
    SDL_GetWindowSize(window, &result.width, &result.height);
    return result;
}



internal int
ResizeBackBuffer(struct game_offscreen_buffer *buffer, SDL_Window *window, u16 width, u16 height)
{
    // TODO: ASSERT WINDOW not null
    // TODO: ASSERT 0 < width/height <= 4096

    int bytes_per_pixel = SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888);
    if(buffer->memory)
    {
        // free(buffer->memory);
        munmap(buffer->memory, buffer->width * buffer->height * bytes_per_pixel);
    }

    if(buffer->texture)
    {
        SDL_DestroyTexture(buffer->texture);
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    buffer->texture = SDL_CreateTexture(renderer,
                                        SDL_PIXELFORMAT_ARGB8888,
                                        SDL_TEXTUREACCESS_STREAMING,
                                        width,
                                        height);

    if(!buffer->texture)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: SDL_CreateTexture(GlobalBackBuffer): %s\n",
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
    buffer->width = width;
    buffer->height = height;
    buffer->pitch = width * bytes_per_pixel;
    // buffer->memory = malloc(buffer->width * buffer->height * bytes_per_pixel);
    buffer->memory = mmap(NULL,
                        buffer->width * buffer->height * bytes_per_pixel,
                        PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS,
                        -1,
                        0);
    if(!buffer->memory)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                      "%s: malloc(buffer->memory): %s\n",
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
    return RETURN_SUCCESS;
}


internal int
UpdateWindow(SDL_Window *window, struct game_offscreen_buffer *buffer)
{
    // TODO: ASSERT WINDOW not null
    // TODO: Handle all those "ifs"  with SDL_Asserts.
    i32 retval = RETURN_FAILURE;
#if 0
    if(!buffer->texture)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: GlobalBackBuffer Texture NULL, nothing to update\n",
                __func__);
        return retval;
    }
#endif

    retval = SDL_UpdateTexture(buffer->texture,
                               NULL,
                               buffer->memory,
                               buffer->pitch);

    if(retval < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_UpdateTexture(GlobalBackBuffer): %s\n",
                __func__, SDL_GetError());
        return retval;
    }

    SDL_Renderer *renderer = SDL_GetRenderer(window);
#if 0
    if(!renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_GetRenderer(): %s\n",
                __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
#endif

    retval = SDL_RenderCopy(renderer, buffer->texture, NULL, NULL);
#if 0
    if(retval < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_RenderCopy(GlobalBackBuffer.texture): %s\n",
                __func__, SDL_GetError());
        return retval;
    }
#endif

    SDL_RenderPresent(renderer);
    return RETURN_SUCCESS;
}


internal i32
HandleQuit()
{
    return RETURN_EXIT;
}


internal i32
HandleWindow(const SDL_Event event)
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
            //ResizeBackBuffer(window, event.window.data1, event.window.data2);
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
            UpdateWindow(window, &GlobalBackBuffer);
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
HandleKey(const SDL_Event event)
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
HandleEvent(const SDL_Event event)
{
    // TODO: Assert eevent not null
    i32 retval = RETURN_SUCCESS;
    switch(event.type)
    {
        case SDL_QUIT:          retval = HandleQuit(); break;
        case SDL_WINDOWEVENT:   retval = HandleWindow(event); break;
        case SDL_KEYUP:         retval = HandleKey(event); break;
        case SDL_KEYDOWN:       retval = HandleKey(event); break;
        default: break;
    }
    return retval;
}


internal i32
GameControllersInit()
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
GameControllersQuit()
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


void HandleController(void)
{
    f32 speed = 2.0f;
    for(int i = 0; i< MAX_CONTROLLERS; i++)
    {
        i32 retval = 0;
        bool up = false;
        bool down = false;
        // bool left = false;
        // bool right = false;
        // bool back = false;
        bool start = false;
        // bool lshoulder = false;
        bool rshoulder = false;
        bool buttonA = false;
        bool buttonB = false;
        bool buttonX = false;
        bool buttonY = false;
        i16  lstickX = 0;
        i16  lstickY = 0;
        // i16  rstickX = 0;
        i16  rstickY = 0;
        if(ControllerHandles[i] != NULL && SDL_GameControllerGetAttached(ControllerHandles[i]))
        {
            up        = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_UP);
            down      = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            // left      = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            // right     = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            // back      = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_BACK);
            start     = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_START);
            // lshoulder = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            rshoulder = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            buttonA   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_A);
            buttonB   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_B);
            buttonX   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_X);
            buttonY   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_Y);
            lstickX   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTX);
            lstickY   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTY);
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
        } else {
            // TODO: Controller not plugged in (CLOSE IT!)
        }

    }
    return;
}


internal SDL_Window *
InitGame(u16 screen_width, u16 screen_height)
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

    GameControllersInit();

    return window;
}


internal void
ExitGame(SDL_Window *window)
{
    // TODO: Assert Window not null
    SDL_Log ("Exiting\n");
    GameControllersQuit();
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
main(void)
{
    i32 exitval = EXIT_FAILURE;
    i32 retval = RETURN_SUCCESS;

    SDL_Window *window = InitGame(800, 600);
    if(!window) {
        goto __EXIT__;
    }

    u64 perf_count_frequency = SDL_GetPerformanceFrequency();

    struct window_dimension wdim = WindowGetDimension(window);
    ResizeBackBuffer(&GlobalBackBuffer, window, wdim.width, wdim.height);

    // Queue Buffer size
    global_sound_output.samples_per_second = 48000;
    global_sound_output.toneHz = 256.0f;
    global_sound_output.amplitude = 2000.0f;
    global_sound_output.running_sample_index = 0;
    global_sound_output.wave_period = (f32)global_sound_output.samples_per_second / global_sound_output.toneHz;
    global_sound_output.bytes_per_sample = sizeof(i16) * 2;
    global_sound_output.secondary_buffer_size = global_sound_output.samples_per_second * global_sound_output.bytes_per_sample;
    global_sound_output.latency_sample_count = global_sound_output.samples_per_second / 30;

    /* Initialize sound paused*/
    InitAudioCallback(global_sound_output.samples_per_second, global_sound_output.secondary_buffer_size);
    PlaySineWaveCallback(0, global_sound_output.latency_sample_count * global_sound_output.bytes_per_sample);
    SDL_PauseAudioDevice(adev, 0);

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
            retval = HandleEvent(event);
            if(retval == RETURN_EXIT)
                running = false;
        }
        HandleController();

        /* Play sound */
        i32 byte_to_lock  = 0;
        i32 bytes_to_write = 0;
        sound_get_cursors(adev, &byte_to_lock, &bytes_to_write);
        PlaySineWaveCallback(byte_to_lock, bytes_to_write);

        /* Draw screen */
        struct game_offscreen_buffer buffer = {};
        buffer.memory = GlobalBackBuffer.memory;
        buffer.width = GlobalBackBuffer.width;
        buffer.height = GlobalBackBuffer.height;
        buffer.pitch = GlobalBackBuffer.pitch;
        GameUpdateAndRender(&buffer, XOffset, YOffset);
        UpdateWindow(window, &GlobalBackBuffer);

        ++XOffset;
        YOffset+=.75;

        //u64 perf_count_frequency = SDL_GetPerformanceFrequency();
        u64 end_cycle_count = _rdtsc();
        u64 end_counter = SDL_GetPerformanceCounter();
        u64 counter_elapsed = end_counter - last_counter;
        u64 cycles_elapsed = end_cycle_count - last_cycle_count;

        f64 ms_per_frame = 1000.0 * (f64)counter_elapsed / (f64)perf_count_frequency;
        f64 fps = (f64)perf_count_frequency / (f64)counter_elapsed;
        f64 mcpf = (f64)cycles_elapsed / 1000000.0;

        fprintf(stdout, "%6.02fms/f, %6.02ff/s %6.02fmc/f -- tone: %7.02f, wave: %9.02f\r", ms_per_frame, fps, mcpf, global_sound_output.toneHz, global_sound_output.wave_period);

        last_cycle_count = end_cycle_count;
        last_counter = end_counter;
    }

    exitval = EXIT_SUCCESS;
__EXIT__:
    if (adev > 0)
        SDL_CloseAudioDevice(adev);
    ExitGame(window);
    return exitval;
}

