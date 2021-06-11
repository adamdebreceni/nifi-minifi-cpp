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

#include <stdexcept>

#include "../Logger.h"
#include "spdlog/common.h"


namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace logging {

static spdlog::level::level_enum to_spdlog_level(LOG_LEVEL level) {
  switch (level) {
    case LOG_LEVEL::trace: return spdlog::level::level_enum::trace;
    case LOG_LEVEL::debug: return spdlog::level::level_enum::debug;
    case LOG_LEVEL::info: return spdlog::level::level_enum::info;
    case LOG_LEVEL::warn: return spdlog::level::level_enum::warn;
    case LOG_LEVEL::err: return spdlog::level::level_enum::err;
    case LOG_LEVEL::critical: return spdlog::level::level_enum::critical;
    case LOG_LEVEL::off: return spdlog::level::level_enum::off;
  }
  throw std::invalid_argument("Expected a valid LOG_LEVEL value, got: " + std::to_string(level));
}

static LOG_LEVEL to_minifi_log_level(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::level_enum::trace: return LOG_LEVEL::trace;
    case spdlog::level::level_enum::debug: return LOG_LEVEL::debug;
    case spdlog::level::level_enum::info: return LOG_LEVEL::info;
    case spdlog::level::level_enum::warn: return LOG_LEVEL::warn;
    case spdlog::level::level_enum::err: return LOG_LEVEL::err;
    case spdlog::level::level_enum::critical: return LOG_LEVEL::critical;
    case spdlog::level::level_enum::off: return LOG_LEVEL::off;
  }
  throw std::invalid_argument("Expected a valid spdlog::level::level_enum value, got: " + std::to_string(level));
}

}  // namespace logging
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org