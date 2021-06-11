/**
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

#include "SocketDescriptor.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif /* WIN32_LEAN_AND_MEAN */
#include <WinSock2.h>
#endif

class SocketDescriptorImpl {
#ifdef WIN32
  using Impl_t = SOCKET;
#else
  using Impl_t = int;
#endif  // WIN32

 public:
  SocketDescriptorImpl(Impl_t val) : value_(val) {}
  operator Impl_t() const {return value_;}

 private:
  Impl_t value_;
};


SocketDescriptorImpl& SocketDescriptor::value() {
  static_assert(sizeof(SocketDescriptor::raw_) >= sizeof(SocketDescriptorImpl), "Buffer is not large enough");
  static_assert(alignof(SocketDescriptor) >= alignof(SocketDescriptorImpl), "Alignment is not strict enough");
  return *reinterpret_cast<SocketDescriptorImpl*>(raw_);
}
const SocketDescriptorImpl& SocketDescriptor::value() const {
  return *reinterpret_cast<const SocketDescriptorImpl*>(raw_);
}

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org