/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */


#include "pagespeed/kernel/image/image_converter.h"

using net_instaweb::MessageHandler;


#include <setjmp.h>
#include <cstddef>

extern "C" {
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"  // NOLINT
#else
#include "third_party/libpng/src/png.h"
#endif

#ifdef USE_SYSTEM_ZLIB
#include "zlib.h"
#else
#include "third_party/zlib/src/zlib.h"
#endif
}  // extern "C"

#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_interface_frame_adapter.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace {
// In some cases, converting a PNG to JPEG results in a smaller
// file. This is at the cost of switching from lossless to lossy, so
// we require that the savings are substantial before in order to do
// the conversion. We choose 80% size reduction as the minimum before
// we switch a PNG to JPEG.
const double kMinJpegSavingsRatio = 0.8;

// As above, but for use when comparing lossy WebPs to lossless formats.
const double kMinWebpSavingsRatio = 0.8;

// If 'new_image' and 'new_image_type' represent a valid image that is
// smaller than 'threshold_ratio' times the size of the current
// 'best_image' (if any), then updates 'best_image' and
// 'best_image_type' to point to the values of 'new_image' and
// 'new_image_type'.
void SelectSmallerImage(
    pagespeed::image_compression::ImageConverter::ImageType new_image_type,
    const GoogleString& new_image,
    const double threshold_ratio,
    pagespeed::image_compression::ImageConverter::ImageType* const
    best_image_type,
    const GoogleString** const best_image,
    MessageHandler* handler) {
  size_t new_image_size = new_image.size();
  if (new_image_size > 0 &&
      ((*best_image_type ==
        pagespeed::image_compression::ImageConverter::IMAGE_NONE) ||
       ((new_image_type !=
         pagespeed::image_compression::ImageConverter::IMAGE_NONE) &&
        (*best_image != NULL) &&
        (new_image_size < (*best_image)->size() * threshold_ratio)))) {
    *best_image_type = new_image_type;
    *best_image = &new_image;

    PS_DLOG_INFO(handler, \
        "%p best is now %d", static_cast<void *>(best_image_type), \
        new_image_type);
  }
}

// To estimate the number of bytes from the number of pixels, we divide
// by a magic ratio.  The 'correct' ratio is of course dependent on the
// image itself, but we are ignoring that so we can make a fast judgement.
// It is also dependent on a variety of image optimization settings, but
// for now we will assume the 'rewrite_images' bucket is on, and vary only
// on the jpeg compression level.
//
// Consider a testcase from our system tests, which resizes
// mod_pagespeed_example/images/Puzzle.jpg to 256x192, or 49152
// pixels, using compression level 75.  Our default byte threshold for
// jpeg progressive conversion is 10240 (rewrite_options.cc).
// Converting to progressive in this case makes the image slightly
// larger (8251 bytes vs 8157 bytes), so we'd like this to be the
// threshold where we decide *not* to convert to progressive.
// Dividing 49152 by 5 (multiplying by 0.2) gets us just under our
// default 10k byte threshold.
//
// Making this number smaller will break apache/system_test.sh with this
// failure:
//     failure at line 353
// FAILed Input: /tmp/.../fetched_directory/*256x192*Puzzle* : 8251 -le 8157
// in 'quality of jpeg output images with generic quality flag'
// FAIL.
//
// A first attempt at computing that ratio is based on an analysis of Puzzle.jpg
// at various compression ratios.  Sized to 256x192, or 49152 pixels:
//
// compression level    size(no progressive)  no_progressive/49152
// 50,                  5891,                 0.1239217122
// 55,                  6186,                 0.1299615486
// 60,                  6661,                 0.138788298
// 65,                  7068,                 0.1467195606
// 70,                  7811,                 0.1611197005
// 75,                  8402,                 0.1728746669
// 80,                  9800,                 0.1976280565
// 85,                  11001,                0.220020749
// 90,                  15021,                0.2933279089
// 95,                  19078,                0.3703545493
// 100,                 19074,                0.3704283796
//
// At compression level 100, byte-sizes are almost identical to compression 95
// so we throw this data-point out.
//
// Plotting this data in a graph the data is non-linear.  Experimenting in a
// spreadsheet we get decent visual linearity by transforming the somewhat
// arbitrary compression ratio with the formula (1 / (110 - compression_level)).
// Drawing a line through the data-points at compression levels 50 and 95, we
// get a slope of 4.92865674 and an intercept of 0.04177743.  Double-checking,
// this fits the other data-points we have reasonably well, except for the
// one at compression_level 100.
const double JpegPixelToByteRatio(int compression_level) {
  if ((compression_level > 95) || (compression_level < 0)) {
    compression_level = 95;
  }
  double kSlope = 4.92865674;
  double kIntercept = 0.04177743;
  double ratio = kSlope / (110.0 - compression_level) + kIntercept;
  return ratio;
}

}  // namespace

