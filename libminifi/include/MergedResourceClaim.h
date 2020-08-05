/**
 * @file ResourceClaim.h
 * Resource Claim class declaration
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

#include <string>
#include <vector>
#include <queue>
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include "core/Core.h"
#include "core/StreamManager.h"
#include "properties/Configure.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// ResourceClaim Class
class MergedResourceClaim : public ResourceClaim {
 public:
  MergedResourceClaim(const std::shared_ptr<core::StreamManager<ResourceClaim>>& manager, const std::vector<std::shared_ptr<ResourceClaim>>& components, const std::string& header, const std::string& demarcator, const std::string& footer);

  // Destructor
  ~MergedResourceClaim() override = default;

 private:
  std::string header_;
  std::string demarcator_;
  std::string footer_;

  std::vector<std::shared_ptr<ResourceClaim>> components_;

  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  MergedResourceClaim(const MergedResourceClaim &parent) = delete;
  MergedResourceClaim &operator=(const MergedResourceClaim &parent) = delete;
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
