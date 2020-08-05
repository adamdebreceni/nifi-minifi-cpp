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

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * All streams serialize/deserialize in big-endian
 */
class Stream {
 public:
  virtual void close() {
    throw std::runtime_error("Close is not supported");
  }
  virtual void seek(uint64_t offset) {
    throw std::runtime_error("Seek is not supported");
  }

  virtual ~Stream() = default;
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
