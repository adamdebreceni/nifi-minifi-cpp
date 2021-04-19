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

#include <random>

#include "../../extensions/rocksdb-repos/RocksDbStream.h"
#include "../../extensions/rocksdb-repos/DatabaseContentRepository.h"
#include "utils/MinifiConcurrentQueue.h"
#include "utils/file/FileUtils.h"
#include "../Utils.h"
#include "utils/gsl.h"
#include "utils/Id.h"

#undef NDEBUG

using namespace org::apache::nifi::minifi;

std::mt19937 getGenerator() {
  static std::mt19937 gen = [] () -> std::mt19937 {
    std::random_device rd;
    return std::mt19937(rd());
  }();
  return gen;
}

static constexpr size_t data_size = 10000;

std::string getContent() {
  std::mt19937 gen = getGenerator();
  std::uniform_int_distribution<unsigned> dis((unsigned)'a', (unsigned)'z');
  std::string content;
  content.reserve(data_size);
  for (size_t i = 0; i < data_size; ++i) {
    content += (char)dis(gen);
  }
  return content;
}

static constexpr size_t writer_count = 4;
static constexpr size_t reader_count = 4;

utils::Identifier nextId() {
  static utils::Identifier current;
  static std::mutex mtx;
  std::lock_guard<std::mutex> guard(mtx);
  current = current.next();
  return current;
}

struct Runner {
  Runner(internal::RocksDatabase* db, std::string prefix): db(db), prefix(std::move(prefix)) {
    for (size_t writer_idx{0}; writer_idx < writer_count; ++writer_idx) {
      writers.emplace_back(&Runner::write, this);
    }
    for (size_t reader_idx{0}; reader_idx < reader_count; ++reader_idx) {
      readers.emplace_back(&Runner::read, this);
    }
  }

  void read() {
    while (running) {
      std::string path;
      if (queue.dequeueWait(path)) {
        io::RocksDbStream stream(path, minifi::gsl::make_not_null(db), false);
        std::string data;
        stream.read(data);
        assert(data.length() == data_size);
        ++read_count;
      }
    }
  }

  void write() {
    while (running) {
      auto opendb = db->open();
      assert(opendb);
      auto batch = opendb->createWriteBatch();
      std::vector<std::string> ids;
      for (size_t idx = 0; idx < 100; ++idx) {
        std::string path = prefix + nextId().to_string();
        //logging::LoggerFactory<Runner>::getLogger()->log_error("Id = %s", path);
        io::RocksDbStream stream(path, minifi::gsl::make_not_null(db), true, &batch);
        static std::string data = getContent();
        stream.write(data);
        ids.emplace_back(std::move(path));
      }
      auto status = opendb->Write(rocksdb::WriteOptions{}, &batch);
      assert(status.ok());
      for (auto&& id : ids) {
        queue.enqueue(std::move(id));
      }
      write_count += ids.size();
    }
  }

  void signalStop() {
    queue.stop();
    running = false;
  }

  void join() {
    for (auto& writer : writers) {
      writer.join();
    }
    for (auto& reader : readers) {
      reader.join();
    }

    auto logger = logging::LoggerFactory<Runner>::getLogger();
    logger->log_error("Runner '%s': write = %d, read = %d", prefix, write_count.load(), read_count.load());
  }

  std::atomic_bool running{true};
  std::list<std::thread> writers;
  std::list<std::thread> readers;

  utils::ConditionConcurrentQueue<std::string> queue;

  minifi::internal::RocksDatabase* db;
  std::atomic<int> read_count{0};
  std::atomic<int> write_count{0};
  std::string prefix;
};

struct RunnerPool {
  RunnerPool(internal::RocksDatabase* db, size_t count, bool use_columns) {
    for (size_t idx = 0; idx < count; ++idx) {
      std::string prefix;
      if (use_columns) {
        prefix = std::string{"Runner_"} + std::to_string(idx) + ":";
      }
      runners.emplace_back(db, prefix);
    }
  }

  void stop() {
    for (auto& runner : runners) {
      runner.signalStop();
    }
    for (auto& runner : runners) {
      runner.join();
    }
  }

  std::list<Runner> runners;
};

void run(bool use_columns) {
  char format[] = "/var/tmp/testdb.XXXXXX";
  std::string dir = utils::file::FileUtils::create_temp_directory(format);
  rocksdb::Options options;
  options.create_if_missing = true;
  options.use_direct_io_for_flush_and_compaction = true;
  options.use_direct_reads = true;
  options.merge_operator = std::make_shared<core::repository::StringAppender>();
  options.error_if_exists = false;
  options.max_successive_merges = 0;
  auto db = utils::make_unique<internal::RocksDatabase>(options, dir);

  RunnerPool pool(db.get(), 2, use_columns);

  std::this_thread::sleep_for(std::chrono::seconds{5});

  pool.stop();
}

void run_test(size_t thread_count) {
  std::atomic_bool running(true);
  std::mutex mtx;
  size_t counter = 0;
  std::list<std::thread> threads;
  for (size_t idx = 0; idx < thread_count; ++idx) {
    threads.emplace_back([&] {
      while (running) {
        std::lock_guard<std::mutex> guard(mtx);
        ++counter;
      }
    });
  }

  std::this_thread::sleep_for(std::chrono::seconds{2});
  running = false;
  for (auto& thread : threads) {
    thread.join();
  }
  auto logger = logging::LoggerFactory<Runner>::getLogger();
  logger->log_error("Thread count: %zu, Counter: %zu", thread_count, counter);
}

int main() {
  run(true);
  run(false);
  run(true);
  run(false);
//  run_test(1);
//  run_test(2);
//  run_test(4);
//  run_test(8);
}

