// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/win_util.h"

#include <objbase.h>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/scoped_native_library.h"
#include "base/stl_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"
#include "base/win/scoped_co_mem.h"
#include "base/win/win_client_metrics.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace win {

namespace {

// Saves the current thread's locale ID when initialized, and restores it when
// the instance is going out of scope.
class ThreadLocaleSaver {
 public:
  ThreadLocaleSaver() : original_locale_id_(GetThreadLocale()) {}
  ~ThreadLocaleSaver() { SetThreadLocale(original_locale_id_); }

 private:
  LCID original_locale_id_;

  DISALLOW_COPY_AND_ASSIGN(ThreadLocaleSaver);
};

}  // namespace

// The test is somewhat silly, because some bots some have UAC enabled and some
// have it disabled. At least we check that it does not crash.
TEST(BaseWinUtilTest, TestIsUACEnabled) {
  UserAccountControlIsEnabled();
}

TEST(BaseWinUtilTest, TestGetUserSidString) {
  string16 user_sid;
  EXPECT_TRUE(GetUserSidString(&user_sid));
  EXPECT_TRUE(!user_sid.empty());
}

TEST(BaseWinUtilTest, TestGetNonClientMetrics) {
  NONCLIENTMETRICS_XP metrics = {0};
  GetNonClientMetrics(&metrics);
  EXPECT_GT(metrics.cbSize, 0u);
  EXPECT_GT(metrics.iScrollWidth, 0);
  EXPECT_GT(metrics.iScrollHeight, 0);
}

TEST(BaseWinUtilTest, TestGetLoadedModulesSnapshot) {
  std::vector<HMODULE> snapshot;

  ASSERT_TRUE(GetLoadedModulesSnapshot(::GetCurrentProcess(), &snapshot));
  size_t original_snapshot_size = snapshot.size();
  ASSERT_GT(original_snapshot_size, 0u);
  snapshot.clear();

  // Load in a new module. Pick zipfldr.dll as it is present from WinXP to
  // Win10, including ARM64 Win10, and yet rarely used.
  const char16 dll_name[] = FILE_PATH_LITERAL("zipfldr.dll");
  ASSERT_EQ(NULL, ::GetModuleHandle(as_wcstr(dll_name)));

  ScopedNativeLibrary new_dll((FilePath(dll_name)));
  ASSERT_NE(static_cast<HMODULE>(NULL), new_dll.get());
  ASSERT_TRUE(GetLoadedModulesSnapshot(::GetCurrentProcess(), &snapshot));
  ASSERT_GT(snapshot.size(), original_snapshot_size);
  ASSERT_TRUE(Contains(snapshot, new_dll.get()));
}

TEST(BaseWinUtilTest, TestUint32ToInvalidHandle) {
  // Ensure that INVALID_HANDLE_VALUE is preserved when going to a 32-bit value
  // and back on 64-bit platforms.
  uint32_t invalid_handle = HandleToUint32(INVALID_HANDLE_VALUE);
  EXPECT_EQ(INVALID_HANDLE_VALUE, Uint32ToHandle(invalid_handle));
}

TEST(BaseWinUtilTest, String16FromGUID) {
  const GUID kGuid = {0x7698f759,
                      0xf5b0,
                      0x4328,
                      {0x92, 0x38, 0xbd, 0x70, 0x8a, 0x6d, 0xc9, 0x63}};
  const base::StringPiece16 kGuidStr(
      STRING16_LITERAL("{7698F759-F5B0-4328-9238-BD708A6DC963}"));
  auto guid_string16 = String16FromGUID(kGuid);
  EXPECT_EQ(guid_string16, kGuidStr);
  wchar_t guid_wchar[39];
  ::StringFromGUID2(kGuid, guid_wchar, base::size(guid_wchar));
  EXPECT_STREQ(as_wcstr(guid_string16), guid_wchar);
  ScopedCoMem<OLECHAR> clsid_string;
  ::StringFromCLSID(kGuid, &clsid_string);
  EXPECT_STREQ(as_wcstr(guid_string16), clsid_string.get());
}

TEST(BaseWinUtilTest, GetWindowObjectName) {
  base::string16 created_desktop_name(STRING16_LITERAL("test_desktop"));
  HDESK desktop_handle =
      ::CreateDesktop(created_desktop_name.c_str(), nullptr, nullptr, 0,
                      DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS |
                          READ_CONTROL | WRITE_DAC | WRITE_OWNER,
                      nullptr);

  ASSERT_NE(desktop_handle, nullptr);
  EXPECT_EQ(created_desktop_name, GetWindowObjectName(desktop_handle));
  ASSERT_TRUE(::CloseDesktop(desktop_handle));
}

TEST(BaseWinUtilTest, IsRunningUnderDesktopName) {
  HDESK thread_desktop = ::GetThreadDesktop(::GetCurrentThreadId());

  ASSERT_NE(thread_desktop, nullptr);
  base::string16 desktop_name = GetWindowObjectName(thread_desktop);

  EXPECT_TRUE(IsRunningUnderDesktopName(desktop_name));
  EXPECT_TRUE(IsRunningUnderDesktopName(base::ToLowerASCII(desktop_name)));
  EXPECT_TRUE(IsRunningUnderDesktopName(base::ToUpperASCII(desktop_name)));
  EXPECT_FALSE(IsRunningUnderDesktopName(
      desktop_name + STRING16_LITERAL("_non_existent_desktop_name")));
}

}  // namespace win
}  // namespace base
