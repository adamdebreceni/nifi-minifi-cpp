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

#include <mutex>

#include "utils/OptionalUtils.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/checkpoint.h"
#include "logging/Logger.h"
#include "utils/AtomicIntrusivePtr.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

struct ResolvedKey {
  rocksdb::ColumnFamilyHandle* column{nullptr};
  rocksdb::Slice key;
};

class RocksDatabase;

struct ColumnHandle {
  ColumnHandle(std::unique_ptr<rocksdb::ColumnFamilyHandle> impl): impl(std::move(impl)) {}
  virtual ~ColumnHandle();
  std::unique_ptr<rocksdb::ColumnFamilyHandle> impl;
};

struct DefaultColumnHandle : ColumnHandle {
  using ColumnHandle::ColumnHandle;
  ~DefaultColumnHandle() override;
};

struct ColumnListNode {
  std::string column_name;
  std::unique_ptr<ColumnHandle> handle;
  std::unique_ptr<ColumnListNode> next;

  ColumnListNode(std::string name, std::unique_ptr<ColumnHandle> handle)
    : column_name(std::move(name)), handle(std::move(handle)) {}
};

struct ColumnList {
  std::atomic<gsl::owner<ColumnListNode*>> head{nullptr};

  ~ColumnList() {
    delete head.load();
  }

  ColumnListNode* find(const std::string& name) const {
    ColumnListNode* current = head.load();
    while (current != nullptr) {
      if (current->column_name == name) {
        return current;
      }
      current = current->next.get();
    }
    return nullptr;
  }

  /**
   * !! WARNING !! NOT THREAD SAFE
   * @param new_head
   */
  void push_front(std::unique_ptr<ColumnListNode> new_head) {
    gsl_Expects(!new_head->next);
    new_head->next.reset(head.load());
    // take ownership
    head.store(new_head.release());
  }
};

using ColumnMap = std::map<std::string, ColumnHandle*>;

struct DBHandle : utils::RefCountedObject {
  DBHandle() = default;
  ~DBHandle();
  std::unique_ptr<rocksdb::DB> impl;
  std::mutex column_mtx;
  ColumnList columns;
};

class OpenRocksDB;

class WriteBatch {
  friend class OpenRocksDB;
  explicit WriteBatch(gsl::not_null<OpenRocksDB*> db): db_(std::move(db)) {}
 public:
  rocksdb::Status Put(const rocksdb::Slice& key, const rocksdb::Slice& value);
  rocksdb::Status Delete(const rocksdb::Slice& key);
  rocksdb::Status Merge(const rocksdb::Slice& key, const rocksdb::Slice& value);

 private:
  gsl::not_null<OpenRocksDB*> db_;
  rocksdb::WriteBatch impl_;
};

class Iterator {
 public:
  explicit Iterator(rocksdb::Status error) : error_(std::move(error)) {}
  Iterator(std::vector<std::string> columns, std::vector<std::unique_ptr<rocksdb::Iterator>> iterators);

  bool Valid() const;
  void Next();
  rocksdb::Status status() const;

  std::string key() const;
  rocksdb::Slice value() const;

 private:
  utils::optional<rocksdb::Status> error_;
  std::vector<std::string> columns_;
  std::vector<std::unique_ptr<rocksdb::Iterator>> iterators_;
  size_t column_idx_{0};
};

// Not thread safe
class OpenRocksDB {
  friend class RocksDatabase;
  friend class WriteBatch;

  OpenRocksDB(RocksDatabase& db, gsl::not_null<utils::IntrusivePtr<DBHandle>> impl);

 public:
  OpenRocksDB(const OpenRocksDB&) = delete;
  OpenRocksDB(OpenRocksDB&&) noexcept = default;
  OpenRocksDB& operator=(const OpenRocksDB&) = delete;
  OpenRocksDB& operator=(OpenRocksDB&&) = default;

  rocksdb::Status Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  rocksdb::Status Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value);

  std::vector<rocksdb::Status> MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values);

  rocksdb::Status Write(const rocksdb::WriteOptions& options, WriteBatch* updates);

  rocksdb::Status Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key);

  rocksdb::Status Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value);

  bool GetProperty(const rocksdb::Slice& property, std::string* value);

  Iterator NewIterator(const rocksdb::ReadOptions& options);

  rocksdb::Status NewCheckpoint(rocksdb::Checkpoint** checkpoint);

  rocksdb::Status FlushWAL(bool sync);

  WriteBatch createWriteBatch();

 private:
  rocksdb::Status resolve(const rocksdb::Slice& full_key, ResolvedKey& resolved);

  rocksdb::Status getOrCreateColumn(const std::string& name, rocksdb::ColumnFamilyHandle*& handle);

  gsl::not_null<RocksDatabase*> db_;
  gsl::not_null<utils::IntrusivePtr<DBHandle>> impl_;
};

class RocksDatabase {
  friend class OpenRocksDB;

 public:
  enum class Mode {
    ReadOnly,
    ReadWrite
  };

  RocksDatabase(const rocksdb::Options& options, const std::string& name, Mode mode = Mode::ReadWrite);

  utils::optional<OpenRocksDB> open();

  ~RocksDatabase();

 private:
  /*
   * notify RocksDatabase that the next open should check if they can reopen the database
   * until a successful reopen no more open is possible
   */
  void invalidate();

  rocksdb::Status createColumnFamily(const std::string& name);

  const rocksdb::Options open_options_;
  const std::string db_name_;
  const Mode mode_;

  std::mutex mtx_;
  utils::AtomicIntrusivePtr<DBHandle> impl_;

  static std::shared_ptr<core::logging::Logger> logger_;
};

} /* namespace internal */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
