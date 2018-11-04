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

static const kern2d KERNEL_X = {
  .data = (enval []) {
    -1, -2, -1,
     0,  0,  0,
     1,  2,  1
  },
  .magnitude = 8,
  .width = 3,
  .height = 3,
  .buf_width = 3,
  .buf_height = 3
};

static const kern2d KERNEL_Y = {
  .data = (enval []) {
    -1,  0,  1,
    -2,  0,  2,
    -1,  0,  1
  },
  .magnitude = 8,
  .width = 3,
  .height = 3,
  .buf_width = 3,
  .buf_height = 3
};

static void conv2d(const kern2d *kernel, const gray_image *in,
                   energymap *out, enval scale, bool zero);
static void conv2d_partial(const kern2d *kernel, const gray_image *in,
                    energymap *out, enval scale, bool zero, const size_t *removed);
static void conv_pixel(const kern2d *kernel, const gray_image *in,
                energymap *out, enval scale, bool zero, size_t i, size_t j);


void compute_energymap_partial(const gray_image *in, energymap *out,
                              const size_t *removed) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  conv2d_partial(&KERNEL_X, in, out, 2, true, removed);
  conv2d_partial(&KERNEL_Y, in, out, 2, false, removed);
}

static void conv2d_partial(const kern2d *kernel, const gray_image *in,
                    energymap *out, enval scale, bool zero, const size_t *removed) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);
  assert(removed);

  size_t hh = in->height;
  size_t ww = in->width;

  for (size_t i = 0; i < hh; i++) {
    size_t j0 = removed[i] - (kernel->width / 2) - (kernel->height - 1);
    for (size_t dj = 0; dj < kernel->width - 1 + (kernel->height-1)*2; dj++) {
      size_t j = j0 + dj;
      if (j < ww) {
        conv_pixel(kernel, in, out, scale, zero, i, j);
      }
    }
  }
}

void compute_energymap(const gray_image *in, energymap *out) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  conv2d(&KERNEL_X, in, out, 2, true);
  conv2d(&KERNEL_Y, in, out, 2, false);
}

static void conv2d(const kern2d *kernel, const gray_image *in,
                   energymap *out, enval scale, bool zero) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  size_t hh = in->height;
  size_t ww = in->width;

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < ww; j++) {
      conv_pixel(kernel, in, out, scale, zero, i, j);
    }
  }
}

static void conv_pixel(const kern2d *kernel, const gray_image *in,
                energymap *out, enval scale, bool zero, size_t i, size_t j) {
  size_t hh = in->height;
  size_t ww = in->width;
  size_t khh = kernel->height;
  size_t kww = kernel->width;

  size_t i0;
  if      (i < khh/2)      i0 = 0;
  else if (i > hh-khh/2-1) i0 = hh-khh-1;
  else                     i0 = i-khh/2;

  size_t j0;
  if      (j < kww/2)      j0 = 0;
  else if (j > ww-kww/2-1) j0 = ww-kww-1;
  else                     j0 = j-kww/2;

  enval result = 0;
  for (size_t ii = 0; ii < khh; ii++) {
    for (size_t jj = 0; jj < kww; jj++) {
      enval in_val = GET_PIXEL(in, i0+ii, j0+jj);
      enval kern_val = GET_PIXEL(kernel, ii, jj);
      result += in_val * kern_val;
    }
  }

  result /= kernel->magnitude;
  result /= scale;

  if (zero) {
    GET_PIXEL(out, i, j) = 0;
  }

  GET_PIXEL(out, i, j) += (enval) abs(result);
}
