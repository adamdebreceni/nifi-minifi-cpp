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

#include "../TestBase.h"

struct PermutationsExhausted : std::runtime_error {
  using runtime_error::runtime_error;
};

/**
 * Globally order atomic operations across all threads for debugging.
 */
struct ThreadScheduler {
  struct Thread {
    enum class State {
      None,
      Waiting,
      Finished
    };
    std::thread::id thread_id;
    std::string name;
    mutable std::atomic<State> state{State::None};

    Thread(std::thread::id thread_id, std::string name) : thread_id(thread_id), name(std::move(name)) {}

    friend bool operator<(const Thread& lhs, const Thread& rhs) {
      return lhs.name < rhs.name;
    }
  };

  void registerThread(std::string name) {
    std::lock_guard<std::mutex> lock(mtx);
    logger_->log_debug("Registering thread \"%s\"", name);
    auto result = threads.emplace(std::this_thread::get_id(), std::move(name));
    gsl_Expects(result.second);
  }

  void reset(bool preserve_order = false) {
    std::lock_guard<std::mutex> lock(mtx);
    logger_->log_debug("Resetting thread scheduler");
    threads.clear();
    next_operation_idx = 0;
    if (!preserve_order) {
      previous_order.clear();
    }
  }

  static ThreadScheduler& get() {
    static ThreadScheduler instance;
    return instance;
  }

  template<typename Fn>
  auto exec(Fn&& fn, const std::string& file, int line) -> decltype((std::forward<Fn>(fn)())) {
    if (!running) {
      return std::forward<Fn>(fn)();
    }
    std::unique_lock<std::mutex> lock(mtx);
    enter(lock, file, line);
    decltype((std::forward<Fn>(fn)())) result = std::forward<Fn>(fn)();
    exit(lock);
    return result;
  }

  bool couldRunThreadAfter(std::unique_lock<std::mutex>& lock) {
    auto next_thread_it = std::next(find_this_thread());
    bool has_runnable_after = false;
    cv.wait(lock, [&] {
      bool all_finished = true;
      for (auto it = next_thread_it; it != threads.end(); ++it) {
        if (it->state == Thread::State::Waiting) {
          has_runnable_after = true;
          return true;
        } else if (it->state != Thread::State::Finished) {
          all_finished = false;
        }
      }
      return all_finished;
    });
    return has_runnable_after;
  }

  void enter(std::unique_lock<std::mutex>& lock, const std::string& file, int line) {
    auto this_thread_it = find_this_thread();
    this_thread_it->state = Thread::State::Waiting;
    cv.notify_all();
    cv.wait(lock, [&] {
      return find_next_thread() == this_thread_it;
    });
    this_thread_it->state = Thread::State::None;
    logger_->log_debug("Enter: %s at %s:%d", find_this_thread()->name, file, line);
    if (next_operation_idx < previous_order.size()) {
      if (previous_order[next_operation_idx].thread_name == this_thread_it->name) {
        // the same thread is scheduled as previously, no need to change anything
        assert(previous_order[next_operation_idx].could_have_run_next == couldRunThreadAfter(lock));
      } else {
        // a new thread is scheduled
        assert(previous_order[next_operation_idx].could_have_run_next);
        previous_order[next_operation_idx] = Exec{this_thread_it->name, couldRunThreadAfter(lock)};
        previous_order.erase(previous_order.begin() + next_operation_idx + 1, previous_order.end());
      }
    } else {
      previous_order.push_back(Exec{this_thread_it->name, couldRunThreadAfter(lock)});
    }
    ++next_operation_idx;
  }

  void exit(std::unique_lock<std::mutex>& /*lock*/) {
    auto this_thread_it = find_this_thread();
    logger_->log_debug("Exit: %s", this_thread_it->name);
    // notify waiting threads
    cv.notify_all();
  }

  void finish() {
    // the calling thread won't be doing any interesting operations anymore
    std::lock_guard<std::mutex> guard(mtx);
    auto this_thread_it = find_this_thread();
    gsl_Expects(this_thread_it->state != Thread::State::Finished);
    logger_->log_debug("Finish: %s", this_thread_it->name);
    this_thread_it->state = Thread::State::Finished;
    cv.notify_all();
  }

  std::set<Thread>::iterator find_this_thread() {
    auto id = std::this_thread::get_id();
    auto it = std::find_if(threads.begin(), threads.end(), [&] (const Thread& thread) {
      return thread.thread_id == id;
    });
    gsl_Expects(it != threads.end());
    return it;
  }

  std::string get_this_thread_name() {
    auto id = std::this_thread::get_id();
    auto it = std::find_if(threads.begin(), threads.end(), [&] (const Thread& thread) {
      return thread.thread_id == id;
    });
    if (it != threads.end()) {
      return it->name;
    }
    return "unknown";
  }

