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

//     and sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_combine_filter.h"

#include <vector>

#include "base/logging.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/debug_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "pagespeed/kernel/base/charset_util.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mem_file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/statistics.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/cache/lru_cache.h"
#include "pagespeed/kernel/html/html_parse_test_base.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/http/semantic_type.h"

namespace net_instaweb {

namespace {

const char kDomain[] = "http://combine_css.test/";
const char kProxyMapDomain[] = "http://proxy.test/";
const char kYellow[] = ".yellow {background-color: yellow;}";
const char kBlue[] = ".blue {color: blue;}\n";
const char kACssBody[] = ".c1 {\n background-color: blue;\n}\n";
const char kBCssBody[] = ".c2 {\n color: yellow;\n}\n";
const char kCCssBody[] = ".c3 {\n font-weight: bold;\n}\n";
const char kCssA[] = "a.css";
const char kCssB[] = "b.css";

class CssCombineFilterTest : public RewriteTestBase {
 protected:
  CssCombineFilterTest()
      : css_combine_opportunities_(statistics()->GetVariable(
            CssCombineFilter::kCssCombineOpportunities)),
        css_file_count_reduction_(statistics()->GetVariable(
            CssCombineFilter::kCssFileCountReduction)) {}

  virtual void SetUp() {
    RewriteTestBase::SetUp();
    options()->set_support_noscript_enabled(false);
    AddFilter(RewriteOptions::kCombineCss);
    AddOtherFilter(RewriteOptions::kCombineCss);
  }

  // Test spriting CSS with options to write headers and use a hasher.
  void CombineCss(const StringPiece& id,
                  const StringPiece& barrier_text,
                  const StringPiece& debug_text,
                  bool is_barrier) {
    CombineCssWithNames(id, barrier_text, debug_text, is_barrier,
                        kCssA, kCssB, true);
  }

  // Synthesizes an HTML css link element, with no media tag.
  virtual GoogleString Link(const StringPiece& href) {
    return Link(href, "", false);
  }

  // Synthesizes an HTML css link element.  If media is non-empty, then a
  // media tag is included.
  virtual GoogleString Link(const StringPiece& href, const StringPiece& media,
                            bool close) {
    GoogleString out(StrCat(
        "<link rel=\"stylesheet\" type=\"text/css\" href=\"", href, "\""));
    if (!media.empty()) {
      StrAppend(&out, " media=\"", media, "\"");
    }
    if (close) {
      out.append("/");
    }
    out.append(">");
    return out;
  }

  void SetupCssResources(const char* a_css_name,
                         const char* b_css_name) {
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(StrCat(kDomain, a_css_name), default_css_header,
                     kACssBody);
    SetFetchResponse(StrCat(kDomain, b_css_name), default_css_header,
                     kBCssBody);
    SetFetchResponse(StrCat(kDomain, "c.css"), default_css_header, kCCssBody);
  }

  void CombineCssWithNames(const StringPiece& id,
                           const StringPiece& barrier_text,
                           const StringPiece& debug_text,
                           bool is_barrier,
                           const char* a_css_name,
                           const char* b_css_name,
                           bool expect_combine) {
    // URLs and content for HTML document and resources.
    CHECK_EQ(StringPiece::npos, id.find("/"));
    GoogleString html_url = StrCat(kDomain, id, ".html");
    GoogleString a_css_url = StrCat(kDomain, a_css_name);
    GoogleString b_css_url = StrCat(kDomain, b_css_name);
    GoogleString c_css_url = StrCat(kDomain, "c.css");

    GoogleString html_input = StrCat(
        "<head>\n"
        "  ", Link(a_css_name), "\n"
        "  ", Link(b_css_name), "\n");
    StrAppend(&html_input,
        "  <title>Hello, Instaweb</title>\n",
        barrier_text,
        "</head>\n"
        "<body>\n"
        "  <div class='c1'>\n"
        "    <div class='c2'>\n"
        "      Yellow on Blue\n"
        "    </div>\n"
        "  </div>\n"
        "  ", Link("c.css"), "\n"
        "</body>\n");

    SetupCssResources(a_css_name, b_css_name);

    int orig_file_count_reduction = css_file_count_reduction_->Get();

    ParseUrl(html_url, html_input);

    if (combined_headers_.empty()) {
      AppendDefaultHeaders(kContentTypeCss, &combined_headers_);
    }

    // Check for CSS files in the rewritten page.
    StringVector css_urls;
    CollectCssLinks(id, output_buffer_, &css_urls);
    EXPECT_LE(1UL, css_urls.size());

    const GoogleString& combine_url = css_urls[0];
    GoogleUrl gurl(combine_url);

    // Expected CSS combination.
    // This syntax must match that in css_combine_filter
    // a.css + b.css => a+b.css
    GoogleString expected_combination = StrCat(kACssBody, kBCssBody);
    int expected_file_count_reduction = orig_file_count_reduction + 1;
    if (!is_barrier) {
      // a.css + b.css + c.css => a+b+c.css
      expected_combination.append(kCCssBody);
      expected_file_count_reduction = orig_file_count_reduction + 2;
    }

    if (!expect_combine) {
      expected_file_count_reduction = 0;
    }

    EXPECT_EQ(expected_file_count_reduction, css_file_count_reduction_->Get());
    if (expected_file_count_reduction > 0) {
      EXPECT_STREQ(RewriteOptions::kCssCombinerId,
                   AppliedRewriterStringFromLog());
    }

    GoogleString expected_output(AddHtmlBody(StrCat(
        "<head>\n"
        "  ", Link(combine_url), "\n"
        "  \n"  // The whitespace from the original link is preserved here ...
        "  <title>Hello, Instaweb</title>\n",
        debug_text,
        barrier_text,
        "</head>\n"
        "<body>\n"
        "  <div class='c1'>\n"
        "    <div class='c2'>\n"
        "      Yellow on Blue\n"
        "    </div>\n"
        "  </div>\n"
        "  ", (is_barrier ? Link("c.css") : ""), "\n"
        "</body>\n")));
    if (!debug_text.empty()) {
      EXPECT_HAS_SUBSTR(DebugFilter::FormatEndDocumentMessage(
                        0, 0, 0, 0, 0, false, StringSet(), StringVector()),
                    output_buffer_);
    }
    if (expect_combine) {
      EXPECT_HAS_SUBSTR(expected_output, output_buffer_);

      // Fetch the combination to make sure we can serve the result from above.
      ExpectStringAsyncFetch expect_callback(true, CreateRequestContext());
      GoogleUrl base_url(html_url);
      GoogleUrl combine_gurl(base_url, combine_url);
      rewrite_driver()->FetchResource(combine_gurl.Spec(), &expect_callback);
      rewrite_driver()->WaitForCompletion();
      EXPECT_EQ(HttpStatus::kOK,
                expect_callback.response_headers()->status_code())
          << combine_gurl.Spec();
      EXPECT_EQ(expected_combination, expect_callback.buffer());

      // Now try to fetch from another server (other_rewrite_driver()) that
      // does not already have the combination cached.
      // TODO(sligocki): This has too much shared state with the first server.
      // See RewriteImage for details.
      ExpectStringAsyncFetch other_expect_callback(true,
                                                   CreateRequestContext());
      message_handler_.Message(kInfo, "Now with serving.");
      file_system()->Enable();
      other_rewrite_driver()->FetchResource(combine_gurl.Spec(),
                                            &other_expect_callback);
      other_rewrite_driver()->WaitForCompletion();
      EXPECT_EQ(HttpStatus::kOK,
                other_expect_callback.response_headers()->status_code());
      EXPECT_EQ(expected_combination, other_expect_callback.buffer());

      // Try to fetch from an independent server.
      ServeResourceFromManyContexts(combine_gurl.spec_c_str(),
                                    expected_combination);
    }
  }

  // Version of Encode specifically for testing combination of combine_css
  // and css_filter.  We can juggle the order of these filters, and this
  // provides us with a single place to repair.
  GoogleString EncodeCssCombineAndOptimize(const StringPiece& path,
                                           const StringVector& name_vector) {
    const bool kCombineAndThenOptimize = true;
    if (kCombineAndThenOptimize) {
      return Encode(path, RewriteOptions::kCssFilterId, "0",
                    Encode("", RewriteOptions::kCssCombinerId, "0",
                           name_vector, "css"), "css");
    } else {
      StringVector optimized_css_names;
      for (int i = 0, size = name_vector.size(); i < size; ++i) {
        optimized_css_names.push_back(
            Encode("", RewriteOptions::kCssFilterId, "0",
                   name_vector[i], "css"));
      }
      return Encode(path, RewriteOptions::kCssCombinerId, "0",
                    optimized_css_names, "css");
    }
  }

  // Test what happens when CSS combine can't find a previously-rewritten
  // resource during a subsequent resource fetch.  This used to segfault.
  void CssCombineMissingResource() {
    GoogleString a_css_url = StrCat(kDomain, kCssA);
    GoogleString c_css_url = StrCat(kDomain, "c.css");

    GoogleString expected_combination = StrCat(kACssBody, kCCssBody);

    // Put original CSS files into our fetcher.
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(a_css_url, default_css_header, kACssBody);
    SetFetchResponse(c_css_url, default_css_header, kCCssBody);

    // First make sure we can serve the combination of a & c.  This is to avoid
    // spurious test successes.

    GoogleString kACUrl =
        Encode(kDomain, RewriteOptions::kCssCombinerId, "0",
               MultiUrl(kCssA, "c.css"), "css");
    GoogleString kABCUrl = Encode(kDomain, RewriteOptions::kCssCombinerId, "0",
                                  MultiUrl(kCssA, "bbb.css", "c.css"),
                                  "css");
    ExpectStringAsyncFetch expect_callback(
        true, RequestContext::NewTestRequestContext(
            server_context()->thread_system()));

    // NOTE: This first fetch used to return status 0 because response_headers
    // weren't initialized by the first resource fetch (but were cached
    // correctly).  Content was correct.
    EXPECT_TRUE(rewrite_driver()->FetchResource(kACUrl, &expect_callback));
    rewrite_driver()->WaitForCompletion();
    EXPECT_EQ(HttpStatus::kOK,
              expect_callback.response_headers()->status_code());
    EXPECT_EQ(expected_combination, expect_callback.buffer());

    // We repeat the fetch to prove that it succeeds from cache:
    expect_callback.Reset();
    EXPECT_TRUE(rewrite_driver()->FetchResource(kACUrl, &expect_callback));
    rewrite_driver()->WaitForCompletion();
    EXPECT_EQ(HttpStatus::kOK,
              expect_callback.response_headers()->status_code());
    EXPECT_EQ(expected_combination, expect_callback.buffer());

    // Now let's try fetching the url that references a missing resource
    // (bbb.css) in addition to the two that do exist, a.css and c.css.  Using
    // an entirely non-existent resource appears to test a strict superset of
    // filter code paths when compared with returning a 404 for the resource.
    SetFetchFailOnUnexpected(false);
    ExpectStringAsyncFetch fail_callback(
        false, RequestContext::NewTestRequestContext(
            server_context()->thread_system()));
    EXPECT_TRUE(
        rewrite_driver()->FetchResource(kABCUrl, &fail_callback));
    rewrite_driver()->WaitForCompletion();

    // What status we get here depends a lot on details of when exactly
    // we detect the failure. If done early enough, nothing will be set.
    // This test may change, but see also
    // ResourceCombinerTest.TestContinuingFetchWhenFastFailed
    EXPECT_EQ("", fail_callback.buffer());
  }

