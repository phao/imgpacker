CC=gcc

COMMON_C_FLAGS=`sdl2-config --cflags` -Wall -Wextra -std=c99 -pedantic
DEBUG_C_FLAGS=-g3
OPTIMIZATION_C_FLAGS=-O0
CC_CMD=$(CC) $(COMMON_C_FLAGS) $(DEBUG_C_FLAGS) $(OPTIMIZATION_C_FLAGS)

LD=gcc
OBJS=Main.o BinPack2D.o xPNG.o
LIBS=`sdl2-config --libs` -lpng -lSDL2_image

.c.o:
	$(CC_CMD) -c $<

$(shell $(CC_CMD) -MM *.c > deps)
include deps

build: $(OBJS)
	$(LD) $(OBJS) -o imgpacker $(LIBS)
	rm -f deps

clean:
	rm -f $(OBJS) deps
