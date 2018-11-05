/**
 * @file car.h
 * @brief Public interface for content aware resize library
 */

#ifndef _CAR_H_
#define _CAR_H_

#include <stddef.h>
#include <stdint.h>

typedef uint8_t pixval;

typedef struct {
  pixval red;
  pixval green;
  pixval blue;
} rgb_pixel;

typedef struct {
  rgb_pixel *data;
  size_t width;
  size_t height;
  size_t buf_width;
  size_t buf_height;
  size_t buf_start;
} rgb_image;

int seam_carve_baseline(const rgb_image *in, rgb_image *out);

#endif /* _CAR_H_ */
