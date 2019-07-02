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


#include "net/instaweb/rewriter/public/image.h"

#include <algorithm>
#include <cstddef>

extern "C" {
#ifdef USE_SYSTEM_ZLIB
#include "zlib.h"
#else
#include "third_party/zlib/src/zlib.h"
#endif
}  // extern "C"

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/image_data_lookup.h"
#include "net/instaweb/rewriter/public/image_url_encoder.h"
#include "net/instaweb/rewriter/public/webp_optimizer.h"
#include "pagespeed/kernel/base/annotated_message_handler.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/image/gif_reader.h"
#include "pagespeed/kernel/image/image_analysis.h"
#include "pagespeed/kernel/image/image_converter.h"
#include "pagespeed/kernel/image/image_frame_interface.h"
#include "pagespeed/kernel/image/image_resizer.h"
#include "pagespeed/kernel/image/image_util.h"
#include "pagespeed/kernel/image/jpeg_optimizer.h"
#include "pagespeed/kernel/image/jpeg_utils.h"
#include "pagespeed/kernel/image/png_optimizer.h"
#include "pagespeed/kernel/image/read_image.h"
#include "pagespeed/kernel/image/scanline_interface.h"
#include "pagespeed/kernel/image/scanline_status.h"
#include "pagespeed/kernel/image/scanline_utils.h"
#include "pagespeed/kernel/image/webp_optimizer.h"

extern "C" {
#ifdef USE_SYSTEM_LIBWEBP
#include "webp/decode.h"
#else
#include "third_party/libwebp/src/webp/decode.h"
#endif
#ifdef USE_SYSTEM_LIBPNG
#include "png.h"  // NOLINT
#else
#include "third_party/libpng/src/png.h"
#endif
}

using pagespeed::image_compression::AnalyzeImage;
using pagespeed::image_compression::ConversionTimeoutHandler;
using pagespeed::image_compression::CreateScanlineReader;
using pagespeed::image_compression::CreateScanlineWriter;
using pagespeed::image_compression::GifReader;
using pagespeed::image_compression::GRAY_8;
using pagespeed::image_compression::ImageConverter;
using pagespeed::image_compression::ImageFormat;
using pagespeed::image_compression::ImageFormatToString;
using pagespeed::image_compression::JpegCompressionOptions;
using pagespeed::image_compression::JpegScanlineWriter;
using pagespeed::image_compression::JpegUtils;
using pagespeed::image_compression::OptimizeJpegWithOptions;
using pagespeed::image_compression::PixelFormat;
using pagespeed::image_compression::PngCompressParams;
using pagespeed::image_compression::PngOptimizer;
using pagespeed::image_compression::PngReader;
using pagespeed::image_compression::PngReaderInterface;
using pagespeed::image_compression::PngScanlineWriter;
using pagespeed::image_compression::PreferredLibwebpLevel;
using pagespeed::image_compression::RETAIN;
using pagespeed::image_compression::RGB_888;
using pagespeed::image_compression::RGBA_8888;
using pagespeed::image_compression::ScanlineReaderInterface;
using pagespeed::image_compression::ScanlineResizer;
using pagespeed::image_compression::ScanlineWriterInterface;
using pagespeed::image_compression::WebpConfiguration;
using pagespeed::image_compression::WEBP_NONE;
using pagespeed::image_compression::WEBP_LOSSY;
using pagespeed::image_compression::WEBP_LOSSLESS;
using pagespeed::image_compression::WEBP_ANIMATED;

namespace net_instaweb {

namespace ImageHeaders {

const char kPngHeader[] = "\x89PNG\r\n\x1a\n";
const size_t kPngHeaderLength = STATIC_STRLEN(kPngHeader);
const char kPngIHDR[] = "\0\0\0\x0dIHDR";
const size_t kPngIntSize = 4;
const size_t kPngSectionHeaderLength = 2 * kPngIntSize;
const size_t kIHDRDataStart = kPngHeaderLength + kPngSectionHeaderLength;
const size_t kPngSectionMinSize = kPngSectionHeaderLength + kPngIntSize;
const size_t kPngColourTypeOffset = kIHDRDataStart + 2 * kPngIntSize + 1;
const char kPngAlphaChannel = 0x4;  // bit of ColourType set for alpha channel
const char kPngIDAT[] = "IDAT";
const char kPngtRNS[] = "tRNS";

const char kGifHeader[] = "GIF8";
const size_t kGifHeaderLength = STATIC_STRLEN(kGifHeader);
const size_t kGifDimStart = kGifHeaderLength + 2;
const size_t kGifIntSize = 2;

const size_t kJpegIntSize = 2;
const int64 kMaxJpegQuality = 100;
const int64 kQualityForJpegWithUnkownQuality = 85;

}  // namespace ImageHeaders

namespace {

const char kGifString[] = "gif";
const char kPngString[] = "png";
const uint8 kAlphaOpaque = 255;

void UpdateWebpStats(bool ok, bool was_timed_out, int64 time_elapsed_ms,
                     Image::ConversionVariables::VariableType var_type,
                     Image::ConversionVariables* conversion_vars) {
  if (conversion_vars != NULL) {
    Image::ConversionBySourceVariable* the_var = conversion_vars->Get(var_type);
    if (the_var != NULL) {
      if (was_timed_out) {
        the_var->timeout_count->Add(1);
        DCHECK(!ok);
      } else {
        if (ok) {
          the_var->success_ms->Add(time_elapsed_ms);
        } else {
          the_var->failure_ms->Add(time_elapsed_ms);
        }
      }
    }
  }
}

// TODO(huibao): Unify ImageType and ImageFormat.
ImageFormat ImageTypeToImageFormat(ImageType type) {
  ImageFormat format = pagespeed::image_compression::IMAGE_UNKNOWN;
  switch (type) {
    case IMAGE_UNKNOWN:
      format = pagespeed::image_compression::IMAGE_UNKNOWN;
      break;
    case IMAGE_JPEG:
      format = pagespeed::image_compression::IMAGE_JPEG;
      break;
    case IMAGE_PNG:
      format = pagespeed::image_compression::IMAGE_PNG;
      break;
    case IMAGE_GIF:
      format = pagespeed::image_compression::IMAGE_GIF;
      break;
    case IMAGE_WEBP:
    case IMAGE_WEBP_LOSSLESS_OR_ALPHA:
    case IMAGE_WEBP_ANIMATED:
      format = pagespeed::image_compression::IMAGE_WEBP;
      break;
  }
  return format;
}

ImageFormat GetOutputImageFormat(ImageFormat in_format) {
  if (in_format == pagespeed::image_compression::IMAGE_GIF) {
    return pagespeed::image_compression::IMAGE_PNG;
  } else {
    return in_format;
  }
}

ScanlineWriterInterface* CreateUncompressedPngWriter(
    size_t width, size_t height, GoogleString* output,
    MessageHandler* handler, bool use_transparent_for_blank_image) {
  PngCompressParams config(PNG_FILTER_NONE, Z_NO_COMPRESSION, false);
  PixelFormat pixel_format =
      use_transparent_for_blank_image ?
      RGBA_8888 :
      RGB_888;
  return CreateScanlineWriter(
      pagespeed::image_compression::IMAGE_PNG,
      pixel_format, width, height, &config, output, handler);
}

}  // namespace

// TODO(jmaessen): Put ImageImpl into private namespace.

class ImageImpl : public Image {
 public:
  ImageImpl(const StringPiece& original_contents,
            const GoogleString& url,
            const StringPiece& file_prefix,
            CompressionOptions* options,
            Timer* timer,
            MessageHandler* handler);
  ImageImpl(int width, int height, ImageType type,
            const StringPiece& tmp_dir,
            Timer* timer, MessageHandler* handler,
            CompressionOptions* options);

