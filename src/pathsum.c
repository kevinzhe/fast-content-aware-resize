/**
 * @file pathsum.c
 * @brief Compute least path sums of an energy map
 */

#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <x86intrin.h>

#include "car_internal.h"
#include "energy.h"
#include "pathsum.h"

_Static_assert(sizeof(enval) == sizeof(int32_t), "unexpected enval datatype size");

static enval min3(enval a, enval b, enval c);
static int min3idx(enval a, enval b, enval c);
static int min2idx(enval a, enval b);

void compute_pathsum(const energymap *in, energymap *result) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(result));
  assert(in->width == result->width);
  assert(in->height == result->height);
  
  size_t ww = in->width;
  size_t hh = in->height;

  // copy the first row first
  memcpy(&GET_PIXEL(result, 0, 0), &GET_PIXEL(in, 0, 0), sizeof(GET_PIXEL(result, 0, 0))*ww);

  // push the min val down
  for (size_t i = 1; i < hh; i++) {
    size_t j = 0;
    // do the first value
    {
      enval cc = GET_PIXEL(result, i-1, j+0);
      enval rr = GET_PIXEL(result, i-1, j+1);
      enval minval;
      if (cc <= rr) {
        minval = cc;
      } else {
        minval = rr;
      }
      GET_PIXEL(result, i, 0) = GET_PIXEL(in, i, j) + minval;
    }

    j++;

    // do the middle values
    size_t unroll = 4;
    size_t elts_per_vec = sizeof(__m256i) / sizeof(enval);
    assert(elts_per_vec == 8);
    for (; j+elts_per_vec*unroll <= ww; j += elts_per_vec*unroll) {
      __m256i curvals0 = _mm256_loadu_si256((void *)&GET_PIXEL(in, i, j+0*elts_per_vec));
      __m256i curvals1 = _mm256_loadu_si256((void *)&GET_PIXEL(in, i, j+1*elts_per_vec));
      __m256i curvals2 = _mm256_loadu_si256((void *)&GET_PIXEL(in, i, j+2*elts_per_vec));
      __m256i curvals3 = _mm256_loadu_si256((void *)&GET_PIXEL(in, i, j+3*elts_per_vec));

      __m256i minvals0 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+0*elts_per_vec-1)),
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+0*elts_per_vec+0))
            ),
            _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+0*elts_per_vec+1))
          );
      __m256i minvals1 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+1*elts_per_vec-1)),
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+1*elts_per_vec+0))
            ),
            _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+1*elts_per_vec+1))
          );
      __m256i minvals2 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+2*elts_per_vec-1)),
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+2*elts_per_vec+0))
            ),
            _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+2*elts_per_vec+1))
          );
      __m256i minvals3 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+3*elts_per_vec-1)),
              _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+3*elts_per_vec+0))
            ),
            _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+3*elts_per_vec+1))
          );

      _mm256_storeu_si256(
        (void *)&GET_PIXEL(result, i, j+0*elts_per_vec),
        _mm256_add_epi32(minvals0, curvals0)
      );
      _mm256_storeu_si256(
        (void *)&GET_PIXEL(result, i, j+1*elts_per_vec),
        _mm256_add_epi32(minvals1, curvals1)
      );
      _mm256_storeu_si256(
        (void *)&GET_PIXEL(result, i, j+2*elts_per_vec),
        _mm256_add_epi32(minvals2, curvals2)
      );
      _mm256_storeu_si256(
        (void *)&GET_PIXEL(result, i, j+3*elts_per_vec),
        _mm256_add_epi32(minvals3, curvals3)
      );
    }

    // finish up the remaining elements
    for (; j < ww; j++) {
      enval ll, cc, rr;
      cc = GET_PIXEL(result, i-1, j);
      if (j > 0) ll = GET_PIXEL(result, i-1, j-1);
      else ll = cc;
      if (j < ww-1) rr = GET_PIXEL(result, i-1, j+1);
      else rr = cc;
      assert(ll >= 0);
      assert(rr >= 0);
      assert(cc >= 0);
      GET_PIXEL(result, i, j) = GET_PIXEL(in, i, j) + min3(ll, cc, rr);
    }
  }
}

void find_minseam(const energymap *pathsum, size_t *result) {
  assert(IS_IMAGE(pathsum));
  assert(result);

  size_t ww = pathsum->width;
  size_t hh = pathsum->height;

  enval minval = GET_PIXEL(pathsum, hh-1, 0);
  size_t minidx = 0;
  for (size_t j = 0; j < ww; j++) {
    enval val = GET_PIXEL(pathsum, hh-1, j);
    if (val < minval) {
      minval = val;
      minidx = j;
    }
  }

  result[hh-1] = minidx;

  for (size_t i = hh-2; i != SIZE_MAX; i--) {
    size_t previdx = result[i+1];
    enval cc = GET_PIXEL(pathsum, i, previdx);
    int delta;
    if (previdx == 0) {
      enval rr = GET_PIXEL(pathsum, i, previdx+1);
      delta = min2idx(cc, rr);
    } else if (previdx == ww-1) {
      enval ll = GET_PIXEL(pathsum, i, previdx-1);
      delta = -min2idx(cc, ll);
    } else {
      enval ll = GET_PIXEL(pathsum, i, previdx-1);
      enval rr = GET_PIXEL(pathsum, i, previdx+1);
      delta = min3idx(ll, cc, rr);
    }
    size_t col = (size_t)((int64_t)(previdx) + delta);
    assert(col < ww);
    result[i] = col;
  }
}

static enval min3(enval a, enval b, enval c) {
  if (b <= a && b <= c) return b;
  if (a <= c) return a;
  return c;
}

static int min3idx(enval a, enval b, enval c) {
  if (b <= a && b <= c) return 0;
  if (a <= c) return -1;
  return 1;
}

static int min2idx(enval a, enval b) {
  if (a <= b) return 0;
  return 1;
}
