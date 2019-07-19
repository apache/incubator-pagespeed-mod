// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/win/shortcut.h"

#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <propkey.h>
#include <wrl/client.h>

#include "base/files/file_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/win/scoped_propvariant.h"
#include "base/win/win_util.h"
#include "base/win/windows_version.h"

namespace base {
namespace win {

namespace {

using Microsoft::WRL::ComPtr;

// Initializes |i_shell_link| and |i_persist_file| (releasing them first if they
// are already initialized).
// If |shortcut| is not NULL, loads |shortcut| into |i_persist_file|.
// If any of the above steps fail, both |i_shell_link| and |i_persist_file| will
// be released.
void InitializeShortcutInterfaces(const wchar_t* shortcut,
                                  ComPtr<IShellLink>* i_shell_link,
                                  ComPtr<IPersistFile>* i_persist_file) {
  i_shell_link->Reset();
  i_persist_file->Reset();
  if (FAILED(::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(i_shell_link->GetAddressOf()))) ||
      FAILED(i_shell_link->CopyTo(i_persist_file->GetAddressOf())) ||
      (shortcut && FAILED((*i_persist_file)->Load(shortcut, STGM_READWRITE)))) {
    i_shell_link->Reset();
    i_persist_file->Reset();
  }
}

}  // namespace

ShortcutProperties::ShortcutProperties()
    : icon_index(-1), dual_mode(false), options(0U) {
}

ShortcutProperties::ShortcutProperties(const ShortcutProperties& other) =
    default;

ShortcutProperties::~ShortcutProperties() {
}

bool CreateOrUpdateShortcutLink(const FilePath& shortcut_path,
                                const ShortcutProperties& properties,
                                ShortcutOperation operation) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  // A target is required unless |operation| is SHORTCUT_UPDATE_EXISTING.
  if (operation != SHORTCUT_UPDATE_EXISTING &&
      !(properties.options & ShortcutProperties::PROPERTIES_TARGET)) {
    NOTREACHED();
    return false;
  }

  bool shortcut_existed = PathExists(shortcut_path);

  // Interfaces to the old shortcut when replacing an existing shortcut.
  ComPtr<IShellLink> old_i_shell_link;
  ComPtr<IPersistFile> old_i_persist_file;

  // Interfaces to the shortcut being created/updated.
  ComPtr<IShellLink> i_shell_link;
  ComPtr<IPersistFile> i_persist_file;
  switch (operation) {
    case SHORTCUT_CREATE_ALWAYS:
      InitializeShortcutInterfaces(NULL, &i_shell_link, &i_persist_file);
      break;
    case SHORTCUT_UPDATE_EXISTING:
      InitializeShortcutInterfaces(as_wcstr(shortcut_path.value()),
                                   &i_shell_link, &i_persist_file);
      break;
    case SHORTCUT_REPLACE_EXISTING:
      InitializeShortcutInterfaces(as_wcstr(shortcut_path.value()),
                                   &old_i_shell_link, &old_i_persist_file);
      // Confirm |shortcut_path| exists and is a shortcut by verifying
      // |old_i_persist_file| was successfully initialized in the call above. If
      // so, initialize the interfaces to begin writing a new shortcut (to
      // overwrite the current one if successful).
      if (old_i_persist_file.Get())
        InitializeShortcutInterfaces(NULL, &i_shell_link, &i_persist_file);
      break;
    default:
      NOTREACHED();
  }

  // Return false immediately upon failure to initialize shortcut interfaces.
  if (!i_persist_file.Get())
    return false;

