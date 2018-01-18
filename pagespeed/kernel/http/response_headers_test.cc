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



// Unit-test SimpleUrlData, in particular it's HTTP header parser.

#include "pagespeed/kernel/http/response_headers.h"

#include <cstddef>                     // for size_t
#include <algorithm>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_timer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/time_util.h"
#include "pagespeed/kernel/base/timer.h"  // for Timer
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http.pb.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers_parser.h"

namespace net_instaweb {

class ResponseHeadersTest : public testing::Test {
 protected:
  ResponseHeadersTest()
      : parser_(&response_headers_),
        max_age_300_("max-age=300") {
    ConvertTimeToString(MockTimer::kApr_5_2010_ms, &start_time_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms + 5 * Timer::kMinuteMs,
                        &start_time_plus_5_minutes_string_);
    ConvertTimeToString(MockTimer::kApr_5_2010_ms + 6 * Timer::kMinuteMs,
                        &start_time_plus_6_minutes_string_);
    with_auth_.Add(HttpAttributes::kAuthorization, "iris scan");
  }

  void CheckGoogleHeaders(const ResponseHeaders& response_headers) {
    EXPECT_EQ(200, response_headers.status_code());
    EXPECT_EQ(1, response_headers.major_version());
    EXPECT_EQ(0, response_headers.minor_version());
    EXPECT_EQ(GoogleString("OK"),
              GoogleString(response_headers.reason_phrase()));
    ConstStringStarVector values;
    EXPECT_TRUE(response_headers.Lookup("X-Google-Experiment", &values));
    EXPECT_EQ(GoogleString("23729,24249,24253"), *(values[0]));
    EXPECT_TRUE(response_headers.Lookup(HttpAttributes::kSetCookie, &values));
    EXPECT_EQ(2, values.size());
    EXPECT_EQ(GoogleString("PREF=ID=3935f510d83d2a7a:TM=1270493386:LM=127049338"
                           "6:S=u_18e6r8aJ83N6P1; "
                           "expires=Wed, 04-Apr-2012 18:49:46 GMT; path=/; "
                           "domain=.google.com"),
              *(values[0]));
    EXPECT_EQ(GoogleString("NID=33=aGkk7cKzznoUuCd19qTgXlBjXC8fc_luIo2Yk9BmrevU"
                           "gXYPTazDF8Q6JvsO6LvTu4mfI8_44iIBLu4pF-Mvpe4wb7pYwej"
                           "4q9HvbMLRxt-OzimIxmd-bwyYVfZ2PY1B; "
                           "expires=Tue, 05-Oct-2010 18:49:46 GMT; path=/; "
                           "domain=.google.com; HttpOnly"),
              *(values[1]));
    EXPECT_EQ(12, response_headers.NumAttributes());
    EXPECT_EQ(GoogleString("X-Google-GFE-Response-Body-Transformations"),
              GoogleString(response_headers.Name(11)));
    EXPECT_EQ(GoogleString("gunzipped"),
              GoogleString(response_headers.Value(11)));
  }

  void ParseHeaders(const StringPiece& headers) {
    parser_.Clear();
    parser_.ParseChunk(headers, &message_handler_);
  }

  // Check sizes of the header vector and map.
  void ExpectSizes(int num_headers, int num_header_names) {
    EXPECT_EQ(num_headers, response_headers_.NumAttributes());
    EXPECT_EQ(num_header_names, response_headers_.NumAttributeNames());
  }

  bool ComputeImplicitCaching(
      int status_code, const char* content_type,
      const GoogleString& max_age_string,
      const GoogleString& start_time_plus_implicit_ttl_string) {
    GoogleString header_text =
        StringPrintf("HTTP/1.0 %d OK\r\n"
                     "Date: %s\r\n"
                     "Content-type: %s\r\n\r\n",
                     status_code, start_time_string_.c_str(), content_type);
    response_headers_.Clear();
    ParseHeaders(header_text);
    bool cacheable = response_headers_.IsProxyCacheable();
    if (!cacheable) {
      EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kCacheControl));
      EXPECT_EQ(NULL, response_headers_.Lookup1(HttpAttributes::kExpires));
    } else {
      EXPECT_STREQ(max_age_string,
                   response_headers_.Lookup1(HttpAttributes::kCacheControl));
      EXPECT_STREQ(start_time_plus_implicit_ttl_string,
                   response_headers_.Lookup1(HttpAttributes::kExpires));
    }
    return cacheable;
  }

  bool ComputeImplicitCaching(int status_code, const char* content_type) {
    return ComputeImplicitCaching(status_code, content_type, max_age_300_,
                                  start_time_plus_5_minutes_string_);
  }

  bool IsHtmlLike(const StringPiece& type) {
    response_headers_.Clear();
    GoogleString header_text = "HTTP/1.1 200 OK\r\n";
    if (!type.empty()) {
      StrAppend(&header_text, "Content-Type: ", type, "\r\n");
    }
    header_text += "\r\n";
    ParseHeaders(header_text);
    return response_headers_.IsHtmlLike();
  }

  // At the end of every test, check to make sure that clearing the meta-data
  // produces an equivalent structure to a freshly initialized one.
  virtual void TearDown() {
    response_headers_.Clear();
    ResponseHeaders empty_response_headers;

    // TODO(jmarantz): at present we lack a comprehensive serialization
    // that covers all the member variables, but at least we can serialize
    // to an HTTP-compatible string.
    EXPECT_EQ(empty_response_headers.ToString(), response_headers_.ToString());
  }

  // Wrapper method to expose the cache-dirty bit so we can test to
  // ensure it gets set on mutations.
  bool ResponseCachingDirty() const {
    return response_headers_.cache_fields_dirty_;
  }

  bool IsProxyCacheable(const RequestHeaders& request_headers,
                        ResponseHeaders::VaryOption respect_vary) {
    return response_headers_.IsProxyCacheable(request_headers.GetProperties(),
                                              respect_vary,
                                              ResponseHeaders::kNoValidator);
  }

  bool IsProxyCacheable(const RequestHeaders& request_headers) {
    return response_headers_.IsProxyCacheable(
        request_headers.GetProperties(),
        ResponseHeaders::kRespectVaryOnResources,
        ResponseHeaders::kNoValidator);
  }

  bool IsVaryCacheable(bool has_cookie, bool has_cookie2,
                       ResponseHeaders::VaryOption respect_vary,
                       ResponseHeaders::ValidatorOption has_validator) {
    RequestHeaders::Properties properties;
    properties.has_cookie = has_cookie;
    properties.has_cookie2 = has_cookie2;
    return response_headers_.IsProxyCacheable(
        properties, respect_vary, has_validator);
  }

  void CheckCookies(const ResponseHeaders& headers, StringPiece name,
                    StringPiece value, int64 expiration) {
    GoogleString expiration_string;
    ConvertTimeToString(expiration, &expiration_string);
    expiration_string = StrCat(" Expires=", expiration_string);
    StringPieceVector values;
    StringPieceVector attributes;
    EXPECT_TRUE(headers.HasCookie(name, &values, &attributes));
    EXPECT_EQ(1, values.size());
    EXPECT_EQ(value, values[0]);
    EXPECT_EQ(4, attributes.size());
    EXPECT_EQ(expiration_string, attributes[0]);
    EXPECT_EQ(" Domain=test.com", attributes[1]);
    EXPECT_EQ(" Path=/", attributes[2]);
    EXPECT_EQ(" HttpOnly", attributes[3]);
  }

  template<class Proto>
  void RemoveIfNotInOverrideWrapper(const Headers<Proto>& keep,
                                    Headers<Proto>* headers) {
    headers->RemoveIfNotIn(keep);
  }

  // Initiates a ResponseHeaders instance with the specified cache-control
  // value, calls SetCacheControlPublic, and returns the resulting cached
  // control as a joined string.
  GoogleString AddPublicToCacheControl(const StringVector& cache_control) {
    ResponseHeaders headers;
    for (int i = 0, n = cache_control.size(); i < n; ++i) {
      headers.Add(HttpAttributes::kCacheControl, cache_control[i]);
    }
    headers.SetCacheControlPublic();
    return headers.LookupJoined(HttpAttributes::kCacheControl);
  }

  GoogleMessageHandler message_handler_;
  ResponseHeaders response_headers_;
  ResponseHeadersParser parser_;

  // RequestHeaders with and without an 'Authorization:' header.
  RequestHeaders with_auth_;
  RequestHeaders without_auth_;

  GoogleString start_time_string_;
  GoogleString start_time_plus_5_minutes_string_;
  GoogleString start_time_plus_6_minutes_string_;
  const GoogleString max_age_300_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResponseHeadersTest);
};

