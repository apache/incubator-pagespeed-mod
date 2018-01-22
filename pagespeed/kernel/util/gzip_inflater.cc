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


#include "pagespeed/kernel/util/gzip_inflater.h"

#include <cstddef>
#include <cstdlib>
#include "base/logging.h"
#ifdef USE_SYSTEM_ZLIB
#include "zlib.h"  // NOLINT
#include "zconf.h"  // NOLINT
#else
#include "third_party/zlib/src/zlib.h"
#include "third_party/zlib/src/zconf.h"
#endif
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/writer.h"

namespace {

// Helper that snapshots the state of a z_stream structure.
struct ZlibSnapshot {
 public:
  explicit ZlibSnapshot(z_stream* zlib)
      : total_out(zlib->total_out),
        total_in(zlib->total_in),
        avail_in(zlib->avail_in),
        next_in(zlib->next_in) {
  }

  const uLong total_out;
  const uLong total_in;
  const uInt avail_in;
  Bytef* const next_in;
};

bool IsValidZlibStreamHeaderByte(uint8 first_byte) {
  // The first byte of a zlib stream contains the compression method
  // and the compression info. See http://www.ietf.org/rfc/rfc1950.txt
  // for more details.
  const uint8 compression_method = first_byte & 0xf;
  const uint8 compression_info = first_byte >> 4;
  // Zlib RFC states that compression method must be 8, and that
  // compression info must be 7 or less. If either of these does not
  // hold, we do not have a valid zlib stream.
  return (compression_method == 8 && compression_info <= 7);
}

}  // namespace

namespace net_instaweb {

GzipInflater::GzipInflater(InflateType type)
    : zlib_(NULL),
      format_(type == kGzip ? FORMAT_GZIP : FORMAT_ZLIB_STREAM),
      finished_(false),
      error_(false) {
  if (type != kGzip && type != kDeflate) {
    LOG(INFO) << "Received unexpected inflate type: " << type;
    error_ = true;
  }
}

GzipInflater::~GzipInflater() {
  Free();
}

void GzipInflater::Free() {
  if (zlib_ == NULL) {
    // Already freed.
    return;
  }

  int err = inflateEnd(zlib_);
  if (err != Z_OK) {
    error_ = true;
  }

  free(zlib_);

  zlib_ = NULL;
}

/* static */
bool GzipInflater::GetWindowBitsForFormat(
    StreamFormat format, int* out_window_bits) {
  // From zlib.h:
  //  [For zlib stream format] the windowBits parameter is the base
  //  two logarithm of the window size... windowBits can also be
  //  -8..-15 for raw inflate... or add 16 to decode only the gzip
  //  format.
  switch (format) {
    case FORMAT_GZIP:
      *out_window_bits = 31;
      return true;
    case FORMAT_ZLIB_STREAM:
      *out_window_bits = 15;
      return true;
    case FORMAT_RAW_INFLATE:
      *out_window_bits = -15;
      return true;
  }
  LOG(INFO) << "Unknown StreamFormat: " << format;
  return false;
}

bool GzipInflater::Init() {
  if (zlib_ != NULL) {
    return false;
  }

  zlib_ = static_cast<z_stream *>(malloc(sizeof(z_stream)));
  if (zlib_ == NULL) {
    return false;
  }
  memset(zlib_, 0, sizeof(z_stream));

  int window_bits = 0;
  if (!GetWindowBitsForFormat(format_, &window_bits)) {
    error_ = true;
    return false;
  }
  int err = inflateInit2(zlib_, window_bits);

  if (err != Z_OK) {
    Free();
    error_ = true;
    return false;
  }

  return true;
}

bool GzipInflater::HasUnconsumedInput() const {
  if (zlib_ == NULL) {
    return false;
  }

  if (finished_ || error_) {
    return false;
  }

  return zlib_->avail_in > 0;
}

bool GzipInflater::SetInput(const void *in, size_t in_size) {
  if (zlib_ == NULL) {
    return false;
  }

  if (HasUnconsumedInput()) {
    return false;
  }

  if (finished_) {
    return false;
  }

  if (error_) {
    return false;
  }

  if (in == NULL || in_size == 0) {
    return false;
  }

  if (format_ == FORMAT_ZLIB_STREAM &&
      zlib_->total_in == 0 &&
      !IsValidZlibStreamHeaderByte(static_cast<const uint8*>(in)[0])) {
    // Special case: Content-Encoding: deflate can sometimes be zlib
    // stream and sometimes be raw deflate. The header byte is not a
    // valid zlib stream header byte, so try to decode as raw deflate
    // format. See comments in SwitchToRawDeflateFormat for more
    // information.
    LOG(INFO) << "Detected invalid zlib stream header byte. "
              << "Trying raw deflate format.";
    SwitchToRawDeflateFormat();
  }

  SetInputInternal(in, in_size);
  return true;
}

void GzipInflater::SetInputInternal(const void *in, size_t in_size) {
  // The zlib library won't modify the buffer, but it does not use
  // const here, so we must const cast.
  zlib_->next_in  = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(in));
  zlib_->avail_in = static_cast<uInt>(in_size);
}

