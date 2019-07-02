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

//
//
// CPU: Intel Westmere with HyperThreading (3 cores) dL1:32KB dL2:256KB
// Benchmark                               Time(ns)    CPU(ns) Iterations
// ----------------------------------------------------------------------
// BM_ParseAndSerializeNewParserEachIter     433780     433690       1591
// BM_ParseAndSerializeReuseParser           433498     436118       1628
// BM_ParseAndSerializeReuseParserX50      22954185   22900000        100
//
// Disclaimer: comparing runs over time and across different machines
// can be misleading.  When contemplating an algorithm change, always do
// interleaved runs with the old & new algorithm.

#include "pagespeed/kernel/html/html_parse.h"

#include <algorithm>
#include <cstdlib>  // for exit
#include <memory>
#include <vector>

#include "base/logging.h"
#include "strings/stringpiece_utils.h"
#include "pagespeed/kernel/base/benchmark.h"
#include "pagespeed/kernel/base/google_message_handler.h"
#include "pagespeed/kernel/base/null_message_handler.h"
#include "pagespeed/kernel/base/null_writer.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/html/html_writer_filter.h"

namespace net_instaweb {

namespace {

// Lazily grab all the HTML text from testdata.  Note that we will
// never free this string but that's not considered a memory leak
// in Google because it's reachable from a static.
//
// TODO(jmarantz): this function was duplicated to
// third_party/pagespeed/automatic/rewriter_speed_test.cc and should possibly
// be factored out.
GoogleString* sHtmlText = NULL;
const StringPiece GetHtmlText() {
  if (sHtmlText == NULL) {
    sHtmlText = new GoogleString;
    StdioFileSystem file_system;
    StringVector files;
    GoogleMessageHandler handler;
    static const char kDir[] = "net/instaweb/htmlparse/testdata";
    if (!file_system.ListContents(kDir, &files, &handler)) {
      LOG(ERROR) << "Unable to find test data for HTML benchmark, skipping";
      return StringPiece();
    }
    std::sort(files.begin(), files.end());
    for (int i = 0, n = files.size(); i < n; ++i) {
      GoogleString buffer;
      // Note that we do not want to include xmp_tag.html here as it
      // includes an unterminated <xmp> tag, so anything afterwards
      // will just get accumulated into that --- which was especially
      // noticeable in the X100 test.
      if (strings::EndsWith(StringPiece(files[i]), "xmp_tag.html")) {
        continue;
      }

      if (strings::EndsWith(StringPiece(files[i]), ".html")) {
        if (!file_system.ReadFile(files[i].c_str(), &buffer, &handler)) {
          LOG(ERROR) << "Unable to open:" << files[i];
          exit(1);
        }
      }
      StrAppend(sHtmlText, buffer);
    }
  }
  return *sHtmlText;
}

static void BM_ParseAndSerializeNewParserEachIter(int iters) {
  StopBenchmarkTiming();
  StringPiece text = GetHtmlText();
  if (text.empty()) {
    return;
  }
  NullWriter writer;
  NullMessageHandler handler;

  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    HtmlParse parser(&handler);
    HtmlWriterFilter writer_filter(&parser);
    parser.AddFilter(&writer_filter);
    writer_filter.set_writer(&writer);
    parser.StartParse("http://example.com/benchmark");
    parser.ParseText(text);
    parser.FinishParse();
  }
}
BENCHMARK(BM_ParseAndSerializeNewParserEachIter);

static void BM_ParseAndSerializeReuseParser(int iters) {
  StopBenchmarkTiming();
  StringPiece text = GetHtmlText();
  if (text.empty()) {
    return;
  }

  NullWriter writer;
  NullMessageHandler handler;
  HtmlParse parser(&handler);
  HtmlWriterFilter writer_filter(&parser);
  parser.AddFilter(&writer_filter);
  writer_filter.set_writer(&writer);

  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    parser.StartParse("http://example.com/benchmark");
    parser.ParseText(text);
    parser.FinishParse();
  }
}
BENCHMARK(BM_ParseAndSerializeReuseParser);

static void BM_ParseAndSerializeReuseParserX50(int iters) {
  StopBenchmarkTiming();
  StringPiece orig = GetHtmlText();
  if (orig.empty()) {
    return;
  }
  GoogleString text;
  text.reserve(50 * orig.size());
  // Repeat the text 50 times to get a ~1.5M file.
  for (int i = 0; i < 50; ++i) {
    StrAppend(&text, orig);
  }

  NullWriter writer;
  NullMessageHandler handler;
  HtmlParse parser(&handler);
  HtmlWriterFilter writer_filter(&parser);
  parser.AddFilter(&writer_filter);
  writer_filter.set_writer(&writer);

  StartBenchmarkTiming();
  for (int i = 0; i < iters; ++i) {
    parser.StartParse("http://example.com/benchmark");
    parser.ParseText(text);
    parser.FinishParse();
  }
}
BENCHMARK(BM_ParseAndSerializeReuseParserX50);

}  // namespace

}  // namespace net_instaweb
