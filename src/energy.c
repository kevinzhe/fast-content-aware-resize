/**
 * @file energy.c
 * @brief Routines to create energy maps
 */

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include "car_internal.h"
#include "energy.h"

typedef struct {
  enval *data;
  enval magnitude;
  size_t width;
  size_t height;
  size_t buf_width;
  size_t buf_height;
  size_t buf_start;
} kern2d;

#define KERNEL_X ((kern2d) {  \
  .data = (enval []) {        \
    -1, -2, -1,               \
     0,  0,  0,               \
     1,  2,  1                \
  },                          \
  .magnitude = 8,             \
  .width = 3,                 \
  .height = 3,                \
  .buf_width = 3,             \
  .buf_height = 3,            \
  .buf_start = 0              \
})

#define KERNEL_Y ((kern2d) {  \
  .data = (enval []) {        \
    -1,  0,  1,               \
    -2,  0,  2,               \
    -1,  0,  1                \
  },                          \
  .magnitude = 8,             \
  .width = 3,                 \
  .height = 3,                \
  .buf_width = 3,             \
  .buf_height = 3,            \
  .buf_start = 0              \
})

static const size_t KERNEL_WIDTH = 3;
static const size_t KERNEL_HEIGHT = 3;


static void conv_pixel(const gray_image *in, energymap *out, size_t i, size_t j);
static void conv_pixel_vec(const gray_image *in, energymap *out, size_t i, size_t j, size_t len);

#define LOAD_EIGHT_UNSIGNED_BYTES(data) \
  (_mm256_cvtepu8_epi32(_mm_loadl_epi64((const __m128i *)(data))))

void compute_energymap_partial(const gray_image *in, energymap *out, const size_t *removed) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);
  assert(removed);

  size_t hh = in->height;
  size_t khh = KERNEL_HEIGHT;
  size_t kww = KERNEL_WIDTH;

  size_t vec_width = 8;

  for (size_t i = 0; i < hh; i++) {
    size_t j0 = removed[i] - (kww/2) - (khh-1)-1;
    conv_pixel_vec(in, out, i, j0, vec_width);
  }
}

void compute_energymap(const gray_image *in, energymap *out) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  size_t hh = in->height;
  size_t ww = in->width;

  for (size_t i = 0; i < hh; i++) {
    conv_pixel_vec(in, out, i, 0, ww);
  }
}

static void conv_pixel(const gray_image *in, energymap *out, size_t i, size_t j) {
  size_t hh = in->height;
  size_t ww = in->width;
  size_t khh = KERNEL_HEIGHT;
  size_t kww = KERNEL_WIDTH;

  size_t i0;
  if      (i < khh/2)      i0 = 0;
  else if (i > hh-khh/2-1) i0 = hh-khh-1;
  else                     i0 = i-khh/2;

  size_t j0;
  if      (j < kww/2)      j0 = 0;
  else if (j > ww-kww/2-1) j0 = ww-kww-1;
  else                     j0 = j-kww/2;

  enval resultx0 = GET_PIXEL(in, i0+0, j0+0) * GET_PIXEL(&KERNEL_X, 0, 0);
  enval resulty0 = GET_PIXEL(in, i0+0, j0+0) * GET_PIXEL(&KERNEL_Y, 0, 0);
  enval resultx1 = GET_PIXEL(in, i0+0, j0+1) * GET_PIXEL(&KERNEL_X, 0, 1);
  enval resultx2 = GET_PIXEL(in, i0+0, j0+2) * GET_PIXEL(&KERNEL_X, 0, 2);
  enval resulty2 = GET_PIXEL(in, i0+0, j0+2) * GET_PIXEL(&KERNEL_Y, 0, 2);
  enval resulty3 = GET_PIXEL(in, i0+1, j0+0) * GET_PIXEL(&KERNEL_Y, 1, 0);
  enval resulty5 = GET_PIXEL(in, i0+1, j0+2) * GET_PIXEL(&KERNEL_Y, 1, 2);
  enval resultx6 = GET_PIXEL(in, i0+2, j0+0) * GET_PIXEL(&KERNEL_X, 2, 0);
  enval resulty6 = GET_PIXEL(in, i0+2, j0+0) * GET_PIXEL(&KERNEL_Y, 2, 0);
  enval resultx7 = GET_PIXEL(in, i0+2, j0+1) * GET_PIXEL(&KERNEL_X, 2, 1);
  enval resultx8 = GET_PIXEL(in, i0+2, j0+2) * GET_PIXEL(&KERNEL_X, 2, 2);
  enval resulty8 = GET_PIXEL(in, i0+2, j0+2) * GET_PIXEL(&KERNEL_Y, 2, 2);

  enval resultx = (resultx0 + resultx1) + (resultx2 + resultx6) + (resultx7 + resultx8);
  enval resulty = (resulty0 + resulty2) + (resulty3 + resulty5) + (resulty6 + resulty8);

  resultx = abs(resultx);
  resulty = abs(resulty);

  resultx /= KERNEL_X.magnitude*2;
  resulty /= KERNEL_Y.magnitude*2;

  GET_PIXEL(out, i, j) = resultx + resulty;
}

