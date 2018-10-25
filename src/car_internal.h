/**
 * @file car_internal.h
 * @brief Internal declarations for the content aware resize library
 */

#ifndef _CAR_INTERNAL_H_
#define _CAR_INTERNAL_H_

#include <car.h>
#include <stddef.h>

typedef struct {
  pixval *data;
  size_t width;
  size_t height;
} gray_image;

bool is_rgb_image(const rgb_image *img);
bool is_gray_image(const gray_image *img);

uint64_t get_cycle_count(void);

#endif /* _CAR_INTERNAL_H_ */
