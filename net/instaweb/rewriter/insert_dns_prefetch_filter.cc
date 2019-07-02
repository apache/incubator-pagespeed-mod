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

// Filter to inject <link rel="dns-prefetch" href="//www.example.com"> tags in
// the HEAD to enable the browser to do DNS prefetching.

#include "net/instaweb/rewriter/public/insert_dns_prefetch_filter.h"

#include <cstdlib>
#include <memory>
#include <set>
#include <utility>                      // for pair
#include <vector>

#include "net/instaweb/rewriter/flush_early.pb.h"
#include "net/instaweb/rewriter/public/resource_tag_scanner.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_element.h"
#include "pagespeed/kernel/html/html_name.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/semantic_type.h"
#include "pagespeed/kernel/http/user_agent_matcher.h"
#include "pagespeed/opt/logging/enums.pb.h"
#include "pagespeed/opt/logging/log_record.h"

namespace {
// Maximum number of DNS prefetch tags inserted in an HTML page.
const int kMaxDnsPrefetchTags = 8;

// Maximum difference between the number of domains in two rewrites to consider
// the domains list stable.
const int kMaxDomainDiff = 2;

// Below are a couple of values of the "rel" attribute of LINK tag which are
// relevant to DNS prefetch.
const char* kRelPrefetch = "prefetch";
const char* kRelDnsPrefetch = "dns-prefetch";
}  // namespace

