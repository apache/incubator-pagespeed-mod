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

//         jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/rewriter/public/output_resource.h"

#include <cstddef>

#include "base/logging.h"
#include "net/instaweb/http/public/http_value.h"
#include "net/instaweb/rewriter/cached_result.pb.h"
#include "net/instaweb/rewriter/input_info.pb.h"
#include "net/instaweb/rewriter/public/domain_lawyer.h"
#include "net/instaweb/rewriter/public/output_resource_kind.h"
#include "net/instaweb/rewriter/public/resource.h"
#include "net/instaweb/rewriter/public/resource_namer.h"
#include "net/instaweb/rewriter/public/rewrite_driver.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "net/instaweb/rewriter/public/server_context.h"
#include "net/instaweb/rewriter/public/url_namer.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/proto_util.h"
#include "pagespeed/kernel/base/sha1_signature.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/cache/cache_interface.h"
#include "pagespeed/kernel/http/content_type.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/url_to_filename_encoder.h"

namespace net_instaweb {

namespace {

// Helper to allow us to use synchronous caches synchronously even with
// asynchronous interface, until we're changed to be fully asynchronous.
class SyncCallback : public CacheInterface::Callback {
 public:
  SyncCallback() : called_(false), state_(CacheInterface::kNotFound) {
  }

  virtual void Done(CacheInterface::KeyState state) {
    called_ = true;
    state_ = state;
  }

