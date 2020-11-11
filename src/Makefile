CC=gcc
DIRECTIVES=-DGAME_INTERNAL -DGAME_SLOW 
## -DCASEYDEFS
## DIRECTIVES=-DDEBUG -DGAME_INTERNAL -DGAME_SLOW
##	$(CC) $(DIRECTIVES) $(CFLAGS) game.c -fPIC -shared -o game.so.tmp 
## TODO(Igor): --pedantic
CFLAGS=-g -Og -Wall -Werror -Wno-unused-function -Wno-unused-variable

build: game.so game

all: clean game.so game

game.so:
	$(CC) $(DIRECTIVES) $(CFLAGS) -c -fpic game.c -o game.so.o
	$(CC) $(DIRECTIVES) $(CFLAGS) -shared -o game.so.tmp  game.so.o
	mv game.so.tmp game.so

game:
	$(CC) $(DIRECTIVES) $(CFLAGS) sdl_game.c -I. -o game.x86_64 -ldl $(shell pkg-config --libs --cflags sdl2) -lm


clean:
	rm -f game.so game.so.tmp game game.x86_65 test.out game.so.o game.o *.hmi
