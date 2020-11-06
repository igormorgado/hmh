all:
	gcc -ggdb -DDEBUG -o hmh sdl_game.c  -I. $(shell pkg-config --libs --cflags sdl2) -lm

clean:
	rm -f hmh