  if ((properties.options & ShortcutProperties::PROPERTIES_TARGET) &&
      FAILED(i_shell_link->SetPath(as_wcstr(properties.target.value())))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_WORKING_DIR) &&
      FAILED(i_shell_link->SetWorkingDirectory(
          as_wcstr(properties.working_dir.value())))) {
    return false;
  }

  if (properties.options & ShortcutProperties::PROPERTIES_ARGUMENTS) {
    if (FAILED(i_shell_link->SetArguments(as_wcstr(properties.arguments))))
      return false;
  } else if (old_i_persist_file.Get()) {
    wchar_t current_arguments[MAX_PATH] = {0};
    if (SUCCEEDED(old_i_shell_link->GetArguments(current_arguments,
                                                 MAX_PATH))) {
      i_shell_link->SetArguments(current_arguments);
    }
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_DESCRIPTION) &&
      FAILED(i_shell_link->SetDescription(as_wcstr(properties.description)))) {
    return false;
  }

  if ((properties.options & ShortcutProperties::PROPERTIES_ICON) &&
      FAILED(i_shell_link->SetIconLocation(as_wcstr(properties.icon.value()),
                                           properties.icon_index))) {
    return false;
  }

  bool has_app_id =
      (properties.options & ShortcutProperties::PROPERTIES_APP_ID) != 0;
  bool has_dual_mode =
      (properties.options & ShortcutProperties::PROPERTIES_DUAL_MODE) != 0;
  bool has_toast_activator_clsid =
      (properties.options &
       ShortcutProperties::PROPERTIES_TOAST_ACTIVATOR_CLSID) != 0;
  if (has_app_id || has_dual_mode || has_toast_activator_clsid) {
    ComPtr<IPropertyStore> property_store;
    if (FAILED(i_shell_link.CopyTo(property_store.GetAddressOf())) ||
        !property_store.Get())
      return false;

    if (has_app_id && !SetAppIdForPropertyStore(property_store.Get(),
                                                properties.app_id.c_str())) {
      return false;
    }
    if (has_dual_mode &&
        !SetBooleanValueForPropertyStore(property_store.Get(),
                                         PKEY_AppUserModel_IsDualMode,
                                         properties.dual_mode)) {
      return false;
    }
    if (has_toast_activator_clsid &&
        !SetClsidForPropertyStore(property_store.Get(),
                                  PKEY_AppUserModel_ToastActivatorCLSID,
                                  properties.toast_activator_clsid)) {
      return false;
    }
  }

  // Release the interfaces to the old shortcut to make sure it doesn't prevent
  // overwriting it if needed.
  old_i_persist_file.Reset();
  old_i_shell_link.Reset();

  HRESULT result = i_persist_file->Save(as_wcstr(shortcut_path.value()), TRUE);

  // Release the interfaces in case the SHChangeNotify call below depends on
  // the operations above being fully completed.
  i_persist_file.Reset();
  i_shell_link.Reset();

  // If we successfully created/updated the icon, notify the shell that we have
  // done so.
  const bool succeeded = SUCCEEDED(result);
  if (succeeded) {
    if (shortcut_existed) {
      // TODO(gab): SHCNE_UPDATEITEM might be sufficient here; further testing
      // required.
      SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
    } else {
      SHChangeNotify(SHCNE_CREATE, SHCNF_PATH, shortcut_path.value().c_str(),
                     NULL);
    }
  }

  return succeeded;
}

