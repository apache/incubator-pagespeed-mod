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


#include "net/instaweb/rewriter/public/url_left_trim_filter.h"

#include "base/logging.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_test_base.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class UrlLeftTrimFilterTest : public RewriteTestBase {
 protected:
  virtual void SetUp() {
    RewriteTestBase::SetUp();
    left_trim_filter_.reset(new UrlLeftTrimFilter(rewrite_driver(),
                                                  statistics()));
    rewrite_driver()->AddFilter(left_trim_filter_.get());
  }

  // Must set base url to "http://www.example.com/dir/*something*"
  // before running these.
  void TestAnchors(const StringPiece& base_url) {
    SetFilterBaseUrl(base_url);
    OneTrim(true, "http://www.example.com/dir/?var=val#anchor",
            "/dir/?var=val#anchor");
    OneTrim(true, "http://www.example.com/dir/#anchor", "/dir/#anchor");
    OneTrim(true, "http://www.example.com/dir/foo.html", "foo.html");
    OneTrim(true, "http://www.example.com/dir/abc/f?g=h", "abc/f?g=h");
    OneTrim(true, "http://www.example.com/dir/f?g=h#anchor",
            "f?g=h#anchor");
    OneTrim(true, "http://www.example.com/dir/index.html#",
            "index.html#");
    OneTrim(true, "http://www.example.com/dir/index.html?f=g#bottom",
            "index.html?f=g#bottom");
    OneTrim(true, "http://www.example.com/dir/index.html?f=g#bottom",
            "index.html?f=g#bottom");
    OneTrim(false, "#top", "");
  }

  void OneTrim(bool changed,
               const StringPiece& init, const StringPiece& expected) {
    StringPiece url(init);
    GoogleString trimmed;
    CHECK(base_url_.get() != NULL);
    EXPECT_EQ(changed, left_trim_filter_->Trim(
        *base_url_.get(), url, &trimmed,
        rewrite_driver()->message_handler()));
    if (changed) {
      EXPECT_STREQ(expected, trimmed);
    }
  }

  void SetFilterBaseUrl(const StringPiece& base_url) {
    base_url_.reset(new GoogleUrl(base_url));
  }

  virtual bool AddBody() const { return false; }

  // TODO(jmarantz): factor out all the image references in this file to use
  // this method.
  GoogleString Image(const StringPiece& image) {
    return StrCat("<img src='", image, "'>");
  }

 private:
  scoped_ptr<UrlLeftTrimFilter> left_trim_filter_;
  scoped_ptr<GoogleUrl> base_url_;
};

static const char kBase[] = "http://foo.bar/baz/";
static const char kHttp[] = "http:";
static const char kPath[] = "/baz/";

TEST_F(UrlLeftTrimFilterTest, SimpleTrims) {
  SetFilterBaseUrl("http://foo.bar/baz/");
  // TODO(jmaessen): strip protocol given user-agent-sensitive trimming
  OneTrim(false, "http://www.google.com/", "http://www.google.com/");
  OneTrim(true, kBase, kPath);
  OneTrim(true, "http://foo.bar/baz/quux", "quux");
  OneTrim(true, "/baz/quux", "quux");
  OneTrim(true, "//foo.bar/img/img1.jpg", "/img/img1.jpg");
  OneTrim(false, "/img/img1.jpg", "/img/img1.jpg");
  OneTrim(false, kHttp, kHttp);  // false, because /baz/ is 5 chars long
  OneTrim(true, "//foo.bar/baz/quux", "quux");
  OneTrim(false, "baz/img.jpg", "baz/img.jpg");
}

static const char kRootedBase[] = "http://foo.bar/";

// Catch screw cases when a base url lies at the root of a domain.
TEST_F(UrlLeftTrimFilterTest, RootedTrims) {
  SetFilterBaseUrl(kRootedBase);
  // TODO(jmaessen): strip protocol given user-agent-sensitive trimming
  OneTrim(false, "http://www.google.com/", "http://www.google.com/");
  OneTrim(true, kBase, "baz/");
  OneTrim(false, "//www.google.com/", "//www.google.com/");
  OneTrim(true, kPath, "baz/");
  OneTrim(false, "quux", "quux");
}

static const char kNone[] =
    "<head><base href='ftp://what.the/heck/'/>"
    "<link rel='stylesheet' href='http://what.the.cow/heck/'/></head>"
    "<body><a href='spdy://www.google.com/'>google</a>"
    "<img src='file:///where/the/heck.jpg'/></body>";

TEST_F(UrlLeftTrimFilterTest, NoChanges) {
  ValidateNoChanges("none_forward", kNone);
}

static const char kSome[] =
    "<head><base href='http://foo.bar/baz/'/>"
    "<link rel='stylesheet' href='http://foo.bar/baz/'/></head>"
    "<body><a href='http://www.google.com/'>google</a>"
    "<img src='http://foo.bar/baz/nav.jpg'/>"
    "<img src='http://foo.bar/img/img1.jpg'/>"
    "<img src='/baz/img2.jpg'/>"
    "<img src='//foo.bar/baz/widget.png'/>"
    "<a href='./xyz/something.html'>text!</a></body>";

