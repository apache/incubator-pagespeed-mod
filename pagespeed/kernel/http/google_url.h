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

//         nforman@google.com  (Naomi Forman)

#ifndef PAGESPEED_KERNEL_HTTP_GOOGLE_URL_H_
#define PAGESPEED_KERNEL_HTTP_GOOGLE_URL_H_

#include <cstddef>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"


#include "third_party/chromium/src/googleurl/src/gurl.h"
#include "third_party/chromium/src/googleurl/src/url_parse.h"
#include "third_party/chromium/src/googleurl/src/url_util.h"

namespace net_instaweb {

// Prepare for flattening of the namespaces in newer Chromiums.

namespace url {

using url_canon::Replacements;
using url_parse::PORT_UNSPECIFIED;
using url_parse::PORT_INVALID;
using url_parse::Component;
using url_parse::Parsed;
using url_util::Initialize;
using url_util::Shutdown;

};  // namespace url


enum UrlRelativity {
  kAbsoluteUrl,   // http://example.com/foo/bar/file.ext?k=v#f
  kNetPath,       // //example.com/foo/bar/file.ext?k=v#f
  kAbsolutePath,  // /foo/bar/file.ext?k=v#f
  kRelativePath,  // bar/file.ext?k=v#f
};

class GoogleUrl {
 public:
  explicit GoogleUrl(const GoogleString& spec);
  explicit GoogleUrl(StringPiece sp);
  explicit GoogleUrl(const char* str);
  // The following three constructors create a new GoogleUrl by resolving the
  // String(Piece) against the base.
  GoogleUrl(const GoogleUrl& base, const GoogleString& relative);
  GoogleUrl(const GoogleUrl& base, StringPiece relative);
  GoogleUrl(const GoogleUrl& base, const char* relative);
  GoogleUrl();

  void Swap(GoogleUrl* google_url);

  bool Reset(StringPiece new_url);
  bool Reset(const GoogleUrl& new_url);
  bool Reset(const GoogleUrl& base, const GoogleString& relative);
  bool Reset(const GoogleUrl& base, StringPiece relative);
  bool Reset(const GoogleUrl& base, const char* relative);

  // Resets this URL to be invalid.
  void Clear();

  // Is a valid web (HTTP or HTTPS) URL. Most users will want this.
  bool IsWebValid() const;
  // Also allows data: URLs.
  bool IsWebOrDataValid() const;
  // Only use for you don't care about scheme, just need to know that URL is
  // well-formed. Note: This will accept things like "foo:bar".
  bool IsAnyValid() const;

  // Returns a new GoogleUrl that is identical to this one but with additional
  // query param.  Name and value should both be unescaped.
  // This is a factory method that returns a pointer, the caller is responsible
  // for the management of the new object's memory (the caller owns the
  // pointer).
  GoogleUrl* CopyAndAddQueryParam(StringPiece unescaped_name,
                                  StringPiece unescaped_value) const;
  // Same as CopyAndAddQueryParam() but name and value must already be escaped.
  // Most users should use CopyAndAddQueryParam() instead for safety.
  GoogleUrl* CopyAndAddEscapedQueryParam(
      StringPiece escaped_name, StringPiece escaped_value) const;

  // For "http://a.com/b/c/d?e=f/g#r" returns "http://a.com/b/c/d"
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece AllExceptQuery() const;

  // For "http://a.com/b/c/d?e=f#r" returns "#r"
  // For "http://a.com/b/c/d?e=f#r1#r2" returns "#r1#r2"
  // AllExceptQuery() + Query() + AllAfterQuery() = Spec() when url is valid
  // Different from Parsed.ref in the case of multiple "#"s after "?"
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece AllAfterQuery() const;

  // For "http://a.com/b/c/d?e=f/g" returns "http://a.com/b/c/",
  // including trailing slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece AllExceptLeaf() const;

  // For "http://a.com/b/c/d?e=f/g" returns "d?e=f/g", omitting leading slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece LeafWithQuery() const;

  // For "http://a.com/b/c/d?e=f/g" returns "d", omitting leading slash.
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece LeafSansQuery() const;

  // For "http://a.com/b/c/d?E=f/g" returns "/b/c/d?e=f/g"
  // including leading slash
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece PathAndLeaf() const;

  // For "http://a.com/b/c/d/g.html" returns "/b/c/d/" including leading and
  // trailing slashes.
  // For queries, "http://a.com/b/c/d?E=f/g" returns "/b/c/".
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece PathSansLeaf() const;

  // For "http://a.com/b/c/d?E=f/g returns "/b/c/d" including leading slash,
  // and excluding the query.
  StringPiece PathSansQuery() const;

  // Scheme-relative URL. Spec() == Scheme() + ":" + NetPath().
  // Named based on http://tools.ietf.org/html/rfc1808#section-2.2
  // For "http://a.com/b/c/d?E=f/g#r" returns "//a.com/b/c/d?E=f/g#r".
  // For "file:///tmp/foo" returns "///tmp/foo".
  StringPiece NetPath() const;

  // Extracts the filename portion of the path and returns it. The filename
  // is everything after the last slash in the path. This may be empty.
  GoogleString ExtractFileName() const;

  StringPiece Host() const;

  // For "http://a.com/b/c.html" returns "a.com".
  // For "http://a.com:1234/b/c.html" returns "a.com:1234".
  StringPiece HostAndPort() const;

