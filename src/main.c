#include <stdint.h>
#include <stdbool.h>
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


bool HandleEvent(SDL_Event *event);

// GLOBAL STUFF FOR NOW;

int main(void) { 
    SDL_Window *window;
    SDL_Renderer *renderer;
    u16 screen_width = 800;
    u16 screen_height = 600;
    u32 exitval;
    int retval;
    int sdl_flags = SDL_INIT_VIDEO | SDL_INIT_JOYSTICK;
    int rnd_flags = SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC;

    exitval = EXIT_FAILURE;

    retval = SDL_Init(sdl_flags);
    if(retval < 0)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION, "SDL_Init: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
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
                __func__,
                SDL_GetError());
        goto __EXIT__;
    }

    renderer = SDL_CreateRenderer(window, -1, rnd_flags);
    if(!renderer)
    {
        SDL_LogError (SDL_LOG_CATEGORY_APPLICATION,
                "%s: SDL_CreateRenderer(): %s\n",
                __func__,
                SDL_GetError());
        goto __EXIT__;
    }


    bool quit = false;
    SDL_Event event;
    while(quit == false)
    {
        SDL_WaitEvent(&event);
        quit = HandleEvent(&event);
    }
    exitval = EXIT_SUCCESS;
    SDL_Log("Exiting\n");

__EXIT__:
    if (renderer)           SDL_DestroyRenderer(renderer);
    if (window)             SDL_DestroyWindow(window);
    SDL_Quit();
    return exitval;
}

bool HandleEvent(SDL_Event *event)
{
    bool quit;  
    switch(event->type)
    {
        case SDL_QUIT:
            SDL_Log("Window close requested\n");
            quit = true;
            break;

        case SDL_WINDOWEVENT:
            static bool isWhite = true;
            switch(event->window.event)
            {
                case SDL_WINDOWEVENT_RESIZED:
                    SDL_Log("Window Resized (%d, %d)\n", event->window.data1, event->window.data2);
                    break;
                case SDL_WINDOWEVENT_EXPOSED:
                {
                    SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
                    SDL_Renderer *renderer = SDL_GetRenderer(window);
                    SDL_Log("Window exposed!\n");
                    if(isWhite == true)
                    {
                        SDL_Log("White!\n");
                        SDL_SetRenderDrawColor(renderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
                        isWhite = false;
                    } else {
                        SDL_Log("Black!\n");
                        SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
                        isWhite = true;
                    }
                    SDL_RenderClear(renderer);
                    SDL_RenderPresent(renderer);
                } break;
            }
            break;

        case SDL_KEYDOWN:
            switch(event->key.keysym.sym)
            {
                case SDLK_ESCAPE:
                case SDLK_q: quit = true; break;
                case SDLK_RETURN:
                    if(event->key.keysym.mod & KMOD_LALT) {
                        SDL_Log("ALT-ENTER pressed\n");
                    }
                    break;
            }
            break;
    }
    return quit;
}
