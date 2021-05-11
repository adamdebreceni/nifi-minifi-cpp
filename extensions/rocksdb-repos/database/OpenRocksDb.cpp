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

#include "OpenRocksDb.h"
#include "ColumnHandle.h"
#include "RocksDbInstance.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace internal {

OpenRocksDb::OpenRocksDb(RocksDbInstance& db, gsl::not_null<std::shared_ptr<rocksdb::DB>> impl, gsl::not_null<std::shared_ptr<ColumnHandle>> column)
    : db_(&db), impl_(std::move(impl)), column_(std::move(column)) {}

rocksdb::Status OpenRocksDb::Put(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Put(options, column_->handle.get(), key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDb::Get(const rocksdb::ReadOptions& options, const rocksdb::Slice& key, std::string* value) {
  rocksdb::Status result = impl_->Get(options, column_->handle.get(), key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

std::vector<rocksdb::Status> OpenRocksDb::MultiGet(const rocksdb::ReadOptions& options, const std::vector<rocksdb::Slice>& keys, std::vector<std::string>* values) {
  std::vector<rocksdb::Status> results = impl_->MultiGet(
      options, std::vector<rocksdb::ColumnFamilyHandle*>(keys.size(), column_->handle.get()), keys, values);
  for (const auto& result : results) {
    if (result == rocksdb::Status::NoSpace()) {
      db_->invalidate();
      break;
    }
  }
  return results;
}

rocksdb::Status OpenRocksDb::Write(const rocksdb::WriteOptions& options, internal::WriteBatch* updates) {
  rocksdb::Status result = impl_->Write(options, &updates->impl_);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDb::Delete(const rocksdb::WriteOptions& options, const rocksdb::Slice& key) {
  rocksdb::Status result = impl_->Delete(options, column_->handle.get(), key);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

rocksdb::Status OpenRocksDb::Merge(const rocksdb::WriteOptions& options, const rocksdb::Slice& key, const rocksdb::Slice& value) {
  rocksdb::Status result = impl_->Merge(options, column_->handle.get(), key, value);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

bool OpenRocksDb::GetProperty(const rocksdb::Slice& property, std::string* value) {
  return impl_->GetProperty(column_->handle.get(), property, value);
}

std::unique_ptr<rocksdb::Iterator> OpenRocksDb::NewIterator(const rocksdb::ReadOptions& options) {
  return std::unique_ptr<rocksdb::Iterator>{impl_->NewIterator(options, column_->handle.get())};
}

rocksdb::Status OpenRocksDb::NewCheckpoint(rocksdb::Checkpoint **checkpoint) {
  return rocksdb::Checkpoint::Create(impl_.get(), checkpoint);
}

rocksdb::Status OpenRocksDb::FlushWAL(bool sync) {
  rocksdb::Status result = impl_->FlushWAL(sync);
  if (result == rocksdb::Status::NoSpace()) {
    db_->invalidate();
  }
  return result;
}

WriteBatch OpenRocksDb::createWriteBatch() const noexcept {
  return WriteBatch(column_->handle.get());
}

rocksdb::DB* OpenRocksDb::get() {
  return impl_.get();
}

}  // namespace internal
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
