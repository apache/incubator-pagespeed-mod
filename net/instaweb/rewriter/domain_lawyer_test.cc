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


#include "net/instaweb/rewriter/public/domain_lawyer.h"

#include "pagespeed/kernel/base/gtest.h"
#include "pagespeed/kernel/base/mock_message_handler.h"
#include "pagespeed/kernel/base/null_mutex.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/http/google_url.h"

namespace {

const char kResourceUrl[] = "styles/style.css?appearance=reader";
const char kCdnPrefix[] = "http://graphics8.nytimes.com/";
const char kRequestDomain[] = "http://www.nytimes.com/";
const char kRequestDomainPort[] = "http://www.nytimes.com:8080/";

}  // namespace

namespace net_instaweb {

class DomainLawyerTest : public testing::Test {
 protected:
  DomainLawyerTest()
      : orig_request_("http://www.nytimes.com/index.html"),
        port_request_("http://www.nytimes.com:8080/index.html"),
        https_request_("https://www.nytimes.com/index.html"),
        message_handler_(new NullMutex) {
    domain_lawyer_with_all_domains_authorized_.AddDomain(
        "*", &message_handler_);
  }

  // Syntactic sugar to map a request.
  bool MapRequest(const GoogleUrl& original_request,
                  const StringPiece& resource_url,
                  GoogleString* mapped_domain_name) {
    GoogleUrl resolved_request;
    return MapRequest(original_request, resource_url, mapped_domain_name,
                      &resolved_request);
  }

  // Syntactic sugar to map a request.
  bool MapRequest(const GoogleUrl& original_request,
                  const StringPiece& resource_url,
                  GoogleString* mapped_domain_name,
                  GoogleUrl* resolved_request) {
    return domain_lawyer_.MapRequestToDomain(
        original_request, resource_url, mapped_domain_name, resolved_request,
        &message_handler_);
  }

  bool MapOrigin(const StringPiece& in, GoogleString* out) {
    bool is_proxy = true;
    out->clear();
    GoogleString host_header;
    return domain_lawyer_.MapOrigin(in, out, &host_header,
                                    &is_proxy) && !is_proxy;
  }

  bool MapOriginAndHost(const StringPiece& in, GoogleString* origin,
                        GoogleString* host_header) {
    bool is_proxy = true;
    origin->clear();
    host_header->clear();
    return domain_lawyer_.MapOrigin(in, origin, host_header,
                                    &is_proxy) && !is_proxy;
  }

  bool MapProxy(const StringPiece& in, GoogleString* out) {
    bool is_proxy = false;
    out->clear();
    GoogleString host_header;
    return domain_lawyer_.MapOrigin(in, out, &host_header, &is_proxy) &&
      is_proxy;
  }

  bool AddOriginDomainMapping(const StringPiece& dest, const StringPiece& src) {
    return domain_lawyer_.AddOriginDomainMapping(dest, src, "",
                                                 &message_handler_);
  }

  bool AddRewriteDomainMapping(const StringPiece& dest,
                               const StringPiece& src) {
    return domain_lawyer_.AddRewriteDomainMapping(dest, src, &message_handler_);
  }

  bool AddShard(const StringPiece& domain, const StringPiece& shards) {
    return domain_lawyer_.AddShard(domain, shards, &message_handler_);
  }

  bool WillDomainChange(StringPiece url) {
    GoogleUrl gurl(domain_lawyer_.NormalizeDomainName(url));
    return domain_lawyer_.WillDomainChange(gurl);
  }

  bool IsDomainAuthorized(const GoogleUrl& context_gurl, StringPiece url) {
    GoogleUrl gurl(url);
    return domain_lawyer_.IsDomainAuthorized(context_gurl, gurl);
  }

  GoogleUrl orig_request_;
  GoogleUrl port_request_;
  GoogleUrl https_request_;
  DomainLawyer domain_lawyer_;
  DomainLawyer domain_lawyer_with_all_domains_authorized_;
  MockMessageHandler message_handler_;
};

TEST_F(DomainLawyerTest, RelativeDomain) {
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(orig_request_, kResourceUrl, &mapped_domain_name));
  EXPECT_STREQ(kRequestDomain, mapped_domain_name);
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
}

TEST_F(DomainLawyerTest, AbsoluteDomain) {
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(orig_request_, StrCat(kRequestDomain, kResourceUrl),
                         &mapped_domain_name));
  EXPECT_STREQ(kRequestDomain, mapped_domain_name);
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
}

TEST_F(DomainLawyerTest, ExternalDomainNotDeclared) {
  GoogleString mapped_domain_name;
  EXPECT_FALSE(MapRequest(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name));
}

TEST_F(DomainLawyerTest, ExternalDomainDeclared) {
  StringPiece cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix));

  // Any domain is authorized with respect to an HTML from the same domain.
  EXPECT_TRUE(IsDomainAuthorized(orig_request_, orig_request_.Origin()));

  // But to pull in a resource from another domain, we must first authorize it.
  EXPECT_FALSE(IsDomainAuthorized(orig_request_, cdn_domain));
  ASSERT_TRUE(domain_lawyer_.AddDomain(cdn_domain, &message_handler_));
  EXPECT_TRUE(IsDomainAuthorized(orig_request_, cdn_domain));
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name));
  EXPECT_STREQ(cdn_domain, mapped_domain_name);

  // Make sure that we do not allow requests when the port is present; we've
  // only authorized origin "http://www.nytimes.com/",
  // not "http://www.nytimes.com:8080/".
  // The '-1' below is to strip the trailing slash.
  GoogleString orig_cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix) - 1);
  GoogleString port_cdn_domain(cdn_domain.data(), cdn_domain.size() - 1);
  port_cdn_domain += ":8080/";
  EXPECT_FALSE(MapRequest(
      orig_request_, StrCat(port_cdn_domain, "/", kResourceUrl),
      &mapped_domain_name));
  EXPECT_FALSE(domain_lawyer_.DoDomainsServeSameContent(
      port_cdn_domain, cdn_domain));
}

TEST_F(DomainLawyerTest, ExternalUpperCaseDomainDeclared) {
  GoogleString cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix));
  UpperString(&cdn_domain);   // will get normalized in AddDomain.
  ASSERT_TRUE(domain_lawyer_.AddDomain(cdn_domain, &message_handler_));
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name));
  LowerString(&cdn_domain);
  EXPECT_STREQ(cdn_domain, mapped_domain_name);

  // Make sure that we do not allow requests when the port is present; we've
  // only authorized origin "http://www.nytimes.com/",
  // not "http://www.nytimes.com:8080/".
  // The '-1' below is to strip the trailing slash.
  GoogleString orig_cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix) - 1);
  GoogleString port_cdn_domain(cdn_domain.data(), cdn_domain.size() - 1);
  port_cdn_domain += ":8080/";
  EXPECT_FALSE(MapRequest(
      orig_request_, StrCat(port_cdn_domain, "/", kResourceUrl),
      &mapped_domain_name));
}

TEST_F(DomainLawyerTest, MixedCasePath) {
  GoogleUrl context_gurl("http://origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddDomain("EXAMPLE.com/HI/lo", &message_handler_));
  EXPECT_TRUE(IsDomainAuthorized(context_gurl,
                                 "http://example.com/HI/lo/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl,
                                  "http://example.com/hi/lo/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl,
                                  "https://example.com/HI/lo/file"));
}

TEST_F(DomainLawyerTest, RedundantPortsOnDeclaration) {
  GoogleUrl context_gurl("http://origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddDomain("http://a.com:80", &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddDomain("https://b.com:443", &message_handler_));
  EXPECT_TRUE(IsDomainAuthorized(context_gurl, "http://a.com/file"));
  EXPECT_TRUE(IsDomainAuthorized(context_gurl, "https://b.com/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "http://b.com/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "https://a.com/file"));
}

TEST_F(DomainLawyerTest, RedundantPortsOnTest) {
  GoogleUrl context_gurl("http://origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddDomain("http://a.com", &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddDomain("https://b.com", &message_handler_));
  EXPECT_TRUE(IsDomainAuthorized(context_gurl, "http://a.com:80/file"));
  EXPECT_TRUE(IsDomainAuthorized(context_gurl, "https://b.com:443/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "http://a.com:443/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "http://b.com:443/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "http://b.com:80/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "https://a.com:443/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "https://a.com:80/file"));
  EXPECT_FALSE(IsDomainAuthorized(context_gurl, "https://b.com:80/file"));
}

TEST_F(DomainLawyerTest, ExternalDomainDeclaredWithoutScheme) {
  StringPiece cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix));
  ASSERT_TRUE(domain_lawyer_.AddDomain(kCdnPrefix + strlen("http://"),
                                       &message_handler_));
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name));
  EXPECT_STREQ(cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, ExternalDomainDeclaredWithoutTrailingSlash) {
  StringPiece cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix));
  // The '-1' below is to strip the trailing slash.
  StringPiece cdn_domain_no_slash(kCdnPrefix, STATIC_STRLEN(kCdnPrefix) - 1);
  ASSERT_TRUE(domain_lawyer_.AddDomain(cdn_domain_no_slash, &message_handler_));
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name));
  EXPECT_STREQ(cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, WildcardDomainDeclared) {
  StringPiece cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix));
  ASSERT_TRUE(domain_lawyer_.AddDomain("*.nytimes.com", &message_handler_));
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      orig_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name));
  EXPECT_STREQ(cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, RelativeDomainPort) {
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(port_request_, kResourceUrl, &mapped_domain_name));
  EXPECT_STREQ(kRequestDomainPort, mapped_domain_name);
}

TEST_F(DomainLawyerTest, AbsoluteDomainPort) {
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      port_request_, StrCat(kRequestDomainPort, kResourceUrl),
      &mapped_domain_name));
  EXPECT_STREQ(kRequestDomainPort, mapped_domain_name);
}