bool ResolveShortcutProperties(const FilePath& shortcut_path,
                               uint32_t options,
                               ShortcutProperties* properties) {
  DCHECK(options && properties);
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  if (options & ~ShortcutProperties::PROPERTIES_ALL)
    NOTREACHED() << "Unhandled property is used.";

  ComPtr<IShellLink> i_shell_link;

  // Get pointer to the IShellLink interface.
  if (FAILED(::CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                IID_PPV_ARGS(&i_shell_link)))) {
    return false;
  }

  ComPtr<IPersistFile> persist;
  // Query IShellLink for the IPersistFile interface.
  if (FAILED(i_shell_link.CopyTo(persist.GetAddressOf())))
    return false;

  // Load the shell link.
  if (FAILED(persist->Load(as_wcstr(shortcut_path.value()), STGM_READ)))
    return false;

  // Reset |properties|.
  properties->options = 0;

  char16 temp[MAX_PATH];
  if (options & ShortcutProperties::PROPERTIES_TARGET) {
    if (FAILED(i_shell_link->GetPath(as_writable_wcstr(temp), MAX_PATH, NULL,
                                     SLGP_UNCPRIORITY))) {
      return false;
    }
    properties->set_target(FilePath(temp));
  }

  if (options & ShortcutProperties::PROPERTIES_WORKING_DIR) {
    if (FAILED(i_shell_link->GetWorkingDirectory(as_writable_wcstr(temp),
                                                 MAX_PATH)))
      return false;
    properties->set_working_dir(FilePath(temp));
  }

  if (options & ShortcutProperties::PROPERTIES_ARGUMENTS) {
    if (FAILED(i_shell_link->GetArguments(as_writable_wcstr(temp), MAX_PATH)))
      return false;
    properties->set_arguments(temp);
  }

  if (options & ShortcutProperties::PROPERTIES_DESCRIPTION) {
    // Note: description length constrained by MAX_PATH.
    if (FAILED(i_shell_link->GetDescription(as_writable_wcstr(temp), MAX_PATH)))
      return false;
    properties->set_description(temp);
  }

  if (options & ShortcutProperties::PROPERTIES_ICON) {
    int temp_index;
    if (FAILED(i_shell_link->GetIconLocation(as_writable_wcstr(temp), MAX_PATH,
                                             &temp_index))) {
      return false;
    }
    properties->set_icon(FilePath(temp), temp_index);
  }

  if (options & (ShortcutProperties::PROPERTIES_APP_ID |
                 ShortcutProperties::PROPERTIES_DUAL_MODE |
                 ShortcutProperties::PROPERTIES_TOAST_ACTIVATOR_CLSID)) {
    ComPtr<IPropertyStore> property_store;
    if (FAILED(i_shell_link.CopyTo(property_store.GetAddressOf())))
      return false;

    if (options & ShortcutProperties::PROPERTIES_APP_ID) {
      ScopedPropVariant pv_app_id;
      if (property_store->GetValue(PKEY_AppUserModel_ID,
                                   pv_app_id.Receive()) != S_OK) {
        return false;
      }
      switch (pv_app_id.get().vt) {
        case VT_EMPTY:
          properties->set_app_id(string16());
          break;
        case VT_LPWSTR:
          properties->set_app_id(WideToUTF16(pv_app_id.get().pwszVal));
          break;
        default:
          NOTREACHED() << "Unexpected variant type: " << pv_app_id.get().vt;
          return false;
      }
    }

    if (options & ShortcutProperties::PROPERTIES_DUAL_MODE) {
      ScopedPropVariant pv_dual_mode;
      if (property_store->GetValue(PKEY_AppUserModel_IsDualMode,
                                   pv_dual_mode.Receive()) != S_OK) {
        return false;
      }
      switch (pv_dual_mode.get().vt) {
        case VT_EMPTY:
          properties->set_dual_mode(false);
          break;
        case VT_BOOL:
          properties->set_dual_mode(pv_dual_mode.get().boolVal == VARIANT_TRUE);
          break;
        default:
          NOTREACHED() << "Unexpected variant type: " << pv_dual_mode.get().vt;
          return false;
      }
    }

    if (options & ShortcutProperties::PROPERTIES_TOAST_ACTIVATOR_CLSID) {
      ScopedPropVariant pv_toast_activator_clsid;
      if (property_store->GetValue(PKEY_AppUserModel_ToastActivatorCLSID,
                                   pv_toast_activator_clsid.Receive()) !=
          S_OK) {
        return false;
      }
      switch (pv_toast_activator_clsid.get().vt) {
        case VT_EMPTY:
          properties->set_toast_activator_clsid(CLSID_NULL);
          break;
        case VT_CLSID:
          properties->set_toast_activator_clsid(
              *(pv_toast_activator_clsid.get().puuid));
          break;
        default:
          NOTREACHED() << "Unexpected variant type: "
                       << pv_toast_activator_clsid.get().vt;
          return false;
      }
    }
  }

  return true;
}

bool ResolveShortcut(const FilePath& shortcut_path,
                     FilePath* target_path,
                     string16* args) {
  uint32_t options = 0;
  if (target_path)
    options |= ShortcutProperties::PROPERTIES_TARGET;
  if (args)
    options |= ShortcutProperties::PROPERTIES_ARGUMENTS;
  DCHECK(options);

  ShortcutProperties properties;
  if (!ResolveShortcutProperties(shortcut_path, options, &properties))
    return false;

  if (target_path)
    *target_path = properties.target;
  if (args)
    *args = properties.arguments;
  return true;
}

bool CanPinShortcutToTaskbar() {
  // "Pin to taskbar" stopped being supported in Windows 10.
  return GetVersion() < Version::WIN10;
}

bool PinShortcutToTaskbar(const FilePath& shortcut) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);
  DCHECK(CanPinShortcutToTaskbar());

  intptr_t result = reinterpret_cast<intptr_t>(ShellExecute(
      NULL, L"taskbarpin", as_wcstr(shortcut.value()), NULL, NULL, 0));
  return result > 32;
}

bool UnpinShortcutFromTaskbar(const FilePath& shortcut) {
  ScopedBlockingCall scoped_blocking_call(FROM_HERE, BlockingType::MAY_BLOCK);

  intptr_t result = reinterpret_cast<intptr_t>(ShellExecute(
      NULL, L"taskbarunpin", as_wcstr(shortcut.value()), NULL, NULL, 0));
  return result > 32;
}

}  // namespace win
}  // namespace base
