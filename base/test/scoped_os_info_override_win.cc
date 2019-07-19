// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_os_info_override_win.h"

#include <windows.h>

#include "base/win/windows_version.h"

namespace base {
namespace test {

ScopedOSInfoOverride::ScopedOSInfoOverride(Type type)
    : original_info_(base::win::OSInfo::GetInstance()),
      overriding_info_(CreateInfoOfType(type)) {
  *base::win::OSInfo::GetInstanceStorage() = overriding_info_.get();
}

ScopedOSInfoOverride::~ScopedOSInfoOverride() {
  *base::win::OSInfo::GetInstanceStorage() = original_info_;
}

// static
ScopedOSInfoOverride::UniqueOsInfo ScopedOSInfoOverride::CreateInfoOfType(
    Type type) {
  _OSVERSIONINFOEXW version_info = {sizeof(version_info)};
  _SYSTEM_INFO system_info = {};
  int os_type = 0;

  switch (type) {
    case Type::kWin10Pro:
    case Type::kWin10Home:
      version_info.dwMajorVersion = 10;
      version_info.dwMinorVersion = 0;
      version_info.dwBuildNumber = 15063;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_WORKSTATION;
      version_info.wSuiteMask = VER_SUITE_PERSONAL;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 1;
      system_info.dwAllocationGranularity = 8;

      os_type =
          type == Type::kWin10Home ? PRODUCT_HOME_BASIC : PRODUCT_PROFESSIONAL;
      break;
    case Type::kWinServer2016:
      version_info.dwMajorVersion = 10;
      version_info.dwMinorVersion = 0;
      version_info.dwBuildNumber = 17134;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_SERVER;
      version_info.wSuiteMask = VER_SUITE_ENTERPRISE;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 4;
      system_info.dwAllocationGranularity = 64 * 1024;

      os_type = PRODUCT_STANDARD_SERVER;
      break;
    case Type::kWin81Pro:
      version_info.dwMajorVersion = 6;
      version_info.dwMinorVersion = 3;
      version_info.dwBuildNumber = 9600;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_WORKSTATION;
      version_info.wSuiteMask = VER_SUITE_PERSONAL;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 1;
      system_info.dwAllocationGranularity = 64 * 1024;

      os_type = PRODUCT_PROFESSIONAL;
      break;
    case Type::kWinServer2012R2:
      version_info.dwMajorVersion = 6;
      version_info.dwMinorVersion = 3;
      version_info.dwBuildNumber = 9600;
      version_info.wServicePackMajor = 0;
      version_info.wServicePackMinor = 0;
      version_info.szCSDVersion[0] = 0;
      version_info.wProductType = VER_NT_SERVER;
      version_info.wSuiteMask = VER_SUITE_ENTERPRISE;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 2;
      system_info.dwAllocationGranularity = 64 * 1024;

      os_type = PRODUCT_STANDARD_SERVER;
      break;
    case Type::kWin7ProSP1:
      version_info.dwMajorVersion = 6;
      version_info.dwMinorVersion = 1;
      version_info.dwBuildNumber = 7601;
      version_info.wServicePackMajor = 1;
      version_info.wServicePackMinor = 0;
      wcscpy_s(version_info.szCSDVersion, L"Service Pack 1");
      version_info.wProductType = VER_NT_WORKSTATION;
      version_info.wSuiteMask = VER_SUITE_PERSONAL;

      system_info.wProcessorArchitecture = PROCESSOR_ARCHITECTURE_AMD64;
      system_info.dwNumberOfProcessors = 1;
      system_info.dwAllocationGranularity = 64 * 1024;

      os_type = PRODUCT_PROFESSIONAL;
      break;
  }

  return UniqueOsInfo(new base::win::OSInfo(version_info, system_info, os_type),
                      &ScopedOSInfoOverride::deleter);
}

// static
void ScopedOSInfoOverride::deleter(base::win::OSInfo* info) {
  delete info;
}

}  // namespace test
}  // namespace base
