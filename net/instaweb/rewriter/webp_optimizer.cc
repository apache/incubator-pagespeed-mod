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


#include "net/instaweb/rewriter/public/webp_optimizer.h"

#include <csetjmp>
#include <cstddef>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/jpeg_reader.h"
#include "pagespeed/kernel/image/jpeg_utils.h"

extern "C" {
#ifdef USE_SYSTEM_LIBWEBP
#include "webp/decode.h"
#include "webp/encode.h"
#else
#include "third_party/libwebp/src/webp/decode.h"
#include "third_party/libwebp/src/webp/encode.h"
#endif
// TODO(jmaessen): open source imports & build of libwebp.
}

extern "C" {
#ifdef USE_SYSTEM_LIBJPEG
#include "jpeglib.h"  // NOLINT
#else
#include "third_party/libjpeg_turbo/src/jpeglib.h"
#endif
}

using pagespeed::image_compression::JpegUtils;

namespace net_instaweb {

class MessageHandler;

namespace {

// Whether to enable support for YUV -> YUV conversion.  Is currently disabled,
// as trials showed colorspace mismatches in jpeg versus webp.
const bool kUseYUV = false;

// The YUV samples emerging from libjpeg are packed in that order (rather than
// being represented as three distinct planes, which is what libwebp does).
// We define the following constants to make reading array indexing code
// marginally easier.
const int kYPlane = 0;
const int kUPlane = 1;
const int kVPlane = 2;
const int kPlanes = 3;

#ifdef DCT_IFAST_SUPPORTED
const J_DCT_METHOD fastest_dct_method = JDCT_IFAST;
#else
#ifdef DCT_FLOAT_SUPPORTED
// const J_DCT_METHOD fastest_dct_method = JDCT_FLOAT;
#else
// const J_DCT_METHOD fastest_dct_method = JDCT_ISLOW;
#endif
#endif


int GoogleStringWebpWriter(const uint8_t* data, size_t data_size,
                           const WebPPicture* const picture) {
  GoogleString* compressed_webp =
      static_cast<GoogleString*>(picture->custom_ptr);
  // The cast below deals with char signedness issues (data is always unsigned)
  compressed_webp->append(reinterpret_cast<const char*>(data), data_size);
  return 1;
}

class WebpOptimizer {
 public:
  explicit WebpOptimizer(MessageHandler* handler);
  ~WebpOptimizer();

  // Take the given input file and transcode it to webp.
  // Return true on success.
  bool CreateOptimizedWebp(const GoogleString& original_jpeg,
                           int configured_quality,
                           WebpProgressHook progress_hook,
                           void* progress_hook_data,
                           GoogleString* compressed_webp);

 private:
  // Compute the offset of a pixel sample given x and y position.
  size_t PixelOffset(size_t x, size_t y) const {
    return (kPlanes * x + y * row_stride_);
  }
  // Fetch a pixel sample from the given plane and offset, modified by the given
  // 0/1 x and y offsets.  Note that all the arguments here are expected to be
  // constant except source_offset.
  int SampleAt(int plane, int source_offset, int x_offset, int y_offset) const {
    return static_cast<int>(pixels_[plane + source_offset +
                                    PixelOffset(x_offset, y_offset)]);
  }
  bool DoReadJpegPixels(J_COLOR_SPACE color_space,
                        const GoogleString& original_jpeg);
  bool ReadJpegPixels(J_COLOR_SPACE color_space,
                      const GoogleString& original_jpeg);
  bool WebPImportYUV(WebPPicture* const picture);

  // The function to be called by libwebp's progress hook (with 'this'
  // as the user data), which in turn will call the user-supplied function
  // in progress_hook_, passing it progress_hook_data_.
  static int ProgressHook(int percent, const WebPPicture* picture);

  // Structure for jpeg decompression
  MessageHandler* message_handler_;
  pagespeed::image_compression::JpegReader reader_;
  uint8* pixels_;
  uint8** rows_;  // Holds offsets into pixels_ during decompression
  unsigned int width_, height_;  // Type-compatible with libjpeg.
  size_t row_stride_;

  // Structures for webp recompression
  WebpProgressHook progress_hook_;
  void* progress_hook_data_;