  // For "http://a.com/b/c/d?e=f/g returns "http://a.com"
  // without trailing slash
  // Returns a StringPiece, only valid for the lifetime of this object.
  StringPiece Origin() const;

  // Returns the query-string, not including the "?".  Note that the
  // query will be in escaped syntax, and is suitable for passing to
  // QueryParams for parsing and unescaping.
  StringPiece Query() const;

  // Returns scheme of stored url.
  StringPiece Scheme() const;

  // It is illegal to call this for invalid urls (check IsWebValid() first).
  StringPiece Spec() const;

  // Returns gurl_.spec_ without checking to see if it's valid or empty.
  StringPiece UncheckedSpec() const;

  // This method is primarily for printf purposes.
  const char* spec_c_str() const {
    return gurl_.possibly_invalid_spec().c_str();
  }

  int IntPort() const { return gurl_.IntPort(); }

  // Returns the effective port number, which is dependent on the scheme.
  int EffectiveIntPort() const { return gurl_.EffectiveIntPort(); }

  // Returns the default port for given scheme, or url::PORT_UNSPECIFIED
  // if the scheme isn't recognized. Scheme is expected to be in lowercase.
  static int DefaultPortForScheme(StringPiece scheme);

  bool is_empty() const { return gurl_.is_empty(); }
  bool has_scheme() const { return gurl_.has_scheme(); }
  bool has_path() const { return gurl_.has_path(); }
  bool has_query() const { return gurl_.has_query(); }

  bool SchemeIs(const char* lower_ascii_scheme) const {
    return gurl_.SchemeIs(lower_ascii_scheme);
  }

  // TODO(nforman): get GURL to take a StringPiece so we don't have to do
  // any copying.
  bool SchemeIs(StringPiece lower_ascii_scheme) const {
    return gurl_.SchemeIs(lower_ascii_scheme.as_string().c_str());
  }

  // Find out how relative the URL string is.
  static UrlRelativity FindRelativity(StringPiece url);

  // If possible, produce a URL as relative as url_relativity, relative to
  // base_url. If not possible, simply returns the absolute URL string.
  // Returns a StringPiece, only valid for the lifetime of this object.
  //
  // It is illegal to call this for invalid urls (check IsWebValid() first).
  StringPiece Relativize(UrlRelativity url_relativity,
                         const GoogleUrl& base_url) const;

  // Defiant equality operator!
  bool operator==(const GoogleUrl& other) const {
    return gurl_ == other.gurl_;
  }
  bool operator!=(const GoogleUrl& other) const {
    return gurl_ != other.gurl_;
  }

  // Unescape a query parameter, converting all %XX to the the actual char 0xXX.
  // This also converts '+' to ' ' which is valid only in query parameters.
  // For example, this will convert "foo%21bar+baz" to "foo!bar baz".
  //
  // This will work with strings that have embedded NULs and %00s.
  //
  // TODO(jmarantz): Change signature to return a bool so if the escaped
  // syntax was not valid, we can help the caller avoid relying on this value.
  static GoogleString UnescapeQueryParam(StringPiece escaped) {
    return UnescapeHelper(escaped, true);
  }

  // UnescapeQueryParam converts "+" to " ", but that is not correct for other
  // parts of a URL.
  static GoogleString UnescapeIgnorePlus(StringPiece escaped) {
    return UnescapeHelper(escaped, false);
  }

  // Escapes a string for use in a URL query param.
  //
  // This function escapes reserved chars (ex: '/', ':', '?', '&', etc.).
  static GoogleString EscapeQueryParam(StringPiece unescaped);

  // Produces a sanitary, escaped version of a URL. The URL may already have
  // some mix of escaped and non-escaped sections. This function is idempotent
  // and can safely be used on any URL without changing the meaning according
  // to RFC 3986.
  //
  // Result will not contain: 0x00-0x1F SPC "<>\^`{|} 0x7F-0xFF
  // Result may contain: a-z A-Z 0-9 -._~:/?#[]@!$&'()*+,;=%
  static GoogleString Sanitize(StringPiece url);

  // Returns the canonical representation of a given path component of URL.
  // Will also prepend / if it's not there. This will follow the same rules for
  // what's in %-encoded form and what isn't as GoogleUrl does.
  static GoogleString CanonicalizePath(StringPiece path);

 private:
  // Returned by *Position methods when that position is not well-defined.
  static const size_t npos;

  static const char kReservedChars[];
  static bool IsReservedChar(char c);

  explicit GoogleUrl(const GURL& gurl);
  void Init();

  static size_t LeafEndPosition(const GURL& gurl);
  static size_t LeafStartPosition(const GURL& gurl);
  static size_t PathStartPosition(const GURL& gurl);
  size_t LeafEndPosition() const;
  size_t LeafStartPosition() const;
  size_t PathStartPosition() const;
  static GoogleString UnescapeHelper(StringPiece escaped,
                                     bool convert_plus_to_space);

  // Resolves a URL against a base. Returns whether the resolution worked.
  inline bool ResolveHelper(const GURL& base, const std::string& path_and_leaf);

  GURL gurl_;
  bool is_web_valid_;
  bool is_web_or_data_valid_;

  DISALLOW_COPY_AND_ASSIGN(GoogleUrl);
};  // class GoogleUrl

}  // namespace net_instaweb


#endif  // PAGESPEED_KERNEL_HTTP_GOOGLE_URL_H_