static const char kSomeRewritten[] =
    "<head><base href='http://foo.bar/baz/'/>"
    "<link rel='stylesheet' href='/baz/'/></head>"
    "<body><a href='http://www.google.com/'>google</a>"
    "<img src='nav.jpg'/>"
    "<img src='/img/img1.jpg'/>"
    "<img src='img2.jpg'/>"
    "<img src='widget.png'/>"
    "<a href='xyz/something.html'>text!</a></body>";

TEST_F(UrlLeftTrimFilterTest, SomeChanges) {
  ValidateExpected("some_forward", kSome, kSomeRewritten);
}

static const char kFirstDoc[] =
    "<head><base href='http://foo/'/></head>"
    "<body><a href='http://foo/abc'>link</a>"
    "<img src='www.google.com/pretty_picture.jpg'>"
    "<img src='http://foo/bar/123.png'></body>";

static const char kFirstDocRewritten[] =
    "<head><base href='http://foo/'/></head>"
    "<body><a href='abc'>link</a>"
    "<img src='www.google.com/pretty_picture.jpg'>"
    "<img src='bar/123.png'></body>";

static const char kSecondDoc[] =
    "<head><base href='http://newurl/baz/'/></head>"
    "<body><a href='http://foo/baz/abc'>text</a>"
    "<a href='http://newurl/baz/target'>more text</a>"
    "<img src='www.google.com/pretty_picture.jpg'>"
    "<img src='/baz/image.jpg'></body>";

static const char kSecondDocRewritten[] =
    "<head><base href='http://newurl/baz/'/></head>"
    "<body><a href='http://foo/baz/abc'>text</a>"
    "<a href='target'>more text</a>"
    "<img src='www.google.com/pretty_picture.jpg'>"
    "<img src='image.jpg'></body>";

TEST_F(UrlLeftTrimFilterTest, TwoBases) {
  ValidateExpected("first_doc", kFirstDoc, kFirstDocRewritten);
  ValidateExpected("second_doc", kSecondDoc, kSecondDocRewritten);
}

static const char kPartialUrl[] =
    "<head><base href='http://abcdef/123'/></head>"
    "<body><a href='abcdef/something'>link</a>"
    "<img src='http://abcdefg'></body>";

static const char kPartialUrlRewritten[] =
    "<head><base href='http://abcdef/123'/></head>"
    "<body><a href='abcdef/something'>link</a>"
    "<img src='http://abcdefg'></body>";

TEST_F(UrlLeftTrimFilterTest, PartialUrl) {
  ValidateExpected("partial_url", kPartialUrl, kPartialUrlRewritten);
}

// TODO(nforman): in correct html, the base tag (with href) must come before
// any other urls, thereby making them all relative to the same thing (i.e.
// the doc's url if there is no base tag, and the base tag url if there is one).
// However, different browsers deal with malformed html in different ways.
// Some browsers change the base at the point of the base tag (Firefox),
// and therefore will resolve the following (located at http://abc.com/foo.html)
// <html>
// <head>
// <title>Foo - Too Many Bases Test</title>
// <a href="imghp">Google Images, before base tag</a>
// <base href="http://www.google.com">
// </head>
// <body>
// <a href="/">Empty Link after base tag.</a>
// </body>
// to an invalid link, http://abc.com/imghp, and to http://www.google.com.
// However, chrome will resolve all the urls against "http://www.google.com",
// giving http://www.google.com/imghp and http://www.google.com.
// Furthermore, chrome and firefox handle the multiple base tags issue
// differently.
// Our current behavior is to ignore any src or href attributes that come
// before the base tag.
static const char kMidBase[] =
    "<head><link rel='stylesheet' href='http://foo.bar/baz'/>"
    "<a href='baz.html'>strange link in header</a>"
    "<base href='http://foo.bar'></head>"
    "<body><img src='//foo.bar/img.jpg'></body>";

static const char kMidBaseRewritten[] =
    "<head><link rel='stylesheet' href='http://foo.bar/baz'/>"
    "<a href='baz.html'>strange link in header</a>"
    "<base href='http://foo.bar'></head>"
    "<body><img src='img.jpg'></body>";

TEST_F(UrlLeftTrimFilterTest, MidwayBaseUrl) {
  ValidateExpected("midway_base", kMidBase, kMidBaseRewritten);
}

static const char kAnnoyingWiki[] =
    "<head><base href='http://en.wikipedia.org/wiki/Labrador_Retriever'/>"
    "</head><body><img src='/wiki/img.jpg'>"
    "<a href='/wiki/File:puppy.jpg'>dog</a></body>";

static const char kAnnoyingWikiRewritten[] =
    "<head><base href='http://en.wikipedia.org/wiki/Labrador_Retriever'/>"
    "</head><body><img src='img.jpg'>"
    "<a href='/wiki/File:puppy.jpg'>dog</a></body>";

