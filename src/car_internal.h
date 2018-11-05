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
  size_t buf_start;
} gray_image;

#define IS_IMAGE(img) (                       \
       (img)                                  \
    && (img)->data                            \
    && (img)->width > 0                       \
    && (img)->height > 0                      \
    && (img)->buf_width > 0                   \
    && (img)->buf_height > 0                  \
    && (img)->buf_width >= (img)->width       \
    && (img)->buf_height >= (img)->height     \
    && (img)->buf_start < (img)->buf_width    \
  )

#define GET_CYCLE_COUNT() __rdtsc()

#define INITIALIZE_IMAGE(img, _width, _height)    \
  do {                             \
    (img)->width = (_width);       \
    (img)->height = (_height);     \
    (img)->buf_width = (_width);   \
    (img)->buf_height = (_height); \
    (img)->buf_start = 0;          \
    (img)->data = malloc(sizeof(*((img)->data)) * (_width)*(_height)); \
  } while (0)

#define GET_PIXEL(img, row, col) ((img)->data[(row)*((img)->buf_width) + (col) + (img)->buf_start])

#endif /* _CAR_INTERNAL_H_ */