namespace pagespeed {

namespace image_compression {

ScanlineStatus ImageConverter::ConvertImageWithStatus(
    ScanlineReaderInterface* reader,
    ScanlineWriterInterface* writer) {
  void* scan_row;
  while (reader->HasMoreScanLines()) {
    ScanlineStatus reader_status =
        reader->ReadNextScanlineWithStatus(&scan_row);
    if (!reader_status.Success()) {
      return reader_status;
    }
    ScanlineStatus writer_status =
        writer->WriteNextScanlineWithStatus(scan_row);
    if (!writer_status.Success()) {
      return writer_status;
    }
  }

  ScanlineStatus writer_status = writer->FinalizeWriteWithStatus();
  if (!writer_status.Success()) {
    return writer_status;
  }

  return ScanlineStatus(SCANLINE_STATUS_SUCCESS);
}

ScanlineStatus ImageConverter::ConvertMultipleFrameImage(
    MultipleFrameReader* reader,
    MultipleFrameWriter* writer) {
  ImageSpec image_spec;
  FrameSpec frame_spec;
  const void* scan_row = NULL;

  ScanlineStatus status;
  if (reader->GetImageSpec(&image_spec, &status) &&
      writer->PrepareImage(&image_spec, &status)) {
    while (reader->HasMoreFrames() &&
           reader->PrepareNextFrame(&status) &&
           reader->GetFrameSpec(&frame_spec, &status) &&
           writer->PrepareNextFrame(&frame_spec, &status)) {
      while (reader->HasMoreScanlines() &&
             reader->ReadNextScanline(&scan_row, &status) &&
             writer->WriteNextScanline(scan_row, &status)) {
        // intentional empty loop body
      }
    }
  }
  writer->FinalizeWrite(&status);
  return status;
}


bool ImageConverter::ConvertPngToJpeg(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const JpegCompressionOptions& options,
    GoogleString* out,
    MessageHandler* handler) {
  DCHECK(out->empty());
  out->clear();

  // Initialize the reader.
  PngScanlineReader png_reader(handler);

  // Since JPEG only support 8 bits/channels, we need convert PNG
  // having 1,2,4,16 bits/channel to 8 bits/channel.
  //   -PNG_TRANSFORM_EXPAND expands 1,2 and 4 bit channels to 8 bit
  //                         channels, and de-colormaps images.
  //   -PNG_TRANSFORM_STRIP_16 will strip 16 bit channels to get 8 bit
  //                           channels.
  png_reader.set_transform(PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16);

  // Since JPEGs can only support opaque images, require this in the reader.
  png_reader.set_require_opaque(true);

  // Configure png reader error handlers.
  if (setjmp(*png_reader.GetJmpBuf())) {
    PS_LOG_INFO(handler, "libpng failed to decode the PNG image.");
    return false;
  }

  if (!png_reader.InitializeRead(png_struct_reader, in)) {
    return false;
  }

  // Try converting if the image is opaque.
  bool jpeg_success = false;
  size_t width = png_reader.GetImageWidth();
  size_t height = png_reader.GetImageHeight();
  PixelFormat format = png_reader.GetPixelFormat();

  if (height > 0 && width > 0 && format != UNSUPPORTED) {
    JpegScanlineWriter jpeg_writer(handler);

    // libjpeg's error handling mechanism requires that longjmp be used
    // to get control after an error.
    jmp_buf env;
    if (setjmp(env)) {
      // This code is run only when libjpeg hit an error, and called
      // longjmp(env).
      jpeg_writer.AbortWrite();
    } else {
      jpeg_writer.SetJmpBufEnv(&env);
      if (jpeg_writer.Init(width, height, format)) {
        jpeg_writer.InitializeWrite(&options, out);
        jpeg_success = ConvertImage(&png_reader, &jpeg_writer);
      }
    }
  }
  return jpeg_success;
}

bool ImageConverter::OptimizePngOrConvertToJpeg(
    const PngReaderInterface& png_struct_reader, const GoogleString& in,
    const JpegCompressionOptions& options, GoogleString* out,
    bool* is_out_png, MessageHandler* handler) {

  bool jpeg_success = ConvertPngToJpeg(png_struct_reader, in, options, out,
                                       handler);

  // Try Optimizing the PNG.
  // TODO(satyanarayana): Try reusing the PNG structs for png->jpeg and optimize
  // png operations.
  GoogleString optimized_png_out;
  bool png_success = PngOptimizer::OptimizePngBestCompression(
      png_struct_reader, in, &optimized_png_out, handler);

  // Consider using jpeg's only if it gives substantial amount of byte savings.
  if (png_success &&
      (!jpeg_success ||
       out->size() > kMinJpegSavingsRatio * optimized_png_out.size())) {
    out->clear();
    out->assign(optimized_png_out);
    *is_out_png = true;
  } else {
    *is_out_png = false;
  }

  return jpeg_success || png_success;
}

bool ImageConverter::ConvertPngToWebp(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const WebpConfiguration& webp_config,
    GoogleString* const out,
    bool* is_opaque,
    MessageHandler* handler) {
    ScanlineWriterInterface* webp_writer = NULL;
    bool success = ConvertPngToWebp(png_struct_reader, in, webp_config,
                                    out, is_opaque, &webp_writer, handler);
    delete webp_writer;
    return success;
}

bool ImageConverter::ConvertPngToWebp(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const WebpConfiguration& webp_config,
    GoogleString* const out,
    bool* is_opaque,
    ScanlineWriterInterface** webp_writer,
    MessageHandler* handler) {
  DCHECK(out->empty());
  out->clear();

  if (*webp_writer != NULL) {
    PS_LOG_DFATAL(handler, "Expected *webp_writer == NULL");
    return false;
  }

  // Initialize the reader.
  PngScanlineReader png_reader(handler);

  // Since the WebP API only support 8 bits/channels, we need convert PNG
  // having 1,2,4,16 bits/channel to 8 bits/channel.
  //   -PNG_TRANSFORM_EXPAND expands 1,2 and 4 bit channels to 8 bit
  //                         channels, and de-colormaps images.
  //   -PNG_TRANSFORM_STRIP_16 will strip 16 bit channels to get 8 bit/channel
  //   -PNG_TRANSFORM_GRAY_TO_RGB will transform grayscale to RGB
  png_reader.set_transform(
      PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_16 |
      PNG_TRANSFORM_GRAY_TO_RGB);

  // If alpha quality is zero, refuse to process transparent images.
  png_reader.set_require_opaque(webp_config.alpha_quality == 0);

  // Configure png reader error handlers.
  if (setjmp(*png_reader.GetJmpBuf())) {
    PS_LOG_INFO(handler, "libpng failed to decoded the PNG image.");
    return false;
  }
  if (!png_reader.InitializeRead(png_struct_reader, in, is_opaque)) {
    return false;
  }

  bool webp_success = false;
  size_t width = png_reader.GetImageWidth();
  size_t height = png_reader.GetImageHeight();
  PixelFormat format = png_reader.GetPixelFormat();

  (*webp_writer) =
      new FrameToScanlineWriterAdapter(new WebpFrameWriter(handler));

  if (height > 0 && width > 0 && format != UNSUPPORTED) {
    if ((*webp_writer)->Init(width, height, format) &&
        (*webp_writer)->InitializeWrite(&webp_config, out)) {
      webp_success = ConvertImage(&png_reader, *webp_writer);
    }
  }

  return webp_success;
}

ImageConverter::ImageType ImageConverter::GetSmallestOfPngJpegWebp(
    const PngReaderInterface& png_struct_reader,
    const GoogleString& in,
    const JpegCompressionOptions* jpeg_options,
    const WebpConfiguration* webp_config,
    GoogleString* out,
    MessageHandler* handler) {
  GoogleString jpeg_out, png_out, webp_lossless_out, webp_lossy_out;
  const GoogleString* best_lossless_image = NULL;
  const GoogleString* best_lossy_image = NULL;
  const GoogleString* best_image = NULL;
  ImageType best_lossless_image_type = IMAGE_NONE;
  ImageType best_lossy_image_type = IMAGE_NONE;
  ImageType best_image_type = IMAGE_NONE;

  ScanlineWriterInterface* webp_writer = NULL;
  WebpConfiguration webp_config_lossless;
  bool is_opaque = false;
  if (!ConvertPngToWebp(png_struct_reader, in, webp_config_lossless,
                        &webp_lossless_out, &is_opaque, &webp_writer,
                        handler)) {
    PS_DLOG_INFO(handler, "Could not convert image to lossless WebP");
    webp_lossless_out.clear();
  }
  if ((webp_config != NULL) &&
      (!webp_writer->InitializeWrite(webp_config, &webp_lossy_out) ||
       !webp_writer->FinalizeWrite())) {
    PS_DLOG_INFO(handler, "Could not convert image to custom WebP");
    webp_lossy_out.clear();
  }
  delete webp_writer;

  if (!PngOptimizer::OptimizePngBestCompression(png_struct_reader, in,
                                                &png_out, handler)) {
    PS_DLOG_INFO(handler, "Could not optimize PNG");
    png_out.clear();
  }

  // If jpeg options are passed in and we haven't determined for sure
  // that the image has transparency, try jpeg conversion.
  if ((jpeg_options != NULL) &&
      (webp_lossy_out.empty() || is_opaque) &&
      !ConvertPngToJpeg(png_struct_reader, in, *jpeg_options, &jpeg_out,
      handler)) {
    PS_DLOG_INFO(handler, "Could not convert image to JPEG");
    jpeg_out.clear();
  }

  SelectSmallerImage(IMAGE_NONE, in, 1,
                     &best_lossless_image_type, &best_lossless_image, handler);
  SelectSmallerImage(IMAGE_WEBP, webp_lossless_out, 1,
                     &best_lossless_image_type, &best_lossless_image, handler);
  SelectSmallerImage(IMAGE_PNG, png_out, 1,
                     &best_lossless_image_type, &best_lossless_image, handler);

  SelectSmallerImage(IMAGE_WEBP, webp_lossy_out, 1,
                     &best_lossy_image_type, &best_lossy_image, handler);
  SelectSmallerImage(IMAGE_JPEG, jpeg_out, 1,
                     &best_lossy_image_type, &best_lossy_image, handler);

  // To compensate for the lower quality, the lossy images must be
  // substantially smaller than the lossless images.
  double threshold_ratio = (best_lossy_image_type == IMAGE_WEBP ?
                            kMinWebpSavingsRatio : kMinJpegSavingsRatio);
  best_image_type = best_lossless_image_type;
  best_image = best_lossless_image;
  SelectSmallerImage(best_lossy_image_type, *best_lossy_image, threshold_ratio,
                     &best_image_type, &best_image, handler);

  out->clear();
  out->assign((best_image != NULL) ? *best_image : in);

  return best_image_type;
}

bool GenerateBlankImage(size_t width, size_t height, bool has_transparency,
                        GoogleString* output, MessageHandler* handler) {
  // Create a PNG writer with no compression.
  PngCompressParams config(PNG_FILTER_NONE, Z_NO_COMPRESSION);
  PixelFormat pixel_format = (has_transparency ? RGBA_8888 : RGB_888);

  net_instaweb::scoped_ptr<ScanlineWriterInterface> png_writer(
      CreateScanlineWriter(IMAGE_PNG, pixel_format, width, height, &config,
                           output, handler));
  if (png_writer == NULL) {
    PS_LOG_ERROR(handler, "Failed to create an image writer.");
    return false;
  }

  // Create a transparent scanline.
  const size_t bytes_per_scanline = width *
    GetNumChannelsFromPixelFormat(pixel_format, handler);
  net_instaweb::scoped_array<unsigned char> scanline(
      new unsigned char[bytes_per_scanline]);
  memset(scanline.get(), 0, bytes_per_scanline);

  // Fill the entire image with the blank scanline.
  for (int row = 0; row < static_cast<int>(height); ++row) {
    if (!png_writer->WriteNextScanline(
        reinterpret_cast<void*>(scanline.get()))) {
      return false;
    }
  }

  if (!png_writer->FinalizeWrite()) {
    return false;
  }
  return true;
}

bool ShouldConvertToProgressive(int64 quality, int threshold,
                                int num_bytes, int desired_width,
                                int desired_height) {
  bool progressive = false;

  if (num_bytes >= threshold) {
    progressive = true;
    int num_pixels = desired_width * desired_height;

    double ratio = JpegPixelToByteRatio(quality);
    int64 estimated_bytes = num_pixels * ratio;

    if (estimated_bytes < threshold) {
      progressive = false;
    }
  }
  return progressive;
}

}  // namespace image_compression

}  // namespace pagespeed
