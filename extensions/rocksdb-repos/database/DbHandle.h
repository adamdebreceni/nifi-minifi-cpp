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

#include <memory>
#include <utility>
#include <vector>
#include "rocksdb/db.h"
#include "RocksDbUtils.h"

namespace org::apache::nifi::minifi::internal {

struct DbHandle {
  explicit DbHandle(std::unique_ptr<rocksdb::DB> handle, std::vector<DBOptionsPatch> dbo_patches)
      : dbo_patches(dbo_patches), handle(std::move(handle)) {}
  ~DbHandle();
  // we need to keep the patch object alive as the DB handle could
  // reference patcher-owned resources
  std::vector<DBOptionsPatch> dbo_patches;
  std::unique_ptr<rocksdb::DB> handle;
};

}  // namespace org::apache::nifi::minifi::internal
