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
  enval *kernel;
  size_t width;
  size_t height;
} kern2d;

static const kern2d KERNEL_X = {
  .kernel = (enval []) {
    -1, -2, -1,
     0,  0,  0,
     1,  2,  1
  },
  .width = 3,
  .height = 3
};

static const kern2d KERNEL_Y = {
  .kernel = (enval []) {
    -1,  0,  1,
    -2,  0,  2,
    -1,  0,  1
  },
  .width = 3,
  .height = 3
};

static void conv2d(const kern2d *kernel, const gray_image *in,
                   energymap *out, enval scale);

void compute_energymap(const gray_image *in, energymap *out) {
  assert(is_gray_image(in));
  assert(is_energymap(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  conv2d(&KERNEL_X, in, out, 2);
  conv2d(&KERNEL_Y, in, out, 2);
}

static void conv2d(const kern2d *kernel, const gray_image *in,
                   energymap *out, enval scale) {
  assert(is_gray_image(in));
  assert(is_energymap(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  enval kern_mag = 0;
  for (size_t i = 0; i < kernel->width * kernel->height; i++) {
    kern_mag += abs(kernel->kernel[i]);
  }

  size_t hh = in->height;
  size_t ww = in->width;
  size_t khh = kernel->height;
  size_t kww = kernel->width;

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < ww; j++) {
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
          enval in_val = in->data[(i0+ii)*ww+j0+jj];
          enval kern_val = kernel->kernel[ii*kww+jj];
          result += in_val * kern_val;
        }
      }

      result /= kern_mag;
      result /= scale;

      out->data[i*ww+j] += (enval) abs(result);
    }
  }
}

bool is_energymap(const energymap *map) {
  return map
      && map->width > 0
      && map->height > 0
      && map->data;
}
