/*
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)
//     and sligocki@google.com (Shawn Ligocki)

#include "net/instaweb/rewriter/public/css_combine_filter.h"

#include <vector>

#include "base/logging.h"
#include "net/instaweb/htmlparse/public/html_parse_test_base.h"
#include "net/instaweb/http/public/log_record.h"
#include "net/instaweb/http/public/content_type.h"
#include "net/instaweb/http/public/meta_data.h"
#include "net/instaweb/http/public/mock_callback.h"
#include "net/instaweb/http/public/response_headers.h"
#include "net/instaweb/rewriter/public/cache_extender.h"
#include "net/instaweb/rewriter/public/debug_filter.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/util/public/charset_util.h"
#include "net/instaweb/util/public/google_url.h"
#include "net/instaweb/util/public/gtest.h"
#include "net/instaweb/util/public/lru_cache.h"
#include "net/instaweb/util/public/mem_file_system.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/mock_message_handler.h"
#include "net/instaweb/util/public/statistics.h"
#include "net/instaweb/util/public/stl_util.h"
#include "net/instaweb/util/public/string.h"
#include "net/instaweb/util/public/string_util.h"

namespace net_instaweb {

namespace {

const char kDomain[] = "http://combine_css.test/";
const char kYellow[] = ".yellow {background-color: yellow;}";
const char kBlue[] = ".blue {color: blue;}\n";
const char kACssBody[] = ".c1 {\n background-color: blue;\n}\n";
const char kBCssBody[] = ".c2 {\n color: yellow;\n}\n";
const char kCCssBody[] = ".c3 {\n font-weight: bold;\n}\n";

class CssCombineFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    AddFilter(RewriteOptions::kCombineCss);
    AddOtherFilter(RewriteOptions::kCombineCss);
  }

  // Test spriting CSS with options to write headers and use a hasher.
  void CombineCss(const StringPiece& id,
                  const StringPiece& barrier_text,
                  const StringPiece& debug_text,
                  bool is_barrier) {
    CombineCssWithNames(id, barrier_text, debug_text, is_barrier,
                        "a.css", "b.css", true);
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
    LoggingInfo logging_info;
    LogRecord log_record(&logging_info);
    rewrite_driver()->set_log_record(&log_record);
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

    Variable* css_file_count_reduction =
        statistics()->GetVariable(CssCombineFilter::kCssFileCountReduction);
    int orig_file_count_reduction = css_file_count_reduction->Get();

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

    EXPECT_EQ(expected_file_count_reduction, css_file_count_reduction->Get());
    if (expected_file_count_reduction > 0) {
      EXPECT_STREQ("cc", logging_info.applied_rewriters());
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
      StrAppend(&expected_output,
                "<!--css_combine: end_document-->"
                "<!--",
                DebugFilter::FormatEndDocumentMessage(0, 0, 0, 0, 0),
                "-->");
    }
    if (expect_combine) {
      EXPECT_EQ(expected_output, output_buffer_);

      // Fetch the combination to make sure we can serve the result from above.
      ExpectStringAsyncFetch expect_callback(true);
      rewrite_driver()->FetchResource(combine_url, &expect_callback);
      rewrite_driver()->WaitForCompletion();
      EXPECT_EQ(HttpStatus::kOK,
                expect_callback.response_headers()->status_code())
          << combine_url;
      EXPECT_EQ(expected_combination, expect_callback.buffer());

      // Now try to fetch from another server (other_rewrite_driver()) that
      // does not already have the combination cached.
      // TODO(sligocki): This has too much shared state with the first server.
      // See RewriteImage for details.
      ExpectStringAsyncFetch other_expect_callback(true);
      message_handler_.Message(kInfo, "Now with serving.");
      file_system()->Enable();
      other_rewrite_driver()->FetchResource(combine_url,
                                            &other_expect_callback);
      other_rewrite_driver()->WaitForCompletion();
      EXPECT_EQ(HttpStatus::kOK,
                other_expect_callback.response_headers()->status_code());
      EXPECT_EQ(expected_combination, other_expect_callback.buffer());

      // Try to fetch from an independent server.
      ServeResourceFromManyContexts(combine_url, expected_combination);
    }
  }

  // Test what happens when CSS combine can't find a previously-rewritten
  // resource during a subsequent resource fetch.  This used to segfault.
  void CssCombineMissingResource() {
    GoogleString a_css_url = StrCat(kDomain, "a.css");
    GoogleString c_css_url = StrCat(kDomain, "c.css");

    GoogleString expected_combination = StrCat(kACssBody, kCCssBody);

    // Put original CSS files into our fetcher.
    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(a_css_url, default_css_header, kACssBody);
    SetFetchResponse(c_css_url, default_css_header, kCCssBody);

    // First make sure we can serve the combination of a & c.  This is to avoid
    // spurious test successes.

    GoogleString kACUrl = Encode(kDomain, "cc", "0", MultiUrl("a.css", "c.css"),
                                 "css");
    GoogleString kABCUrl = Encode(kDomain, "cc", "0",
                                  MultiUrl("a.css", "bbb.css", "c.css"),
                                  "css");
    ExpectStringAsyncFetch expect_callback(true);

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
    ExpectStringAsyncFetch fail_callback(false);
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
  //     "a.css",
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

    GoogleString out;
    EXPECT_TRUE(FetchResourceUrl(munged_url,  &out));

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
    GoogleString a_css_url = StrCat(kTestDomain, "a.css");
    GoogleString b_css_url = StrCat(kTestDomain, "b.css");

    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(a_css_url, default_css_header, kYellow);
    SetFetchResponse(b_css_url, default_css_header, kBlue);

    GoogleString combined_url = Encode(kTestDomain, "cc", "0",
                                       MultiUrl("a.css", "b.css"), "css");

    SetupWriter();
    SetXhtmlMimetype();

    rewrite_driver()->StartParse(kTestDomain);
    GoogleString input_beginning =
        StrCat(kXhtmlDtd, "<div>", Link("a.css"), Link("b.css"));
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
    const char a_css_url[] = "http://other_domain.test/foo/a.css";
    const char b_css_url[] = "http://other_domain.test/foo/b.css";
    const char c_css_url[] = "http://other_domain.test/foo/c.css";

    ResponseHeaders default_css_header;
    SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
    SetFetchResponse(a_css_url, default_css_header, kACssBody);
    SetFetchResponse(b_css_url, default_css_header, kBCssBody);
    SetFetchResponse(c_css_url, default_css_header, kCCssBody);

    // Rewrite
    ParseUrl(html_url, html_input);

    // Check for CSS files in the rewritten page.
    CollectCssLinks("combine_css_no_media-links", output_buffer_, css_urls);
  }

  void TestFetch() {
    SetupCssResources("a.css", "b.css");
    GoogleString content;
    const GoogleString combined_url = Encode(
        kDomain, "cc", "0", MultiUrl("a.css", "b.css"), "css");
    ASSERT_TRUE(FetchResourceUrl(combined_url, &content));
    EXPECT_EQ(StrCat(kACssBody, kBCssBody), content);
  }

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
  SetupCssResources("a.css", "b.css");
  GoogleString html_input = StrCat(
      "<head>\n"
      "  ", Link("a.css"), "\n"
      "  ", Link("b.css"), "\n");
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


TEST_F(CssCombineFilterCustomOptions, CssPreserveURLs) {
  options()->set_css_preserve_urls(true);
  CssCombineFilterTest::SetUp();
  SetHtmlMimetype();
  CombineCssWithNames("combine_css_no_hash", "", "", false, "a.css", "b.css",
                      false);
}

// Make sure that if we re-parse the same html twice we do not
// end up recomputing the CSS (and writing to cache) again
TEST_F(CssCombineFilterTest, CombineCssRecombine) {
  SetHtmlMimetype();
  UseMd5Hasher();
  CombineCss("combine_css_recombine", "", "", false);
  int inserts_before = lru_cache()->num_inserts();

  CombineCss("combine_css_recombine", "", "", false);
  int inserts_after = lru_cache()->num_inserts();
  EXPECT_EQ(inserts_before, inserts_after);
  EXPECT_EQ(0, lru_cache()->num_identical_reinserts());
}


// http://code.google.com/p/modpagespeed/issues/detail?q=css&id=39
TEST_F(CssCombineFilterTest, DealWithParams) {
  SetHtmlMimetype();
  CombineCssWithNames("with_params", "", "", false, "a.css?U", "b.css?rev=138",
                      true);
}

// http://code.google.com/p/modpagespeed/issues/detail?q=css&id=252
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
      "  ", Link("a.css"), "\n"  // unclosed
      "  <script type='text/javascript' src='c.js'></script>"     // 'in' <link>
      "  ", Link("b.css")));
  GoogleString combination(StrCat(
      "  ", Link(Encode(kTestDomain, "cc", "0", MultiUrl("a.css", "b.css"),
                        "css"),
                 "", true),
      "\n"
      "  <script type='text/javascript' src='c.js'></script>  "));

  // Put original CSS files into our fetcher.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, "a.css"), default_css_header, ".a {}");
  SetFetchResponse(StrCat(kTestDomain, "b.css"), default_css_header, ".b {}");
  ValidateExpected("claims_xhtml_but_has_unclosed_links",
                   StringPrintf(html_format, kXhtmlDtd, unclosed_links.c_str()),
                   StringPrintf(html_format, kXhtmlDtd, combination.c_str()));
}

// http://code.google.com/p/modpagespeed/issues/detail?id=306
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
      Link("a.css", "screen", true), Link("b.css", "screen", true)));
  GoogleString combination(
      Link(Encode(kTestDomain, "cc", "0", MultiUrl("a.css", "b.css"), "css"),
           "screen", true));

  // Put original CSS files into our fetcher.
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, "a.css"), default_css_header, ".a {}");
  SetFetchResponse(StrCat(kTestDomain, "b.css"), default_css_header, ".b {}");
  ValidateExpected("xhtml_combination_closed",
                   StringPrintf(html_format, kXhtmlDtd, links.c_str()),
                   StringPrintf(html_format, kXhtmlDtd, combination.c_str()));
}

TEST_F(CssCombineFilterTest, CombineCssWithIEDirective) {
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

TEST_F(CssCombineFilterWithDebugTest, CombineCssWithIEDirectiveDebug) {
  SetHtmlMimetype();
  GoogleString ie_directive_barrier(StrCat(
      "<!--[if IE]>\n",
      Link("http://graphics8.nytimes.com/css/0.1/screen/build/homepage/ie.css"),
      "\n<![endif]-->"));
  UseMd5Hasher();
  CombineCss("combine_css_ie", ie_directive_barrier,
             "<!--css_combine: ie directive-->", true);
}

TEST_F(CssCombineFilterTest, CombineCssWithStyle) {
  SetHtmlMimetype();
  const char style_barrier[] = "<style>a { color: red }</style>\n";
  UseMd5Hasher();
  CombineCss("combine_css_style", style_barrier, "", true);
}

TEST_F(CssCombineFilterWithDebugTest, CombineCssWithStyleDebug) {
  SetHtmlMimetype();
  const char style_barrier[] = "<style>a { color: red }</style>\n";
  UseMd5Hasher();
  CombineCss("combine_css_style", style_barrier,
             "<!--css_combine: inline style-->", true);
}

TEST_F(CssCombineFilterTest, CombineCssWithBogusLink) {
  SetHtmlMimetype();
  const char bogus_barrier[] = "<link rel='stylesheet' "
      "href='crazee://big/blue/fake' type='text/css'>\n";
  UseMd5Hasher();
  CombineCss("combine_css_bogus_link", bogus_barrier, "",  true);
}

TEST_F(CssCombineFilterWithDebugTest, CombineCssWithBogusLink) {
  SetHtmlMimetype();
  const char bogus_barrier[] = "<link rel='stylesheet' "
      "href='crazee://big/blue/fake' type='text/css'>\n";
  UseMd5Hasher();
  CombineCss("combine_css_bogus_link", bogus_barrier,
             "<!--css_combine: resource not rewriteable-->",  true);
}

TEST_F(CssCombineFilterTest, CombineCssWithImportInFirst) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", "@Import '1a.css'", "", true);
  css_in.Add("2.css", kYellow, "", true);
  css_in.Add("3.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_import1", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());
}

TEST_F(CssCombineFilterTest, CombineCssWithImportInSecond) {
  CssLink::Vector css_in, css_out;
  css_in.Add("1.css", kYellow, "", true);
  css_in.Add("2.css", "@Import '2a.css'", "", true);
  css_in.Add("3.css", kYellow, "", true);
  BarrierTestHelper("combine_css_with_import1", css_in, &css_out);
  EXPECT_EQ("1.css", css_out[0]->url_);
  EXPECT_EQ(2, css_out.size());
}

TEST_F(CssCombineFilterTest, StripBom) {
  GoogleString html_url = StrCat(kDomain, "bom.html");
  GoogleString a_css_url = StrCat(kDomain, "a.css");
  GoogleString b_css_url = StrCat(kDomain, "b.css");

  // BOM documentation: http://www.unicode.org/faq/utf_bom.html
  GoogleString bom_body = StrCat(kUtf8Bom, kBCssBody);

  ResponseHeaders default_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_header);

  SetFetchResponse(a_css_url, default_header, kACssBody);
  SetFetchResponse(b_css_url, default_header, bom_body);

  StringVector css_urls;
  GoogleString input_buffer(StrCat(
      "<head>\n"
      "  ", Link("a.css"), "\n"
      "  ", Link("b.css"), "\n"
      "</head>\n"));
  ParseUrl(html_url, input_buffer);

  CollectCssLinks("combine_css_no_bom", output_buffer_, &css_urls);
  ASSERT_EQ(1UL, css_urls.size());
  GoogleString actual_combination;
  EXPECT_TRUE(FetchResourceUrl(css_urls[0], &actual_combination));
  int bom_pos = actual_combination.find(kUtf8Bom);
  EXPECT_EQ(GoogleString::npos, bom_pos);

  GoogleString input_buffer_reversed(StrCat(
      "<head>\n"
      "  ", Link("b.css"), "\n"
      "  ", Link("a.css"), "\n"
      "</head>\n"));
  ParseUrl(html_url, input_buffer_reversed);
  css_urls.clear();
  actual_combination.clear();
  CollectCssLinks("combine_css_beginning_bom", output_buffer_, &css_urls);
  ASSERT_EQ(1UL, css_urls.size());
  EXPECT_TRUE(FetchResourceUrl(css_urls[0], &actual_combination));
  bom_pos = actual_combination.find(kUtf8Bom);
  EXPECT_EQ(0, bom_pos);
  bom_pos = actual_combination.rfind(kUtf8Bom);
  EXPECT_EQ(0, bom_pos);
}

TEST_F(CssCombineFilterTest, StripBomReconstruct) {
  // Make sure we strip the BOM properly when reconstructing, too.
  const char kCssA[] = "a.css";
  const char kCssB[] = "b.css";
  const char kCssText[] = "div {background-image:url(fancy.png);}";
  SetResponseWithDefaultHeaders(kCssA, kContentTypeCss,
                                StrCat(kUtf8Bom, kCssText),
                                300);
  SetResponseWithDefaultHeaders(kCssB, kContentTypeCss,
                                StrCat(kUtf8Bom, kCssText),
                                300);
  GoogleString css_url =
      Encode(kTestDomain, "cc", "0", MultiUrl(kCssA, kCssB), "css");
  GoogleString css_out;
  EXPECT_TRUE(FetchResourceUrl(css_url, &css_out));
  EXPECT_EQ(StrCat(kUtf8Bom, kCssText, kCssText), css_out);
}

TEST_F(CssCombineFilterTest, CombineCssWithNoscriptBarrier) {
  SetHtmlMimetype();
  const char noscript_barrier[] =
      "<noscript>\n"
      "  <link rel='stylesheet' href='d.css' type='text/css'>\n"
      "</noscript>\n";

  // Put this in the Test class to remove repetition here and below.
  GoogleString d_css_url = StrCat(kDomain, "d.css");
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(d_css_url, default_css_header, d_css_body);

  UseMd5Hasher();
  CombineCss("combine_css_noscript", noscript_barrier, "", true);
}

TEST_F(CssCombineFilterTest, CombineCssWithFakeNoscriptBarrier) {
  SetHtmlMimetype();
  const char non_barrier[] =
      "<noscript>\n"
      "  <p>You have no scripts installed</p>\n"
      "</noscript>\n";
  UseMd5Hasher();
  CombineCss("combine_css_fake_noscript", non_barrier, "", false);
}

TEST_F(CssCombineFilterTest, CombineCssWithMediaBarrier) {
  SetHtmlMimetype();
  const char media_barrier[] =
      "<link rel='stylesheet' href='d.css' type='text/css' media='print'>\n";

  GoogleString d_css_url = StrCat(kDomain, "d.css");
  const char d_css_body[] = ".c4 {\n color: green;\n}\n";
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(d_css_url, default_css_header, d_css_body);

  UseMd5Hasher();
  CombineCss("combine_css_media", media_barrier, "", true);
}

TEST_F(CssCombineFilterTest, CombineCssWithNonMediaBarrier) {
  SetHtmlMimetype();

  // Put original CSS files into our fetcher.
  GoogleString html_url = StrCat(kDomain, "no_media_barrier.html");
  GoogleString a_css_url = StrCat(kDomain, "a.css");
  GoogleString b_css_url = StrCat(kDomain, "b.css");
  GoogleString c_css_url = StrCat(kDomain, "c.css");
  GoogleString d_css_url = StrCat(kDomain, "d.css");

  const char d_css_body[] = ".c4 {\n color: green;\n}\n";

  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(a_css_url, default_css_header, kACssBody);
  SetFetchResponse(b_css_url, default_css_header, kBCssBody);
  SetFetchResponse(c_css_url, default_css_header, kCCssBody);
  SetFetchResponse(d_css_url, default_css_header, d_css_body);

  // Only the first two CSS files should be combined.
  GoogleString html_input(StrCat(
      "<head>\n"
      "  ", Link("a.css", "print", false), "\n"
      "  ", Link("b.css", "print", false), "\n"));
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
      "  ", Link("a.css"), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link("b.css"), "\n"
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
      "  ", Link("a.css"), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link("b.css"), "\n"
      "  ", Link("c.css"), "\n"
      "</head>\n"));
  CombineWithBaseTag(input_buffer, &css_urls);

  GoogleString expected_output(StrCat(
      "<head>\n"
      "  ", Link("a.css"), "\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(css_urls[1]), "\n"
      "  \n"
      "</head>\n"));
  EXPECT_EQ(2UL, css_urls.size());
  EXPECT_EQ(EncodeWithBase("http://other_domain.test/",
                           "http://other_domain.test/foo/", "cc", "0",
                           MultiUrl("b.css", "c.css"), "css"),
            css_urls[1]);
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_TRUE(GoogleUrl(css_urls[1]).is_valid());
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
      "  ", Link("b.css"), "\n"
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
                           "http://other_domain.test/foo/", "cc", "0",
                           MultiUrl("a.css", "b.css"), "css"),
            css_urls[0]);
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_TRUE(GoogleUrl(css_urls[0]).is_valid());
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
      "  ", Link("a.css"), "\n"
      "  ", Link("b.css"), "\n"
      "</head>\n"), &css_urls);

  GoogleString expected_output(StrCat(
      "<head>\n"
      "  <base href='http://other_domain.test/foo/'>\n"
      "  ", Link(css_urls[0]), "\n"
      "  \n"
      "</head>\n"));
  EXPECT_EQ(1UL, css_urls.size());
  EXPECT_EQ(AddHtmlBody(expected_output), output_buffer_);
  EXPECT_EQ(EncodeWithBase("http://other_domain.test/",
                           "http://other_domain.test/foo/", "cc", "0",
                           MultiUrl("a.css", "b.css"), "css"),
            css_urls[0]);
  EXPECT_TRUE(GoogleUrl(css_urls[0]).is_valid());
}

TEST_F(CssCombineFilterTest, CombineCssNoInput) {
  SetFetchFailOnUnexpected(false);
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(StrCat(kTestDomain, "b.css"),
                   default_css_header, ".a {}");
  static const char html_input[] =
      "<head>\n"
      "  <link rel='stylesheet' href='a_broken.css' type='text/css'>\n"
      "  <link rel='stylesheet' href='b.css' type='text/css'>\n"
      "</head>\n"
      "<body><div class='c1'><div class='c2'><p>\n"
      "  Yellow on Blue</p></div></div></body>";
  ValidateNoChanges("combine_css_missing_input", html_input);
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
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
  GoogleUrl dummy_encoded(Encode(StrCat(kTestDomain, "styles/"), "x", "0",
                                 "x", "x"));
  EXPECT_EQ(dummy_encoded.AllExceptLeaf(), base);
  EXPECT_EQ(kNumCssInCombination, segments.size());

  segments.clear();
  ASSERT_TRUE(css_out[1]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
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
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
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
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
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
  ASSERT_TRUE(css_out[0]->DecomposeCombinedUrl(&base, &segments,
                                               &message_handler_));
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
  EXPECT_TRUE(FetchResourceUrl(css_out[0]->url_, &actual_combination));
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
  EXPECT_TRUE(FetchResourceUrl(css_out[0]->url_, &actual_combination));
  // TODO(sligocki): Check headers?
  EXPECT_EQ(expected_combination, actual_combination);
}

TEST_F(CssCombineFilterTest, ShardSubresources) {
  UseMd5Hasher();
  DomainLawyer* lawyer = options()->domain_lawyer();
  lawyer->AddShard(kTestDomain, "shard1.com,shard2.com", &message_handler_);

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
  EXPECT_TRUE(FetchResourceUrl(css_out[0]->url_, &actual_combination));
  GoogleUrl gurl(css_out[0]->url_);
  ASSERT_TRUE(gurl.is_valid());
  GoogleUrl dummy_encoded(Encode(StrCat(kTestDomain, long_name, "/"), "x", "0",
                                 "x", "x"));
  EXPECT_EQ(dummy_encoded.PathSansLeaf(), gurl.PathSansLeaf());
  ResourceNamer namer;
  ASSERT_TRUE(namer.Decode(gurl.LeafWithQuery()));
  EXPECT_EQ("a.css+b.css", namer.name());
  EXPECT_EQ(StrCat(kYellow, kBlue), actual_combination);
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
  DomainLawyer* laywer = options()->domain_lawyer();
  laywer->AddRewriteDomainMapping("a.com", "b.com", &message_handler_);
  bool supply_mock = false;
  css_in.Add("http://a.com/1.css", kYellow, "", supply_mock);
  css_in.Add("http://b.com/2.css", kBlue, "", supply_mock);
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse("http://a.com/1.css", default_css_header,
                                kYellow);
  SetFetchResponse("http://b.com/2.css", default_css_header,
                                kBlue);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(1, css_out.size());
  GoogleString actual_combination;
  EXPECT_TRUE(FetchResourceUrl(css_out[0]->url_, &actual_combination));
  EXPECT_EQ(Encode("http://a.com/", "cc", "0", MultiUrl("1.css", "2.css"),
                   "css"),
            css_out[0]->url_);
  EXPECT_EQ(StrCat(kYellow, kBlue), actual_combination);
}

// Verifies that we cannot do the same cross-domain combo when we lack
// the domain mapping.
TEST_F(CssCombineFilterTest, CrossUnmappedDomain) {
  CssLink::Vector css_in, css_out;
  DomainLawyer* laywer = options()->domain_lawyer();
  laywer->AddDomain("a.com", &message_handler_);
  laywer->AddDomain("b.com", &message_handler_);
  bool supply_mock = false;
  const char kUrl1[] = "http://a.com/1.css";
  const char kUrl2[] = "http://b.com/2.css";
  css_in.Add(kUrl1, kYellow, "", supply_mock);
  css_in.Add(kUrl2, kBlue, "", supply_mock);
  ResponseHeaders default_css_header;
  SetDefaultLongCacheHeaders(&kContentTypeCss, &default_css_header);
  SetFetchResponse(kUrl1, default_css_header, kYellow);
  SetFetchResponse(kUrl2, default_css_header, kBlue);
  BarrierTestHelper("combine_css_with_style", css_in, &css_out);
  EXPECT_EQ(2, css_out.size());
  GoogleString actual_combination;
  EXPECT_EQ(kUrl1, css_out[0]->url_);
  EXPECT_EQ(kUrl2, css_out[1]->url_);
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
  input_css_links.Add("a.css", kYellow, "", true /* supply_mock*/);
  input_css_links.Add("b.css", kYellow, "", true /* supply_mock*/);
  input_css_links.Add("404.css", kYellow, "", false /* supply_mock*/);
  input_css_links.Add("c.css", kYellow, "", true /* supply_mock*/);
  input_css_links.Add("d.css", kYellow, "", true /* supply_mock*/);

  BarrierTestHelper("two_comb", input_css_links, &output_css_links);

  ASSERT_EQ(3, output_css_links.size());
  EXPECT_EQ(Encode(kTestDomain, "cc", "0", MultiUrl("a.css", "b.css"), "css"),
            output_css_links[0]->url_);
  EXPECT_EQ("404.css", output_css_links[1]->url_);
  EXPECT_EQ(Encode(kTestDomain, "cc", "0", MultiUrl("c.css", "d.css"), "css"),
            output_css_links[2]->url_);

  // Get rid of the "modern" cache key, while keeping the old one.
  lru_cache()->Delete(
      ",htest.com,_a.css+,htest.com,_b.css+,htest.com,_404.css+"
      ",htest.com,_c.css+,htest.com,_d.css:cc");

  // Now do it again...
  BarrierTestHelper("two_comb", input_css_links, &output_css_links);
}

