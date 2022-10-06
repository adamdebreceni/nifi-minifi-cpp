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

#include "core/ForwardingContentSession.h"

#include <memory>

#include "core/ContentRepository.h"
#include "ResourceClaim.h"
#include "io/BaseStream.h"
#include "Exception.h"

namespace org::apache::nifi::minifi::core {

ForwardingContentSession::ForwardingContentSession(std::shared_ptr<ContentRepository> repository) : repository_(std::move(repository)) {}

std::shared_ptr<ResourceClaim> ForwardingContentSession::create() {
  auto claim = std::make_shared<ResourceClaim>(repository_);
  created_claims_.insert(claim);
  return claim;
}

std::shared_ptr<io::BaseStream> ForwardingContentSession::write(const std::shared_ptr<ResourceClaim>& resource_id) {
  if (created_claims_.find(resource_id) == created_claims_.end()) {
    throw Exception(REPOSITORY_EXCEPTION, "Can only overwrite owned resource");
  }
  return repository_->write(*resource_id, false);
}

std::shared_ptr<io::BaseStream> ForwardingContentSession::append(const std::shared_ptr<ResourceClaim>& resource_id) {
  return repository_->write(*resource_id, true);
}

std::shared_ptr<io::BaseStream> ForwardingContentSession::read(const std::shared_ptr<ResourceClaim>& resource_id) {
  return repository_->read(*resource_id);
}

void ForwardingContentSession::commit() {
  created_claims_.clear();
}

void ForwardingContentSession::rollback() {
  created_claims_.clear();
}

}  // namespace org::apache::nifi::minifi::core

