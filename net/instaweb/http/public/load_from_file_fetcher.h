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

// Author: oschaafi@we-amp.com (Otto van der Schaaf)

#ifndef NET_INSTAWEB_HTTP_PUBLIC_LOAD_FROM_FILE_FETCHER_H_
#define NET_INSTAWEB_HTTP_PUBLIC_LOAD_FROM_FILE_FETCHER_H_

#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

class AsyncFetch;
class GoogleUrl;
class MessageHandler;
class ResponseHeaders;
class RewriteOptions;
class Timer;
class Writer;

// TODO(sligocki): Can we forward declare these somehow?
// class FileSystem;
// class FileSystem::InputFile;

class LoadFromFileFetcher : public UrlAsyncFetcher {
 public:
  LoadFromFileFetcher(const RewriteOptions* options, FileSystem* file_system,
                     Timer* timer, UrlAsyncFetcher* base_fetcher);
  virtual ~LoadFromFileFetcher();


  // This is a synchronous/blocking implementation.
  virtual void Fetch(const GoogleString& url,
                     MessageHandler* message_handler,
                     AsyncFetch* fetch);

  // Helper function to return a generic error response.
  void RespondError(ResponseHeaders* response_headers, Writer* response_writer,
                    MessageHandler* handler);

 private:
  const RewriteOptions* rewrite_options_;
  FileSystem* file_system_;
  Timer* timer_;

  // Response to use if something goes wrong.
  GoogleString error_body_;
  UrlAsyncFetcher* base_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(LoadFromFileFetcher);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_LOAD_FROM_FILE_FETCHER_H_