TEST_F(CssCombineFilterTest, InvalidFetchCache) {
  // Regression test for crashes when we're asked to do an invalid
  // fetch and then repeat it for a rewriter inside an XHTML-DTD page.
  SetFetchResponse404("404a.css");
  SetFetchResponse404("404b.css");

  EXPECT_FALSE(TryFetchResource(
      Encode(kTestDomain, "cc", "0", MultiUrl("404a.css", "404b.css"), "css")));
  ValidateNoChanges("invalid",
                    StrCat(kXhtmlDtd,
                           CssLinkHref("404a.css"),
                           CssLinkHref("404b.css")));
}

TEST_F(CssCombineFilterTest, NoCombineParseErrors) {
  // Notice: This CSS file does not close its { and thus will break the
  // next stylesheet if they are combined, changing the page.
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                "h1 { color: red", 100);
  SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                "h2 { color: blue; }", 100);

  ValidateNoChanges("bad_parse", StrCat(CssLinkHref("a.css"),
                                        CssLinkHref("b.css")));
}

TEST_F(CssCombineFilterTest, NoCombineParseErrorsAtRule) {
  // Notice: This CSS file does not close its { and thus will break the
  // next stylesheet if they are combined, changing the page.
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                "@foobar { color: red", 100);
  SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                "h2 { color: blue; }", 100);

  ValidateNoChanges("bad_parse", StrCat(CssLinkHref("a.css"),
                                        CssLinkHref("b.css")));
}

