/**
 * @file energy.c
 * @brief Routines to create energy maps
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "car_internal.h"
#include "energy.h"

typedef struct {
  enval *data;
  enval magnitude;
  size_t width;
  size_t height;
  size_t buf_width;
  size_t buf_height;
} kern2d;

#define KERNEL_X (kern2d) {   \
  .data = (enval []) {        \
    -1, -2, -1,               \
     0,  0,  0,               \
     1,  2,  1                \
  },                          \
  .magnitude = 8,             \
  .width = 3,                 \
  .height = 3,                \
  .buf_width = 3,             \
  .buf_height = 3             \
}

#define KERNEL_Y (kern2d) {   \
  .data = (enval []) {        \
    -1,  0,  1,               \
    -2,  0,  2,               \
    -1,  0,  1                \
  },                          \
  .magnitude = 8,             \
  .width = 3,                 \
  .height = 3,                \
  .buf_width = 3,             \
  .buf_height = 3             \
}

static const size_t KERNEL_WIDTH = 3;
static const size_t KERNEL_HEIGHT = 3;


static void conv_pixel(const gray_image *in, energymap *out, size_t i, size_t j);


void compute_energymap_partial(const gray_image *in, energymap *out, const size_t *removed) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);
  assert(removed);

  size_t hh = in->height;
  size_t ww = in->width;
  size_t khh = KERNEL_HEIGHT;
  size_t kww = KERNEL_WIDTH;

  for (size_t i = 0; i < hh; i++) {
    size_t j0 = removed[i] - (kww / 2) - (khh - 1);
    for (size_t dj = 0; dj < kww - 1 + (khh-1)*2; dj++) {
      size_t j = j0 + dj;
      if (j < ww) {
        conv_pixel(in, out, i, j);
      }
    }
  }
}

void compute_energymap(const gray_image *in, energymap *out) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  size_t hh = in->height;
  size_t ww = in->width;

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < ww; j++) {
      conv_pixel(in, out, i, j);
    }
  }
}

static void conv_pixel(const gray_image *in, energymap *out, size_t i, size_t j) {
  size_t hh = in->height;
  size_t ww = in->width;
  size_t khh = KERNEL_HEIGHT;
  size_t kww = KERNEL_WIDTH;

  size_t i0;
  if      (i < khh/2)      i0 = 0;
  else if (i > hh-khh/2-1) i0 = hh-khh-1;
  else                     i0 = i-khh/2;

  size_t j0;
  if      (j < kww/2)      j0 = 0;
  else if (j > ww-kww/2-1) j0 = ww-kww-1;
  else                     j0 = j-kww/2;

  enval resultx0 = GET_PIXEL(in, i0+0, j0+0) * GET_PIXEL(&KERNEL_X, 0, 0);
  enval resulty0 = GET_PIXEL(in, i0+0, j0+0) * GET_PIXEL(&KERNEL_Y, 0, 0);
  enval resultx1 = GET_PIXEL(in, i0+0, j0+1) * GET_PIXEL(&KERNEL_X, 0, 1);
  enval resultx2 = GET_PIXEL(in, i0+0, j0+2) * GET_PIXEL(&KERNEL_X, 0, 2);
  enval resulty2 = GET_PIXEL(in, i0+0, j0+2) * GET_PIXEL(&KERNEL_Y, 0, 2);
  enval resulty3 = GET_PIXEL(in, i0+1, j0+0) * GET_PIXEL(&KERNEL_Y, 1, 0);
  enval resulty5 = GET_PIXEL(in, i0+1, j0+2) * GET_PIXEL(&KERNEL_Y, 1, 2);
  enval resultx6 = GET_PIXEL(in, i0+2, j0+0) * GET_PIXEL(&KERNEL_X, 2, 0);
  enval resulty6 = GET_PIXEL(in, i0+2, j0+0) * GET_PIXEL(&KERNEL_Y, 2, 0);
  enval resultx7 = GET_PIXEL(in, i0+2, j0+1) * GET_PIXEL(&KERNEL_X, 2, 1);
  enval resultx8 = GET_PIXEL(in, i0+2, j0+2) * GET_PIXEL(&KERNEL_X, 2, 2);
  enval resulty8 = GET_PIXEL(in, i0+2, j0+2) * GET_PIXEL(&KERNEL_Y, 2, 2);

  enval resultx = (resultx0 + resultx1) + (resultx2 + resultx6) + (resultx7 + resultx8);

  enval resulty = (resulty0 + resulty2) + (resulty3 + resulty5) + (resulty6 + resulty8);

  resultx /= KERNEL_X.magnitude;
  resultx /= 2;

  resulty /= KERNEL_X.magnitude;
  resulty /= 2;

  GET_PIXEL(out, i, j) = (enval) abs(resultx) + (enval) abs(resulty);
}
