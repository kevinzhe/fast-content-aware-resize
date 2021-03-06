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
  size_t buf_width;
  size_t buf_height;
  size_t buf_start;
} energymap;

double compute_energymap(const gray_image *in, energymap *out);

/**
 * @brief Recompute the energy for pixels that changed between iterations.
 * @param gray_image the image to compute the energy map for
 * @param out energy map from the last iteration, with the last seam removed
 * @param removed pixels that were removed in the last iteration
 */
double compute_energymap_partial(const gray_image *in, energymap *out,
                                 const size_t *removed);

#endif /* _ENERGY_H_ */