// Parse the headers from google.com
TEST_F(ResponseHeadersTest, TestParseAndWrite) {
  const GoogleString http_data = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "X-Google-Experiment: 23729,24249,24253\r\n",
      "Date: ", start_time_string_, "\r\n",
      "Expires: -1\r\n"
      "Cache-Control: private, max-age=0\r\n"
      "Content-Type: text/html; charset=ISO-8859-1\r\n"
      "Set-Cookie: PREF=ID=3935f510d83d2a7a:TM=1270493386:LM=1270493386:S="
      "u_18e6r8aJ83N6P1; expires=Wed, 04-Apr-2012 18:49:46 GMT; path=/; do"
      "main=.google.com\r\n"
      "Set-Cookie: NID=33=aGkk7cKzznoUuCd19qTgXlBjXC8fc_luIo2Yk9BmrevUgXYP"
      "TazDF8Q6JvsO6LvTu4mfI8_44iIBLu4pF-Mvpe4wb7pYwej4q9HvbMLRxt-OzimIxmd"
      "-bwyYVfZ2PY1B; expires=Tue, 05-Oct-2010 18:49:46 GMT; path=/; domai"
      "n=.google.com; HttpOnly\r\n"
      "Server: gws\r\n"
      "X-XSS-Protection: 0\r\n"
      "ntend.gws/50,qyva4:80\r\n"
      "taticweb.staticfrontend.gws/50,qyva4:80\r\n"
      "X-Google-GFE-Response-Body-Transformations: gunzipped\r\n"
      "\r\n"
      "<!doctype html><html><head>"
      "<meta http-equiv=\"content-type\" content=\"");

  // Make a small buffer to test that we will successfully parse headers
  // that are split across buffers.  This is from
  //     wget --save-headers http://www.google.com
  const int bufsize = 100;
  int num_consumed = 0;
  for (int i = 0, n = http_data.size(); i < n; i += bufsize) {
    int size = std::min(bufsize, n - i);
    num_consumed += parser_.ParseChunk(StringPiece(http_data).substr(i, size),
                                       &message_handler_);
    if (parser_.headers_complete()) {
      break;
    }
  }

  // Verifies that after the headers, we see the content.  Note that this
  // test uses 'wget' style output, and wget takes care of any unzipping,
  // so this should not be mistaken for a content decoder, such as the
  // net/instaweb/latencylabs/http_response_serializer.h.
  static const char start_of_doc[] = "<!doctype html>";
  EXPECT_EQ(0, strncmp(start_of_doc, http_data.c_str() + num_consumed,
                       STATIC_STRLEN(start_of_doc)));
  CheckGoogleHeaders(response_headers_);

  // Now write the headers into a string.
  GoogleString outbuf;
  StringWriter writer(&outbuf);
  response_headers_.WriteAsHttp(&writer, &message_handler_);

  // Re-read into a fresh meta-data object and parse again.
  ResponseHeaders response_headers2;
  ResponseHeadersParser parser2(&response_headers2);
  num_consumed = parser2.ParseChunk(outbuf, &message_handler_);
  EXPECT_EQ(outbuf.size(), static_cast<size_t>(num_consumed));
  CheckGoogleHeaders(response_headers2);

  // Write the headers as binary into a string.
  outbuf.clear();
  response_headers_.WriteAsBinary(&writer, &message_handler_);

  // Re-read into a fresh meta-data object and compare.
  ResponseHeaders response_headers3;
  ASSERT_TRUE(response_headers3.ReadFromBinary(outbuf, &message_handler_));
  CheckGoogleHeaders(response_headers3);
}

TEST_F(ResponseHeadersTest, TestSizeEstimate) {
  GoogleString headers = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "Cache-control: max-age=300\r\n"
      "Date: ", start_time_string_, "\r\n",
      "X-Pagespeed: Fast\r\n"
      "\r\n");
  ParseHeaders(headers);
  EXPECT_EQ(headers.length(), response_headers_.SizeEstimate());
}