  DISALLOW_COPY_AND_ASSIGN(WebpOptimizer);
};  // class WebpOptimizer

WebpOptimizer::WebpOptimizer(MessageHandler* handler)
    : message_handler_(handler),
      reader_(handler),
      pixels_(NULL),
      rows_(NULL),
      width_(0),
      height_(0),
      row_stride_(0),
      progress_hook_(NULL),
      progress_hook_data_(NULL) {
}

WebpOptimizer::~WebpOptimizer() {
  delete[] pixels_;
  DCHECK(rows_ == NULL);
}

// Does most of the work of ReadJpegPixels (see below); errors transfer control
// out so that we can clean up properly.
bool WebpOptimizer::DoReadJpegPixels(J_COLOR_SPACE color_space,
                                     const GoogleString& original_jpeg) {
  // Set up jpeg error handling.
  jmp_buf env;
  if (setjmp(env)) {
    // We get here if libjpeg encountered a decompression error.
    return false;
  }
  // Install env so that it is longjmp'd to on error:
  jpeg_decompress_struct* jpeg_decompress = reader_.decompress_struct();
  jpeg_decompress->client_data = static_cast<void*>(&env);

  reader_.PrepareForRead(original_jpeg.data(), original_jpeg.size());

  if (jpeg_read_header(jpeg_decompress, TRUE) != JPEG_HEADER_OK) {
    return false;
  }

  // Settings largely cribbed from the cwebp.c example source code.
  // Difference: we ask for YCbCr as the out_color_space.  Not sure
  // why RGB is used in the command line utility.  Is this so we handle
  // non-YCbCr jpegs gracefully without additional checking?
  jpeg_decompress->out_color_space = color_space;
  // For whatever reason, libjpeg doesn't always seem to define JDCT_FASTEST to
  // match a *configured, working* dct method (which makes this symbol pretty
  // pointless, actually)!  As a result, we end up having to use the default
  // (slow and conservative) method.  This is horrible and broken.
  //  jpeg_decompress->dct_method = JDCT_FASTEST;
  jpeg_decompress->do_fancy_upsampling = TRUE;

  if (!jpeg_start_decompress(jpeg_decompress) ||
      jpeg_decompress->output_components != kPlanes) {
    return false;
  }

  // Figure out critical dimensions of image, and allocate space for image data.
  width_ = jpeg_decompress->output_width;
  height_ = jpeg_decompress->output_height;
  row_stride_ = width_ * jpeg_decompress->output_components * sizeof(*pixels_);

  pixels_ = new uint8[row_stride_ * height_];
  // jpeglib expects to get an array of pointers to rows, so allocate one and
  // point it to contiguous rows in *pixels_.
  rows_ = new uint8*[height_];
  for (unsigned int i = 0; i < height_; ++i) {
    rows_[i] = pixels_ + PixelOffset(0, i);
  }
  while (jpeg_decompress->output_scanline < height_) {
    // Try to read all remaining lines; we should get as many as the library is
    // comfortable handing over at one go.
    int rows_read =
        jpeg_read_scanlines(jpeg_decompress,
                            rows_ + jpeg_decompress->output_scanline,
                            height_ - jpeg_decompress->output_scanline);
    if (rows_read == 0) {
      return false;
    }
  }
  return jpeg_finish_decompress(jpeg_decompress);
}

// Initialize width_, height_, row_stride_, and pixels_ with data from the
// jpeg_decompress structure.  Returns a status for errors that are caught in
// our code.  Jpeglib errors are handled by longjmp-ing to internal handler
// code.  We rely on the destructor to clean up pixel data after an error.
//
// Most of the work is done in DoReadJpegPixels, with errors ending up out here
// where we can clean them up.  This avoids stack variable trouble if
// decompression fails and longjmps.
bool WebpOptimizer::ReadJpegPixels(J_COLOR_SPACE color_space,
                                   const GoogleString& original_jpeg) {
  bool read_ok = DoReadJpegPixels(color_space, original_jpeg);
  delete[] rows_;
  rows_ = NULL;
  jpeg_decompress_struct* jpeg_decompress = reader_.decompress_struct();
  // NULL out the setjmp information stored by DoReadJpegPixels; there should be
  // no further decompression failures, and the stack would be invalid if there
  // were.
  jpeg_decompress->client_data = NULL;
  jpeg_destroy_decompress(jpeg_decompress);
  return read_ok;
}

// Import YUV pixels_ into *picture, downsampling UV as appropriate.  This is
// based on the RGB downsampling code in libwebp v0.2 src/enc/picture.c, but
// there's annoyingly no YUV downsampling code there.
// If WebPImportYUV succeeds, picture will have bitmaps allocated and must
// be cleaned up using WebPPictureFree(...).
bool WebpOptimizer::WebPImportYUV(WebPPicture* const picture) {
  if (!WebPPictureAlloc(picture)) {
    return false;
  }
  // Luma (Y) import
  for (size_t y = 0; y < height_; ++y) {
    for (size_t x = 0; x < width_; ++x) {
      picture->y[x + y * picture->y_stride] =
          pixels_[kYPlane + PixelOffset(x, y)];
    }
  }
  // Downsample U and V, handling boundaries.  Better averaging is a TODO
  // in the webp code, so this may need to change in future.
  unsigned int half_height = height_ >> 1;
  unsigned int half_width = width_ >> 1;
  unsigned int extra_height = height_ & 1;
  unsigned int extra_width = width_ & 1;
  size_t x, y;
  // Note that to preserve similar structure in the edge cases below We rely on
  // overincrement of x and y and use them after the loop terminates.  This
  // should make it easier to understand the loop structures.
  for (y = 0; y < half_height; ++y) {
    for (x = 0; x < half_width; ++x) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0) +
          SampleAt(kUPlane, source_offset, 1, 0) +
          SampleAt(kUPlane, source_offset, 0, 1) +
          SampleAt(kUPlane, source_offset, 1, 1);
      picture->u[picture_offset] = (2 + pixel_sum_u) >> 2;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0) +
          SampleAt(kVPlane, source_offset, 1, 0) +
          SampleAt(kVPlane, source_offset, 0, 1) +
          SampleAt(kVPlane, source_offset, 1, 1);
      picture->v[picture_offset] = (2 + pixel_sum_v) >> 2;
    }
    // Note: x == half_width
    if (extra_width != 0) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0) +
          SampleAt(kUPlane, source_offset, 0, 1);
      picture->u[picture_offset] = (1 + pixel_sum_u) >> 1;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0) +
          SampleAt(kVPlane, source_offset, 0, 1);
      picture->v[picture_offset] = (1 + pixel_sum_v) >> 1;
    }
  }
  if (extra_height != 0) {
    // Note: y == half_height
    for (x = 0; x < half_width; ++x) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0) +
          SampleAt(kUPlane, source_offset, 1, 0);
      picture->u[picture_offset] = (1 + pixel_sum_u) >> 1;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0) +
          SampleAt(kVPlane, source_offset, 1, 0);
      picture->v[picture_offset] = (1 + pixel_sum_v) >> 1;
    }
    // Note: x == half_width
    if (extra_width != 0) {
      int source_offset = PixelOffset(2 * x, 2 * y);
      int picture_offset = x + y * picture->uv_stride;
      int pixel_sum_u =
          SampleAt(kUPlane, source_offset, 0, 0);
      picture->u[picture_offset] = pixel_sum_u;
      int pixel_sum_v =
          SampleAt(kVPlane, source_offset, 0, 0);
      picture->v[picture_offset] = pixel_sum_v;
    }
  }
  return true;
}