void GzipInflater::SwitchToRawDeflateFormat() {
  // The HTTP RFC indicates that the "deflate" Content-Encoding is
  // actually the zlib stream format: "The "zlib" format defined in
  // RFC 1950 [31] in combination with the "deflate" compression
  // mechanism described in RFC 1951 [29]."
  //
  // There is some confusion about this and some HTTP servers will
  // serve "raw deflate" whereas others will serve the correct zlib
  // stream format. From http://www.zlib.net/zlib_faq.html#faq39:
  //  "gzip" is the gzip format, and "deflate" is the zlib
  //  format. They should probably have called the second one "zlib"
  //  instead to avoid confusion with the raw deflate compressed data
  //  format. While the HTTP 1.1 RFC 2616 correctly points to the zlib
  //  specification in RFC for the "deflate" transfer encoding, there
  //  have been reports of servers and browsers that incorrectly
  //  produce or expect raw deflate data per the deflate specficiation
  //  in RFC 1951, most notably Microsoft. So even though the
  //  "deflate" transfer encoding using the zlib format would be the
  //  more efficient approach (and in fact exactly what the zlib
  //  format was designed for), using the "gzip" transfer encoding is
  //  probably more reliable due to an unfortunate choice of name on
  //  the part of the HTTP 1.1 authors."
  Free();
  format_ = FORMAT_RAW_INFLATE;
  Init();
}

int GzipInflater::InflateBytes(char *buf, size_t buf_size) {
  if (zlib_ == NULL) {
    return -1;
  }

  if (!HasUnconsumedInput()) {
    return -1;
  }

  if (finished_) {
    return -1;
  }

  if (error_) {
    return -1;
  }

  if (buf == NULL || buf_size == 0) {
    return -1;
  }

  zlib_->next_out = reinterpret_cast<Bytef *>(buf);
  zlib_->avail_out = static_cast<uInt>(buf_size);

  // Take a snapshot of the zlib state before we attempt to inflate,
  // as we may need to recall the previous state if the inflate fails.
  const ZlibSnapshot zlib_snapshot(zlib_);
  int err = inflate(zlib_, Z_SYNC_FLUSH);

  if (format_ == FORMAT_ZLIB_STREAM &&
      zlib_snapshot.total_in == 0 &&
      err == Z_DATA_ERROR) {
    // Special case: Content-Encoding: deflate can sometimes be zlib
    // stream and sometimes be raw deflate. We failed to decode the
    // response as zlib stream so we'll try raw deflate
    // format. Ideally we would auto-detect which of zlib stream and
    // raw deflate was being used, but the set of legal headers for
    // each stream overlaps, so the only sure way to detect is to try
    // one format, then switch the other if the first one fails.  See
    // comments in SwitchToRawDeflateFormat for more information.
    LOG(INFO) << "Failed to decode as zlib stream. Trying raw deflate.";
    SwitchToRawDeflateFormat();
    zlib_->next_in  = zlib_snapshot.next_in;
    zlib_->avail_in = zlib_snapshot.avail_in;
    zlib_->next_out = reinterpret_cast<Bytef *>(buf);
    zlib_->avail_out = static_cast<uInt>(buf_size);
    err = inflate(zlib_, Z_SYNC_FLUSH);
  }

  const size_t inflated_bytes = zlib_->total_out - zlib_snapshot.total_out;

  if (err == Z_STREAM_END) {
    finished_ = true;
  } else if (err == Z_OK) {
    if (inflated_bytes < buf_size) {
      // Sanity check that if we didn't fill the output buffer, it's
      // because we consumed all of the input.
      DCHECK(!HasUnconsumedInput());
    }
  } else if (err == Z_BUF_ERROR) {
    // Sanity check that if we encountered this error, it's because we
    // were unable to write any inflated bytes to the output
    // buffer. zlib documentation says that this is a non-terminal
    // error, so we do not set error_ to true here.
    DCHECK_EQ(inflated_bytes, static_cast<size_t>(0));
  } else {
    error_ = true;
    return -1;
  }

  return static_cast<int>(inflated_bytes);
}

