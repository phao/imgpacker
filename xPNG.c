#include <assert.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <png.h>
#include <SDL2/SDL.h>

#include "xPNG.h"

static const char *e_msg = "";

static int
png_colortype_from_surface(SDL_Surface *surface) {
	int colortype = PNG_COLOR_MASK_COLOR; /* grayscale not supported */

	if (surface->format->palette)
		colortype |= PNG_COLOR_MASK_PALETTE;
	else if (surface->format->Amask)
		colortype |= PNG_COLOR_TYPE_RGB_ALPHA ;

	return colortype;
}

static void
xpng_user_warn(png_structp ctx, png_const_charp str) {
  (void) ctx;

	e_msg = str;
}

static void
xpng_user_error(png_structp ctx, png_const_charp str) {
  (void) ctx;

	e_msg = str;
}

int
xpng_save_surface(const char *filename, SDL_Surface *surf) {
  assert(surf);
  assert(filename);
  assert(*filename);

	FILE *fp;
	png_structp png_ptr;
	png_infop info_ptr;
	int i, colortype;
	png_bytep *row_pointers;

	/* Opening output file */
	fp = fopen(filename, "wb");
	if (fp == NULL) {
		return X_PNG_IO_FAIL;
	}

	/* Initializing png structures and callbacks */
	png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
		NULL, xpng_user_error, xpng_user_warn);
	if (png_ptr == NULL) {
		return X_PNG_FAIL;
	}

	info_ptr = png_create_info_struct(png_ptr);
	if (info_ptr == NULL) {
		png_destroy_write_struct(&png_ptr, (png_infopp) NULL);
		return X_PNG_FAIL;
	}

	if (setjmp(png_jmpbuf(png_ptr))) {
		png_destroy_write_struct(&png_ptr, &info_ptr);
		fclose(fp);
		return X_PNG_FAIL;
	}

	png_init_io(png_ptr, fp);

	colortype = png_colortype_from_surface(surf);
	png_set_IHDR(png_ptr, info_ptr, surf->w, surf->h, 8, colortype,
    PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	/* Writing the image */
	png_write_info(png_ptr, info_ptr);
	png_set_packing(png_ptr);

	row_pointers = (png_bytep*) malloc(sizeof(png_bytep)*surf->h);
	for (i = 0; i < surf->h; i++) {
		row_pointers[i] = (png_bytep)(Uint8 *)surf->pixels + i*surf->pitch;
  }
	png_write_image(png_ptr, row_pointers);
	png_write_end(png_ptr, info_ptr);

	/* Cleaning out... */
	free(row_pointers);
	png_destroy_write_struct(&png_ptr, &info_ptr);
	fclose(fp);

	return X_PNG_OK;
}

const char *
xpng_strerror(int code) {
  switch (code) {
    case X_PNG_FAIL:
      return strerror(errno);
    case X_PNG_IO_FAIL:
      assert(e_msg);
      return e_msg;
  }
  return 0;
}
