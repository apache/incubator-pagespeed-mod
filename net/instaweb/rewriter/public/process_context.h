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


#ifndef NET_INSTAWEB_REWRITER_PUBLIC_PROCESS_CONTEXT_H_
#define NET_INSTAWEB_REWRITER_PUBLIC_PROCESS_CONTEXT_H_

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/scoped_ptr.h"

namespace pagespeed { namespace js { struct JsTokenizerPatterns; } };

namespace net_instaweb {

// This class encapsulates the initialization and cleanup of static and
// global variables across Pagespeed Automatic.  The usage of this class
// is optional, but can help with cleaning up valgrind messages.
//
// It is up to the user to ensure the destructor is called at an appropriate
// time in their flow.  There is no statically constructed object declared
// in mem_clean_up.cc, although this class can be instantiated statically
// if that's the best mechanism in the environment.
class ProcessContext {
 public:
  ProcessContext();
  ~ProcessContext();

  // Returns a js_tokenizer_patterns object that was allocated by the
  // ProcessContext constructor, once per process startup.  The reason
  // to manage this object this way, as opposed to constructing it
  // from the factory, is to speed up tests, where factories are
  // constructed and destroyed for every test method.
  // JsTokenizerPatterns requires compiles multiple regular
  // expressions on construction, and this takes a significant amount
  // of time in debug builds.
  const pagespeed::js::JsTokenizerPatterns* js_tokenizer_patterns() const {
    return js_tokenizer_patterns_.get();
  }

 private:
  scoped_ptr<pagespeed::js::JsTokenizerPatterns> js_tokenizer_patterns_;

  DISALLOW_COPY_AND_ASSIGN(ProcessContext);
};

}  // namespace net_instaweb

#endif  // NET_INSTAWEB_REWRITER_PUBLIC_PROCESS_CONTEXT_H_
