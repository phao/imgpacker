CC=gcc

UNIT_BASE_FLAGS=`sdl2-config --cflags` -Wall -Wextra -std=c99 -pedantic
UNIT_DEBUG_FLAGS=-g3
UNIT_OPTIMIZATION_FLAGS=-O0
UNIT_CMD=$(CC) $(UNIT_BASE_FLAGS) $(UNIT_DEBUG_FLAGS) $(UNITOPTIMIZATION_FLAGS)

LD=gcc
OBJS=Main.o BinPack2D.o xPNG.o
LIBS=`sdl2-config --libs` -lpng -lSDL2_image

.c.o:
	$(UNIT_CMD) -c $<

$(shell $(UNIT_CMD) -MM *.c > deps)
include deps

build: $(OBJS)
	$(LD) $(OBJS) -o imgpacker $(LIBS)
	rm -f deps

clean:
	rm -f $(OBJS) deps