// Test caching header interpretation.  Note that the detailed testing
// of permutations is done in pagespeed/core/resource_util_test.cc.  We
// are just trying to ensure that we have populated the Resource object
// properly and that we have extracted the bits we need.
TEST_F(ResponseHeadersTest, TestCachingNeedDate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Cache-control: max-age=300\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

// Make sure we deal correctly when we have no Date or Cache-Control headers.
TEST_F(ResponseHeadersTest, TestNoHeaders) {
  ParseHeaders("HTTP/1.0 200 OK\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

// Corner case, bug noticed when we have Content-Type, but no Date header.
TEST_F(ResponseHeadersTest, TestNoContentTypeNoDate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Content-Type: text/css\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, TestNoContentTypeCacheNoDate) {
  ParseHeaders("HTTP/1.0 200 OK\r\n"
               "Content-Type: text/css\r\n"
               "Cache-Control: max-age=301\r\n\r\n");
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(0, response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, TestCachingPublic) {
  // In this test we'll leave the explicit "public" flag in to make sure
  // we can parse it.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n\r\n"));

  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_TRUE(IsProxyCacheable(with_auth_));
  EXPECT_TRUE(IsProxyCacheable(without_auth_));
  EXPECT_EQ(300 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.date_ms());
}

TEST_F(ResponseHeadersTest, TestCachingPartialReply) {
  // Make sure we don't cache a partial reply.
  ParseHeaders(StrCat("HTTP/1.0 206 Partial Reply\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n\r\n"));

  EXPECT_FALSE(response_headers_.IsBrowserCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_FALSE(IsProxyCacheable(with_auth_));
  EXPECT_FALSE(IsProxyCacheable(without_auth_));
}

// Private caching
TEST_F(ResponseHeadersTest, TestCachingPrivate) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: private, max-age=10\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_FALSE(IsProxyCacheable(with_auth_));
  EXPECT_FALSE(IsProxyCacheable(without_auth_));
  EXPECT_EQ(10 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.date_ms());
}

// Default caching (public unless request has authorization headers)
TEST_F(ResponseHeadersTest, TestCachingDefault) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=100\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_FALSE(IsProxyCacheable(with_auth_));
  EXPECT_TRUE(IsProxyCacheable(without_auth_));
  EXPECT_EQ(100 * 1000,
            response_headers_.CacheExpirationTimeMs() -
            response_headers_.date_ms());
}

// By default, cache permanent redirects.
TEST_F(ResponseHeadersTest, TestCachingDefaultPermRedirect) {
  ParseHeaders(StrCat("HTTP/1.1 301 Moved Permanently\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "\r\n"));
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
}

// Even when explicitly set, don't cache temporary redirects.
TEST_F(ResponseHeadersTest, TestCachingExplicitTempRedirect302) {
  ParseHeaders(StrCat("HTTP/1.1 302 Found\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=300\r\n"
                      "\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestCachingExplicitTempRedirect307) {
  ParseHeaders(StrCat("HTTP/1.1 307 Temporary Redirect\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=300\r\n"
                      "\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

// Test that we don't erroneously cache a 204 even though it is marked
// explicitly as cacheable. Note: We could cache this, but many status codes
// are only cacheable depending on precise input headers, to be cautious, we
// blacklist everything other than 200.
TEST_F(ResponseHeadersTest, TestCachingInvalidStatus) {
  ParseHeaders(StrCat("HTTP/1.0 204 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

// Test that we don't erroneously cache a 304.
// Note: Even though it claims to be publicly cacheable, that cacheability only
// applies to the response based on the precise request headers or it applies
// to the original 200 response.
TEST_F(ResponseHeadersTest, TestCachingNotModified) {
  ParseHeaders(StrCat("HTTP/1.0 304 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsBrowserCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

// Test that we don't cache an HTML file without explicit caching, but
// that we do cache images, css, and javascript.
TEST_F(ResponseHeadersTest, TestImplicitCache) {
  EXPECT_FALSE(ComputeImplicitCaching(200, "text/html"));
  EXPECT_FALSE(ComputeImplicitCaching(200, "unknown"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "text/javascript"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "text/css"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/jpeg"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/gif"));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/png"));

  EXPECT_FALSE(ComputeImplicitCaching(204, "text/html"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "unknown"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "text/javascript"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "text/css"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/jpeg"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/gif"));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/png"));
}

// Test that we don't cache an HTML file without explicit caching, but
// that we do cache images, css, and javascript.
TEST_F(ResponseHeadersTest, TestModifiedImplicitCache) {
  GoogleString max_age_500 = "max-age=500";
  GoogleString start_time_plus_implicit_ttl_string;
  ConvertTimeToString(MockTimer::kApr_5_2010_ms + 500 * Timer::kSecondMs,
                      &start_time_plus_implicit_ttl_string);
  response_headers_.set_implicit_cache_ttl_ms(500 * Timer::kSecondMs);

  EXPECT_FALSE(ComputeImplicitCaching(200, "text/html", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_FALSE(ComputeImplicitCaching(200, "unknown", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_TRUE(ComputeImplicitCaching(200, "text/javascript", max_age_500,
                                     start_time_plus_implicit_ttl_string));
  EXPECT_TRUE(ComputeImplicitCaching(200, "text/css", max_age_500,
                                     start_time_plus_implicit_ttl_string));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/jpeg", max_age_500,
                                     start_time_plus_implicit_ttl_string));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/gif", max_age_500,
                                     start_time_plus_implicit_ttl_string));
  EXPECT_TRUE(ComputeImplicitCaching(200, "image/png", max_age_500,
                                     start_time_plus_implicit_ttl_string));

  EXPECT_FALSE(ComputeImplicitCaching(204, "text/html", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_FALSE(ComputeImplicitCaching(204, "unknown", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_FALSE(ComputeImplicitCaching(204, "text/javascript", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_FALSE(ComputeImplicitCaching(204, "text/css", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/jpeg", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/gif", max_age_500,
                                      start_time_plus_implicit_ttl_string));
  EXPECT_FALSE(ComputeImplicitCaching(204, "image/png", max_age_500,
                                      start_time_plus_implicit_ttl_string));
}

TEST_F(ResponseHeadersTest, TestSetCookieCacheabilityForHtml) {
  // HTML is cacheable if there are explicit caching directives, but no
  // Set-Cookie headers.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/html\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  // HTML is not cacheable if there is a Set-Cookie header even though there are
  // explicit caching directives.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/html\r\n"
                      "Set-Cookie: cookie\r\n"
                      "Set-Cookie: cookie2\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  // HTML is not cacheable if there is a Set-Cookie2 header even though there
  // are explicit caching directives.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/html\r\n"
                      "Set-Cookie2: cookie\r\n"
                      "Set-Cookie2: cookie2\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestSetCookieCacheabilityForNonHtml) {
  // CSS is cacheable if there are explicit caching directives, but no
  // Set-Cookie headers.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/css\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  // CSS is still cacheable even if there is a Set-Cookie.
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Content-Type: text/css\r\n"
                      "Set-Cookie: cookie\r\n"
                      "Set-Cookie: cookie2\r\n"
                      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestSetCookieCacheabilityFor301Redirect) {
  // 301 Redirects are cacheable if there are explicit caching directives, but
  // no Set-Cookie headers.
  ParseHeaders(StrCat(
      "HTTP/1.0 301 Moved Permanently\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsProxyCacheable());

  // 301 Redirects are cacheable if there are no caching directives and no
  // Set-Cookie headers.
  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 301 Moved Permanently\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsProxyCacheable());

  // 301 Redirects are not cacheable if there are cookies.
  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 301 Moved Permanently\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie: cookie\r\n"
      "Set-Cookie: cookie2\r\n"
      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 301 Moved Permanently\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie2: cookie\r\n"
      "Set-Cookie2: cookie2\r\n"
      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  // 301 Redirects are not cacheable if there are cookies and no caching
  // headers.
  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 301 Moved Permanently\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie: cookie\r\n"
      "Set-Cookie: cookie2\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 301 Moved Permanently\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie2: cookie\r\n"
      "Set-Cookie2: cookie2\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestSetCookieCacheabilityFor302Redirect) {
  // 302 Redirects are not cacheable if there are explicit caching directives,
  // but no Set-Cookie headers.
  ParseHeaders(StrCat(
      "HTTP/1.0 302 Moved\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  // 302 Redirects are not cacheable if there are no caching directives and no
  // Set-Cookie headers.
  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 302 Moved\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  // 302 Redirects are not cacheable if there are cookies.
  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 302 Moved\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie: cookie\r\n"
      "Set-Cookie: cookie2\r\n"
      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 302 Moved\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie2: cookie\r\n"
      "Set-Cookie2: cookie2\r\n"
      "Cache-control: max-age=300\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  // 302 Redirects are not cacheable if there are cookies and no caching
  // headers.
  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 302 Moved\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie: cookie\r\n"
      "Set-Cookie: cookie2\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());

  response_headers_.Clear();
  ParseHeaders(StrCat(
      "HTTP/1.0 302 Moved\r\n"
      "Date: ",
      start_time_string_,
      "\r\n"
      "Location: http://www.foo.com/\r\n"
      "Set-Cookie2: cookie\r\n"
      "Set-Cookie2: cookie2\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, GetSanitizedProto) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Set-Cookie: CG=US:CA:Mountain+View\r\n"
                      "Set-Cookie: UA=chrome\r\n"
                      "Cache-Control: max-age=100\r\n"
                      "Set-Cookie: path=/\r\n"
                      "Vary: User-Agent\r\n"
                      "Set-Cookie2: LA=1275937193\r\n"
                      "Vary: Accept-Encoding\r\n"
                      "Connection: Foo, bar, Connection, Keep-Alive, "
                      "Cache-Control,, foo\r\n"
                      "foo: bar\r\n"
                      "bar: foo\r\n"
                      "ShouldRemain: foo\r\n"
                      "\r\n"));
  HttpResponseHeaders proto;
  response_headers_.GetSanitizedProto(&proto);
  ASSERT_EQ(proto.header_size(), 5);
  EXPECT_EQ(proto.header(0).name(), HttpAttributes::kDate);
  // Cache-Control is an end-to-end header, and should not be sanitized even
  // though it is referenced in the Connection: header.
  EXPECT_EQ(proto.header(1).name(), HttpAttributes::kCacheControl);
  EXPECT_EQ(proto.header(1).value(), "max-age=100");
  EXPECT_EQ(proto.header(2).name(), HttpAttributes::kVary);
  EXPECT_EQ(proto.header(2).value(), "User-Agent");
  EXPECT_EQ(proto.header(3).name(), HttpAttributes::kVary);
  EXPECT_EQ(proto.header(3).value(), "Accept-Encoding");
  EXPECT_EQ(proto.header(4).name(), "ShouldRemain");
  EXPECT_EQ(proto.header(4).value(), "foo");
  EXPECT_EQ(proto.status_code(), 200);
}

TEST_F(ResponseHeadersTest, TestRemoveAll) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Set-Cookie: CG=US:CA:Mountain+View\r\n"
                      "Set-Cookie: UA=chrome\r\n"
                      "Cache-Control: max-age=100\r\n"
                      "Set-Cookie: path=/\r\n"
                      "Vary: User-Agent\r\n"
                      "Set-Cookie: LA=1275937193\r\n"
                      "Vary: Accept-Encoding\r\n"
                      "\r\n"));
  ConstStringStarVector vs;
  ExpectSizes(8, 4);

  // Removing a header which isn't there removes nothing and returns false.
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kLocation, &vs));
  EXPECT_FALSE(response_headers_.RemoveAll(HttpAttributes::kLocation));
  ExpectSizes(8, 4);

  // Removing a headers which is there works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kVary));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  ExpectSizes(6, 3);

  // Removing something which has already been removed has no effect.
  EXPECT_FALSE(response_headers_.RemoveAll(HttpAttributes::kVary));
  ExpectSizes(6, 3);

  // Remove the rest one-by-one.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kSetCookie));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));
  ExpectSizes(2, 2);
  EXPECT_EQ(2, response_headers_.NumAttributes());

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kDate, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kDate));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kDate, &vs));
  ExpectSizes(1, 1);

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kCacheControl));
  ExpectSizes(0, 0);
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
}

TEST_F(ResponseHeadersTest, TestRemoveAllFromSortedArray) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Set-Cookie: CG=US:CA:Mountain+View\r\n"
                      "Set-Cookie: UA=chrome\r\n"
                      "Cache-Control: max-age=100\r\n"
                      "Set-Cookie: path=/\r\n"
                      "Vary: User-Agent\r\n"
                      "Set-Cookie: LA=1275937193\r\n"
                      "Vary: Accept-Encoding\r\n"
                      "\r\n"));
  ConstStringStarVector vs;
  ExpectSizes(8, 4);

  // Empty set means remove nothing and return false.
  EXPECT_FALSE(response_headers_.RemoveAllFromSortedArray(NULL, 0));
  ExpectSizes(8, 4);

  // Removing headers which aren't there removes nothing and returns false.
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kLocation, &vs));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kGzip, &vs));
  const StringPiece removes0[] = {
    HttpAttributes::kGzip,
    HttpAttributes::kLocation,
  };
  EXPECT_FALSE(response_headers_.RemoveAllFromSortedArray(
      removes0, arraysize(removes0)));
  ExpectSizes(8, 4);

  // Removing multiple headers works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));
  const StringPiece removes1[] = {
    HttpAttributes::kSetCookie,
    HttpAttributes::kVary,
  };
  EXPECT_TRUE(response_headers_.RemoveAllFromSortedArray(
      removes1, arraysize(removes1)));
  ExpectSizes(2, 2);
  EXPECT_EQ(2, response_headers_.NumAttributes());
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kVary, &vs));
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kSetCookie, &vs));

  // Removing something which has already been removed has no effect.
  EXPECT_FALSE(response_headers_.RemoveAllFromSortedArray(
      removes1, arraysize(removes1)));
  ExpectSizes(2, 2);

  // Removing one header works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kDate, &vs));
  static StringPiece removes2[] = {
    HttpAttributes::kDate
  };
  EXPECT_TRUE(response_headers_.RemoveAllFromSortedArray(
      removes2, arraysize(removes2)));
  ExpectSizes(1, 1);
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kDate, &vs));

  // Removing a header that is there after one that isn't works.
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
  const StringPiece removes3[] = {
    HttpAttributes::kCacheControl,
    "X-Bogus-Attribute",
  };
  EXPECT_TRUE(response_headers_.RemoveAllFromSortedArray(
      removes3, arraysize(removes3)));
  ExpectSizes(0, 0);
  EXPECT_FALSE(response_headers_.Lookup(HttpAttributes::kCacheControl, &vs));
}

TEST_F(ResponseHeadersTest, TestRemoveIfNotIn) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Set-Cookie: CG=US:CA:Mountain+View\r\n"
                      "Set-Cookie: UA=chrome\r\n"
                      "Set-Cookie: UA=chrome\r\n"  // kept: 2 in keep_set.
                      "Set-Cookie: UA=chrome\r\n"  // pruned: bag will be empty.
                      "Cache-Control: max-age=100, private, must-revalidate\r\n"
                      "Set-Cookie: path=/\r\n"
                      "Vary: User-Agent,User-Agent,User-Agent\r\n"  // keep 2/3
                      "Set-Cookie: LA=1275937193\r\n"
                      "\r\n"));
  ResponseHeaders keep_set;
  keep_set.Add(HttpAttributes::kSetCookie, "UA=chrome");
  keep_set.Add(HttpAttributes::kSetCookie, "UA=chrome");
  keep_set.Add(HttpAttributes::kSetCookie, "LA=1275937193");
  keep_set.Add(HttpAttributes::kVary, "User-Agent, User-Agent");
  keep_set.Add("cache-control", "max-age=100");  // case-insensitive.
  keep_set.Add("CACHE-CONTROL", "must-revalidate");
  keep_set.Add("not-in-original", "won't-be-added");
  response_headers_.ComputeCaching();
  EXPECT_FALSE(ResponseCachingDirty());
  RemoveIfNotInOverrideWrapper(keep_set, &response_headers_);
  EXPECT_TRUE(ResponseCachingDirty());
  ExpectSizes(5, 3);
  EXPECT_TRUE(response_headers_.HasValue(HttpAttributes::kCacheControl,
                                         "max-age=100"));
  EXPECT_TRUE(response_headers_.HasValue(HttpAttributes::kCacheControl,
                                         "must-revalidate"));
  EXPECT_TRUE(response_headers_.HasValue(HttpAttributes::kSetCookie,
                                         "LA=1275937193"));
  EXPECT_TRUE(response_headers_.HasValue(HttpAttributes::kSetCookie,
                                         "UA=chrome"));
  EXPECT_TRUE(response_headers_.HasValue(HttpAttributes::kVary,
                                         "User-Agent"));
  EXPECT_FALSE(response_headers_.HasValue(HttpAttributes::kCacheControl,
                                          "private"));
  EXPECT_FALSE(response_headers_.HasValue(HttpAttributes::kSetCookie,
                                          "CG=US:CA:Mountain+View"));
  EXPECT_FALSE(response_headers_.Has("Date"));
  EXPECT_STREQ(
      "HTTP/1.0 200 OK\r\n"
      "Set-Cookie: UA=chrome\r\n"
      "Set-Cookie: UA=chrome\r\n"  // kept: 2 in keep_set.
      "Cache-Control: max-age=100, must-revalidate\r\n"
      "Vary: User-Agent, User-Agent\r\n"  // note third 'User-Agent' is gone.
      "Set-Cookie: LA=1275937193\r\n"
      "\r\n",
      response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestReasonPhrase) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  EXPECT_EQ(HttpStatus::kOK, response_headers_.status_code());
  EXPECT_EQ(GoogleString("OK"),
            GoogleString(response_headers_.reason_phrase()));
}

