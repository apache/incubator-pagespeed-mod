// Copyright 2016 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: jefftk@google.com (Jeff Kaufman)

#ifndef PAGESPEED_CONTROLLER_MANAGER_H_
#define PAGESPEED_CONTROLLER_MANAGER_H_

#include <sys/types.h>

#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/file_system.h"
#include "pagespeed/kernel/base/message_handler.h"
#include "pagespeed/kernel/base/scoped_ptr.h"
#include "pagespeed/kernel/base/string.h"
#include "pagespeed/kernel/base/thread.h"
#include "pagespeed/kernel/base/thread_system.h"
#include "pagespeed/kernel/base/timer.h"
#include "pagespeed/system/system_rewrite_driver_factory.h"


namespace net_instaweb {

// Handles forking off a controller process, restarting it if it dies, and
// shutting down the process if the host reloads config or shuts down.
//
// We fork a babysitter process, which forks a controller process.  If the
// controller process dies without calling exit() the babysitter will fork off
// another controller.
//
// The controller runs a thread that watches for the root process to die, or to
// ask it to quit.  We use pipes for communication between the master process
// and the controller.  If the master process goes away, the controller reading
// will get EOF.  If the master process wants the controller to shut down so it
// can be replaced, it writes a byte.
class ControllerManager {
 public:
  // Takes ownership of nothing.
  ControllerManager(SystemRewriteDriverFactory* factory,
                    ThreadSystem* thread_system,
                    MessageHandler* handler);
  virtual ~ControllerManager() {}

  // Called on system startup, before forking off any workers.  Starts up a
  // babysitter process that starts a controller process and restarts the
  // controller if it dies.  Also called (again) on configuration reloading.
  void ForkOffControllerProcess();

 private:
  // Controller will be hooked up here.  This method is called in a single
  // centralized "controller" process, and if that process dies it will be
  // started again.
  void RunController();

  // Remove inherited signal handlers in babysitter/controller, and replace them
  // with our cleanups as needed.
  void SetUpSignalHandlers();

  // Set us up as a proper daemon, with no stdin/out/err and not process group.
  void Daemonize();

  class ProcessDeathWatcherThread : public ThreadSystem::Thread {
   public:
    // Takes ownership of nothing.  Not that it matters, since we run until we
    // exit.
    ProcessDeathWatcherThread(ThreadSystem* thread_system,
                              int controller_read_fd,
                              MessageHandler* handler);

    virtual void Run();

   private:
    MessageHandler* handler_;
    int controller_read_fd_;

    DISALLOW_COPY_AND_ASSIGN(ProcessDeathWatcherThread);
  };

  SystemRewriteDriverFactory* factory_;
  ThreadSystem* thread_system_;
  MessageHandler* handler_;

  scoped_ptr<ProcessDeathWatcherThread> process_death_watcher_thread_;

  DISALLOW_COPY_AND_ASSIGN(ControllerManager);
};

}  // namespace net_instaweb

#endif  // PAGESPEED_CONTROLLER_MANAGER_H_
