/**
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

#include <array>
#include <ostream>
#include <string>
#include <utility>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<size_t N>
class FixedLengthString : public std::array<char, N + 1> {
 public:
  operator std::string() const {  // NOLINT
    return {this->data()};
  }

  friend std::ostream &operator<<(std::ostream &out, const FixedLengthString &str) {
    return out << str.data();
  }

  friend std::string operator+(const std::string &lhs, const FixedLengthString &rhs) {
    return lhs + rhs.data();
  }

  friend std::string operator+(std::string &&lhs, const FixedLengthString &rhs) {
    return std::move(lhs) + rhs.data();
  }

  friend std::string operator+(const FixedLengthString &lhs, const std::string &rhs) {
    return lhs.data() + rhs;
  }

  friend std::string operator+(const FixedLengthString &lhs, std::string &&rhs) {
    return lhs.data() + std::move(rhs);
  }

  friend bool operator==(const std::string& lhs, const FixedLengthString& rhs) {
    return lhs == rhs.data();
  }

  friend bool operator==(const FixedLengthString& lhs, const std::string& rhs) {
    return lhs.data() == rhs;
  }

  friend bool operator==(const FixedLengthString& lhs, const FixedLengthString& rhs) {
    return static_cast<std::array<char, N + 1>>(lhs) == static_cast<std::array<char, N + 1>>(rhs);
  }

  friend bool operator!=(const std::string& lhs, const FixedLengthString& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator!=(const FixedLengthString& lhs, const std::string& rhs) {
    return !(lhs == rhs);
  }

  friend bool operator!=(const FixedLengthString& lhs, const FixedLengthString& rhs) {
    return !(lhs == rhs);
  }
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org