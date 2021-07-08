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

#include "core/extension/ExtensionManager.h"
#include "core/logging/LoggerConfiguration.h"
#include "utils/file/FileUtils.h"
#include "core/extension/Executable.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

namespace {
struct LibraryDescriptor {
  std::string name;
  std::string dir;
  std::string filename;

  bool verify(const std::shared_ptr<logging::Logger>& /*logger*/) const {
    // TODO(adebreceni): check signature
    return true;
  }

  std::string getFullPath() const {
    return utils::file::PathUtils::concat_path(dir, filename);
  }
};
}  // namespace

static utils::optional<LibraryDescriptor> asDynamicLibrary(const std::string& dir, const std::string& filename) {
#if defined(WIN32)
  const std::string extension = ".dll";
#elif defined(__APPLE__)
  const std::string extension = ".dylib";
#else
  const std::string extension = ".so";
#endif
  if (!utils::StringUtils::endsWith(filename, extension)) {
    return {};
  }
  return LibraryDescriptor{
    filename.substr(0, filename.length() - extension.length()),
    dir,
    filename
  };
}

std::shared_ptr<logging::Logger> ExtensionManager::logger_ = logging::LoggerFactory<ExtensionManager>::getLogger();

ExtensionManager::ExtensionManager() {
  modules_.push_back(utils::make_unique<Executable>());
  active_module_ = modules_[0].get();
}

ExtensionManager& ExtensionManager::instance() {
  static ExtensionManager instance;
  return instance;
}

#ifdef WIN32
static const char* const EXTENSION_PREFIX = "minifi-";
#else
static const char* const EXTENSION_PREFIX = "libminifi-";
#endif

bool ExtensionManager::initialize(const std::shared_ptr<Configure>& config) {
  static bool initialized = ([&] {
    logger_->log_error("Initializing extensions");
    // initialize executable
    instance().active_module_->initialize(config);
    utils::optional<std::string> dir = config ? config->get(nifi_extension_directory) : utils::nullopt;
    if (!dir) return;
    std::vector<LibraryDescriptor> libraries;
    utils::file::FileUtils::list_dir(dir.value(), [&] (const std::string& path, const std::string& filename) {
      if (!utils::StringUtils::startsWith(filename, EXTENSION_PREFIX)) return true;
      utils::optional<LibraryDescriptor> library = asDynamicLibrary(path, filename);
      if (library && library->verify(logger_)) {
        libraries.push_back(std::move(library.value()));
      }
      return true;
    }, logger_, false);
    for (const auto& library : libraries) {
      auto module = utils::make_unique<DynamicLibrary>(library.name, library.getFullPath());
      instance().active_module_ = module.get();
      if (!module->load()) {
        // error already logged by method
        continue;
      }
      if (!module->initialize(config)) {
        logger_->log_error("Failed to initialize module '%s' at '%s'", library.name, library.getFullPath());
      } else {
        instance().modules_.push_back(std::move(module));
      }
    }
  }(), true);
  return initialized;
}

void ExtensionManager::registerExtension(Extension *extension) {
  active_module_->registerExtension(extension);
}

void ExtensionManager::unregisterExtension(Extension *extension) {
  for (const auto& module : modules_) {
    if (module->unregisterExtension(extension)) {
      return;
    }
  }
}

bool ExtensionManager::unloadModule(const std::string& name) {
  logger_->log_info("Trying to unload module '%s'", name);
  auto it = std::find_if(modules_.begin(), modules_.end(), [&] (const std::unique_ptr<Module>& module) {
    gsl_Expects(module);
    return module->getName() == name;
  });
  if (it == modules_.end()) {
    logger_->log_error("Could not find module '%s'", name);
    return false;
  }
  auto* lib = dynamic_cast<DynamicLibrary*>(it->get());
  if (lib == nullptr) {
    logger_->log_error("Cannot unload non-dynamic library '%s'", name);
    return false;
  }
  if (!lib->unload()) {
    logger_->log_error("Unloading library '%s' failed", name);
    return false;
  }
  modules_.erase(it);
  return true;
}

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
