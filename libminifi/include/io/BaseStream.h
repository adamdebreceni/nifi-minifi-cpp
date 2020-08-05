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

#include <string>
#include <vector>
#include <iostream>
#include <cstdint>
#include "core/expect.h"
#include "InputStream.h"
#include "OutputStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

/**
 * Base Stream is the base of a composable stream architecture.
 * Intended to be the base of layered streams ala DatInputStreams in Java.
 *
 * ** Not intended to be thread safe as it is not intended to be shared**
 *
 * Extensions may be thread safe and thus shareable, but that is up to the implementation.
 */
class BaseStream : public InputStream, public OutputStream {
 public:
  virtual void close() {
    throw std::runtime_error("Close is not supported");
  }
  virtual void seek(uint64_t offset) {
    throw std::runtime_error("Seek is not supported");
  }

  virtual int initialize() {
    return 1;
  }

  virtual const uint8_t* getBuffer() const {
    throw std::runtime_error("Not a buffered stream");
  }
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
