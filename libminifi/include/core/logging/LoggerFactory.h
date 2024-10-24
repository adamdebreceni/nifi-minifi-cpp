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

#include <string>
#include <memory>

#include "core/logging/Logger.h"
#include "core/Core.h"

namespace org::apache::nifi::minifi::core::logging {

class LoggerFactoryBase {
 public:
  static std::shared_ptr<Logger> getAliasedLogger(const std::string& name, const std::optional<utils::Identifier>& id = {});
};

template<typename T>
class LoggerFactory : public LoggerFactoryBase {
 public:
  static std::shared_ptr<Logger> getLogger() {
    static std::shared_ptr<Logger> logger = getAliasedLogger(core::getClassName<T>());
    return logger;
  }

  static std::shared_ptr<Logger> getLogger(const utils::Identifier& uuid) {
    return getAliasedLogger(core::getClassName<T>(), uuid);
  }
};

}  // namespace org::apache::nifi::minifi::core::logging
