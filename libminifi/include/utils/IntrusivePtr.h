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

#include <cstdint>
#include <atomic>
#include "utils/GeneralUtils.h"

#ifndef DEBUG_ATOMIC_OPERATION
#define DEBUG_ATOMIC_OPERATION(op) op
#endif

#ifndef DEBUG_ENTER_METHOD
#define DEBUG_ENTER_METHOD() (void(0))
#endif

#ifndef DEBUG_ENTER_DESTRUCTOR
#define DEBUG_ENTER_DESTRUCTOR() (void(0))
#endif

#ifndef DEBUG_ENTER_CONSTRUCTOR
#define DEBUG_ENTER_CONSTRUCTOR() (void(0))
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

template<typename T>
class AtomicIntrusivePtr;

template<typename T>
class IntrusivePtr;

class RefCountedObject {
  template<typename T>
  friend class AtomicIntrusivePtr;

 public:
  RefCountedObject() {
    DEBUG_ENTER_CONSTRUCTOR();
  }

  void ref() {
    DEBUG_ENTER_METHOD();
    DEBUG_ATOMIC_OPERATION(++ref_count_);
  }

  bool unref() {
    DEBUG_ENTER_METHOD();
    return DEBUG_ATOMIC_OPERATION(--ref_count_) == 0;
  }

  ~RefCountedObject() {
    DEBUG_ENTER_DESTRUCTOR();
  }

 private:
  void changeRefCount(int diff) {
    DEBUG_ENTER_METHOD();
    if (diff == 0) return;
    DEBUG_ATOMIC_OPERATION(ref_count_ += diff);
  }

  std::atomic<int> ref_count_{0};
};

template<typename T>
class AtomicIntrusivePtr;

template<typename T>
class IntrusivePtr {
  static_assert(std::is_base_of<RefCountedObject, T>::value, "Can only be used with reference counted objects");

  template<typename>
  friend class AtomicIntrusivePtr;

 public:
  IntrusivePtr() = default;
  explicit IntrusivePtr(T* value) {
    reset(value);
  }
  IntrusivePtr(const IntrusivePtr& other) {
    reset(other.value_);
  }
  IntrusivePtr(IntrusivePtr&& other) noexcept {
    T* value = exchange(other.value_, nullptr);
    resetImpl(value, false);
  }
  IntrusivePtr& operator=(IntrusivePtr&& other) noexcept(noexcept(std::declval<T>().~T())) {
    if (this == &other) {
      return *this;
    }
    T* value = exchange(other.value_, nullptr);
    resetImpl(value, false);
    return *this;
  }
  IntrusivePtr& operator=(const IntrusivePtr& other) {
    if (this == &other) {
      return *this;
    }
    reset(other.value_);
    return *this;
  }
  IntrusivePtr& operator=(nullptr_t) {
    reset(nullptr);
  }
  void reset(T* value = nullptr) {
    resetImpl(value, true);
  }
  explicit operator bool() const noexcept {
    return value_;
  }
  bool operator!=(nullptr_t) const noexcept {
    return value_ != nullptr;
  }
  T& operator*() const noexcept {
    return *value_;
  }
  T* get() const {
    return value_;
  }
  T* operator->() const {
    return value_;
  }
  ~IntrusivePtr() {
    reset();
  }

 private:
  void resetImpl(T* value, bool increment_new_counter) {
    if (increment_new_counter && value) {
      value->RefCountedObject::ref();
    }
    if (value_ && value_->RefCountedObject::unref()) {
      // nobody else references this object
      delete value_;
    }
    value_ = value;
  }

  T* value_{nullptr};
};

template<typename T, typename ...Args>
IntrusivePtr<T> make_intrusive(Args&& ...args) {
  return IntrusivePtr<T>(new T(std::forward<Args>(args)...));
}

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
