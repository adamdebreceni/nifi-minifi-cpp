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

#ifndef LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_
#define LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_

#include <utility>
#include <mutex>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include "utils/StringUtils.h"
#ifndef WIN32
#include <dlfcn.h>
#define DLL_EXPORT
#else
#define WIN32_LEAN_AND_MEAN 1
#include <Windows.h>    // Windows specific libraries for collecting software metrics.
#include <Psapi.h>
#pragma comment(lib, "psapi.lib" )
#define DLL_EXPORT __declspec(dllexport)
#endif
#include "core/Core.h"
#include "io/BufferStream.h"
#include "ObjectFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

#define RESOURCE_FAILURE -1

#define RESOURCE_SUCCESS 1

#ifdef WIN32
#define RTLD_LAZY   0
#define RTLD_NOW    0

#define RTLD_GLOBAL (1 << 1)
#define RTLD_LOCAL  (1 << 2)
#endif

/**
 * Function that is used to create the
 * processor factory from the shared object.
 */
typedef ObjectFactory* createFactory();

/**
 * Processor class loader that accepts
 * a variety of mechanisms to load in shared
 * objects.
 */
class ClassLoader {
 public:
  static ClassLoader &getDefaultClassLoader();

  /**
   * Constructor.
   */
  ClassLoader();

  /**
   * Retrieves a class loader
   * @param name name of class loader
   * @return class loader reference
   */
  ClassLoader& getClassLoader(const std::string& name) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    return class_loaders_[name];
  }

  /**
   * Register a class with the give ProcessorFactory
   */
  void registerClass(const std::string &name, std::unique_ptr<ObjectFactory> factory) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    if (loaded_factories_.find(name) != loaded_factories_.end()) {
      logger_->log_error("Class '%s' is already registered", name);
      return;
    }
    logger_->log_error("Registering class '%s'", name);
    loaded_factories_.insert(std::make_pair(name, std::move(factory)));
  }

  void unregisterClass(const std::string& name) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    if (loaded_factories_.erase(name) == 0) {
      logger_->log_error("Could not unregister non-registered class '%s'", name);
      return;
    } else {
      logger_->log_error("Unregistered class '%s'", name);
    }
  }

  std::vector<std::string> getClasses(const std::string &group) const {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    std::vector<std::string> classes;
    for (const auto& child_loader : class_loaders_) {
      for (auto&& clazz : child_loader.second.getClasses(group)) {
        classes.push_back(std::move(clazz));
      }
    }
    for (const auto& factory : loaded_factories_) {
      if (factory.second->getGroupName() == group) {
        classes.push_back(factory.second->getClassName());
      }
    }
    return classes;
  }

  utils::optional<std::string> getGroupForClass(const std::string &class_name) const {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    for (const auto& child_loader : class_loaders_) {
      utils::optional<std::string> group = child_loader.second.getGroupForClass(class_name);
      if (group) {
        return group;
      }
    }
    auto factory = loaded_factories_.find(class_name);
    if (factory != loaded_factories_.end()) {
      return factory->second->getGroupName();
    }
    return {};
  }

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::shared_ptr<T> instantiate(const std::string &class_name, const std::string &name);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  std::shared_ptr<T> instantiate(const std::string &class_name, const utils::Identifier &uuid);

  /**
   * Instantiate object based on class_name
   * @param class_name class to create
   * @param uuid uuid of object
   * @return nullptr or object created from class_name definition.
   */
  template<class T = CoreComponent>
  T *instantiateRaw(const std::string &class_name, const std::string &name);

 protected:
