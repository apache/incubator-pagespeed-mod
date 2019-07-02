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

#include "pagespeed/opt/ads/ads_attribute.h"

#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {
namespace ads_attribute {

// Names of attributes used in adsbygoogle snippets.
const char kDataAdClient[] = "data-ad-client";
const char kDataAdChannel[] = "data-ad-channel";
const char kDataAdSlot[] = "data-ad-slot";
const char kDataAdFormat[] = "data-ad-format";

// Names of attributes used in showads snippets.
const char kGoogleAdClient[] = "google_ad_client";
const char kGoogleAdChannel[] = "google_ad_channel";
const char kGoogleAdSlot[] = "google_ad_slot";
const char kGoogleAdFormat[] = "google_ad_format";
const char kGoogleAdWidth[] = "google_ad_width";
const char kGoogleAdHeight[] = "google_ad_height";
const char kGoogleAdOutput[] = "google_ad_output";

GoogleString LookupAdsByGoogleAttributeName(
    StringPiece show_ads_attribute_name) {
  StringPieceVector items;
  SplitStringPieceToVector(
      show_ads_attribute_name, "_", &items, false); /* don't omit empty */
  // TODO(chenyu): check if 'show_ads_attribute_name' is valid.
  if (items.size() < 2 || items.at(0) != "google") {
    return "";
  }

  GoogleString result = "data";
  for (int i = 1, n = items.size(); i < n; ++i) {
    StrAppend(&result, "-", items.at(i));
  }
  return result;
}

}  // namespace ads_attribute
}  // namespace net_instaweb
