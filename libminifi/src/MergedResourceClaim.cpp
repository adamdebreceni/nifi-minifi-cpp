/**
 * @file MergedResourceClaim.cpp
 * MergedResourceClaim class implementation
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
#include "MergedResourceClaim.h"
#include <map>
#include <queue>
#include <string>
#include <vector>
#include <memory>
#include "core/StreamManager.h"
#include "utils/Id.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

MergedResourceClaim::MergedResourceClaim(const std::shared_ptr<core::StreamManager<ResourceClaim>>& manager, const std::vector<std::shared_ptr<ResourceClaim>>& components, const std::string& header, const std::string& demarcator, const std::string& footer)
    : ResourceClaim(manager),
      components_(components),
      footer_(footer),
      demarcator_(demarcator),
      header_(header) {}

} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