  bool called_;
  CacheInterface::KeyState state_;
};

}  // namespace

OutputResource::OutputResource(const RewriteDriver* driver,
                               StringPiece resolved_base,
                               StringPiece unmapped_base,
                               StringPiece original_base,
                               const ResourceNamer& full_name,
                               OutputResourceKind kind)
    : Resource(driver, NULL /* no type yet*/),
      writing_complete_(false),
      cached_result_owned_(false),
      cached_result_(NULL),
      resolved_base_(resolved_base.data(), resolved_base.size()),
      unmapped_base_(unmapped_base.data(), unmapped_base.size()),
      original_base_(original_base.data(), original_base.size()),
      rewrite_options_(driver->options()),
      kind_(kind) {
  DCHECK(rewrite_options_ != NULL);
  full_name_.CopyFrom(full_name);
  CHECK(EndsInSlash(resolved_base)) <<
      "resolved_base must end in a slash, was: " << resolved_base;
  set_enable_cache_purge(rewrite_options_->enable_cache_purge());
  set_respect_vary(
      ResponseHeaders::GetVaryOption(rewrite_options_->respect_vary()));
  set_proactive_resource_freshening(
      rewrite_options_->proactive_resource_freshening());
}

OutputResource::~OutputResource() {
  clear_cached_result();
}

void OutputResource::DumpToDisk(MessageHandler* handler) {
  GoogleString file_name = DumpFileName();
  FileSystem* file_system = server_context_->file_system();
  FileSystem::OutputFile* output_file =
      file_system->OpenOutputFile(file_name.c_str(), handler);
  if (output_file == NULL) {
    handler->Message(kWarning, "Unable to open dump file: %s",
                     file_name.c_str());
    return;
  }

  // Serialize headers.
  GoogleString headers;
  StringWriter string_writer(&headers);
  response_headers_.WriteAsHttp(&string_writer, handler);
  bool ok_headers = output_file->Write(headers, handler);

  // Serialize payload.
  bool ok_body = output_file->Write(ExtractUncompressedContents(), handler);

  if (!ok_headers || !ok_body) {
    handler->Message(kWarning,
                     "Error writing dump file: %s", file_name.c_str());
  }

  file_system->Close(output_file, handler);
}

Writer* OutputResource::BeginWrite(MessageHandler* handler) {
  value_.Clear();
  full_name_.ClearHash();
  computed_url_.clear();  // Since dependent on full_name_.
  CHECK(!writing_complete_);
  return &value_;
}

void OutputResource::EndWrite(MessageHandler* handler) {
  CHECK(!writing_complete_);
  value_.SetHeaders(&response_headers_);
  Hasher* hasher = server_context_->hasher();
  full_name_.set_hash(hasher->Hash(ExtractUncompressedContents()));
  full_name_.set_signature(ComputeSignature());
  computed_url_.clear();  // Since dependent on full_name_.
  writing_complete_ = true;
}

StringPiece OutputResource::suffix() const {
  CHECK(type_ != NULL);
  return type_->file_extension();
}

GoogleString OutputResource::DumpFileName() const {
  GoogleString filename;
  UrlToFilenameEncoder::EncodeSegment(
      server_context_->filename_prefix(), url(), '/', &filename);
  return filename;
}

GoogleString OutputResource::name_key() const {
  GoogleString id_name = full_name_.EncodeIdName();
  GoogleString result;
  CHECK(!resolved_base_.empty());  // Corresponding path in url() is dead code
  result = StrCat(resolved_base_, id_name);
  return result;
}

// TODO(jmarantz): change the name to reflect the fact that it is not
// just an accessor now.
GoogleString OutputResource::url() const {
  // Computing our URL is relatively expensive and it can be set externally,
  // so we compute it the first time we're called and cache the result;
  // computed_url_ is declared mutable.
  if (computed_url_.empty()) {
    computed_url_ = server_context()->url_namer()->Encode(
        rewrite_options_, *this, UrlNamer::kSharded);
  }
  return computed_url_;
}

GoogleString OutputResource::HttpCacheKey() const {
  GoogleString canonical_url =
      server_context()->url_namer()->Encode(rewrite_options_, *this,
                                            UrlNamer::kUnsharded);
  GoogleString mapped_domain_name;
  GoogleUrl resolved_request;
  const DomainLawyer* lawyer = rewrite_options()->domain_lawyer();

  // MapRequestToDomain needs a base URL, which ought to be irrelevant here,
  // as we're already absolute.
  GoogleUrl base(canonical_url);
  if (base.IsWebValid() &&
      lawyer->MapRequestToDomain(
          base, canonical_url, &mapped_domain_name, &resolved_request,
          server_context()->message_handler())) {
    resolved_request.Spec().CopyToString(&canonical_url);
  }
  return canonical_url;
}

GoogleString OutputResource::UrlEvenIfHashNotSet() {
  GoogleString result;
  if (!has_hash()) {
    full_name_.set_hash("0");
    result = server_context()->url_namer()->Encode(
        rewrite_options_, *this, UrlNamer::kSharded);
    full_name_.ClearHash();
  } else {
    result = url();
  }
  return result;
}

void OutputResource::SetHash(StringPiece hash) {
  CHECK(!writing_complete_);
  CHECK(!has_hash());
  full_name_.set_hash(hash);
  computed_url_.clear();  // Since dependent on full_name_.
}

void OutputResource::LoadAndCallback(NotCacheablePolicy not_cacheable_policy,
                                     const RequestContextPtr& request_context,
                                     AsyncCallback* callback) {
  // TODO(oschaaf): Output resources shouldn't be loaded via LoadAsync, but
  // rather through FetchResource. Yet 
  // ProxyInterfaceTest.TestNoDebugAbortAfterMoreThenOneYear does manage to hit
  // this code. See https://github.com/apache/incubator-pagespeed-mod/issues/1553
  callback->Done(false /* lock_failure */, writing_complete_);
}

GoogleString OutputResource::decoded_base() const {
  GoogleUrl gurl(url());
  GoogleString decoded_url;
  if (server_context()->url_namer()->Decode(gurl, rewrite_options(),
                                            &decoded_url)) {
    gurl.Reset(decoded_url);
  }
  return gurl.AllExceptLeaf().as_string();
}

void OutputResource::SetType(const ContentType* content_type) {
  Resource::SetType(content_type);
  if (content_type != NULL) {
    // TODO(jmaessen): The addition of 1 below avoids the leading ".";
    // make this convention consistent and fix all code.
    full_name_.set_ext(content_type->file_extension() + 1);
    computed_url_.clear();  // Since dependent on full_name_.
    DCHECK_LE(full_name_.ext().size(),
              static_cast<size_t>(ContentType::MaxProducedExtensionLength()))
        << "OutputResource with extension length > "
           "ContentType::MaxProducedExtensionLength()";
  }
}

CachedResult* OutputResource::EnsureCachedResultCreated() {
  if (cached_result_ == NULL) {
    clear_cached_result();
    cached_result_ = new CachedResult();
    cached_result_owned_ = true;
  } else {
    DCHECK(!cached_result_->frozen()) << "Cannot mutate frozen cached result";
  }
  return cached_result_;
}

void OutputResource::UpdateCachedResultPreservingInputInfo(
    CachedResult* to_update) const {
  // TODO(sligocki): Fix this so that the *cached_result() does have inputs set.
  protobuf::RepeatedPtrField<InputInfo> temp;
  temp.Swap(to_update->mutable_input());
  *to_update = *cached_result();
  temp.Swap(to_update->mutable_input());
}

void OutputResource::clear_cached_result() {
  if (cached_result_owned_) {
    delete cached_result_;
    cached_result_owned_ = false;
  }
  cached_result_ = NULL;
}

GoogleString OutputResource::ComputeSignature() {
  GoogleString signing_key = rewrite_options_->url_signing_key();
  GoogleString computed_signature;
  if (!signing_key.empty()) {
    GoogleString data = HttpCacheKey();
    int data_length =
        data.size() -
        (full_name_.ext().size() + full_name_.hash().size() +
         full_name_.signature().size() + 2);  // For the two separating dots.
    const SHA1Signature* signature = rewrite_options_->sha1signature();
    computed_signature =
        signature->Sign(signing_key, data.substr(0, data_length));
  }
  return computed_signature;
}

bool OutputResource::CheckSignature() {
  // If signing isn't enforced, then consider all URLs to be valid and just
  // ignore the passed signature if there is one.
  if (rewrite_options_->url_signing_key().empty()) {
    return true;
  }
  GoogleString computed_signature = ComputeSignature();
  StringPiece provided_signature = full_name_.signature();
  // The following code is equivalent to "computed_signature ==
  // provided_signature" but will not short-circuit. This protects us from
  // timing attacks where someone may be able to figure out the correct
  // signature by measuring that ones with the correct first N characters take
  // slightly longer to check. See
  // http://codahale.com/a-lesson-in-timing-attacks/
  bool valid =
      CountCharacterMismatches(computed_signature, provided_signature) == 0;
  if (!valid) {
    MessageHandler* handler = server_context_->message_handler();
    GoogleString message =
        StrCat("Invalid resource signature for ", UrlEvenIfHashNotSet(),
               " provided. Expected ", computed_signature, " Received ",
               provided_signature);
    handler->Message(
        kInfo,
        "Invalid resource signature for %s provided. Expected %s Received %s",
        UrlEvenIfHashNotSet().c_str(), computed_signature.c_str(),
        provided_signature.data());
  }
  // If signing isn't enforced, return true always, but do this after checking
  // if the signature was correct for logging purposes.
  return valid || rewrite_options_->accept_invalid_signatures();
}

}  // namespace net_instaweb
