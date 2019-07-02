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

#include <map>
#include <set>
#include <utility>  // for std::pair
#include <vector>

#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/stl_util.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/wildcard.h"
#include "pagespeed/kernel/http/domain_registry.h"
#include "pagespeed/kernel/http/google_url.h"

namespace net_instaweb {

class DomainLawyer::Domain {
 public:
  explicit Domain(const StringPiece& name)
      : wildcard_(name),
        name_(name.data(), name.size()),
        rewrite_domain_(NULL),
        origin_domain_(NULL),
        authorized_(false),
        cycle_breadcrumb_(false),
        is_proxy_(false) {
  }

  bool IsWildcarded() const { return !wildcard_.IsSimple(); }
  bool Match(const StringPiece& domain) { return wildcard_.Match(domain); }
  Domain* rewrite_domain() const { return rewrite_domain_; }
  Domain* origin_domain() const { return origin_domain_; }
  const GoogleString& name() const { return name_; }

  // When multiple domains are mapped to the same rewrite-domain, they
  // should have consistent origins.  If they don't, we print an error
  // message but we keep rolling.  This is because we don't want to
  // introduce an incremental change that would invalidate existing
  // pagespeed.conf files.
  //
  void MergeOrigin(Domain* origin_domain, MessageHandler* handler) {
    if (cycle_breadcrumb_) {
      // See DomainLawyerTest.RewriteOriginCycle
      return;
    }
    cycle_breadcrumb_ = true;
    if ((origin_domain != origin_domain_) && (origin_domain != NULL)) {
      if (origin_domain_ != NULL) {
        if (handler != NULL) {
          handler->Message(kError,
                           "RewriteDomain %s has conflicting origins %s and "
                           "%s, overriding to %s",
                           name_.c_str(),
                           origin_domain_->name_.c_str(),
                           origin_domain->name_.c_str(),
                           origin_domain->name_.c_str());
        }
      }
      origin_domain_ = origin_domain;
      for (int i = 0; i < num_shards(); ++i) {
        shards_[i]->MergeOrigin(origin_domain, handler);
      }
      if (rewrite_domain_ != NULL) {
        rewrite_domain_->MergeOrigin(origin_domain, handler);
      }
    }
    cycle_breadcrumb_ = false;
  }

  // handler==NULL means this is happening from a 'merge' so we will
  // silently let the new rewrite_domain win.
  bool SetRewriteDomain(Domain* rewrite_domain, MessageHandler* handler) {
    if (rewrite_domain == rewrite_domain_) {
      return true;
    }

    // Don't break old configs on this new consistency check
    // for ModPagespeedMapRewriteDomain.  However,
    // ModPagespeedMapProxyDomain has no legacy configuration, and
    // in that context it's a functional problem to have multiple
    // proxy directories mapped to a single origin, so we must fail
    // the configuration.
    if (is_proxy_ && (rewrite_domain_ != NULL)) {
      if (handler != NULL) {
        handler->Message(kError,
                         "ProxyDomain %s has conflicting proxies %s and %s",
                         name_.c_str(),
                         rewrite_domain_->name_.c_str(),
                         rewrite_domain->name_.c_str());
      }
      return false;
    }

    rewrite_domain_ = rewrite_domain;
    rewrite_domain->MergeOrigin(origin_domain_, handler);
    return true;  // don't break old configs on this new consistency check.
  }

  // handler==NULL means this is happening from a 'merge' so we will
  // silently let the new origin_domain win.
  bool SetOriginDomain(Domain* origin_domain, MessageHandler* handler) {
    if (origin_domain == origin_domain_) {
      return true;
    }

    // Don't break old configs on this new consistency check
    // for ModPagespeedMapOriginDomain.  However,
    // ModPagespeedMapProxyDomain has no legacy configuration, and
    // in that context it's a functional problem to have the same
    // proxy directory mapped to multiple origins, so we must fail
    // the configuration.
    if ((origin_domain_ != NULL) &&
        (origin_domain_->is_proxy_ || origin_domain->is_proxy_)) {
      if (handler != NULL) {
        handler->Message(kError,
                         "ProxyDomain %s has conflicting origins %s and %s",
                         name_.c_str(),
                         origin_domain_->name_.c_str(),
                         origin_domain->name_.c_str());
      }
      return false;
    }

    MergeOrigin(origin_domain, handler);
    if (rewrite_domain_ != NULL) {
      rewrite_domain_->MergeOrigin(origin_domain_, handler);
    }

    return true;
  }

  bool SetProxyDomain(Domain* origin_domain, MessageHandler* handler) {
    origin_domain->is_proxy_ = true;
    return (SetOriginDomain(origin_domain, handler) &&
            origin_domain->SetRewriteDomain(this, handler));
  }

  // handler==NULL means this is happening from a 'merge' so we will
  // silently let the new rewrite_domain win.
  bool SetShardFrom(Domain* rewrite_domain, MessageHandler* handler) {
    if ((rewrite_domain_ != rewrite_domain) && (rewrite_domain_ != NULL)) {
      if (handler != NULL) {
        // We only treat this as an error when the handler is non-null.  We
        // use a null handler during merges, and will do the best we can
        // to get correct behavior.
        handler->Message(kError,
                         "Shard %s has conflicting rewrite_domain %s and %s",
                         name_.c_str(),
                         rewrite_domain_->name_.c_str(),
                         rewrite_domain->name_.c_str());
        return false;
      }
    }
    MergeOrigin(rewrite_domain->origin_domain_, handler);
    rewrite_domain->shards_.push_back(this);
    rewrite_domain_ = rewrite_domain;
    return true;
  }

