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

#ifndef PAGESPEED_KERNEL_IMAGE_IMAGE_RESIZER_H_
#define PAGESPEED_KERNEL_IMAGE_IMAGE_RESIZER_H_

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"

namespace pagespeed {

namespace image_compression {

using net_instaweb::MessageHandler;

class ResizeRow;
class ResizeCol;

// Class ScanlineResizer resizes an image, and outputs a scanline at a time.
// To use it, you need to provide an initialized reader implementing
// ScanlineReaderInterface. The ScanlineResizer object will instruct the reader
// to fetch the image scanlines required for the resized scanline.
//
// You can specify the width, the height, or both in pixels. If you want to
// preserve the aspect ratio, you can specify only one of them, and pass in
// kPreserveAspectRatio for the other one.
//
// Currently, ScanlineResizer only supports shrinking. It works best when the
// image shrinks significantly, e.g, by more than 2x times.
class ScanlineResizer : public ScanlineReaderInterface {
 public:
  explicit ScanlineResizer(MessageHandler* handler);
  ~ScanlineResizer() override;

  // Initializes the resizer with a reader and the desired output size.
  bool Initialize(ScanlineReaderInterface* reader, size_t output_width,
                  size_t output_height);

  // Reads the next available scanline. Returns an error if the next scanline
  // is not available. This can happen when the reader cannot provide enough
  // image rows, or when all of the scanlines have been read.
  ScanlineStatus ReadNextScanlineWithStatus(void** out_scanline_bytes) override;

  // Resets the resizer to its initial state. Always returns true.
  bool Reset() override;

  // Returns number of bytes required to store a scanline.
  size_t GetBytesPerScanline() override {
    return static_cast<size_t>(elements_per_row_);
  }

  // Returns true if there are more scanlines to read. Returns false if the
  // object has not been initialized or all of the scanlines have been read.
  bool HasMoreScanLines() override;

  // Returns the height of the image.
  size_t GetImageHeight() override { return static_cast<size_t>(height_); }

  // Returns the width of the image.
  size_t GetImageWidth() override { return static_cast<size_t>(width_); }

  // Returns the pixel format of the image.
  PixelFormat GetPixelFormat() override { return reader_->GetPixelFormat(); }

  // Returns true if the image is encoded in progressive / interlacing format.
  bool IsProgressive() override { return reader_->IsProgressive(); }

  // This method should not be called. If it does get called, in DEBUG mode it
  // will throw a FATAL error and in RELEASE mode it does nothing.
  ScanlineStatus InitializeWithStatus(const void* image_buffer,
                                      size_t buffer_length) override;

  static const size_t kPreserveAspectRatio = 0;

 private:
  ScanlineReaderInterface* reader_;
  // Horizontal resizer.
  std::unique_ptr<ResizeRow> resizer_x_;
  // Vertical resizer.
  std::unique_ptr<ResizeCol> resizer_y_;

  net_instaweb::scoped_array<uint8> output_;
  int width_;
  int height_;
  int elements_per_row_;

  // Buffer for storing the intermediate results.
  net_instaweb::scoped_array<float> buffer_;
  int bytes_per_buffer_row_;
  MessageHandler* message_handler_;

  DISALLOW_COPY_AND_ASSIGN(ScanlineResizer);
};

}  // namespace image_compression

}  // namespace pagespeed

#endif  // PAGESPEED_KERNEL_IMAGE_IMAGE_RESIZER_H_
