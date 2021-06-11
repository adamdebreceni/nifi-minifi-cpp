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

#include "io/internal/SocketDescriptor.h"
#include "io/internal/SocketDescriptorImpl.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

SocketDescriptor::SocketDescriptor(const SocketDescriptorImpl& val) {
  value() = val;
}

bool SocketDescriptor::isValid() const {
#ifdef WIN32
  return value() != INVALID_SOCKET && value() >= 0;
#else
  return value() >= 0;
#endif /* WIN32 */
}

SocketDescriptor SocketDescriptor::Invalid = [] {
#ifdef WIN32
  return SocketDescriptor(INVALID_SOCKET);
#else
  return SocketDescriptor(-1);
#endif
}();

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org