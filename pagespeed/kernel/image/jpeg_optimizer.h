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


#ifndef PAGESPEED_KERNEL_IMAGE_JPEG_OPTIMIZER_H_
#define PAGESPEED_KERNEL_IMAGE_JPEG_OPTIMIZER_H_

#include <setjmp.h>
#include <cstddef>
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"

// DO NOT INCLUDE LIBJPEG HEADERS HERE. Doing so causes build errors
// on Windows.

namespace net_instaweb {
class MessageHandler;
}

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

enum ColorSampling {
  RETAIN,
  YUV420,
  YUV422,
  YUV444
};

struct JpegLossyOptions {
  JpegLossyOptions() : quality(85), num_scans(-1), color_sampling(YUV420) {}
  // jpeg_quality - Can take values in the range [1,100].
  // For web images, the preferred value for quality is 85.
  // For smaller images like thumbnails, the preferred value for quality is 75.
  // Setting it to values below 50 is generally not preferable.
  int quality;

  // No. of progressive scan that needs to be included in the final output.
  // -1 indicates to use all scans that are present.
  int num_scans;

  // Color sampling that needs to be used while recompressing the image.
  ColorSampling color_sampling;
};

struct JpegCompressionOptions : public ScanlineWriterConfig {
  JpegCompressionOptions()
    : progressive(false), retain_color_profile(false),
      retain_exif_data(false), lossy(false) {}

  ~JpegCompressionOptions() override;

  // Whether or not to produce a progressive JPEG. This parameter will only be
  // applied for images with YCbCr colorspace, and it is ignored for other
  // colorspaces.
  bool progressive;

  // If set to 'true' any color profile information is retained.
  bool retain_color_profile;

  // If set to 'true' any exif information is retained.
  bool retain_exif_data;

  // Whether or not to use lossy compression.
  bool lossy;

  // Lossy compression options. Only applicable if lossy (above) is set to true.
  JpegLossyOptions lossy_options;
};

// Performs lossless optimization, that is, the output image will be
// pixel-for-pixel identical to the input image.
bool OptimizeJpeg(const GoogleString &original,
                  GoogleString *compressed,
                  MessageHandler* handler);

// Performs JPEG optimizations with the provided options.
bool OptimizeJpegWithOptions(const GoogleString &original,
                             GoogleString *compressed,
                             const JpegCompressionOptions &options,
                             MessageHandler* handler);

// User of this class must call this functions in the following sequence
// func () {
//   JpegScanlineWriter jpeg_writer;
//   jmp_buf env;
//   if (setjmp(env)) {
//     jpeg_writer.AbortWrite();
//     return;
//   }
//   jpeg_writer.SetJmpBufEnv(&env);
//   if (jpeg_writer.Init(width, height, format)) {
//     jpeg_writer.SetJpegCompressParams(quality);
//     jpeg_writer.InitializeWrite(out);
//     while(has_lines_to_write) {
//       writer.WriteNextScanline(next_scan_line);
//     }
//     writer.FinalizeWrite()
//   }
// }
class JpegScanlineWriter : public ScanlineWriterInterface {
 public:
  explicit JpegScanlineWriter(MessageHandler* handler);
  virtual ~JpegScanlineWriter();

  // Set the environment for longjmp calls.
  void SetJmpBufEnv(jmp_buf* env);

  // This function is only called when jpeg library call longjmp for
  // cleaning up the jpeg structs.
  void AbortWrite();

  virtual ScanlineStatus InitWithStatus(const size_t width, const size_t height,
                                        PixelFormat pixel_format);
  // Set the compression options via 'params', which should be a
  // JpegCompressionOptions*. Since writer only supports lossy
  // encoding, it is an error to pass in a 'params' that has the lossy
  // field set to false.
  virtual ScanlineStatus InitializeWriteWithStatus(const void* params,
                                                   GoogleString *compressed);
  virtual ScanlineStatus WriteNextScanlineWithStatus(
      const void *scanline_bytes);
  virtual ScanlineStatus FinalizeWriteWithStatus();

 private:
  // Since writer only supports lossy encoding, it is an error to pass
  // in a compression options that has lossy field set to false.
  void SetJpegCompressParams(const JpegCompressionOptions& options);

  // Opaque struct that is defined in the cc file and contains our
  // JPEG-compressor-specific structures.
  struct Data;
  Data* const data_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(JpegScanlineWriter);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_JPEG_OPTIMIZER_H_