static void conv_pixel_vec(const gray_image *in, energymap *out, size_t i, size_t j, size_t len) {
  assert(len > 0);
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  const size_t ww = in->width;
  const size_t hh = in->height;
  const size_t kww = KERNEL_WIDTH;
  const size_t khh = KERNEL_HEIGHT;

  const size_t vec_width = 8;

  const size_t j0 = j;
  const size_t j1 = j0 + len;

  // if we're on the top or bottom edge, don't vectorize
  if (i < khh/2 || i > hh-khh/2-1) {
    for (; j < j1; j++) {
      conv_pixel(in, out, i, j);
    }

  } else {
    // do the left edge
    for (; j < j1 && j < kww/2; j++) {
      conv_pixel(in, out, i, j);
    }

    // do the middle
    for (; j+vec_width <= j1 && j+vec_width < ww-kww/2; j += vec_width) {
      // load the image pixel values
      __m256i pixvals0 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i-1, j-1));
      __m256i pixvals1 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i-1, j  ));
      __m256i pixvals2 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i-1, j+1));
      __m256i pixvals3 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i  , j-1));
      __m256i pixvals4 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i  , j  ));
      __m256i pixvals5 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i  , j+1));
      __m256i pixvals6 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i+1, j-1));
      __m256i pixvals7 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i+1, j  ));
      __m256i pixvals8 = LOAD_EIGHT_UNSIGNED_BYTES(&GET_PIXEL(in, i+1, j+1));

      // load the x kernel values
      __m256i kernvalsx0 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 0, 0));
      __m256i kernvalsx1 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 0, 1));
      __m256i kernvalsx2 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 0, 2));
      __m256i kernvalsx3 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 1, 0));
      __m256i kernvalsx4 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 1, 1));
      __m256i kernvalsx5 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 1, 2));
      __m256i kernvalsx6 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 2, 0));
      __m256i kernvalsx7 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 2, 1));
      __m256i kernvalsx8 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_X, 2, 2));

      // load the y kernel values
      __m256i kernvalsy0 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 0, 0));
      __m256i kernvalsy1 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 0, 1));
      __m256i kernvalsy2 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 0, 2));
      __m256i kernvalsy3 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 1, 0));
      __m256i kernvalsy4 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 1, 1));
      __m256i kernvalsy5 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 1, 2));
      __m256i kernvalsy6 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 2, 0));
      __m256i kernvalsy7 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 2, 1));
      __m256i kernvalsy8 = _mm256_set1_epi32(GET_PIXEL(&KERNEL_Y, 2, 2));

      // multiply the x kernel values
      __m256i resultx0 = _mm256_mullo_epi32(pixvals0, kernvalsx0);
      __m256i resultx1 = _mm256_mullo_epi32(pixvals1, kernvalsx1);
      __m256i resultx2 = _mm256_mullo_epi32(pixvals2, kernvalsx2);
      __m256i resultx3 = _mm256_mullo_epi32(pixvals3, kernvalsx3);
      __m256i resultx4 = _mm256_mullo_epi32(pixvals4, kernvalsx4);
      __m256i resultx5 = _mm256_mullo_epi32(pixvals5, kernvalsx5);
      __m256i resultx6 = _mm256_mullo_epi32(pixvals6, kernvalsx6);
      __m256i resultx7 = _mm256_mullo_epi32(pixvals7, kernvalsx7);
      __m256i resultx8 = _mm256_mullo_epi32(pixvals8, kernvalsx8);

      // multiply the y kernel values
      __m256i resulty0 = _mm256_mullo_epi32(pixvals0, kernvalsy0);
      __m256i resulty1 = _mm256_mullo_epi32(pixvals1, kernvalsy1);
      __m256i resulty2 = _mm256_mullo_epi32(pixvals2, kernvalsy2);
      __m256i resulty3 = _mm256_mullo_epi32(pixvals3, kernvalsy3);
      __m256i resulty4 = _mm256_mullo_epi32(pixvals4, kernvalsy4);
      __m256i resulty5 = _mm256_mullo_epi32(pixvals5, kernvalsy5);
      __m256i resulty6 = _mm256_mullo_epi32(pixvals6, kernvalsy6);
      __m256i resulty7 = _mm256_mullo_epi32(pixvals7, kernvalsy7);
      __m256i resulty8 = _mm256_mullo_epi32(pixvals8, kernvalsy8);

      // reduce the x kernel products
      __m256i resultx =
        _mm256_add_epi32(
          _mm256_add_epi32(
            _mm256_add_epi32(
              _mm256_add_epi32(resultx0, resultx1),
              _mm256_add_epi32(resultx2, resultx3)
            ),
            _mm256_add_epi32(
              _mm256_add_epi32(resultx4, resultx5),
              _mm256_add_epi32(resultx6, resultx7)
            )
          ),
          resultx8
        );

      // reduce the y kernel products
      __m256i resulty =
        _mm256_add_epi32(
          _mm256_add_epi32(
            _mm256_add_epi32(
              _mm256_add_epi32(resulty0, resulty1),
              _mm256_add_epi32(resulty2, resulty3)
            ),
            _mm256_add_epi32(
              _mm256_add_epi32(resulty4, resulty5),
              _mm256_add_epi32(resulty6, resulty7)
            )
          ),
          resulty8
        );

      // absolute value
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by magnitude * 2
      assert(KERNEL_X.magnitude == 8);
      assert(KERNEL_Y.magnitude == 8);
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)&GET_PIXEL(out, i, j), result);
    }

    // do the right edge
    for (; j < j1 && j < ww; j++) {
      conv_pixel(in, out, i, j);
    }
  }
}