  void set_authorized(bool authorized) { authorized_ = authorized; }
  int num_shards() const { return shards_.size(); }
  void set_host_header(StringPiece x) { x.CopyToString(&host_header_); }
  const GoogleString& host_header() const { return host_header_; }

  // Indicates whether this domain is authorized when found in URLs
  // HTML files are as direct requests to the web server.  Domains
  // get authorized by mentioning them in ModPagespeedDomain,
  // ModPagespeedMapRewriteDomain, ModPagespeedShardDomain, and as
  // the from-list in ModPagespeedMapOriginDomain.  However, the target
  // of ModPagespeedMapOriginDomain is not implicitly authoried --
  // that may be 'localhost'.
  bool authorized() const { return authorized_; }

  Domain* shard(int shard_index) const { return shards_[shard_index]; }
  bool is_proxy() const { return is_proxy_; }
  void set_is_proxy(bool is_proxy) { is_proxy_ = is_proxy; }

  GoogleString Signature() const {
    GoogleString signature;
    StrAppend(&signature, name_, "_",
              authorized_ ? "_a" : "_n", "_");
    // Assuming that there will be no cycle of Domains like Domain A has a
    // rewrite domain to domain B which in turn have the original domain as A.
    if (rewrite_domain_ != NULL) {
      StrAppend(&signature, "R:", rewrite_domain_->name(), "_");
    }
    if (!host_header_.empty()) {
      StrAppend(&signature, "H:", host_header_, "|");
    }
    if (origin_domain_ != NULL) {
      StrAppend(&signature,
                origin_domain_->is_proxy_ ? "P:" : "O:",
                origin_domain_->name(), "_");
    }
    for (int index = 0; index < num_shards(); ++index) {
      if (shards_[index] != NULL) {
        StrAppend(&signature, "S:", shards_[index]->name(), "_");
      }
    }
    return signature;
  }

  GoogleString ToString() const {
    GoogleString output = name_;

    if (authorized_) {
      StrAppend(&output, " Auth");
    }

    if (rewrite_domain_ != NULL) {
      StrAppend(&output,
                is_proxy_ ? " ProxyDomain:" : " RewriteDomain:",
                rewrite_domain_->name());
    }

    if (origin_domain_ != NULL) {
      StrAppend(&output,
                (origin_domain_->is_proxy_
                 ? " ProxyOriginDomain:" : " OriginDomain:"),
                origin_domain_->name());
    }

    if (!shards_.empty()) {
      StrAppend(&output, " Shards:{");
      for (int i = 0, n = shards_.size(); i < n; ++i) {
        StrAppend(&output, (i == 0 ? "" : ", "), shards_[i]->name());
      }
      StrAppend(&output, "}");
    }

    if (!host_header_.empty()) {
      StrAppend(&output, " HostHeader:", host_header_);
    }

    return output;
  }

 private:
  Wildcard wildcard_;
  GoogleString name_;

  // The rewrite_domain, if non-null, gives the location of where this
  // Domain should be rewritten.  This can be used to move resources onto
  // a CDN or onto a cookieless domain.  We also use this pointer to
  // get from shards back to the domain they were sharded from.
  Domain* rewrite_domain_;

  // The origin_domain, if non-null, gives the location of where
  // resources should be fetched from by mod_pagespeed, in lieu of how
  // it is specified in the HTML.  This allows, for example, a CDN to
  // fetch content from an origin domain, or an origin server behind a
  // load-balancer to specify localhost or an IP address of a host to
  // go to directly, skipping DNS resolution and reducing outbound
  // traffic.
  Domain* origin_domain_;

  // Explicitly specified Host header for use with MapOriginDomain.  When
  // empty, this indicates that the domain specified in the URL argument
  // to MapOrigin and MapOriginUrl should be used as the host header.
  GoogleString host_header_;

  // A rewrite_domain keeps track of all its shards.
  DomainVector shards_;

  bool authorized_;

  // This boolean helps us prevent spinning through a cycle in the
  // graph that can be expressed between shards and rewrite domains, e.g.
  //   ModPagespeedMapOriginDomain a b
  //   ModPagespeedMapRewriteDomain b c
  //   ModPagespeedAddShard b c
  bool cycle_breadcrumb_;

