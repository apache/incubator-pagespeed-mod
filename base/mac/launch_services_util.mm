// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "base/mac/launch_services_util.h"

#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"

namespace base {
namespace mac {

NSRunningApplication* OpenApplicationWithPath(
    const base::FilePath& bundle_path,
    const CommandLine& command_line,
    NSWorkspaceLaunchOptions launch_options) {
  NSString* bundle_url_spec = base::SysUTF8ToNSString(bundle_path.value());
  NSURL* bundle_url = [NSURL fileURLWithPath:bundle_url_spec isDirectory:YES];
  DCHECK(bundle_url);
  if (!bundle_url) {
    return nil;
  }

  // NSWorkspace automatically adds the binary path as the first argument and
  // it should not be included into the list.
  std::vector<std::string> argv = command_line.argv();
  int argc = argv.size();
  NSMutableArray* launch_args = [NSMutableArray arrayWithCapacity:argc - 1];
  for (int i = 1; i < argc; ++i) {
    [launch_args addObject:base::SysUTF8ToNSString(argv[i])];
  }

  NSDictionary* configuration = @{
    NSWorkspaceLaunchConfigurationArguments : launch_args,
  };
  NSError* launch_error = nil;
  // TODO(jeremya): this opens a new browser window if Chrome is already
  // running without any windows open.
  NSRunningApplication* app =
      [[NSWorkspace sharedWorkspace] launchApplicationAtURL:bundle_url
                                                    options:launch_options
                                              configuration:configuration
                                                      error:&launch_error];
  if (launch_error) {
    LOG(ERROR) << base::SysNSStringToUTF8([launch_error localizedDescription]);
    return nil;
  }
  DCHECK(app);
  return app;
}

}  // namespace mac
}  // namespace base
