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

#include "RocksDatabase.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

rocksdb::Status WriteBatch::Put(const rocksdb::Slice &key, const rocksdb::Slice &value) {
  ResolvedKey resolved;
  rocksdb::Status result = db_->resolve(key, resolved);
  if (!result.ok()) {
    return result;
  }
  return impl_.Put(resolved.column, resolved.key, value);
}

rocksdb::Status WriteBatch::Delete(const rocksdb::Slice &key) {
  ResolvedKey resolved;
  rocksdb::Status result = db_->resolve(key, resolved);
  if (!result.ok()) {
    return result;
  }
  return impl_.Delete(resolved.column, resolved.key);
}

rocksdb::Status WriteBatch::Merge(const rocksdb::Slice &key, const rocksdb::Slice &value) {
  ResolvedKey resolved;
  rocksdb::Status result = db_->resolve(key, resolved);
  if (!result.ok()) {
    return result;
  }
  return impl_.Merge(resolved.column, resolved.key, value);
}

Iterator::Iterator(std::vector<std::string> columns, std::vector<std::unique_ptr<rocksdb::Iterator>> iterators)
  : columns_(std::move(columns)),
    iterators_(std::move(iterators)) {
  gsl_Expects(!iterators_.empty());
  iterators_[0]->SeekToFirst();
}

bool Iterator::Valid() const {
  if (error_) {
    return false;
  }
  return iterators_[column_idx_]->Valid();
}

void Iterator::Next() {
  iterators_[column_idx_]->Next();
  while (!iterators_[column_idx_]->Valid() && iterators_[column_idx_]->status().ok()) {
    // iterator terminated because of end-of-data
    if (column_idx_ < iterators_.size() - 1) {
      // we have more columns
      ++column_idx_;
      iterators_[column_idx_]->SeekToFirst();
    } else {
      // we have no more columns
      break;
    }
  }
}

rocksdb::Status Iterator::status() const {
  if (error_) {
    return error_.value();
  }
  return iterators_[column_idx_]->status();
}

std::string Iterator::key() const {
  std::string key = columns_[column_idx_];
  key += ":";
  key += iterators_[column_idx_]->key().ToString();
  return key;
}

rocksdb::Slice Iterator::value() const {
  return iterators_[column_idx_]->value();
}

OpenRocksDB::OpenRocksDB(RocksDatabase& db, gsl::not_null<utils::IntrusivePtr<DBHandle>> impl)
  : db_(&db), impl_(std::move(impl)) {}

rocksdb::Status OpenRocksDB::Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  ResolvedKey resolved;
  rocksdb::Status result = resolve(key, resolved);
  if (result.ok()) {
    result = impl_->impl->Put(options, resolved.column, resolved.key, value);
  }
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value) {
  ResolvedKey resolved;
  rocksdb::Status result = resolve(key, resolved);
  if (result.ok()) {
    result = impl_->impl->Get(options, resolved.column, resolved.key, value);
  }
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

std::vector<rocksdb::Status> OpenRocksDB::MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values) {
  values->resize(keys.size());
  std::vector<rocksdb::ColumnFamilyHandle*> columns;
  columns.reserve(keys.size());
  std::vector<rocksdb::Slice> resolved_keys;
  resolved_keys.reserve(keys.size());
  for (auto& key : keys) {
    ResolvedKey resolved;
    rocksdb::Status result = resolve(key, resolved);
    if (!result.ok()) {
      return std::vector<rocksdb::Status>(keys.size(), result);
    }
    resolved_keys.push_back(resolved.key);
    columns.push_back(resolved.column);
  }
  std::vector<rocksdb::Status> results = impl_->impl->MultiGet(options, columns, resolved_keys, values);
  for (const auto& result : results) {
    if (result == rocksdb::Status::NoSpace()) {
      db_->invalidate();
      break;
    }
  }
  return results;
}