  // Common framework for testing barriers.  A null-terminated set of css
  // names is specified, with optional media tags.  E.g.
  //   static const char* link[] {
  //     kCssA,
  //     "styles/b.css",
  //     "print.css media=print",
  //   }
  //
  // The output of this function is the collected CSS links after rewrite.
  void BarrierTestHelper(
      const StringPiece& id,
      const CssLink::Vector& input_css_links,
      CssLink::Vector* output_css_links) {
    // TODO(sligocki): Allow other domains (this is constrained right now b/c
    // of SetResponseWithDefaultHeaders.
    GoogleString html_url = StrCat(kTestDomain, id, ".html");
    GoogleString html_input("<head>\n");
    for (int i = 0, n = input_css_links.size(); i < n; ++i) {
      const CssLink* link = input_css_links[i];
      if (!link->url_.empty()) {
        if (link->supply_mock_) {
          // If the css-vector contains a 'true' for this, then we supply the
          // mock fetcher with headers and content for the CSS file.
          SetResponseWithDefaultHeaders(link->url_, kContentTypeCss,
                                        link->content_, 600);
        }
        StrAppend(&html_input, "  ", Link(link->url_, link->media_, false),
                  "\n");
      } else {
        html_input += link->content_;
      }
    }
    html_input += "</head>\n<body>\n  <div class='yellow'>\n";
    html_input += "    Hello, mod_pagespeed!\n  </div>\n</body>\n";

    ParseUrl(html_url, html_input);
    CollectCssLinks("combine_css_missing_files", output_buffer_,
                    output_css_links);

    // TODO(jmarantz): fetch all content and provide output as text.
  }

  // Helper for testing handling of URLs with trailing junk
  void TestCorruptUrl(const char* new_suffix) {
    CssLink::Vector css_in, css_out;
    css_in.Add("1.css", kYellow, "", true);
    css_in.Add("2.css", kYellow, "", true);
    BarrierTestHelper("no_ext_corrupt", css_in, &css_out);
    ASSERT_EQ(1, css_out.size());
    GoogleString normal_url = css_out[0]->url_;

    ASSERT_TRUE(StringCaseEndsWith(normal_url, ".css"));
    GoogleString munged_url = StrCat(
        normal_url.substr(0, normal_url.length() - STATIC_STRLEN(".css")),
        new_suffix);

    GoogleUrl base_url(kTestDomain);
    GoogleUrl munged_gurl(base_url, munged_url);
    GoogleString out;
    EXPECT_TRUE(FetchResourceUrl(munged_gurl.Spec(),  &out));

    // Now re-do it and make sure the new suffix didn't get stuck in the URL
    STLDeleteElements(&css_out);
    css_out.clear();
    BarrierTestHelper("no_ext_corrupt", css_in, &css_out);
    ASSERT_EQ(1, css_out.size());
    EXPECT_EQ(css_out[0]->url_, normal_url);
  }

  // Test to make sure we don't miscombine things when handling the input
  // as XHTML producing non-flat <link>'s from the parser
  void TestXhtml(bool flush) {
    GoogleString a_css_url = StrCat(kTestDomain, kCssA);
    GoogleString b_css_url = StrCat(kTestDomain, kCssB);

    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(a_css_url, default_css_header, kYellow);
    SetFetchResponse(b_css_url, default_css_header, kBlue);

    GoogleString combined_url =
        Encode("", RewriteOptions::kCssCombinerId, "0",
               MultiUrl(kCssA, kCssB), "css");

    SetupWriter();
    SetXhtmlMimetype();

    rewrite_driver()->StartParse(kTestDomain);
    GoogleString input_beginning =
        StrCat(kXhtmlDtd, "<div>", Link(kCssA), Link(kCssB));
    rewrite_driver()->ParseText(input_beginning);

    if (flush) {
      // This is a regression test: previously getting a flush here would
      // cause attempts to modify data structures, as we would only
      // start seeing the links at the </div>
      rewrite_driver()->Flush();
    }
    rewrite_driver()->ParseText("</div>");
    rewrite_driver()->FinishParse();

    // Note: As of 3/25/2011 our parser ignores XHTML directives from DOCTYPE
    // or mime-type, since those are not reliable: see Issue 252.  So we
    // do sloppy HTML-style parsing in all cases.  If we were to decided that
    // we could reliably detect XHTML then we could consider tightening the
    // parser constraints, in which case the expected results from this
    // code might change depending on the 'flush' arg to this method.
    EXPECT_EQ(
        StrCat(kXhtmlDtd, "<div>", Link(combined_url, "", true), "</div>"),
        output_buffer_);
  }

  void CombineWithBaseTag(const StringPiece& html_input,
                          StringVector *css_urls) {
    // Put original CSS files into our fetcher.
    GoogleString html_url = StrCat(kDomain, "base_url.html");
    static const char kACssUrl[] = "http://other_domain.test/foo/a.css";
    static const char kBCssUrl[] = "http://other_domain.test/foo/b.css";
    static const char kCCssUrl[] = "http://other_domain.test/foo/c.css";

    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(kACssUrl, default_css_header, kACssBody);
    SetFetchResponse(kBCssUrl, default_css_header, kBCssBody);
    SetFetchResponse(kCCssUrl, default_css_header, kCCssBody);

    // Rewrite
    ParseUrl(html_url, html_input);

    // Check for CSS files in the rewritten page.
    CollectCssLinks("combine_css_no_media-links", output_buffer_, css_urls);
  }

  void TestFetch() {
    SetupCssResources(kCssA, kCssB);
    GoogleString content;
    const GoogleString combined_url =
        Encode(kDomain, RewriteOptions::kCssCombinerId, "0",
               MultiUrl(kCssA, kCssB), "css");
    ASSERT_TRUE(FetchResourceUrl(combined_url, &content));
    EXPECT_EQ(StrCat(kACssBody, kBCssBody), content);
  }

  void AddHeader(StringPiece resource, StringPiece name, StringPiece value) {
    AddToResponse(StrCat(kDomain, resource), name, value);
  }

  GoogleString SetupReconstructOriginHeaders() {
    SetHtmlMimetype();
    SetupCssResources(kCssA, kCssB);
    AddHeader(kCssA, "in_all_3", "abc");
    AddHeader(kCssB, "in_all_3", "abc");
    AddHeader("c.css", "in_all_3", "abc");
    AddHeader(kCssB, "in_b", "b");
    AddHeader("c.css", "in_c", "c");
    return Encode(kDomain, RewriteOptions::kCssCombinerId, "0",
                  MultiUrl(kCssA, kCssB, "c.css"), "css");
  }

  void VerifyCrossUnmappedDomainNotRewritten() {
    CssLink::Vector css_in, css_out;
    AddDomain("a.com");
    AddDomain("b.com");
    bool supply_mock = false;
    static const char kUrl1[] = "http://a.com/1.css";
    static const char kUrl2[] = "http://b.com/2.css";
    css_in.Add(kUrl1, kYellow, "", supply_mock);
    css_in.Add(kUrl2, kBlue, "", supply_mock);
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(kUrl1, default_css_header, kYellow);
    SetFetchResponse(kUrl2, default_css_header, kBlue);
    BarrierTestHelper("combine_css_with_style", css_in, &css_out);
    EXPECT_EQ(2, css_out.size());
    EXPECT_STREQ(kUrl1, css_out[0]->url_);
    EXPECT_STREQ(kUrl2, css_out[1]->url_);
  }

  Variable* css_combine_opportunities_;
  Variable* css_file_count_reduction_;

