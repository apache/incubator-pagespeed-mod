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

#include "net/instaweb/rewriter/public/css_url_encoder.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/request_properties.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/util/url_escaper.h"

namespace net_instaweb {

CssUrlEncoder::~CssUrlEncoder() { }

void CssUrlEncoder::Encode(const StringVector& urls,
                           const ResourceContext* data,
                           GoogleString* rewritten_url) const {
  DCHECK(data != NULL) << "null data passed to CssUrlEncoder::Encode";
  DCHECK_EQ(1U, urls.size());

  rewritten_url->append("A.");

  UrlEscaper::EncodeToUrlSegment(urls[0], rewritten_url);
}

// The generic Decode interface is supplied so that
// RewriteSingleResourceFilter and/or RewriteDriver can decode any
// ResourceNamer::name() field and find the set of URLs that are
// referenced.
bool CssUrlEncoder::Decode(const StringPiece& encoded,
                           StringVector* urls,
                           ResourceContext* data,
                           MessageHandler* handler) const {
  CHECK(data != NULL);
  if ((encoded.size() < 2) || (encoded[1] != '.')) {
    handler->Message(kWarning, "Invalid CSS Encoding: %s",
                     encoded.as_string().c_str());
    return false;
  }
  switch (encoded[0]) {
    case 'V':
      data->set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_LOSSLESS_ALPHA);
      data->set_inline_images(true);
      break;
    case 'W':
      data->set_libwebp_level(ResourceContext::LIBWEBP_LOSSY_ONLY);
      data->set_inline_images(true);
      break;
    case 'I':
      data->set_libwebp_level(ResourceContext::LIBWEBP_NONE);
      data->set_inline_images(true);
      break;
    case 'A':
      break;
  }

  GoogleString* url = StringVectorAdd(urls);
  StringPiece remaining = encoded.substr(2);
  if (UrlEscaper::DecodeFromUrlSegment(remaining, url)) {
    return true;
  } else {
    urls->pop_back();
    return false;
  }
}

void CssUrlEncoder::SetInliningImages(
    const RequestProperties& request_properties,
    ResourceContext* resource_context) {
  DCHECK(resource_context != NULL)
      << "null data passed to CssUrlEncoder::SetInliningImages";

  resource_context->set_inline_images(
      request_properties.SupportsImageInlining());
}

}  // namespace net_instaweb
