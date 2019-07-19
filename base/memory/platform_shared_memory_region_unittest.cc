// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/platform_shared_memory_region.h"

#include "base/logging.h"
#include "base/memory/shared_memory.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/process/process_metrics.h"
#include "base/system/sys_info.h"
#include "base/test/gtest_util.h"
#include "base/test/test_shared_memory_util.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX) && !defined(OS_IOS)
#include <mach/mach_vm.h>
#include <sys/mman.h>
#elif defined(OS_POSIX) && !defined(OS_IOS)
#include <sys/mman.h>
#include "base/debug/proc_maps_linux.h"
#elif defined(OS_WIN)
#include <windows.h>
#elif defined(OS_FUCHSIA)
#include <lib/zx/object.h>
#include <lib/zx/process.h>
#include "base/fuchsia/fuchsia_logging.h"
#endif

namespace base {
namespace subtle {

const size_t kRegionSize = 1024;

class PlatformSharedMemoryRegionTest : public ::testing::Test {};

// Tests that a default constructed region is invalid and produces invalid
// mappings.
TEST_F(PlatformSharedMemoryRegionTest, DefaultConstructedRegionIsInvalid) {
  PlatformSharedMemoryRegion region;
  EXPECT_FALSE(region.IsValid());
  WritableSharedMemoryMapping mapping = MapForTesting(&region);
  EXPECT_FALSE(mapping.IsValid());
  PlatformSharedMemoryRegion duplicate = region.Duplicate();
  EXPECT_FALSE(duplicate.IsValid());
  EXPECT_FALSE(region.ConvertToReadOnly());
}

// Tests that creating a region of 0 size returns an invalid region.
TEST_F(PlatformSharedMemoryRegionTest, CreateRegionOfZeroSizeIsInvalid) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(0);
  EXPECT_FALSE(region.IsValid());

  PlatformSharedMemoryRegion region2 =
      PlatformSharedMemoryRegion::CreateUnsafe(0);
  EXPECT_FALSE(region2.IsValid());
}

// Tests that creating a region of size bigger than the integer max value
// returns an invalid region.
TEST_F(PlatformSharedMemoryRegionTest, CreateTooLargeRegionIsInvalid) {
  size_t too_large_region_size =
      static_cast<size_t>(std::numeric_limits<int>::max()) + 1;
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(too_large_region_size);
  EXPECT_FALSE(region.IsValid());

  PlatformSharedMemoryRegion region2 =
      PlatformSharedMemoryRegion::CreateUnsafe(too_large_region_size);
  EXPECT_FALSE(region2.IsValid());
}

// Tests that regions consistently report their size as the size requested at
// creation time even if their allocation size is larger due to platform
// constraints.
TEST_F(PlatformSharedMemoryRegionTest, ReportedSizeIsRequestedSize) {
  constexpr size_t kTestSizes[] = {1, 2, 3, 64, 4096, 1024 * 1024};
  for (size_t size : kTestSizes) {
    PlatformSharedMemoryRegion region =
        PlatformSharedMemoryRegion::CreateWritable(size);
    EXPECT_EQ(region.GetSize(), size);

    region.ConvertToReadOnly();
    EXPECT_EQ(region.GetSize(), size);
  }
}

// Tests that a writable region can be converted to read-only.
TEST_F(PlatformSharedMemoryRegionTest, ConvertWritableToReadOnly) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_EQ(region.GetMode(), PlatformSharedMemoryRegion::Mode::kWritable);
  ASSERT_TRUE(region.ConvertToReadOnly());
  EXPECT_EQ(region.GetMode(), PlatformSharedMemoryRegion::Mode::kReadOnly);
}

// Tests that a writable region can be converted to unsafe.
TEST_F(PlatformSharedMemoryRegionTest, ConvertWritableToUnsafe) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_EQ(region.GetMode(), PlatformSharedMemoryRegion::Mode::kWritable);
  ASSERT_TRUE(region.ConvertToUnsafe());
  EXPECT_EQ(region.GetMode(), PlatformSharedMemoryRegion::Mode::kUnsafe);
}