 private:
  GoogleString combined_headers_;
};

TEST_F(CssCombineFilterTest, CombineCss) {
  SetHtmlMimetype();
  CombineCss("combine_css_no_hash", "", "", false);
}

TEST_F(CssCombineFilterTest, CombineCssUnhealthy) {
  lru_cache()->set_is_healthy(false);
  SetHtmlMimetype();
  SetupCssResources(kCssA, kCssB);
  GoogleString html_input = StrCat(
      "<head>\n"
      "  ", Link(kCssA), "\n"
      "  ", Link(kCssB), "\n");
  ParseUrl(StrCat(kDomain, "unhealthy.html"), html_input);
  EXPECT_EQ(AddHtmlBody(html_input), output_buffer_);
}

TEST_F(CssCombineFilterTest, Fetch) {
  TestFetch();
}

// Even with the cache unhealthy, we can still fetch already-optimized
// resources.
TEST_F(CssCombineFilterTest, FetchUnhealthy) {
  lru_cache()->set_is_healthy(false);
  TestFetch();
}

TEST_F(CssCombineFilterTest, CombineCssMD5) {
  SetHtmlMimetype();
  UseMd5Hasher();
  CombineCss("combine_css_md5", "", "", false);
}

class CssCombineFilterCustomOptions : public CssCombineFilterTest {
 protected:
  // Derived classes need to set custom options and explicitly call
  // CssCombineFilterTest::SetUp();
  virtual void SetUp() {}
};

// Tests Issue 600 in which CSS files in a MapProxyDomain were not combined with
// local files but they should have been after mapping into the same domain.
TEST_F(CssCombineFilterCustomOptions, CssCombineAcrossProxyDomains) {
  // Proxy http://kProxyMapDomain/ onto http://kTestDomain/proxied/
  DomainLawyer* lawyer = options()->WriteableDomainLawyer();
  GoogleString proxy_target = StrCat(kTestDomain, "proxied/");
  ASSERT_TRUE(lawyer->AddProxyDomainMapping(proxy_target, kProxyMapDomain,
                                            StringPiece(), &message_handler_));
  CssCombineFilterTest::SetUp();
  SetHtmlMimetype();

  // Create http://kTestDomain/a.css and http://kProxyMapDomain/b.css
  GoogleString a_local_css_url = StrCat(kTestDomain, kCssA);
  GoogleString b_proxy_css_url = StrCat(kProxyMapDomain, kCssB);
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(a_local_css_url, default_css_header, kACssBody);
  SetFetchResponse(b_proxy_css_url, default_css_header, kBCssBody);

  // Parse html that links to both css files.
  GoogleString html_input(StrCat(
      "<head>\n"
      "  ", Link(a_local_css_url), "\n"
      "  ", Link(b_proxy_css_url), "\n"
      "</head>\n"));

  ParseUrl(StrCat(kTestDomain, "base_url.html"), html_input);

  // The two css files should be combined since they're now in the same domain.
  StringVector css_urls;
  CollectCssLinks("combine_css", output_buffer_, &css_urls);
  ASSERT_EQ(1UL, css_urls.size());
  // Encode doesn't allow a '/' as it expects a leaf so we have to add proxied/
  // after the encoding occurs.
  GoogleString encoded =
      Encode(kTestDomain, RewriteOptions::kCssCombinerId, "0",
             MultiUrl(kCssA, kCssB), "css");
  GlobalReplaceSubstring(kCssB, "proxied,_b.css", &encoded);
  EXPECT_STREQ(encoded, css_urls[0]);

  // Make sure we can fetch the combined resource.
  GoogleString output;
  ResponseHeaders response_headers;
  EXPECT_TRUE(FetchResourceUrl(css_urls[0], &output, &response_headers));
  EXPECT_EQ(StrCat(kACssBody, kBCssBody), output);

  // Now clear the cache and reconstruct it.
  lru_cache()->Clear();
  EXPECT_TRUE(FetchResourceUrl(css_urls[0], &output, &response_headers));
  EXPECT_EQ(StrCat(kACssBody, kBCssBody), output);
}

// Dual to CssCombineAcrossProxyDomains to ensure that if no mapping occurs we
// do not combine CSS files from different domains.
TEST_F(CssCombineFilterCustomOptions, CssDoNotCombineAcrossNotProxiedDomains) {
  // Proxy http://kProxyMapDomain/ onto http://kTestDomain/proxied/
  CssCombineFilterTest::SetUp();
  SetHtmlMimetype();

  // Create http://kTestDomain/a.css and http://kProxyMapDomain/b.css
  GoogleString a_local_css_url = StrCat(kTestDomain, kCssA);
  GoogleString b_proxy_css_url = StrCat(kProxyMapDomain, kCssB);
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(a_local_css_url, default_css_header, kACssBody);
  SetFetchResponse(b_proxy_css_url, default_css_header, kBCssBody);

  // Parse html that links to both css files.
  GoogleString html_input(StrCat(
      "<head>\n"
      "  ", Link(a_local_css_url), "\n"
      "  ", Link(b_proxy_css_url), "\n"
      "</head>\n"));

  ParseUrl(StrCat(kTestDomain, "base_url.html"), html_input);

  // The two css files should not be combined since they're not mapped to the
  // same domain.
  StringVector css_urls;
  CollectCssLinks("combine_css", output_buffer_, &css_urls);
  ASSERT_EQ(2UL, css_urls.size());
  EXPECT_STREQ(a_local_css_url, css_urls[0]);
  EXPECT_STREQ(b_proxy_css_url, css_urls[1]);
}

// Make sure that if we re-parse the same html twice we do not
// end up recomputing the CSS (and writing to cache) again
TEST_F(CssCombineFilterTest, CombineCssRecombine) {
  SetHtmlMimetype();
  UseMd5Hasher();
  RequestHeaders request_headers;
  rewrite_driver()->SetRequestHeaders(request_headers);

  CombineCss("combine_css_recombine", "", "", false);
  int inserts_before = lru_cache()->num_inserts();

  CombineCss("combine_css_recombine", "", "", false);
  int inserts_after = lru_cache()->num_inserts();
  EXPECT_EQ(inserts_before, inserts_after);
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}


// https://github.com/apache/incubator-pagespeed-mod/issues/39
TEST_F(CssCombineFilterTest, DealWithParams) {
  SetHtmlMimetype();
  CombineCssWithNames("with_params", "", "", false, "a.css?U", "b.css?rev=138",
                      true);
}

// https://github.com/apache/incubator-pagespeed-mod/issues/252
TEST_F(CssCombineFilterTest, ClaimsXhtmlButHasUnclosedLink) {
  // XHTML text should not have unclosed links.  But if they do, like
  // in Issue 252, then we should leave them alone.
  static const char html_format[] =
      "<head>\n"
      "  %s\n"
      "  %s\n"
      "</head>\n"
      "<body><div class='c1'><div class='c2'><p>\n"
      "  Yellow on Blue</p></div></div></body>";

  GoogleString unclosed_links(StrCat(
      "  ", Link(kCssA), "\n"  // unclosed
      "  <script type='text/javascript' src='c.js'></script>"     // 'in' <link>
      "  ", Link(kCssB)));
  GoogleString combination(StrCat(
      "  ", Link(Encode("", RewriteOptions::kCssCombinerId, "0",
                        MultiUrl(kCssA, kCssB), "css"),
                 "", true),
      "\n"
      "  <script type='text/javascript' src='c.js'></script>  "));

  // Put original CSS files into our fetcher.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, kCssA), default_css_header, ".a {}");
  SetFetchResponse(StrCat(kTestDomain, kCssB), default_css_header, ".b {}");
  ValidateExpected("claims_xhtml_but_has_unclosed_links",
                   StringPrintf(html_format, kXhtmlDtd, unclosed_links.c_str()),
                   StringPrintf(html_format, kXhtmlDtd, combination.c_str()));
}

// http://github.com/apache/incubator-pagespeed-mod/issues/306
TEST_F(CssCombineFilterTest, XhtmlCombineLinkClosed) {
  // XHTML text should not have unclosed links.  But if they do, like
  // in Issue 252, then we should leave them alone.
  static const char html_format[] =
      "<head>\n"
      "  %s\n"
      "  %s\n"
      "</head>\n"
      "<body><div class='c1'><div class='c2'><p>\n"
      "  Yellow on Blue</p></div></div></body>";

  GoogleString links(StrCat(
      Link(kCssA, "screen", true), Link(kCssB, "screen", true)));
  GoogleString combination(
      Link(Encode("", RewriteOptions::kCssCombinerId, "0",
                  MultiUrl(kCssA, kCssB), "css"),
           "screen", true));

  // Put original CSS files into our fetcher.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, kCssA), default_css_header, ".a {}");
  SetFetchResponse(StrCat(kTestDomain, kCssB), default_css_header, ".b {}");
  ValidateExpected("xhtml_combination_closed",
                   StringPrintf(html_format, kXhtmlDtd, links.c_str()),
                   StringPrintf(html_format, kXhtmlDtd, combination.c_str()));
}

TEST_F(CssCombineFilterTest, IEDirectiveBarrier) {
  SetHtmlMimetype();
  GoogleString ie_directive_barrier(StrCat(
      "<!--[if IE]>\n",
      Link("http://graphics8.nytimes.com/css/0.1/screen/build/homepage/ie.css"),
      "\n<![endif]-->"));
  UseMd5Hasher();
  CombineCss("combine_css_ie", ie_directive_barrier, "", true);
}

class CssCombineFilterWithDebugTest : public CssCombineFilterTest {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kDebug);
    CssCombineFilterTest::SetUp();
  }
};

TEST_F(CssCombineFilterWithDebugTest, IEDirectiveBarrier) {
  SetHtmlMimetype();
  GoogleString ie_directive_barrier(StrCat(
      "<!--[if IE]>\n",
      Link("http://graphics8.nytimes.com/css/0.1/screen/build/homepage/ie.css"),
      "\n<![endif]-->"));
  UseMd5Hasher();
  CombineCss("combine_css_ie", ie_directive_barrier,
             "<!--combine_css: Could not combine over barrier: IE directive-->",
             true);
}

TEST_F(CssCombineFilterTest, StyleBarrier) {
  SetHtmlMimetype();
  static const char kStyleBarrier[] = "<style>a { color: red }</style>\n";
  UseMd5Hasher();
  CombineCss("combine_css_style", kStyleBarrier, "", true);
}

TEST_F(CssCombineFilterWithDebugTest, StyleBarrier) {
  SetHtmlMimetype();
  static const char kStyleBarrier[] = "<style>a { color: red }</style>\n";
  UseMd5Hasher();
  CombineCss("combine_css_style", kStyleBarrier,
             "<!--combine_css: Could not combine over barrier: inline style-->",
             true);
}

TEST_F(CssCombineFilterTest, BogusLinkBarrier) {
  SetHtmlMimetype();
  static const char kBogusBarrier[] = "<link rel='stylesheet' "
      "href='crazee://big/blue/fake' type='text/css'>\n";
  UseMd5Hasher();
  CombineCss("combine_css_bogus_link", kBogusBarrier, "",  true);
}

TEST_F(CssCombineFilterWithDebugTest, BogusLinkBarrier) {
  SetHtmlMimetype();
  static const char kBogusBarrier[] = "<link rel='stylesheet' "
      "href='crazee://big/blue/fake' type='text/css'>\n";
  UseMd5Hasher();
  CombineCss("combine_css_bogus_link", kBogusBarrier,
             "<!--combine_css: Could not combine over barrier: "
             "resource not rewritable-->",  true);
}

TEST_F(CssCombineFilterTest, AlternateStylesheetBarrier) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='alternate stylesheet' type='text/css' href='a.css'>";
  UseMd5Hasher();
  CombineCss("alternate_stylesheet_barrier", kBarrier, "", true);
}

