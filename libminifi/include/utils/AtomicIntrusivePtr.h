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
#include <thread>
#include "utils/GeneralUtils.h"
#include "IntrusivePtr.h"

#ifndef DEBUG_ATOMIC_OPERATION
#define DEBUG_ATOMIC_OPERATION(op) op
#endif

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

static constexpr bool use_lock = true;

// uses the lowest 16 bits to implement the reference counter
template<typename T>
class AtomicIntrusivePtr {
  static_assert(sizeof(uintptr_t) * CHAR_BIT >= 32, "Pointer is not big enough");

  struct UInt15 {
    uint16_t value{0};
    explicit UInt15(uint16_t value): value(value) {}
    explicit operator bool() const noexcept {
      return value != 0;
    }
    bool increment() {
      uint16_t new_value = value + 1;
      if (new_value < (1U << 15U)) {
        // all good, did not overflow
        value = new_value;
        return true;
      }
      // would have overflown
      return false;
    }
    void decrement() {
      gsl_Expects(value != 0);
      --value;
    }
  };
  struct Ptr {
    uintptr_t value{0};
    explicit operator bool() const noexcept {
      return value != 0;
    }
    void set(T* ptr) {
      value = reinterpret_cast<uintptr_t>(ptr) << 16U;
      gsl_Expects(get() == ptr);
    }
    T* get() const noexcept {
      return reinterpret_cast<T*>(value >> 16U);
    }
    UInt15 simpleCounter() const noexcept {
      // lowest 15 bits
      return UInt15{static_cast<uint16_t>(value & 0x7fffU)};
    }
    void setSimpleCounter(UInt15 counter) {
      value &= ~static_cast<uintptr_t>(0x7fffU);
      value |= counter.value;
    }
    // a simple pointer uses lowest 15 bits for counter
    bool isSimple() const noexcept {
      // 16th bit
      return (value & (1U << 15U)) == 0;
    }
  };
 public:
  AtomicIntrusivePtr() = default;

  ~AtomicIntrusivePtr() {
    store(IntrusivePtr<T>(nullptr));
  }

  void store(const IntrusivePtr<T>& ptr) {
    if (use_lock) {
      std::lock_guard<std::mutex> guard(mtx);
      instance = ptr;
      return;
    }
    IntrusivePtr<T> value = load();
    Ptr new_ptr;
    new_ptr.set(ptr.get());
    Ptr old_ptr = DEBUG_ATOMIC_OPERATION(ptr_.load());
    while (true) {
      if (old_ptr.get() != value.get()) {
        // somebody else got here before us, nothing to do
        return;
      }
      UInt15 counter = old_ptr.simpleCounter();
      // increment global
      if (value) {
        value->changeRefCount(counter.value);
      }
      // now we can swap the pointers
      if (DEBUG_ATOMIC_OPERATION(ptr_.compare_exchange_strong(old_ptr, new_ptr))) {
        // successfully swapped pointer, decrement global counter
        if (ptr) {
          ptr->ref();
        }
        if (value) {
          value->unref();
        }
        break;
      } else {
        // failed to swap pointers, either local counter changed or
        // the pointer itself or both
        if (value) {
          value->changeRefCount(-static_cast<int>(counter.value));
        }
      }
    }
  }

  IntrusivePtr<T> load() {
    if (use_lock) {
      std::lock_guard<std::mutex> guard(mtx);
      return instance;
    }
    // TODO: explicit memory order
    Ptr ptr = DEBUG_ATOMIC_OPERATION(ptr_.load());
    while (true) {
      if (!ptr) {
        return IntrusivePtr<T>(nullptr);
      }
      if (ptr.isSimple()) {
        UInt15 counter = ptr.simpleCounter();
        if (counter.increment()) {
          Ptr new_ptr = ptr;
          new_ptr.setSimpleCounter(counter);
          if (DEBUG_ATOMIC_OPERATION(!ptr_.compare_exchange_strong(ptr, new_ptr))) {
            // value changed under us
            continue;
          }
          // successfully incremented local counter
          IntrusivePtr<T> result(ptr.get());
          // copied pointer, incremented global counter, we should now decrement the local counter
          Ptr current = DEBUG_ATOMIC_OPERATION(ptr_.load());
          while (true) {
            if (current.isSimple()) {
              UInt15 curr_counter = current.simpleCounter();
              if (current.get() == ptr.get() && curr_counter) {
                curr_counter.decrement();
                Ptr new_current = current;
                new_current.setSimpleCounter(curr_counter);
                if (DEBUG_ATOMIC_OPERATION(ptr_.compare_exchange_strong(current, new_current))) {
                  break;
                }
              } else {
                // atomic pointer has been changed, decrement global counter instead
                result->unref();
                break;
              }
            } else {
              throw std::runtime_error("Unimplemented");
            }
          }

          return result;
        } else {
          std::this_thread::yield();
        }
      } else {
        throw std::runtime_error("Unimplemented");
      }
    }
  }

 private:
  std::atomic<Ptr> ptr_{};

  std::mutex mtx;
  utils::IntrusivePtr<T> instance;
};

}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