// Tests that the platform-specific handle converted to read-only cannot be used
// to perform a writable mapping with low-level system APIs like mmap().
TEST_F(PlatformSharedMemoryRegionTest, ReadOnlyHandleIsNotWritable) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_TRUE(region.ConvertToReadOnly());
  EXPECT_EQ(region.GetMode(), PlatformSharedMemoryRegion::Mode::kReadOnly);
  EXPECT_TRUE(
      CheckReadOnlyPlatformSharedMemoryRegionForTesting(std::move(region)));
}

// Tests that the PassPlatformHandle() call invalidates the region.
TEST_F(PlatformSharedMemoryRegionTest, InvalidAfterPass) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  ignore_result(region.PassPlatformHandle());
  EXPECT_FALSE(region.IsValid());
}

// Tests that the region is invalid after move.
TEST_F(PlatformSharedMemoryRegionTest, InvalidAfterMove) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  PlatformSharedMemoryRegion moved_region = std::move(region);
  EXPECT_FALSE(region.IsValid());
  EXPECT_TRUE(moved_region.IsValid());
}

// Tests that calling Take() with the size parameter equal to zero returns an
// invalid region.
TEST_F(PlatformSharedMemoryRegionTest, TakeRegionOfZeroSizeIsInvalid) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  PlatformSharedMemoryRegion region2 = PlatformSharedMemoryRegion::Take(
      region.PassPlatformHandle(), region.GetMode(), 0, region.GetGUID());
  EXPECT_FALSE(region2.IsValid());
}

// Tests that calling Take() with the size parameter bigger than the integer max
// value returns an invalid region.
TEST_F(PlatformSharedMemoryRegionTest, TakeTooLargeRegionIsInvalid) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  PlatformSharedMemoryRegion region2 = PlatformSharedMemoryRegion::Take(
      region.PassPlatformHandle(), region.GetMode(),
      static_cast<size_t>(std::numeric_limits<int>::max()) + 1,
      region.GetGUID());
  EXPECT_FALSE(region2.IsValid());
}

// Tests that mapping zero bytes fails.
TEST_F(PlatformSharedMemoryRegionTest, MapAtZeroBytesTest) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  WritableSharedMemoryMapping mapping = MapAtForTesting(&region, 0, 0);
  EXPECT_FALSE(mapping.IsValid());
}

// Tests that mapping bytes out of the region limits fails.
TEST_F(PlatformSharedMemoryRegionTest, MapAtOutOfTheRegionLimitsTest) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  WritableSharedMemoryMapping mapping =
      MapAtForTesting(&region, 0, region.GetSize() + 1);
  EXPECT_FALSE(mapping.IsValid());
}

// Tests that mapping with a size and offset causing overflow fails.
TEST_F(PlatformSharedMemoryRegionTest, MapAtWithOverflowTest) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(
          SysInfo::VMAllocationGranularity() * 2);
  ASSERT_TRUE(region.IsValid());
  size_t size = std::numeric_limits<size_t>::max();
  size_t offset = SysInfo::VMAllocationGranularity();
  // |size| + |offset| should be below the region size due to overflow but
  // mapping a region with these parameters should be invalid.
  EXPECT_LT(size + offset, region.GetSize());
  WritableSharedMemoryMapping mapping = MapAtForTesting(&region, offset, size);
  EXPECT_FALSE(mapping.IsValid());
}

#if defined(OS_POSIX) && !defined(OS_ANDROID) && \
    (!defined(OS_MACOSX) || defined(OS_IOS))
// Tests that the second handle is closed after a conversion to read-only on
// POSIX.
TEST_F(PlatformSharedMemoryRegionTest,
       ConvertToReadOnlyInvalidatesSecondHandle) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  ASSERT_TRUE(region.ConvertToReadOnly());
  FDPair fds = region.GetPlatformHandle();
  EXPECT_LT(fds.readonly_fd, 0);
}