// See: http://www.alistapart.com/articles/alternate/
//  and http://www.w3.org/TR/html4/present/styles.html#h-14.3.1
TEST_F(CssCombineFilterTest, AlternateStylesheets) {
  SetResponseWithDefaultHeaders("a.css", kContentTypeCss,
                                "h1 { color: red; }", 100);
  SetResponseWithDefaultHeaders("b.css", kContentTypeCss,
                                "h2 { color: blue; }", 100);

  // Normal (persistent) CSS links are combined.
  ValidateExpected(
      "persistent",
      "<link rel='stylesheet' href='a.css'>"
      "<link rel='stylesheet' href='b.css'>",
      StringPrintf("<link rel='stylesheet' href='%s'/>", Encode(
          kTestDomain, "cc", "0", MultiUrl("a.css", "b.css"), "css").c_str()));

  // Make sure we accept mixed case for the keyword.
  ValidateExpected(
      "mixed_case",
      "<link rel=' StyleSheet' href='a.css'>"
      "<link rel='styleSHEET  ' href='b.css'>",
      StringPrintf("<link rel=' StyleSheet' href='%s'/>", Encode(
          kTestDomain, "cc", "0", MultiUrl("a.css", "b.css"), "css").c_str()));

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
  SetResponseWithDefaultHeaders("a.css", kContentTypeJavascript, kYellow, 100);
  SetResponseWithDefaultHeaders("b.css", kContentTypeJavascript, kBlue, 100);
  GoogleString combined_url =
      Encode(kTestDomain, "cc", "0", MultiUrl("a.css", "b.css"), "css");

  ValidateExpected("combine",
                   StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
                   CssLinkHref(combined_url));
  EXPECT_EQ(0,
            rewrite_driver()->statistics()->GetVariable(
                CacheExtender::kCacheExtensions)->Get());

  // Now try cached.
  ValidateExpected("combine",
                   StrCat(CssLinkHref("a.css"), CssLinkHref("b.css")),
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
};

// See TestFollowCombine below: change one, change them both!
TEST_F(CssFilterWithCombineTest, TestFollowCombine) {
  SetHtmlMimetype();

  // Make sure we don't regress dealing with combiner deleting things sanely
  // in rewrite filter.
  const char kCssA[] = "a.css";
  const char kCssB[] = "b.css";
  const GoogleString kCssOut =
      Encode(kTestDomain, "cf", "0",
             Encode("", "cc", "0", MultiUrl("a.css", "b.css"), "css"), "css");
  const char kCssText[] = " div {    } ";
  const char kCssTextOptimized[] = "div{}";

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
  EXPECT_NE(Encode(kTestDomain, "cc", "0", "a.css", "css"),
            EncodeNormal(kTestDomain, "cc", "0", "a.css", "css"));

  // A verbatim copy of the test above but using TestUrlNamer.
  const char kCssA[] = "a.css";
  const char kCssB[] = "b.css";
  const GoogleString kCssOut =
      Encode(kTestDomain, "cf", "0",
             Encode("", "cc", "0", MultiUrl("a.css", "b.css"), "css"), "css");
  const char kCssText[] = " div {    } ";
  const char kCssTextOptimized[] = "div{}";

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

/*
  TODO(jmarantz): cover intervening FLUSH
  TODO(jmarantz): consider converting some of the existing tests to this
   format, covering
           IE Directive
           @Import in any css element except the first
           link in noscript tag
           change in 'media'
           incompatible domain
           intervening inline style tag (TODO: outline first?)
*/

}  // namespace

}  // namespace net_instaweb