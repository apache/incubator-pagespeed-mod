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


#include "pagespeed/kernel/http/query_params.h"

#include "base/logging.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

void QueryParams::ParseFromUrl(const GoogleUrl& gurl) {
  CHECK_EQ(0, size());
  map_.AddFromNameValuePairs(gurl.Query(), "&", '=',
                             /* omit_if_no_value= */ false);
}

void QueryParams::ParseFromUntrustedString(StringPiece query_param_string) {
  GoogleUrl gurl(StrCat("http://www.example.com/?", query_param_string));
  ParseFromUrl(gurl);
}

bool QueryParams::UnescapedValue(int index, GoogleString* unescaped_val) const {
  const GoogleString* val = map_.value(index);
  if (val == NULL) {
    return false;
  }
  // TODO(jmarantz): make GoogleUrl::UnescapeQueryParam check for invalid
  // encodings.
  *unescaped_val = GoogleUrl::UnescapeQueryParam(*val);
  return true;
}

bool QueryParams::Lookup1Unescaped(const StringPiece& name,
                                   GoogleString* unescaped_val) const {
  const GoogleString* val = map_.Lookup1(name);
  if (val == NULL) {
    return false;
  }
  // TODO(jmarantz): make GoogleUrl::UnescapeQueryParam check for invalid
  // encodings.
  *unescaped_val = GoogleUrl::UnescapeQueryParam(*val);
  return true;
}

GoogleString QueryParams::ToEscapedString() const {
  GoogleString str;
  const char* prefix="";
  for (int i = 0; i < size(); ++i) {
    const GoogleString* escaped_value = EscapedValue(i);
    if (escaped_value == NULL) {
      StrAppend(&str, prefix, name(i));
    } else {
      StrAppend(&str, prefix, name(i), "=", *escaped_value);
    }
    prefix = "&";
  }
  return str;
}

}  // namespace net_instaweb
