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

enum RETURN_STATUS {
    RETURN_FAILURE = -1,
    RETURN_SUCCESS = 0,
    RETURN_EXIT = 255
};

i32  InitGame(void);
void ExitGame(void);
i32  HandleEvent(const SDL_Event *event);
i32  HandleWindow(const SDL_Event *event);
i32  HandleKeyDown(const SDL_Event *event);

static int ResizeWindow(SDL_Window *window, int width, int height);
static int UpdateWindow(SDL_Window *window);

// GLOBAL STUFF FOR NOW;
SDL_Window *window;
SDL_Renderer *renderer;
u16 screen_width = 800;
u16 screen_height = 600;

static SDL_Texture *texture;
static void *BitmapMemory;
static i32 BitmapWidth;


int main(void) { 
    i32 exitval;
    i32 retval;
    bool quit = false;

    retval = InitGame();
    if(retval < 0) {
        exitval = EXIT_FAILURE;
        goto __EXIT__;
    }

    while(quit == false)
    {
        SDL_Event event;
        while(SDL_PollEvent(&event)) 
        {
           retval = HandleEvent(&event);
           if(retval == RETURN_EXIT) 
           {
               quit = true;
           }
        }

        // RenderWeirdGradient(XOffset, YOffset);
        // UpdateWindow(window, renderer);
    }

    exitval = EXIT_SUCCESS;
__EXIT__:
     ExitGame();
     return exitval;
}


i32 InitGame(void)
{
    i32 retval;
    int sdl_flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
    int rnd_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;

    retval = SDL_Init(sdl_flags);
    if(retval < 0)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, 
                      "%s: SDL_Init(): %s\n", 
                      __func__, SDL_GetError());
        return retval;
    }

    window = SDL_CreateWindow("Gamepads and viewports",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            screen_width,
            screen_height,
            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if(!window)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_CreateWindow(): %s\n",
                __func__, SDL_GetError());
        return RETURN_FAILURE;
    }

    renderer = SDL_CreateRenderer(window, -1, rnd_flags);
    if(!renderer)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_CreateRenderer(): %s\n",
                __func__,
                SDL_GetError());
        return RETURN_FAILURE;
    }

    return RETURN_SUCCESS;
}


void ExitGame(void)
{
    SDL_Log("Exiting\n");
    if (renderer)           SDL_DestroyRenderer(renderer);
    if (window)             SDL_DestroyWindow(window);
    SDL_Quit();
    return; 
}


i32 HandleEvent(const SDL_Event *event)
{
    i32 retval;
    switch(event->type)
    {
        case SDL_QUIT:          retval = RETURN_EXIT; break;
        case SDL_WINDOWEVENT:   retval = HandleWindow(event); break;
        case SDL_KEYDOWN:       retval = HandleKeyDown(event); break;
        default: break;
    }
    return retval;
}


i32 HandleWindow(const SDL_Event *event)
{
    switch(event->window.event)
    {
        case SDL_WINDOWEVENT_MOVED:
        {} break;

        case SDL_WINDOWEVENT_RESIZED:
        { /* handled in SIZE_CHANGED */ } break;

        case SDL_WINDOWEVENT_SIZE_CHANGED:
        {
            SDL_Log("%s: Window size changed (%d, %d)\n",
                    __func__, event->window.data1, event->window.data2);

            SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
            ResizeWindow(window, event->window.data1, event->window.data2);
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
            SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
            UpdateWindow(window);
        } break;

        case SDL_WINDOWEVENT_CLOSE:
        {
            SDL_Log("%s: Window Close requested\n", __func__);
            return RETURN_EXIT;
        } break;

        case SDL_WINDOWEVENT_TAKE_FOCUS:
        {
            SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
            SDL_RaiseWindow(window);
        } break;

        default:
        {
            SDL_Log("%s: Window event not handled: %d\n", __func__, event->window.event);
        } break;
    }
    return RETURN_SUCCESS;
}


i32 HandleKeyDown(const SDL_Event *event)
{
    switch(event->key.keysym.sym)
    {
        case SDLK_ESCAPE: return RETURN_EXIT; break;
        case SDLK_q: return RETURN_EXIT; break;
        case SDLK_RETURN:
            if(event->key.keysym.mod & KMOD_LALT) {
                SDL_Log("ALT-ENTER pressed\n");
            }
            break;
        default: break;
    }
    return RETURN_SUCCESS;
}

static int ResizeWindow(SDL_Window *window, int width, int height)
{
    SDL_Renderer *renderer = SDL_GetRenderer(window);

    if(texture)
    {
        SDL_DestroyTexture(texture);
    }
    texture = SDL_CreateTexture(renderer, 
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                width,
                                height);
    if(!texture)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, 
                      "%s: SDL_CreateTexture(): %s\n", 
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }

    if(BitmapMemory)
    {
        free(BitmapMemory);
        //munmap(BitmapMemory, width * height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));
    }
    // Later benchmark vs mmap
    BitmapMemory = malloc(width * height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));
    //BitmapMemory = mmap(NULL,
    //                    width * height * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888),
    //                    PROT_READ | PROT_WRITE,
    //                    MAP_ANONYMOUS | MAP_PRIVATE,
    //                    -1,
    //                    0);
    if(!BitmapMemory)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, 
                      "%s: malloc(BitmapMemory): %s\n", 
                      __func__, SDL_GetError());
        return RETURN_FAILURE;
    }
    BitmapWidth = width;
    return RETURN_SUCCESS;
}

static int UpdateWindow(SDL_Window *window)
{
    i32 retval = RETURN_FAILURE;

    if(!texture) {
        return retval;
    }

    retval = SDL_UpdateTexture(texture,
                               NULL, 
                               BitmapMemory, 
                               BitmapWidth * SDL_BYTESPERPIXEL(SDL_PIXELFORMAT_ARGB8888));

    if(retval < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_UpdateTexture(): %s\n",
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

    retval = SDL_RenderCopy(renderer, texture, NULL, NULL);
    if(retval < 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_RenderCopy(): %s\n",
                __func__, SDL_GetError());
        return retval;
    }

    SDL_RenderPresent(renderer);
    return RETURN_SUCCESS;
}
