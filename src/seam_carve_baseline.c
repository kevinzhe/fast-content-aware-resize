/**
 * @file seam_carve_baseline.c
 * @brief Naive baseline implementation of seam carving
 */

#include <assert.h>
#include <log.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <car.h>

#include "car_internal.h"
#include "energy.h"
#include "pathsum.h"

#define TIMING_INIT (memset(&__timing, 0, sizeof(__timing)))
#define TIC (__timing.__start = GET_CYCLE_COUNT())
#define TOC(attr) (__timing.attr += GET_CYCLE_COUNT() - __timing.__start)

static void log_timing(void);
static void rgb2gray(const rgb_image *in, gray_image *out);
static void __attribute__((unused)) gray2rgb(const gray_image *in, rgb_image *out);
static void imgcpy(rgb_image *dst, const rgb_image *src);

static struct {
  uint64_t __start;
  uint64_t grey;
  uint64_t conv;
  uint64_t convp;
  uint64_t pathsum;
  uint64_t minpath;
  uint64_t rmpath;
  uint64_t malloc;
} __timing;

#define remove_seam(img, to_remove)                                               \
  do {                                                                            \
    if (((to_remove)[0] + (to_remove)[(img)->height-1]) / 2 > (img)->width/2) {   \
      for (size_t i = 0; i < (img)->height; i++) {                                \
        void *src = &GET_PIXEL((img), i, (to_remove)[i] + 1);                     \
        void *dst = &GET_PIXEL((img), i, (to_remove)[i] + 0);                     \
        size_t n = sizeof((img)->data[0]) * ((img)->width - (to_remove)[i] - 1);  \
        memmove(dst, src, n);                                                     \
      }                                                                           \
    } else {                                                                      \
      for (size_t i = 0; i < (img)->height; i++) {                                \
        void *src = &GET_PIXEL((img), i, 0);                                      \
        void *dst = &GET_PIXEL((img), i, 1);                                      \
        size_t n = sizeof((img)->data[0]) * (to_remove)[i];                       \
        memmove(dst, src, n);                                                     \
      }                                                                           \
      (img)->buf_start++;                                                         \
    }                                                                             \
    (img)->width--;                                                               \
  } while (0)

uint8_t bigbuf[100*1024*1024];

int seam_carve_baseline(const rgb_image *in, rgb_image *out) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(out->width <= in->width);
  assert(out->height == in->height);
  assert(out->buf_width == out->width);
  assert(out->buf_height == out->height);

  log_info("Carving %d seams", in->width - out->width);

  TIMING_INIT;

  // make a rgb copy to work on
  TIC;
  rgb_image rgb_in_tmp;
  INITIALIZE_IMAGE(&rgb_in_tmp, in->width, in->height);
  if (!rgb_in_tmp.data) {
    log_fatal("malloc failed");
    return 1;
  }
  memcpy(rgb_in_tmp.data, in->data, sizeof(rgb_pixel)*in->width*in->height);
  TOC(malloc);

  // make a grayscale copy to work on
  TIC;
  gray_image in_tmp;
  INITIALIZE_IMAGE(&in_tmp, in->width, in->height);
  if (!in_tmp.data) {
    log_fatal("malloc failed");
    free(rgb_in_tmp.data);
    return 1;
  }
  TOC(malloc);
  TIC;
  rgb2gray(in, &in_tmp);
  TOC(grey);

  // allocate the energy map
  TIC;
  energymap img_en;
  INITIALIZE_IMAGE(&img_en, in->width, in->height);
  if (!img_en.data) {
    log_fatal("malloc_failed");
    free(rgb_in_tmp.data);
    free(in_tmp.data);
    return 1;
  }
  TOC(malloc);

  // allocate the pathsum array
  TIC;
  energymap img_pathsum;
  INITIALIZE_IMAGE(&img_pathsum, in->width, in->height);
  if (!img_pathsum.data) {
    log_fatal("malloc failed");
    free(rgb_in_tmp.data);
    free(in_tmp.data);
    free(img_en.data);
  }
  TOC(malloc);

  // allocate space for the current found seam to remove
  TIC;
  size_t *to_remove = malloc(sizeof(size_t) * in->height);
  if (!to_remove) {
    free(rgb_in_tmp.data);
    free(in_tmp.data);
    free(img_en.data);
    free(img_pathsum.data);
    log_fatal("malloc failed");
    return 1;
  }
  TOC(malloc);

  size_t pathsum_inout = 0;
  static double best_conv_cpe = INFINITY;

  // remove one seam at a time until done
  for (size_t ww = in->width-1; ww >= out->width; ww--) {
    if (ww == in->width-1) {
      // compute the initial energy map
      TIC;
      double cpe = compute_energymap(&in_tmp, &img_en);
      TOC(conv);
      if (cpe < best_conv_cpe) best_conv_cpe = cpe;
      // compute the initial path sum
      TIC;
      compute_pathsum(&img_en, &img_pathsum);
      TOC(pathsum);
    } else {
      // compute a partial energy map
      TIC;
      double cpe = compute_energymap_partial(&in_tmp, &img_en, to_remove);
      TOC(convp);
      if (cpe < best_conv_cpe) best_conv_cpe = cpe;
      // compute a partial path sum
      TIC;
      pathsum_inout += compute_pathsum_partial(&img_en, &img_pathsum, to_remove);
      TOC(pathsum);
    }

    // find the seam
    TIC;
    find_minseam(&img_pathsum, to_remove);
    TOC(minpath);

    // remove the seam from the grey
    TIC;
    remove_seam(&in_tmp, to_remove);
    TOC(rmpath);

    // remove the seam from the rgb
    TIC;
    remove_seam(&rgb_in_tmp, to_remove);
    TOC(rmpath);

    // remove the seam from the energymap
    TIC;
    remove_seam(&img_en, to_remove);
    TOC(rmpath);

    // remove the seam from the pathsum
    TIC;
    remove_seam(&img_pathsum, to_remove);
    TOC(rmpath);
  }

  assert(in_tmp.width == rgb_in_tmp.width);
  assert(in_tmp.width == out->width);

  // finish up
  TIC;
  imgcpy(out, &rgb_in_tmp);
  free(rgb_in_tmp.data);
  free(in_tmp.data);
  free(img_en.data);
  free(to_remove);
  free(img_pathsum.data);
  TOC(malloc);

  double gbps = ((double)(pathsum_inout) / 1024.0 / 1024.0 / 1024.0)
      / ((double)(__timing.pathsum) / 3200000000.0);
  log_info("pathsum: %f gb/s", gbps);
  log_info("conv   : %f cpe", best_conv_cpe);

  log_info("Seam carving completed");
  log_timing();

  return 0;
}

