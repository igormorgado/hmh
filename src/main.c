#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <SDL2/SDL.h>


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

struct offscreen_buffer 
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

SDL_Window * InitGame(u16 screen_width, u16 screen_height);
void ExitGame(SDL_Window *window);
i32  HandleEvent(const SDL_Event event);
i32  HandleWindow(const SDL_Event event);
i32  HandleKey(const SDL_Event event);
void HandleController(void);

struct window_dimension WindowGetDimension(SDL_Window *window);
static int ResizeBackBuffer(SDL_Window *window, u16 width, u16 height);
static int UpdateWindow(SDL_Window *window);
static int RenderWeirdGradient(SDL_Window *window, i16 xoffset, i16 yoffset);

i32  GameControllersInit();
void GameControllersQuit();

// GLOBAL STUFF
struct offscreen_buffer GlobalBackBuffer;
u8 red = 0;
f32 XOffset = 0.f;
f32 YOffset = 0.f;

#define MAX_CONTROLLERS 4
SDL_GameController *ControllerHandles[MAX_CONTROLLERS];
SDL_Haptic *RumbleHandles[MAX_CONTROLLERS];


int
main(void) 
{ 
    i32 exitval;
    i32 retval = RETURN_SUCCESS;

    SDL_Window *window = InitGame(800, 600);
    if(!window) {
        exitval = EXIT_FAILURE;
        goto __EXIT__;
    }


    struct window_dimension wdim = WindowGetDimension(window);
    ResizeBackBuffer(window, wdim.width, wdim.height);
    bool running = true;
    while(running == true)
    {
        SDL_Event event;
        while(SDL_PollEvent(&event)) 
        {
           retval = HandleEvent(event);
           if(retval == RETURN_EXIT) 
           {
               running = false;
           }
        }
        HandleController();

        RenderWeirdGradient(window, XOffset, YOffset);
        UpdateWindow(window);

        ++XOffset;
        YOffset+=.75;
    }

    exitval = EXIT_SUCCESS;
__EXIT__:
     ExitGame(window);
     return exitval;
}


SDL_Window *
InitGame(u16 screen_width, u16 screen_height)
{
    // TODO: Assert Window not null
    // TODO: Assert widht/height < 4096
    i32 retval;
    int sdl_flags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC;
    int wnd_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE;
    int rnd_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;

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


void
ExitGame(SDL_Window *window)
{
    // TODO: Assert Window not null
    SDL_Log ("Exiting\n");
    GameControllersQuit();
    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if (GlobalBackBuffer.memory)    free(GlobalBackBuffer.memory);
    if (GlobalBackBuffer.texture)   SDL_DestroyTexture(GlobalBackBuffer.texture);
    if (renderer)                   SDL_DestroyRenderer(renderer);
    if (window)                     SDL_DestroyWindow(window);
    SDL_Quit();
    return; 
}


i32
HandleEvent(const SDL_Event event)
{
    // TODO: Assert eevent not null
    i32 retval = RETURN_SUCCESS;
    switch(event.type)
    {
        case SDL_QUIT:          retval = RETURN_EXIT; break;
        case SDL_WINDOWEVENT:   retval = HandleWindow(event); break;
        case SDL_KEYUP:         retval = HandleKey(event); break;
        case SDL_KEYDOWN:       retval = HandleKey(event); break;
        default: break;
    }
    return retval;
}


i32
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
            UpdateWindow(window);
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


i32
HandleKey(const SDL_Event event)
{
    SDL_Keycode KeyCode = event.key.keysym.sym;
    SDL_Keymod  KeyMod  = event.key.keysym.mod;
    // Should I use instead:
    // SDL_Keymod  KeyMod = SDL_GetModState()

    bool isDown = (event.key.state == SDL_PRESSED);
    bool wasDown = false;
    if (event.key.state == SDL_RELEASED)
    {
        wasDown = true;
    } else if (event.key.repeat != 0) {
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
            // return RETURN_EXIT;
        }
        else if((KeyCode == SDLK_RETURN) && (KeyMod & KMOD_ALT))
        {
            SDL_Log("ALT-ENTER pressed\n");
        }
        else
        {
            SDL_Log("Unmapped key pressed\n");
        }
    }
    return RETURN_SUCCESS;
}


static int
ResizeBackBuffer(SDL_Window *window, u16 width, u16 height)
{
    // TODO: ASSERT WINDOW not null
    // TODO: ASSERT 0 < width/height <= 4096

    if(GlobalBackBuffer.memory)
    {
        free(GlobalBackBuffer.memory);
        //munmap(GlobalBackBuffer.memory,
        //       GlobalBackBuffer.width * GlobalBackBuffer.height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));
    }
    
    if(GlobalBackBuffer.texture)
    {
        SDL_DestroyTexture(GlobalBackBuffer.texture);
    }

    GlobalBackBuffer.width = width;
    GlobalBackBuffer.height = height;

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    GlobalBackBuffer.texture = SDL_CreateTexture(renderer, 
                                                  SDL_PIXELFORMAT_ARGB8888,
                                                  SDL_TEXTUREACCESS_STREAMING,
                                                  GlobalBackBuffer.width,
                                                  GlobalBackBuffer.height);
    if(!GlobalBackBuffer.texture)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, 
                      "%s: SDL_CreateTexture(GlobalBackBuffer): %s\n", 
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }

    // Later benchmark vs mmap
    // TODO: GET PIXEL FORMAT FROM TEXTURE
    GlobalBackBuffer.memory = malloc(GlobalBackBuffer.width * GlobalBackBuffer.height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));
    // BitmapMemory = mmap(NULL,
    //                     GlobalBackBuffer.width * GlobalBackBuffer.height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888),
    //                     PROT_READ | PROT_WRITE,
    //                     MAP_ANONYMOUS | MAP_PRIVATE,
    //                     -1,
    //                     0);
    if(!GlobalBackBuffer.memory)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, 
                      "%s: malloc(GlobalBackBuffer.memory): %s\n", 
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
    return RETURN_SUCCESS;
}