rocksdb::Status OpenRocksDB::Write(const rocksdb::WriteOptions& options, WriteBatch* updates) {
  rocksdb::Status result = impl_->impl->Write(options, &updates->impl_);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key) {
  ResolvedKey resolved;
  rocksdb::Status result = resolve(key, resolved);
  if (result.ok()) {
    result = impl_->impl->Delete(options, resolved.column, resolved.key);
  }
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  ResolvedKey resolved;
  rocksdb::Status result = resolve(key, resolved);
  if (result.ok()) {
    result = impl_->impl->Merge(options, resolved.column, resolved.key, value);
  }
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

bool OpenRocksDB::GetProperty(const rocksdb::Slice& property, std::string* value) {
  // TODO: provide ColumnFamilyHandle
  return impl_->impl->GetProperty(property, value);
}

Iterator OpenRocksDB::NewIterator(const rocksdb::ReadOptions& options) {
  std::vector<rocksdb::Iterator*> iterator_ptrs;
  std::vector<std::string> column_names;
  std::vector<rocksdb::ColumnFamilyHandle*> handles;
  for (ColumnListNode* node = impl_->columns.head.load(); node != nullptr; node = node->next.get()) {
    column_names.emplace_back(node->column_name);
    handles.push_back(node->handle->impl.get());
  }
  rocksdb::Status status = impl_->impl->NewIterators(options, handles, &iterator_ptrs);
  if (!status.ok()) {
    RocksDatabase::logger_->log_error("Failed to create iterators");
    return Iterator{status};
  }
  std::vector<std::unique_ptr<rocksdb::Iterator>> iterators;
  iterators.reserve(iterator_ptrs.size());
  for (const auto& ptr : iterator_ptrs) {
    iterators.emplace_back(ptr);
  }
  return Iterator{std::move(column_names), std::move(iterators)};
}

rocksdb::Status OpenRocksDB::NewCheckpoint(rocksdb::Checkpoint **checkpoint) {
  return rocksdb::Checkpoint::Create(impl_->impl.get(), checkpoint);
}

rocksdb::Status OpenRocksDB::FlushWAL(bool sync) {
  rocksdb::Status result = impl_->impl->FlushWAL(sync);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDB::resolve(const rocksdb::Slice& full_key, ResolvedKey& resolved) {
  // for (size_t pos = 0; pos < full_key.size(); ++pos) {
  //   if (full_key[pos] == ':') {
  //     auto status = getOrCreateColumn(rocksdb::Slice{full_key.data(), pos}.ToString(), resolved.column);
  //     if (status.ok()) {
  //       resolved.key = rocksdb::Slice{full_key.data() + pos + 1, full_key.size() - pos - 1};
  //     }
  //     return status;
  //   }
  // }
  resolved.column = impl_->impl->DefaultColumnFamily();
  resolved.key = full_key;
  return rocksdb::Status::OK();
}

rocksdb::Status OpenRocksDB::getOrCreateColumn(const std::string &name, rocksdb::ColumnFamilyHandle*& handle) {
  rocksdb::Status status;
  ColumnListNode* column = impl_->columns.find(name);
  if (!column) {
    RocksDatabase::logger_->log_debug("Couldn't find column '%s' in the database, creating", name);
    status = db_->createColumnFamily(name);
    if (status.ok()) {
      column = impl_->columns.find(name);
      gsl_Expects(column);
    }
  }
  if (status.ok()) {
    handle = column->handle->impl.get();
  }
  return status;
}

WriteBatch OpenRocksDB::createWriteBatch() {
  return WriteBatch{gsl::make_not_null(this)};
}

std::shared_ptr<core::logging::Logger> RocksDatabase::logger_ = core::logging::LoggerFactory<RocksDatabase>::getLogger();

RocksDatabase::RocksDatabase(const rocksdb::Options& options, const std::string& name, Mode mode) : open_options_(options), db_name_(name), mode_(mode) {
  //open();
}

void RocksDatabase::invalidate() {
  // std::lock_guard<std::mutex> db_guard{ mtx_ };
  // discard our own instance
  impl_.store(utils::IntrusivePtr<DBHandle>(nullptr));
//  columns_.clear();
//  impl_.reset();
}

utils::optional<OpenRocksDB> RocksDatabase::open() {
  // double checked locking
  auto impl = impl_.load();
  if (!impl) {
    std::lock_guard<std::mutex> db_guard{ mtx_ };
    impl = impl_.load();
    if (!impl) {
      impl = utils::make_intrusive<DBHandle>();
      // database is not opened yet
      rocksdb::DB* db_instance = nullptr;
      rocksdb::Status result;
      std::vector<std::string> column_family_names;
      rocksdb::Status status = rocksdb::DB::ListColumnFamilies(open_options_, db_name_, &column_family_names);
      bool open_with_column_families = status.ok();
      std::vector<rocksdb::ColumnFamilyDescriptor> column_family_descriptors;
      for (const auto& column_name : column_family_names) {
        column_family_descriptors.emplace_back(rocksdb::ColumnFamilyDescriptor{column_name, open_options_});
      }
      std::vector<rocksdb::ColumnFamilyHandle*> column_handles;
      switch (mode_) {
        case Mode::ReadWrite:
          if (open_with_column_families) {
            result = rocksdb::DB::Open(open_options_, db_name_, column_family_descriptors, &column_handles, &db_instance);
          } else {
            result = rocksdb::DB::Open(open_options_, db_name_, &db_instance);
            if (result.ok()) {
              impl->columns.push_front(utils::make_unique<ColumnListNode>(
                  db_instance->DefaultColumnFamily()->GetName(),
                  utils::make_unique<DefaultColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(db_instance->DefaultColumnFamily())))
              );
            }
          }
          if (!result.ok()) {
            logger_->log_error("Cannot open writable rocksdb database %s, error: %s", db_name_, result.ToString());
          }
          break;
        case Mode::ReadOnly:
          if (open_with_column_families) {
            result = rocksdb::DB::OpenForReadOnly(open_options_, db_name_, column_family_descriptors, &column_handles, &db_instance);
          } else {
            result = rocksdb::DB::OpenForReadOnly(open_options_, db_name_, &db_instance);
            if (result.ok()) {
              impl->columns.push_front(utils::make_unique<ColumnListNode>(
                  db_instance->DefaultColumnFamily()->GetName(),
                  utils::make_unique<DefaultColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(db_instance->DefaultColumnFamily())))
              );
            }
          }
          if (!result.ok()) {
            logger_->log_error("Cannot open read-only rocksdb database %s, error: %s", db_name_, result.ToString());
          }
          break;
      }
      if (!result.ok()) {
        // we failed to open the database
        return utils::nullopt;
      }
      gsl_Expects(db_instance);
      impl->impl.reset(db_instance);
      for (auto& handle : column_handles) {
        impl->columns.push_front(utils::make_unique<ColumnListNode>(
            handle->GetName(),
            utils::make_unique<ColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(handle)))
        );
      }
      impl_.store(impl);
    }
  }
  return OpenRocksDB(*this, gsl::make_not_null<utils::IntrusivePtr<DBHandle>>(impl));
}