TEST_F(DomainLawyerTest, PortExternalDomainNotDeclared) {
  GoogleString mapped_domain_name;
  EXPECT_FALSE(MapRequest(
      port_request_, StrCat(kCdnPrefix, kResourceUrl), &mapped_domain_name));
}

TEST_F(DomainLawyerTest, PortExternalDomainDeclared) {
  GoogleString port_cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix) - 1);
  port_cdn_domain += ":8080/";
  ASSERT_TRUE(domain_lawyer_.AddDomain(port_cdn_domain, &message_handler_));
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      port_request_, StrCat(port_cdn_domain, kResourceUrl),
      &mapped_domain_name));
  EXPECT_STREQ(port_cdn_domain, mapped_domain_name);

  // Make sure that we do not allow requests when the port is missing; we've
  // only authorized origin "http://www.nytimes.com:8080/",
  // not "http://www.nytimes.com:8080".
  GoogleString orig_cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix) - 1);
  orig_cdn_domain += "/";
  EXPECT_FALSE(MapRequest(port_request_, StrCat(orig_cdn_domain, kResourceUrl),
                          &mapped_domain_name));
}

TEST_F(DomainLawyerTest, PortWildcardDomainDeclared) {
  GoogleString port_cdn_domain(kCdnPrefix, STATIC_STRLEN(kCdnPrefix) - 1);
  port_cdn_domain += ":8080/";
  ASSERT_TRUE(domain_lawyer_.AddDomain("*.nytimes.com:*", &message_handler_));
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(port_request_, StrCat(port_cdn_domain, kResourceUrl),
                         &mapped_domain_name));
  EXPECT_STREQ(port_cdn_domain, mapped_domain_name);
}

TEST_F(DomainLawyerTest, HttpsDomain) {
  ASSERT_TRUE(domain_lawyer_.AddDomain("https://nytimes.com",
                                       &message_handler_));
}

TEST_F(DomainLawyerTest, ResourceFromHttpsPage) {
  ASSERT_TRUE(domain_lawyer_.AddDomain("www.nytimes.com", &message_handler_));
  GoogleString mapped_domain_name;

  // We now handle requests for https, though subsequent fetching might fail.
  ASSERT_TRUE(MapRequest(https_request_, kResourceUrl, &mapped_domain_name));
  ASSERT_TRUE(MapRequest(https_request_, StrCat(kRequestDomain, kResourceUrl),
                         &mapped_domain_name));
}

TEST_F(DomainLawyerTest, MapHttpsAcrossHosts) {
  ASSERT_TRUE(AddOriginDomainMapping("http://insecure.nytimes.com",
                                     "https://secure.nytimes.com"));
  ASSERT_TRUE(AddOriginDomainMapping("https://secure.nytimes.com",
                                     "http://insecure.nytimes.com"));
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin(
      "https://secure.nytimes.com/css/stylesheet.css", &mapped));
  EXPECT_STREQ("http://insecure.nytimes.com/css/stylesheet.css", mapped);
  ASSERT_TRUE(MapOrigin(
      "http://insecure.nytimes.com/css/stylesheet.css", &mapped));
  EXPECT_EQ("https://secure.nytimes.com/css/stylesheet.css", mapped);
}

TEST_F(DomainLawyerTest, MapHttpsAcrossSchemes) {
  ASSERT_TRUE(AddOriginDomainMapping("http://nytimes.com",
                                     "https://nytimes.com"));
  ASSERT_TRUE(AddOriginDomainMapping("https://nytimes.com",
                                     "http://nytimes.com"));
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("https://nytimes.com/css/stylesheet.css", &mapped));
  EXPECT_STREQ("http://nytimes.com/css/stylesheet.css", mapped);
  ASSERT_TRUE(MapOrigin("http://nytimes.com/css/stylesheet.css", &mapped));
  EXPECT_EQ("https://nytimes.com/css/stylesheet.css", mapped);
}

TEST_F(DomainLawyerTest, MapHttpsAcrossPorts) {
  ASSERT_TRUE(AddOriginDomainMapping("http://nytimes.com:8181",
                                     "https://nytimes.com"));
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("https://nytimes.com/css/stylesheet.css", &mapped));
  EXPECT_STREQ("http://nytimes.com:8181/css/stylesheet.css", mapped);
}

TEST_F(DomainLawyerTest, MapHttpsAcrossSchemesAndPorts) {
  ASSERT_TRUE(AddOriginDomainMapping("http://localhost:8080",
                                     "https://nytimes.com:8443"));
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin(
      "https://nytimes.com:8443/css/stylesheet.css", &mapped));
  EXPECT_STREQ("http://localhost:8080/css/stylesheet.css", mapped);
}