static int
UpdateWindow(SDL_Window *window)
{
    // TODO: ASSERT WINDOW not null
    i32 retval = RETURN_FAILURE;
    if(!GlobalBackBuffer.texture) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: GlobalBackBuffer Texture NULL, nothing to update\n",
                __func__);
        return retval;
    }

    retval = SDL_UpdateTexture(GlobalBackBuffer.texture,
                               NULL, 
                               GlobalBackBuffer.memory, 
                               GlobalBackBuffer.width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));

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

    retval = SDL_RenderCopy(renderer, GlobalBackBuffer.texture, NULL, NULL);
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


static int
RenderWeirdGradient(SDL_Window *window, i16 blueoffset, i16 greenoffset)
{

    // TODO: ASSERT WINDOW NOT NULL
    // TODO: ASSERT BLUEOFFSET GREEN OFFSET < 255
    i32 retval = RETURN_FAILURE;

    SDL_Renderer *renderer = SDL_GetRenderer(window);
    if(!renderer)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_GetRenderer(): %s\n",
                __func__, SDL_GetError());
        return retval;
    }

    SDL_RendererInfo info;
    retval = SDL_GetRendererInfo(renderer, &info);
    if(retval < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_GetRendererInfo(): %s\n",
                __func__, SDL_GetError());
        return retval;
    }

    i32 pitch = GlobalBackBuffer.width * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888);

    u8 *row = (u8 *)GlobalBackBuffer.memory;
    for(int y = 0; y < GlobalBackBuffer.height; ++y)
    {
        u32 *pixel = (u32*)row;
        for(int x = 0; x < GlobalBackBuffer.width; ++x)
        {
            u8 blue = (x + blueoffset);
            u8 green = (y + greenoffset);
            *pixel++ = ((red << 16) | (green << 8) | blue);
        }
        row += pitch;
    }
    return RETURN_SUCCESS;
}

struct window_dimension
WindowGetDimension(SDL_Window *window)
{
    struct window_dimension result;
    SDL_GetWindowSize(window, &result.width, &result.height);
    return result;
}


void HandleController(void)
{
    f32 speed = 2.0f;
    for(int i = 0; i< MAX_CONTROLLERS; i++)
    {
        i32 retval = 0;
        bool up;
        bool down;
        bool left;
        bool right;
        bool back;
        bool start;
        bool lshoulder;
        bool rshoulder;
        bool buttonA;
        bool buttonB;
        bool buttonX;
        bool buttonY;
        i16  lstickX;
        i16  lstickY;
        if(ControllerHandles[i] != NULL && SDL_GameControllerGetAttached(ControllerHandles[i]))
        {
            up        = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_UP);
            down      = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_DOWN);
            left      = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_LEFT);
            right     = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
            back      = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_BACK);
            start     = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_START);
            lshoulder = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
            rshoulder = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
            buttonA   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_A);
            buttonB   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_B);
            buttonX   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_X);
            buttonY   = SDL_GameControllerGetButton(ControllerHandles[i], SDL_CONTROLLER_BUTTON_Y);
            lstickX   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTX); 
            lstickY   = SDL_GameControllerGetAxis(ControllerHandles[i], SDL_CONTROLLER_AXIS_LEFTY); 

        } else {
            // TODO: Controller not plugged int
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
        } else if (buttonY == true) {
            red += 1;
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

    }
    return;
}

i32
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
            // RumbleHandles[ControllerIndex] = SDL_HapticOpenFromJoystick(SDL_GameControllerGetJoystick(ControllerHandles[ControllerIndex]));
            RumbleHandles[ControllerIndex] = SDL_HapticOpen(i);
            if(SDL_HapticRumbleInit(RumbleHandles[ControllerIndex]) >= 0)
            {
                // DO NOTHING
            } else {
                SDL_HapticClose(RumbleHandles[ControllerIndex]);
                RumbleHandles[ControllerIndex] = NULL;
            }
            ControllerIndex++;
        }
    }
    return ControllerIndex;
}

void
GameControllersQuit()
{
    for (int i = 0; i < MAX_CONTROLLERS; i++)
    {
        if (ControllerHandles[i]) SDL_GameControllerClose(ControllerHandles[i]);
        if (RumbleHandles[i])     SDL_HapticClose(RumbleHandles[i]);
    }
}