RocksDatabase::~RocksDatabase() {
  logger_->log_error("Closing database");
}

rocksdb::Status RocksDatabase::createColumnFamily(const std::string &name) {
  auto impl = impl_.load();
  if (!impl) {
    logger_->log_error("Cannot create a column: implementation is reset");
    return rocksdb::Status::NotFound("Implementation is reset");
  }
  std::lock_guard<std::mutex> db_guard{ impl->column_mtx };
  if (impl->columns.find(name)) {
    logger_->log_error("Column '%s' already exists in database '%s'", name, impl->impl->GetName());
    return rocksdb::Status::OK();
  }
  rocksdb::ColumnFamilyHandle* raw_handle{nullptr};
  auto status = impl->impl->CreateColumnFamily(open_options_, name, &raw_handle);
  if (!status.ok()) {
    logger_->log_error("Failed to create column '%s' in database '%s'", name, impl->impl->GetName());
    return status;
  }
  logger_->log_error("Successfully created column '%s' in database '%s'", name, impl->impl->GetName());
  auto handle = utils::make_unique<ColumnHandle>(std::unique_ptr<rocksdb::ColumnFamilyHandle>(raw_handle));
  auto column_list_node = utils::make_unique<ColumnListNode>(name, std::move(handle));
  // transfer ownership
  impl->columns.push_front(std::move(column_list_node));
  return rocksdb::Status::OK();
}

ColumnHandle::~ColumnHandle() {
  if (impl) {
    logging::LoggerFactory<ColumnHandle>::getLogger()->log_error("Closing column handle '%s'", impl->GetName());
  }
}

DBHandle::~DBHandle() {
  logging::LoggerFactory<DBHandle>::getLogger()->log_error("Closing database handle '%s'", impl->GetName());
}

DefaultColumnHandle::~DefaultColumnHandle() {
  // do not delete the default column handle
  logging::LoggerFactory<DefaultColumnHandle>::getLogger()->log_error("Releasing default column handle '%s'", impl->GetName());
  impl.release();
}

} /* namespace internal */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
