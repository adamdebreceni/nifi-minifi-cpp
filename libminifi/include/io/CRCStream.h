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
#ifndef LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_
#define LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_

#include <zlib.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#ifdef WIN32
#include <winsock2.h>

#else
#include <arpa/inet.h>

#endif
#include "BaseStream.h"
#include "Exception.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {
namespace internal {

template<typename StreamType>
class CRCStreamBase : public virtual Stream {
 public:
  StreamType *getstream() const {
    return child_stream_;
  }

  void close() override { child_stream_->close(); }

  int initialize() override {
    child_stream_->initialize();
    reset();
    return 0;
  }

  void updateCRC(uint8_t *buffer, uint32_t length) {
    crc_ = crc32(crc_, buffer, length);
  }

  uint64_t getCRC() {
    return crc_;
  }

  void reset() {
    crc_ = crc32(0L, Z_NULL, 0);
  }

 protected:
  uint64_t crc_ = 0;
  StreamType *child_stream_ = nullptr;
};

template<typename StreamType>
class InputCRCStream : public virtual CRCStreamBase<StreamType>, public InputStream {
 public:
  using InputStream::read;
  using CRCStreamBase<StreamType>::child_stream_;
  using CRCStreamBase<StreamType>::crc_;

  int read(uint8_t *buf, int buflen) override {
    int ret = child_stream_->read(buf, buflen);
    if (ret > 0) {
      crc_ = crc32(crc_, buf, ret);
    }
    return ret;
  }

  size_t size() const override { return child_stream_->size(); }
};

template<typename StreamType>
class OutputCRCStream : public virtual CRCStreamBase<StreamType>, public OutputStream {
 public:
  using OutputStream::write;
  using CRCStreamBase<StreamType>::child_stream_;
  using CRCStreamBase<StreamType>::crc_;

  int write(const uint8_t *value, int size) override {
    int ret = child_stream_->write(value, size);
    if (ret > 0) {
      crc_ = crc32(crc_, value, ret);
    }
    return ret;
  }
};

struct empty_class {};

}  // namespace internal

template<typename StreamType>
class CRCStream : public std::conditional<std::is_base_of<InputStream, StreamType>::value, internal::InputCRCStream<StreamType>, internal::empty_class>::type
                  , public std::conditional<std::is_base_of<OutputStream, StreamType>::value, internal::OutputCRCStream<StreamType>, internal::empty_class>::type {
  using internal::CRCStreamBase<StreamType>::child_stream_;
  using internal::CRCStreamBase<StreamType>::crc_;

 public:
  explicit CRCStream(StreamType *child_stream) {
    child_stream_ = child_stream;
    crc_ = crc32(0L, Z_NULL, 0);
  }

  CRCStream(StreamType *child_stream, uint64_t initial_crc) {
    crc_ = initial_crc;
    child_stream_ = child_stream;
  }

  CRCStream(CRCStream &&move) noexcept {
    crc_ = std::move(move.crc_);
    child_stream_ = std::move(move.child_stream_);
  }
};


}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_IO_CRCSTREAM_H_