static void imgcpy(rgb_image *dst, const rgb_image *src) {
  assert(IS_IMAGE(dst));
  assert(IS_IMAGE(src));
  assert(dst->width == src->width);
  assert(dst->height == src->height);

  size_t ww = dst->width;
  size_t hh = dst->height;

  size_t row_size = ww * sizeof(*dst->data);

  for (size_t i = 0; i < hh; i++) {
    memcpy(&GET_PIXEL(dst, i, 0), &GET_PIXEL(src, i, 0), row_size);
  }
}

static void rgb2gray(const rgb_image *in, gray_image *out) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));

  size_t hh = in->height;
  size_t ww = in->width;

  assert(hh == out->height);
  assert(ww == out->width);

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < ww; j++) {
      rgb_pixel *pix = &GET_PIXEL(in, i, j);
      GET_PIXEL(out, i, j) = (pixval)(pix->red/3 + pix->green/3 + pix->blue/3);
    }
  }
}

static void gray2rgb(const gray_image *in, rgb_image *out) {
  assert(IS_IMAGE(in));
  assert(IS_IMAGE(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  size_t hh = in->height;
  size_t ww = in->width;

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < ww; j++) {
      pixval val = GET_PIXEL(in, i, j);
      rgb_pixel *out_val = &GET_PIXEL(out, i, j);
      out_val->red = val;
      out_val->green = val;
      out_val->blue = val;
    }
  }
}


static void log_timing(void) {
  uint64_t total =
      __timing.grey
    + __timing.conv
    + __timing.convp
    + __timing.pathsum
    + __timing.minpath
    + __timing.rmpath
    + __timing.malloc;
  log_info("grey   \t%llu\t%3.2f%%", 100.0 * (double) __timing.grey    / (double) total, __timing.grey   );
  log_info("conv   \t%llu\t%3.2f%%", 100.0 * (double) __timing.conv    / (double) total, __timing.conv   );
  log_info("convp  \t%llu\t%3.2f%%", 100.0 * (double) __timing.convp   / (double) total, __timing.convp  );
  log_info("pathsum\t%llu\t%3.2f%%", 100.0 * (double) __timing.pathsum / (double) total, __timing.pathsum);
  log_info("minpath\t%llu\t%3.2f%%", 100.0 * (double) __timing.minpath / (double) total, __timing.minpath);
  log_info("rmpath \t%llu\t%3.2f%%", 100.0 * (double) __timing.rmpath  / (double) total, __timing.rmpath );
  log_info("malloc \t%llu\t%3.2f%%", 100.0 * (double) __timing.malloc  / (double) total, __timing.malloc );
  log_info("total  \t%llu", total);
}
