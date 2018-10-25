/**
 * @file main.c
 * @brief Entrypoint into the program
 */

#include <log.h>
#include <stdint.h>
#include <stdlib.h>
#include <wand/magick_wand.h>

#include <car.h>
#include <x86intrin.h>

int main(int argc, const char *argv[]) {
  if (argc < 3) {
    log_fatal("No file specified");
    return 1;
  }

  const char *inpath = argv[1];
  const char *outpath = argv[2];

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

  // allocate the input image
  rgb_image in;
  in.width = ww;
  in.height = hh;
  in.data = malloc(sizeof(rgb_pixel) * ww*hh);
  if (!in.data) {
    log_fatal("malloc failed");
    return 1;
  }

  // allocate the output image
  rgb_image out;
  out.width = ww-200;
  out.height = hh;
  out.data = malloc(sizeof(rgb_pixel) * out.width*out.height);
  if (!out.data) {
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
  log_info("Completed in %llu cycles (%f cycles per pixel)", end-start, (float)(end-start)/(float)(ww*hh));

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

  MagickWandTerminus();
  log_info("MagickWand terminated");
  log_info("Exiting");

  return 0;
}