TEST_F(CssCombineFilterWithDebugTest, AlternateStylesheetBarrier) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='alternate stylesheet' type='text/css' href='a.css'>";
  UseMd5Hasher();
  CombineCss("alternate_stylesheet_barrier", kBarrier,
             "<!--combine_css: Could not combine over barrier: "
             "custom or alternate stylesheet attribute-->", true);
}

TEST_F(CssCombineFilterTest, NonStandardAttributesBarrier) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='stylesheet' type='text/css' href='a.css' foo='bar'>";
  UseMd5Hasher();
  CombineCss("non_standard_attributes_barrier", kBarrier, "", true);
}

TEST_F(CssCombineFilterWithDebugTest, NonStandardAttributesBarrier) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='stylesheet' type='text/css' href='a.css' foo='bar'>";
  UseMd5Hasher();
  CombineCss("non_standard_attributes_barrier", kBarrier,
             "<!--combine_css: Could not combine over barrier: "
             "potentially non-combinable attribute: &#39;foo&#39;-->", true);
}

TEST_F(CssCombineFilterTest, NonStandardAttributesBarrierWithId) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='stylesheet' type='text/css' href='a.css' foo='bar' id=baz>";
  UseMd5Hasher();
  CombineCss("non_standard_attributes_with_id_barrier", kBarrier, "", true);
}

TEST_F(CssCombineFilterWithDebugTest,
       NonStandardAttributesBarrierWithId) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='stylesheet' type='text/css' href='a.css' foo='bar' id=baz>";
  UseMd5Hasher();
  CombineCss("non_standard_attributes_with_id_barrier", kBarrier,
             "<!--combine_css: Could not combine over barrier: "
             "potentially non-combinable attributes: &#39;foo&#39;"
             " and &#39;id&#39;-->", true);
}

TEST_F(CssCombineFilterTest, NonStandardAttributesBarrierWithAllowedId) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='stylesheet' type='text/css' href='a.css' foo='bar' id=baz>";
  UseMd5Hasher();
  options()->ClearSignatureForTesting();
  options()->AddCssCombiningWildcard("b?z");
  server_context()->ComputeSignature(options());
  CombineCss("non_standard_attributes_with_allowed_id_barrier",
             kBarrier, "", true);
}

TEST_F(CssCombineFilterWithDebugTest,
       NonStandardAttributesBarrierWithAllowedId) {
  SetHtmlMimetype();
  static const char kBarrier[] =
      "<link rel='stylesheet' type='text/css' href='a.css' foo='bar' id=baz>";
  UseMd5Hasher();
  options()->ClearSignatureForTesting();
  options()->AddCssCombiningWildcard("b?z");
  server_context()->ComputeSignature(options());
  CombineCss("non_standard_attributes_with_allowed_id_barrier", kBarrier,
             "<!--combine_css: Could not combine over barrier: "
             "potentially non-combinable attribute: &#39;foo&#39;-->", true);
}

TEST_F(CssCombineFilterTest, IdAloneIsNoBarrier) {
  SetHtmlMimetype();
  static const char kInput[] =
      "<link rel='stylesheet' type='text/css' href='a.css' id=baz>"
      "<link rel='stylesheet' type='text/css' href='b.css'>";
  UseMd5Hasher();
  options()->ClearSignatureForTesting();
  options()->AddCssCombiningWildcard("b?z");
  server_context()->ComputeSignature(options());

  SetupCssResources("a.css", "b.css");

  ParseUrl(StrCat(kDomain, "index.html"), kInput);

  StringVector css_urls;
  CollectCssLinks("id_alone_is_no_barrier", output_buffer_, &css_urls);
  EXPECT_EQ(1UL, css_urls.size());
}

TEST_F(CssCombineFilterTest, CombineCssWithImportInFirst) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", "@Import '1a.css';", "", true);
  css_in.Add("2.css", kYellow, "", true);
  css_in.Add("3.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_import1", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());
}

TEST_F(CssCombineFilterTest, CombineCssWithImportInSecond) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", kYellow, "", true);
  css_in.Add("2.css", "@Import '2a.css';", "", true);
  css_in.Add("3.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_import1", css_in, &css_out);
  EXPECT_EQ("1.css", css_out[0]->url_);
  EXPECT_EQ(2, css_out.size());
}

TEST_F(CssCombineFilterTest, StripBom) {
  GoogleString html_url = StrCat(kDomain, "bom.html");
  GoogleString a_css_url = StrCat(kDomain, kCssA);
  GoogleString b_css_url = StrCat(kDomain, kCssB);

  // BOM documentation: http://www.unicode.org/faq/utf_bom.html
  GoogleString bom_body = StrCat(kUtf8Bom, kBCssBody);

  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_header);

  SetFetchResponse(a_css_url, default_header, kACssBody);
  SetFetchResponse(b_css_url, default_header, bom_body);

  StringVector css_urls;
  GoogleString input_buffer(StrCat(
      "<head>\n"
      "  ", Link(kCssA), "\n"
      "  ", Link(kCssB), "\n"
      "</head>\n"));
  ParseUrl(html_url, input_buffer);

  CollectCssLinks("combine_css_no_bom", output_buffer_, &css_urls);
  ASSERT_EQ(1UL, css_urls.size());
  GoogleString actual_combination;
  GoogleUrl base_url(html_url);
  GoogleUrl css_url(base_url, css_urls[0]);
  EXPECT_TRUE(FetchResourceUrl(css_url.Spec(), &actual_combination));
  int bom_pos = actual_combination.find(kUtf8Bom);
  EXPECT_EQ(GoogleString::npos, bom_pos);

  GoogleString input_buffer_reversed(StrCat(
      "<head>\n"
      "  ", Link(kCssB), "\n"
      "  ", Link(kCssA), "\n"
      "</head>\n"));
  ParseUrl(html_url, input_buffer_reversed);
  css_urls.clear();
  actual_combination.clear();
  CollectCssLinks("combine_css_beginning_bom", output_buffer_, &css_urls);
  ASSERT_EQ(1UL, css_urls.size());
  css_url.Reset(base_url, css_urls[0]);
  EXPECT_TRUE(FetchResourceUrl(css_url.Spec(), &actual_combination));
  bom_pos = actual_combination.find(kUtf8Bom);
  EXPECT_EQ(0, bom_pos);
  bom_pos = actual_combination.rfind(kUtf8Bom);
  EXPECT_EQ(0, bom_pos);
}

TEST_F(CssCombineFilterTest, StripBomReconstruct) {
  // Make sure we strip the BOM properly when reconstructing, too.
  static const char kCssText[] = "div {background-image:url(fancy.png);}";
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                StrCat(kUtf8Bom, kCssText),
                                300);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                StrCat(kUtf8Bom, kCssText),
                                300);
  GoogleString css_url =
      Encode(kTestDomain, RewriteOptions::kCssCombinerId, "0",
             MultiUrl(kCssA, kCssB), "css");
  GoogleString css_out;
  EXPECT_TRUE(FetchResourceUrl(css_url, &css_out));
  EXPECT_EQ(StrCat(kUtf8Bom, kCssText, "\n", kCssText), css_out);
}

TEST_F(CssCombineFilterTest, CombineCssWithNoscriptBarrier) {
  SetHtmlMimetype();
  static const char kNoscriptBarrier[] =
      "<noscript>\n"
      "  <link rel='stylesheet' href='d.css' type='text/css'>\n"
      "</noscript>\n";

  // Put this in the Test class to remove repetition here and below.
  GoogleString d_css_url = StrCat(kDomain, "d.css");
  static const char kDCssBody[] = ".c4 {\n color: green;\n}\n";
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(d_css_url, default_css_header, kDCssBody);

  UseMd5Hasher();
  CombineCss("combine_css_noscript", kNoscriptBarrier, "", true);
}

TEST_F(CssCombineFilterTest, CombineCssWithFakeNoscriptBarrier) {
  SetHtmlMimetype();
  static const char kNonBarrier[] =
      "<noscript>\n"
      "  <p>You have no scripts installed</p>\n"
      "</noscript>\n";
  UseMd5Hasher();
  CombineCss("combine_css_fake_noscript", kNonBarrier, "", false);
}

TEST_F(CssCombineFilterTest, CombineCssWithMediaBarrier) {
  SetHtmlMimetype();
  static const char kMediaBarrier[] =
      "<link rel='stylesheet' href='d.css' type='text/css' media='print'>\n";

  GoogleString d_css_url = StrCat(kDomain, "d.css");
  static const char kDCssBody[] = ".c4 {\n color: green;\n}\n";
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(d_css_url, default_css_header, kDCssBody);

  UseMd5Hasher();
  CombineCss("combine_css_media", kMediaBarrier, "", true);
}

TEST_F(CssCombineFilterTest, CombineCssWithNonMediaBarrier) {
  SetHtmlMimetype();

  // Put original CSS files into our fetcher.
  GoogleString html_url = StrCat(kDomain, "no_media_barrier.html");
  GoogleString a_css_url = StrCat(kDomain, kCssA);
  GoogleString b_css_url = StrCat(kDomain, kCssB);
  GoogleString c_css_url = StrCat(kDomain, "c.css");
  GoogleString d_css_url = StrCat(kDomain, "d.css");

  static const char kDCssBody[] = ".c4 {\n color: green;\n}\n";

  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(a_css_url, default_css_header, kACssBody);
  SetFetchResponse(b_css_url, default_css_header, kBCssBody);
  SetFetchResponse(c_css_url, default_css_header, kCCssBody);
  SetFetchResponse(d_css_url, default_css_header, kDCssBody);

  // Only the first two CSS files should be combined.
  GoogleString html_input(StrCat(
      "<head>\n"
      "  ", Link(kCssA, "print", false), "\n"
      "  ", Link(kCssB, "print", false), "\n"));
  StrAppend(&html_input,
      "  ", Link("c.css"), "\n"
      "  ", Link("d.css", "print", false), "\n"
      "</head>");

  // Rewrite
  ParseUrl(html_url, html_input);

  // Check for CSS files in the rewritten page.
  StringVector css_urls;
  CollectCssLinks("combine_css_no_media-links", output_buffer_, &css_urls);
  EXPECT_EQ(3UL, css_urls.size());
  const GoogleString& combine_url = css_urls[0];

  GoogleString expected_output(StrCat(
      "<head>\n"
      "  ", Link(combine_url, "print", false), "\n"
      "  \n"
      "  ", Link("c.css"), "\n"
      "  ", Link("d.css", "print", false), "\n"
      "</head>"));
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
}