  // Identifies origin-domains that have been been used in
  // AddProxyDomainMapping, and thus should not require a modified
  // Host header when fetching resources.
  bool is_proxy_;
};

DomainLawyer::~DomainLawyer() {
  Clear();
}

bool DomainLawyer::AddDomain(const StringPiece& domain_name,
                             MessageHandler* handler) {
  return (AddDomainHelper(domain_name, true, true, false, handler) != NULL);
}

bool DomainLawyer::AddKnownDomain(const StringPiece& domain_name,
                                  MessageHandler* handler) {
  return (AddDomainHelper(domain_name, false, false, false, handler) != NULL);
}

GoogleString DomainLawyer::NormalizeDomainName(const StringPiece& domain_name) {
  // Ensure that the following specifications are treated identically:
  //     www.google.com/abc
  //     http://www.google.com/abc
  //     www.google.com/abc
  //     http://www.google.com/abc
  //     WWW.GOOGLE.COM/abc
  // all come out the same, but distinct from
  //     www.google.com/Abc
  // As the path component is case-sensitive.
  //
  // Example: domain-mapping domain-mapping
  // http://musicasacra.lemon42.com/DE/evoscripts/musica_sacra/returnBinaryImage
  // We need to case-fold only "musicasacra.lemon42.com" and not
  // "returnBinaryImage" or "DE".
  GoogleString domain_name_str;
  static const char kSchemeDelim[] = "://";
  stringpiece_ssize_type scheme_delim_start = domain_name.find(kSchemeDelim);
  if (scheme_delim_start == StringPiece::npos) {
    domain_name_str = StrCat("http://", domain_name);
    scheme_delim_start = 4;
  } else {
    domain_name.CopyToString(&domain_name_str);
  }
  EnsureEndsInSlash(&domain_name_str);

  // Lower-case all characters in the string, up until the "/" that terminates
  // the hostname.  We pass origin_start into the find() call to avoid tripping
  // on the "/" in "http://".
  GoogleString::size_type origin_start = scheme_delim_start +
      STATIC_STRLEN(kSchemeDelim);
  GoogleString::size_type slash = domain_name_str.find('/', origin_start);
  DCHECK_NE(GoogleString::npos, slash);
  for (char* p = &(domain_name_str[0]), *e = p + slash; p < e; ++p) {
    *p = LowerChar(*p);
  }

  // For "https", any ":443" in the host is redundant; ditto for :80 and http.
  StringPiece scheme(domain_name_str.data(), scheme_delim_start);
  StringPiece origin(domain_name_str.data() + origin_start,
                     slash - origin_start);
  if ((scheme == "https") && origin.ends_with(":443")) {
    domain_name_str.erase(slash - 4, 4);
  } else if ((scheme == "http") && origin.ends_with(":80")) {
    domain_name_str.erase(slash - 3, 3);
  }

  return domain_name_str;
}

DomainLawyer::Domain* DomainLawyer::AddDomainHelper(
    const StringPiece& domain_name, bool warn_on_duplicate,
    bool authorize, bool is_proxy, MessageHandler* handler) {
  if (domain_name.empty()) {
    // handler will be NULL only when called from Merge, which should
    // only have pre-validated (non-empty) domains.  So it should not
    // be possible to get here from Merge.
    if (handler != NULL) {
      handler->MessageS(kWarning, "Empty domain passed to AddDomain");
    }
    return NULL;
  }

  if (authorize && domain_name == "*") {
    authorize_all_domains_ = true;
  }

  // TODO(matterbury): need better data structures to eliminate the O(N) logic:
  // 1) Use a trie for domain_map_ as we need to find the domain whose trie
  //    path matches the beginning of the given domain_name since we no longer
  //    match just the domain name.
  // 2) Use a better lookup structure for wildcard searching.
  GoogleString domain_name_str = NormalizeDomainName(domain_name);
  Domain* domain = NULL;
  std::pair<DomainMap::iterator, bool> p = domain_map_.insert(
      DomainMap::value_type(domain_name_str, domain));
  DomainMap::iterator iter = p.first;
  if (p.second) {
    domain = new Domain(domain_name_str);
    iter->second = domain;
    if (domain->IsWildcarded()) {
      wildcarded_domains_.push_back(domain);
    }
  } else {
    domain = iter->second;
    if (warn_on_duplicate && (authorize == domain->authorized())) {
      handler->Message(kWarning, "AddDomain of domain already in map: %s",
                       domain_name_str.c_str());
      domain = NULL;
    }
  }
  if (domain != NULL) {
    if (authorize) {
      domain->set_authorized(true);
    }
    if (is_proxy) {
      domain->set_is_proxy(true);
    }
  }
  return domain;
}

// Looks up the Domain* object by name.  From the Domain object
// we can tell if it's wildcarded, in which case it cannot be
// the 'to' field for a map, and whether resources from it should
// be mapped to a different domain, either for rewriting or for
// fetching.
DomainLawyer::Domain* DomainLawyer::FindDomain(const GoogleUrl& gurl) const {
  // First do a quick lookup on the domain name only, since that's the most
  // common case. Failing that, try searching for domain + path.
  // TODO(matterbury): see AddDomainHelper for speed issues.
  Domain* domain = NULL;

  // There may be multiple entries in the map with the same domain,
  // but varying paths.  We want to choose the entry with the longest
  // domain that prefix-matches GURL.  So do the lookup starting
  // with the entire origin+path, then shorten the string removing
  // path components, looking for an exact match till we get to the origin
  // with no path.
  //
  // TODO(jmarantz): IMO the best data structure for this is an explicit
  // tree.  That would allow starting from the top and searching down,
  // rather than starting at the bottom and searching up, with each search
  // a lookup over the entire set of domains.
  GoogleString domain_path;
  gurl.AllExceptLeaf().CopyToString(&domain_path);
  StringPieceVector components;
  SplitStringPieceToVector(gurl.PathSansLeaf(), "/", &components, false);

  // PathSansLeaf gives something like "/a/b/c/" so after splitting with
  // omit_empty_strings==false, the first and last elements are always
  // present and empty.
  //
  // Note that the GURL can be 'about:blank' so be paranoid about getting
  // what we expect.
  if ((2U <= components.size()) &&
      components[0].empty() &&
      components[components.size() - 1].empty()) {
    int component_size = 0;
    for (int i = components.size() - 1; (domain == NULL) && (i >= 1); --i) {
      domain_path.resize(domain_path.size() - component_size);
      DCHECK(StringPiece(domain_path).ends_with("/"));
      DomainMap::const_iterator p = domain_map_.find(domain_path);
      if (p != domain_map_.end()) {
        domain = p->second;
      } else {
        // Remove the path component.  Consider input
        // "http://a.com/x/yy/zzz/w".  We will split PathSansLeaf, which
        // is "/x/yy/zzz/", so we will get StringPieceVector ["", "x",
        // "yy", "zzz", ""].  In the first iteration we want to consider
        // the entire path in the search, so we initialize
        // component_size to 0 above the loop.  In the next iteration we
        // want to chop off "zzz/" so we increment the component size by
        // one to get rid of the slash.  Note that we passed 'false'
        // into SplitStringPieceToVector so if there are double-slashes
        // they will show up as distinct components and we will get rid
        // of them one at a time.
        component_size = components[i - 1].size() + 1;
      }
    }
  }

  if (domain == NULL) {
    for (int i = 0, n = wildcarded_domains_.size(); i < n; ++i) {
      domain = wildcarded_domains_[i];
      if (domain->Match(domain_path)) {
        break;
      } else {
        domain = NULL;
      }
    }
  }
  return domain;
}

void DomainLawyer::FindDomainsRewrittenTo(
    const GoogleUrl& original_url,
    ConstStringStarVector* from_domains) const {
  // TODO(rahulbansal): Make this more efficient by maintaining the map of
  // rewrite_domain -> from_domains.
  if (!original_url.IsWebValid()) {
    LOG(ERROR) << "Invalid url " << original_url.Spec();
    return;
  }

  GoogleString domain_name;
  original_url.Origin().CopyToString(&domain_name);
  EnsureEndsInSlash(&domain_name);
  for (DomainMap::const_iterator p = domain_map_.begin();
      p != domain_map_.end(); ++p) {
    Domain* src_domain = p->second;
    if (!src_domain->IsWildcarded() && (src_domain->rewrite_domain() != NULL) &&
        domain_name == src_domain->rewrite_domain()->name()) {
      from_domains->push_back(&src_domain->name());
    }
  }
}

bool DomainLawyer::MapRequestToDomain(
    const GoogleUrl& original_request,
    const StringPiece& resource_url,  // relative to original_request
    GoogleString* mapped_domain_name,
    GoogleUrl* resolved_request,
    MessageHandler* handler) const {
  CHECK(original_request.IsAnyValid());
  GoogleUrl original_origin(original_request.Origin());
  resolved_request->Reset(original_request, resource_url);

  bool ret = false;
  // We can map a request to/from http/https.
  if (resolved_request->IsWebValid()) {
    GoogleUrl resolved_origin(resolved_request->Origin());

    // Looks at the resolved domain name + path from the original request
    // and the resource_url (which might override the original request).
    // Gets the Domain* object out of that.
    Domain* resolved_domain = FindDomain(*resolved_request);

    // The origin domain is authorized by default.
    if (resolved_origin == original_origin) {
      resolved_origin.Spec().CopyToString(mapped_domain_name);
      ret = true;
    } else if (resolved_domain != NULL && resolved_domain->authorized()) {
      if (resolved_domain->IsWildcarded()) {
        // This is a sharded domain. We do not do the sharding in this function.
        resolved_origin.Spec().CopyToString(mapped_domain_name);
      } else {
        *mapped_domain_name = resolved_domain->name();
      }
      ret = true;
    }

    // If we actually got a Domain* out of the lookups so far, then a
    // mapping to a different rewrite_domain may be contained there.  This
    // helps move resources to CDNs or cookieless domains.
    //
    // Note that at this point, we are not really caring where we fetch
    // from.  We are only concerned here with what URLs we will write into
    // HTML files.  See MapOrigin below which is used to redirect fetch
    // requests to a different domain (e.g. localhost).
    if (ret && resolved_domain != NULL) {
      Domain* mapped_domain = resolved_domain->rewrite_domain();
      if (mapped_domain != NULL) {
        CHECK(!mapped_domain->IsWildcarded());
        CHECK(mapped_domain != resolved_domain);
        *mapped_domain_name = mapped_domain->name();
        GoogleUrl mapped_request;
        ret = MapUrlHelper(*resolved_domain, *mapped_domain,
                           *resolved_request, &mapped_request);
        if (ret) {
          resolved_request->Swap(&mapped_request);
        }
      }
    }
  }
  return ret;
}

bool DomainLawyer::IsDomainAuthorized(const GoogleUrl& original_request,
                                      const GoogleUrl& domain_to_check) const {
  if (authorize_all_domains_) {
    return true;
  }
  bool ret = false;
  if (domain_to_check.IsWebValid()) {
    if (original_request.IsWebValid() &&
        (original_request.Origin() == domain_to_check.Origin())) {
      ret = true;
    } else {
      Domain* path_domain = FindDomain(domain_to_check);
      ret = (path_domain != NULL) && path_domain->authorized();
    }
  }
  return ret;
}

bool DomainLawyer::IsOriginKnown(const GoogleUrl& domain_to_check) const {
  if (domain_to_check.IsWebValid()) {
    Domain* path_domain = FindDomain(domain_to_check);
    return (path_domain != NULL);
  }
  return false;
}

bool DomainLawyer::MapOrigin(const StringPiece& in, GoogleString* out,
                             GoogleString* host_header, bool* is_proxy) const {
  GoogleUrl gurl(in);
  return gurl.IsWebValid() && MapOriginUrl(gurl, out, host_header, is_proxy);
}

bool DomainLawyer::MapOriginUrl(const GoogleUrl& gurl,
                                GoogleString* out, GoogleString* host_header,
                                bool* is_proxy) const {
  bool ret = false;
  *is_proxy = false;
  host_header->clear();

  // We can map an origin to/from http/https.
  if (gurl.IsWebValid()) {
    ret = true;
    gurl.Spec().CopyToString(out);
    Domain* domain = FindDomain(gurl);
    if (domain != NULL) {
      Domain* origin_domain = domain->origin_domain();
      if (origin_domain != NULL) {
        GoogleUrl mapped_gurl;
        if (MapUrlHelper(*domain, *origin_domain, gurl, &mapped_gurl)) {
          mapped_gurl.Spec().CopyToString(out);
        }
        *is_proxy = origin_domain->is_proxy();
        const GoogleString& origin_header = origin_domain->host_header();
        if (!origin_header.empty()) {
          *host_header = origin_header;
        }
      }
    }

    if (host_header->empty()) {
      gurl.HostAndPort().CopyToString(host_header);
    }
  }

  return ret;
}

bool DomainLawyer::MapUrlHelper(const Domain& from_domain,
                                const Domain& to_domain,
                                const GoogleUrl& gurl,
                                GoogleUrl* mapped_gurl) const {
  CHECK(!to_domain.IsWildcarded());

  GoogleUrl from_domain_gurl(from_domain.name());
  StringPiece from_domain_path(from_domain_gurl.PathSansLeaf());
  StringPiece path_and_leaf(gurl.PathAndLeaf());
  DCHECK(path_and_leaf.starts_with(from_domain_path));

  // Trim the URL's domain we came from based on how it was specifed in the
  // from_domain.  E.g. if you write
  //    ModPagespeedMap*Domain localhost/foo cdn.com/bar
  // and the URL being mapped is
  //    http://cdn.com/bar/x
  // then we set path_and_leaf to "x".  This testcase gets hit in
  // DomainLawyerTest.OriginAndExternWithPaths.
  //
  // Even if the from_domain has no subdirectory, we need to remove
  // the leading slash to make it a relative reference and retain any
  // subdirectory in the to_domain.
  //
  // Note: We must prepend "./" to make sure the path_and_leaf is not an
  // absolute URL, which will cause problems below. For example:
  // "http://www.example.com/data:image/jpeg" should be converted to the
  // relative URL "./data:image/jpeg", not the absolute URL "data:image/jpeg".
  GoogleString rel_url =
      StrCat("./", path_and_leaf.substr(from_domain_path.size()));
  // Make sure this isn't a valid absolute URL.
  DCHECK(!GoogleUrl(rel_url).IsWebValid())
      << "URL " << gurl.Spec() << " is being mapped to absolute URL "
      << rel_url << " which will break many things.";
  GoogleUrl to_domain_gurl(to_domain.name());
  mapped_gurl->Reset(to_domain_gurl, rel_url);
  return mapped_gurl->IsWebValid();
}

bool DomainLawyer::AddRewriteDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    MessageHandler* handler) {
  bool result = MapDomainHelper(to_domain_name, comma_separated_from_domains,
                                "" /* host_header */,
                                &Domain::SetRewriteDomain,
                                true /* allow_wildcards */,
                                true /* allow_map_to_https */,
                                true /* authorize */,
                                handler);
  can_rewrite_domains_ |= result;
  return result;
}