TEST_F(ResponseHeadersTest, TestReasonPhraseMissing) {
  static const char kText[] = "HTTP/1.0 200\r\nContent-type: text/html\r\n\r\n";
  ParseHeaders(kText);
  EXPECT_EQ(HttpStatus::kOK, response_headers_.status_code());
  EXPECT_STREQ("OK", response_headers_.reason_phrase());
}

TEST_F(ResponseHeadersTest, TestReasonPhraseHasOnlySpace) {
  static const char kText[] =
      "HTTP/1.0 200 \r\nContent-type: text/html\r\n\r\n";
  ParseHeaders(kText);
  EXPECT_EQ(HttpStatus::kOK, response_headers_.status_code());
  EXPECT_STREQ("OK", response_headers_.reason_phrase());
}

TEST_F(ResponseHeadersTest, TestReasonPhraseBogusCode) {
  static const char kText[] =
      "HTTP/1.0 6765 \r\nContent-type: text/html\r\n\r\n";
  ParseHeaders(kText);
  EXPECT_EQ(6765, response_headers_.status_code());
  EXPECT_STREQ("Internal Server Error", response_headers_.reason_phrase());
}

TEST_F(ResponseHeadersTest, TestSetDate) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=100");
  ConstStringStarVector date;
  ASSERT_TRUE(response_headers_.Lookup("Date", &date));
  EXPECT_EQ(1, date.size());
  response_headers_.ComputeCaching();
  const int64 k100_sec = 100 * 1000;
  ASSERT_EQ(MockTimer::kApr_5_2010_ms + k100_sec,
            response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, TestUpdateFrom) {
  const char old_header_string[] =
      "HTTP/1.1 200 OK\r\n"
      "Date: Fri, 22 Apr 2011 19:34:33 GMT\r\n"
      "Server: Apache/2.2.3 (CentOS)\r\n"
      "Last-Modified: Tue, 08 Mar 2011 18:28:32 GMT\r\n"
      "Accept-Ranges: bytes\r\n"
      "Content-Length: 241260\r\n"
      "Cache-control: public, max-age=600\r\n"
      "Content-Type: image/jpeg\r\n"
      "\r\n";
  const char new_header_string[] =
      "HTTP/1.1 304 Not Modified\r\n"
      "Date: Fri, 22 Apr 2011 19:49:59 GMT\r\n"
      "Server: Apache/2.2.3 (CentOS)\r\n"
      "Cache-control: public, max-age=3600\r\n"
      "Set-Cookie: LA=1275937193\r\n"
      "Set-Cookie: UA=chrome\r\n"
      "\r\n";
  const char expected_merged_header_string[] =
      "HTTP/1.1 200 OK\r\n"
      "Last-Modified: Tue, 08 Mar 2011 18:28:32 GMT\r\n"
      "Accept-Ranges: bytes\r\n"
      "Content-Length: 241260\r\n"
      "Content-Type: image/jpeg\r\n"
      "Date: Fri, 22 Apr 2011 19:49:59 GMT\r\n"
      "Server: Apache/2.2.3 (CentOS)\r\n"
      "Cache-control: public, max-age=3600\r\n"
      "Set-Cookie: LA=1275937193\r\n"
      "Set-Cookie: UA=chrome\r\n"
      "\r\n";

  // Setup old and new headers
  ResponseHeaders old_headers, new_headers;
  ResponseHeadersParser old_parser(&old_headers), new_parser(&new_headers);
  old_parser.ParseChunk(old_header_string, &message_handler_);
  new_parser.ParseChunk(new_header_string, &message_handler_);

  // Update old_headers from new_headers.
  old_headers.UpdateFrom(new_headers);

  // Make sure in memory map is updated.
  ConstStringStarVector date_strings;
  EXPECT_TRUE(old_headers.Lookup("Date", &date_strings));
  EXPECT_EQ(1, date_strings.size());
  EXPECT_EQ("Fri, 22 Apr 2011 19:49:59 GMT", *date_strings[0]);
  ConstStringStarVector set_cookie_strings;
  EXPECT_TRUE(old_headers.Lookup(HttpAttributes::kSetCookie,
                                 &set_cookie_strings));
  EXPECT_EQ(8, old_headers.NumAttributeNames());

  // Make sure protobuf is updated.
  GoogleString actual_merged_header_string;
  StringWriter merged_writer(&actual_merged_header_string);
  old_headers.WriteAsHttp(&merged_writer, &message_handler_);

  EXPECT_EQ(expected_merged_header_string, actual_merged_header_string);
}

TEST_F(ResponseHeadersTest, TestCachingVaryStar) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Vary: *\r\n\r\n\r\n"));
  EXPECT_FALSE(response_headers_.IsProxyCacheable());
  EXPECT_FALSE(response_headers_.IsProxyCacheable(
      RequestHeaders::Properties(),
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kNoValidator));
  EXPECT_FALSE(response_headers_.IsProxyCacheable(
      RequestHeaders::Properties(),
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kNoValidator));
}

TEST_F(ResponseHeadersTest, TestCachingVaryCookieNonHtml) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Vary: Cookie\r\n\r\n\r\n"));
  // Verify that all 16 combinations of having cookies, cookie2, respecting
  // and ignoring vary, and claiming a validator, result in this pattern
  // being uncacheable.
  for (int has_cookie = 0; has_cookie < 2; ++has_cookie) {
    for (int has_cookie2 = 0; has_cookie2 < 2; ++has_cookie2) {
      for (int vary = 0; vary < 2; ++vary) {
        for (int validator = 0; validator < 2; ++validator) {
          EXPECT_FALSE(IsVaryCacheable(
              has_cookie != 0,
              has_cookie2 != 0,
              ResponseHeaders::GetVaryOption(vary != 0),
              (validator != 0) ? ResponseHeaders::kHasValidator
              : ResponseHeaders::kNoValidator));
        }
      }
    }
  }
}

