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


#include "net/instaweb/http/public/http_dump_url_async_writer.h"

#include "base/logging.h"
#include "net/instaweb/http/public/async_fetch.h"
#include "net/instaweb/http/public/http_dump_url_fetcher.h"
#include "net/instaweb/http/public/request_context.h"
#include "net/instaweb/http/public/url_async_fetcher.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/file_writer.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/http/http_names.h"
#include "pagespeed/kernel/http/request_headers.h"
#include "pagespeed/kernel/http/response_headers.h"
#include "pagespeed/kernel/util/gzip_inflater.h"

namespace net_instaweb {

class HttpDumpUrlAsyncWriter::DumpFetch : public StringAsyncFetch {
 public:
  DumpFetch(const GoogleString& url, MessageHandler* handler,
            AsyncFetch* base_fetch, const GoogleString& filename,
            HttpDumpUrlFetcher* dump_fetcher, FileSystem* file_system,
            const RequestContextPtr& request_context)
      : StringAsyncFetch(request_context),
        url_(url), handler_(handler), base_fetch_(base_fetch),
        filename_(filename), dump_fetcher_(dump_fetcher),
        file_system_(file_system) {
    DCHECK(request_context.get() != NULL);
  }

  void StartFetch(const bool accept_gzip, UrlAsyncFetcher* base_fetcher) {
    // In general we will want to always ask the origin for gzipped output,
    // but we are leaving in variable so this could be overridden by the
    // instantiator of the DumpUrlWriter.
    request_headers()->CopyFrom(*base_fetch_->request_headers());
    if (accept_gzip) {
      request_headers()->Replace(HttpAttributes::kAcceptEncoding,
                                 HttpAttributes::kGzip);
    }

    base_fetcher->Fetch(url_, handler_, this);
  }

  // Finishes the Fetch when called back.
  virtual void HandleDone(bool success) {
    response_headers()->SetContentLength(buffer().size());
    // TODO(jmarantz): http://tools.ietf.org/html/rfc2616#section-13.5.1
    // tells us we can also remove Keep-Alive, Proxy-Authenticate,
    // Proxy-Authorization, TE, Trailers, Transfer-Encoding, and Upgrade.
    response_headers()->RemoveAll(HttpAttributes::kConnection);
    response_headers()->ComputeCaching();

    // Do not write an empty file if the fetch failed.
    if (success) {
      // Check to see if a response marked as gzipped are really unzippable.
      if (response_headers()->WasGzippedLast()) {
        GzipInflater inflater(GzipInflater::kGzip);
        inflater.Init();
        if (buffer().empty()) {
          response_headers()->Remove(HttpAttributes::kContentEncoding,
                                     HttpAttributes::kGzip);
        } else {
          CHECK(inflater.SetInput(buffer().data(), buffer().size()));
          while (inflater.HasUnconsumedInput()) {
            char buf[kStackBufferSize];
            if ((inflater.InflateBytes(buf, sizeof(buf)) == 0) ||
                inflater.error()) {
              response_headers()->RemoveAll(HttpAttributes::kContentEncoding);
              break;
            }
          }
        }
      }

      FileSystem::OutputFile* file = file_system_->OpenTempFile(
          filename_ + ".temp", handler_);
      if (file != NULL) {
        handler_->Message(kInfo, "Storing %s as %s", url_.c_str(),
                          filename_.c_str());
        GoogleString temp_filename = file->filename();
        FileWriter file_writer(file);
        success = response_headers()->WriteAsHttp(&file_writer, handler_) &&
            file->Write(buffer(), handler_);
        success &= file_system_->Close(file, handler_);
        success &= file_system_->RenameFile(temp_filename.c_str(),
                                            filename_.c_str(),
                                            handler_);
      } else {
        success = false;
      }
    }

    if (success) {
      // Let dump fetcher fetch the actual response so that it can decompress.
      StringAsyncFetch dump_target(request_context());
      dump_target.set_request_headers(base_fetch_->request_headers());
      dump_target.set_response_headers(base_fetch_->response_headers());
      dump_fetcher_->Fetch(url_, handler_, &dump_target);
      // We expect the dump fetcher to operate synchronously
      CHECK(dump_target.done());
      success = dump_target.success();
      base_fetch_->Write(dump_target.buffer(), handler_);
    } else if (response_headers()->status_code() != 0) {
      // We are not going to be able to read the response from the file
      // system so we better pass the error message through.
      //
      // Status code == 0 means that the headers were not even parsed, this
      // will cause a DCHECK failure in AsyncFetch, so we don't pass anything
      // through.
      base_fetch_->response_headers()->CopyFrom(*response_headers());
      base_fetch_->HeadersComplete();
      base_fetch_->Write(buffer(), handler_);
    }

    base_fetch_->Done(success);
    delete this;
  }

 private:
  const GoogleString url_;
  MessageHandler* handler_;
  AsyncFetch* base_fetch_;

  const GoogleString filename_;
  HttpDumpUrlFetcher* dump_fetcher_;
  FileSystem* file_system_;

  DISALLOW_COPY_AND_ASSIGN(DumpFetch);
};

HttpDumpUrlAsyncWriter::~HttpDumpUrlAsyncWriter() {
}

void HttpDumpUrlAsyncWriter::Fetch(const GoogleString& url,
                                   MessageHandler* handler,
                                   AsyncFetch* base_fetch) {
  GoogleString filename;
  GoogleUrl gurl(url);
  dump_fetcher_.GetFilename(gurl, &filename, handler);

  if (file_system_->Exists(filename.c_str(), handler).is_true()) {
    dump_fetcher_.Fetch(url, handler, base_fetch);
  } else {
    DumpFetch* fetch = new DumpFetch(url, handler, base_fetch, filename,
                                     &dump_fetcher_, file_system_,
                                     base_fetch->request_context());
    fetch->StartFetch(accept_gzip_, base_fetcher_);
  }
}

}  // namespace net_instaweb