bool DomainLawyer::DomainNameToTwoProtocols(
    const StringPiece& domain_name,
    GoogleString* http_url, GoogleString* https_url) {
  *http_url = NormalizeDomainName(domain_name);
  StringPiece http_url_piece(*http_url);
  if (!http_url_piece.starts_with("http:")) {
    return false;
  }
  *https_url = StrCat("https", http_url_piece.substr(4));
  return true;
}

bool DomainLawyer::TwoProtocolDomainHelper(
      const StringPiece& to_domain_name,
      const StringPiece& from_domain_name,
      const StringPiece& host_header,
      SetDomainFn set_domain_fn,
      bool authorize,
      MessageHandler* handler) {
  GoogleString http_to_url, http_from_url, https_to_url, https_from_url;
  if (!DomainNameToTwoProtocols(to_domain_name, &http_to_url, &https_to_url)) {
    return false;
  }
  if (!DomainNameToTwoProtocols(from_domain_name,
                                &http_from_url, &https_from_url)) {
    return false;
  }
  if (!MapDomainHelper(http_to_url, http_from_url,
                       host_header,
                       set_domain_fn,
                       false, /* allow_wildcards */
                       false, /* allow_map_to_https */
                       authorize, handler)) {
    return false;
  }
  if (!MapDomainHelper(https_to_url, https_from_url,
                       host_header,
                       set_domain_fn,
                       false, /* allow_wildcards */
                       true, /* allow_map_to_https */
                       authorize, handler)) {
    // Note that we still retain the http domain mapping in this case.
    return false;
  }
  return true;
}