// This test, as rewritten as of March 2011, is testing an invalid HTML
// construct, where no hrefs should precede a base tag.  The current expected
// behavior is that we leave any urls before the base tag alone, and then try
// to combine urls after the base tag.  Since this test has only one css after
// the base tag, it should leave that one alone.
TEST_F(CssCombineFilterTest, NoCombineCssBaseUrlOutOfOrder) {
  SetHtmlMimetype();
  StringVector css_urls;
  GoogleString input_buffer(StrCat(
      "<head>\n"
      "  ", Link(kCssA), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(kCssB), "\n"
      "</head>\n"));
  CombineWithBaseTag(input_buffer, &css_urls);
  EXPECT_EQ(2UL, css_urls.size());
  EXPECT_EQ(AddHtmlBody(input_buffer), output_buffer_);
}

// Same invalid configuration, but now with two css refs after the base tag,
// which should get combined.
TEST_F(CssCombineFilterTest, CombineCssBaseUrlOutOfOrder) {
  SetHtmlMimetype();
  StringVector css_urls;
  GoogleString input_buffer(StrCat(
      "<head>\n"
      "  ", Link(kCssA), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(kCssB), "\n"
      "  ", Link("c.css"), "\n"
      "</head>\n"));
  CombineWithBaseTag(input_buffer, &css_urls);

  GoogleString expected_output(StrCat(
      "<head>\n"
      "  ", Link(kCssA), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(css_urls[1]), "\n"
      "  \n"
      "</head>\n"));
  EXPECT_EQ(2UL, css_urls.size());
  // Note: Combined css_urls[1] is still relative, just like the original URLs.
  GoogleString combine_url =
      StrCat("http://other_domain.test/foo/", css_urls[1]);
  EXPECT_EQ(EncodeWithBase("http://other_domain.test/",
                           "http://other_domain.test/foo/",
                           RewriteOptions::kCssCombinerId, "0",
                           MultiUrl(kCssB, "c.css"), "css"),
            combine_url);
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_TRUE(GoogleUrl(combine_url).IsWebValid());
}

// Same invalid configuration, but now with a full qualified url before
// the base tag.  We should be able to find and combine that one.
TEST_F(CssCombineFilterTest, CombineCssAbsoluteBaseUrlOutOfOrder) {
  SetHtmlMimetype();
  StringVector css_urls;
  GoogleString input_buffer(StrCat(
      "<head>\n"
      "  ", Link("http://other_domain.test/foo/a.css"), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(kCssB), "\n"
      "</head>\n"));
  CombineWithBaseTag(input_buffer, &css_urls);

  GoogleString expected_output(StrCat(
      "<head>\n"
      "  ", Link(css_urls[0]), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  \n"
      "</head>\n"));
  EXPECT_EQ(1UL, css_urls.size());
  EXPECT_EQ(EncodeWithBase("http://other_domain.test/",
                           "http://other_domain.test/foo/",
                           RewriteOptions::kCssCombinerId, "0",
                           MultiUrl(kCssA, kCssB), "css"),
            // Note: Combined css_urls[0] is absolute because the first original
            // URL was absolute, even though the next URL is relative.
            css_urls[0]);
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_TRUE(GoogleUrl(css_urls[0]).IsWebValid());
}

// Here's the same test as NoCombineCssBaseUrlOutOfOrder, legalized to have
// the base url before the first link.
TEST_F(CssCombineFilterTest, CombineCssBaseUrlCorrectlyOrdered) {
  SetHtmlMimetype();
  // <base> tag correctly precedes any urls.
  StringVector css_urls;
  CombineWithBaseTag(StrCat(
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(kCssA), "\n"
      "  ", Link(kCssB), "\n"
      "</head>\n"), &css_urls);

  GoogleString expected_output(StrCat(
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(css_urls[0]), "\n"
      "  \n"
      "</head>\n"));
  EXPECT_EQ(1UL, css_urls.size());
  // Note: Combined css_urls[0] is still relative, just like the original URLs.
  GoogleString combine_url =
      StrCat("http://other_domain.test/foo/", css_urls[0]);
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_EQ(EncodeWithBase("http://other_domain.test/",
                           "http://other_domain.test/foo/",
                           RewriteOptions::kCssCombinerId, "0",
                           MultiUrl(kCssA, kCssB), "css"),
            combine_url);
  EXPECT_TRUE(GoogleUrl(combine_url).IsWebValid());
}

TEST_F(CssCombineFilterTest, CombineCssNoInput) {
  SetFetchFailOnUnexpected(false);
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, kCssB),
                   default_css_header, ".a {}");
  static const char kHtmlInput[] =
      "<head>\n"
      "  <link rel='stylesheet' href='a_broken.css' type='text/css'>\n"
      "  <link rel='stylesheet' href='b.css' type='text/css'>\n"
      "</head>\n"
      "<body><div class='c1'><div class='c2'><p>\n"
      "  Yellow on Blue</p></div></div></body>";
  ValidateNoChanges("combine_css_missing_input", kHtmlInput);
}

TEST_F(CssCombineFilterTest, CombineCssXhtml) {
  TestXhtml(false);
}

TEST_F(CssCombineFilterTest, CombineCssXhtmlWithFlush) {
  TestXhtml(true);
}

TEST_F(CssCombineFilterTest, CombineCssMissingResource) {
  CssCombineMissingResource();
}

TEST_F(CssCombineFilterTest, CombineCssManyFiles) {
  // Prepare an HTML fragment with too many CSS files to combine,
  // exceeding the char limit.
  //
  // It looks like we can fit a limited number of encodings of
  // "yellow%d.css" in the buffer.  It might be more general to base
  // this on the constant declared in RewriteOptions but I think it's
  // easier to understand leaving these exposed as constants; we can
  // abstract them later.
  const int kNumCssLinks = 100;
  // Note: Without CssCombine::Partnership::kUrlSlack this was:
  // const int kNumCssInCombination = 18
  const int kNumCssInCombination = 70;  // based on how we encode "yellow%d.css"
  CssLink::Vector css_in, css_out;
  for (int i = 0; i < kNumCssLinks; ++i) {
    css_in.Add(StringPrintf("styles/yellow%d.css", i),
               kYellow, "", true);
  }
  BarrierTestHelper("combine_css_many_files", css_in, &css_out);
  ASSERT_EQ(2, css_out.size());

  // Check that the first element is really a combination.
  GoogleString base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(kTestDomain, &base, &segments,
                                               &message_handler_))
      << css_out[0]->url_;
  GoogleUrl dummy_encoded(Encode(StrCat(kTestDomain, "styles/"), "x", "0",
                                 "x", "x"));
  EXPECT_EQ(dummy_encoded.AllExceptLeaf(), base);
  EXPECT_EQ(kNumCssInCombination, segments.size());

  segments.clear();
  ASSERT_TRUE(css_out[1]->DecomposeCombinedUrl(kTestDomain, &base, &segments,
                                               &message_handler_))
      << css_out[1]->url_;
  EXPECT_EQ(dummy_encoded.AllExceptLeaf(), base);
  EXPECT_EQ(kNumCssLinks - kNumCssInCombination, segments.size());
}

TEST_F(CssCombineFilterTest, CombineCssManyFilesOneOrphan) {
  // This test differs from the previous test in we have exactly one CSS file
  // that stays on its own.
  // Note: Without CssCombine::Partnership::kUrlSlack this was:
  // const int kNumCssInCombination = 18
  const int kNumCssInCombination = 70;  // based on how we encode "yellow%d.css"
  const int kNumCssLinks = kNumCssInCombination + 1;
  CssLink::Vector css_in, css_out;
  for (int i = 0; i < kNumCssLinks - 1; ++i) {
    css_in.Add(StringPrintf("styles/yellow%d.css", i),
               kYellow, "", true);
  }
  css_in.Add("styles/last_one.css",
             kYellow, "", true);
  BarrierTestHelper("combine_css_many_files", css_in, &css_out);
  ASSERT_EQ(2, css_out.size());

  // Check that the first element is really a combination.
  GoogleString base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(kTestDomain, &base, &segments,
                                               &message_handler_))
      << css_out[0]->url_;
  GoogleUrl dummy_encoded(Encode(StrCat(kTestDomain, "styles/"), "x", "0",
                                 "x", "x"));
  EXPECT_EQ(dummy_encoded.AllExceptLeaf(), base);
  EXPECT_EQ(kNumCssInCombination, segments.size());
  EXPECT_EQ("styles/last_one.css", css_out[1]->url_);
}

// Note -- this test is redundant with CombineCssMissingResource -- this
// is a taste test.  This new mechanism is more code per test but I think
// the failures are more obvious and the expect/assert tests are in the
// top level of the test which might make it easier to debug.
TEST_F(CssCombineFilterTest, CombineCssNotCached) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", kYellow, "", true);
  css_in.Add("2.css", kYellow, "", true);
  css_in.Add("3.css", kYellow, "", false);
  css_in.Add("4.css", kYellow, "", true);
  SetFetchFailOnUnexpected(false);
  BarrierTestHelper("combine_css_not_cached", css_in, &css_out);
  EXPECT_EQ(3, css_out.size());
  GoogleString base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(kTestDomain, &base, &segments,
                                               &message_handler_))
      << css_out[0]->url_;
  EXPECT_EQ(2, segments.size());
  EXPECT_EQ("1.css", segments[0]);
  EXPECT_EQ("2.css", segments[1]);
  EXPECT_EQ("3.css", css_out[1]->url_);
  EXPECT_EQ("4.css", css_out[2]->url_);
}

// Note -- this test is redundant with CombineCssWithIEDirective -- this
// is a taste test.
TEST_F(CssCombineFilterTest, CombineStyleTag) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", kYellow, "", true);
  css_in.Add("2.css", kYellow, "", true);
  css_in.Add("", "<style>a { color: red }</style>\n", "", false);
  css_in.Add("4.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(2, css_out.size());
  GoogleString base;
  StringVector segments;
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(kTestDomain, &base, &segments,
                                               &message_handler_))
      << css_out[0]->url_;
  ASSERT_EQ(2, segments.size());
  EXPECT_EQ("1.css", segments[0]);
  EXPECT_EQ("2.css", segments[1]);
  EXPECT_EQ("4.css", css_out[1]->url_);
}

