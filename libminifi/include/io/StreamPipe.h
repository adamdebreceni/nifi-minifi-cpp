/**
 * @file FlowFileRecord.h
 * Flow file record class declaration
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

#include <memory>
#include "BaseStream.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// FlowFile IO Callback functions for input and output
// throw exception for error
class InputStreamCallback {
 public:
  virtual ~InputStreamCallback() = default;
  // virtual void process(std::ifstream *stream) = 0;

  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) = 0;
};
class OutputStreamCallback {
 public:
  virtual ~OutputStreamCallback() = default;
  virtual int64_t process(std::shared_ptr<io::BaseStream> stream) = 0;
};

namespace internal {

inline int64_t pipe(const std::shared_ptr<io::BaseStream>& src, const std::shared_ptr<io::BaseStream>& dst) {
  uint8_t buffer[4096U];
  int64_t totalTransferred = 0;
  while (true) {
    int readRet = src->read(buffer, sizeof(buffer));
    if (readRet < 0) {
      return readRet;
    }
    if (readRet == 0) {
      break;
    }
    int remaining = readRet;
    int transferred = 0;
    while (remaining > 0) {
      int writeRet = dst->write(buffer + transferred, remaining);
      // TODO(adebreceni):
      //   write might return 0, e.g. in case of a congested server
      //   what should we return then?
      //     - the number of bytes read or
      //     - the number of bytes wrote
      if (writeRet < 0) {
        return writeRet;
      }
      transferred += writeRet;
      remaining -= writeRet;
    }
    totalTransferred += transferred;
  }
  return totalTransferred;
}

}  // namespace internal

class InputStreamPipe : public InputStreamCallback {
 public:
  explicit InputStreamPipe(std::shared_ptr<io::BaseStream> output) : output_(std::move(output)) {}

  int64_t process(std::shared_ptr<io::BaseStream> stream) override {
    return internal::pipe(stream, output_);
  }

 private:
  std::shared_ptr<io::BaseStream> output_;
};

class OutputStreamPipe : public OutputStreamCallback {
 public:
  explicit OutputStreamPipe(std::shared_ptr<io::BaseStream> input) : input_(std::move(input)) {}

  int64_t process(std::shared_ptr<io::BaseStream> stream) override {
    return internal::pipe(input_, stream);
  }

 private:
  std::shared_ptr<io::BaseStream> input_;
};



}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
