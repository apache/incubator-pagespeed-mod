// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/winheap_stubs_win.h"

#include "base/bits.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace allocator {
namespace {

bool IsPtrAligned(void* ptr, size_t alignment) {
  CHECK(base::bits::IsPowerOfTwo(alignment));
  uintptr_t address = reinterpret_cast<uintptr_t>(ptr);
  return base::bits::Align(address, alignment) == address;
}

}  // namespace

TEST(WinHeapStubs, AlignedAllocationAreAligned) {
  for (size_t alignment = 1; alignment < 65536; alignment *= 2) {
    SCOPED_TRACE(alignment);

    void* ptr = WinHeapAlignedMalloc(10, alignment);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(IsPtrAligned(ptr, alignment));

    ptr = WinHeapAlignedRealloc(ptr, 1000, alignment);
    ASSERT_NE(ptr, nullptr);
    EXPECT_TRUE(IsPtrAligned(ptr, alignment));

    WinHeapAlignedFree(ptr);
  }
}

TEST(WinHeapStubs, AlignedReallocationsCorrectlyCopyData) {
  constexpr size_t kAlignment = 64;
  constexpr uint8_t kMagicByte = 0xab;

  size_t old_size = 8;
  void* ptr = WinHeapAlignedMalloc(old_size, kAlignment);
  ASSERT_NE(ptr, nullptr);

  // Cause allocations to grow and shrink and confirm allocation contents are
  // copied regardless.
  constexpr size_t kSizes[] = {10, 1000, 50, 3000, 30, 9000};

  for (size_t size : kSizes) {
    SCOPED_TRACE(size);

    memset(ptr, kMagicByte, old_size);
    ptr = WinHeapAlignedRealloc(ptr, size, kAlignment);
    ASSERT_NE(ptr, nullptr);

    for (size_t i = 0; i < std::min(size, old_size); i++) {
      SCOPED_TRACE(i);
      ASSERT_EQ(reinterpret_cast<uint8_t*>(ptr)[i], kMagicByte);
    }

    old_size = size;
  }

  WinHeapAlignedFree(ptr);
}

}  // namespace allocator
}  // namespace base
