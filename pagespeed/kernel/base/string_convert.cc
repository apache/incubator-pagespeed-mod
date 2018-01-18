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


#include "pagespeed/kernel/base/basictypes.h"
#include "pagespeed/kernel/base/string_util.h"

namespace net_instaweb {

bool AccumulateDecimalValue(char c, uint32* value) {
  bool ret = true;
  if ((c >= '0') && (c <= '9')) {
    *value *= 10;
    *value += c - '0';
  } else {
    ret = false;
  }
  return ret;
}

bool AccumulateHexValue(char c, uint32* value) {
  int digit = 0;
  if ((c >= '0') && (c <= '9')) {
    digit = c - '0';
  } else if ((c >= 'a') && (c <= 'f')) {
    digit = 10 + c - 'a';
  } else if ((c >= 'A') && (c <= 'F')) {
    digit = 10 + c - 'A';
  } else {
    return false;
  }
  *value = *value * 16 + digit;
  return true;
}

}  // namespace net_instaweb
