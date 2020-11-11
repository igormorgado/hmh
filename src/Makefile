CC=gcc
DIRECTIVES=-DGAME_INTERNAL -DGAME_SLOW 
DIRECTIVES=-DGAME_INTERNAL
## -DCASEYDEFS
## DIRECTIVES=-DDEBUG -DGAME_INTERNAL -DGAME_SLOW
	## $(CC) $(DIRECTIVES) $(CFLAGS) -c -fPIC game.c -o game.so.o
	## $(CC) $(DIRECTIVES) $(CFLAGS) -shared -o game.so.tmp  game.so.o
## TODO(Igor): --pedantic
CFLAGS=-g -Og -Wall -Werror -Wno-unused-function -Wno-unused-variable

build: game.so sdl_game

all: clean game.so sdl_game

game.so:
	$(CC) $(DIRECTIVES) $(CFLAGS) -fPIC -shared game.c -o game.so.tmp 
	mv game.so.tmp game.so

sdl_game:
	$(CC) $(DIRECTIVES) $(CFLAGS) sdl_game.c -I. -o sdl_game.x86_64 -ldl $(shell pkg-config --libs --cflags sdl2) -lm


clean:
	rm -f game.so game.so.tmp game game.x86_65 test.out game.so.o game.o *.hmi sdl_game *.o sdl_game.x86_64