namespace net_instaweb {

InsertDnsPrefetchFilter::InsertDnsPrefetchFilter(RewriteDriver* driver)
    : CommonFilter(driver) {
  Clear();
}

InsertDnsPrefetchFilter::~InsertDnsPrefetchFilter() {
}

void InsertDnsPrefetchFilter::DetermineEnabled(GoogleString* disabled_reason) {
  set_is_enabled(true);
  driver()->set_write_property_cache_dom_cohort(true);
}

void InsertDnsPrefetchFilter::Clear() {
  dns_prefetch_inserted_ = false;
  in_head_ = false;
  domains_to_ignore_.clear();
  domains_in_body_.clear();
  dns_prefetch_domains_.clear();
  user_agent_supports_dns_prefetch_ = false;
}

// Read the information related to DNS prefetch tags from the property cache
// info and populate it in the driver's flush_early_info.
void InsertDnsPrefetchFilter::StartDocumentImpl() {
  Clear();
  // Avoid inserting the domain name of this page by pre-inserting it into
  // domains_to_ignore_.
  GoogleString host = driver()->base_url().Host().as_string();
  domains_to_ignore_.insert(host);
  user_agent_supports_dns_prefetch_ =
      driver()->server_context()->user_agent_matcher()->SupportsDnsPrefetch(
          driver()->user_agent());
  RewriterHtmlApplication::Status status = user_agent_supports_dns_prefetch_ ?
      RewriterHtmlApplication::ACTIVE :
      RewriterHtmlApplication::USER_AGENT_NOT_SUPPORTED;
  driver()->log_record()->LogRewriterHtmlStatus(
      RewriteOptions::FilterId(RewriteOptions::kInsertDnsPrefetch),
      status);
}

// Write the information about domains gathered in this rewrite into the
// driver's flush_early_info. This will be written to the property cache when
// the DOM cohort is written. We write a limited set of entries to avoid
// thrashing the browser's DNS cache.
void InsertDnsPrefetchFilter::EndDocument() {
  FlushEarlyInfo* flush_early_info = driver()->flush_early_info();
  flush_early_info->set_total_dns_prefetch_domains_previous(
      flush_early_info->total_dns_prefetch_domains());
  flush_early_info->set_total_dns_prefetch_domains(
      dns_prefetch_domains_.size());
  flush_early_info->clear_dns_prefetch_domains();
  StringVector::const_iterator end = dns_prefetch_domains_.end();
  for (StringVector::const_iterator it = dns_prefetch_domains_.begin();
       it != end; ++it) {
    flush_early_info->add_dns_prefetch_domains(*it);
    if (flush_early_info->dns_prefetch_domains_size() >=
        kMaxDnsPrefetchTags) {
      break;
    }
  }
}

// When a resource url is encountered, try to add its domain to the list of
// domains for which DNS prefetch tags can be inserted. DNS prefetch tags added
// by the origin server will automatically be excluded since we process LINK
// tags.
// TODO(bharathbhushan): Make sure that this filter does not insert DNS prefetch
// tags for resources inserted by the flush early filter.
void InsertDnsPrefetchFilter::StartElementImpl(HtmlElement* element) {
  if (element->keyword() == HtmlName::kHead) {
    in_head_ = true;
    return;
  }
  // We don't need to add domains in NOSCRIPT elements since most browsers
  // support javascript and won't download resources inside NOSCRIPT elements.
  if (noscript_element() != NULL) {
    return;
  }
  resource_tag_scanner::UrlCategoryVector attributes;
  resource_tag_scanner::ScanElement(element, driver()->options(), &attributes);
  for (int i = 0, n = attributes.size(); i < n; ++i) {
    switch (attributes[i].category) {
      // The categories below are downloaded by the browser to display the page.
      // So DNS prefetch hints are useful.
      case semantic_type::kImage:
      case semantic_type::kScript:
      case semantic_type::kStylesheet:
      case semantic_type::kOtherResource:
        MarkAlreadyInHead(attributes[i].url);
        break;

      case semantic_type::kPrefetch:
        if (element->keyword() == HtmlName::kLink) {
          // For LINK tags, many of the link types are detected as image or
          // stylesheet by the ResourceTagScanner. "prefetch" and "dns-prefetch"
          // are recognized here since they are relevant for resource download.
          // If a DNS prefetch tag inserted by the origin server is found in
          // BODY, it is not useful to insert it but calling MarkAlreadyInHead
          // will insert it.  So we avoid calling MarkAlreadyInHead in this
          // specific case.
          HtmlElement::Attribute* rel_attr =
              element->FindAttribute(HtmlName::kRel);
          if (rel_attr != NULL) {
            if (StringCaseEqual(rel_attr->DecodedValueOrNull(), kRelPrefetch) ||
                (in_head_ && StringCaseEqual(rel_attr->DecodedValueOrNull(),
                                             kRelDnsPrefetch))) {
              MarkAlreadyInHead(attributes[i].url);
            }
          }
        }
        break;

      case semantic_type::kHyperlink:
      case semantic_type::kUndefined:
        break;
    }
  }
}

// At the end of the first HEAD, insert the DNS prefetch tags if the list of
// domains is stable.
void InsertDnsPrefetchFilter::EndElementImpl(HtmlElement* element) {
  if (!user_agent_supports_dns_prefetch_) {
    return;
  }
  if (element->keyword() == HtmlName::kHead) {
    in_head_ = false;
    if (!dns_prefetch_inserted_) {
      // Don't add <link rel='dns-prefetch' ...> tags if we flushed them in
      // the flush early flow.
      dns_prefetch_inserted_ = true;
      const FlushEarlyInfo& flush_early_info = *(driver()->flush_early_info());
      if (IsDomainListStable(flush_early_info)) {
        const char* tag_to_insert =
            driver()->user_agent_matcher()->SupportsDnsPrefetchUsingRelPrefetch(
                driver()->user_agent()) ? kRelPrefetch : kRelDnsPrefetch;
        protobuf::RepeatedPtrField<GoogleString>::const_iterator end =
            flush_early_info.dns_prefetch_domains().end();
        for (protobuf::RepeatedPtrField<GoogleString>::const_iterator it =
             flush_early_info.dns_prefetch_domains().begin();
             it != end; ++it) {
          HtmlElement* link = driver()->NewElement(element, HtmlName::kLink);
          driver()->AddAttribute(link, HtmlName::kRel, tag_to_insert);
          driver()->AddAttribute(link, HtmlName::kHref, StrCat("//", *it));
          driver()->AppendChild(element, link);
          driver()->log_record()->SetRewriterLoggingStatus(
              RewriteOptions::FilterId(RewriteOptions::kInsertDnsPrefetch),
              RewriterApplication::APPLIED_OK);
        }
      } else {
        driver()->log_record()->SetRewriterLoggingStatus(
            RewriteOptions::FilterId(RewriteOptions::kInsertDnsPrefetch),
            RewriterApplication::NOT_APPLIED);
      }
    }
  }
}

void InsertDnsPrefetchFilter::MarkAlreadyInHead(
    HtmlElement::Attribute* urlattr) {
  if (urlattr != NULL && urlattr->DecodedValueOrNull() != NULL) {
    GoogleUrl url(driver()->base_url(), urlattr->DecodedValueOrNull());
    GoogleString domain;
    if (url.IsWebValid()) {
      url.Host().CopyToString(&domain);
    }
    if (!domain.empty()) {
      if (in_head_) {
        std::pair<StringSet::iterator, bool> result =
            domains_to_ignore_.insert(domain);
        if (driver()->options()->Enabled(RewriteOptions::kFlushSubresources)
            && result.second) {
          // Prefetch dns for the domains present in the head if flush
          // sub-resources filter is enabled.
          dns_prefetch_domains_.push_back(domain);
        }
      } else {
        if (domains_to_ignore_.find(domain) == domains_to_ignore_.end()) {
          std::pair<StringSet::iterator, bool> result =
              domains_in_body_.insert(domain);
          if (result.second) {
            dns_prefetch_domains_.push_back(domain);
          }
        }
      }
    }
  }
}

// Say we are doing the 'n'th rewrite. If the number of domains eligible for DNS
// prefetch tags in 'n-1'th and 'n-2'th rewrite differs by at most
// kMaxDomainDiff, then the list is considered stable and this method returns
// true in that case.
bool InsertDnsPrefetchFilter::IsDomainListStable(
    const FlushEarlyInfo& flush_early_info) const {
  return std::abs(flush_early_info.total_dns_prefetch_domains() -
      flush_early_info.total_dns_prefetch_domains_previous()) <= kMaxDomainDiff;
}

}  // namespace net_instaweb