TEST_F(CssCombineFilterTest, NoAbsolutifySameDir) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", ".yellow {background-image: url('1.png');}\n", "", true);
  css_in.Add("2.css", ".yellow {background-image: url('2.png');}\n", "", true);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());

  // Note: the urls are not absolutified.
  GoogleString expected_combination =
      ".yellow {background-image: url('1.png');}\n"
      ".yellow {background-image: url('2.png');}\n";

  // Check fetched resource.
  GoogleString actual_combination;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, css_out[0]->url_),
                               &actual_combination));
  // TODO(sligocki): Check headers?
  EXPECT_EQ(expected_combination, actual_combination);
}

TEST_F(CssCombineFilterTest, DoRewriteForDifferentDir) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", ".yellow {background-image: url('1.png');}\n", "", true);
  css_in.Add("foo/2.css", ".yellow {background-image: url('2.png');}\n",
             "", true);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());

  GoogleString expected_combination =
      ".yellow {background-image: url('1.png');}\n"
      ".yellow {background-image: url('foo/2.png');}\n";

  // Check fetched resource.
  GoogleString actual_combination;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, css_out[0]->url_),
                               &actual_combination));
  // TODO(sligocki): Check headers?
  EXPECT_EQ(expected_combination, actual_combination);
}

TEST_F(CssCombineFilterTest, ShardSubresources) {
  UseMd5Hasher();
  AddShard(kTestDomain, "shard1.com,shard2.com");

  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", ".yellow {background-image: url('1.png');}\n", "", true);
  css_in.Add("2.css", ".yellow {background-image: url('2.png');}\n", "", true);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());

  // Note: the urls are sharded to absolute domains.
  GoogleString expected_combination =
      ".yellow {background-image: url('http://shard1.com/1.png');}\n"
      ".yellow {background-image: url('http://shard2.com/2.png');}\n";

  // Check fetched resource.
  GoogleString actual_combination;
  EXPECT_TRUE(FetchResourceUrl(css_out[0]->url_, &actual_combination));
  EXPECT_EQ(expected_combination, actual_combination);
}

// Verifies that we don't produce URLs that are too long in a corner case.
TEST_F(CssCombineFilterTest, CrossAcrossPathsExceedingUrlSize) {
  CssLink::Vector css_in, css_out;
  GoogleString long_name(600, 'z');
  css_in.Add(long_name + "/a.css", kYellow, "", true);
  css_in.Add(long_name + "/b.css", kBlue, "", true);

  // This last 'Add' causes the resolved path to change from long_path to "/".
  // Which makes the encoding way too long. So we expect this URL not to be
  // added to the combination and for the combination base to remain long_path.
  css_in.Add("sites/all/modules/ckeditor/ckeditor.css?3", "z", "", true);
  BarrierTestHelper("cross_paths", css_in, &css_out);
  EXPECT_EQ(2, css_out.size());
  GoogleString actual_combination;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, css_out[0]->url_),
                               &actual_combination));
  GoogleUrl base_url(kTestDomain);
  GoogleUrl gurl(base_url, css_out[0]->url_);
  ASSERT_TRUE(gurl.IsWebValid());
  GoogleUrl dummy_encoded(Encode(StrCat(kTestDomain, long_name, "/"), "x", "0",
                                 "x", "x"));
  EXPECT_EQ(dummy_encoded.PathSansLeaf(), gurl.PathSansLeaf());
  ResourceNamer namer;
  ASSERT_TRUE(rewrite_driver()->Decode(gurl.LeafWithQuery(), &namer));
  EXPECT_EQ("a.css+b.css", namer.name());
  EXPECT_EQ(StrCat(kYellow, "\n", kBlue), actual_combination);
}

// Verifies that we don't allow path-crossing URLs if that option is turned off.
TEST_F(CssCombineFilterTest, CrossAcrossPathsDisallowed) {
  options()->ClearSignatureForTesting();
  options()->set_combine_across_paths(false);
  server_context()->ComputeSignature(options());
  CssLink::Vector css_in, css_out;
  css_in.Add("a/a.css", kYellow, "", true);
  css_in.Add("b/b.css", kBlue, "", true);
  BarrierTestHelper("cross_paths", css_in, &css_out);
  ASSERT_EQ(2, css_out.size());
  EXPECT_EQ("a/a.css", css_out[0]->url_);
  EXPECT_EQ("b/b.css", css_out[1]->url_);
}

TEST_F(CssCombineFilterTest, CrossMappedDomain) {
  CssLink::Vector css_in, css_out;
  AddRewriteDomainMapping("a.com", "b.com");
  bool supply_mock = false;
  css_in.Add("http://a.com/1.css", kYellow, "", supply_mock);
  css_in.Add("http://b.com/2.css", kBlue, "", supply_mock);
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse("http://a.com/1.css", default_css_header, kYellow);
  SetFetchResponse("http://b.com/2.css", default_css_header, kBlue);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());
  GoogleString actual_combination;
  EXPECT_TRUE(FetchResourceUrl(css_out[0]->url_, &actual_combination));
  EXPECT_EQ(Encode("http://a.com/", RewriteOptions::kCssCombinerId, "0",
                   MultiUrl("1.css", "2.css"), "css"),
            css_out[0]->url_);
  EXPECT_EQ(StrCat(kYellow, "\n", kBlue), actual_combination);
}

// Verifies that we cannot do the same cross-domain combo when we lack
// the domain mapping.
TEST_F(CssCombineFilterTest, CrossUnmappedDomain) {
  VerifyCrossUnmappedDomainNotRewritten();
}

// Verifies the same but we check the debug message.
TEST_F(CssCombineFilterWithDebugTest, DebugUnauthorizedDomain) {
  VerifyCrossUnmappedDomainNotRewritten();
  EXPECT_HAS_SUBSTR("<html>\n"
                      "<head>\n"
                      "  <link rel=\"stylesheet\" type=\"text/css\""
                      " href=\"http://a.com/1.css\">\n"
                      "  <link "
                      "rel=\"stylesheet\" type=\"text/css\""
                      " href=\"http://b.com/2.css\">\n"
                      "</head>\n"
                      "<body>\n"
                      "  <div class='yellow'>\n"
                      "    Hello, mod_pagespeed!\n"
                      "  </div>\n"
                      "</body>\n"
                      "</html>"
                      "<!--", output_buffer_);
  EXPECT_HAS_SUBSTR(
      StrCat(DebugFilter::FormatEndDocumentMessage(0, 0, 0, 0, 0, false,
                                                   StringSet(), StringVector()),
             "-->"),
      output_buffer_);
}

// Verifies that we cannot do the same cross-domain combo when we lack
// the domain mapping even if inline_unauthorized_resources is enabled.
TEST_F(CssCombineFilterTest, CrossUnmappedDomainWithUnauthEnabled) {
  options()->ClearSignatureForTesting();
  options()->AddInlineUnauthorizedResourceType(semantic_type::kStylesheet);
  server_context()->ComputeSignature(options());
  VerifyCrossUnmappedDomainNotRewritten();
}

// Make sure bad requests do not corrupt our extension.
TEST_F(CssCombineFilterTest, NoExtensionCorruption) {
  TestCorruptUrl(".css%22");
}

TEST_F(CssCombineFilterTest, NoQueryCorruption) {
  TestCorruptUrl(".css?query");
}

TEST_F(CssCombineFilterTest, NoWrongExtCorruption) {
  TestCorruptUrl(".html");
}

TEST_F(CssCombineFilterTest, TwoCombinationsTwice) {
  // Regression test for a case where we were picking up some
  // partial cache results for sync path even in async path, and hence
  // got confused and CHECK-failed.

  CssLink::Vector input_css_links, output_css_links;
  SetFetchResponse404("404.css");
  input_css_links.Add(kCssA, kYellow, "", true /* supply_mock*/);
  input_css_links.Add(kCssB, kYellow, "", true /* supply_mock*/);
  input_css_links.Add("404.css", kYellow, "", false /* supply_mock*/);
  input_css_links.Add("c.css", kYellow, "", true /* supply_mock*/);
  input_css_links.Add("d.css", kYellow, "", true /* supply_mock*/);

  BarrierTestHelper("two_comb", input_css_links, &output_css_links);

  ASSERT_EQ(3, output_css_links.size());
  EXPECT_EQ(Encode("", RewriteOptions::kCssCombinerId, "0",
                   MultiUrl(kCssA, kCssB), "css"),
            output_css_links[0]->url_);
  EXPECT_EQ("404.css", output_css_links[1]->url_);
  EXPECT_EQ(Encode("", RewriteOptions::kCssCombinerId, "0",
                   MultiUrl("c.css", "d.css"), "css"),
            output_css_links[2]->url_);

  // Get rid of the "modern" cache key, while keeping the old one.
  lru_cache()->Delete(HttpCacheKey(
      ",htest.com,_a.css+,htest.com,_b.css+,htest.com,_404.css+"
      ",htest.com,_c.css+,htest.com,_d.css:cc"));

  // Now do it again...
  BarrierTestHelper("two_comb", input_css_links, &output_css_links);
}

TEST_F(CssCombineFilterTest, InvalidFetchCache) {
  // Regression test for crashes when we're asked to do an invalid
  // fetch and then repeat it for a rewriter inside an XHTML-DTD page.
  SetFetchResponse404("404a.css");
  SetFetchResponse404("404b.css");

  EXPECT_FALSE(TryFetchResource(
      Encode(kTestDomain, RewriteOptions::kCssCombinerId, "0",
             MultiUrl("404a.css", "404b.css"), "css")));
  ValidateNoChanges("invalid",
                    StrCat(kXhtmlDtd,
                           CssLinkHref("404a.css"),
                           CssLinkHref("404b.css")));
}

TEST_F(CssCombineFilterTest, NoCombineParseErrors) {
  // Notice: This CSS file does not close its { and thus will break the
  // next stylesheet if they are combined, changing the page.
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                "h1 { color: red", 100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                "h2 { color: blue; }", 100);

  ValidateNoChanges("bad_parse", StrCat(CssLinkHref(kCssA),
                                        CssLinkHref(kCssB)));
}