  virtual void Dimensions(ImageDim* natural_dim);
  virtual bool ResizeTo(const ImageDim& new_dim);
  virtual bool DrawImage(Image* image, int x, int y);
  virtual bool EnsureLoaded(bool output_useful);
  virtual bool ShouldConvertToProgressive(int64 quality) const;
  virtual void SetResizedDimensions(const ImageDim& dims) { dims_ = dims; }
  virtual void SetTransformToLowRes();
  virtual const GoogleString& url() { return url_; }
  virtual const GoogleString& debug_message() { return debug_message_; }
  virtual const GoogleString& resize_debug_message() {
    return resize_debug_message_;
  }

  void SetDebugMessageUrl(const GoogleString& url) override {
    // We add a space here so we can format-in empty one by default.
    debug_message_url_ = StrCat(" ", url);
  }

  bool GenerateBlankImage();

  StringPiece original_contents() { return original_contents_; }

 private:
  // Maximum number of libpagespeed conversion attempts.
  // TODO(vchudnov): Consider making this tunable.
  static const int kMaxConversionAttempts = 2;

  // Concrete helper methods called by parent class
  virtual void ComputeImageType();
  virtual bool ComputeOutputContents();

  bool ComputeOutputContentsFromGifOrPng(
      const GoogleString& string_for_image,
      const PngReaderInterface* png_reader,
      bool fall_back_to_png,
      const char* dbg_input_format,
      ImageType input_type,
      ConversionVariables::VariableType var_type);

  // Helper methods
  static bool ComputePngTransparency(const StringPiece& buf);

  // Internal methods used only in the implementation
  void UndoChange();
  void FindJpegSize();
  void FindPngSize();
  void FindGifSize();
  void FindWebpSize();

  // Convert the given options object to jpeg compression options.
  void ConvertToJpegOptions(const Image::CompressionOptions& options,
                            JpegCompressionOptions* jpeg_options);

  // Optimizes the png image_data, readable via png_reader.
  bool OptimizePng(
      const PngReaderInterface& png_reader,
      const GoogleString& image_data);

  // Converts image_data, readable via png_reader, to a jpeg if
  // possible or a png if not, using the settings in options_.
  bool OptimizePngOrConvertToJpeg(
      const PngReaderInterface& png_reader,
      const GoogleString& image_data);

  // Converts image_data, readable via png_reader, to a webp using the
  // settings in options_, if allowed by those settings. The alpha channel
  // is always losslessly compressed, while the color may be lossily or
  // or losslessly compressed, depending on 'compress_color_losslessly'.
  bool ConvertPngToWebp(
      const PngReaderInterface& png_reader,
      const GoogleString& image_data,
      bool compress_color_losslessly,
      bool has_transparency,
      ConversionVariables::VariableType var_type);

  // Convert the JPEG in original_jpeg to WebP format in
  // compressed_webp using the quality specified in
  // configured_quality.
  bool ConvertJpegToWebp(
      const GoogleString& original_jpeg, int configured_quality,
      GoogleString* compressed_webp);

  static bool ContinueWebpConversion(
      int percent,
      void* user_data);

  // Determines whether we can attempt a libpagespeed conversion
  // without exceeding kMaxConversionAttempts. If so, increments the
  // number of attempts.
  bool MayConvert() {
    if (options_.get()) {
      VLOG(1) << "Conversions attempted: "
              << options_->conversions_attempted;
      if (options_->conversions_attempted < kMaxConversionAttempts) {
        ++options_->conversions_attempted;
        return true;
      }
    }
    return false;
  }

  int GetJpegQualityFromImage(const StringPiece& contents) {
    const int quality = JpegUtils::GetImageQualityFromImage(contents.data(),
                                                            contents.size(),
                                                            handler_.get());
    return quality;
  }

  // Quality level for compressing the resized image.
  int EstimateQualityForResizedJpeg();

  bool ConvertAnimatedGifToWebp(bool has_transparency);

  const GoogleString file_prefix_;
  scoped_ptr<MessageHandler> handler_;
  bool changed_;
  const GoogleString url_;
  ImageDim dims_;
  ImageDim resized_dimensions_;
  GoogleString resized_image_;
  scoped_ptr<Image::CompressionOptions> options_;
  bool low_quality_enabled_;
  Timer* timer_;
  GoogleString debug_message_;
  GoogleString resize_debug_message_;
  GoogleString debug_message_url_;

