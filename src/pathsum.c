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

static void compute_pathsum_row(const energymap *in, energymap *result,
                                size_t i, size_t j0, size_t n);
static enval min3(enval a, enval b, enval c);
static int min3idx(enval a, enval b, enval c);
static int min2idx(enval a, enval b);
static size_t min(size_t a, size_t b);
static size_t max(size_t a, size_t b);

void compute_pathsum(const energymap *in, energymap *result) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(result));
  assert(in->width == result->width);
  assert(in->height == result->height);
  
  size_t ww = in->width;
  size_t hh = in->height;

  // push the min val down
  for (size_t i = 0; i < hh; i++) {
    compute_pathsum_row(in, result, i, 0, ww);
  }
}

size_t compute_pathsum_partial(const energymap *in, energymap *result, size_t *removed) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(result));
  assert(in->width == result->width);
  assert(in->height == result->height);
  assert(removed);

  size_t ww = in->width;
  size_t hh = in->height;

  size_t j0 = ww;
  size_t j1 = 0;

  size_t total_size = 0;

  for (size_t i = 1; i < hh; i++) {
    j0 = min(j0, removed[i-1] > 0 ? removed[i-1] - 1 : 0);
    j1 = max(j1, removed[i-1] < ww ? removed[i-1] + 1 : ww);
    assert(j1 > j0);
    compute_pathsum_row(in, result, i, j0, j1-j0);
    total_size += (j1-j0) * sizeof(enval);
    if (j0 > 0) j0--;
    if (j1 < ww) j1++;
  }

  return total_size;
}

static void compute_pathsum_row(const energymap *in, energymap *result,
                                size_t i, size_t j0, size_t n) {
  size_t ww = in->width;

  size_t j = j0;

  // the first row is just a copy
  if (i == 0) {
    memcpy(&GET_PIXEL(result, i, j), &GET_PIXEL(in, i, j), sizeof(GET_PIXEL(result, 0, 0))*n);
    return;
  }

  // do the first value
  if (j == 0) {
    enval cc = GET_PIXEL(result, i-1, j+0);
    enval rr = GET_PIXEL(result, i-1, j+1);
    enval minval;
    if (cc <= rr) {
      minval = cc;
    } else {
      minval = rr;
    }
    GET_PIXEL(result, i, 0) = GET_PIXEL(in, i, j) + minval;
    j++;
  }

  // do the middle values
  size_t unroll = 3;
  size_t elts_per_vec = sizeof(__m256i) / sizeof(enval);
  assert(elts_per_vec == 8);

  // Left vector for min comparison
  // nned 3, since shifting last one will spill in to next iteration
  __m256i ll0 = _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+0*elts_per_vec-1));
  __m256i ll1 = _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+1*elts_per_vec-1));
  __m256i ll2 = _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+2*elts_per_vec-1));
  __m256i ll3 = _mm256_loadu_si256((void *)&GET_PIXEL(result, i-1, j+3*elts_per_vec-1));

  // shifting immediates for doing blends and shuffles/permutes
  __m256i shift1 = _mm256_set_epi32(0, 7, 6, 5, 4, 3, 2, 1);
  __m256i shift2 = _mm256_set_epi32(1, 0, 7, 6, 5, 4, 3, 2);

  enval *current = &GET_PIXEL(in, i, j);
  enval *res = &GET_PIXEL(result, i, j);
  enval *topleft = &GET_PIXEL(result, i-1, j-1);
  topleft += elts_per_vec*(unroll+1); // starts at the next iteration's addresses

  for (; j+elts_per_vec*unroll < ww && j+elts_per_vec*unroll <= j0+n; j += elts_per_vec*unroll) {
    // current energy values
    __m256i curvals0 = _mm256_loadu_si256((void *)(current+0*elts_per_vec));
    __m256i curvals1 = _mm256_loadu_si256((void *)(current+1*elts_per_vec));
    __m256i curvals2 = _mm256_loadu_si256((void *)(current+2*elts_per_vec));

    // Shift left vector by one element left to get center values
    __m256i cc0 = _mm256_blend_epi32(ll0, ll1, 0x1);
    cc0 = _mm256_permutevar8x32_epi32(cc0, shift1);

    __m256i cc1 = _mm256_blend_epi32(ll1, ll2, 0x1);
    cc1 = _mm256_permutevar8x32_epi32(cc1, shift1);

    __m256i cc2 = _mm256_blend_epi32(ll2, ll3, 0x1);
    cc2 = _mm256_permutevar8x32_epi32(cc2, shift1);

    // Shift left vector by two elements left to get right values
    __m256i rr0 = _mm256_blend_epi32(ll0, ll1, 0x3);
    rr0 = _mm256_permutevar8x32_epi32(rr0, shift2);

    __m256i rr1 = _mm256_blend_epi32(ll1, ll2, 0x3);
    rr1 = _mm256_permutevar8x32_epi32(rr1, shift2);

    __m256i rr2 = _mm256_blend_epi32(ll2, ll3, 0x3);
    rr2 = _mm256_permutevar8x32_epi32(rr2, shift2);

    // Get vector of minimum values
    __m256i minvals0 = _mm256_min_epi32(_mm256_min_epi32(ll0, cc0), rr0);
    __m256i minvals1 = _mm256_min_epi32(_mm256_min_epi32(ll1, cc1), rr1);
    __m256i minvals2 = _mm256_min_epi32(_mm256_min_epi32(ll2, cc2), rr2);

    // load the next iteration's top left vectors
    ll0 = ll3;
    ll1 = _mm256_loadu_si256((void *)(topleft+0*elts_per_vec));
    ll2 = _mm256_loadu_si256((void *)(topleft+1*elts_per_vec));
    ll3 = _mm256_loadu_si256((void *)(topleft+2*elts_per_vec));

    _mm256_storeu_si256((void *)(res+0*elts_per_vec), _mm256_add_epi32(minvals0, curvals0));
    _mm256_storeu_si256((void *)(res+1*elts_per_vec), _mm256_add_epi32(minvals1, curvals1));
    _mm256_storeu_si256((void *)(res+2*elts_per_vec), _mm256_add_epi32(minvals2, curvals2));

    current += elts_per_vec*unroll;
    res += elts_per_vec*unroll;
    topleft += elts_per_vec*unroll;
  }

  // finish up the remaining elements
  for (; j < ww && j < j0+n; j++) {
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

static size_t min(size_t a, size_t b) {
  if (a <= b) return a;
  return b;
}

static size_t max(size_t a, size_t b) {
  if (a >= b) return a;
  return b;
}