int WebpOptimizer::ProgressHook(int percent, const WebPPicture* picture) {
  const WebpOptimizer* webp_optimizer =
      static_cast<WebpOptimizer*>(picture->user_data);
  return webp_optimizer->progress_hook_(percent,
                                        webp_optimizer->progress_hook_data_);
}

// Main body of transcode.
bool WebpOptimizer::CreateOptimizedWebp(
    const GoogleString& original_jpeg,
    int configured_quality,
    WebpProgressHook progress_hook,
    void* progress_hook_data,
    GoogleString* compressed_webp) {
  // Begin by making sure we can create a webp image at all:
  WebPPicture picture;
  WebPConfig config;
  int input_quality = JpegUtils::GetImageQualityFromImage(original_jpeg.data(),
                                                          original_jpeg.size(),
                                                          message_handler_);

  if (!WebPPictureInit(&picture) || !WebPConfigInit(&config)) {
    // Version mismatch.
    return false;
  }

  if (configured_quality == kNoQualityGiven) {
    // If configured quality is not available use the webp config
    // quality as the configured quality.
    configured_quality = config.quality;
  }

  int output_quality = configured_quality;

  if (input_quality != kNoQualityGiven && input_quality < configured_quality) {
      output_quality =  input_quality;
    } else {
    // If JpegUtils::GetImageQualityFromImage couldn't figure
    // out the quality or if the input quality is more than the configured
    // quality, use configured quality to rewrite.
      output_quality = configured_quality;
  }

  if (!WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, output_quality)) {
    // Couldn't use the default preset.
    return false;
  } else {
    // Set WebP compression method to 3 (4 is the default). From
    // third_party/libwebp/v0_2/src/webp/encode.h, the method determines the
    // 'quality/speed trade-off (0=fast, 6=slower-better). On a representative
    // set of images, we see a 26% improvement in the 75th percentile
    // compression time, even greater improvements further along the tail, and
    // no increase in file size. Method 2 incurs a prohibitive 10% increase in
    // file size, which is not worth the compression time savings.
    config.method = 3;
    if (!WebPValidateConfig(&config)) {
      return false;
    }
  }

  J_COLOR_SPACE color_space = kUseYUV ? JCS_YCbCr : JCS_RGB;

  if (!ReadJpegPixels(color_space, original_jpeg)) {
    return false;
  }

  // At this point, we're done reading the jpeg, and the color data
  // is stored in *pixels.  Now we just need to turn this into a webp.
  // Regardless of the import method we use, we need to set the picture
  // up beforehand as follows:
  picture.writer = &GoogleStringWebpWriter;
  picture.custom_ptr = static_cast<void*>(compressed_webp);
  picture.width = width_;
  picture.height = height_;
  if (progress_hook != NULL) {
    picture.progress_hook = ProgressHook;
    picture.user_data = this;
    progress_hook_ = progress_hook;
    progress_hook_data_ = progress_hook_data;
  }

  if (kUseYUV) {
    // pixels_ are YUV at full resolution; WebP requires us to downsample the U
    // and V planes explicitly (and store the three planes separately).
    if (!WebPImportYUV(&picture)) {
      return false;
    }
  } else if (!WebPPictureImportRGB(&picture, pixels_, row_stride_)) {
    return false;
  }

  // We're done with the original pixels, so clean them up.  If an error occurs,
  // this cleanup will happen in the destructor instead.
  delete[] pixels_;
  pixels_ = NULL;

  // Now we need to take picture and WebP encode it.
  bool result = WebPEncode(&config, &picture);

  // Clean up the picture and return status.
  WebPPictureFree(&picture);

  return result;
}

}  // namespace