bool DomainLawyer::AddTwoProtocolRewriteDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& from_domain_name,
    MessageHandler* handler) {
  bool result = TwoProtocolDomainHelper(to_domain_name, from_domain_name,
                                        "" /* host_header */,
                                        &Domain::SetRewriteDomain,
                                        true /*authorize */, handler);
  can_rewrite_domains_ |= result;
  return result;
}

bool DomainLawyer::AddOriginDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    const StringPiece& host_header,
    MessageHandler* handler) {
  return MapDomainHelper(to_domain_name, comma_separated_from_domains,
                         host_header,
                         &Domain::SetOriginDomain,
                         true /* allow_wildcards */,
                         true /* allow_map_to_https */,
                         false /* authorize */,
                         handler);
}

bool DomainLawyer::AddProxyDomainMapping(
    const StringPiece& proxy_domain_name,
    const StringPiece& origin_domain_name,
    const StringPiece& to_domain_name,
    MessageHandler* handler) {
  bool result;

  if (to_domain_name.empty()) {
    // 1. Rewrite from origin_domain to proxy_domain.
    // 2. Set origin_domain->is_proxy = true.
    // 3. Map origin from proxy_domain to origin_domain.
    result = MapDomainHelper(origin_domain_name, proxy_domain_name,
                             "" /* host_header */,
                             &Domain::SetProxyDomain,
                             false /* allow_wildcards */,
                             true /* allow_map_to_https */,
                             true /* authorize */,
                             handler);
  } else {
    // 1. Rewrite from origin_domain to to_domain.
    // 2. Set origin_domain->is_proxy = true.
    // 3. Map origin from to_domain to origin_domain.
    result = MapDomainHelper(origin_domain_name, to_domain_name,
                             "" /* host_header */,
                             &Domain::SetProxyDomain,
                             false /* allow_wildcards */,
                             true /* allow_map_to_https */,
                             true /* authorize */,
                             handler);
    // 4. Rewrite from proxy_domain to to_domain. This way when the CDN asks us
    // for resources on proxy_domain it knows to use the CDN domain for the
    // cache key.
    result &= MapDomainHelper(to_domain_name, proxy_domain_name,
                              "" /* host_header */,
                              &Domain::SetRewriteDomain,
                              false /* allow_wildcards */,
                              true /* allow_map_to_https */,
                              true /* authorize */,
                              handler);
    // 5. Map origin from proxy_domain to origin_domain. This tells the proxy
    // how to fetch files from the origin for reconstruction.
    result &= MapDomainHelper(origin_domain_name, proxy_domain_name,
                              "" /* host_header */,
                              &Domain::SetOriginDomain,
                              false /* allow wildcards */,
                              true /* allow_map_to_https */,
                              true /* authorize */,
                              handler);
  }
  return result;
}


