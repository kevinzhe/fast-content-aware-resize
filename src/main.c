/**
 * @file main.c
 * @brief Entrypoint into the program
 */

#include <log.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <wand/magick_wand.h>

#include <car.h>
#include <x86intrin.h>

#include "car_internal.h"

int main(int argc, const char *argv[]) {
  if (argc < 4) {
    printf("Usage: %s [in] [out] [width]\n", argv[0]);
    printf("  in: Input image path (eg. in.jpg)\n");
    printf("  in: Where to save output image (eg. out.jpg)\n");
    printf("  width: How many vertical seams to remove (eg. 200)\n");
    return 1;
  }

  const char *inpath = argv[1];
  const char *outpath = argv[2];
  size_t to_remove;
  if (sscanf(argv[3], "%zu", &to_remove) != 1) {
    log_fatal("Invalid seam count: %s", argv[3]);
    return 1;
  }

  // initialize magickwand
  MagickWandGenesis();
  MagickWand *mw = NewMagickWand();
  log_info("MagickWand initialized");

  // load the image
  if (MagickReadImage(mw, inpath) != MagickTrue) {
    log_fatal("Could not open image: %s", inpath);
    MagickWandTerminus();
    return 1;
  }

  log_info("Opened image: %s", inpath);

  size_t ww = MagickGetImageWidth(mw);
  size_t hh = MagickGetImageHeight(mw);

  if (to_remove > ww) {
    log_fatal("Image width %zu, can't remove %llu seams", ww, to_remove);
    return 1;
  }

  if (ww-to_remove < 10) {
    log_fatal("Output image width must be at least 10 pixels\n");
    return 1;
  }

  // allocate the input image
  rgb_image in;
  INITIALIZE_IMAGE(&in, ww, hh);
  if (!in.data) {
    log_fatal("malloc failed");
    return 1;
  }

  // allocate the output image
  rgb_image out;
  INITIALIZE_IMAGE(&out, ww-to_remove, hh);
  if (!out.data) {
    free(in.data);
    log_fatal("malloc failed");
    return 1;
  }

  // read the image
  MagickExportImagePixels(mw, 0, 0, ww, hh, "RGB", CharPixel, in.data);

  // do the carve
  uint64_t start = __rdtsc();
  if (seam_carve_baseline(&in, &out) != 0) {
    log_fatal("seam_carve_baseline failed");
    return 1;
  }
  uint64_t end = __rdtsc();
  log_info("Completed in %llu cycles (%0.2fs)", end-start, (float)(end-start)/2500000000.0);

  // bring the image back to magickwand
  MagickCropImage(mw, out.width, out.height, 0, 0);
  MagickImportImagePixels(mw, 0, 0, out.width, out.height, "RGB", CharPixel, out.data);

  // write it to file
  log_info("Writing result to %s", outpath);
  if (MagickWriteImage(mw, outpath) != MagickTrue) {
    log_fatal("Failed to write output: %s", outpath);
    MagickWandTerminus();
    return 1;
  }

  free(in.data);
  free(out.data);

  MagickWandTerminus();
  log_info("MagickWand terminated");
  log_info("Exiting");

  return 0;
}