bool OptimizeWebp(const GoogleString& original_jpeg, int configured_quality,
                  WebpProgressHook progress_hook, void* progress_hook_data,
                  GoogleString* compressed_webp,
                  MessageHandler* message_handler) {
  WebpOptimizer optimizer(message_handler);
  return optimizer.CreateOptimizedWebp(original_jpeg, configured_quality,
                                       progress_hook, progress_hook_data,
                                       compressed_webp);
}

// Helper function to initialize picture object from WebP decode buffer.
static bool WebPDecBufferToPicture(const WebPDecBuffer* const buf,
                                   WebPPicture* const picture) {
  const WebPYUVABuffer* const yuva = &buf->u.YUVA;
  if ((yuva->u_stride != yuva->v_stride) || (buf->colorspace != MODE_YUVA)) {
    return false;
  }
  picture->width = buf->width;
  picture->height = buf->height;
  picture->y = yuva->y;
  picture->u = yuva->u;
  picture->v = yuva->v;
  picture->a = yuva->a;
  picture->y_stride = yuva->y_stride;
  picture->uv_stride = yuva->u_stride;
  picture->a_stride = yuva->a_stride;
  picture->colorspace = WEBP_YUV420A;
  return true;
}

bool ReduceWebpImageQuality(const GoogleString& original_webp,
                            int quality, GoogleString* compressed_webp) {
  if (quality < 1) {
    // No compression.
    *compressed_webp = original_webp;
    return true;
  } else if (quality > 100) {
    quality = 100;
  }

  const uint8* webp = reinterpret_cast<const uint8*>(original_webp.data());
  const int webp_size = original_webp.size();
  // At the recommendation of skal@, we decompress and recompress in YUV(A)
  // space here. We used to do this for jpeg conversion (as evidenced by the
  // code above), but there are subtle differences between webp and jpeg YUV
  // space conversions that require an adjustment step that was never
  // implemented (see http://en.wikipedia.org/wiki/YCbCr).
  // Here, however, it makes conversions less lossy and allows us to operate
  // exclusively on the downsampled image, and of course we're operating in the
  // webp YUV(A) space in both cases.
  WebPConfig config;
  if (WebPConfigPreset(&config, WEBP_PRESET_DEFAULT, quality) == 0) {
    // Couldn't set up preset.
    return false;
  }
  WebPPicture picture;
  if (WebPPictureInit(&picture) == 0) {
    // Couldn't set up picture due to library version mismatch.
    return false;
  }

  WebPDecoderConfig dec_config;
  WebPInitDecoderConfig(&dec_config);
  WebPDecBuffer* const output_buffer = &dec_config.output;
  output_buffer->colorspace = MODE_YUVA;
  bool success = ((WebPDecode(webp, webp_size, &dec_config) == VP8_STATUS_OK) &&
                  WebPDecBufferToPicture(output_buffer, &picture));
  if (success) {
    picture.writer = &GoogleStringWebpWriter;
    picture.custom_ptr = reinterpret_cast<void*>(compressed_webp);

    success = WebPEncode(&config, &picture);
  }

  WebPFreeDecBuffer(output_buffer);

  return success;
}

}  // namespace net_instaweb
