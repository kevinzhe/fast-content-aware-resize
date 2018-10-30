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
static void remove_seam(void *img_data, size_t width, size_t height,
                        size_t pixsize, const size_t *to_remove);

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

int seam_carve_baseline(const rgb_image *in, rgb_image *out) {
  assert(is_rgb_image(in));
  assert(is_rgb_image(out));
  assert(out->width <= in->width);
  assert(out->height == in->height);

  log_info("Carving %d seams", in->width - out->width);

  TIMING_INIT;

  // make a rgb copy to work on
  TIC;
  rgb_image rgb_in_tmp;
  rgb_in_tmp.width = in->width;
  rgb_in_tmp.height = in->height;
  rgb_in_tmp.data = malloc(sizeof(rgb_pixel) * rgb_in_tmp.width*rgb_in_tmp.height);
  if (!rgb_in_tmp.data) {
    log_fatal("malloc failed");
    return 1;
  }
  memcpy(rgb_in_tmp.data, in->data, sizeof(rgb_pixel)*in->width*in->height);
  TOC(malloc);

  // make a grayscale copy to work on
  TIC;
  gray_image in_tmp;
  in_tmp.width = in->width;
  in_tmp.height = in->height;
  in_tmp.data = malloc(sizeof(pixval) * in_tmp.width*in_tmp.height);
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
  img_en.width = in->width;
  img_en.height = in->height;
  img_en.data = malloc(sizeof(enval) * in->width*in->height);
  if (!img_en.data) {
    log_fatal("malloc_failed");
    return 1;
  }
  TOC(malloc);

  // allocate space for the current found seam to remove
  TIC;
  size_t *to_remove = malloc(sizeof(size_t) * in->height);
  if (!to_remove) {
    free(rgb_in_tmp.data);
    free(in_tmp.data);
    free(img_en.data);
    log_fatal("malloc failed");
    return 1;
  }
  TOC(malloc);

  // remove one seam at a time until done
  for (size_t ww = in->width-1; ww >= out->width; ww--) {
    if (ww == in->width-1) {
      // compute the initial energy map
      TIC;
      compute_energymap(&in_tmp, &img_en);
      TOC(conv);
    } else {
      // compute a partial energy map
      TIC;
      compute_energymap_partial(&in_tmp, &img_en, to_remove);
      TOC(convp);
    }

    // allocate and make the path sums
    TIC;
    energymap img_pathsum;
    img_pathsum.width = in_tmp.width;
    img_pathsum.height = in_tmp.height;
    img_pathsum.data = malloc(sizeof(enval) * in_tmp.width*in_tmp.height);
    if (!img_pathsum.data) {
      free(rgb_in_tmp.data);
      free(in_tmp.data);
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
    find_minseam(&img_pathsum, to_remove);
    TOC(minpath);

    // remove the seam from the grey
    TIC;
    remove_seam(in_tmp.data, in_tmp.width, in_tmp.height,
                sizeof(in_tmp.data[0]), to_remove);
    TOC(rmpath);

    // remove the seam from the rgb
    TIC;
    remove_seam(rgb_in_tmp.data, rgb_in_tmp.width, in_tmp.height,
                sizeof(rgb_in_tmp.data[0]), to_remove);
    TOC(rmpath);

    // remove the seam from the energymap
    TIC;
    remove_seam(img_en.data, img_en.width, img_en.height,
                sizeof(img_en.data[0]), to_remove);
    TOC(rmpath);

    // update the sizes
    in_tmp.width--;
    rgb_in_tmp.width--;
    img_en.width--;

    // clean up
    free(img_pathsum.data);
  }

  assert(in_tmp.width == rgb_in_tmp.width);
  assert(in_tmp.width == out->width);

  // finish up
  TIC;
  memcpy(out->data, rgb_in_tmp.data, sizeof(rgb_pixel)*in_tmp.width*in_tmp.height);
  free(rgb_in_tmp.data);
  free(in_tmp.data);
  free(img_en.data);
  free(to_remove);
  TOC(malloc);

  log_info("Seam carving completed");
  log_timing();

  return 0;
}

static void remove_seam(void *img_data, size_t width, size_t height,
                        size_t pixsize, const size_t *to_remove) {
  assert(img_data);
  assert(width > 0);
  assert(height > 0);
  assert(pixsize > 0);

  size_t hh = height;
  size_t ww = width;

  uint8_t *img_bytes = (uint8_t *)img_data;

  for (size_t i = 0; i < hh; i++) {
    assert(to_remove[i] < ww);

    uint8_t *src_l = img_bytes + pixsize*(i*ww);
    uint8_t *dst_l = img_bytes + pixsize*(i*(ww-1));
    size_t n_l = to_remove[i] * pixsize;

    uint8_t *src_r = img_bytes + pixsize*(i*ww + to_remove[i] + 1);
    uint8_t *dst_r = img_bytes + pixsize*(i*(ww-1) + to_remove[i]);
    size_t n_r = (ww - to_remove[i] - 1) * pixsize;

    memmove(dst_l, src_l, n_l);
    memmove(dst_r, src_r, n_r);
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
