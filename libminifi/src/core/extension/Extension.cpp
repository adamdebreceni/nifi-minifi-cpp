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

#include "core/extension/Extension.h"
#include "core/extension/ExtensionInterface.h"
#include "utils/GeneralUtils.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

std::shared_ptr<logging::Logger> Extension::logger_ = logging::LoggerFactory<Extension>::getLogger();

Extension::Extension(std::string name, std::string library_path, gsl::owner<void*> handle)
  : name_(std::move(name)),
    library_path_(std::move(library_path)),
    handle_(handle) {
  initializer_ = reinterpret_cast<std::decay<decltype(initializeExtension)>::type>(dlsym(handle_, "initializeExtension"));
  deinitializer_ = reinterpret_cast<std::decay<decltype(deinitializeExtension)>::type>(dlsym(handle_, "deinitializeExtension"));
}

std::unique_ptr<Extension> Extension::load(std::string name, std::string library_path) {
  dlerror();
  gsl::owner<void*> handle = dlopen(library_path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!handle) {
    logger_->log_error("Failed to load extension '%s' at '%s': %s", name, library_path, dlerror());
    return nullptr;
  } else {
    logger_->log_info("Loaded extension '%s' at '%s'", name, library_path);
  }
  return std::unique_ptr<Extension>(new Extension(std::move(name), std::move(library_path), std::move(handle)));
}

Extension::~Extension() {
  if (!initialized_) {
    logger_->log_debug("Extension '%s' was not initialized, no teardown needed", name_);
  } else {
    if (deinitializer_) {
      logger_->log_debug("Deinitializing extension '%s'", name_);
      deinitializer_();
    } else {
      logger_->log_debug("No deinitialization needed for '%s'", name_);
    }
  }
  dlerror();
  if (dlclose(handle_)) {
    logger_->log_error("Failed to unload extension '%s' at '%': %s", name_, library_path_, dlerror());
  } else {
    logger_->log_info("Unloaded extension '%s' at '%s'", name_, library_path_);
  }
}

bool Extension::initialize(const std::shared_ptr<Configure>& config) {
  if (initialized_) {
    return true;
  }
  std::lock_guard<std::mutex> guard(mtx_);
  if (initialized_) {
    return true;
  }
  if (initializer_) {
    logger_->log_debug("Initializing extension '%s'", name_);
    if (!initializer_(config)) {
      return false;
    }
  } else {
    logger_->log_debug("No initialization needed for '%s'", name_);
  }
  initialized_ = true;
  return true;
}

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
