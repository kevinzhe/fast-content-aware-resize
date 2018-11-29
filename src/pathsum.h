/**
 * @file pathsum.h
 * @brief Compute and manipulate path sums from an energy map
 */

#ifndef _PATHSUM_H_
#define _PATHSUM_H_

#include "car_internal.h"
#include "energy.h"

void compute_pathsum(const energymap *in, energymap *result);

size_t compute_pathsum_partial(const energymap *in, energymap *result, size_t *removed);

void find_minseam(const energymap *pathsum, size_t *result);

#endif /* _PATHSUM_H_ */