  DISALLOW_COPY_AND_ASSIGN(ImageImpl);
};

void ImageImpl::SetTransformToLowRes() {
  // TODO(vchudnov): Deprecate low_quality_enabled_.
  low_quality_enabled_ = true;
  // TODO(vchudnov): All these settings should probably be tunable.
  if (options_->preferred_webp != WEBP_NONE) {
    options_->preferred_webp = WEBP_LOSSY;
  }
  options_->webp_quality = 10;
  options_->webp_animated_quality = 10;
  options_->jpeg_quality = 10;
}

Image::Image(const StringPiece& original_contents)
    : image_type_(IMAGE_UNKNOWN),
      original_contents_(original_contents),
      output_contents_(),
      output_valid_(false),
      rewrite_attempted_(false) { }

ImageImpl::ImageImpl(const StringPiece& original_contents,
                     const GoogleString& url,
                     const StringPiece& file_prefix,
                     CompressionOptions* options,
                     Timer* timer,
                     MessageHandler* handler)
    : Image(original_contents),
      file_prefix_(file_prefix.data(), file_prefix.size()),
      changed_(false),
      url_(url),
      options_(options),
      low_quality_enabled_(false),
      timer_(timer) {
  const GoogleString annotation = StrCat(url, ": ");
  handler_.reset(new AnnotatedMessageHandler(annotation, handler));
}

Image* NewImage(const StringPiece& original_contents,
                const GoogleString& url,
                const StringPiece& file_prefix,
                Image::CompressionOptions* options,
                Timer* timer,
                MessageHandler* handler) {
  return new ImageImpl(original_contents, url, file_prefix, options,
                       timer, handler);
}

Image::Image(ImageType type)
    : image_type_(type),
      original_contents_(),
      output_contents_(),
      output_valid_(false),
      rewrite_attempted_(false) { }

ImageImpl::ImageImpl(int width, int height, ImageType type,
                     const StringPiece& tmp_dir,
                     Timer* timer, MessageHandler* handler,
                     CompressionOptions* options)
    : Image(type),
      file_prefix_(tmp_dir.data(), tmp_dir.size()),
      changed_(false),
      low_quality_enabled_(false),
      timer_(timer) {
  options_.reset(options);
  dims_.set_width(width);
  dims_.set_height(height);
  handler_.reset(new AnnotatedMessageHandler(handler));
}

bool ImageImpl::GenerateBlankImage() {
  DCHECK(image_type_ == IMAGE_PNG) << "Blank image must be a PNG.";

  if (pagespeed::image_compression::GenerateBlankImage(dims_.width(),
      dims_.height(), options_->use_transparent_for_blank_image,
      &output_contents_, handler_.get())) {
    output_valid_ = true;
    return true;
  }
  return false;
}

Image* BlankImageWithOptions(int width, int height, ImageType type,
                             const StringPiece& tmp_dir,
                             Timer* timer, MessageHandler* handler,
                             Image::CompressionOptions* options) {
  scoped_ptr<ImageImpl> image(new ImageImpl(width, height, type, tmp_dir,
                                            timer, handler, options));
  if (image != NULL && image->GenerateBlankImage()) {
    return image.release();
  }
  return NULL;
}

Image::~Image() {
}

// Looks through blocks of jpeg stream to find SOFn block
// indicating encoding and dimensions of image.
// Loosely based on code and FAQs found here:
//    http://www.faqs.org/faqs/jpeg-faq/part1/
void ImageImpl::FindJpegSize() {
  const StringPiece& buf = original_contents_;
  size_t pos = 2;  // Position of first data block after header.
  while (pos < buf.size()) {
    // Read block identifier
    int id = CharToInt(buf[pos++]);
    if (id == 0xff) {  // Padding byte
      continue;
    }
    // At this point pos points to first data byte in block.  In any block,
    // first two data bytes are size (including these 2 bytes).  But first,
    // make sure block wasn't truncated on download.
    if (pos + ImageHeaders::kJpegIntSize > buf.size()) {
      break;
    }
    int length = JpegIntAtPosition(buf, pos);
    // Now check for a SOFn header, which describes image dimensions.
    if (0xc0 <= id && id <= 0xcf &&  // SOFn header
        length >= 8 &&               // Valid SOFn block size
        pos + 1 + 3 * ImageHeaders::kJpegIntSize <= buf.size() &&
        // Above avoids case where dimension data was truncated
        id != 0xc4 && id != 0xc8 && id != 0xcc) {
      // 0xc4, 0xc8, 0xcc aren't actually valid SOFn headers.
      // NOTE: we don't care if we have the whole SOFn block,
      // just that we can fetch both dimensions without trouble.
      // Our image download could be truncated at this point for
      // all we care.
      // We're a bit sloppy about SOFn block size, as it's
      // actually 8 + 3 * buf[pos+2], but for our purposes this
      // will suffice as we don't parse subsequent metadata (which
      // describes the formatting of chunks of image data).
      dims_.set_height(
          JpegIntAtPosition(buf, pos + 1 + ImageHeaders::kJpegIntSize));
      dims_.set_width(
          JpegIntAtPosition(buf, pos + 1 + 2 * ImageHeaders::kJpegIntSize));
      break;
    }
    pos += length;
  }
  if (!ImageUrlEncoder::HasValidDimensions(dims_) ||
      (dims_.height() <= 0) || (dims_.width() <= 0)) {
    dims_.Clear();
    PS_LOG_INFO(handler_, "Couldn't find jpeg dimensions (data truncated?).");
  }
}

// Looks at first (IHDR) block of png stream to find image dimensions.
// See also: http://www.w3.org/TR/PNG/
void ImageImpl::FindPngSize() {
  const StringPiece& buf = original_contents_;
  // Here we make sure that buf contains at least enough data that we'll be able
  // to decipher the image dimensions first, before we actually check for the
  // headers and attempt to decode the dimensions (which are the first two ints
  // after the IHDR section label).
  if ((buf.size() >=  // Not truncated
       ImageHeaders::kIHDRDataStart + 2 * ImageHeaders::kPngIntSize) &&
      (StringPiece(buf.data() + ImageHeaders::kPngHeaderLength,
                   ImageHeaders::kPngSectionHeaderLength) ==
       StringPiece(ImageHeaders::kPngIHDR,
                   ImageHeaders::kPngSectionHeaderLength))) {
    dims_.set_width(PngIntAtPosition(buf, ImageHeaders::kIHDRDataStart));
    dims_.set_height(PngIntAtPosition(
        buf, ImageHeaders::kIHDRDataStart + ImageHeaders::kPngIntSize));
  } else {
    PS_LOG_INFO(handler_, "Couldn't find png dimensions "
                "(data truncated or IHDR missing).");
  }
}

// Looks at header of GIF file to extract image dimensions.
// See also: http://en.wikipedia.org/wiki/Graphics_Interchange_Format
void ImageImpl::FindGifSize() {
  const StringPiece& buf = original_contents_;
  // Make sure that buf contains enough data that we'll be able to
  // decipher the image dimensions before we attempt to do so.
  if (buf.size() >=
      ImageHeaders::kGifDimStart + 2 * ImageHeaders::kGifIntSize) {
    // Not truncated
    dims_.set_width(GifIntAtPosition(buf, ImageHeaders::kGifDimStart));
    dims_.set_height(GifIntAtPosition(
        buf, ImageHeaders::kGifDimStart + ImageHeaders::kGifIntSize));
  } else {
    PS_LOG_INFO(handler_, "Couldn't find gif dimensions (data truncated)");
  }
}

void ImageImpl::FindWebpSize() {
  const uint8* webp = reinterpret_cast<const uint8*>(original_contents_.data());
  const int webp_size = original_contents_.size();
  int width = 0, height = 0;
  if (WebPGetInfo(webp, webp_size, &width, &height) > 0) {
    dims_.set_width(width);
    dims_.set_height(height);
  } else {
    PS_LOG_INFO(handler_, "Couldn't find webp dimensions ");
  }
}

// Looks at image data in order to determine image type, and also fills in any
// dimension information it can (setting image_type_ and dims_).
void ImageImpl::ComputeImageType() {
  image_type_ =
      pagespeed::image_compression::ComputeImageType(original_contents_);

  switch (image_type_) {
    case IMAGE_JPEG:
      FindJpegSize();
      break;
    case IMAGE_PNG:
      FindPngSize();
      break;
    case IMAGE_GIF:
      FindGifSize();
      break;
    case IMAGE_WEBP:
    case IMAGE_WEBP_LOSSLESS_OR_ALPHA:
    case IMAGE_WEBP_ANIMATED:
      FindWebpSize();
      break;
    case IMAGE_UNKNOWN:
      break;
  }
}

const ContentType* Image::TypeToContentType(ImageType image_type) {
  const ContentType* res = NULL;
  switch (image_type) {
    case IMAGE_UNKNOWN:
      break;
    case IMAGE_JPEG:
      res = &kContentTypeJpeg;
      break;
    case IMAGE_PNG:
      res = &kContentTypePng;
      break;
    case IMAGE_GIF:
      res = &kContentTypeGif;
      break;
    case IMAGE_WEBP:
    case IMAGE_WEBP_LOSSLESS_OR_ALPHA:
    case IMAGE_WEBP_ANIMATED:
      res = &kContentTypeWebp;
      break;
  }
  return res;
}

// Compute whether a PNG can have transparent / semi-transparent pixels
// by walking the image data in accordance with the spec:
//   http://www.w3.org/TR/PNG/
// If the colour type (UK spelling from spec) includes an alpha channel, or
// there is a tRNS section with at least one entry before IDAT, then we assume
// the image contains non-opaque pixels and return true.
bool ImageImpl::ComputePngTransparency(const StringPiece& buf) {
  // We assume the image has transparency until we prove otherwise.
  // This allows us to deal conservatively with truncation etc.
  bool has_transparency = true;
  if (buf.size() > ImageHeaders::kPngColourTypeOffset &&
      ((buf[ImageHeaders::kPngColourTypeOffset] &
        ImageHeaders::kPngAlphaChannel) == 0)) {
    // The colour type indicates that there is no dedicated alpha channel.  Now
    // we must look for a tRNS section indicating the existence of transparent
    // colors or palette entries.
    size_t section_start = ImageHeaders::kPngHeaderLength;
    while (section_start + ImageHeaders::kPngSectionHeaderLength < buf.size()) {
      size_t section_size = PngIntAtPosition(buf, section_start);
      if (PngSectionIdIs(ImageHeaders::kPngIDAT, buf, section_start)) {
        // tRNS section must occur before first IDAT.  This image doesn't have a
        // tRNS section, and thus doesn't have transparency.
        has_transparency = false;
        break;
      } else if (PngSectionIdIs(ImageHeaders::kPngtRNS, buf, section_start) &&
                 section_size > 0) {
        // Found a nonempty tRNS section.  This image has_transparency.
        break;
      } else {
        // Move on to next section.
        section_start += section_size + ImageHeaders::kPngSectionMinSize;
      }
    }
  }
  return has_transparency;
}

bool ImageImpl::EnsureLoaded(bool output_useful) {
  return true;
}

// Determine the quality level for compressing the resized image.
// If a JPEG image needs resizing, we decompress it first, then resize it,
// and finally compress it into a new JPEG image. To compress the output image,
// We would like to use the quality level that was used in the input image,
// if such information can be calculated from the input image; otherwise, we
// will use the quality level set in the configuration; otherwise, we will use
// a predefined default quality.
int ImageImpl::EstimateQualityForResizedJpeg() {
  int input_quality = GetJpegQualityFromImage(original_contents_);
  int output_quality = std::min(ImageHeaders::kMaxJpegQuality,
                                options_->jpeg_quality);
  if (input_quality > 0 && output_quality > 0) {
    return std::min(input_quality, output_quality);
  } else if (input_quality > 0) {
    return input_quality;
  } else if (output_quality > 0) {
    return output_quality;
  } else {
    return ImageHeaders::kQualityForJpegWithUnkownQuality;
  }
}

void ImageImpl::Dimensions(ImageDim* natural_dim) {
  if (!ImageUrlEncoder::HasValidDimensions(dims_)) {
    ComputeImageType();
  }
  *natural_dim = dims_;
}

bool ImageImpl::ResizeTo(const ImageDim& new_dim) {
  CHECK(ImageUrlEncoder::HasValidDimensions(new_dim));
  if ((new_dim.width() <= 0) || (new_dim.height() <= 0)) {
    return false;
  }

  if (changed_) {
    // If we already resized, drop data and work with original image.
    UndoChange();
  }

  // TODO(huibao): Enable resizing for WebP and images with alpha channel.
  // We have the tools ready but no tests.
  const ImageFormat original_format = ImageTypeToImageFormat(image_type());
  if (original_format == pagespeed::image_compression::IMAGE_WEBP) {
    return false;
  }

  scoped_ptr<ScanlineReaderInterface> image_reader(
      CreateScanlineReader(original_format,
                           original_contents_.data(),
                           original_contents_.length(),
                           handler_.get()));
  if (image_reader == NULL) {
    resize_debug_message_ =
        StringPrintf("Cannot resize: Cannot open the image%s to resize",
                     debug_message_url_.c_str());
    PS_LOG_INFO(handler_, "Cannot open the image to resize.");
    return false;
  }

  ScanlineResizer resizer(handler_.get());
  if (!resizer.Initialize(image_reader.get(), new_dim.width(),
                          new_dim.height())) {
    resize_debug_message_ =
        StringPrintf("Cannot resize%s: Unable to initialize resizer",
                     debug_message_url_.c_str());
    return false;
  }

  // Create a writer.
  scoped_ptr<ScanlineWriterInterface> writer;
  const ImageFormat resized_format = GetOutputImageFormat(original_format);
  switch (resized_format) {
    case pagespeed::image_compression::IMAGE_JPEG:
      {
        JpegCompressionOptions jpeg_config;
        jpeg_config.lossy = true;
        jpeg_config.lossy_options.quality = EstimateQualityForResizedJpeg();
        writer.reset(CreateScanlineWriter(resized_format,
                                          resizer.GetPixelFormat(),
                                          resizer.GetImageWidth(),
                                          resizer.GetImageHeight(),
                                          &jpeg_config,
                                          &resized_image_,
                                          handler_.get()));
      }
      break;

    case pagespeed::image_compression::IMAGE_PNG:
      {
        PngCompressParams png_config(PNG_FILTER_NONE, Z_DEFAULT_STRATEGY,
                                     false);
        writer.reset(CreateScanlineWriter(resized_format,
                                          resizer.GetPixelFormat(),
                                          resizer.GetImageWidth(),
                                          resizer.GetImageHeight(),
                                          &png_config,
                                          &resized_image_,
                                          handler_.get()));
      }
      break;

    default:
      resize_debug_message_ =
          StringPrintf("Cannot resize%s: Unsupported image format",
                       debug_message_url_.c_str());
      PS_LOG_DFATAL(handler_, "Unsupported image format");
  }

  if (writer == NULL) {
    return false;
  }

  // Resize the image and save the results in 'resized_image_'.
  void* scanline = NULL;
  while (resizer.HasMoreScanLines()) {
    if (!resizer.ReadNextScanline(&scanline)) {
      resize_debug_message_ =
          StringPrintf("Cannot resize%s: Reading image failed",
                       debug_message_url_.c_str());
      return false;
    }
    if (!writer->WriteNextScanline(scanline)) {
      resize_debug_message_ =
          StringPrintf("Cannot resize%s: Writing image failed",
                       debug_message_url_.c_str());
      return false;
    }
  }
  if (!writer->FinalizeWrite()) {
    resize_debug_message_ =
        StringPrintf("Cannot resize%s: Finalizing writing image failed",
                     debug_message_url_.c_str());
    return false;
  }

  changed_ = true;
  output_valid_ = false;
  rewrite_attempted_ = false;
  output_contents_.clear();
  resized_dimensions_ = new_dim;
  resize_debug_message_ = StringPrintf(
      "Resized image%s from %dx%d to %dx%d",
      debug_message_url_.c_str(),
      dims_.width(), dims_.height(),
      resized_dimensions_.width(), resized_dimensions_.height());
  return true;
}

void ImageImpl::UndoChange() {
  if (changed_) {
    output_valid_ = false;
    rewrite_attempted_ = false;
    output_contents_.clear();
    resized_image_.clear();
    image_type_ = IMAGE_UNKNOWN;
    changed_ = false;
  }
}

// TODO(huibao): Refactor image rewriting. We may have a centralized
// controller and a set of naive image writers. The controller looks at
// the input image type and the filter settings, and decides which output
// format(s) to try and the configuration for each output format. The writers
// simply write the output based on the specified configurations and should not
// be aware of the input type nor the filters.
//
// Here are some thoughts for the new design.
// 1. Create a scanline reader based on the type of input image.
// 2. If the image is going to be resized, wrap the reader into a resizer, which
//    is also a scanline reader.
// 3. Create a scanline writer or mutliple writers based the filter settings.
//    The parameters for the writer will also be determined by the filters.
//
// Transfer all of the scanlines from the reader to the writer and the image is
// rewritten (and resized)!

// Performs image optimization and output
bool ImageImpl::ComputeOutputContents() {
  if (rewrite_attempted_) {
    return output_valid_;
  }
  rewrite_attempted_ = true;
  if (!output_valid_) {
    StringPiece contents;
    bool resized;

    // Choose appropriate source for image contents.
    // Favor original contents if image unchanged.
    resized = !resized_image_.empty();
    if (resized) {
      contents = resized_image_;
    } else {
      contents = original_contents_;
    }

    // Take image contents and re-compress them.
    // The basic logic is this:
    // * low_quality_enabled_ acts as though convert_gif_to_png and
    //   convert_png_to_webp were both set for this image.
    // * We compute the intended final end state of all the
    //   convert_X_to_Y options, and try to convert to the final
    //   option in one shot. If that fails, we back off by each of the stages.
    // * We return as soon as any applicable conversion succeeds. We
    //   do not compare the sizes of alternative conversions.
    // If we can't optimize the image, we'll fail.
    bool ok = false;
    // We copy the data to a string eagerly as we're very likely to need it
    // (only unrecognized formats don't require it, in which case we probably
    // don't get this far in the first place).
    // TODO(jmarantz): The PageSpeed library should, ideally, take StringPiece
    // args rather than const string&.  We would save lots of string-copying
    // if we made that change.
    GoogleString string_for_image(contents.data(), contents.size());
    scoped_ptr<PngReaderInterface> png_reader;
    switch (image_type()) {
      case IMAGE_UNKNOWN:
        break;
      case IMAGE_WEBP:
      case IMAGE_WEBP_LOSSLESS_OR_ALPHA:
        if (resized || options_->recompress_webp) {
          ok = MayConvert() &&
              ReduceWebpImageQuality(string_for_image,
                                     options_->webp_quality,
                                     &output_contents_);
        }
        // TODO(pulkitg): Convert a webp image to jpeg image if
        // web_preferred_ is false.
        break;
      case IMAGE_WEBP_ANIMATED:
        // TODO(huibao): Recompress animated WebP.
        ok = false;
        break;
      case IMAGE_JPEG:
        if (MayConvert() &&
            options_->convert_jpeg_to_webp &&
            (options_->preferred_webp != WEBP_NONE)) {
          ok = ConvertJpegToWebp(string_for_image, options_->webp_quality,
                                 &output_contents_);
          VLOG(1) << "Image conversion: " << ok << " jpeg->webp for " << url_;
          if (!ok) {
            // Image is not going to be webp-converted!
            PS_LOG_INFO(handler_, "Failed to create webp!");
          }
        }
        if (ok) {
          image_type_ = IMAGE_WEBP;
        } else if (MayConvert() &&
                   (resized || options_->recompress_jpeg)) {
          JpegCompressionOptions jpeg_options;
          ConvertToJpegOptions(*options_.get(), &jpeg_options);
          ok = OptimizeJpegWithOptions(string_for_image, &output_contents_,
                                       jpeg_options, handler_.get());
          VLOG(1) << "Image conversion: " << ok << " jpeg->jpeg for " << url_;
        }
        break;
      case IMAGE_PNG:
        png_reader.reset(new PngReader(handler_.get()));
        ok = ComputeOutputContentsFromGifOrPng(
            string_for_image,
            png_reader.get(),
            (resized || options_->recompress_png) /* fall_back_to_png */,
            kPngString,
            IMAGE_PNG,
            Image::ConversionVariables::FROM_PNG);
        break;
      case IMAGE_GIF:
        ImageType current_image_type = IMAGE_GIF;
        if (resized) {
          // If the GIF image has been resized, it has already been
          // converted to a PNG image.
          png_reader.reset(new PngReader(handler_.get()));
          current_image_type = IMAGE_PNG;
        } else if (options_->convert_gif_to_png || low_quality_enabled_ ||
                   options_->allow_webp_animated) {
          png_reader.reset(new GifReader(handler_.get()));
        } else {
          break;
        }
        ok = ComputeOutputContentsFromGifOrPng(
            string_for_image,
            png_reader.get(),
            options_->convert_gif_to_png /* fall_back_to_png */,
            kGifString,
            current_image_type,
            Image::ConversionVariables::FROM_GIF);
        break;
    }
    output_valid_ = ok;
  }
  return output_valid_;
}

inline bool ImageImpl::ConvertJpegToWebp(
    const GoogleString& original_jpeg, int configured_quality,
    GoogleString* compressed_webp) {
  ConversionTimeoutHandler timeout_handler(options_->webp_conversion_timeout_ms,
                                           timer_, handler_.get());
  timeout_handler.Start(compressed_webp);
  bool ok = OptimizeWebp(original_jpeg, configured_quality,
                         ConversionTimeoutHandler::Continue, &timeout_handler,
                         compressed_webp, handler_.get());
  timeout_handler.Stop();

  bool was_timed_out = timeout_handler.was_timed_out();
  int64 time_elapsed_ms = timeout_handler.time_elapsed_ms();

  UpdateWebpStats(ok, was_timed_out, time_elapsed_ms,
                 Image::ConversionVariables::FROM_JPEG,
                 options_->webp_conversion_variables);

  UpdateWebpStats(ok, was_timed_out, time_elapsed_ms,
                 Image::ConversionVariables::OPAQUE,
                 options_->webp_conversion_variables);
  return ok;
}

bool ImageImpl::ConvertAnimatedGifToWebp(bool has_transparency) {
  ConversionTimeoutHandler timeout_handler(
      options_->webp_conversion_timeout_ms, timer_, handler_.get());
  timeout_handler.Start(&output_contents_);

  // Parameters controlling WebP compression.
  WebpConfiguration webp_config;
  webp_config.quality = options_->webp_animated_quality;
  webp_config.progress_hook = ConversionTimeoutHandler::Continue;
  webp_config.user_data = &timeout_handler;
  // TODO(huibao): Evaluate the following parameters.
  webp_config.method = 3;
  webp_config.kmin = 3;
  webp_config.kmax = 5;
  webp_config.lossless = false;
  webp_config.alpha_quality = 100;
  webp_config.alpha_compression = 1;  // alpha plane compressed losslessly

  pagespeed::image_compression::ScanlineStatus status;
  scoped_ptr<pagespeed::image_compression::MultipleFrameReader> reader(
      CreateImageFrameReader(
          pagespeed::image_compression::IMAGE_GIF,
          original_contents_.data(),
          original_contents_.length(),
          handler_.get(), &status));
  if (!status.Success()) {
    PS_LOG_ERROR(handler_, "Cannot read the animated GIF image.");
    return false;
  }

  scoped_ptr<pagespeed::image_compression::MultipleFrameWriter> writer(
      CreateImageFrameWriter(
          pagespeed::image_compression::IMAGE_WEBP,
          &webp_config,
          &output_contents_,
          handler_.get(), &status));
  if (!status.Success()) {
    PS_LOG_ERROR(handler_, "Cannot create an animated WebP image for output.");
    return false;
  }

  // Copy all pixels in all frames from the reader to the writer. This will do
  // format conversion and compression.
  pagespeed::image_compression::ImageSpec image_spec;
  pagespeed::image_compression::FrameSpec frame_spec;
  const void* scan_row = NULL;
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

  timeout_handler.Stop();
  bool was_timed_out = timeout_handler.was_timed_out();
  int64 time_elapsed_ms = timeout_handler.time_elapsed_ms();
  bool ok = status.Success();

  UpdateWebpStats(ok, was_timed_out, time_elapsed_ms,
                  Image::ConversionVariables::FROM_GIF_ANIMATED,
                  options_->webp_conversion_variables);

  UpdateWebpStats(ok, was_timed_out, time_elapsed_ms,
                  (has_transparency ?
                   Image::ConversionVariables::NONOPAQUE :
                   Image::ConversionVariables::OPAQUE),
                  options_->webp_conversion_variables);

  return ok;
}

inline bool ImageImpl::ComputeOutputContentsFromGifOrPng(
    const GoogleString& string_for_image,
    const PngReaderInterface* png_reader,
    bool fall_back_to_png,
    const char* dbg_input_format,
    ImageType input_type,
    ConversionVariables::VariableType var_type) {
  // Don't try to optimize empty images, it just messes things up.
  if (dims_.width() <= 0 || dims_.height() <= 0) {
    return false;
  }

  bool ok = false;
  bool is_animated = false;
  bool has_transparency = false;
  bool is_photo = false;
  bool compress_color_losslessly = false;
  ImageType output_type = IMAGE_UNKNOWN;

  AnalyzeImage(ImageTypeToImageFormat(input_type),
               string_for_image.data(), string_for_image.length(),
               NULL /* width */, NULL /* height */, NULL /* is_progressive */,
               &is_animated, &has_transparency, &is_photo,
               NULL /* quality */, NULL /* reader */, handler_.get());

  debug_message_ = StringPrintf("Image%s has%s transparent pixels,"
                                " is%s sensitive to compression noise, and"
                                " has%s animation.",
                                debug_message_url_.c_str(),
                                (has_transparency ? "" : " no"),
                                (is_photo ? " not" : ""),
                                (is_animated ? "" : " no"));

  // By default, a lossless image conversion is eligible for lossless webp
  // conversion.
  if (is_animated) {
    if (options_->preferred_webp == WEBP_ANIMATED &&
        options_->webp_animated_quality > 0) {
      output_type = IMAGE_WEBP_ANIMATED;
    }
    // else we can't recompress this image
  } else if (is_photo && options_->convert_png_to_jpeg &&
             (input_type == IMAGE_PNG ||
              (input_type == IMAGE_GIF && options_->convert_gif_to_png))) {
    // Can be converted to lossy format.
    if (!has_transparency) {
      // No alpha; can be converted to WebP lossy or JPEG.
      if (options_->preferred_webp != WEBP_NONE &&
          options_->convert_jpeg_to_webp &&
          options_->webp_quality > 0) {
        compress_color_losslessly = false;
        output_type = IMAGE_WEBP;
      } else if (options_->jpeg_quality > 0) {
        output_type = IMAGE_JPEG;
      }
    } else {
      if (options_->allow_webp_alpha &&
          options_->convert_jpeg_to_webp &&
          options_->webp_quality > 0) {
        compress_color_losslessly = false;
        output_type = IMAGE_WEBP_LOSSLESS_OR_ALPHA;
      }
    }
  } else {
    // Must be converted to lossless format.
    if (options_->preferred_webp == WEBP_ANIMATED ||
        options_->preferred_webp == WEBP_LOSSLESS) {
      compress_color_losslessly = true;
      output_type = IMAGE_WEBP_LOSSLESS_OR_ALPHA;
    }
  }

  if (output_type == IMAGE_WEBP_ANIMATED) {
    ok = ConvertAnimatedGifToWebp(has_transparency);
  } else {
    if (output_type == IMAGE_WEBP ||
        output_type == IMAGE_WEBP_LOSSLESS_OR_ALPHA) {
      ok = MayConvert() &&
          ConvertPngToWebp(*png_reader, string_for_image,
                           compress_color_losslessly, has_transparency,
                           var_type);
      // TODO(huibao): Re-evaluate why we need to try a different format, if the
      // conversion to WebP failed.
      if (!ok) {
        // If the conversion to WebP failed, we will try converting the image to
        // jpeg or png.
        if (output_type == IMAGE_WEBP) {
          output_type = IMAGE_JPEG;
        } else {
          fall_back_to_png = true;
        }
      }
    }

    if (output_type == IMAGE_JPEG) {
      JpegCompressionOptions jpeg_options;
      ConvertToJpegOptions(*options_.get(), &jpeg_options);
      ok = MayConvert() &&
          ImageConverter::ConvertPngToJpeg(*png_reader, string_for_image,
                                           jpeg_options, &output_contents_,
                                           handler_.get());
    }

    if (!ok && fall_back_to_png) {
      ok = MayConvert() &&
          PngOptimizer::OptimizePngBestCompression(*png_reader,
                                                   string_for_image,
                                                   &output_contents_,
                                                   handler_.get());
      output_type = IMAGE_PNG;
    }
  }

  if (ok) {
    image_type_ = output_type;
  } else {
    image_type_ = input_type;
  }

  VLOG(1) << "Image conversion: " << ok << " " << dbg_input_format << "->"
          << ImageFormatToString(ImageTypeToImageFormat(image_type_)) << " for "
          << url_;

  return ok;
}

bool ImageImpl::ConvertPngToWebp(
      const PngReaderInterface& png_reader,
      const GoogleString& input_image,
      bool compress_color_losslessly,
      bool has_transparency,
      ConversionVariables::VariableType var_type) {
  ConversionTimeoutHandler timeout_handler(options_->webp_conversion_timeout_ms,
                                           timer_, handler_.get());
  WebpConfiguration webp_config;

  // Quality/speed trade-off (0=fast, 6=slower-better).
  // This is the default value in libpagespeed. We should evaluate
  // whether this is the optimal value, and consider making it
  // tunable.
  webp_config.method = 3;
  webp_config.quality = options_->webp_quality;
  webp_config.progress_hook = ConversionTimeoutHandler::Continue;
  webp_config.user_data = &timeout_handler;

  ImageType target_image_type = IMAGE_WEBP_LOSSLESS_OR_ALPHA;
  if (compress_color_losslessly) {
    // Note that webp_config.alpha_quality and
    // webp_config.alpha_compression are only meaningful in the
    // lossy compression case.
    webp_config.lossless = true;
  } else {
    webp_config.lossless = false;
    if (has_transparency) {
      webp_config.alpha_quality = 100;
      webp_config.alpha_compression = 1;
    } else {
      webp_config.alpha_quality = 0;
      webp_config.alpha_compression = 0;
      image_type_ = IMAGE_WEBP;
    }
  }

  // TODO(huibao): Remove "is_opaque" from the returned arguments in
  // ConvertPngToWebp() and PngScanlineReader::InitializeRead().
  // The technique they use can only detect some of the opaque images.
  // PixelFormatOptimizer has a more expensive, but comprehensive solution.
  bool not_used;
  timeout_handler.Start(&output_contents_);
  bool ok = ImageConverter::ConvertPngToWebp(
      png_reader, input_image, webp_config,
      &output_contents_, &not_used, handler_.get());

  if (ok) {
    image_type_ = target_image_type;
  }
  timeout_handler.Stop();

  bool was_timed_out = timeout_handler.was_timed_out();
  int64 time_elapsed_ms = timeout_handler.time_elapsed_ms();

  UpdateWebpStats(ok, was_timed_out, time_elapsed_ms, var_type,
                  options_->webp_conversion_variables);

  UpdateWebpStats(ok, was_timed_out, time_elapsed_ms,
                  (has_transparency ?
                   Image::ConversionVariables::NONOPAQUE :
                   Image::ConversionVariables::OPAQUE),
                  options_->webp_conversion_variables);

  return ok;
}

bool ImageImpl::OptimizePng(
    const PngReaderInterface& png_reader,
    const GoogleString& image_data) {
  bool ok = MayConvert() &&
      PngOptimizer::OptimizePngBestCompression(png_reader,
                                               image_data,
                                               &output_contents_,
                                               handler_.get());
  if (ok) {
    image_type_ = IMAGE_PNG;
  }
  return ok;
}

bool ImageImpl::OptimizePngOrConvertToJpeg(
    const PngReaderInterface& png_reader,
    const GoogleString& image_data) {
  bool is_png;
  JpegCompressionOptions jpeg_options;
  ConvertToJpegOptions(*options_.get(), &jpeg_options);
  bool ok = MayConvert() &&
      ImageConverter::OptimizePngOrConvertToJpeg(
          png_reader, image_data, jpeg_options,
          &output_contents_, &is_png, handler_.get());
  if (ok) {
    if (is_png) {
      image_type_ = IMAGE_PNG;
    } else {
      image_type_ = IMAGE_JPEG;
    }
  }
  return ok;
}

void ImageImpl::ConvertToJpegOptions(const Image::CompressionOptions& options,
                                     JpegCompressionOptions* jpeg_options) {
  int input_quality = GetJpegQualityFromImage(original_contents_);
  jpeg_options->retain_color_profile = options.retain_color_profile;
  jpeg_options->retain_exif_data = options.retain_exif_data;
  int output_quality = EstimateQualityForResizedJpeg();

  if (options.jpeg_quality > 0) {
    // If the source image is JPEG we want to fallback to lossless if the input
    // quality is less than the quality we want to set for final compression and
    // num progressive scans is not set. Incase we are not able to decode the
    // input image quality, then we use lossless path.
    if (image_type() != IMAGE_JPEG ||
        options.jpeg_num_progressive_scans > 0 ||
        input_quality > output_quality) {
      jpeg_options->lossy = true;
      jpeg_options->lossy_options.quality = output_quality;
      if (options.progressive_jpeg) {
        jpeg_options->lossy_options.num_scans =
            options.jpeg_num_progressive_scans;
      }

      if (options.retain_color_sampling) {
        jpeg_options->lossy_options.color_sampling = RETAIN;
      }
    }
  }

  jpeg_options->progressive = options.progressive_jpeg &&
      ShouldConvertToProgressive(output_quality);
}

bool ImageImpl::ShouldConvertToProgressive(int64 quality) const {
  bool progressive = false;
  const ImageDim* expected_dimensions = &dims_;
  if (ImageUrlEncoder::HasValidDimensions(resized_dimensions_)) {
    expected_dimensions = &resized_dimensions_;
  }
  if (ImageUrlEncoder::HasValidDimensions(*expected_dimensions)) {
    progressive = pagespeed::image_compression::ShouldConvertToProgressive(
        quality, options_->progressive_jpeg_min_bytes,
        original_contents_.size(), expected_dimensions->width(),
        expected_dimensions->height());
  } else {
    progressive = (static_cast<int64>(original_contents_.size()) >=
                   options_->progressive_jpeg_min_bytes);
  }
  return progressive;
}

StringPiece Image::Contents() {
  StringPiece contents;
  if (this->image_type() != IMAGE_UNKNOWN) {
    contents = original_contents_;
    if (output_valid_ || ComputeOutputContents()) {
      contents = output_contents_;
    }
  }
  return contents;
}

bool ImageImpl::DrawImage(Image* image, int x, int y) {
  // Create a reader for reading the original canvas image.
  scoped_ptr<ScanlineReaderInterface> canvas_reader(CreateScanlineReader(
      pagespeed::image_compression::IMAGE_PNG,
      output_contents_.data(),
      output_contents_.length(),
      handler_.get()));
  if (canvas_reader == NULL) {
    PS_LOG_ERROR(handler_, "Cannot open canvas image.");
    return false;
  }

  // Get the size and pixel format of the original canvas image.
  const size_t canvas_width = canvas_reader->GetImageWidth();
  const size_t canvas_height = canvas_reader->GetImageHeight();
  const PixelFormat canvas_pixel_format = canvas_reader->GetPixelFormat();

  // Initialize a reader for reading the image which will be sprited.
  ImageImpl* impl = static_cast<ImageImpl*>(image);
  scoped_ptr<ScanlineReaderInterface> image_reader(CreateScanlineReader(
      ImageTypeToImageFormat(impl->image_type()),
                             impl->original_contents().data(),
                             impl->original_contents().length(),
                             handler_.get()));
  if (image_reader == NULL) {
    PS_LOG_INFO(handler_, "Cannot open the image which will be sprited.");
    return false;
  }

  // Get the size of the image which will be sprited.
  const size_t image_width = image_reader->GetImageWidth();
  const size_t image_height = image_reader->GetImageHeight();
  const PixelFormat image_pixel_format = image_reader->GetPixelFormat();

  if (x + image_width > canvas_width || y + image_height > canvas_height) {
    PS_LOG_INFO(handler_, "The new image cannot fit into the canvas.");
    return false;
  }

  bool has_transparency = false;
  PixelFormat output_pixel_format = RGB_888;
  if (image_pixel_format == RGBA_8888 ||
      canvas_pixel_format == RGBA_8888) {
    has_transparency = true;
    output_pixel_format = RGBA_8888;
  }

  const size_t bytes_per_pixel =
      GetNumChannelsFromPixelFormat(output_pixel_format, handler_.get());
  const size_t bytes_per_scanline = canvas_width * bytes_per_pixel;
  scoped_array<uint8> scanline(new uint8[bytes_per_scanline]);

  // Create a writer for writing the new canvas image.
  GoogleString canvas_image;
  scoped_ptr<ScanlineWriterInterface> canvas_writer(
      CreateUncompressedPngWriter(canvas_width, canvas_height,
                                  &canvas_image, handler_.get(),
                                  has_transparency));
  if (canvas_writer == NULL) {
    PS_LOG_ERROR(handler_, "Failed to create canvas writer.");
    return false;
  }

  // Overlay the new image onto the canvas image.
  for (int row = 0; row < static_cast<int>(canvas_height); ++row) {
    uint8* canvas_line = NULL;
    if (!canvas_reader->ReadNextScanline(
        reinterpret_cast<void**>(&canvas_line))) {
      PS_LOG_ERROR(handler_, "Failed to read canvas image.");
      return false;
    }

    if (row >= y && row < y + static_cast<int>(image_height)) {
      uint8* image_line = NULL;
      if (!image_reader->ReadNextScanline(
          reinterpret_cast<void**>(&image_line))) {
        PS_LOG_INFO(handler_,
                     "Failed to read the image which will be sprited.");
        return false;
      }

      // Set the entire scanline to white. This operation has no effect
      // on the webpage; it just gives a clean background to the
      // sprite image.
      memset(scanline.get(), kAlphaOpaque, x * bytes_per_pixel);
      memset(scanline.get() + (x + image_width) * bytes_per_pixel,
             kAlphaOpaque,
             (canvas_width - image_width - x) * bytes_per_pixel);

      ExpandPixelFormat(image_width, image_pixel_format, 0, image_line,
                        output_pixel_format, x, scanline.get(), handler_.get());
    } else {
      ExpandPixelFormat(canvas_width, canvas_pixel_format, 0, canvas_line,
                        output_pixel_format, 0, scanline.get(), handler_.get());
    }

    if (!canvas_writer->WriteNextScanline(
        reinterpret_cast<void*>(scanline.get()))) {
      PS_LOG_ERROR(handler_, "Failed to write canvas image.");
      return false;
    }
  }

  if (!canvas_writer->FinalizeWrite()) {
    PS_LOG_ERROR(handler_, "Failed to close canvas file.");
    return false;
  }

  output_contents_ = canvas_image;
  output_valid_ = true;
  return true;
}

}  // namespace net_instaweb
