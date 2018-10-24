/**
 * @file energy.h
 * @brief Declarations for helpers that create the energy map
 */

#ifndef _ENERGY_H_
#define _ENERGY_H_

#include <stddef.h>
#include <stdint.h>

typedef int32_t enval;

typedef struct {
  enval *data;
  size_t width;
  size_t height;
} energymap;

void compute_energymap(const gray_image *in, energymap *out);

bool is_energymap(const energymap *map);

#endif /* _ENERGY_H_ */
