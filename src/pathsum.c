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
  assert(is_energymap(in));
  assert(is_energymap(result));
  assert(in->width == result->width);
  assert(in->height == result->height);
  
  size_t ww = in->width;
  size_t hh = in->height;

  // copy the first row first
  memcpy(result->data, in->data, sizeof(result->data[0]) * ww);

  // push the min val down
  for (size_t i = 1; i < hh; i++) {
    size_t j = 0;
    // do the first value
    {
      enval cc = result->data[(i-1)*ww+j+0];
      enval rr = result->data[(i-1)*ww+j+1];
      enval minval;
      if (cc <= rr) {
        minval = cc;
      } else {
        minval = rr;
      }
      result->data[i*ww] = in->data[i*ww+j] + minval;
    }

    j++;

    // do the middle values
    size_t unroll = 4;
    size_t elts_per_vec = sizeof(__m256i) / sizeof(enval);
    assert(elts_per_vec == 8);
    for (; j+elts_per_vec*unroll < ww; j += elts_per_vec*unroll) {
      __m256i curvals0 = _mm256_loadu_si256((void *)&in->data[i*ww+j+0*elts_per_vec]);
      __m256i curvals1 = _mm256_loadu_si256((void *)&in->data[i*ww+j+1*elts_per_vec]);
      __m256i curvals2 = _mm256_loadu_si256((void *)&in->data[i*ww+j+2*elts_per_vec]);
      __m256i curvals3 = _mm256_loadu_si256((void *)&in->data[i*ww+j+3*elts_per_vec]);

      __m256i minvals0 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+0*elts_per_vec-1]),
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+0*elts_per_vec+0])
            ),
            _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+0*elts_per_vec+1])
          );
      __m256i minvals1 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+1*elts_per_vec-1]),
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+1*elts_per_vec+0])
            ),
            _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+1*elts_per_vec+1])
          );
      __m256i minvals2 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+2*elts_per_vec-1]),
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+2*elts_per_vec+0])
            ),
            _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+2*elts_per_vec+1])
          );
      __m256i minvals3 =
          _mm256_min_epi32(
            _mm256_min_epi32(
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+3*elts_per_vec-1]),
              _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+3*elts_per_vec+0])
            ),
            _mm256_loadu_si256((void *)&result->data[(i-1)*ww+j+3*elts_per_vec+1])
          );

      _mm256_storeu_si256(
        (void *)&result->data[i*ww+j+0*elts_per_vec],
        _mm256_add_epi32(minvals0, curvals0)
      );
      _mm256_storeu_si256(
        (void *)&result->data[i*ww+j+1*elts_per_vec],
        _mm256_add_epi32(minvals1, curvals1)
      );
      _mm256_storeu_si256(
        (void *)&result->data[i*ww+j+2*elts_per_vec],
        _mm256_add_epi32(minvals2, curvals2)
      );
      _mm256_storeu_si256(
        (void *)&result->data[i*ww+j+3*elts_per_vec],
        _mm256_add_epi32(minvals3, curvals3)
      );
    }

    // finish up the remaining elements
    for (; j < ww; j++) {
      enval ll, cc, rr;
      cc = result->data[(i-1)*ww+j];
      if (j > 0) ll = result->data[(i-1)*ww+j-1];
      else ll = cc;
      if (j < ww-1) rr = result->data[(i-1)*ww+j+1];
      else rr = cc;
      assert(ll >= 0);
      assert(rr >= 0);
      assert(cc >= 0);
      result->data[i*ww+j] = in->data[i*ww+j] + min3(ll, cc, rr);
    }
  }
}

void find_minseam(const energymap *pathsum, size_t *result) {
  assert(is_energymap(pathsum));
  assert(result);

  size_t ww = pathsum->width;
  size_t hh = pathsum->height;

  enval minval = pathsum->data[(hh-1)*ww];
  size_t minidx = 0;
  for (size_t j = 0; j < ww; j++) {
    enval val = pathsum->data[(hh-1)*ww+j];
    if (val < minval) {
      minval = val;
      minidx = j;
    }
  }

  result[hh-1] = minidx;

  for (size_t i = hh-2; i != SIZE_MAX; i--) {
    size_t previdx = result[i+1];
    enval cc = pathsum->data[i*ww+previdx];
    int delta;
    if (previdx == 0) {
      enval rr = pathsum->data[i*ww+previdx+1];
      delta = min2idx(cc, rr);
    } else if (previdx == ww-1) {
      enval ll = pathsum->data[i*ww+previdx-1];
      delta = -min2idx(cc, ll);
    } else {
      enval ll = pathsum->data[i*ww+previdx-1];
      enval rr = pathsum->data[i*ww+previdx+1];
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
