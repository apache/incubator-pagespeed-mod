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

#include <cstdio>
#include <cstdlib>

#include "net/instaweb/rewriter/public/javascript_library_identification.h"
#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_message_handler.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/md5_hasher.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/stdio_file_system.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/string_util.h"
#include "pagespeed/kernel/js/js_minify.h"
#include "pagespeed/kernel/js/js_tokenizer.h"
#include "pagespeed/kernel/util/gflags.h"

// Command-line javascript minifier and metadata printer.  Takes a single
// javascript file as either standard input or a command-line argument, and by
// default prints the minified code for that file to stdout.  If
// --print_size_and_hash is specified, it instead prints the size of the
// minified file (in bytes) and its minified md5 sum, suitable for configuring
// library recognition in mod_pagespeed. If --use_experimental_minifier is
// specified, use the new JS minifier.

namespace net_instaweb {

DEFINE_bool(print_size_and_hash, false,
            "Instead of printing minified JavaScript, print the size "
            "and url-encoded md5 checksum of the minified input.  "
            "This yields results suitable for a "
            "ModPagespeedLibrary directive.");

DEFINE_bool(use_experimental_minifier, true,
            "Use the new JS minifier to minify the input instead "
            "of the old one.");

namespace {

bool JSMinifyMain(int argc, char** argv) {
  net_instaweb::FileMessageHandler handler(stderr);
  net_instaweb::StdioFileSystem file_system;
  if (argc >= 4) {
    handler.Message(kError,
                    "Usage: \n"
                    "  js_minify [--print_size_and_hash] "
                    "[--nouse_experimental_minifier] foo.js\n"
                    "  js_minify [--print_size_and_hash] "
                    "[--nouse_experimental_minifier] < foo.js\n"
                    "Without --print_size_and_hash prints minified foo.js\n"
                    "With --print_size_and_hash instead prints minified "
                    "size and content hash suitable for ModPagespeedLibrary\n");
    return false;
  }
  // Choose stdin if no file name on command line.
  const char* filename = "<stdin>";
  FileSystem::InputFile* input = file_system.Stdin();
  if (argc == 2) {
    filename = argv[1];
    input = file_system.OpenInputFile(filename, &handler);
  }
  // Just read and process the input in bulk.
  GoogleString original;
  if (!file_system.ReadFile(input, &original, &handler)) {
    return false;
  }
  // Decide which minifier we are using.
  GoogleString stripped;
  bool result;
  if (FLAGS_use_experimental_minifier) {
    pagespeed::js::JsTokenizerPatterns patterns;
    result = pagespeed::js::MinifyUtf8Js(&patterns, original, &stripped);
  } else {
    result = pagespeed::js::MinifyJs(original, &stripped);
  }
  if (!result) {
    handler.Message(kError,
                    "%s: Couldn't minify; "
                    "stripping leading and trailing whitespace.\n",
                    filename);
    TrimWhitespace(original, &stripped);
  }
  FileSystem::OutputFile* stdout = file_system.Stdout();
  if (FLAGS_print_size_and_hash) {
    MD5Hasher hasher(JavascriptLibraryIdentification::kNumHashChars);
    uint64 size = stripped.size();
    bool ret = stdout->Write(Integer64ToString(size), &handler);
    ret &= stdout->Write(" ", &handler);
    ret &= stdout->Write(hasher.Hash(stripped), &handler);
    return ret;
  } else {
    return stdout->Write(stripped, &handler);
  }
}

}  // namespace

}  // namespace net_instaweb

int main(int argc, char** argv) {
  net_instaweb::ParseGflags(argv[0], &argc, &argv);
  return net_instaweb::JSMinifyMain(argc, argv) ? EXIT_SUCCESS : EXIT_FAILURE;
}
