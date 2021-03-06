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

#ifndef NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_COUNTER_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_COUNTER_H_

#include "net/instaweb/rewriter/public/css_tag_scanner.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class GoogleUrl;
class MessageHandler;

// "Transformer" that records the URLs it sees (with counts) instead of
// applying any transformation.
class CssUrlCounter : public CssTagScanner::Transformer {
 public:
  // base_url and handler must live longer than CssUrlCounter.
  CssUrlCounter(const GoogleUrl* base_url, MessageHandler* handler)
      : base_url_(base_url), handler_(handler) {}
  ~CssUrlCounter() override;

  // Record and count URLs in in_text. Does not reset url_counts_, so if you
  // call this multiple times it will accumulate url_counts_ over all inputs.
  // Returns False if CssUrlCounter found unparseable URLs.
  bool Count(const StringPiece& in_text);

  // Access URL occurrence counts after you've scanned a CSS file.
  const StringIntMap& url_counts() const { return url_counts_; }

 private:
  // CssTagScanner::Transform interface. Called indirectly by Count().
  TransformStatus Transform(GoogleString* str) override;

  // Counts for how many times each URL was found in the CSS file.
  StringIntMap url_counts_;

  // Base URL for CSS file, needed to absolutify URLs in Transform.
  const GoogleUrl* base_url_;

  MessageHandler* handler_;

  DISALLOW_COPY_AND_ASSIGN(CssUrlCounter);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_CSS_URL_COUNTER_H_