TEST_F(ResponseHeadersTest, TestCachingVaryCookieHtml) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Content-Type: text/html\r\n"
                      "Vary: Cookie\r\n\r\n\r\n"));
  EXPECT_FALSE(IsVaryCacheable(
      true,   // has_cookie
      false,  // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_TRUE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_FALSE(IsVaryCacheable(
      true,   // has_cookie
      false,  // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_TRUE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kHasValidator));

  EXPECT_FALSE(IsVaryCacheable(
      true,   // has_cookie
      false,  // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kNoValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kNoValidator));
  EXPECT_FALSE(IsVaryCacheable(
      true,   // has_cookie
      false,  // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kNoValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kNoValidator));
}

TEST_F(ResponseHeadersTest, TestCachingVaryCookie2Html) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Content-Type: text/html\r\n"
                      "Vary: Cookie2\r\n\r\n\r\n"));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      true,    // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_TRUE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      true,    // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_TRUE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kHasValidator));

  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      true,    // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kNoValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kNoValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      true,    // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kNoValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kIgnoreVaryOnResources,
      ResponseHeaders::kNoValidator));
}

TEST_F(ResponseHeadersTest, TestCachingVaryCookieUserAgent) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Vary: Cookie,User-Agent\r\n\r\n\r\n"));
  EXPECT_FALSE(IsVaryCacheable(
      true,   // has_cookie
      false,  // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
}

TEST_F(ResponseHeadersTest, TestCachingVaryAcceptEncoding) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Vary: Accept-Encoding\r\n\r\n\r\n"));
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_TRUE(IsVaryCacheable(
      true,    // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_TRUE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
}

TEST_F(ResponseHeadersTest, TestCachingVaryAcceptEncodingCookieNonHtml) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Vary: Accept-Encoding,Cookie\r\n\r\n\r\n"));
  EXPECT_FALSE(IsVaryCacheable(
      true,    // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_FALSE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
}

TEST_F(ResponseHeadersTest, TestCachingVaryAcceptEncodingCookieHtml) {
  ParseHeaders(StrCat("HTTP/1.0 200 OK\r\n"
                      "Date: ", start_time_string_, "\r\n"
                      "Cache-control: public, max-age=300\r\n"
                      "Content-Type: text/html\r\n"
                      "Vary: Accept-Encoding,Cookie\r\n\r\n\r\n"));
  EXPECT_FALSE(IsVaryCacheable(
      true,    // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
  EXPECT_TRUE(IsVaryCacheable(
      false,   // has_cookie
      false,   // has_cookie2
      ResponseHeaders::kRespectVaryOnResources,
      ResponseHeaders::kHasValidator));
}

TEST_F(ResponseHeadersTest, TestSetDateAndCaching) {
  response_headers_.SetDateAndCaching(MockTimer::kApr_5_2010_ms,
                                      6 * Timer::kMinuteMs);
  const GoogleString expected_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, CommaSeparatedEmptyValues) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: \r\n"
      "Vary: Accept-Encoding, User-Agent\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  EXPECT_FALSE(response_headers_.RequiresBrowserRevalidation());
  EXPECT_FALSE(response_headers_.RequiresProxyRevalidation());
  EXPECT_TRUE(response_headers_.Has(HttpAttributes::kCacheControl));
  EXPECT_STREQ("", response_headers_.Lookup1(HttpAttributes::kCacheControl));

  response_headers_.Clear();
  response_headers_.Add(HttpAttributes::kCacheControl, "");
  EXPECT_TRUE(response_headers_.Has(HttpAttributes::kCacheControl));
}

TEST_F(ResponseHeadersTest, TestReserializingCommaValues) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360, private, must-revalidate\r\n"
      "Vary: Accept-Encoding, User-Agent\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kCacheControl, &values);
  EXPECT_EQ(3, values.size());
  EXPECT_TRUE(response_headers_.RequiresBrowserRevalidation());
  EXPECT_TRUE(response_headers_.RequiresProxyRevalidation());
  values.clear();
  response_headers_.Lookup(HttpAttributes::kVary, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_EQ(comma_headers, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestMustRevalidate) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 200 (OK)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Cache-Control: max-age=360, must-revalidate\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  EXPECT_TRUE(response_headers_.RequiresBrowserRevalidation());
  EXPECT_TRUE(response_headers_.RequiresProxyRevalidation());
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestRequiresProxyRevalidation) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 200 (OK)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Cache-Control: max-age=360, proxy-revalidate\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  EXPECT_FALSE(response_headers_.RequiresBrowserRevalidation());
  EXPECT_TRUE(response_headers_.RequiresProxyRevalidation());
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
}

TEST_F(ResponseHeadersTest, TestProxyAndMustRevalidate) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 200 (OK)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Cache-Control: max-age=360, must-revalidate, proxy-revalidate\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  EXPECT_TRUE(response_headers_.RequiresBrowserRevalidation());
  EXPECT_TRUE(response_headers_.RequiresProxyRevalidation());
  EXPECT_TRUE(response_headers_.IsBrowserCacheable());
  EXPECT_TRUE(response_headers_.IsProxyCacheable());
}

// There was a bug that calling RemoveAll would re-populate the proto from
// map_ which would separate all comma-separated values.
TEST_F(ResponseHeadersTest, TestRemoveDoesntSeparateCommaValues) {
  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=0, no-cache");
  response_headers_.Add(HttpAttributes::kSetCookie, "blah");
  response_headers_.Add(HttpAttributes::kVary, "Accept-Encoding, Cookie");

  // 1) RemoveAll
  EXPECT_TRUE(response_headers_.RemoveAll(HttpAttributes::kSetCookie));

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kCacheControl, &values));
  EXPECT_EQ(2, values.size());
  values.clear();
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(2, values.size());

  const char expected_headers[] =
      "HTTP/1.0 0 (null)\r\n"
      "Cache-Control: max-age=0, no-cache\r\n"
      "Vary: Accept-Encoding, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  // 2) Remove
  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kVary, "Cookie"));

  const char expected_headers2[] =
      "HTTP/1.0 0 (null)\r\n"
      "Cache-Control: max-age=0, no-cache\r\n"
      "Vary: Accept-Encoding\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers2, response_headers_.ToString());

  // 3) RemoveAllFromSortedArray
  const StringPiece remove_vector[] = {
    HttpAttributes::kVary,
  };
  EXPECT_TRUE(response_headers_.RemoveAllFromSortedArray(
      remove_vector, arraysize(remove_vector)));

  const char expected_headers3[] =
      "HTTP/1.0 0 (null)\r\n"
      "Cache-Control: max-age=0, no-cache\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers3, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestKeepSeparateCommaValues) {
  response_headers_.Add(HttpAttributes::kVary, "Accept-Encoding");
  response_headers_.Add(HttpAttributes::kVary, "User-Agent");
  response_headers_.Add(HttpAttributes::kVary, "Cookie");

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(3, values.size());

  // We keep values separate by default.
  const char expected_headers[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding\r\n"
      "Vary: User-Agent\r\n"
      "Vary: Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kVary, "User-Agent"));

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(2, values.size());

  // But they are combined after a Remove.
  //
  // NOTE: This is mostly to document current behavior. Feel free to re-gold
  // this if you update the Remove method to not combine headers.
  const char expected_headers2[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers2, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestKeepTogetherCommaValues) {
  response_headers_.Add(HttpAttributes::kVary,
                        "Accept-Encoding, User-Agent, Cookie");

  ConstStringStarVector values;
  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(3, values.size());

  const char expected_headers[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding, User-Agent, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kVary, "User-Agent"));

  EXPECT_TRUE(response_headers_.Lookup(HttpAttributes::kVary, &values));
  EXPECT_EQ(2, values.size());

  const char expected_headers2[] =
      "HTTP/1.0 0 (null)\r\n"
      "Vary: Accept-Encoding, Cookie\r\n"
      "\r\n";
  EXPECT_EQ(expected_headers2, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestGzipped) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: deflate, gzip\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kContentEncoding, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_TRUE(response_headers_.IsGzipped());
  EXPECT_TRUE(response_headers_.WasGzippedLast());
}

TEST_F(ResponseHeadersTest, TestGzippedNotLast) {
  const GoogleString comma_headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: gzip, deflate\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(comma_headers);
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kContentEncoding, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_TRUE(response_headers_.IsGzipped());
  EXPECT_FALSE(response_headers_.WasGzippedLast());
}

TEST_F(ResponseHeadersTest, TestRemove) {
  const GoogleString headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: chunked, deflate, chunked, gzip\r\n"
      "\r\n");
  const GoogleString headers_removed = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "Content-Encoding: deflate, gzip\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kContentEncoding,
                                       "chunked"));
  EXPECT_EQ(headers_removed, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestRemoveConcat) {
  const GoogleString headers = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Content-Encoding: gzip\r\n"
      "\r\n");
  const GoogleString headers_removed = StrCat(
      "HTTP/1.0 0 (null)\r\n"
      "Date: ", start_time_string_, "\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_TRUE(response_headers_.Remove(HttpAttributes::kContentEncoding,
                                       "gzip"));
  EXPECT_EQ(headers_removed, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, TestParseFirstLineOk) {
  response_headers_.ParseFirstLine("HTTP/1.0 200 OK");
  EXPECT_EQ(1, response_headers_.major_version());
  EXPECT_EQ(0, response_headers_.minor_version());
  EXPECT_EQ(200, response_headers_.status_code());
  EXPECT_EQ(GoogleString("OK"),
            GoogleString(response_headers_.reason_phrase()));
}

