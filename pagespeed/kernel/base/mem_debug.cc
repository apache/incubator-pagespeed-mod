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


// Very simple memory debugging overrides for operator new/delete, to
// help us quickly find simple memory violations:
//   1. Double destruct
//   2. Read before write (via scribbling)
//   3. Read after delete (via scribbling)
//
// Note that valgrind does all of this much better, but is too slow to
// run all the time, and it's not obvious how well it works when
// Apache is forking processes that do all the interesting work.  However,
// if this code helps find a problem then you can run valgrind with Apache
// in -X mode and that will be a better debug tool.
//
// Principle of operation: add 8 bytes to every allocation.  The first
// 4 bytes are a marker (kLiveMarker or kDeadMarker1).  The next 4
// bytes are used to store size of the allocation, which helps us
// know how many bytes to scribble when we free.
//
// TODO(jmarantz): consider integrating a richer memory debug library such
// as dmalloc, efence, or the heap-checking features of tcmalloc.
//
// Note: this memory debugging will interfere with Valgrind's ability to
// detect read-before-write errors, and hence should be disabled if you
// want to run with valgrind.  This can be detected automatically using
// macros from valgrind.h, but then that would *require* valgrind be installed
// before building mod_pagespeed.  See
//
// http://valgrind.org/docs/manual/manual-core-adv.html#manual-core-adv.clientreq

// TODO(jmarantz): consider controlling inclusion of this module in the gyp
// files rather than as an ifdef.
//
// Note: to be assured that these overrides are included in Debug builds but
// are not included in Release builds, type:
//   nm out/Debug/libmod_pagespeed.so   | /usr/bin/c++filt |grep 'operator new'
//   nm out/Release/libmod_pagespeed.so | /usr/bin/c++filt |grep 'operator new'
#ifndef NDEBUG

#include <cstdlib>
#include "base/logging.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace {

const int32 kLiveMarker = 0xfeedface;   // first 4 bytes after alloc
const int32 kDeadMarker1 = 0xabacabff;  // first 4 bytes after free
const int32 kDeadMarker2 = 0xdeadbeef;  // overwrites the 'size' field on free
const size_t kOverhead = 2 * sizeof(int32);  // number of extra bytes to alloc

size_t rounded_size(size_t size) {
  if (size == 0) {
    size = kOverhead;
  } else if ((size % kOverhead) != 0) {
    size = size + kOverhead - (size % kOverhead);
  }
  return size;
}

void scribble(void* ptr, size_t size, int32 scribble_word) {
  CHECK_EQ(0U, size % sizeof(int32));
  int num_ints = size / sizeof(int32);
  int* p = static_cast<int*>(ptr);
  for (int i = 0; i < num_ints; ++i, ++p) {
    *p = scribble_word;
  }
}

void* debug_malloc(size_t size) {
  size_t rounded = rounded_size(size);
  int32* marker = static_cast<int*>(malloc(rounded + kOverhead));
  CHECK(marker != NULL);
  marker[0] = kLiveMarker;
  marker[1] = size;
  int32* ret = marker + 2;
  scribble(ret, rounded, kLiveMarker);
  return reinterpret_cast<char*>(marker) + kOverhead;
}

void debug_free(void* ptr) {
  if (ptr != NULL) {
    char* alloced_ptr = static_cast<char*>(ptr) - kOverhead;
    int32* marker = reinterpret_cast<int32*>(alloced_ptr);
    scribble(ptr, rounded_size(marker[1]), kDeadMarker2);
    CHECK_EQ(kLiveMarker, marker[0]);
    marker[0] = kDeadMarker1;
    marker[1] = kDeadMarker2;
    free(marker);
  }
}

}  // namespace

// C++ operator new/delete overrides, in all 8 combinations:
//    (new vs delete) * (const std::nothrow_t& vs not) * ([] vs not)
// On MacOS __THROW appears to be missing so hide those in an ifdef.
#ifndef __THROW
#define __THROW
#endif

void* operator new(size_t size) throw (std::bad_alloc) {
  return debug_malloc(size);
}

void* operator new[](size_t size) throw (std::bad_alloc) {
  return debug_malloc(size);
}

void* operator new(size_t size, const std::nothrow_t&) __THROW {
  return debug_malloc(size);
}

void operator delete(void* ptr) __THROW {
  debug_free(ptr);
}

void operator delete(void* ptr, const std::nothrow_t&) __THROW {
  debug_free(ptr);
}

void* operator new[](size_t size, const std::nothrow_t&) __THROW {
  return debug_malloc(size);
}

void operator delete[](void* ptr) __THROW {
  debug_free(ptr);
}

void operator delete[](void* ptr, const std::nothrow_t&) __THROW {
  debug_free(ptr);
}

#endif  // !NDEBUG
