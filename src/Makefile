CC=gcc
DIRECTIVES=-DGAME_INTERNAL
# -DGAME_SLOW  -DDEBUG
CFLAGS=-Ofast -Wall -Werror -Wno-unused-function -Wno-unused-variable
SDL_FLAGS = $(shell pkg-config --libs --cflags sdl2)
LDFLAGS = -lm
TARGETS = game.so sdl_game


build: ${TARGETS}

all: clean build

game.so: game.c ../include/game.h ../include/game_platform.h
	$(CC) $(DIRECTIVES) $(CFLAGS) -fPIC -I. -I../include -shared game.c -o game.so

sdl_game: sdl_game.c ../include/sdl_game.h ../include/game_platform.h
	$(CC) $(DIRECTIVES) $(CFLAGS) sdl_game.c -I. -I../include -o sdl_game -ldl $(SDL_FLAGS) $(LDFLAGS)


cclean:
	rm -f *.hmi *.o *.tmp

clean:
	rm -f *.o *.so *.tmp *.hmi *.x86_64 *.x86_32 sdl_game
