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


#include "net/instaweb/http/public/http_response_parser.h"

#include <cstdio>
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/stack_buffer.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/base/writer.h"
#include "pagespeed/kernel/http/response_headers_parser.h"

namespace net_instaweb {

bool HttpResponseParser::ParseFile(FileSystem::InputFile* file) {
  char buf[kStackBufferSize];
  int nread;
  while (ok_ && ((nread = file->Read(buf, sizeof(buf), handler_)) > 0)) {
    ParseChunk(StringPiece(buf, nread));
  }
  return ok_;
}

bool HttpResponseParser::Parse(FILE* stream) {
  char buf[kStackBufferSize];
  int nread;
  while (ok_ && ((nread = fread(buf, 1, sizeof(buf), stream)) > 0)) {
    ParseChunk(StringPiece(buf, nread));
  }
  return ok_;
}

bool HttpResponseParser::ParseChunk(const StringPiece& data) {
  if (reading_headers_) {
    int consumed = parser_.ParseChunk(data, handler_);
    if (parser_.headers_complete()) {
      // In this chunk we may have picked up some of the body.
      // Before we move to the next buffer, send it to the output
      // stream.
      ok_ = writer_->Write(data.substr(consumed), handler_);
      reading_headers_ = false;
    }
  } else {
    ok_ = writer_->Write(data, handler_);
  }
  return ok_;
}

}  // namespace net_instaweb
