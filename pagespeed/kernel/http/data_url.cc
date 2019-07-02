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


#include "pagespeed/kernel/http/data_url.h"

#include <cstddef>
#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/base64_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/content_type.h"

namespace net_instaweb {

void DataUrl(const ContentType& content_type,
             const Encoding encoding,
             const StringPiece& content,
             GoogleString* result) {
  result->assign("data:");
  result->append(content_type.mime_type());
  switch (encoding) {
    case BASE64: {
      result->append(";base64,");
      GoogleString encoded;
      Mime64Encode(content, &encoded);
      result->append(encoded);
      break;
    }
// TODO(jmaessen): Figure out if we ever need non-BASE64 encodings,
// and if so actually write them.  Here are the stubs.
//     case UTF8:
//       result->append(";charset=\"utf-8\",");
//       // TODO(jmaessen): find %-encoding code to use here.
//       //   jmarantz has one pending.
//       result.append(content);
//     case LATIN1:
//       result->append(";charset=\"\",");
//       // TODO(jmaessen): find %-encoding code to use here.
//       //   Not the UTF-8 one!
    default: {
      // either UNKNOWN or PLAIN.  No special encoding or alphabet.  We're in a
      // context where we don't want to fail, so we try to give sensible output
      // if encoding is actually out of range; this gives some hope of graceful
      // degradation of experience.
      result->append(",");
      content.AppendToString(result);
      break;
    }
  }
}

bool IsDataUrl(const StringPiece url) {
  return strings::StartsWith(url, "data:");
}

bool IsDataImageUrl(const StringPiece url) {
  return strings::StartsWith(url, "data:image/");
}

bool ParseDataUrl(const StringPiece& url,
                  const ContentType** content_type,
                  Encoding* encoding,
                  StringPiece* encoded_content) {
  const char kData[] = "data:";
  const size_t kDataSize = STATIC_STRLEN(kData);
  const char kBase64[] = ";base64";
  const size_t kBase64Size = STATIC_STRLEN(kBase64);
  // First invalidate all outputs.
  *content_type = NULL;
  *encoding = UNKNOWN;
  *encoded_content = StringPiece();
  size_t header_boundary = url.find(',');
  if (header_boundary == url.npos || !strings::StartsWith(url, kData)) {
    return false;
  }
  StringPiece header(url.data(), header_boundary);
  size_t mime_boundary = header.find(';');
  if (mime_boundary == url.npos) {
    // no charset or base64 encoding.
    mime_boundary = header_boundary;
    *encoding = PLAIN;
  } else if (header_boundary >= mime_boundary + kBase64Size) {
    if (strings::EndsWith(header, kBase64)) {
      *encoding = BASE64;
    } else {
      *encoding = PLAIN;
    }
  }
  StringPiece mime_type(url.data() + kDataSize, mime_boundary - kDataSize);
  *content_type = MimeTypeToContentType(mime_type);
  encoded_content->set(url.data() + header_boundary + 1,
                       url.size() - header_boundary - 1);
  return true;
}

bool DecodeDataUrlContent(Encoding encoding,
                          const StringPiece& encoded_content,
                          GoogleString* decoded_content) {
  switch (encoding) {
    case PLAIN:
      // No change, just copy data.
      encoded_content.CopyToString(decoded_content);
      return true;
    case BASE64:
      return Mime64Decode(encoded_content, decoded_content);
    default:
      return false;
  }
}

}  // namespace net_instaweb