// Tests that the second handle is closed after a conversion to unsafe on
// POSIX.
TEST_F(PlatformSharedMemoryRegionTest, ConvertToUnsafeInvalidatesSecondHandle) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  ASSERT_TRUE(region.ConvertToUnsafe());
  FDPair fds = region.GetPlatformHandle();
  EXPECT_LT(fds.readonly_fd, 0);
}
#endif

void CheckReadOnlyMapProtection(void* addr) {
#if defined(OS_MACOSX) && !defined(OS_IOS)
  vm_region_basic_info_64 basic_info;
  mach_vm_size_t dummy_size = 0;
  void* temp_addr = addr;
  MachVMRegionResult result = GetBasicInfo(
      mach_task_self(), &dummy_size,
      reinterpret_cast<mach_vm_address_t*>(&temp_addr), &basic_info);
  ASSERT_EQ(result, MachVMRegionResult::Success);
  EXPECT_EQ(basic_info.protection & VM_PROT_ALL, VM_PROT_READ);
  EXPECT_EQ(basic_info.max_protection & VM_PROT_ALL, VM_PROT_READ);
#elif defined(OS_POSIX) && !defined(OS_IOS)
  std::string proc_maps;
  ASSERT_TRUE(base::debug::ReadProcMaps(&proc_maps));
  std::vector<base::debug::MappedMemoryRegion> regions;
  ASSERT_TRUE(base::debug::ParseProcMaps(proc_maps, &regions));
  auto it =
      std::find_if(regions.begin(), regions.end(),
                   [addr](const base::debug::MappedMemoryRegion& region) {
                     return region.start == reinterpret_cast<uintptr_t>(addr);
                   });
  ASSERT_TRUE(it != regions.end());
  // PROT_READ may imply PROT_EXEC on some architectures, so just check that
  // permissions don't contain PROT_WRITE bit.
  EXPECT_FALSE(it->permissions & base::debug::MappedMemoryRegion::WRITE);
#elif defined(OS_WIN)
  MEMORY_BASIC_INFORMATION memory_info;
  size_t result = VirtualQueryEx(GetCurrentProcess(), addr, &memory_info,
                                 sizeof(memory_info));

  ASSERT_GT(result, 0ULL) << "Failed executing VirtualQueryEx "
                          << logging::SystemErrorCodeToString(
                                 logging::GetLastSystemErrorCode());
  EXPECT_EQ(memory_info.AllocationProtect, static_cast<DWORD>(PAGE_READONLY));
  EXPECT_EQ(memory_info.Protect, static_cast<DWORD>(PAGE_READONLY));
#elif defined(OS_FUCHSIA)
// TODO(alexilin): We cannot call zx_object_get_info ZX_INFO_PROCESS_MAPS in
// this process. Consider to create an auxiliary process that will read the
// test process maps.
#endif
}

bool TryToRestoreWritablePermissions(void* addr, size_t len) {
#if defined(OS_POSIX) && !defined(OS_IOS)
  int result = mprotect(addr, len, PROT_READ | PROT_WRITE);
  return result != -1;
#elif defined(OS_WIN)
  DWORD old_protection;
  return VirtualProtect(addr, len, PAGE_READWRITE, &old_protection);
#elif defined(OS_FUCHSIA)
  zx_status_t status =
      zx::vmar::root_self()->protect(reinterpret_cast<uintptr_t>(addr), len,
                                     ZX_VM_PERM_READ | ZX_VM_PERM_WRITE);
  return status == ZX_OK;
#else
  return false;
#endif
}

// Tests that protection bits are set correctly for read-only region.
TEST_F(PlatformSharedMemoryRegionTest, MappingProtectionSetCorrectly) {
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  ASSERT_TRUE(region.ConvertToReadOnly());
  WritableSharedMemoryMapping ro_mapping = MapForTesting(&region);
  ASSERT_TRUE(ro_mapping.IsValid());
  CheckReadOnlyMapProtection(ro_mapping.memory());

  EXPECT_FALSE(TryToRestoreWritablePermissions(ro_mapping.memory(),
                                               ro_mapping.mapped_size()));
  CheckReadOnlyMapProtection(ro_mapping.memory());
}