TEST_F(ResponseHeadersTest, TestParseFirstLinePermanentRedirect) {
  response_headers_.ParseFirstLine("HTTP/1.1 301 Moved Permanently");
  EXPECT_EQ(1, response_headers_.major_version());
  EXPECT_EQ(1, response_headers_.minor_version());
  EXPECT_EQ(301, response_headers_.status_code());
  EXPECT_EQ(GoogleString("Moved Permanently"),
            GoogleString(response_headers_.reason_phrase()));
}

TEST_F(ResponseHeadersTest, RemoveAllCaseInsensitivity) {
  ResponseHeaders headers;
  headers.Add("content-encoding", "gzip");
  EXPECT_STREQ("gzip", headers.Lookup1("Content-Encoding"));
  headers.RemoveAll("Content-Encoding");
  EXPECT_EQ(NULL, headers.Lookup1("content-encoding"));
  EXPECT_EQ(NULL, headers.Lookup1("Content-Encoding"));
  EXPECT_EQ(0, headers.NumAttributes()) << headers.Name(0);
}

TEST_F(ResponseHeadersTest, DetermineContentType) {
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_EQ(&kContentTypePng, response_headers_.DetermineContentType());
}

TEST_F(ResponseHeadersTest, DetermineContentTypeMulti) {
  // Per the mime sniffing spec, the -last- content-type header wins.
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/webp\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_EQ(&kContentTypeWebp, response_headers_.DetermineContentType());

  static const char headers2[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: nonsense\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers2);
  EXPECT_EQ(NULL, response_headers_.DetermineContentType());
}

TEST_F(ResponseHeadersTest, DetermineContentTypeWithCharset) {
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);
  EXPECT_EQ(&kContentTypeHtml, response_headers_.DetermineContentType());

  const ContentType* content_type;
  GoogleString charset;
  response_headers_.DetermineContentTypeAndCharset(&content_type, &charset);
  EXPECT_EQ(&kContentTypeHtml, content_type);
  EXPECT_STREQ("UTF-8", charset);
}

TEST_F(ResponseHeadersTest, DetermineContentTypeAndCharsetNonExisting) {
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);

  const ContentType* content_type = &kContentTypeHtml;
  GoogleString charset = "EBCDIC";
  response_headers_.DetermineContentTypeAndCharset(&content_type, &charset);
  EXPECT_EQ(NULL, content_type);
  EXPECT_TRUE(charset.empty());
}

TEST_F(ResponseHeadersTest, DetermineCharset) {
  static const char headers_no_charset[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers_no_charset);
  EXPECT_TRUE(response_headers_.DetermineCharset().empty());

  static const char headers_with_charset[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png; charset=utf-8\r\n"
      "Content-Type: image/png; charset=koi8-r\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers_with_charset);
  EXPECT_EQ("koi8-r", response_headers_.DetermineCharset());

  // We take the charset that goes with the last content-type
  // header, since that's the one that matches.
  static const char multiple_headers_with_charset[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png; charset=iso-8859-1\r\n"
      "Content-Type: image/png\r\n"
      "Content-Type: image/png; charset=utf-8\r\n"
      "Content-Type: image/png\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(multiple_headers_with_charset);
  EXPECT_TRUE(response_headers_.DetermineCharset().empty());
}

TEST_F(ResponseHeadersTest, FixupMissingDate) {
  static const char headers[] =
      "HTTP/1.1 200 OK\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n";
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.FixDateHeaders(MockTimer::kApr_5_2010_ms);
  response_headers_.ComputeCaching();
  EXPECT_EQ(MockTimer::kApr_5_2010_ms, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, LastModifiedAsInt64) {
  response_headers_.Clear();
  EXPECT_FALSE(response_headers_.has_last_modified_time_ms());
  response_headers_.SetLastModified(MockTimer::kApr_5_2010_ms);
  response_headers_.ComputeCaching();
  EXPECT_STREQ("Mon, 05 Apr 2010 18:51:26 GMT", response_headers_.Lookup1(
      HttpAttributes::kLastModified));
  EXPECT_EQ(MockTimer::kApr_5_2010_ms,
            response_headers_.last_modified_time_ms());
  EXPECT_TRUE(response_headers_.has_last_modified_time_ms());
}

TEST_F(ResponseHeadersTest, DoNotCorrectValidDate) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock back by 1 second will not affect the date.
  int64 prev_date = response_headers_.date_ms();
  response_headers_.FixDateHeaders(prev_date - 1000);
  EXPECT_EQ(prev_date, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, FixupStaleDate) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock *forward* by 1 second *will* affect the date.
  int64 new_date = response_headers_.date_ms() + 1000;
  response_headers_.FixDateHeaders(new_date);
  EXPECT_EQ(new_date, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, FixupStaleDateWithExpires) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date:    ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_5_minutes_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock *forward* by 1 second *will* affect the date, and
  // also push the Expires along with it.
  int64 orig_date = response_headers_.date_ms();
  ASSERT_EQ(orig_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
  int64 new_date = orig_date + 1000;

  response_headers_.FixDateHeaders(new_date);
  EXPECT_EQ(new_date, response_headers_.date_ms());
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) != NULL);
  EXPECT_EQ(new_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, FixupStaleDateWithMaxAge) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Date:    ", start_time_string_, "\r\n"
      "Cache-Control: max-age=300\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  // Setting clock *forward* by 1 second *will* affect the date, and
  // also push the Expires along with it.
  int64 orig_date = response_headers_.date_ms();
  ASSERT_EQ(orig_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
  ASSERT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
  int64 new_date = orig_date + 1000;

  response_headers_.FixDateHeaders(new_date);
  EXPECT_EQ(new_date, response_headers_.date_ms());

  // Still no Expires entry, but the cache expiration time is still 5 minutes.
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
  EXPECT_EQ(new_date + 5 * Timer::kMinuteMs,
            response_headers_.CacheExpirationTimeMs());
}

TEST_F(ResponseHeadersTest, MissingDateRemoveExpires) {
  const GoogleString headers = StrCat(
      "HTTP/1.1 200 OK\r\n"
      "Expires: ", start_time_plus_5_minutes_string_, "\r\n"
      "Content-Type: text/html; charset=UTF-8\r\n"
      "\r\n");
  response_headers_.Clear();
  ParseHeaders(headers);
  response_headers_.ComputeCaching();

  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kDate) == NULL);
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) != NULL);
  response_headers_.FixDateHeaders(MockTimer::kApr_5_2010_ms);
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kDate) != NULL);
  EXPECT_TRUE(response_headers_.Lookup1(HttpAttributes::kExpires) == NULL);
}

TEST_F(ResponseHeadersTest, TestSetCacheControlMaxAge) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=0, no-cache");
  response_headers_.ComputeCaching();

  response_headers_.SetCacheControlMaxAge(300 * Timer::kSecondMs);

  const GoogleString expected_headers = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_5_minutes_string_, "\r\n"
      "Cache-Control: max-age=300,no-cache\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers, response_headers_.ToString());

  response_headers_.RemoveAll(HttpAttributes::kCacheControl);
  response_headers_.ComputeCaching();

  response_headers_.SetCacheControlMaxAge(360 * Timer::kSecondMs);
  GoogleString expected_headers2 = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers2, response_headers_.ToString());

  response_headers_.RemoveAll(HttpAttributes::kCacheControl);
  response_headers_.Add(HttpAttributes::kCacheControl,
                        "max-age=10,private,no-cache,max-age=20,max-age=30");
  response_headers_.ComputeCaching();

  response_headers_.SetCacheControlMaxAge(360 * Timer::kSecondMs);
  GoogleString expected_headers3 = StrCat(
      "HTTP/1.0 200 OK\r\n"
      "Date: ", start_time_string_, "\r\n"
      "Expires: ", start_time_plus_6_minutes_string_, "\r\n"
      "Cache-Control: max-age=360,private,no-cache\r\n"
      "\r\n");
  EXPECT_EQ(expected_headers3, response_headers_.ToString());
}

TEST_F(ResponseHeadersTest, CheckErrorCodes) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  EXPECT_FALSE(response_headers_.IsErrorStatus());
  EXPECT_FALSE(response_headers_.IsServerErrorStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kNotModified);
  EXPECT_FALSE(response_headers_.IsErrorStatus());
  EXPECT_FALSE(response_headers_.IsServerErrorStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kNotFound);
  EXPECT_TRUE(response_headers_.IsErrorStatus());
  EXPECT_FALSE(response_headers_.IsServerErrorStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kInternalServerError);
  EXPECT_TRUE(response_headers_.IsErrorStatus());
  EXPECT_TRUE(response_headers_.IsServerErrorStatus());
}

