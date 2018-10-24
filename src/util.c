/**
 * @file util.c
 * @brief CAR library utilities
 */

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
