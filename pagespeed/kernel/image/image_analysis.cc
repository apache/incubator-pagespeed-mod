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


#include "pagespeed/kernel/image/image_analysis.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "base/logging.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/jpeg_utils.h"
#include "pagespeed/kernel/image/pixel_format_optimizer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_interface_frame_adapter.h"
#include "pagespeed/kernel/image/scanline_status.h"
#include "pagespeed/kernel/image/scanline_utils.h"

namespace pagespeed {

namespace {

// Threshold for histogram. The histogram bins with values less than
// (max_hist_bin * kHistogramThreshold) will be ignored in computing
// the photo metric. Values of 0.005, 0.01, and 0.02 have been tried and
// the best one is 0.01.
const float kHistogramThreshold = 0.01;

// Minimum metric value in order to be treated as a photo. The recommended
// value, 16, was found by examining about 1000 PNG images with no alpha
// or a completely opaque alpha channel.
const float kPhotoMetricThreshold = 16;

template <class T>
inline T AbsDif(T v1, T v2) {
  return (v1 >= v2 ? v1 - v2 : v2 - v1);
}

}  // namespace

namespace image_compression {

// Compute the gradient by Sobel filter. The kernels in the x and y
// directions, respectively, are given by:
//   [  1  2  1 ]        [ 1 0 -1 ]
//   [  0  0  0 ]        [ 2 0 -2 ]
//   [ -1 -2 -1 ]        [ 1 0 -1 ]
template<class T>
void ComputeGradientFromLuminance(const T* luminance, int width, int height,
                                  int elements_per_line, float norm_factor,
                                  uint8_t* gradient) {
  memset(gradient, 0, width * height * sizeof(gradient[0]));
  norm_factor *= 0.25;  // Remove the magnification factor of Sobel filter (4).
  for (int y = 1; y < height - 1; ++y) {
    int in_idx = y * elements_per_line + 1;
    int out_idx = y * width + 1;
    for (int x = 1; x < width - 1; ++x, ++in_idx, ++out_idx) {
      int32_t dif_y =
          static_cast<int32_t>(luminance[in_idx - elements_per_line - 1]) +
          (static_cast<int32_t>(luminance[in_idx - elements_per_line]) << 1) +
          static_cast<int32_t>(luminance[in_idx - elements_per_line + 1]) -
          static_cast<int32_t>(luminance[in_idx + elements_per_line - 1]) -
          (static_cast<int32_t>(luminance[in_idx + elements_per_line]) << 1) -
          static_cast<int32_t>(luminance[in_idx + elements_per_line + 1]);

      int32_t dif_x =
          static_cast<int32_t>(luminance[in_idx - 1 - elements_per_line]) +
          (static_cast<int32_t>(luminance[in_idx - 1]) << 1) +
          static_cast<int32_t>(luminance[in_idx - 1 + elements_per_line]) -
          static_cast<int32_t>(luminance[in_idx + 1 - elements_per_line]) -
          (static_cast<int32_t>(luminance[in_idx + 1]) << 1) -
          static_cast<int32_t>(luminance[in_idx + 1 + elements_per_line]);

      // The results of "dif_x * dif_x + dif_y * dif_y" will not overflow
      // because the data in dif_x and dif_y have at most 12 bits.
      float dif2 = static_cast<float>(dif_x * dif_x + dif_y * dif_y);
      float dif = std::sqrt(dif2) * norm_factor + 0.5f;
      gradient[out_idx] = static_cast<uint8_t>(std::min(255.0f, dif));
    }
  }
}

bool SobelGradient(const uint8_t* image, int width, int height,
                   int bytes_per_line, PixelFormat pixel_format,
                   MessageHandler* handler, uint8_t* gradient) {
  if (width < 3 || height < 3 ||
      (pixel_format != GRAY_8 && pixel_format != RGB_888 &&
       pixel_format != RGBA_8888)) {
    return false;
  }

  if (pixel_format == GRAY_8) {
    const float norm_factor = 1.0f;
    ComputeGradientFromLuminance(image, width, height, bytes_per_line,
                                 norm_factor, gradient);
  } else {
    int32_t* luminance = static_cast<int32_t*>(malloc(width * height *
                                                      sizeof(int32_t)));
    if (luminance == NULL) {
      return false;
    }

    const int num_channels =
      GetNumChannelsFromPixelFormat(pixel_format, handler);

    // Compute the luminance which is simply the average of R, G, and B
    // after applying the normalization factor.
    int32_t* out_pixel = luminance;
    for (int y = 0; y < height; ++y) {
      const uint8_t* in_channel = image + y * bytes_per_line;
      for (int x = 0; x < width; ++x) {
        *out_pixel = static_cast<int32_t>(in_channel[0]) +
                     static_cast<int32_t>(in_channel[1]) +
                     static_cast<int32_t>(in_channel[2]);
        ++out_pixel;
        in_channel += num_channels;
      }
    }

    const float norm_factor = 1.0f / 3.0f;
    ComputeGradientFromLuminance(luminance, width, height, width, norm_factor,
                                 gradient);
    free(luminance);
  }
  return true;
}

void Histogram(const uint8_t* image, int width, int height, int bytes_per_line,
               int x0, int y0, float* hist) {
  DCHECK(bytes_per_line >= width);

  uint32_t hist_int[kNumColorHistogramBins];
  memset(hist_int, 0, kNumColorHistogramBins * sizeof(hist_int[0]));

  // Aggregate the histogram.
  for (int y = y0; y < y0 + height; ++y) {
    int i = y * bytes_per_line + x0;
    for (int x = 0; x < width; ++x, ++i) {
      ++hist_int[image[i]];
    }
  }

  for (int i = 0; i < kNumColorHistogramBins; ++i) {
    hist[i] = static_cast<float>(hist_int[i]);
  }
}

float WidestPeakWidth(const float* hist, float threshold) {
  float max_hist = *std::max_element(hist, hist + kNumColorHistogramBins);
  float threshold_hist = threshold * max_hist;

  int widest_peak = 0;
  int i = 0;
  while (i < kNumColorHistogramBins) {
    // Skip all bins which are smaller than the threshold.
    for (; i < kNumColorHistogramBins && hist[i] < threshold_hist; ++i) {
    }
    // Now we have a bin which meets the threshold, or we have finished
    // all of the bins.
    int first_significant_bin = i;
    for (; i < kNumColorHistogramBins && hist[i] >= threshold_hist; ++i) {
    }
    // Now we have gone through a peak or we have run out of bins. We will
    // check whether it is wider than all of the previous ones.
    float width = i - first_significant_bin;
    if (widest_peak < width) {
      widest_peak = width;
    }
  }

  return widest_peak;
}

float PhotoMetric(const uint8_t* image, int width, int height,
                  int bytes_per_line, PixelFormat pixel_format,
                  float threshold, MessageHandler* handler) {
  const float KMinMetric = 0;

  uint8_t* gradient = static_cast<uint8_t*>(malloc(width * height *
                                                   sizeof(uint8_t)));
  if (gradient == NULL) {
    return KMinMetric;
  }

  if (!SobelGradient(image, width, height, bytes_per_line, pixel_format,
                     handler, gradient)) {
    // Conservatively assume that the image is computer generated graphics if we
    // cannot compute its gradient.
    free(gradient);
    return KMinMetric;
  }

  float hist[kNumColorHistogramBins];
  Histogram(gradient, width-2, height-2, width, 1, 1, hist);
  free(gradient);
  return WidestPeakWidth(hist, threshold);
}

bool IsPhoto(ScanlineReaderInterface* reader, MessageHandler* handler) {
  // Pretend that the image is not a photo if we cannot process it.
  bool kDefaultReturnValue = false;

  // If we cannot process the image or if the image has non-opaque alpha
  // channel, return false (i.e., not a photo). Most (>99%) images with
  // non-opaque alpha channel are not photo.
  if (reader->GetPixelFormat() == UNSUPPORTED ||
      reader->GetPixelFormat() == RGBA_8888 ||
      reader->GetImageWidth() == 0 || reader->GetImageHeight() == 0) {
    return kDefaultReturnValue;
  }

  const int width = reader->GetImageWidth();
  const int height = reader->GetImageHeight();
  const PixelFormat pixel_format = reader->GetPixelFormat();
  const int bytes_per_line = width *
      GetNumChannelsFromPixelFormat(pixel_format, handler);

  uint8_t* image = static_cast<uint8_t*>(malloc(bytes_per_line * height *
                                                sizeof(uint8_t)));
  if (image == NULL) {
    return kDefaultReturnValue;
  }

  for (int y = 0; y < height; ++y) {
    uint8_t* scanline = NULL;
    if (!reader->HasMoreScanLines() ||
        !reader->ReadNextScanline(reinterpret_cast<void**>(&scanline))) {
      free(image);
      return kDefaultReturnValue;
    }
    memcpy(image + y * bytes_per_line, scanline, bytes_per_line);
  }

  float metric = PhotoMetric(image, width, height, bytes_per_line,
                             pixel_format, kHistogramThreshold, handler);
  free(image);
  return metric >= kPhotoMetricThreshold;
}

bool AnalyzeImage(ImageFormat image_type,
                  const void* image_buffer,
                  size_t buffer_length,
                  int* width,
                  int* height,
                  bool* is_progressive,
                  bool* is_animated,
                  bool* has_transparency,
                  bool* is_photo,
                  int* quality,
                  ScanlineReaderInterface** reader,
                  MessageHandler* handler) {
  net_instaweb::scoped_ptr<ScanlineReaderInterface> sf_reader;
  net_instaweb::scoped_ptr<PixelFormatOptimizer> optimizer;
  bool image_is_animated = false;
  int image_width = 0;
  int image_height = 0;
  bool image_is_progressive = false;
  ScanlineStatus status;
  if (image_type != IMAGE_GIF) {
    // PNG and JPEG images only have a single frame. WebP may have multiple
    // frames but that is rare, so they will not be analyzed.

    // TODO(huibao): Upgrade WebpScanlineReader to support multiple frame
    // WebP images.
    sf_reader.reset(CreateScanlineReader(image_type, image_buffer,
                                         buffer_length, handler));
    if (sf_reader == NULL) {
      return false;
    }
  } else {
    // GIF images may have multiple frames (animation). If it has multiple
    // frames, we can only get its width and height; if not, we can convert
    // it to a scanline reader and find out whether it is a photo and/or
    // transparent.
    net_instaweb::scoped_ptr<MultipleFrameReader> mf_reader(
        CreateImageFrameReader(image_type, image_buffer, buffer_length,
                               handler, &status));
    if (mf_reader == NULL) {
      return false;
    }

    ImageSpec image_spec;
    if (!mf_reader->GetImageSpec(&image_spec, &status)) {
      return false;
    }
    image_is_animated = (image_spec.num_frames > 1);

    if (image_is_animated) {
      image_width = image_spec.width;
      image_height = image_spec.height;
    } else {
      sf_reader.reset(new FrameToScanlineReaderAdapter(mf_reader.release()));
      if (sf_reader == NULL) {
        return false;
      }
      status = sf_reader->InitializeWithStatus(image_buffer, buffer_length);
      if (!status.Success()) {
        return false;
      }
    }
  }

  if (!image_is_animated) {
    image_width = sf_reader->GetImageWidth();
    image_height = sf_reader->GetImageHeight();
    image_is_progressive = sf_reader->IsProgressive();
  }

  // No matter how many frames the image has, we can always find out whether it
  // is animated, its width, and its height.
  if (is_animated != NULL) {
    *is_animated = image_is_animated;
  }
  if (width != NULL) {
    *width = image_width;
  }
  if (height != NULL) {
    *height = image_height;
  }
  if (is_progressive != NULL) {
    *is_progressive = image_is_progressive;
  }

  // Finding whether the image is transparent or photo requires processing
  // the entire image. We do this only when it's requested. We also do this
  // only for single frame images now, because for mutliple frames images each
  // frame may have different attributes.
  //
  // TODO(huibao): Enhance PixelFormatOptimizer and IsPhoto() so they support
  // MultipleFrameReader. PixelFormatOptimizer may return unique attributes
  // for each frame since the frames may have different values. IsPhoto() may
  // return a single value for all of the frames because it is unlikely that
  // the image consists of both photos and graphics.
  if (sf_reader != NULL && (has_transparency != NULL || is_photo != NULL)) {
    // Initialize the optimizer which will remove alpha channel if it is
    // completely opaque.
    optimizer.reset(new PixelFormatOptimizer(handler));
    if (!optimizer->Initialize(sf_reader.release()).Success()) {
      return false;
    }

    // Report the interesting information of the optimized image.
    if (has_transparency != NULL) {
      *has_transparency = (optimizer->GetPixelFormat() == RGBA_8888);
    }
    if (is_photo != NULL) {
      if (image_type == IMAGE_JPEG) {
        // Assume all JPEG images are photos. JPEG is the most popular format
        // in internet and most of them have photo content. For the very few
        // JPEG image with graphics content, we can't really improve the quality
        // by losslessly encoding them, so we simply assume all of them to
        // be photos, in order to save computations.
        *is_photo = true;
      } else {
        // IsPhoto will read all scanlines of the image, so optimizer cannot be
        // used anymore.
        *is_photo = IsPhoto(optimizer.get(), handler);
        optimizer.reset();
      }
    }
  }

  if (quality != NULL && image_type == IMAGE_JPEG) {
    *quality = JpegUtils::GetImageQualityFromImage(image_buffer, buffer_length,
                                                   handler);
    // TODO(huibao): Add utility for finding quality number from WebP images
    // and apply it here.
  }

  // If "reader" has been requested, the caller is responsible for destroying
  // it.
  if (reader != NULL) {
    if (optimizer != NULL) {
      *reader = optimizer.release();
    } else if (sf_reader != NULL) {
      *reader = sf_reader.release();
    } else {
      *reader = NULL;
    }
  }

  return true;
}

}  // namespace image_compression

}  // namespace pagespeed