TEST_F(UrlLeftTrimFilterTest, AnnoyingWiki) {
  ValidateExpected("wiki", kAnnoyingWiki, kAnnoyingWikiRewritten);
}

TEST_F(UrlLeftTrimFilterTest, Directories) {
  SetFilterBaseUrl("http://www.example.com/foo/bar/index.html");
  OneTrim(false, "..", "..");
}

TEST_F(UrlLeftTrimFilterTest, Dots) {
  SetFilterBaseUrl("http://foo/bar/");
  OneTrim(true, "foo/bar/../baz/x.html", "foo/baz/x.html");
}

TEST_F(UrlLeftTrimFilterTest, XKCD) {
  SetFilterBaseUrl("http://forums.xkcd.com/");
  // TODO(jmaessen): strip protocol given user-agent-sensitive trimming
  OneTrim(false, "http://xkcd.com/", "http://xkcd.com/");
}

TEST_F(UrlLeftTrimFilterTest, OneDot) {
  SetFilterBaseUrl("http://foo.bar/baz/index.html");
  OneTrim(true, "./cows/index.html", "cows/index.html");
}

TEST_F(UrlLeftTrimFilterTest, Query) {
  SetFilterBaseUrl("http://foo.bar/index.html");
  OneTrim(true, "http://foo.bar/?a=b", "/?a=b");
}

TEST_F(UrlLeftTrimFilterTest, TrimQuery) {
  SetFilterBaseUrl("http://foo.bar/baz/index.html");
  OneTrim(true, "http://foo.bar/baz/other.html?a=b", "other.html?a=b");
}

TEST_F(UrlLeftTrimFilterTest, DoubleSlashPath) {
  SetFilterBaseUrl("http://foo.bar/baz/index.html");
  OneTrim(true, "http://foo.bar/baz//other.html", "/baz//other.html");
}

TEST_F(UrlLeftTrimFilterTest, DoubleSlashBeginningPath) {
  SetFilterBaseUrl("http://foo.bar/index.html");
  OneTrim(false, "http://foo.bar//other.html", "http://foo.bar//other.html");
}

TEST_F(UrlLeftTrimFilterTest, TripleSlashPath) {
  SetFilterBaseUrl("http://foo.bar/example/index.html");
  OneTrim(true, "http://foo.bar/example///other.html", "/example///other.html");
}

static const char kBlankBase[] =
    "<head><base href=''>"
    "</head><body>"
    "<a href='http://www.google.com/'>foo</a></body>";

static const char kBlankBaseRewritten[] =
    "<head><base href=''>"
    "</head><body>"
    "<a href='http://www.google.com/'>foo</a></body>";

TEST_F(UrlLeftTrimFilterTest, BlankBase) {
  ValidateExpected("wiki", kBlankBase, kBlankBaseRewritten);
}

static const char kRelativeBase[] =
    "<head><base href='/directory/'>"
    "</head><body>"
    "<img src='/directory/img.jpg'></body>";

static const char kRelativeBaseRewritten[] =
    "<head><base href='/directory/'>"
    "</head><body>"
    "<img src='img.jpg'></body>";

TEST_F(UrlLeftTrimFilterTest, RelativeBase) {
  ValidateExpected("wiki", kRelativeBase, kRelativeBaseRewritten);
}

TEST_F(UrlLeftTrimFilterTest, QueryWithEncodedAmpersand) {
  ValidateExpected(
      "ampersand",
      Image("http://test.com/discuss/a.php?&action=vtopic&amp;forum=2"),
      Image("discuss/a.php?&amp;action=vtopic&amp;forum=2"));

  // TODO(jmarantz): consider this case.  We think this should be trimmed:
  //     ValidateExpected("improperly_terminated_multi_byte_escape",
  //       Image("http://test.com/?q=klaatu+nikto+barada&lang=fr"),
  //       Image("?q=klaatu+nikto+barada&amp;lang=fr"));
  // but it is not, because with "&lang=", we interpretet the
  // "&lang" as an improperly terminated "&lang;".  Now "&lang;" is
  // a multi-byte sequence, which we cannot represent, so we get a
  // decoding error, and thus cannot intpreret this URL at all.
  //
  // TODO(jmarantz): consider treating improperly terminated multi-byte
  // escape sequences as literals.
  //
  // For now:
  ValidateNoChanges("improperly_terminated_multi_byte_escape",
                    Image("http://test.com/?q=klaatu+nikto+barada&lang=fr"));
}

TEST_F(UrlLeftTrimFilterTest, Anchors) {
  TestAnchors("http://www.example.com/dir/?var=val");
  TestAnchors("http://www.example.com/dir/index.html");
  TestAnchors("http://www.example.com/dir/index.html#top");
  TestAnchors("http://www.example.com/dir/index.html?f=g");
  TestAnchors("http://www.example.com/dir/index.html?f=g&y=z#bottom");
}

}  // namespace net_instaweb
