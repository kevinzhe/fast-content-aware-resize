/**
 * @file energy.c
 * @brief Routines to create energy maps
 */

#include <assert.h>
#include <math.h>
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
static double conv_pixel_vec(const gray_image *in, energymap *out, size_t i, size_t j, size_t len);

#define LOAD_EIGHT_UNSIGNED_BYTES(data) \
  (_mm256_cvtepu8_epi32(_mm_loadu_si128((const __m128i *)(data))))

double compute_energymap_partial(const gray_image *in, energymap *out, const size_t *removed) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);
  assert(removed);

  size_t hh = in->height;
  size_t khh = KERNEL_HEIGHT;
  size_t kww = KERNEL_WIDTH;

  size_t vec_width = 8;

  double best_cpe = INFINITY;

  for (size_t i = 0; i < hh; i++) {
    size_t j0 = removed[i] - (kww/2) - (khh-1)-1;
    double cpe = conv_pixel_vec(in, out, i, j0, vec_width);
    if (cpe < best_cpe) {
      best_cpe = cpe;
    }
  }

  return best_cpe;
}

double compute_energymap(const gray_image *in, energymap *out) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  size_t hh = in->height;
  size_t ww = in->width;

  double best_cpe = INFINITY;

  for (size_t i = 0; i < hh; i++) {
    double cpe = conv_pixel_vec(in, out, i, 0, ww);
    if (cpe < best_cpe) {
      best_cpe = cpe;
    }
  }

  return best_cpe;
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

static double conv_pixel_vec(const gray_image *in, energymap *out, size_t i, size_t j, size_t len) {
  assert(len > 0);
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  double best_cpe = INFINITY;

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

    pixval *upper = &GET_PIXEL(in, i-1, j-1);
    pixval *mid   = &GET_PIXEL(in, i  , j-1);
    pixval *lower = &GET_PIXEL(in, i+1, j-1);
    enval *res = &GET_PIXEL(out, i, j);

    uint64_t start = __rdtsc();
    size_t elts = 0;

    // do the middle
    for (; j+8*vec_width <= j1 && j+8*vec_width < ww-kww/2; j += 8*vec_width) {
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
      {
      // load the image pixel values
      __m256i pixvals00 = LOAD_EIGHT_UNSIGNED_BYTES(upper+0);
      __m256i pixvals01 = LOAD_EIGHT_UNSIGNED_BYTES(upper+1);
      __m256i pixvals02 = LOAD_EIGHT_UNSIGNED_BYTES(upper+2);
      __m256i pixvals10 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +0);
      __m256i pixvals12 = LOAD_EIGHT_UNSIGNED_BYTES(mid  +2);
      __m256i pixvals20 = LOAD_EIGHT_UNSIGNED_BYTES(lower+0);
      __m256i pixvals21 = LOAD_EIGHT_UNSIGNED_BYTES(lower+1);
      __m256i pixvals22 = LOAD_EIGHT_UNSIGNED_BYTES(lower+2);

      // increment pointer values for next iteration
      upper += vec_width;
      mid   += vec_width;
      lower += vec_width;

      /* ----------------------------------------------------------------------
       * The x and y sobel kernel, in 4 adds, 5 subtracts, and 2 shifts
       * ---------------------------------------------------------------------- */

      // initialize the results with the values they independently use
      __m256i resultx = _mm256_sub_epi32(pixvals21, pixvals01);         // x = 21-01
      __m256i resulty = _mm256_sub_epi32(pixvals12, pixvals10);         // y = 12-10

      // use shifts here to do the x2 to reduce pressure on uop port 5
      resultx = _mm256_slli_epi32(resultx, 1);                          // x *= 2
      resulty = _mm256_slli_epi32(resulty, 1);                          // y *= 2

      // the top left and bottom right corners are shared, so only add them once
      __m256i shared_corners = _mm256_sub_epi32(pixvals22, pixvals00);  // sc = 22-00

      // add in the shared corners
      resultx = _mm256_add_epi32(resultx, shared_corners);              // x += sc
      resulty = _mm256_add_epi32(resulty, shared_corners);              // y += sc

      // add together the other two corners for each kernel
      __m256i x_corners = _mm256_sub_epi32(pixvals20, pixvals02);       // xc = 20-02
      __m256i y_corners = _mm256_sub_epi32(pixvals02, pixvals20);       // yc = 02-20

      // and their own corners
      resultx = _mm256_add_epi32(resultx, x_corners);                   // x += xc
      resulty = _mm256_add_epi32(resulty, y_corners);                   // y += yc

      /* ----------------------------------------------------------------------
       * The sobel kernel has been applied:
       *   x = (21-01)*2 + (22-00) + (20-02)
       *   y = (12-10)*2 + (22-00) + (02-20)
       * ---------------------------------------------------------------------- */

      // take their magnitude
      resultx = _mm256_abs_epi32(resultx);
      resulty = _mm256_abs_epi32(resulty);

      // divide by kernel magnitude * 2
      resultx = _mm256_srai_epi32(resultx, 4);
      resulty = _mm256_srai_epi32(resulty, 4);

      // sum x and y kernel results
      __m256i result = _mm256_add_epi32(resultx, resulty);

      // save it
      _mm256_storeu_si256((__m256i *)res, result);
      res += vec_width;
      elts += vec_width;
      }
    }

    uint64_t end = __rdtsc();
    double cpe = ((double)(end-start)*3.8/3.2)/(double)elts;
    if (cpe < best_cpe) {
      best_cpe = cpe;
    }

    // do the right edge
    for (; j < j1 && j < ww; j++) {
      conv_pixel(in, out, i, j);
    }
  }

  return best_cpe;
}