TEST_F(CssCombineFilterTest, NoCombineParseErrorsAtRule) {
  // Notice: This CSS file does not close its { and thus will break the
  // next stylesheet if they are combined, changing the page.
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                "@foobar { color: red", 100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                "h2 { color: blue; }", 100);

  ValidateNoChanges("bad_parse", StrCat(CssLinkHref(kCssA),
                                        CssLinkHref(kCssB)));
}

TEST_F(CssCombineFilterTest, NoCombineParseErrorsUnclosedComment) {
  // Notice: This CSS file does not close its /* and thus would break the
  // next stylesheet if they were combined.
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                "h1 { color: red; } /* ", 100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                "h2 { color: blue; }", 100);

  ValidateNoChanges("bad_parse", StrCat(CssLinkHref(kCssA),
                                        CssLinkHref(kCssB)));
}

TEST_F(CssCombineFilterTest, NoCombineParseErrorUnclosedImport) {
  // Notice: This CSS file does not close its @import (with a ;)
  // and thus would break the next stylesheet if they were combined.
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                "@import url(b.css)", 100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                "h2 { color: blue; }", 100);

  ValidateNoChanges("bad_parse", StrCat(CssLinkHref(kCssA),
                                        CssLinkHref(kCssB)));
}

TEST_F(CssCombineFilterTest, RobustnessUnclosedString) {
  // Make sure we are robust against something with unterminated string.
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                "q::before {padding: 0px; \"content: foo;}",
                                100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                "h2 { color: blue; }", 100);

  GoogleString combined_url =
      Encode("", RewriteOptions::kCssCombinerId, "0",
             MultiUrl(kCssA, kCssB), "css");

  ValidateNoChanges("unterm_str",
                    StrCat(CssLinkHref(kCssA), CssLinkHref(kCssB)));
}

// See: http://www.alistapart.com/articles/alternate/
//  and http://www.w3.org/TR/html4/present/styles.html#h-14.3.1
TEST_F(CssCombineFilterTest, AlternateStylesheets) {
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                "h1 { color: red; }", 100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                "h2 { color: blue; }", 100);

  // Normal (persistent) CSS links are combined.
  ValidateExpected(
      "persistent",
      "<link rel='stylesheet' href='a.css'>"
      "<link rel='stylesheet' href='b.css'>",
      StringPrintf("<link rel='stylesheet' href='%s'/>", Encode(
          "", RewriteOptions::kCssCombinerId, "0",
          MultiUrl(kCssA, kCssB), "css").c_str()));

  // Make sure we accept mixed case for the keyword.
  ValidateExpected(
      "mixed_case",
      "<link rel=' StyleSheet' href='a.css'>"
      "<link rel='styleSHEET  ' href='b.css'>",
      StringPrintf("<link rel=' StyleSheet' href='%s'/>", Encode(
          "", RewriteOptions::kCssCombinerId, "0",
          MultiUrl(kCssA, kCssB), "css").c_str()));

  // Preferred CSS links are not because we don't want to combine styles with
  // different titles.
  ValidateNoChanges(
      "preferred_different",
      "<link rel='stylesheet' href='a.css' title='foo'>"
      "<link rel='stylesheet' href='b.css' title='bar'>");

  // TODO(sligocki): Should we combine ones with the same title?
  ValidateNoChanges(
      "preferred_same",
      "<link rel='stylesheet' href='a.css' title='foo'>"
      "<link rel='stylesheet' href='b.css' title='foo'>");

  // Alternate CSS links, likewise.
  ValidateNoChanges(
      "alternate_different",
      "<link rel='alternate stylesheet' href='a.css' title='foo'>"
      "<link rel='alternate stylesheet' href='b.css' title='bar'>");

  // TODO(sligocki): Should we combine ones with the same title?
  ValidateNoChanges(
      "alternate_same",
      "<link rel='alternate stylesheet' href='a.css' title='foo'>"
      "<link rel='alternate stylesheet' href='b.css' title='foo'>");
}

TEST_F(CssCombineFilterTest, Stats) {
  Parse("stats_no_link", "");
  // 0 opportunities with 0 links.
  EXPECT_EQ(0, css_combine_opportunities_->Get());
  EXPECT_EQ(0, css_file_count_reduction_->Get());
  ClearStats();

  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                ".a { color: red; }", 100);
  Parse("stats_one_link", Link(kCssA));
  // 0 opportunities with 1 link.
  EXPECT_EQ(0, css_combine_opportunities_->Get());
  EXPECT_EQ(0, css_file_count_reduction_->Get());
  ClearStats();

  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                ".b { color: green; }", 100);
  SetResponseWithDefaultHeaders("c.css", kContentTypeCss,
                                ".c { color: blue; }", 100);
  Parse("stats_3_links", StrCat(Link(kCssA), Link(kCssB), Link("c.css")));
  // 2 opportunity with 3 links.
  EXPECT_EQ(2, css_combine_opportunities_->Get());
  EXPECT_EQ(2, css_file_count_reduction_->Get());
  ClearStats();

  Parse("stats_partial", StrCat(Link(kCssA), Link(kCssB),
                                // media="print" so that it can't be combined.
                                Link("c.css", "print", false)));
  // 2 opportunities, but only one reduction because last is not combinable.
  EXPECT_EQ(2, css_combine_opportunities_->Get());
  EXPECT_EQ(1, css_file_count_reduction_->Get());
  ClearStats();
}

TEST_F(CssCombineFilterTest, StatsWithDelay) {
  SetupWaitFetcher();

  // Setup CSS files.
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                ".a { color: red; }", 100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                ".b { color: green; }", 100);
  SetResponseWithDefaultHeaders("c.css", kContentTypeCss,
                                ".c { color: blue; }", 100);

  GoogleString input_html = StrCat(Link(kCssA), Link(kCssB), Link("c.css"));

  // We have a wait fetcher, so this won't be rewritten on the first run.
  Parse("stats1", input_html);
  // All opportunities for combining will be missed.
  EXPECT_EQ(2, css_combine_opportunities_->Get());
  EXPECT_EQ(0, css_file_count_reduction_->Get());
  ClearStats();

  // Calling callbacks will cause async rewrite to happen, but will not
  // affect stats (which measure actual HTML usage).
  CallFetcherCallbacks();
  EXPECT_EQ(0, css_combine_opportunities_->Get());
  EXPECT_EQ(0, css_file_count_reduction_->Get());
  ClearStats();

  // This time result is rewritten.
  Parse("stats2", input_html);
  // All opportunities for combining will be taken.
  EXPECT_EQ(2, css_combine_opportunities_->Get());
  EXPECT_EQ(2, css_file_count_reduction_->Get());
  ClearStats();

  // Nothing happens here.
  CallFetcherCallbacks();
  EXPECT_EQ(0, css_combine_opportunities_->Get());
  EXPECT_EQ(0, css_file_count_reduction_->Get());
  ClearStats();

  // Same as second load.
  Parse("stats3", input_html);
  EXPECT_EQ(2, css_combine_opportunities_->Get());
  EXPECT_EQ(2, css_file_count_reduction_->Get());
  ClearStats();
}

class CssCombineAndCacheExtendTest : public CssCombineFilterTest {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kExtendCacheCss);
    CssCombineFilterTest::SetUp();
  }
};

TEST_F(CssCombineAndCacheExtendTest, CombineCssNoExtraCacheExtension) {
  SetHtmlMimetype();
  SetResponseWithDefaultHeaders(kCssA, kContentTypeJavascript, kYellow, 100);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeJavascript, kBlue, 100);
  GoogleString combined_url =
      Encode("", RewriteOptions::kCssCombinerId, "0",
             MultiUrl(kCssA, kCssB), "css");

  ValidateExpected("combine",
                   StrCat(CssLinkHref(kCssA), CssLinkHref(kCssB)),
                   CssLinkHref(combined_url));
  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());

  // Now try cached.
  ValidateExpected("combine",
                   StrCat(CssLinkHref(kCssA), CssLinkHref(kCssB)),
                   CssLinkHref(combined_url));
  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());
}

class CssFilterWithCombineTest : public CssCombineFilterTest {
 protected:
  virtual void SetUp() {
    // We setup the options before the upcall so that the
    // CSS filter is created aware of these.
    options()->EnableFilter(RewriteOptions::kRewriteCss);
    CssCombineFilterTest::SetUp();
  }

  GoogleString CssOut() {
    return EncodeCssCombineAndOptimize("", MultiUrl(kCssA, kCssB));
  }

  GoogleString OptimizedContent() { return "div{}div{}"; }

  void SetupResources() {
    static const char kCssText[] = " div {    } ";
    SetResponseWithDefaultHeaders(kCssA, kContentTypeCss, kCssText, 300);
    SetResponseWithDefaultHeaders(kCssB, kContentTypeCss, kCssText, 300);
  }
};

// See TestFollowCombine below: change one, change them both!
TEST_F(CssFilterWithCombineTest, TestFollowCombine) {
  SetHtmlMimetype();
  SetupResources();

  // Make sure we don't regress dealing with combiner deleting things sanely
  // in rewrite filter.
  ValidateExpected(
      "follow_combine", StrCat(Link(kCssA), Link(kCssB)), Link(CssOut()));

  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, CssOut()), &content));
  EXPECT_STREQ(OptimizedContent(), content);
}

TEST_F(CssFilterWithCombineTest, FetchCombinedMinifiedWithGzip) {
  SetHtmlMimetype();
  SetupResources();

  // TODO(jcrowell): The test here shows suboptimial behavior.  We have computed
  // the gzip -9 response and put it in the cache.  However, we have then
  // uncompressed that response and returned it to the client, even though the
  // client wants gzip.  In Apache, mod_deflate will then gzip the response
  // again which (a) wastes CPU time on the superfluous decode and re-encode
  // and (b) delivers a less-than-optimal response, which might be cached by
  // proxies (!).
  GoogleString content;
  ResponseHeaders response_headers;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, CssOut()), &content,
                               &response_headers));
  EXPECT_STREQ(OptimizedContent(), content) << "uncached";
  EXPECT_FALSE(WasGzipped(response_headers));

  response_headers.Clear();
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, CssOut()), &content,
                               &response_headers));
  EXPECT_STREQ(OptimizedContent(), content) << "cached";
  EXPECT_TRUE(WasGzipped(response_headers));
}

