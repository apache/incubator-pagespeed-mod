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

#pragma once

#include <stdexcept>
#include <string>

namespace net_instaweb {
/**
 * Base class for all Envoy exceptions.
 */
class EnvoyException : public std::runtime_error {
public:
  EnvoyException(const std::string& message) : std::runtime_error(message) {}
};

// TODO(oschaaf): restructure.
namespace Client {

/**
 * We translate certain exceptions thrown by TCLAP to NoServingException, for example when
 * help is invoked. This exception is then caught further up the stack and handled.
 */
class NoServingException : public EnvoyException {
public:
  NoServingException() : EnvoyException("NoServingException") {}
};

/**
 * Thrown when an OptionsImpl was not constructed because the argv was invalid.
 */
class MalformedArgvException : public EnvoyException {
public:
  MalformedArgvException(const std::string& what) : EnvoyException(what) {}
};

} // namespace Client

} // namespace net_instaweb
