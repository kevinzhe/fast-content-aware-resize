/**
 * @file car_internal.h
 * @brief Internal declarations for the content aware resize library
 */

#ifndef _CAR_INTERNAL_H_
#define _CAR_INTERNAL_H_

#include <car.h>
#include <stddef.h>
#include <x86intrin.h>

typedef struct {
  pixval *data;
  size_t width;
  size_t height;
  size_t buf_width;
  size_t buf_height;
} gray_image;

bool is_rgb_image(const rgb_image *img);
bool is_gray_image(const gray_image *img);

#define GET_CYCLE_COUNT() __rdtsc()

#define INITIALIZE_IMAGE(img, _width, _height)    \
  do {                             \
    (img)->width = (_width);       \
    (img)->height = (_height);     \
    (img)->buf_width = (_width);   \
    (img)->buf_height = (_height); \
    (img)->data = malloc(sizeof(*((img)->data)) * (_width)*(_height)); \
  } while (0)

#endif /* _CAR_INTERNAL_H_ */