TEST_F(CssFilterWithCombineTest, FetchCombinedMinifiedWithoutGzip) {
  DisableGzip();
  SetHtmlMimetype();
  SetupResources();

  GoogleString content;
  ResponseHeaders response_headers;
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, CssOut()), &content,
                               &response_headers));
  EXPECT_STREQ(OptimizedContent(), content) << "uncached";
  EXPECT_FALSE(WasGzipped(response_headers));

  response_headers.Clear();
  EXPECT_TRUE(FetchResourceUrl(StrCat(kTestDomain, CssOut()), &content,
                               &response_headers));
  EXPECT_STREQ(OptimizedContent(), content) << "cached";
  EXPECT_FALSE(WasGzipped(response_headers));
}

class CssFilterWithCombineTestUrlNamer : public CssFilterWithCombineTest {
 public:
  CssFilterWithCombineTestUrlNamer() {
    SetUseTestUrlNamer(true);
  }
};

// See TestFollowCombine above: change one, change them both!
TEST_F(CssFilterWithCombineTestUrlNamer, TestFollowCombine) {
  SetHtmlMimetype();

  // Check that we really are using TestUrlNamer and not UrlNamer.
  EXPECT_NE(Encode(kTestDomain, RewriteOptions::kCssCombinerId, "0",
                   kCssA, "css"),
            EncodeNormal(kTestDomain, RewriteOptions::kCssCombinerId, "0",
                         kCssA, "css"));

  // A verbatim copy of the test above but using TestUrlNamer.
  const GoogleString kCssOut =
      EncodeCssCombineAndOptimize(kTestDomain, MultiUrl(kCssA, kCssB));
  static const char kCssText[] = " div {    } ";
  static const char kCssTextOptimized[] = "div{}";

  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss, kCssText, 300);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss, kCssText, 300);

  ValidateExpected(
      "follow_combine",
      StrCat(Link(kCssA), Link(kCssB)),
      Link(kCssOut));

  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(kCssOut, &content));
  EXPECT_EQ(StrCat(kCssTextOptimized, kCssTextOptimized), content);
}

class CssCombineMaxSizeTest : public CssCombineFilterTest {
 public:
  void CombineAndCheck(const char* css_file, int64 max_bytes,
                       int num_output_files, const int* num_files_in_output) {
    // Set up the filter.
    options()->ClearSignatureForTesting();
    options()->set_max_combined_css_bytes(max_bytes);
    server_context()->ComputeSignature(options());

    // Add CSS files to the html. The CSS files are named as '1.css',
    // '2.css', '3.css', etc.
    CssLink::Vector css_in;
    int id = 1;
    for (int i = 0; i < num_output_files; ++i) {
      for (int j = 0; j < num_files_in_output[i]; ++j) {
        css_in.Add(InputFileName(id), css_file, "", true);
        ++id;
      }
    }

    // Combine the CSS files in the html.
    CssLink::Vector css_out;
    BarrierTestHelper("max_combined_size", css_in, &css_out);
    ASSERT_EQ(num_output_files, css_out.size());

    // Verify that the CSS files have been combined as expected.
    id = 1;
    for (int i = 0; i < num_output_files; ++i) {
      if (num_files_in_output[i] == 1) {
        EXPECT_EQ(InputFileName(id), css_out[i]->url_);
        ++id;
      } else {
        GoogleString base;
        StringVector segments;
        ASSERT_TRUE(css_out[i]->DecomposeCombinedUrl(
            kTestDomain, &base, &segments, &message_handler_));
        ASSERT_EQ(num_files_in_output[i], segments.size());
        for (int j = 0; j < num_files_in_output[i]; ++j) {
          EXPECT_EQ(InputFileName(id), segments[j]);
          ++id;
        }
      }
    }
  }

 private:
  GoogleString InputFileName(int id) {
    return StringPrintf("%d.css", id);
  }
};

TEST_F(CssCombineMaxSizeTest, NegativeOneByte) {
  const int max_bytes = -1;
  const int num_output_files = 1;
  const int num_files_in_output[num_output_files] = {3};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, ZeroByte) {
  const int max_bytes = 0;
  const int num_output_files = 3;
  const int num_files_in_output[num_output_files] = {1, 1, 1};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, OneFileMinusOneByte) {
  const int max_bytes = STATIC_STRLEN(kYellow) - 1;
  const int num_output_files = 3;
  const int num_files_in_output[num_output_files] = {1, 1, 1};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, OneFile) {
  const int max_bytes = STATIC_STRLEN(kYellow);
  const int num_output_files = 3;
  const int num_files_in_output[num_output_files] = {1, 1, 1};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, OneFilePlusOneByte) {
  const int max_bytes = STATIC_STRLEN(kYellow) + 1;
  const int num_output_files = 3;
  const int num_files_in_output[num_output_files] = {1, 1, 1};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, TwoFilesMinusOneByte) {
  const int max_bytes = 2 * STATIC_STRLEN(kYellow) - 1;
  const int num_output_files = 3;
  const int num_files_in_output[num_output_files] = {1, 1, 1};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, TwoFiles) {
  const int max_bytes = 2 * STATIC_STRLEN(kYellow);
  const int num_output_files = 2;
  const int num_files_in_output[num_output_files] = {2, 2};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, TwoFilesPlusOneByte) {
  const int max_bytes = 2 * STATIC_STRLEN(kYellow) + 1;
  const int num_output_files = 3;
  const int num_files_in_output[num_output_files] = {2, 2, 1};
  CombineAndCheck(kYellow, max_bytes, num_output_files, num_files_in_output);
}

TEST_F(CssCombineMaxSizeTest, ReconstructedResourceExpectedHeaders) {
  GoogleString url = SetupReconstructOriginHeaders();
  ResponseHeaders headers;
  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(url, &content, &headers));
  EXPECT_TRUE(headers.HasValue("in_all_3", "abc"));
  EXPECT_FALSE(headers.Has("in_b"));
  EXPECT_FALSE(headers.Has("in_c"));
  EXPECT_TRUE(headers.IsProxyCacheable());
  EXPECT_EQ(timer()->NowMs() + ServerContext::kGeneratedMaxAgeMs,
            headers.CacheExpirationTimeMs());
  EXPECT_STREQ(
      "HTTP/1.1 200 OK\r\n"
      "in_all_3: abc\r\n"
      "Content-Type: text/css\r\n"
      "Date: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
      "Expires: Wed, 02 Feb 2011 18:51:26 GMT\r\n"
      "Cache-Control: max-age=31536000\r\n"
      "Etag: W/\"0\"\r\n"
      "Last-Modified: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
      "X-Original-Content-Length: 85\r\n"
      "\r\n",
      headers.ToString());
}

TEST_F(CssCombineMaxSizeTest, ReconstructedResourceExpectedHeadersNoStore) {
  GoogleString url = SetupReconstructOriginHeaders();
  // Contrive a case where someone requests a
  // .pagespeed. resource which we don't have in cache, and we go to
  // reconstruct it and once we fetch it, one of the inputs has
  // Cache-Control: no-store.  We don't expect this to happen normally.
  // This is only a corner case where someone has just converted a resource
  // from public to private and we haven't updated yet.
  AddHeader(kCssB, HttpAttributes::kCacheControl, HttpAttributes::kNoStore);
  ResponseHeaders headers;
  GoogleString content;
  EXPECT_TRUE(FetchResourceUrl(url, &content, &headers));
  EXPECT_TRUE(headers.HasValue("in_all_3", "abc"));
  EXPECT_FALSE(headers.Has("in_b"));
  EXPECT_FALSE(headers.Has("in_c"));
  EXPECT_FALSE(headers.IsProxyCacheable());
  EXPECT_STREQ(
      "HTTP/1.1 200 OK\r\n"
      "in_all_3: abc\r\n"
      "Content-Type: text/css\r\n"
      "Last-Modified: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
      "X-Original-Content-Length: 85\r\n"
      "Date: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
      "Expires: Tue, 02 Feb 2010 18:51:26 GMT\r\n"
      "Cache-Control: max-age=0,no-cache,no-store\r\n"
      "\r\n",
      headers.ToString());
}

class CollapseWhitespaceGeneralTest : public RewriteTestBase {
  // Don't add any text to our tests.
  virtual bool AddHtmlTags() const { return false; }
};

// Issue 463: Collapse whitespace after other filters have been applied
// for maximum effectiveness.
TEST_F(CollapseWhitespaceGeneralTest, CollapseAfterCombine) {
  // Note: Even though we enable collapse_whitespace first, it should run
  // after combine_css.
  options()->EnableFilter(RewriteOptions::kCollapseWhitespace);
  options()->EnableFilter(RewriteOptions::kCombineCss);
  rewrite_driver()->AddFilters();

  // Setup resources for combine_css.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(AbsolutifyUrl(kCssA),
                   default_css_header, ".a { color: red; }");
  SetFetchResponse(AbsolutifyUrl(kCssB),
                   default_css_header, ".b { color: green; }");
  SetFetchResponse(AbsolutifyUrl("c.css"),
                   default_css_header, ".c { color: blue; }");

  // Before and expected after text.
  static const char kBefore[] =
      "<html>\n"
      "  <head>\n"
      "    <link rel=stylesheet type=text/css href=a.css>\n"
      "    <link rel=stylesheet type=text/css href=b.css>\n"
      "    <link rel=stylesheet type=text/css href=c.css>\n"
      "  </head>\n"
      "</html>\n";
  static const char kAfterTemplate[] =
      "<html>\n"
      "<head>\n"
      "<link rel=stylesheet type=text/css href=%s />\n"
      "</head>\n"
      "</html>\n";
  GoogleString after = StringPrintf(kAfterTemplate, Encode(
      "", "cc", "0", MultiUrl(kCssA, kCssB, "c.css"), "css").c_str());

  ValidateExpected("collapse_after_combine", kBefore, after);
}

/*
  TODO(jmarantz): cover intervening FLUSH
  TODO(jmarantz): consider converting some of the existing tests to this
   format, covering
           IE Directive
           @Import in any css element except the first
           link in noscript tag
           change in 'media'
           incompatible domain
           intervening inline style tag (TODO(jmarantz): outline first?)
*/

}  // namespace

}  // namespace net_instaweb
