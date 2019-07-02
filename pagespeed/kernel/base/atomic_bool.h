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


#ifndef PAGESPEED_KERNEL_BASE_ATOMIC_BOOL_H_
#define PAGESPEED_KERNEL_BASE_ATOMIC_BOOL_H_

#include "pagespeed/kernel/base/atomicops.h"
#include "pagespeed/kernel/base/basictypes.h"

namespace net_instaweb {

// A boolean flag that can be set atomically and be visible to other
// threads. Please be extra careful with this --- it can go wrong in
// incomprehensible  ways; most of the time, you probably want to use a mutex
// instead.
class AtomicBool {
 public:
  // Guaranteed to be initialized to false.
  AtomicBool() {
    set_value(false);
  }

  ~AtomicBool() {}

  bool value() const {
    return base::subtle::Acquire_Load(&value_);
  }

  void set_value(bool v) {
    base::subtle::Release_Store(&value_, v);
  }

 private:
  base::subtle::AtomicWord value_;
  DISALLOW_COPY_AND_ASSIGN(AtomicBool);
};


}  // namespace net_instaweb

#endif  // PAGESPEED_KERNEL_BASE_ATOMIC_BOOL_H_