// Tests that platform handle permissions are checked correctly.
TEST_F(PlatformSharedMemoryRegionTest,
       CheckPlatformHandlePermissionsCorrespondToMode) {
  using Mode = PlatformSharedMemoryRegion::Mode;
  auto check = [](const PlatformSharedMemoryRegion& region,
                  PlatformSharedMemoryRegion::Mode mode) {
    return PlatformSharedMemoryRegion::
        CheckPlatformHandlePermissionsCorrespondToMode(
            region.GetPlatformHandle(), mode, region.GetSize());
  };

  // Check kWritable region.
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_TRUE(check(region, Mode::kWritable));
  EXPECT_FALSE(check(region, Mode::kReadOnly));

  // Check kReadOnly region.
  ASSERT_TRUE(region.ConvertToReadOnly());
  EXPECT_TRUE(check(region, Mode::kReadOnly));
  EXPECT_FALSE(check(region, Mode::kWritable));
  EXPECT_FALSE(check(region, Mode::kUnsafe));

  // Check kUnsafe region.
  PlatformSharedMemoryRegion region2 =
      PlatformSharedMemoryRegion::CreateUnsafe(kRegionSize);
  ASSERT_TRUE(region2.IsValid());
  EXPECT_TRUE(check(region2, Mode::kUnsafe));
  EXPECT_FALSE(check(region2, Mode::kReadOnly));
}

// Tests that it's impossible to create read-only platform shared memory region.
TEST_F(PlatformSharedMemoryRegionTest, CreateReadOnlyRegionDeathTest) {
#ifdef OFFICIAL_BUILD
  // The official build does not print the reason a CHECK failed.
  const char kErrorRegex[] = "";
#else
  const char kErrorRegex[] =
      "Creating a region in read-only mode will lead to this region being "
      "non-modifiable";
#endif
  EXPECT_DEATH_IF_SUPPORTED(
      PlatformSharedMemoryRegion::Create(
          PlatformSharedMemoryRegion::Mode::kReadOnly, kRegionSize),
      kErrorRegex);
}

// Tests that it's prohibited to duplicate a writable region.
TEST_F(PlatformSharedMemoryRegionTest, DuplicateWritableRegionDeathTest) {
#ifdef OFFICIAL_BUILD
  const char kErrorRegex[] = "";
#else
  const char kErrorRegex[] =
      "Duplicating a writable shared memory region is prohibited";
#endif
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_DEATH_IF_SUPPORTED(region.Duplicate(), kErrorRegex);
}