void GzipInflater::ShutDown() {
  Free();
}

// One-shot contiguous-buffer inflate/deflate code adapted from
// http://www.zlib.net/zpipe.c.  The Inflate usage model here is
// a little simpler to use than the incremental InflateBytes flow.
//
// TODO(jmarantz): make an incremental interface to Deflate.
bool GzipInflater::Deflate(StringPiece in, InflateType format,
                           int compression_level, Writer *writer) {
  z_stream strm;
  char out[kStackBufferSize];

  // set compression level
  if (compression_level < 0 || compression_level > 9) {
    compression_level = Z_DEFAULT_COMPRESSION;
  }

  // allocate deflate state
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  int ret;
  if (format == kGzip) {
    ret = deflateInit2(&strm, compression_level, Z_DEFLATED, 16 | 15, 8,
                       Z_DEFAULT_STRATEGY);
  } else {
    ret = deflateInit(&strm, compression_level);
  }
  if (ret != Z_OK) {
    return false;
  }

  // compress until end of file
  strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  strm.avail_in = in.size();
  // run deflate() on input until output buffer not full, finish
  // compression if all of source has been read in
  do {
    strm.avail_out = kStackBufferSize;
    strm.next_out = reinterpret_cast<Byte*>(out);
    ret = deflate(&strm, Z_FINISH);    // no bad return value
    if (ret == Z_STREAM_ERROR) {
      return false;
    }
    int have = kStackBufferSize - strm.avail_out;
    if (!writer->Write(StringPiece(out, have), NULL)) {
      deflateEnd(&strm);
      return false;
    }
  } while (strm.avail_out == 0);
  if (strm.avail_in != 0) {
    return false;
  }

  // clean up and return
  deflateEnd(&strm);
  return true;
}

bool GzipInflater::Deflate(StringPiece in, InflateType format, Writer* writer) {
  return GzipInflater::Deflate(in, format, Z_DEFAULT_COMPRESSION, writer);
}

// TODO(jmarantz): Consider using the incremental interface to implement
// Inflate.
bool GzipInflater::Inflate(StringPiece in, InflateType format, Writer* writer) {
  z_stream strm;
  char out[kStackBufferSize];
  const int kOutSize = sizeof(out);

  // allocate inflate state
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  strm.avail_in = 0;
  strm.next_in = Z_NULL;
  if (format == kGzip) {
    if (inflateInit2(&strm, (16 + MAX_WBITS)) != Z_OK) {
      return false;
    }
  } else {
    if (inflateInit(&strm) != Z_OK) {
      return false;
    }
  }

  strm.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
  strm.avail_in = in.size();

  // run inflate() on input until output buffer not full
  do {
    strm.avail_out = kOutSize;
    strm.next_out = reinterpret_cast<Bytef*>(out);
    switch (inflate(&strm, Z_NO_FLUSH)) {
      case Z_STREAM_ERROR:
        LOG(DFATAL) << "state should not be not clobbered";
        FALLTHROUGH_INTENDED;
      case Z_NEED_DICT:
        FALLTHROUGH_INTENDED;
      case Z_DATA_ERROR:
      case Z_MEM_ERROR:
        inflateEnd(&strm);
        return false;
      case Z_STREAM_END:
        break;
      default:
        break;
    }
    int have = kOutSize - strm.avail_out;
    if (!writer->Write(StringPiece(static_cast<char*>(out), have), NULL)) {
      inflateEnd(&strm);
      return false;
    }
  } while (strm.avail_out == 0);

  // clean up and return
  inflateEnd(&strm);
  return true;
}

// All gzip files start with a ten-byte header beginning with 0x1f8b.
bool GzipInflater::HasGzipMagicBytes(StringPiece in) {
  return in.size() >= 10 &&
      in[0] == static_cast<char>(0x1f) &&
      in[1] == static_cast<char>(0x8b);
}

}  // namespace net_instaweb
