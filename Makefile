CC=gcc
OUT_FILE=imgpacker

UNIT_BASE_FLAGS=`sdl2-config --cflags` -Wall -Wextra -std=c99 -pedantic
UNIT_DEBUG_FLAGS=-g3
UNIT_OPTIMIZATION_FLAGS=-O0
UNIT_CMD=$(CC) $(UNIT_BASE_FLAGS) $(UNIT_DEBUG_FLAGS) $(UNIT_OPTIMIZATION_FLAGS)

LD=gcc
LD_FLAGS=
OBJS=Main.o BinPack2D.o xPNG.o
LIBS=`sdl2-config --libs` -lpng -lSDL2_image

.c.o:
	$(UNIT_CMD) -c $<

$(shell $(UNIT_CMD) -MM *.c > deps)
include deps

build: $(OBJS)
	$(LD) $(LD_FLAGS) $(OBJS) -o $(OUT_FILE) $(LIBS)
	rm -f deps

clean:
	rm -f $(OBJS) deps $(OUT_FILE)