  std::set<Thread>::iterator find_next_thread() {
    auto next_thread_it = threads.begin();
    if (next_operation_idx < previous_order.size()) {
      auto prev_thread_it = std::find_if(threads.begin(), threads.end(), [&] (const Thread& thread) {
        return thread.name == previous_order[next_operation_idx].thread_name;
      });
      gsl_Expects(prev_thread_it != threads.end());
      if (hasNonFinalAfter()) {
        // not the last non-final exec, run as previously
        return prev_thread_it;
      }
      next_thread_it = std::next(prev_thread_it);
    }
    // skip finished threads
    while (next_thread_it != threads.end() && next_thread_it->state == Thread::State::Finished) {
      ++next_thread_it;
    }
    if (next_thread_it == threads.end()) {
      logger_->log_debug("Permutations exhausted");
      throw PermutationsExhausted("Permutations exhausted");
    }
    // success we have a viable next thread
    return next_thread_it;
  }

  bool hasNonFinalAfter() {
    if (next_operation_idx >= previous_order.size()) {
      return false;
    }
    return std::find_if(previous_order.begin() + next_operation_idx + 1, previous_order.end(), [&] (const Exec& e) {
      return e.could_have_run_next;
    }) != previous_order.end();
  }

  struct Exec {
    std::string thread_name;
    bool could_have_run_next;
  };

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<ThreadScheduler>::getLogger()};

  std::mutex mtx;
  std::condition_variable cv;
  std::set<Thread> threads;
  size_t next_operation_idx{0};
  std::vector<Exec> previous_order;
  std::atomic<bool> running{false};
};

#define DEBUG_ATOMIC_OPERATION(op) \
  (ThreadScheduler::get().exec([&] () -> decltype((op)) { \
    return op; \
  }, __FILE__, __LINE__))

#define THREAD_NAME \
  ThreadScheduler::get().get_this_thread_name()

//struct ObjectRegistryBase {
//  virtual void clear() = 0;
//};
//
//struct ObjectRegistryManager {
//  static ObjectRegistryManager& get() {
//    static ObjectRegistryManager manager;
//    return manager;
//  }
//
//  void registerRegistry(ObjectRegistryBase* reg) {
//    std::lock_guard<std::mutex> guard(mtx);
//    impls.push_back(reg);
//  }
//
//  void clear() {
//    std::lock_guard<std::mutex> guard(mtx);
//    clearing = true;
//    for (auto& impl : impls) {
//      impl->clear();
//    }
//    clearing = false;
//  }
//
//  std::mutex mtx;
//  std::vector<ObjectRegistryBase*> impls;
//  std::atomic_bool clearing{false};
//};
//
//template<typename T>
//class ObjectRegistry : ObjectRegistryBase {
//  enum class ObjectState {
//    Live,
//    Deleted
//  };
//
//  ObjectRegistry() {
//    auto& manager = ObjectRegistryManager::get();
//    manager.registerRegistry(this);
//  }
//
// public:
//  static ObjectRegistry& get() {
//    static ObjectRegistry registry;
//    return registry;
//  }
//
//  void createObject(const T* thiz) {
//    std::lock_guard<std::recursive_mutex> guard(mtx);
//    auto result = objects.emplace(thiz, ObjectState::Live);
//    assert(result.second);
//  }
//
//  void deleteObject(const T* thiz) {
//    if (thiz == nullptr) {
//      return;
//    }
//    std::lock_guard<std::recursive_mutex> guard(mtx);
//    auto it = objects.find(thiz);
//    assert(it != objects.end());
//    assert(it->second == ObjectState::Live);
//    it->second = ObjectState::Deleted;
//  }
//
//  bool isLive(const T* thiz) {
//    std::lock_guard<std::recursive_mutex> guard(mtx);
//    auto it = objects.find(thiz);
//    assert(it != objects.end());
//    return it->second == ObjectState::Live;
//  }
//
//  void clear() override {
//    std::lock_guard<std::recursive_mutex> guard(mtx);
//    for (const auto& obj : objects) {
//      assert(obj.second == ObjectState::Deleted);
//    }
//  }
//
//  std::recursive_mutex mtx;
//  std::map<const T*, ObjectState> objects;
//};
//
//#define DEBUG_ENTER_METHOD() \
//  assert(ObjectRegistry<typename std::remove_pointer<decltype(this)>::type>::get().isLive(this))
//
//#define DEBUG_ENTER_CONSTRUCTOR() \
//  ObjectRegistry<typename std::remove_pointer<decltype(this)>::type>::get().createObject(this)
//
//#define DEBUG_ENTER_DESTRUCTOR() \
//  ObjectRegistry<typename std::remove_pointer<decltype(this)>::type>::get().deleteObject(this)

#include "AtomicIntrusivePtr.h"