bool DomainLawyer::AddTwoProtocolOriginDomainMapping(
    const StringPiece& to_domain_name,
    const StringPiece& from_domain_name,
    const StringPiece& host_header,
    MessageHandler* handler) {
  return TwoProtocolDomainHelper(to_domain_name, from_domain_name,
                                 host_header,
                                 &Domain::SetOriginDomain,
                                 false /*authorize */, handler);
}

bool DomainLawyer::AddShard(
    const StringPiece& shard_domain_name,
    const StringPiece& comma_separated_shards,
    MessageHandler* handler) {
  bool result = MapDomainHelper(shard_domain_name, comma_separated_shards,
                                "" /* host_header */,
                                &Domain::SetShardFrom,
                                false /* allow_wildcards */,
                                true /* allow_map_to_https */,
                                true /* authorize */,
                                handler);
  can_rewrite_domains_ |= result;
  return result;
}

bool DomainLawyer::IsSchemeSafeToMapTo(const StringPiece& domain_name,
                                       bool allow_https_scheme) {
  // The scheme defaults to http so that's the same as explicitly saying http.
  return (domain_name.find("://") == GoogleString::npos ||
          domain_name.starts_with("http://") ||
          (allow_https_scheme && domain_name.starts_with("https://")));
}

bool DomainLawyer::MapDomainHelper(
    const StringPiece& to_domain_name,
    const StringPiece& comma_separated_from_domains,
    const StringPiece& host_header,
    SetDomainFn set_domain_fn,
    bool allow_wildcards,
    bool allow_map_to_https,
    bool authorize_to_domain,
    MessageHandler* handler) {
  if (!IsSchemeSafeToMapTo(to_domain_name, allow_map_to_https)) {
    return false;
  }
  Domain* to_domain = AddDomainHelper(to_domain_name, false,
                                      authorize_to_domain, false, handler);
  if (to_domain == NULL) {
    return false;
  }

  bool ret = false;
  bool mapped_a_domain = false;
  if (to_domain->IsWildcarded()) {
    handler->Message(kError, "Cannot map to a wildcarded domain: %s",
                     to_domain_name.as_string().c_str());
  } else {
    GoogleUrl to_url(to_domain->name());
    StringPieceVector domains;
    SplitStringPieceToVector(comma_separated_from_domains, ",", &domains, true);
    ret = true;
    for (int i = 0, n = domains.size(); i < n; ++i) {
      const StringPiece& domain_name = domains[i];
      Domain* from_domain = AddDomainHelper(domain_name, false, true, false,
                                            handler);
      if (from_domain != NULL) {
        GoogleUrl from_url(from_domain->name());
        if (to_url.Origin() == from_url.Origin()) {
          // Ignore requests to map to the same scheme://hostname:port/.
        } else if (!allow_wildcards && from_domain->IsWildcarded()) {
          handler->Message(kError, "Cannot map from a wildcarded domain: %s",
                           to_domain_name.as_string().c_str());
          ret = false;
        } else {
          bool ok = (from_domain->*set_domain_fn)(to_domain, handler);
          ret &= ok;
          mapped_a_domain |= ok;
        }
      }
    }
    DCHECK(host_header.empty() || !to_domain->is_proxy())
        << "It makes no sense to specify a host header for a proxy:"
        << host_header << ", " << to_domain_name;
    to_domain->set_host_header(host_header);
  }
  return (ret && mapped_a_domain);
}

