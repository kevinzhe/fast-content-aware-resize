/**
 * @file seam_carve_baseline.c
 * @brief Naive baseline implementation of seam carving
 */

#include <assert.h>
#include <log.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <car.h>

#include "car_internal.h"
#include "energy.h"
#include "pathsum.h"

#define TIMING_INIT (memset(&__timing, 0, sizeof(__timing)))
#define TIC (__timing.__start = get_cycle_count())
#define TOC(attr) (__timing.attr += get_cycle_count() - __timing.__start)

static void log_timing(void);
static void rgb2gray(const rgb_image *in, gray_image *out);
static void __attribute__((unused)) gray2rgb(const gray_image *in, rgb_image *out);
static int carve_one_seam(const rgb_image *in, rgb_image *out);
static void remove_seam(const rgb_image *in, rgb_image *out,
                        const size_t *to_remove);

static struct {
  uint64_t __start;
  uint64_t grey;
  uint64_t conv;
  uint64_t pathsum;
  uint64_t minpath;
  uint64_t rmpath;
  uint64_t malloc;
} __timing;

int seam_carve_baseline(const rgb_image *in, rgb_image *out) {
  assert(is_rgb_image(in));
  assert(is_rgb_image(out));
  assert(out->width <= in->width);
  assert(out->height == in->height);

  log_info("Carving %d seams", in->width - out->width);

  TIMING_INIT;

  // allocate a temporary copy of the input
  TIC;
  rgb_image in_tmp;
  in_tmp.width = in->width;
  in_tmp.height = in->height;
  in_tmp.data = calloc(sizeof(rgb_pixel), in_tmp.width*in_tmp.height);
  if (!in_tmp.data) {
    log_fatal("malloc failed");
    return 1;
  }
  memcpy(in_tmp.data, in->data, sizeof(rgb_pixel)*in_tmp.width*in_tmp.height);
  TOC(malloc);

  // remove one seam at a time until done
  for (size_t ww = in->width-1; ww >= out->width; ww--) {
    TIC;

    // allocate space for the output
    rgb_image out_tmp;
    out_tmp.width = ww;
    out_tmp.height = in->height;
    out_tmp.data = calloc(sizeof(rgb_pixel), out_tmp.width*out_tmp.height);
    if (!out_tmp.data) {
      free(in_tmp.data);
      log_fatal("malloc failed");
      return 1;
    }

    TOC(malloc);

    carve_one_seam(&in_tmp, &out_tmp);

    TIC;

    // the output is the next input
    free(in_tmp.data);
    in_tmp.width = out_tmp.width;
    in_tmp.height = out_tmp.height;
    in_tmp.data = out_tmp.data;

    TOC(malloc);
  }

  assert(in_tmp.width == out->width);

  // finish up
  TIC;
  memcpy(out->data, in_tmp.data, sizeof(rgb_pixel)*in_tmp.width*in_tmp.height);
  free(in_tmp.data);
  TOC(malloc);

  log_info("Seam carving completed");
  log_timing();

  return 0;
}

static int carve_one_seam(const rgb_image *in, rgb_image *out) {
  assert(is_rgb_image(in));
  assert(is_rgb_image(out));
  assert(out->width == in->width - 1);
  assert(out->height == in->height);

  // allocate and create the grayscale image
  TIC;
  gray_image img;
  img.width = in->width;
  img.height = in->height;
  img.data = calloc(sizeof(pixval), img.width*img.height);
  if (!img.data) {
    log_fatal("malloc failed");
    return 1;
  }
  TOC(malloc);
  TIC;
  rgb2gray(in, &img);
  TOC(grey);

  // allocate and make the energy map
  TIC;
  energymap img_en;
  img_en.width = img.width;
  img_en.height = img.height;
  img_en.data = calloc(sizeof(enval), img.width*img.height);
  if (!img_en.data) {
    free(img.data);
    log_fatal("malloc_failed");
    return 1;
  }
  TOC(malloc);
  TIC;
  compute_energymap(&img, &img_en);
  TOC(conv);

  // allocate and make the path sums
  TIC;
  energymap img_pathsum;
  img_pathsum.width = img.width;
  img_pathsum.height = img.height;
  img_pathsum.data = calloc(sizeof(enval), img.width*img.height);
  if (!img_pathsum.data) {
    free(img.data);
    free(img_en.data);
    log_fatal("malloc failed");
    return 1;
  }
  TOC(malloc);
  TIC;
  compute_pathsum(&img_en, &img_pathsum);
  TOC(pathsum);

  // find the seam
  TIC;
  size_t *to_remove = calloc(sizeof(size_t), img.height);
  if (!to_remove) {
    free(img.data);
    free(img_en.data);
    free(img_pathsum.data);
    log_fatal("malloc_failed");
    return 1;
  }
  TOC(malloc);
  TIC;
  find_minseam(&img_pathsum, to_remove);
  TOC(minpath);
  TIC;
  remove_seam(in, out, to_remove);
  TOC(rmpath);

  TIC;
  free(img.data);
  free(img_en.data);
  free(img_pathsum.data);
  TOC(malloc);

  return 0;
}

static void remove_seam(const rgb_image *in, rgb_image *out, const size_t *to_remove) {
  assert(is_rgb_image(in));
  assert(is_rgb_image(out));

  size_t hh = in->height;
  size_t ww = in->width;
  size_t ww_out = ww - 1;

  assert(hh == out->height);
  assert(ww == out->width+1);

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < to_remove[i]; j++) {
      out->data[i*ww_out+j] = in->data[i*ww+j];
    }
    for (size_t j = to_remove[i]+1; j < ww; j++) {
      out->data[i*ww_out+j-1] = in->data[i*ww+j];
    }
  }
}

static void rgb2gray(const rgb_image *in, gray_image *out) {
  assert(is_rgb_image(in));
  assert(is_gray_image(out));

  size_t hh = in->height;
  size_t ww = in->width;

  assert(hh == out->height);
  assert(ww == out->width);

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < ww; j++) {
      rgb_pixel *pix = &in->data[i*ww+j];
      out->data[i*ww+j] = (pixval)(pix->red/3 + pix->green/3 + pix->blue/3);
    }
  }
}

static void gray2rgb(const gray_image *in, rgb_image *out) {
  assert(is_gray_image(in));
  assert(is_rgb_image(out));
  assert(in->width == out->width);
  assert(in->height == out->height);

  size_t hh = in->height;
  size_t ww = in->width;

  for (size_t i = 0; i < hh; i++) {
    for (size_t j = 0; j < ww; j++) {
      pixval val = in->data[i*ww+j];
      rgb_pixel *out_val = &out->data[i*ww+j];
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
    + __timing.pathsum
    + __timing.minpath
    + __timing.rmpath
    + __timing.malloc;
  log_info("grey   \t%llu\t%3.2f%%", 100.0 * (double) __timing.grey    / (double) total, __timing.grey   );
  log_info("conv   \t%llu\t%3.2f%%", 100.0 * (double) __timing.conv    / (double) total, __timing.conv   );
  log_info("pathsum\t%llu\t%3.2f%%", 100.0 * (double) __timing.pathsum / (double) total, __timing.pathsum);
  log_info("minpath\t%llu\t%3.2f%%", 100.0 * (double) __timing.minpath / (double) total, __timing.minpath);
  log_info("rmpath \t%llu\t%3.2f%%", 100.0 * (double) __timing.rmpath  / (double) total, __timing.rmpath );
  log_info("malloc \t%llu\t%3.2f%%", 100.0 * (double) __timing.malloc  / (double) total, __timing.malloc );
  log_info("total  \t%llu", total);
}