struct RefObj : utils::RefCountedObject {
  std::string name_;

  explicit RefObj(std::string name): name_(std::move(name)) {
    logging::LoggerFactory<RefObj>::getLogger()->log_debug("Constructing: %s at %p", name_, this);
  }

  ~RefObj() {
    logging::LoggerFactory<RefObj>::getLogger()->log_debug("Destructing: %s at %p", name_, this);
  }
};

struct ThreadPool {
  ThreadPool() {
    ThreadScheduler::get().reset(true);
  }

  void add(const std::string& name, const std::function<void()>& fn) {
    threads.emplace_back([=] {
      ThreadScheduler::get().registerThread(name);
      {
        std::lock_guard<std::mutex> guard(reg_mtx);
        ++registered_thread_count;
        reg_cv.notify_one();
      }
      {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] {return running;});
      }
      try {
        fn();
      } catch (const PermutationsExhausted&) {
        permutations_exhausted = true;
      }
      ThreadScheduler::get().finish();
    });
  }

  void start() {
    ThreadScheduler::get().running = true;
    {
      std::unique_lock<std::mutex> lock(reg_mtx);
      reg_cv.wait(lock, [&] {return registered_thread_count == threads.size();});
    }
    std::lock_guard<std::mutex> guard(mtx);
    logger_->log_debug("Starting thread pool");
    running = true;
    cv.notify_all();
  }

  std::string join() {
    for (auto& thread : threads) {
      thread.join();
    }
    std::string log_msg = "Permutation: [";
    bool first = true;
    size_t idx = 0;
    for (auto& exec : ThreadScheduler::get().previous_order) {
      if (!first) log_msg += ", ";
      first = false;
      log_msg += std::to_string(idx++) + ": ";
      log_msg += exec.thread_name + "(" + (exec.could_have_run_next ? "true" : "false") + ")";
    }
    log_msg += "]";
    ThreadScheduler::get().running = false;
    return log_msg;
  }

  std::list<std::thread> threads;
  std::mutex mtx;
  std::condition_variable cv;
  bool running{false};
  std::atomic<bool> permutations_exhausted{false};

  std::mutex reg_mtx;
  std::condition_variable reg_cv;
  unsigned registered_thread_count{0};

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<ThreadPool>::getLogger()};
};

struct AtomicTestController : TestController {
  AtomicTestController() {
    //LogTestController::getInstance().setTrace<RefObj>();
    LogTestController::getInstance().setError<AtomicTestController>();
    //LogTestController::getInstance().setTrace<ThreadScheduler>();
    LogTestController::getInstance().setError<ThreadPool>();
  }

  ~AtomicTestController() {
    //ObjectRegistryManager::get().clear();
  }

  std::shared_ptr<logging::Logger> logger_{logging::LoggerFactory<AtomicTestController>::getLogger()};
};

TEST_CASE_METHOD(AtomicTestController, "AtomicIntrusivePtr1") {
  utils::AtomicIntrusivePtr<RefObj> atomic_ptr;
  atomic_ptr.store(utils::make_intrusive<RefObj>("One"));
  {
    auto loaded = atomic_ptr.load();
    logger_->log_debug("Loaded: %s", loaded->name_);
  }
  atomic_ptr.store(utils::make_intrusive<RefObj>("Two"));
}

TEST_CASE_METHOD(AtomicTestController, "AtomicIntrusivePtr2") {
  utils::AtomicIntrusivePtr<RefObj> atomic_ptr;
  atomic_ptr.store(utils::make_intrusive<RefObj>("One"));
  atomic_ptr.store(utils::make_intrusive<RefObj>("Two"));
  {
    auto loaded = atomic_ptr.load();
    logger_->log_debug("Loaded: %s", loaded->name_);
  }
}

TEST_CASE_METHOD(AtomicTestController, "AtomicIntrusivePtr3") {
  auto start = std::chrono::steady_clock::now();
  size_t idx = 0;
  while (true) {
    ThreadPool pool;
    utils::AtomicIntrusivePtr<RefObj> atomic_ptr;
    pool.add("A", [&] {
      atomic_ptr.store(utils::make_intrusive<RefObj>("One"));
      atomic_ptr.store(utils::make_intrusive<RefObj>("Two"));
    });
    pool.add("B", [&] {
      auto loaded = atomic_ptr.load();
      if (!loaded) {
        logger_->log_debug("Loaded: nullptr");
      } else {
        logger_->log_debug("Loaded: %s", loaded->name_);
      }
    });
    pool.start();
    //pool.join();
    pool.logger_->log_error("%zu: %s", idx, pool.join());
    ++idx;
    if (pool.permutations_exhausted) {
      break;
    }
  }
  auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start);
  logger_->log_error("Running %zu permutations took %d ms", idx, gsl::narrow<int>(elapsed.count()));
}

