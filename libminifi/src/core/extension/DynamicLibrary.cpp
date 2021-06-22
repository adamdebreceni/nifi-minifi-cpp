/**
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

#include <memory>
#include <dlfcn.h>

#include "core/extension/DynamicLibrary.h"
#include "core/extension/Extension.h"
#include "utils/GeneralUtils.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

std::shared_ptr<logging::Logger> DynamicLibrary::logger_ = logging::LoggerFactory<DynamicLibrary>::getLogger();

DynamicLibrary::DynamicLibrary(std::string name, std::string library_path)
  : Module(std::move(name)),
    library_path_(std::move(library_path)) {
}

bool DynamicLibrary::load() {
  dlerror();
  handle_ = dlopen(library_path_.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle_) {
    logger_->log_error("Failed to load extension '%s' at '%s': %s", name_, library_path_, dlerror());
    return false;
  } else {
    logger_->log_info("Loaded extension '%s' at '%s'", name_, library_path_);
    return true;
  }
}

DynamicLibrary::~DynamicLibrary() {
  if (!handle_) {
    return;
  }
  dlerror();
  if (dlclose(handle_)) {
    logger_->log_error("Failed to unload extension '%s' at '%': %s", name_, library_path_, dlerror());
  } else {
    logger_->log_info("Unloaded extension '%s' at '%s'", name_, library_path_);
  }
}

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