DomainLawyer::Domain* DomainLawyer::CloneAndAdd(const Domain* src) {
  Domain* dst = AddDomainHelper(src->name(), false, src->authorized(),
                                src->is_proxy(), NULL);
  dst->set_host_header(src->host_header());
  return dst;
}

void DomainLawyer::Merge(const DomainLawyer& src) {
  int num_existing_wildcards = num_wildcarded_domains();
  for (DomainMap::const_iterator
           p = src.domain_map_.begin(),
           e = src.domain_map_.end();
       p != e; ++p) {
    Domain* src_domain = p->second;
    Domain* dst_domain = CloneAndAdd(src_domain);
    Domain* src_rewrite_domain = src_domain->rewrite_domain();
    if (src_rewrite_domain != NULL) {
      dst_domain->SetRewriteDomain(CloneAndAdd(src_rewrite_domain), NULL);
    }
    Domain* src_origin_domain = src_domain->origin_domain();
    if (src_origin_domain != NULL) {
      dst_domain->SetOriginDomain(CloneAndAdd(src_origin_domain), NULL);
    }
    for (int i = 0; i < src_domain->num_shards(); ++i) {
      Domain* src_shard = src_domain->shard(i);
      Domain* dst_shard = CloneAndAdd(src_shard);
      dst_shard->SetShardFrom(dst_domain, NULL);
    }
  }

  // Remove the wildcards we just added in map order, and instead add them
  // in the order they were in src.wildcarded_domains.
  wildcarded_domains_.resize(num_existing_wildcards);
  std::set<Domain*> dup_detector(wildcarded_domains_.begin(),
                                 wildcarded_domains_.end());
  for (int i = 0, n = src.wildcarded_domains_.size(); i < n; ++i) {
    Domain* src_domain = src.wildcarded_domains_[i];
    DomainMap::const_iterator p = domain_map_.find(src_domain->name());
    if (p == domain_map_.end()) {
      LOG(DFATAL) << "Domain " << src_domain->name() << " not found in dst";
    } else {
      Domain* dst_domain = p->second;
      if (dup_detector.find(dst_domain) == dup_detector.end()) {
        wildcarded_domains_.push_back(dst_domain);
      }
    }
  }

  can_rewrite_domains_ |= src.can_rewrite_domains_;
  authorize_all_domains_ |= src.authorize_all_domains_;
  if (!src.proxy_suffix_.empty()) {
    if (!proxy_suffix_.empty() && (proxy_suffix_ != src.proxy_suffix_)) {
      LOG(WARNING)
          << "Merging incompatible proxy suffixes " << proxy_suffix_ << " and "
          << src.proxy_suffix_;
    }
    proxy_suffix_ = src.proxy_suffix_;
  }
}

bool DomainLawyer::ShardDomain(const StringPiece& domain_name,
                               uint32 hash,
                               GoogleString* sharded_domain) const {
  GoogleUrl domain_gurl(NormalizeDomainName(domain_name));
  Domain* domain = FindDomain(domain_gurl);
  bool sharded = false;
  if (domain != NULL) {
    if (domain->num_shards() != 0) {
      int shard_index = hash % domain->num_shards();
      domain = domain->shard(shard_index);
      *sharded_domain = domain->name();
      sharded = true;
    }
  }
  return sharded;
}

bool DomainLawyer::WillDomainChange(const GoogleUrl& gurl) const {
  Domain* domain = FindDomain(gurl), *mapped_domain = domain;
  if (domain != NULL) {
    // First check a mapping based on AddRewriteDomainMapping.
    mapped_domain = domain->rewrite_domain();
    if (mapped_domain == NULL)  {
      // Even if there was no AddRewriteDomainMapping for this domain, there
      // may still have been shards.
      mapped_domain = domain;
    }

    // Now check mappings from the shard.
    if (mapped_domain->num_shards() != 0) {
      if (mapped_domain->num_shards() == 1) {
        // Usually we don't expect exactly one shard, but if there is,
        // we know exactly what it will be.
        mapped_domain = mapped_domain->shard(0);
      } else {
        // We don't have enough data in this function to determine what
        // the shard index will be, so we assume pessimistically that
        // the domain will change.
        //
        // TODO(jmarantz): rename this method to MayDomainChange, or
        // pass in the sharding index.
        mapped_domain = NULL;
      }
    }
  }
  return domain != mapped_domain;
}