// Tests that it's prohibited to convert an unsafe region to read-only.
TEST_F(PlatformSharedMemoryRegionTest, UnsafeRegionConvertToReadOnlyDeathTest) {
#ifdef OFFICIAL_BUILD
  const char kErrorRegex[] = "";
#else
  const char kErrorRegex[] =
      "Only writable shared memory region can be converted to read-only";
#endif
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateUnsafe(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_DEATH_IF_SUPPORTED(region.ConvertToReadOnly(), kErrorRegex);
}

// Tests that it's prohibited to convert a read-only region to read-only.
TEST_F(PlatformSharedMemoryRegionTest,
       ReadOnlyRegionConvertToReadOnlyDeathTest) {
#ifdef OFFICIAL_BUILD
  const char kErrorRegex[] = "";
#else
  const char kErrorRegex[] =
      "Only writable shared memory region can be converted to read-only";
#endif
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_TRUE(region.ConvertToReadOnly());
  EXPECT_DEATH_IF_SUPPORTED(region.ConvertToReadOnly(), kErrorRegex);
}

// Tests that it's prohibited to convert a read-only region to unsafe.
TEST_F(PlatformSharedMemoryRegionTest, ReadOnlyRegionConvertToUnsafeDeathTest) {
#ifdef OFFICIAL_BUILD
  const char kErrorRegex[] = "";
#else
  const char kErrorRegex[] =
      "Only writable shared memory region can be converted to unsafe";
#endif
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateWritable(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  ASSERT_TRUE(region.ConvertToReadOnly());
  EXPECT_DEATH_IF_SUPPORTED(region.ConvertToUnsafe(), kErrorRegex);
}

// Tests that it's prohibited to convert an unsafe region to unsafe.
TEST_F(PlatformSharedMemoryRegionTest, UnsafeRegionConvertToUnsafeDeathTest) {
#ifdef OFFICIAL_BUILD
  const char kErrorRegex[] = "";
#else
  const char kErrorRegex[] =
      "Only writable shared memory region can be converted to unsafe";
#endif
  PlatformSharedMemoryRegion region =
      PlatformSharedMemoryRegion::CreateUnsafe(kRegionSize);
  ASSERT_TRUE(region.IsValid());
  EXPECT_DEATH_IF_SUPPORTED(region.ConvertToUnsafe(), kErrorRegex);
}

// Check that taking from a SharedMemoryHandle works.
TEST_F(PlatformSharedMemoryRegionTest, TakeFromSharedMemoryHandle) {
  SharedMemory shm;
  auto region = PlatformSharedMemoryRegion::TakeFromSharedMemoryHandle(
      shm.TakeHandle(), PlatformSharedMemoryRegion::Mode::kUnsafe);
  ASSERT_FALSE(region.IsValid());

  shm.CreateAndMapAnonymous(10);
  region = PlatformSharedMemoryRegion::TakeFromSharedMemoryHandle(
      shm.TakeHandle(), PlatformSharedMemoryRegion::Mode::kUnsafe);
  ASSERT_TRUE(region.IsValid());

#if !(defined(OS_MACOSX) && !defined(OS_IOS))
  // Note that it's not possible on all platforms for TakeFromSharedMemoryHandle
  // to conveniently check if the SharedMemoryHandle is readonly or
  // not. Therefore it is actually possible to get an kUnsafe
  // PlatformSharedMemoryRegion from a readonly handle on some platforms.
  SharedMemoryCreateOptions options;
  options.size = 10;
  options.share_read_only = true;
  shm.Create(options);
  EXPECT_DEATH_IF_SUPPORTED(
      PlatformSharedMemoryRegion::TakeFromSharedMemoryHandle(
          shm.GetReadOnlyHandle(), PlatformSharedMemoryRegion::Mode::kUnsafe),
      "");
#endif  // !(defined(OS_MACOSX) && !defined(OS_IOS))
}

// Check that taking from a readonly SharedMemoryHandle works.
TEST_F(PlatformSharedMemoryRegionTest, TakeFromReadOnlySharedMemoryHandle) {
  SharedMemory shm;
  // Note that getting a read-only handle from an unmapped SharedMemory will
  // fail, so the invalid region case cannot be tested.
  SharedMemoryCreateOptions options;
  options.size = 10;
  options.share_read_only = true;
  shm.Create(options);
  auto readonly_handle = shm.GetReadOnlyHandle();
#if defined(OS_ANDROID)
  readonly_handle.SetRegionReadOnly();
#endif
  auto region = PlatformSharedMemoryRegion::TakeFromSharedMemoryHandle(
      readonly_handle, PlatformSharedMemoryRegion::Mode::kReadOnly);
  ASSERT_TRUE(region.IsValid());
}

// Check that taking from a SharedMemoryHandle in writable mode fails.
TEST_F(PlatformSharedMemoryRegionTest, WritableTakeFromSharedMemoryHandle) {
  SharedMemory shm;
  EXPECT_DEATH_IF_SUPPORTED(
      PlatformSharedMemoryRegion::TakeFromSharedMemoryHandle(
          shm.TakeHandle(), PlatformSharedMemoryRegion::Mode::kWritable),
      "");

  shm.CreateAndMapAnonymous(10);
  EXPECT_DEATH_IF_SUPPORTED(
      PlatformSharedMemoryRegion::TakeFromSharedMemoryHandle(
          shm.TakeHandle(), PlatformSharedMemoryRegion::Mode::kWritable),
      "");
}

}  // namespace subtle
}  // namespace base