#ifdef WIN32

  // base_object doesn't have a handle
  std::map< HMODULE, std::string > resource_mapping_;

  std::string error_str_;
  std::string current_error_;

  void store_error() {
    auto error = GetLastError();

    if (error == 0) {
      error_str_ = "";
      return;
    }

    LPSTR messageBuffer = nullptr;
    size_t size = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

    current_error_ = std::string(messageBuffer, size);

    // Free the buffer.
    LocalFree(messageBuffer);
  }

  void *dlsym(void *handle, const char *name) {
    FARPROC symbol;

    symbol = GetProcAddress((HMODULE)handle, name);

    if (symbol == nullptr) {
      store_error();

      for (auto hndl : resource_mapping_) {
        symbol = GetProcAddress((HMODULE)hndl.first, name);
        if (symbol != nullptr) {
          break;
        }
      }
    }

#ifdef _MSC_VER
#pragma warning(suppress: 4054 )
#endif
    return reinterpret_cast<void*>(symbol);
  }

  const char *dlerror(void) {
    std::lock_guard<std::mutex> lock(internal_mutex_);

    error_str_ = current_error_;

    current_error_ = "";

    return error_str_.c_str();
  }

  void *dlopen(const char *file, int mode) {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    HMODULE object;
    uint32_t uMode = SetErrorMode(SEM_FAILCRITICALERRORS);
    if (nullptr == file) {
      HMODULE allModules[1024];
      HANDLE current_process_id = GetCurrentProcess();
      DWORD cbNeeded;
      object = GetModuleHandle(NULL);

      if (!object)
      store_error();
      if (EnumProcessModules(current_process_id, allModules,
              sizeof(allModules), &cbNeeded) != 0) {
        for (uint32_t i = 0; i < cbNeeded / sizeof(HMODULE); i++) {
          // Get the full path to the module's file.
          resource_mapping_.insert(std::make_pair(allModules[i], "minifi-system"));
        }
      }
    } else {
      char lpFileName[MAX_PATH];
      int i;

      for (i = 0; i < sizeof(lpFileName) - 1; i++) {
        if (!file[i])
        break;
        else if (file[i] == '/')
        lpFileName[i] = '\\';
        else
        lpFileName[i] = file[i];
      }
      lpFileName[i] = '\0';
      object = LoadLibraryEx(lpFileName, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
      if (!object)
      store_error();
      else if ((mode & RTLD_GLOBAL))
      resource_mapping_.insert(std::make_pair(object, lpFileName));
    }

    /* Return to previous state of the error-mode bit flags. */
    SetErrorMode(uMode);

    return reinterpret_cast<void*>(object);
  }

  int dlclose(void *handle) {
    std::lock_guard<std::mutex> lock(internal_mutex_);

    HMODULE object = (HMODULE)handle;
    BOOL ret;

    current_error_ = "";
    ret = FreeLibrary(object);

    resource_mapping_.erase(object);

    ret = !ret;

    return static_cast<int>(ret);
  }

#endif

  std::map<std::string, std::unique_ptr<ObjectFactory>> loaded_factories_;

  std::map<std::string, ClassLoader> class_loaders_;

  mutable std::mutex internal_mutex_;

  std::shared_ptr<logging::Logger> logger_;
};

template<class T>
std::shared_ptr<T> ClassLoader::instantiate(const std::string &class_name, const std::string &name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto result = child_loader.second.instantiate<T>(class_name, name)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(name);
    return std::dynamic_pointer_cast<T>(obj);
  }
  return nullptr;
}

template<class T>
std::shared_ptr<T> ClassLoader::instantiate(const std::string &class_name, const utils::Identifier &uuid) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto result = child_loader.second.instantiate<T>(class_name, uuid)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->create(class_name, uuid);
    return std::dynamic_pointer_cast<T>(obj);
  }
  return nullptr;
}

template<class T>
T *ClassLoader::instantiateRaw(const std::string &class_name, const std::string &name) {
  std::lock_guard<std::mutex> lock(internal_mutex_);
  // allow subsequent classes to override functionality (like ProcessContextBuilder)
  for (auto& child_loader : class_loaders_) {
    if (auto* result = child_loader.second.instantiateRaw<T>(class_name, name)) {
      return result;
    }
  }
  auto factory_entry = loaded_factories_.find(class_name);
  if (factory_entry != loaded_factories_.end()) {
    auto obj = factory_entry->second->createRaw(name);
    return dynamic_cast<T*>(obj);
  }
  return nullptr;
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_CLASSLOADER_H_