bool DomainLawyer::IsProxyMapped(const GoogleUrl& gurl) const {
  Domain* domain = FindDomain(gurl);
  if (domain != NULL) {
    Domain* origin = domain->origin_domain();
    if ((origin != NULL) && origin->is_proxy()) {
      return true;
    }
  }
  return false;
}

bool DomainLawyer::DoDomainsServeSameContent(
    const StringPiece& domain1_name, const StringPiece& domain2_name) const {
  GoogleUrl domain1_gurl(NormalizeDomainName(domain1_name));
  Domain* domain1 = FindDomain(domain1_gurl);
  GoogleUrl domain2_gurl(NormalizeDomainName(domain2_name));
  Domain* domain2 = FindDomain(domain2_gurl);
  if ((domain1 == NULL) || (domain2 == NULL)) {
    return false;
  }
  if (domain1 == domain2) {
    return true;
  }
  Domain* rewrite1 = domain1->rewrite_domain();
  Domain* rewrite2 = domain2->rewrite_domain();
  if ((rewrite1 == domain2) || (rewrite2 == domain1)) {
    return true;
  }
  if ((rewrite1 != NULL) && (rewrite1 == rewrite2)) {
    return true;
  }
  return false;
}

GoogleString DomainLawyer::Signature() const {
  GoogleString signature;

  for (DomainMap::const_iterator iterator = domain_map_.begin();
      iterator != domain_map_.end(); ++iterator) {
    StrAppend(&signature, "D:", iterator->second->Signature(), "-");
  }
  if (!proxy_suffix_.empty()) {
    StrAppend(&signature, ",PS:", proxy_suffix_);
  }

  return signature;
}

GoogleString DomainLawyer::ToString(StringPiece line_prefix) const {
  GoogleString output;
  for (DomainMap::const_iterator iterator = domain_map_.begin();
      iterator != domain_map_.end(); ++iterator) {
    StrAppend(&output, line_prefix, iterator->second->ToString(), "\n");
  }
  if (!proxy_suffix_.empty()) {
    StrAppend(&output, "Proxy Suffix: ", proxy_suffix_);
  }
  return output;
}

void DomainLawyer::Clear() {
  STLDeleteValues(&domain_map_);
  can_rewrite_domains_ = false;
  authorize_all_domains_ = false;
  wildcarded_domains_.clear();
  proxy_suffix_.clear();
}

bool DomainLawyer::StripProxySuffix(const GoogleUrl& gurl, GoogleString* url,
                                    GoogleString* host) const {
  bool ret = false;
  if (gurl.IsWebValid() && !proxy_suffix_.empty()) {
    StringPiece host_and_port = gurl.HostAndPort();
    if (host_and_port.ends_with(proxy_suffix_)) {
      host_and_port.remove_suffix(proxy_suffix_.size());
      host_and_port.CopyToString(host);  // Remove any other port, I suppose.
      *url = StrCat(gurl.Scheme(), "://", host_and_port, gurl.PathAndLeaf());
      ret = true;
    }
  }
  return ret;
}

bool DomainLawyer::AddProxySuffix(const GoogleUrl& base_url,
                                  GoogleString* href) const {
  // Let's say we have a proxy-prefix of ".suffix".  When we visit
  // http://www.example.com.suffix, we can leave relative URLs alone
  // in hyperlinkes.  However, if we see an absolute link to
  // http://www.example.com/foo or http://foo.www.example.com/bar then
  // we want to add the suffix to the hyperlink attribute.
  StringPiece base_host = base_url.Host();
  if (!proxy_suffix_.empty() && base_host.ends_with(proxy_suffix_)) {
    // Remove the suffix from the host so we can find a-tag references to it.
    StringPiece base_host_no_suffix = base_host.substr(
        0, base_host.size() - proxy_suffix_.size());
    GoogleUrl href_gurl(base_url, *href);

    // Note that we purposefully do not check schemes here since we want to
    // permit redirects from http:// to https:// (and likewise inclusion of
    // resources).
    if (href_gurl.IsWebValid() && base_url.IsWebValid()) {
      StringPiece href_domain, base_domain;
      StringPiece href_host = href_gurl.Host();
      if (href_host == base_host_no_suffix) {
        // TODO(jmarantz): handle alternate ports.
        *href = StrCat(href_gurl.Scheme(), "://", base_host,
                       href_gurl.PathAndLeaf());
        return true;
      } else if (domain_registry::MinimalPrivateSuffix(href_host) ==
                 domain_registry::MinimalPrivateSuffix(base_host_no_suffix)) {
        *href = StrCat(href_gurl.Scheme(), "://",
                       href_host, proxy_suffix_,
                       href_gurl.PathAndLeaf());
        return true;
      }
    }
  }
  return false;
}

}  // namespace net_instaweb