TEST_F(ResponseHeadersTest, CheckRedirectStatus) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  EXPECT_FALSE(response_headers_.IsRedirectStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kNotModified);
  EXPECT_FALSE(response_headers_.IsRedirectStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kBadRequest);
  EXPECT_FALSE(response_headers_.IsRedirectStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kBadRequest);
  EXPECT_FALSE(response_headers_.IsRedirectStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kMovedPermanently);
  EXPECT_TRUE(response_headers_.IsRedirectStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kTemporaryRedirect);
  EXPECT_TRUE(response_headers_.IsRedirectStatus());

  response_headers_.SetStatusAndReason(HttpStatus::kFound);
  EXPECT_TRUE(response_headers_.IsRedirectStatus());
}

TEST_F(ResponseHeadersTest, IsHtmlLike) {
  // No header means, not html-like.
  EXPECT_FALSE(IsHtmlLike(""));
  EXPECT_FALSE(IsHtmlLike("text/css"));

  EXPECT_TRUE(IsHtmlLike("text/html"));
  EXPECT_TRUE(IsHtmlLike("application/xhtml+xml"));
}

TEST_F(ResponseHeadersTest, ForceCachingForNoCache) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kCacheControl, "max-age=0, no-cache");
  response_headers_.ForceCaching(360 * 1000);
  response_headers_.ComputeCaching();

  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(360 * 1000, response_headers_.cache_ttl_ms());
  EXPECT_FALSE(response_headers_.Has(HttpAttributes::kExpires));
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kCacheControl, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_STREQ("max-age=0", *(values[0]));
  EXPECT_STREQ("no-cache", *(values[1]));

  response_headers_.UpdateCacheHeadersIfForceCached();
  EXPECT_STREQ("max-age=360",
               response_headers_.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_6_minutes_string_,
               response_headers_.Lookup1(HttpAttributes::kExpires));
}

TEST_F(ResponseHeadersTest, ForceCachingForPrivate) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kCacheControl,
                        "private, max-age=30000000");
  response_headers_.ForceCaching(360 * 1000);
  response_headers_.ComputeCaching();

  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(360 * 1000, response_headers_.cache_ttl_ms());
  EXPECT_FALSE(response_headers_.Has(HttpAttributes::kExpires));
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kCacheControl, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_STREQ("private", *(values[0]));
  EXPECT_STREQ("max-age=30000000", *(values[1]));

  response_headers_.UpdateCacheHeadersIfForceCached();
  EXPECT_STREQ("max-age=360",
               response_headers_.Lookup1(HttpAttributes::kCacheControl));
  EXPECT_STREQ(start_time_plus_6_minutes_string_,
               response_headers_.Lookup1(HttpAttributes::kExpires));
}

TEST_F(ResponseHeadersTest, ForceCachingForAlreadyPublic) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kCacheControl,
                        "public, max-age=3456");
  response_headers_.ForceCaching(360 * 1000);
  response_headers_.ComputeCaching();

  EXPECT_TRUE(response_headers_.IsProxyCacheable());
  EXPECT_EQ(3456 * 1000, response_headers_.cache_ttl_ms());
  EXPECT_FALSE(response_headers_.Has(HttpAttributes::kExpires));
  ConstStringStarVector values;
  response_headers_.Lookup(HttpAttributes::kCacheControl, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_STREQ("public", *(values[0]));
  EXPECT_STREQ("max-age=3456", *(values[1]));

  response_headers_.UpdateCacheHeadersIfForceCached();
  EXPECT_FALSE(response_headers_.Has(HttpAttributes::kExpires));
  response_headers_.Lookup(HttpAttributes::kCacheControl, &values);
  EXPECT_EQ(2, values.size());
  EXPECT_STREQ("public", *(values[0]));
  EXPECT_STREQ("max-age=3456", *(values[1]));
}

TEST_F(ResponseHeadersTest, GetCookieString) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kSetCookie, "CG=US:CA:Mountain+View");
  response_headers_.Add(HttpAttributes::kSetCookie, "UA=chrome");
  response_headers_.Add(HttpAttributes::kSetCookie, "path=/");

  GoogleString cookie_str;
  response_headers_.GetCookieString(&cookie_str);

  EXPECT_STREQ("[\"CG=US:CA:Mountain+View\",\"UA=chrome\",\"path=/\"]",
               cookie_str);
}

TEST_F(ResponseHeadersTest, HasCookie) {
  response_headers_.SetStatusAndReason(HttpStatus::kOK);
  response_headers_.SetDate(MockTimer::kApr_5_2010_ms);
  response_headers_.Add(HttpAttributes::kSetCookie, "CG=US:CA:Mountain+View");
  response_headers_.Add(HttpAttributes::kSetCookie, "UA=chrome");
  response_headers_.Add(HttpAttributes::kSetCookie, "UA=ie");
  response_headers_.Add(HttpAttributes::kSetCookie, "UA=;path=/");

  StringPieceVector values;
  StringPieceVector attributes;
  StringPiece attribute_value;
  EXPECT_FALSE(response_headers_.HasCookie("HttpOnly", NULL, NULL));
  EXPECT_TRUE(response_headers_.HasCookie("UA", &values, &attributes));
  ASSERT_EQ(3, values.size());
  EXPECT_EQ("chrome", values[0]);
  EXPECT_EQ("ie", values[1]);
  EXPECT_EQ("", values[2]);
  ASSERT_EQ(1, attributes.size());
  EXPECT_EQ("path=/", attributes[0]);
  EXPECT_TRUE(response_headers_.FindValueForName(attributes, "path",
                                                 &attribute_value));
  EXPECT_EQ("/", attribute_value);
  EXPECT_TRUE(response_headers_.HasAnyCookiesWithAttribute("path", NULL));
  EXPECT_FALSE(response_headers_.HasAnyCookiesWithAttribute("HttpOnly", NULL));

  response_headers_.Add(HttpAttributes::kSetCookie, "JSESSIONID=123; HttpOnly");
  EXPECT_TRUE(response_headers_.HasAnyCookiesWithAttribute("HttpOnly", NULL));
  EXPECT_FALSE(response_headers_.HasAnyCookiesWithAttribute("yaddayadda",
                                                            NULL));

  response_headers_.RemoveAll(HttpAttributes::kSetCookie);
  values.clear();
  attributes.clear();
  EXPECT_FALSE(response_headers_.HasCookie("JSESSIONID", NULL, NULL));
  EXPECT_FALSE(response_headers_.HasAnyCookiesWithAttribute("HttpOnly", NULL));

  response_headers_.Add(HttpAttributes::kSetCookie, "ID=ABC; HttpOnly ;path=/");
  response_headers_.Add(HttpAttributes::kSetCookie, "UA=chrome");
  response_headers_.Add(HttpAttributes::kSetCookie, "UA=ie");
  response_headers_.Add(HttpAttributes::kSetCookie, "UA=");
  EXPECT_TRUE(response_headers_.HasCookie("ID", &values, &attributes));
  ASSERT_EQ(1, values.size());
  EXPECT_EQ("ABC", values[0]);
  ASSERT_EQ(2, attributes.size());
  EXPECT_EQ(" HttpOnly ", attributes[0]);  // Note, not trimmed.
  EXPECT_EQ("path=/", attributes[1]);
  EXPECT_TRUE(response_headers_.FindValueForName(attributes, "HttpOnly", NULL));
  values.clear();
  attributes.clear();
  EXPECT_TRUE(response_headers_.HasCookie("UA", &values, &attributes));
  EXPECT_EQ(0, attributes.size());
  ASSERT_EQ(3, values.size());
  EXPECT_EQ("chrome", values[0]);
  EXPECT_EQ("ie", values[1]);
  EXPECT_EQ("", values[2]);
}

TEST_F(ResponseHeadersTest, CopyToProto) {
  ResponseHeaders headers;
  headers.set_status_code(200);
  headers.Add("foo", "bar");
  headers.Add("baz", "boo");

  HttpResponseHeaders headers_proto;
  headers.CopyToProto(&headers_proto);

  EXPECT_EQ(200, headers_proto.status_code());
  EXPECT_EQ(2, headers_proto.header_size());
  EXPECT_EQ("foo", headers_proto.header(0).name());
  EXPECT_EQ("bar", headers_proto.header(0).value());
  EXPECT_EQ("baz", headers_proto.header(1).name());
  EXPECT_EQ("boo", headers_proto.header(1).value());
}

