/**
 * @file util.c
 * @brief CAR library utilities
 */

#include <stdint.h>
#include <x86intrin.h>

#include <car.h>
#include "car_internal.h"

bool is_rgb_image(const rgb_image *img) {
  return img
      && img->data
      && img->width > 0
      && img->height > 0;
}

bool is_gray_image(const gray_image *img) {
  return img
      && img->data
      && img->width > 0
      && img->height > 0;
}

uint64_t get_cycle_count(void) {
  return __rdtsc();
}
