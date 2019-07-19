// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <mach/mach.h>

#include "base/allocator/allocator_interception_mac.h"
#include "base/allocator/allocator_shim.h"
#include "base/allocator/malloc_zone_functions_mac.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace allocator {

namespace {
void ResetMallocZone(ChromeMallocZone* zone) {
  MallocZoneFunctions& functions = GetFunctionsForZone(zone);
  ReplaceZoneFunctions(zone, &functions);
}

void ResetAllMallocZones() {
  ChromeMallocZone* default_malloc_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());
  ResetMallocZone(default_malloc_zone);

  vm_address_t* zones;
  unsigned int count;
  kern_return_t kr = malloc_get_all_zones(mach_task_self(), 0, &zones, &count);
  if (kr != KERN_SUCCESS)
    return;
  for (unsigned int i = 0; i < count; ++i) {
    ChromeMallocZone* zone = reinterpret_cast<ChromeMallocZone*>(zones[i]);
    ResetMallocZone(zone);
  }
}
}  // namespace

class AllocatorInterceptionTest : public testing::Test {
 protected:
  void TearDown() override {
    ResetAllMallocZones();
    ClearAllMallocZonesForTesting();
  }
};

#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
TEST_F(AllocatorInterceptionTest, ShimNewMallocZones) {
  InitializeAllocatorShim();
  ChromeMallocZone* default_malloc_zone =
      reinterpret_cast<ChromeMallocZone*>(malloc_default_zone());

  malloc_zone_t new_zone;
  memset(&new_zone, 1, sizeof(malloc_zone_t));
  malloc_zone_register(&new_zone);
  EXPECT_NE(new_zone.malloc, default_malloc_zone->malloc);
  ShimNewMallocZones();
  EXPECT_EQ(new_zone.malloc, default_malloc_zone->malloc);

  malloc_zone_unregister(&new_zone);
}
#endif

}  // namespace allocator
}  // namespace base
