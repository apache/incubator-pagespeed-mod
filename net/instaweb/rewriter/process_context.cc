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


#include "net/instaweb/rewriter/public/process_context.h"

#include "base/logging.h"
#include "pagespeed/kernel/html/html_keywords.h"
#include "pagespeed/kernel/http/domain_registry.h"
#include "pagespeed/kernel/http/google_url.h"
#include "pagespeed/kernel/js/js_tokenizer.h"

#include "base/at_exit.h"
#include "google/protobuf/stubs/common.h"
using namespace google;  // NOLINT

namespace {

int construction_count = 0;

}

// Clean up valgrind-based memory-leak checks by deleting statically allocated
// data from various libraries.  This must be called both from unit-tests
// and from the Apache module, so that valgrind can be run on both of them.

namespace net_instaweb {

namespace {
base::AtExitManager* at_exit_manager = NULL;
}

ProcessContext::ProcessContext()
    : js_tokenizer_patterns_(new pagespeed::js::JsTokenizerPatterns) {
  ++construction_count;
  CHECK_EQ(1, construction_count)
      << "ProcessContext must only be constructed once.";

  domain_registry::Init();
  HtmlKeywords::Init();

  // url/url_util.cc lazily initializes its "standard_schemes" table in a
  // thread-unsafe way and so it must be explicitly initialized prior to thread
  // creation, and explicitly terminated after thread quiescence.
  url::Initialize();

  if (at_exit_manager == NULL) {
    at_exit_manager = new base::AtExitManager;
  }
}

ProcessContext::~ProcessContext() {
  // Clean up statics from third_party code first.

  // The protobuf shutdown infrastructure is lazily initialized in a threadsafe
  // manner.  See third_party/protobuf/src/google/protobuf/stubs/common.cc,
  // function InitShutdownFunctionsOnce.
  google::protobuf::ShutdownProtobufLibrary();

  url::Shutdown();
  HtmlKeywords::ShutDown();
  if (at_exit_manager != NULL) {
    delete at_exit_manager;
    at_exit_manager = NULL;
  }
}

}  // namespace net_instaweb