TEST_F(ResponseHeadersTest, SetQueryParamsAsCookies) {
  const char kBaseHeaders[] =
      "HTTP/1.0 0 (null)\r\nfoo: bar\r\nbaz: boo\r\n\r\n";
  ResponseHeaders headers;
  headers.Add("foo", "bar");
  headers.Add("baz", "boo");
  EXPECT_EQ(kBaseHeaders, headers.ToString());

  const GoogleUrl kTestUrl("http://test.com/index.html");
  const char kPageSpeedQueryParams[] =
      "PageSpeedFilters=+inline_css&xyzzy=plugh&notme=nuh-uh&empty=&null";
  StringPieceVector to_exclude;
  to_exclude.push_back("notme");
  EXPECT_FALSE(headers.SetQueryParamsAsCookies(
      kTestUrl, "", to_exclude, MockTimer::kApr_5_2010_ms));
  EXPECT_TRUE(headers.SetQueryParamsAsCookies(
      kTestUrl, kPageSpeedQueryParams, to_exclude, MockTimer::kApr_5_2010_ms));
  CheckCookies(headers, "PageSpeedFilters", "%2binline_css",
               MockTimer::kApr_5_2010_ms);
  CheckCookies(headers, "xyzzy", "plugh", MockTimer::kApr_5_2010_ms);
  CheckCookies(headers, "empty", "", MockTimer::kApr_5_2010_ms);
  CheckCookies(headers, "null", "", MockTimer::kApr_5_2010_ms);
  EXPECT_TRUE(headers.Sanitize());
  EXPECT_EQ(kBaseHeaders, headers.ToString());
}

TEST_F(ResponseHeadersTest, ClearOptionCookies) {
  const char kBaseHeaders[] =
      "HTTP/1.0 0 (null)\r\nfoo: bar\r\nbaz: boo\r\n\r\n";
  ResponseHeaders headers;
  headers.Add("foo", "bar");
  headers.Add("baz", "boo");
  EXPECT_EQ(kBaseHeaders, headers.ToString());

  const GoogleUrl kTestUrl("http://test.com/index.html");
  const char kPageSpeedQueryParams[] =
      "PageSpeedFilters=+inline_css&xyzzy=plugh&notme=nuh-uh&empty=&null";
  StringPieceVector to_exclude;
  to_exclude.push_back("notme");
  EXPECT_FALSE(headers.ClearOptionCookies(kTestUrl, "", to_exclude));
  EXPECT_TRUE(headers.ClearOptionCookies(kTestUrl, kPageSpeedQueryParams,
                                         to_exclude));
  CheckCookies(headers, "PageSpeedFilters", "", 0);
  CheckCookies(headers, "xyzzy", "", 0);
  CheckCookies(headers, "empty", "", 0);
  CheckCookies(headers, "null", "", 0);
  EXPECT_TRUE(headers.Sanitize());
  EXPECT_EQ(kBaseHeaders, headers.ToString());
}

TEST_F(ResponseHeadersTest, RelCanonicalHeaderValue) {
  EXPECT_EQ("<http://www.example.com>; rel=\"canonical\"",
            ResponseHeaders::RelCanonicalHeaderValue("http://www.example.com"));

  EXPECT_EQ(
      "<http://www.example.com/foo%3Cbar%3E>; rel=\"canonical\"",
      ResponseHeaders::RelCanonicalHeaderValue(
          "http://www.example.com/foo<bar>"));
}

TEST_F(ResponseHeadersTest, HasLinkRelCanonical) {
  ResponseHeaders h1;
  EXPECT_FALSE(h1.HasLinkRelCanonical());

  h1.Add(HttpAttributes::kLink,
         "<http://www.example.com/canonical>; rel=\"next\"");
  EXPECT_FALSE(h1.HasLinkRelCanonical());

  h1.Add(HttpAttributes::kLink,
         "<http://www.example.com/foo>; rel= \"canonical\"; foo=bar");
  EXPECT_TRUE(h1.HasLinkRelCanonical());

  // This one is an expected false positive.
  ResponseHeaders h2;
  h2.Add(HttpAttributes::kLink,
         "<http://www.example.com/foo>; rel= \"next\"; icon=\"canonical.ico\"");
  EXPECT_TRUE(h2.HasLinkRelCanonical());
}

TEST_F(ResponseHeadersTest, ContentLength) {
  ResponseHeaders headers;
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetContentLength(25);
  headers.SetOriginalContentLength(50);
  EXPECT_STREQ("HTTP/1.0 200 OK\r\n"
               "Content-Length: 25\r\n"
               "X-Original-Content-Length: 50\r\n"
               "\r\n",
               headers.ToString());

  // Setting original-content-length again is ignored (first one wins).
  headers.SetOriginalContentLength(75);  // ignored.
  EXPECT_STREQ("HTTP/1.0 200 OK\r\n"
               "Content-Length: 25\r\n"
               "X-Original-Content-Length: 50\r\n"
               "\r\n",
               headers.ToString());

  // Setting the content-length smaller yet again is fine.  e.g. minification
  // first, then compression.
  headers.SetContentLength(20);
  EXPECT_STREQ("HTTP/1.0 200 OK\r\n"
               "X-Original-Content-Length: 50\r\n"
               "Content-Length: 20\r\n"
               "\r\n",
               headers.ToString());

  // Setting the content-length back to the original-content-length erases
  // the original-content-length.
  headers.SetContentLength(50);
  EXPECT_STREQ("HTTP/1.0 200 OK\r\n"
               "Content-Length: 50\r\n"
               "\r\n",
               headers.ToString());
}

TEST_F(ResponseHeadersTest, MultipleOriginalContentLengths) {
  ResponseHeaders headers;
  headers.SetStatusAndReason(HttpStatus::kOK);
  headers.SetContentLength(25);
  headers.SetOriginalContentLength(50);

  // To get more X-Original-Content-Length attributes we can add them
  // directly without going through the SetOriginalContentLength API.
  headers.Add(HttpAttributes::kXOriginalContentLength, "51");
  headers.Add(HttpAttributes::kXOriginalContentLength, "50");  // duplicate
  EXPECT_STREQ("HTTP/1.0 200 OK\r\n"
               "Content-Length: 25\r\n"
               "X-Original-Content-Length: 50\r\n"
               "X-Original-Content-Length: 51\r\n"
               "X-Original-Content-Length: 50\r\n"
               "\r\n",
               headers.ToString());

  // Now setting content-length to 50, we are still left with
  // the "X-Original-Content-Length: 51".
  headers.SetContentLength(50);
  EXPECT_STREQ("HTTP/1.0 200 OK\r\n"
               "X-Original-Content-Length: 51\r\n"
               "Content-Length: 50\r\n"
               "\r\n",
               headers.ToString());
}

TEST_F(ResponseHeadersTest, CacheControlPublic) {
  EXPECT_STREQ("public", AddPublicToCacheControl({}));
  EXPECT_STREQ("max-age=100, public", AddPublicToCacheControl({"max-age=100"}));
  EXPECT_STREQ("public, max-age=100",
               AddPublicToCacheControl({"public, max-age=100"}));
  EXPECT_STREQ("public, max-age=100",
               AddPublicToCacheControl({"public", "max-age=100"}));
  EXPECT_STREQ("max-age=100, private",
               AddPublicToCacheControl({"max-age=100,private"}));
  EXPECT_STREQ("max-age=100, private",
               AddPublicToCacheControl({"max-age=100", "private"}));
  EXPECT_STREQ("no-store", AddPublicToCacheControl({"no-store"}));
  EXPECT_STREQ("no-cache", AddPublicToCacheControl({"no-cache"}));
  EXPECT_STREQ("No-Store", AddPublicToCacheControl({"No-Store"}));
  EXPECT_STREQ("No-Cache", AddPublicToCacheControl({"No-Cache"}));
}

TEST_F(ResponseHeadersTest, TestHopByHopSanitization) {
  // RFC hop-by-hop list: http://tools.ietf.org/html/rfc7230#section-6.1
  ResponseHeaders headers;

  headers.Add(HttpAttributes::kConnection,
              "Keep-Alive, Foo,, , bar, Cache-Control");
  headers.Add(HttpAttributes::kKeepAlive, "foo");
  headers.Add(HttpAttributes::kProxyAuthenticate, "foo");
  headers.Add(HttpAttributes::kProxyAuthorization, "foo");
  headers.Add(HttpAttributes::kTE, "foo");
  headers.Add(HttpAttributes::kTrailers, "foo");
  headers.Add(HttpAttributes::kTransferEncoding, "foo");
  headers.Add(HttpAttributes::kUpgrade, "foo");
  headers.Add(HttpAttributes::kAlternateProtocol, "foo");
  headers.Add(HttpAttributes::kCacheControl, "foo");
  // foo: foo is be referenced in "Connection: Foo", and therefore is marked
  // as hop-by-hop and as such candidate for sanitization.
  headers.Add("foo", "foo");

  EXPECT_TRUE(headers.Sanitize());

  // After sanitization, only end-to-end header Cache-Control should remain.
  EXPECT_EQ("HTTP/1.0 0 (null)\r\nCache-Control: foo\r\n\r\n",
            headers.ToString());

  // Test to make sure we don't screw up if Connection: marks itself as
  // explicitly hop-by-hop.
  ResponseHeaders headers2;
  headers2.Add(HttpAttributes::kConnection,
               "Connection, Foo");
  headers2.Add("foo", "foo");
  headers2.Add("bar", "baz");

  EXPECT_TRUE(headers2.Sanitize());
  EXPECT_EQ("HTTP/1.0 0 (null)\r\nbar: baz\r\n\r\n", headers2.ToString());
}

}  // namespace net_instaweb
