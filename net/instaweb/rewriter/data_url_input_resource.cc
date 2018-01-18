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


#include "net/instaweb/rewriter/public/data_url_input_resource.h"

#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/data_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

DataUrlInputResource::DataUrlInputResource(const GoogleString* url,
                                           Encoding encoding,
                                           const ContentType* type,
                                           const StringPiece& encoded_contents,
                                           const RewriteDriver* driver)
    : Resource(driver, type),
      url_(url),
      encoding_(encoding),
      encoded_contents_(encoded_contents) {
  // Make sure we auto-load.
  if (DecodeDataUrlContent(encoding_, encoded_contents_, &decoded_contents_) &&
      value_.Write(decoded_contents_,
                   driver->server_context()->message_handler())) {
    // Note that we do not set caching headers here.
    // This is because they are expensive to compute; and should not be used
    // for this resource anyway, as it has UseHttpCache() false, and provides
    // IsValidAndCacheable() and an ALWAYS_VALID output of
    // FillInPartitionInputInfo.
    response_headers_.set_major_version(1);
    response_headers_.set_minor_version(1);
    response_headers_.SetStatusAndReason(HttpStatus::kOK);
    response_headers_.Add(HttpAttributes::kContentType, type_->mime_type());
    value_.SetHeaders(&response_headers_);
  }
}

DataUrlInputResource::~DataUrlInputResource() {}

// data: URLs never expire. So we only check that it was decoded correctly.
bool DataUrlInputResource::IsValidAndCacheable() const {
  return response_headers_.status_code() == HttpStatus::kOK;
}

void DataUrlInputResource::FillInPartitionInputInfo(
    HashHint include_content_hash, InputInfo* input) {
  input->set_type(InputInfo::ALWAYS_VALID);
}

void DataUrlInputResource::LoadAndCallback(
    NotCacheablePolicy not_cacheable_policy,
    const RequestContextPtr& request_context,
    AsyncCallback* callback) {
  callback->Done(false /* lock_failure */, loaded());
}

}  // namespace net_instaweb