TEST_F(DomainLawyerTest, AddTwoProtocolDomainMapping) {
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      "ref.nytimes.com", "www.nytimes.com", "", &message_handler_));
  // This will rewrite domains of fetches, but not change urls in page:
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped;
  GoogleString host_header;
  ASSERT_TRUE(MapOriginAndHost(
      "http://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("http://ref.nytimes.com/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com", host_header);
  ASSERT_TRUE(MapOriginAndHost(
      "https://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("https://ref.nytimes.com/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com", host_header);
}

TEST_F(DomainLawyerTest, AddTwoProtocolDomainMappingWithRefPort) {
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      "ref.nytimes.com:8089", "www.nytimes.com", "", &message_handler_));
  // This will rewrite domains of fetches, but not change urls in page:
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped;
  GoogleString host_header;
  ASSERT_TRUE(MapOriginAndHost(
      "http://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("http://ref.nytimes.com:8089/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com", host_header);
  ASSERT_TRUE(MapOriginAndHost(
      "https://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("https://ref.nytimes.com:8089/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com", host_header);
}

TEST_F(DomainLawyerTest, AddTwoProtocolDomainMappingWithServingPort) {
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      "ref.nytimes.com", "www.nytimes.com:8080", "", &message_handler_));
  // This will rewrite domains of fetches, but not change urls in page:
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped;
  GoogleString host_header;
  ASSERT_TRUE(MapOriginAndHost(
      "http://www.nytimes.com:8080/index.html", &mapped, &host_header));
  EXPECT_STREQ("http://ref.nytimes.com/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com:8080", host_header);
  ASSERT_TRUE(MapOriginAndHost(
      "http://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("http://www.nytimes.com/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com", host_header);
  ASSERT_TRUE(MapOriginAndHost(
      "https://www.nytimes.com:8080/index.html", &mapped, &host_header));
  EXPECT_STREQ("https://ref.nytimes.com/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com:8080", host_header);
  ASSERT_TRUE(MapOriginAndHost(
      "https://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("https://www.nytimes.com/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com", host_header);
}

TEST_F(DomainLawyerTest, AddTwoProtocolDomainMappingWithBothPorts) {
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      "ref.nytimes.com:9999", "www.nytimes.com:8080", "", &message_handler_));
  // This will rewrite domains of fetches, but not change urls in page:
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped;
  GoogleString host_header;
  ASSERT_TRUE(MapOriginAndHost(
      "http://www.nytimes.com:8080/index.html", &mapped, &host_header));
  EXPECT_STREQ("http://ref.nytimes.com:9999/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com:8080", host_header);
  ASSERT_TRUE(MapOriginAndHost(
      "https://www.nytimes.com:8080/index.html", &mapped, &host_header));
  EXPECT_STREQ("https://ref.nytimes.com:9999/index.html", mapped);
  EXPECT_STREQ("www.nytimes.com:8080", host_header);
}

TEST_F(DomainLawyerTest, AddTwoProtocolDomainMappingWithHostHeader) {
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      "ref.nytimes.com", "www.nytimes.com", "host.nytimes.com",
      &message_handler_));
  // This will rewrite domains of fetches, but not change urls in page:
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped;
  GoogleString host_header;
  ASSERT_TRUE(MapOriginAndHost(
      "http://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("http://ref.nytimes.com/index.html", mapped);
  EXPECT_STREQ("host.nytimes.com", host_header);
  ASSERT_TRUE(MapOriginAndHost(
      "https://www.nytimes.com/index.html", &mapped, &host_header));
  EXPECT_STREQ("https://ref.nytimes.com/index.html", mapped);
  EXPECT_STREQ("host.nytimes.com", host_header);
}

TEST_F(DomainLawyerTest, MapOriginExplicitHost) {
  ASSERT_TRUE(domain_lawyer_.AddOriginDomainMapping("origin", "*domain", "host",
                                                    &message_handler_));
  bool is_proxy = true;
  GoogleString out;
  GoogleString host;
  ASSERT_TRUE(domain_lawyer_.MapOrigin("http://www.domain/foo.css",
                                       &out, &host, &is_proxy));
  EXPECT_STREQ("http://origin/foo.css", out);
  EXPECT_STREQ("host", host);
  EXPECT_FALSE(is_proxy);
}

TEST_F(DomainLawyerTest, MapOriginWithoutExplicitHost) {
  ASSERT_TRUE(domain_lawyer_.AddOriginDomainMapping("origin", "*domain",
                                                    "" /* host_header */,
                                                    &message_handler_));
  bool is_proxy = true;
  GoogleString out;
  GoogleString host;
  ASSERT_TRUE(domain_lawyer_.MapOrigin("http://www.domain/foo.css",
                                       &out, &host, &is_proxy));
  EXPECT_STREQ("http://origin/foo.css", out);
  EXPECT_STREQ("www.domain", host);
  EXPECT_FALSE(is_proxy);
}

TEST_F(DomainLawyerTest, RewriteHttpsAcrossHosts) {
  ASSERT_TRUE(AddRewriteDomainMapping("http://insecure.nytimes.com",
                                      "https://secure.nytimes.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent(
      "insecure.nytimes.com", "https://secure.nytimes.com"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped_domain_name;
  GoogleUrl insecure_gurl("http://insecure.nytimes.com/index.html");
  ASSERT_TRUE(MapRequest(insecure_gurl,
                         "https://secure.nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://insecure.nytimes.com/", mapped_domain_name);
  // Succeeds because http://insecure... is authorized and matches the request.
  GoogleUrl https_gurl("https://secure.nytimes.com/index.html");
  ASSERT_TRUE(MapRequest(https_gurl,
                         "http://insecure.nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://insecure.nytimes.com/", mapped_domain_name);
  // Succeeds because https://secure... maps to http://insecure...
  ASSERT_TRUE(MapRequest(https_gurl,
                         "https://secure.nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://insecure.nytimes.com/", mapped_domain_name);
}

TEST_F(DomainLawyerTest, RewriteHttpsAcrossPorts) {
  ASSERT_TRUE(AddRewriteDomainMapping("http://nytimes.com:8181",
                                      "https://nytimes.com"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped_domain_name;
  // Succeeds because we map it as specified above.
  GoogleUrl nyt_gurl("http://nytimes.com/index.html");
  ASSERT_TRUE(MapRequest(nyt_gurl, "https://nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://nytimes.com:8181/", mapped_domain_name);
  // Fails because http://nytimes/ is not authorized.
  GoogleUrl nyt_https("https://nytimes.com/index.html");
  ASSERT_FALSE(MapRequest(nyt_https,
                          "http://nytimes.com/css/stylesheet.css",
                          &mapped_domain_name));
  // Succeeds because http://nytimes:8181/ is authorized & matches the request.
  ASSERT_TRUE(MapRequest(nyt_https,
                         "http://nytimes.com:8181/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://nytimes.com:8181/", mapped_domain_name);
  // Succeeds because https://nytimes/ maps to http://nytimes:8181/.
  ASSERT_TRUE(MapRequest(nyt_https,
                         "https://nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://nytimes.com:8181/", mapped_domain_name);
}

TEST_F(DomainLawyerTest, RewriteHttpsAcrossSchemes) {
  ASSERT_TRUE(AddRewriteDomainMapping("http://nytimes.com",
                                      "https://nytimes.com"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped_domain_name;
  GoogleUrl nyt_http("http://nytimes.com/index.html");
  ASSERT_TRUE(MapRequest(nyt_http,
                         "https://nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://nytimes.com/", mapped_domain_name);
  // Succeeds because http://nytimes/ is authorized and matches the request.
  GoogleUrl nyt_https("https://nytimes.com/index.html");
  ASSERT_TRUE(MapRequest(nyt_https,
                         "http://nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://nytimes.com/", mapped_domain_name);
  // Succeeds because https://nytimes/ maps to http://nytimes/.
  ASSERT_TRUE(MapRequest(nyt_https,
                         "https://nytimes.com/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://nytimes.com/", mapped_domain_name);
}

TEST_F(DomainLawyerTest, RewriteHttpsAcrossSchemesAndPorts) {
  ASSERT_TRUE(AddRewriteDomainMapping("http://localhost:8080",
                                      "https://nytimes.com:8443"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped_domain_name;
  GoogleUrl local_8080("http://localhost:8080/index.html");
  ASSERT_TRUE(MapRequest(local_8080,
                         "https://nytimes.com:8443/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://localhost:8080/", mapped_domain_name);
  // Succeeds b/c http://localhost:8080/ is authorized and matches the request.
  GoogleUrl https_nyt_8443("https://nytimes.com:8443/index.html");
  ASSERT_TRUE(MapRequest(https_nyt_8443,
                         "http://localhost:8080/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://localhost:8080/", mapped_domain_name);
  // Succeeds because https://nytimes:8443/ maps to http://localhost:8080/.
  ASSERT_TRUE(MapRequest(https_nyt_8443,
                         "https://nytimes.com:8443/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://localhost:8080/", mapped_domain_name);
  // Relative path also succeeds.
  ASSERT_TRUE(MapRequest(https_nyt_8443, "css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("http://localhost:8080/", mapped_domain_name);
}

TEST_F(DomainLawyerTest, RewriteHttpsToHttps) {
  ASSERT_TRUE(AddRewriteDomainMapping("https://localhost:8443",
                                      "https://nytimes.com:8443"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped_domain_name;
  GoogleUrl local_8443("https://localhost:8443/index.html");
  ASSERT_TRUE(MapRequest(local_8443,
                         "https://nytimes.com:8443/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("https://localhost:8443/", mapped_domain_name);
  // Succeeds b/c https://localhost:8443/ is authorized and matches the request.
  GoogleUrl https_nyt_8443("https://nytimes.com:8443/index.html");
  ASSERT_TRUE(MapRequest(https_nyt_8443,
                         "https://localhost:8443/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("https://localhost:8443/", mapped_domain_name);
  // Succeeds because https://nytimes:8443/ maps to https://localhost:8443/.
  ASSERT_TRUE(MapRequest(https_nyt_8443,
                         "https://nytimes.com:8443/css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("https://localhost:8443/", mapped_domain_name);
  // Relative path also succeeds.
  ASSERT_TRUE(MapRequest(https_nyt_8443, "css/stylesheet.css",
                         &mapped_domain_name));
  EXPECT_STREQ("https://localhost:8443/", mapped_domain_name);
}

TEST_F(DomainLawyerTest, AddTwoProtocolRewriteDomainMapping) {
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolRewriteDomainMapping(
      "www.nytimes.com", "ref.nytimes.com", &message_handler_));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString mapped_domain;
  GoogleUrl containing_page_http("http://www.nytimes.com/index.html");
  GoogleUrl containing_page_https("https://www.nytimes.com/index.html");
  // http page asks for http stylesheet.
  ASSERT_TRUE(MapRequest(
      containing_page_http,
      "http://ref.nytimes.com/css/stylesheet.css", &mapped_domain));
  EXPECT_STREQ("http://www.nytimes.com/", mapped_domain);
  // http page asks for an https stylesheet.  Should still re-map.
  ASSERT_TRUE(MapRequest(
      containing_page_http,
      "https://ref.nytimes.com/css/stylesheet.css", &mapped_domain));
  EXPECT_STREQ("https://www.nytimes.com/", mapped_domain);
  // https page asks for an https stylesheet.
  ASSERT_TRUE(MapRequest(
      containing_page_https,
      "https://ref.nytimes.com/css/stylesheet.css", &mapped_domain));
  EXPECT_STREQ("https://www.nytimes.com/", mapped_domain);
  // https page asks for an http stylesheet.  It shouldn't be doing that, but we
  // preserve the bad behavior so the user realizes something fishy could
  // happen.
  ASSERT_TRUE(MapRequest(
      containing_page_https,
      "http://ref.nytimes.com/css/stylesheet.css", &mapped_domain));
  EXPECT_STREQ("http://www.nytimes.com/", mapped_domain);
}

TEST_F(DomainLawyerTest, FindDomainsRewrittenTo) {
  // No mapping.
  ConstStringStarVector from_domains;
  GoogleUrl gurl("http://www1.example.com/");
  domain_lawyer_.FindDomainsRewrittenTo(gurl, &from_domains);
  EXPECT_EQ(0, from_domains.size());

  // Add mapping.
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolRewriteDomainMapping(
      "www1.example.com", "www.example.com", &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolRewriteDomainMapping(
      "www1.example.com", "xyz.example.com", &message_handler_));

  domain_lawyer_.FindDomainsRewrittenTo(gurl, &from_domains);
  ASSERT_EQ(2, from_domains.size());
  EXPECT_STREQ("http://www.example.com/", *(from_domains[0]));
  EXPECT_STREQ("http://xyz.example.com/", *(from_domains[1]));
}

TEST_F(DomainLawyerTest, AddDomainRedundantly) {
  ASSERT_TRUE(domain_lawyer_.AddDomain("www.nytimes.com", &message_handler_));
  ASSERT_FALSE(domain_lawyer_.AddDomain("www.nytimes.com", &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddDomain("*", &message_handler_));
  ASSERT_FALSE(domain_lawyer_.AddDomain("*", &message_handler_));
}

TEST_F(DomainLawyerTest, VerifyPortIsDistinct1) {
  ASSERT_TRUE(domain_lawyer_.AddDomain("www.example.com", &message_handler_));
  GoogleString mapped_domain_name;
  GoogleUrl context_gurl("http://www.other.com/index.html");
  EXPECT_FALSE(MapRequest(
      context_gurl,
      "http://www.example.com:81/styles.css",
      &mapped_domain_name));
}

TEST_F(DomainLawyerTest, VerifyPortIsDistinct2) {
  ASSERT_TRUE(
      domain_lawyer_.AddDomain("www.example.com:81", &message_handler_));
  GoogleString mapped_domain_name;
  GoogleUrl context_gurl("http://www.other.com/index.html");
  EXPECT_FALSE(MapRequest(
      context_gurl,
      "http://www.example.com/styles.css",
      &mapped_domain_name));
}

TEST_F(DomainLawyerTest, VerifyWildcardedPortSpec) {
  ASSERT_TRUE(domain_lawyer_.AddDomain("www.example.com*", &message_handler_));
  GoogleUrl context_gurl("http://www.origin.com/index.html");
  GoogleString mapped_domain_name;
  EXPECT_TRUE(MapRequest(
      context_gurl,
      "http://www.example.com/styles.css",
      &mapped_domain_name));
  EXPECT_TRUE(MapRequest(
      context_gurl,
      "http://www.example.com:81/styles.css",
      &mapped_domain_name));
}

TEST_F(DomainLawyerTest, MapRewriteDomain) {
  GoogleUrl context_gurl("http://www.origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddDomain("http://cdn.com/", &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddDomain("http://origin.com/",
                                       &message_handler_));
  EXPECT_FALSE(domain_lawyer_.DoDomainsServeSameContent(
      "cdn.com", "origin.com"));
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com", "http://origin.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent(
      "cdn.com", "origin.com"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  // First try the mapping from "origin.com" to "cdn.com".
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      context_gurl,
      "http://origin.com/styles/blue.css",
      &mapped_domain_name));
  EXPECT_STREQ("http://cdn.com/", mapped_domain_name);

  // But a relative reference will not map because we mapped "origin.com",
  // not "www.origin.com".
  ASSERT_TRUE(MapRequest(
      context_gurl,
      "styles/blue.css",
      &mapped_domain_name));
  EXPECT_STREQ("http://www.origin.com/", mapped_domain_name);

  // Now add the mapping from "www".
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com",
                                      "http://www.origin.com"));
  ASSERT_TRUE(MapRequest(
      context_gurl,
      "styles/blue.css",
      &mapped_domain_name));
  EXPECT_STREQ("http://cdn.com/", mapped_domain_name);
}

TEST_F(DomainLawyerTest, MapRewriteDomainAndPath) {
  GoogleUrl context_gurl("http://www.origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddDomain("http://cdn.com/origin/",
                                       &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddDomain("http://origin.com/",
                                       &message_handler_));
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com/origin",
                                      "http://origin.com"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  // First try the mapping from "origin.com" to "cdn.com/origin".
  GoogleUrl resolved_request;
  GoogleString mapped_domain_name;
  ASSERT_TRUE(MapRequest(
      context_gurl,
      "http://origin.com/styles/blue.css",
      &mapped_domain_name,
      &resolved_request));
  EXPECT_STREQ("http://cdn.com/origin/", mapped_domain_name);
  EXPECT_STREQ("http://cdn.com/origin/styles/blue.css",
               resolved_request.Spec());

  // But a relative reference will not map because we mapped "origin.com",
  // not "www.origin.com".
  ASSERT_TRUE(MapRequest(
      context_gurl,
      "styles/blue.css",
      &mapped_domain_name,
      &resolved_request));
  EXPECT_STREQ("http://www.origin.com/", mapped_domain_name);
  EXPECT_STREQ("http://www.origin.com/styles/blue.css",
               resolved_request.Spec());

  // Now add the mapping from "www".
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com/origin",
                                      "http://www.origin.com"));
  ASSERT_TRUE(MapRequest(
      context_gurl,
      "styles/blue.css",
      &mapped_domain_name,
      &resolved_request));
  EXPECT_STREQ("http://cdn.com/origin/", mapped_domain_name);
  EXPECT_STREQ("http://cdn.com/origin/styles/blue.css",
               resolved_request.Spec());
}

TEST_F(DomainLawyerTest, RewriteWithPath) {
  GoogleUrl context_gurl("http://example.com/index.html");
  ASSERT_TRUE(AddRewriteDomainMapping(
      "http://example.com/static/images/", "http://static.com/images/"));
  GoogleString mapped_domain_name;
  GoogleUrl resolved_request;
  ASSERT_TRUE(MapRequest(context_gurl,
                         "http://static.com/images/teapot.png",
                         &mapped_domain_name, &resolved_request));
  EXPECT_STREQ("http://example.com/static/images/", mapped_domain_name);
  EXPECT_STREQ("http://example.com/static/images/teapot.png",
               resolved_request.Spec());
}

TEST_F(DomainLawyerTest, OriginWithPath) {
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://origin.com/subdir/", "http://external.com"));
  GoogleString origin_url;
  ASSERT_TRUE(MapOrigin("http://external.com/styles/main.css", &origin_url));
  EXPECT_STREQ("http://origin.com/subdir/styles/main.css", origin_url);
}

TEST_F(DomainLawyerTest, OriginWithPathOnSource) {
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://origin.com/subdir/", "http://external.com/path"));
  GoogleString origin_url;
  ASSERT_TRUE(MapOrigin("http://external.com/path/styles/main.css",
                        &origin_url));
  EXPECT_STREQ("http://origin.com/subdir/styles/main.css", origin_url);
}

TEST_F(DomainLawyerTest, OriginAndExternWithPaths) {
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://origin.com/subdir/", "http://external.com/static/"));
  GoogleString origin_url;
  ASSERT_TRUE(MapOrigin("http://external.com/static/styles/main.css",
                        &origin_url));
  EXPECT_STREQ("http://origin.com/subdir/styles/main.css", origin_url);
}

TEST_F(DomainLawyerTest, OriginAndExternWithMultipleMatches) {
  domain_lawyer_.AddDomain("http://origin.com", &message_handler_);
  domain_lawyer_.AddDomain("http://origin.com/a/b", &message_handler_);
  domain_lawyer_.AddDomain("http://external.com", &message_handler_);
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://origin.com/a/", "http://external.com/static/"));

  GoogleString origin_url;
  ASSERT_TRUE(MapOrigin("http://external.com/static/styles/main.css",
                        &origin_url));
  EXPECT_STREQ("http://origin.com/a/styles/main.css", origin_url);

  // No mappings should occur on a top level page on external.com,
  // since our directive should apply only to external.com/static.
  const char kTopLevelExternalPage[] = "http://external.com/index.html";
  origin_url.clear();
  ASSERT_TRUE(MapOrigin(kTopLevelExternalPage, &origin_url));
  EXPECT_STREQ(kTopLevelExternalPage, origin_url);
}

TEST_F(DomainLawyerTest, RootDomainOfProxySourceNotAuthorized) {
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://origin.com/a/", "http://external.com/static/"));
  GoogleUrl context_gurl("http://origin.com/index.html");
  GoogleUrl external_domain("http://external.com");

  // It is not OK to rewrite content on external.com.
  EXPECT_FALSE(domain_lawyer_.IsDomainAuthorized(context_gurl,
                                                 external_domain));
  EXPECT_TRUE(
      domain_lawyer_with_all_domains_authorized_.IsDomainAuthorized(
          context_gurl, external_domain));

  // But it *is* OK to rewrite content on external.com/static.
  external_domain.Reset("http://external.com/static/");
  EXPECT_TRUE(domain_lawyer_.IsDomainAuthorized(context_gurl,
                                                external_domain));
}

TEST_F(DomainLawyerTest, OriginAndExternWithMultipleMatchesDoubleSlash) {
  domain_lawyer_.AddDomain("http://origin.com", &message_handler_);
  domain_lawyer_.AddDomain("http://external.com", &message_handler_);
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://origin.com/subdir/", "http://external.com/static/"));

  GoogleString origin_url;
  ASSERT_TRUE(MapOrigin("http://external.com/static/styles//main.css",
                        &origin_url));
  EXPECT_STREQ("http://origin.com/subdir/styles//main.css", origin_url);
}

TEST_F(DomainLawyerTest, MapOriginDomain) {
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://localhost:8080", "http://origin.com:8080"));
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://origin.com:8080/a/b/c?d=f",
                                       &mapped));
  EXPECT_STREQ("http://localhost:8080/a/b/c?d=f", mapped);

  // The origin domain, which might be, say, 'localhost', is not necessarily
  // authorized as a domain for input resources.
  GoogleUrl gurl("http://origin.com:8080/index.html");
  EXPECT_FALSE(MapRequest(gurl, "http://localhost:8080/blue.css", &mapped));
  GoogleUrl page_url("http://origin.com:8080");
  EXPECT_FALSE(IsDomainAuthorized(page_url, "http://localhost:8080"));

  // Of course, if we were to explicitly authorize then it would be ok.
  // First use a wildcard, which will not cover the ":8080", so the
  // Map will still fail.
  ASSERT_TRUE(domain_lawyer_.AddDomain("localhost*", &message_handler_));
  EXPECT_FALSE(MapRequest(gurl, "http://localhost:8080/blue.css", &mapped));

  // Now, include the port explicitly, and the mapping will be allowed.
  ASSERT_TRUE(domain_lawyer_.AddDomain("localhost:8080", &message_handler_));
  EXPECT_TRUE(MapRequest(gurl, "http://localhost:8080/blue.css", &mapped));
}

TEST_F(DomainLawyerTest, ProxyExternalResource) {
  GoogleUrl context_gurl("http://origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddProxyDomainMapping(
      "http://origin.com/external", "http://external.com/static", "",
      &message_handler_));

  // Map proxy_this.png to a subdirectory in origin.com.
  GoogleUrl resolved_request;
  GoogleString mapped_domain_name;
  const char kUrlToProxy[] = "http://external.com/static/images/proxy_this.png";
  ASSERT_TRUE(MapRequest(context_gurl, kUrlToProxy, &mapped_domain_name,
                         &resolved_request));
  EXPECT_STREQ("http://origin.com/external/", mapped_domain_name);
  EXPECT_STREQ("http://origin.com/external/images/proxy_this.png",
               resolved_request.Spec());

  // But when we fetch this resource, we won't find it in external.com so we
  // must map it back to origin.com/static.
  GoogleString origin_url;
  ASSERT_TRUE(MapProxy(resolved_request.Spec(), &origin_url));
  EXPECT_EQ(kUrlToProxy, origin_url);

  // Just because we enabled proxying from external.com/static, doesn't mean
  // we want to proxy from external.com/evil or external.com.
  EXPECT_FALSE(MapRequest(context_gurl, "http://external.com/evil/gifar.gif",
                          &mapped_domain_name, &resolved_request));
  EXPECT_FALSE(MapRequest(context_gurl, "http://external.com/gifar.gif",
                          &mapped_domain_name, &resolved_request));
}

// Test a situation in which origin is proxied, optimized, and rewritten to a
// CDN.
TEST_F(DomainLawyerTest, ProxyExternalResourceToCDN) {
  GoogleUrl context_gurl("http://proxy.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddProxyDomainMapping(
      "http://proxy.com/external",  // Proxies origin, optimizes.
      "http://origin.com/static",   // Origin server, potentially external.
      "http://cdn.com/external",    // CDN, caches responses.
      &message_handler_));

  GoogleUrl resolved_request;
  GoogleString mapped_domain_name;

  // We should rewrite origin.com/static to cdn.com/external
  const char kUrlToProxy[] = "http://origin.com/static/images/proxy_this.png";
  ASSERT_TRUE(MapRequest(context_gurl, kUrlToProxy, &mapped_domain_name,
                         &resolved_request));
  EXPECT_STREQ("http://cdn.com/external/images/proxy_this.png",
               resolved_request.Spec());

  // We should also rewrite proxy.com/external to cdn.com/external for looking
  // up cached resources on proxy.com.
  ASSERT_TRUE(
      MapRequest(context_gurl,
                         "http://proxy.com/external/images/proxy_this.png",
                         &mapped_domain_name,
                         &resolved_request));
  EXPECT_STREQ("http://cdn.com/external/images/proxy_this.png",
               resolved_request.Spec());

  GoogleString external_url;

  // Map CDN domain to Origin
  ASSERT_TRUE(MapProxy("http://cdn.com/external/images/proxy_this.png",
                        &external_url));
  EXPECT_EQ(kUrlToProxy, external_url);

  // Map Proxy domain to Origin
  ASSERT_TRUE(MapProxy("http://proxy.com/external/images/proxy_this.png",
                       &external_url));
  EXPECT_EQ(kUrlToProxy, external_url);

  // Just because we enabled proxying from origin.com/static, doesn't mean
  // we want to proxy from origin.com/evil or origin.com.
  EXPECT_FALSE(MapRequest(context_gurl, "http://origin.com/evil/gifar.gif",
                          &mapped_domain_name, &resolved_request));
  EXPECT_FALSE(MapRequest(context_gurl, "http://origin.com/gifar.gif",
                          &mapped_domain_name, &resolved_request));

  GoogleUrl proxy_url("http://proxy.com/external/a.b");
  EXPECT_TRUE(domain_lawyer_.IsProxyMapped(proxy_url));
  GoogleUrl non_proxy_url("http://proxy.com/a.b");
  EXPECT_FALSE(domain_lawyer_.IsProxyMapped(non_proxy_url));
  GoogleUrl origin_url("http://origin.com/static/a.b");
  EXPECT_FALSE(domain_lawyer_.IsProxyMapped(origin_url));
  GoogleUrl non_origin_url("http://origin.com/a.b");
  EXPECT_FALSE(domain_lawyer_.IsProxyMapped(non_origin_url));
  GoogleUrl cdn_url("http://cdn.com/external/a.b");
  EXPECT_TRUE(domain_lawyer_.IsProxyMapped(cdn_url));
  GoogleUrl non_cdn_url("http://cdn.com/a.b");
  EXPECT_FALSE(domain_lawyer_.IsProxyMapped(non_cdn_url));
}

TEST_F(DomainLawyerTest, ProxyExternalResourceFromHttps) {
  GoogleUrl context_gurl("http://origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddProxyDomainMapping(
      "http://origin.com/external", "https://external.com/static",
      StringPiece(), &message_handler_));

  // Map proxy_this.png to a subdirectory in origin.com.
  GoogleUrl resolved_request;
  GoogleString mapped_domain_name;
  const char kUrlToProxy[] =
      "https://external.com/static/images/proxy_this.png";
  ASSERT_TRUE(MapRequest(context_gurl, kUrlToProxy, &mapped_domain_name,
                         &resolved_request));
  EXPECT_STREQ("http://origin.com/external/", mapped_domain_name);
  EXPECT_STREQ("http://origin.com/external/images/proxy_this.png",
               resolved_request.Spec());

  // But when we fetch this resource, we won't find it in external.com so we
  // must map it back to origin.com/static.
  GoogleString origin_url;
  ASSERT_TRUE(MapProxy(resolved_request.Spec(), &origin_url));
  EXPECT_EQ(kUrlToProxy, origin_url);

  // Just because we enabled proxying from external.com/static, doesn't mean
  // we want to proxy from external.com/evil or external.com.
  EXPECT_FALSE(MapRequest(context_gurl, "https://external.com/evil/gifar.gif",
                          &mapped_domain_name, &resolved_request));
  EXPECT_FALSE(MapRequest(context_gurl, "https://external.com/gifar.gif",
                          &mapped_domain_name, &resolved_request));
}

TEST_F(DomainLawyerTest, ProxyAmbiguous) {
  GoogleUrl context_gurl("http://origin.com/index.html");
  ASSERT_TRUE(domain_lawyer_.AddProxyDomainMapping(
      "http://proxy.com/origin", "http://origin.com", "",  &message_handler_));

  GoogleString out;
  EXPECT_TRUE(MapProxy("http://proxy.com/origin/x", &out));
  EXPECT_STREQ("http://origin.com/x", out);

  // We don't allow proxy/proxy conflicts.
  EXPECT_FALSE(domain_lawyer_.AddProxyDomainMapping(
      "http://proxy.com/origin", "http://ambiguous.com", "",
      &message_handler_));

  EXPECT_TRUE(MapProxy("http://proxy.com/origin/x", &out));
  EXPECT_STREQ("http://origin.com/x", out);

  // We don't allow origin/proxy conflicts either.
  EXPECT_FALSE(AddOriginDomainMapping(
      "http://ambiguous.com", "http://proxy.com/origin"));

  EXPECT_TRUE(MapProxy("http://proxy.com/origin/x", &out));
  EXPECT_STREQ("http://origin.com/x", out);

  // But origin/origin conflicts are noisily ignored; second one wins.
  EXPECT_TRUE(AddOriginDomainMapping("http://origin1.com", "http://x.com"));
  EXPECT_TRUE(MapOrigin("http://x.com/y", &out));
  EXPECT_STREQ("http://origin1.com/y", out);

  EXPECT_TRUE(AddOriginDomainMapping("http://origin2.com", "http://x.com"));
  EXPECT_TRUE(MapOrigin("http://x.com/y", &out));
  EXPECT_STREQ("http://origin2.com/y", out) << "second one wins.";

  // It is also a bad idea to map the same origin to two different proxies.
  EXPECT_FALSE(domain_lawyer_.AddProxyDomainMapping(
      "http://proxy2.com/origin", "http://origin.com", "", &message_handler_));
}

TEST_F(DomainLawyerTest, Merge) {
  // Add some mappings for domain_lawywer_.
  ASSERT_TRUE(domain_lawyer_.AddDomain("http://d1.com/", &message_handler_));
  ASSERT_TRUE(AddRewriteDomainMapping(
      "http://cdn1.com", "http://www.o1.com"));
  ASSERT_TRUE(AddOriginDomainMapping(
      "http://localhost:8080", "http://o1.com:8080"));
  ASSERT_TRUE(domain_lawyer_.AddProxyDomainMapping(
      "http://proxy.com/origin", "http://origin.com", "", &message_handler_));

  // We'll also a mapping that will conflict, and one that won't.
  ASSERT_TRUE(AddOriginDomainMapping("http://dest1/", "http://common_src1"));
  ASSERT_TRUE(AddOriginDomainMapping("http://dest2/", "http://common_src2"));
  ASSERT_TRUE(AddShard("foo.com", "bar1.com,bar2.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent("foo.com", "bar1.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent("foo.com", "bar2.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent("bar1.com", "bar2.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent("bar1.com", "foo.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent("bar2.com", "foo.com"));
  EXPECT_TRUE(domain_lawyer_.DoDomainsServeSameContent("bar2.com", "bar1.com"));

  GoogleString out;
  EXPECT_TRUE(MapProxy("http://proxy.com/origin/x", &out));
  EXPECT_STREQ("http://origin.com/x", out);

  // Now add a similar set of mappings for another lawyer.
  DomainLawyer merged;
  ASSERT_TRUE(merged.AddDomain("http://d2.com/", &message_handler_));
  ASSERT_TRUE(merged.AddRewriteDomainMapping(
      "http://cdn2.com", "http://www.o2.com", &message_handler_));
  ASSERT_TRUE(merged.AddOriginDomainMapping(
      "http://localhost:8080", "http://o2.com:8080", "", &message_handler_));

  // Here's a different mapping for the same source.
  ASSERT_TRUE(merged.AddOriginDomainMapping(
      "http://dest3/", "http://common_src1", "", &message_handler_));
  ASSERT_TRUE(domain_lawyer_.AddOriginDomainMapping(
      "http://dest4/", "http://common_src3", "", &message_handler_));

  merged.Merge(domain_lawyer_);

  // Now the tests for both domains should work post-merger.

  GoogleString mapped;
  GoogleUrl resolved_request;
  GoogleUrl o1_index_gurl("http://www.o1.com/index.html");
  ASSERT_TRUE(merged.MapRequestToDomain(
      o1_index_gurl,
      "styles/blue.css", &mapped, &resolved_request, &message_handler_));
  EXPECT_STREQ("http://cdn1.com/", mapped);
  GoogleUrl o2_index_gurl("http://www.o2.com/index.html");
  ASSERT_TRUE(merged.MapRequestToDomain(
      o2_index_gurl,
      "styles/blue.css", &mapped, &resolved_request, &message_handler_));
  EXPECT_STREQ("http://cdn2.com/", mapped);

  bool is_proxy = true;
  GoogleString host_header;
  ASSERT_TRUE(merged.MapOrigin("http://o1.com:8080/a/b/c?d=f", &mapped,
                               &host_header, &is_proxy));
  EXPECT_STREQ("o1.com:8080", host_header);
  host_header.clear();
  EXPECT_FALSE(is_proxy);
  EXPECT_STREQ("http://localhost:8080/a/b/c?d=f", mapped);
  ASSERT_TRUE(merged.MapOrigin("http://o2.com:8080/a/b/c?d=f", &mapped,
                               &host_header, &is_proxy));
  EXPECT_STREQ("o2.com:8080", host_header);
  EXPECT_FALSE(is_proxy);
  EXPECT_STREQ("http://localhost:8080/a/b/c?d=f", mapped);

  // The conflict will be silently resolved to prefer the mapping from
  // the domain that got merged, which is domain_laywer_1, overriding
  // what was previously in the target.
  ASSERT_TRUE(merged.MapOrigin("http://common_src1", &mapped, &host_header,
                               &is_proxy));
  EXPECT_STREQ("http://dest1/", mapped);
  EXPECT_STREQ("common_src1", host_header);
  EXPECT_FALSE(is_proxy);

  // Now check the domains that were added.
  ASSERT_TRUE(merged.MapOrigin("http://common_src2", &mapped, &host_header,
                               &is_proxy));
  EXPECT_STREQ("http://dest2/", mapped);
  EXPECT_STREQ("common_src2", host_header);
  EXPECT_FALSE(is_proxy);

  ASSERT_TRUE(merged.MapOrigin("http://common_src3", &mapped, &host_header,
                               &is_proxy));
  EXPECT_STREQ("http://dest4/", mapped);
  EXPECT_STREQ("common_src3", host_header);
  EXPECT_FALSE(is_proxy);

  GoogleString shard;
  ASSERT_TRUE(merged.ShardDomain("http://foo.com/", 0, &shard));
  EXPECT_STREQ("http://bar1.com/", shard);

  EXPECT_TRUE(merged.DoDomainsServeSameContent("foo.com", "bar1.com"));
  EXPECT_TRUE(merged.DoDomainsServeSameContent("foo.com", "bar2.com"));
  EXPECT_TRUE(merged.DoDomainsServeSameContent("bar1.com", "bar2.com"));
  EXPECT_TRUE(merged.DoDomainsServeSameContent("bar1.com", "foo.com"));
  EXPECT_TRUE(merged.DoDomainsServeSameContent("bar2.com", "foo.com"));
  EXPECT_TRUE(merged.DoDomainsServeSameContent("bar2.com", "bar1.com"));

  EXPECT_TRUE(merged.DoDomainsServeSameContent("cdn1.com", "www.o1.com"));
  EXPECT_TRUE(merged.DoDomainsServeSameContent("cdn2.com", "www.o2.com"));
  EXPECT_FALSE(merged.DoDomainsServeSameContent("cdn1.com", "cdn2.com"));

  // The proxy settings survive the merge.
  mapped.clear();
  is_proxy = false;
  EXPECT_TRUE(merged.MapOrigin("http://proxy.com/origin/x", &mapped,
                               &host_header, &is_proxy));
  EXPECT_TRUE(is_proxy);
  EXPECT_STREQ("http://origin.com/x", mapped);
  EXPECT_STREQ("proxy.com", host_header);
}

TEST_F(DomainLawyerTest, AddMappingFailures) {
  // Corner cases.
  ASSERT_FALSE(AddRewriteDomainMapping("", "http://origin.com"));
  ASSERT_FALSE(AddRewriteDomainMapping("http://cdn.com", ""));
  ASSERT_FALSE(AddRewriteDomainMapping("http://cdn.com", ","));

  // Ensure that we ignore a mapping of a domain to itself.
  ASSERT_FALSE(AddRewriteDomainMapping("http://origin.com",
                                       "http://origin.com"));
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  ASSERT_FALSE(AddRewriteDomainMapping("http://origin.com/newroot",
                                       "http://origin.com"));
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());

  ASSERT_FALSE(AddRewriteDomainMapping("http://origin.com",
                                       "http://origin.com,"));
  ASSERT_FALSE(AddRewriteDomainMapping("http://origin.com",
                                       ",http://origin.com"));
  ASSERT_FALSE(AddRewriteDomainMapping("http://origin.com/newroot",
                                       "http://origin.com,"));
  ASSERT_FALSE(AddRewriteDomainMapping("http://origin.com/newroot",
                                       ",http://origin.com"));

  // You can never wildcard the target domains.
  EXPECT_FALSE(AddRewriteDomainMapping("foo*.com", "bar.com"));
  EXPECT_FALSE(AddOriginDomainMapping("foo*.com", "bar.com"));
  EXPECT_FALSE(AddShard("foo*.com", "bar.com"));

  // You can use wildcard in source domains for Rewrite and Origin, but not
  // Sharding.
  EXPECT_TRUE(AddRewriteDomainMapping("foo.com", "bar*.com"));
  EXPECT_TRUE(domain_lawyer_.AddOriginDomainMapping("foo.com", "bar*.com",
                                                    "", &message_handler_));
  EXPECT_FALSE(AddShard("foo.com", "bar*.com"));

  EXPECT_TRUE(AddShard("foo.com", "bar1.com,bar2.com"));
}

TEST_F(DomainLawyerTest, Shard) {
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  ASSERT_TRUE(AddShard("foo.com", "bar1.com,bar2.com"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString shard;
  ASSERT_TRUE(domain_lawyer_.ShardDomain("http://foo.com/", 0, &shard));
  EXPECT_STREQ("http://bar1.com/", shard);
  ASSERT_TRUE(domain_lawyer_.ShardDomain("http://foo.com/", 1, &shard));
  EXPECT_STREQ("http://bar2.com/", shard);
  EXPECT_FALSE(domain_lawyer_.ShardDomain("http://other.com/", 0, &shard));
}

TEST_F(DomainLawyerTest, ShardHttps) {
  EXPECT_FALSE(domain_lawyer_.can_rewrite_domains());
  ASSERT_TRUE(AddShard("https://foo.com", "https://bar1.com,https://bar2.com"));
  EXPECT_TRUE(domain_lawyer_.can_rewrite_domains());
  GoogleString shard;
  ASSERT_TRUE(domain_lawyer_.ShardDomain("https://foo.com/", 0, &shard));
  EXPECT_STREQ("https://bar1.com/", shard);
  ASSERT_TRUE(domain_lawyer_.ShardDomain("https://foo.com/", 1, &shard));
  EXPECT_STREQ("https://bar2.com/", shard);
  EXPECT_FALSE(domain_lawyer_.ShardDomain("https://other.com/", 0, &shard));
}

TEST_F(DomainLawyerTest, WillDomainChange) {
  ASSERT_TRUE(AddShard("foo.com", "bar1.com,bar2.com"));
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com", "http://origin.com"));
  EXPECT_TRUE(WillDomainChange("http://foo.com/"));
  EXPECT_TRUE(WillDomainChange("foo.com/"));
  EXPECT_TRUE(WillDomainChange("http://foo.com"));
  EXPECT_TRUE(WillDomainChange("foo.com"));
  EXPECT_TRUE(WillDomainChange("http://origin.com/"));
  EXPECT_TRUE(WillDomainChange("http://bar1.com/"));
  EXPECT_TRUE(WillDomainChange("http://bar2.com/"));
  EXPECT_FALSE(WillDomainChange("http://cdn.com/"));
  EXPECT_FALSE(WillDomainChange("http://other_domain.com/"));
}

TEST_F(DomainLawyerTest, WillDomainChangeSubdirectory) {
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com",
                                      "http://origin.com/subdir"));
  EXPECT_FALSE(WillDomainChange("http://origin.com/"));
  EXPECT_FALSE(WillDomainChange("http://origin.com/subdirx"));
  EXPECT_TRUE(WillDomainChange("http://origin.com/subdir/x"));
}

TEST_F(DomainLawyerTest, WillDomainChangeOnlyOneShard) {
  ASSERT_TRUE(AddShard("foo.com", "bar1.com"));
  ASSERT_TRUE(AddRewriteDomainMapping("http://cdn.com", "http://origin.com"));
  EXPECT_TRUE(WillDomainChange("http://foo.com/"));
  EXPECT_TRUE(WillDomainChange("foo.com/"));
  EXPECT_TRUE(WillDomainChange("http://foo.com"));
  EXPECT_TRUE(WillDomainChange("foo.com"));
  EXPECT_TRUE(WillDomainChange("http://origin.com/"));
  EXPECT_FALSE(WillDomainChange("http://bar1.com/"));
  EXPECT_FALSE(WillDomainChange("http://cdn.com/"));
  EXPECT_FALSE(WillDomainChange("http://other_domain.com/"));
}

TEST_F(DomainLawyerTest, MapRewriteToOriginDomain) {
  ASSERT_TRUE(AddRewriteDomainMapping("rewrite.com", "myhost.com"));
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost.com"));
  GoogleString mapped;

  // Check that we can warp all the way from the rewrite to localhost.
  ASSERT_TRUE(MapOrigin("http://rewrite.com/a/b/c?d=f", &mapped));
  EXPECT_STREQ("http://localhost/a/b/c?d=f", mapped);
}

TEST_F(DomainLawyerTest, MapShardToOriginDomain) {
  ASSERT_TRUE(AddRewriteDomainMapping("cdn.myhost.com", "myhost.com"));
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost.com"));
  ASSERT_TRUE(AddShard("cdn.myhost.com", "s1.com,s2.com"));
  GoogleString mapped;

  // Check that we can warp all the way from the cdn to localhost.
  ASSERT_TRUE(MapOrigin("http://s1.com/a/b/c?d=f", &mapped));
  EXPECT_STREQ("http://localhost/a/b/c?d=f", mapped);
  mapped.clear();
  ASSERT_TRUE(MapOrigin("http://s2.com/a/b/c?d=f", &mapped));
  EXPECT_STREQ("http://localhost/a/b/c?d=f", mapped);
}

TEST_F(DomainLawyerTest, ConflictedOrigin1) {
  ASSERT_TRUE(AddOriginDomainMapping(
      "localhost", "myhost.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  ASSERT_TRUE(AddOriginDomainMapping(
      "other", "myhost.com"));
  EXPECT_EQ(1, message_handler_.SeriousMessages());

  // The second one will win.
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://myhost.com/x", &mapped));
  EXPECT_STREQ("http://other/x", mapped);
}

TEST_F(DomainLawyerTest, NoConflictOnMerge1) {
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost1.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  // We are rewriting multiple source domains to the same domain.  Both
  // source domains have the same origin mapping so there is no conflict
  // message.
  ASSERT_TRUE(AddRewriteDomainMapping("cdn.com", "myhost1.com,myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  // Of course there's no conflict so it's obvious 'localhost' will win.  Check.
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://myhost1.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
  ASSERT_TRUE(MapOrigin("http://myhost2.com/y", &mapped));
  EXPECT_STREQ("http://localhost/y", mapped);
  ASSERT_TRUE(MapOrigin("http://cdn.com/z", &mapped));
  EXPECT_STREQ("http://localhost/z", mapped);
}

TEST_F(DomainLawyerTest, ConflictedOrigin2) {
  ASSERT_TRUE(AddOriginDomainMapping("origin1.com", "myhost1.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("origin2.com", "myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  // We are rewriting multiple source domains to the same domain.  Both
  // source domains have the *different* origin mappings so there will be a
  // conflict message.
  ASSERT_TRUE(AddRewriteDomainMapping("cdn.com", "myhost1.com,myhost2.com"));
  EXPECT_EQ(1, message_handler_.SeriousMessages());

  // The second mapping will win for the automatic propagation for "cdn.com".
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://cdn.com/x", &mapped));
  EXPECT_STREQ("http://origin2.com/x", mapped);

  // However, "myhost1.com"'s explicitly set origin will not be overridden.
  ASSERT_TRUE(MapOrigin("http://myhost1.com/y", &mapped));
  EXPECT_STREQ("http://origin1.com/y", mapped);
}

TEST_F(DomainLawyerTest, NoShardConflict) {
  // We are origin-mapping multiple source domains to the same domain.
  // Even though we've overspecified the origin domain in this graph,
  // there are no conflict messages because the origins are the same.
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost1.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddRewriteDomainMapping("cdn.com", "myhost1.com,myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddShard("cdn.com", "s1.com,s2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  // Unambiguous mappings from either shard or rewrite domain.
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://cdn.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
  mapped.clear();
  ASSERT_TRUE(MapOrigin("http://s1.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
  mapped.clear();
  ASSERT_TRUE(MapOrigin("http://s2.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
}

TEST_F(DomainLawyerTest, NoShardConflictReverse) {
  // This is the same exact test as NoShardConflict, but now we set up
  // the shards first, then the rewrite domain, then the origin mappings.
  ASSERT_TRUE(AddShard("cdn.com", "s1.com,s2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddRewriteDomainMapping("cdn.com", "myhost1.com,myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost1.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  // Unambiguous mappings from either shard or rewrite domain.
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://cdn.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
  mapped.clear();
  ASSERT_TRUE(MapOrigin("http://s1.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
  mapped.clear();
  ASSERT_TRUE(MapOrigin("http://s2.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
}

TEST_F(DomainLawyerTest, NoShardConflictScramble) {
  // Yet another copy of NoShardConflict, but do the rewrite-mapping last.
  ASSERT_TRUE(AddShard("cdn.com", "s1.com,s2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost1.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("localhost", "myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddRewriteDomainMapping("cdn.com", "myhost1.com,myhost2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  // Unambiguous mappings from either shard or rewrite domain.
  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://cdn.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
  mapped.clear();
  ASSERT_TRUE(MapOrigin("http://s1.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
  mapped.clear();
  ASSERT_TRUE(MapOrigin("http://s2.com/x", &mapped));
  EXPECT_STREQ("http://localhost/x", mapped);
}

TEST_F(DomainLawyerTest, ShardConflict1) {
  ASSERT_TRUE(AddShard("cdn1.com", "s1.com,s2.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());

  ASSERT_FALSE(AddShard("cdn2.com", "s2.com,s3.com"));
  EXPECT_EQ(1, message_handler_.SeriousMessages());
}

TEST_F(DomainLawyerTest, RewriteOriginCycle) {
  ASSERT_TRUE(AddShard("b.com", "a.com"));
  ASSERT_TRUE(AddRewriteDomainMapping("b.com", "a.com"));
  // We now have "a.com" and "b.com" in a shard/rewrite cycle.  That's
  // ugly and we don't actually detect that because we don't have a
  // graph traversal that can detect it until we start applying origin
  // domains, which auto-propagate.
  //
  // We will have no serious errors reported until we create the
  // conflict which will chase pointers in a cycle, which gets cut
  // by breadcrumbing, but we wind up with 2 serious errors from
  // one call.

  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("origin1.com", "a.com"));
  EXPECT_EQ(0, message_handler_.SeriousMessages());
  ASSERT_TRUE(AddOriginDomainMapping("origin2.com", "b.com"));
  EXPECT_EQ(2, message_handler_.SeriousMessages());
}

TEST_F(DomainLawyerTest, WildcardOrder) {
  ASSERT_TRUE(AddOriginDomainMapping("host1", "abc*.com"));
  ASSERT_TRUE(AddOriginDomainMapping("host2", "*z.com"));

  GoogleString mapped;
  ASSERT_TRUE(MapOrigin("http://abc.com/x", &mapped));
  EXPECT_STREQ("http://host1/x", mapped);
  ASSERT_TRUE(MapOrigin("http://z.com/x", &mapped));
  EXPECT_STREQ("http://host2/x", mapped);

  // Define a second lawyer with definitions "*abc*.com" which should
  // come after "abc*.com".
  DomainLawyer second_lawyer, merged_lawyer;
  ASSERT_TRUE(second_lawyer.AddOriginDomainMapping("host3", "*abc*.com",
                                                   "", &message_handler_));
  ASSERT_TRUE(second_lawyer.AddOriginDomainMapping(
      "host1", "abc*.com", "", &message_handler_));  // duplicate entry.
  merged_lawyer.Merge(domain_lawyer_);
  merged_lawyer.Merge(second_lawyer);
  EXPECT_EQ(3, merged_lawyer.num_wildcarded_domains());

  // Hopefully we didn't bork the order of "abc* and "*".  Note that just
  // iterating over a std::set will yield the "*" first, as '*' is ascii
  // 42 and 'a' is ascii 97, and the domain-map is over GoogleString.
  bool is_proxy = true;
  GoogleString host_header;
  ASSERT_TRUE(merged_lawyer.MapOrigin("http://abc.com/x", &mapped,
                                      &host_header, &is_proxy));
  EXPECT_STREQ("http://host1/x", mapped);
  EXPECT_FALSE(is_proxy);
  is_proxy = true;
  ASSERT_TRUE(merged_lawyer.MapOrigin("http://xyz.com/x", &mapped,
                                      &host_header, &is_proxy));
  EXPECT_STREQ("http://host2/x", mapped);
  EXPECT_FALSE(is_proxy);
  is_proxy = true;
  ASSERT_TRUE(merged_lawyer.MapOrigin("http://xabc.com/x", &mapped,
                                      &host_header, &is_proxy));
  EXPECT_STREQ("http://host3/x", mapped);
  EXPECT_FALSE(is_proxy);
}

TEST_F(DomainLawyerTest, ComputeSignatureTest) {
  DomainLawyer first_lawyer, second_lawyer;
  ASSERT_TRUE(first_lawyer.AddOriginDomainMapping("host1", "*abc*.com", "",
                                                  &message_handler_));
  ASSERT_TRUE(first_lawyer.AddOriginDomainMapping("host2", "*def*.com", "h2",
                                                  &message_handler_));

  ASSERT_TRUE(second_lawyer.AddRewriteDomainMapping("cdn.com",
                                                    "myhost1.com,myhost2.com",
                                                    &message_handler_));
  EXPECT_STREQ("D:http://*abc*.com/__a_" "O:http://host1/_"
               "-"
               "D:http://*def*.com/__a_" "O:http://host2/_"
               "-"
               "D:http://host1/__n_"
               "-"
               "D:http://host2/__n_" "H:h2|"
               "-",
               first_lawyer.Signature());
  EXPECT_STREQ("D:http://cdn.com/__a_"
               "-"
               "D:http://myhost1.com/__a_" "R:http://cdn.com/_"
               "-"
               "D:http://myhost2.com/__a_" "R:http://cdn.com/_"
               "-",
               second_lawyer.Signature());

  EXPECT_TRUE(first_lawyer.AddShard("domain1", "shard", &message_handler_));
  EXPECT_STREQ("D:http://*abc*.com/__a_" "O:http://host1/_"
               "-"
               "D:http://*def*.com/__a_" "O:http://host2/_"
               "-"
               "D:http://domain1/__a_" "S:http://shard/_"
               "-"
               "D:http://host1/__n_"
               "-"
               "D:http://host2/__n_" "H:h2|"
               "-"
               "D:http://shard/__a_" "R:http://domain1/_"
               "-",
               first_lawyer.Signature());
}

TEST_F(DomainLawyerTest, ToStringTest) {
  DomainLawyer first_lawyer, second_lawyer;
  EXPECT_TRUE(first_lawyer.AddDomain("static.example.com", &message_handler_));
  EXPECT_TRUE(first_lawyer.AddOriginDomainMapping("host1", "*abc*.com", "",
                                                  &message_handler_));
  EXPECT_STREQ(
      "http://*abc*.com/ Auth OriginDomain:http://host1/\n"
      "http://host1/\n"
      "http://static.example.com/ Auth\n",
      first_lawyer.ToString());

  EXPECT_TRUE(second_lawyer.AddRewriteDomainMapping("myhost.cdn.com",
                                                    "myhost1.com,myhost2.com",
                                                    &message_handler_));
  EXPECT_TRUE(
      second_lawyer.AddShard("domain1", "shard,shard2", &message_handler_));
  EXPECT_STREQ(
      "http://domain1/ Auth Shards:{http://shard/, http://shard2/}\n"
      "http://myhost.cdn.com/ Auth\n"
      "http://myhost1.com/ Auth RewriteDomain:http://myhost.cdn.com/\n"
      "http://myhost2.com/ Auth RewriteDomain:http://myhost.cdn.com/\n"
      "http://shard/ Auth RewriteDomain:http://domain1/\n"
      "http://shard2/ Auth RewriteDomain:http://domain1/\n",
      second_lawyer.ToString());
}

TEST_F(DomainLawyerTest, IsOriginKnownTest) {
  DomainLawyer lawyer;
  lawyer.AddDomain("a.com", &message_handler_);
  lawyer.AddDomain("a.com:42", &message_handler_);
  lawyer.AddDomain("https://a.com:43", &message_handler_);
  lawyer.AddRewriteDomainMapping("b.com", "c.com", &message_handler_);
  lawyer.AddOriginDomainMapping("e.com", "d.com", "", &message_handler_);
  lawyer.AddShard("f.com", "s1.f.com,s2.f.com", &message_handler_);

  GoogleUrl z_com("http://z.com");
  EXPECT_FALSE(lawyer.IsOriginKnown(z_com));

  GoogleUrl a_com("http://a.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(a_com));

  GoogleUrl a_com_42("http://a.com:42/sardine");
  EXPECT_TRUE(lawyer.IsOriginKnown(a_com_42));

  GoogleUrl a_com_43("http://a.com:43/bass");
  EXPECT_FALSE(lawyer.IsOriginKnown(a_com_43));

  GoogleUrl s_a_com_43("https://a.com:43/bass");
  EXPECT_TRUE(lawyer.IsOriginKnown(s_a_com_43));

  GoogleUrl s_a_com_44("https://a.com:44/bass");
  EXPECT_FALSE(lawyer.IsOriginKnown(s_a_com_44));

  GoogleUrl b_com("http://b.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(b_com));

  GoogleUrl c_com("http://c.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(c_com));

  GoogleUrl d_com("http://d.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(d_com));

  GoogleUrl e_com("http://e.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(e_com));

  GoogleUrl f_com("http://f.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(f_com));

  GoogleUrl s1_f_com("http://s1.f.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(s1_f_com));

  GoogleUrl s2_f_com("http://s2.f.com");
  EXPECT_TRUE(lawyer.IsOriginKnown(s2_f_com));
}

TEST_F(DomainLawyerTest, NoAbsoluteUrlPath) {
  DomainLawyer lawyer;
  lawyer.AddOriginDomainMapping("b.com", "a.com", "", &message_handler_);

  GoogleUrl foo("http://a.com/foo");
  GoogleString out;
  GoogleString host_header;
  bool is_proxy = true;
  EXPECT_TRUE(lawyer.MapOriginUrl(foo, &out, &host_header, &is_proxy));
  EXPECT_STREQ("http://b.com/foo", out);
  EXPECT_FALSE(is_proxy);

  // Make sure we don't resolve the path: data:image/jpeg as an absolute URL.
  GoogleUrl data("http://a.com/data:image/jpeg");
  out.clear();
  EXPECT_TRUE(lawyer.MapOriginUrl(data, &out, &host_header, &is_proxy));
  EXPECT_STREQ("http://b.com/data:image/jpeg", out);
  EXPECT_FALSE(is_proxy);
}

TEST_F(DomainLawyerTest, AboutBlank) {
  DomainLawyer lawyer;
  lawyer.AddOriginDomainMapping("b.com", "a.com", "", &message_handler_);

  GoogleUrl foo("about:blank");
  GoogleString out;
  GoogleString host_header;
  bool is_proxy = true;
  EXPECT_FALSE(lawyer.MapOriginUrl(foo, &out, &host_header, &is_proxy));
}

TEST_F(DomainLawyerTest, StripProxySuffix) {
  DomainLawyer lawyer;
  GoogleUrl gurl("http://example.com.suffix/path");
  GoogleString host, url = gurl.Spec().as_string();
  EXPECT_FALSE(lawyer.can_rewrite_domains());
  EXPECT_FALSE(lawyer.StripProxySuffix(gurl, &url, &host));
  lawyer.set_proxy_suffix(".suffix");
  EXPECT_TRUE(lawyer.can_rewrite_domains());
  EXPECT_TRUE(lawyer.StripProxySuffix(gurl, &url, &host));
  EXPECT_STREQ("http://example.com/path", url);
  EXPECT_STREQ("example.com", host);

  // The ':80' will get removed by GoogleUrl.
  GoogleUrl http_gurl_80("http://example.com.suffix:80/path");
  url = http_gurl_80.Spec().as_string();
  host.clear();
  url.clear();
  EXPECT_TRUE(lawyer.StripProxySuffix(http_gurl_80, &url, &host));
  EXPECT_STREQ("http://example.com/path", url);
  EXPECT_STREQ("example.com", host);

  // However an ':81' makes the proxy-suffix mismatch.
  GoogleUrl http_gurl_81("http://example.com.suffix:81/path");
  url.clear();
  host.clear();
  EXPECT_FALSE(lawyer.StripProxySuffix(http_gurl_81, &url, &host));

  // 443 on http.  We need to understand why we see this in Apache slurping
  // with a Firefox proxy, but punt for now.
  GoogleUrl http_gurl_443("http://example.com.suffix:443/path");
  url.clear();
  host.clear();
  EXPECT_FALSE(lawyer.StripProxySuffix(http_gurl_443, &url, &host));

  // 443 on https -- that should canonicalize out in GoogleUrl.
  GoogleUrl https_gurl_443("https://example.com.suffix:443/path");
  url.clear();
  host.clear();
  EXPECT_TRUE(lawyer.StripProxySuffix(https_gurl_443, &url, &host));
  EXPECT_STREQ("https://example.com/path", url);
  EXPECT_STREQ("example.com", host);

  GoogleUrl https_gurl("https://example.com.suffix/path");
  url.clear();
  host.clear();
  EXPECT_TRUE(lawyer.StripProxySuffix(https_gurl, &url, &host));
  EXPECT_STREQ("https://example.com/path", url);
  EXPECT_STREQ("example.com", host);
}

TEST_F(DomainLawyerTest, AddProxySuffix) {
  DomainLawyer lawyer;
  GoogleUrl base("http://www.example.com.suffix");
  lawyer.set_proxy_suffix(".suffix");
  EXPECT_TRUE(lawyer.can_rewrite_domains());

  // No need to change relative URLs.
  GoogleString url = "relative.html";
  EXPECT_FALSE(lawyer.AddProxySuffix(base, &url));

  // An absolute reference to a new destination in the origin domain gets
  // suffixed.
  url = "http://www.example.com/absolute.html";
  EXPECT_TRUE(lawyer.AddProxySuffix(base, &url));
  EXPECT_STREQ("http://www.example.com.suffix/absolute.html", url);

  // It also works even if the reference is a domain that's related to the
  // base, by consulting the known suffixes list via domain_registry.
  url = "http://other.example.com/absolute.html";
  EXPECT_TRUE(lawyer.AddProxySuffix(base, &url));
  EXPECT_STREQ("http://other.example.com.suffix/absolute.html", url);

  // However a link to a completely unrelated domain is left unchanged.
  url = "http://other.com/x.html";
  EXPECT_FALSE(lawyer.AddProxySuffix(base, &url));

  // Link to same domain on HTTPS is also OK.
  url = "https://www.example.com/absolute.html";
  EXPECT_TRUE(lawyer.AddProxySuffix(base, &url));
  EXPECT_STREQ("https://www.example.com.suffix/absolute.html", url);
}

TEST_F(DomainLawyerTest, MapNewUrlDomain) {
  StringPiece from_host("www.foo.com/123/www.xyz.com/");
  StringPiece origin_host("www.xyz.com");
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      origin_host, from_host, "", &message_handler_));
  GoogleString origin_url;

  ASSERT_TRUE(MapOrigin("http://www.foo.com/123/www.xyz.com/", &origin_url));
  EXPECT_STREQ("http://www.xyz.com/", origin_url);

  ASSERT_TRUE(MapOrigin("http://www.foo.com/123/www.xyz.com/a/b", &origin_url));
  EXPECT_STREQ("http://www.xyz.com/a/b", origin_url);

  ASSERT_TRUE(
      MapOrigin("https://www.foo.com/123/www.xyz.com/a/b", &origin_url));
  EXPECT_STREQ("https://www.xyz.com/a/b", origin_url);

  ASSERT_TRUE(
      MapOrigin("http://www.foo.com/123/www.xyz.com/#fragment", &origin_url));
  EXPECT_STREQ("http://www.xyz.com/#fragment", origin_url);
}

TEST_F(DomainLawyerTest, MapNewUrlDomainWithoutDomainSuffix) {
  StringPiece from_host("www.foo.com/www.baz.com/");
  StringPiece origin_host("www.baz.com");
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      origin_host, from_host, "", &message_handler_));
  GoogleString origin_url;

  ASSERT_TRUE(MapOrigin("http://www.foo.com/www.baz.com/bar", &origin_url));
  EXPECT_STREQ("http://www.baz.com/bar", origin_url);
}

TEST_F(DomainLawyerTest, MapUrlDomainWithLeaf) {
  StringPiece from_host("www.foo.com");
  StringPiece origin_host("www.baz.com");
  ASSERT_TRUE(domain_lawyer_.AddTwoProtocolOriginDomainMapping(
      origin_host, from_host, "", &message_handler_));
  GoogleString origin_url;

  ASSERT_TRUE(MapOrigin("http://www.foo.com/bar", &origin_url));
  EXPECT_STREQ("http://www.baz.com/bar", origin_url);
}
}  // namespace net_instaweb
