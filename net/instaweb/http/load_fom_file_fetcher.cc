/*
 * Copyright 2017 Google Inc.
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

// Author: oschaaf@we-amp.com (Otto van der Schaaf)

#include "net/instaweb/http/public/load_from_file_fetcher.h"

#include <cstdio>
#include <set>
#include <utility>                     // for pair

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_response_parser.h"
#include "net/instaweb/rewriter/public/rewrite_options.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/string_writer.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"

namespace net_instaweb {

namespace {

static const char kErrorHtml[] =
    "<html><head><title>LoadFromFileFetcher Error</title></head>"
    "<body><h1>LoadFromFileFetcher Error</h1></body></html>";

}  // namespace

LoadFromFileFetcher::LoadFromFileFetcher(const RewriteOptions* options,
                                       FileSystem* file_system,
                                       Timer* timer,
                                       UrlAsyncFetcher* base_fetcher)
    : rewrite_options_(options),
      file_system_(file_system),
      timer_(timer),
      error_body_(kErrorHtml),
      base_fetcher_(base_fetcher) {
  // TODO(oschaaf): this actually fires, yikes.  
  //CHECK(base_fetcher_ != NULL);
}

LoadFromFileFetcher::~LoadFromFileFetcher() {
}

void LoadFromFileFetcher::RespondError(ResponseHeaders* response_headers,
                                      Writer* response_writer,
                                      MessageHandler* handler) {
  response_headers->SetStatusAndReason(HttpStatus::kNotFound);
  response_headers->Add(HttpAttributes::kContentType, "text/html");
  response_headers->ComputeCaching();
  response_writer->Write(error_body_, handler);
}

void LoadFromFileFetcher::Fetch(
    const GoogleString& url, MessageHandler* handler, AsyncFetch* fetch) {
  bool ret = false;
  GoogleString filename;
  GoogleUrl gurl(url);
  //const RequestHeaders& request_headers = *fetch->request_headers();
  ResponseHeaders* response_headers = fetch->response_headers();

  if (gurl.IsWebValid()) {
    bool file_mapped = rewrite_options_->file_load_policy()->ShouldLoadFromFile(gurl, &filename);
    if (file_mapped) {
      // TODO(oschaaf): ignore empty file name, DCHECK maybe
      // TODO(oschaaf): max file size.
      int64 max_file_size_ = 1024*1024*4;
      GoogleString output_buffer;

      // TODO(oschaaf):
      // Pass in NullMessageHandler so that we don't get errors for file not found
      ret = file_system_->ReadFile(
			    filename.c_str(), max_file_size_, &output_buffer, handler);

      if (ret) {
        handler->Message(kInfo, "LoadFromFileFetcher: Fetched %s as %s",
                          url.c_str(), filename.c_str());
        response_headers->set_major_version(1);
        response_headers->set_minor_version(1);
        response_headers->SetStatusAndReason(HttpStatus::kOK);
        int64 now_ms = timer_->NowMs();
        const ContentType* content_type = NameExtensionToContentType(filename);
        response_headers->Add(HttpAttributes::kContentType, content_type->mime_type());
        response_headers->Add("X-PageSpeed-Origin", "LoadFromFile");
        response_headers->FixDateHeaders(now_ms);
        response_headers->SetContentLength(output_buffer.size());
        response_headers->ComputeCaching();
        fetch->Write(output_buffer, handler);

        std::cerr << "@@ loaded fine: " << url << "\n";
        std::cerr << response_headers->ToString() << "\n==========\n";
        std::cerr << output_buffer << "\n";
      } else {
        handler->Message(kInfo,
                        "LoadFromFileFetcher: Failed to find file %s for %s",
                        filename.c_str(), url.c_str());
        std::cerr << "@@ failed to find file for " << url << "\n";
      }
    } else {
      base_fetcher_->Fetch(url ,handler, fetch);
      return;
    }
  } else {
    base_fetcher_->Fetch(url ,handler, fetch);
    return;
  }

  if (!ret) {
    RespondError(response_headers, fetch, handler);
  }
  fetch->Done(ret);
}

}  // namespace net_instaweb
