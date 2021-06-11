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
#include <string>
#include "core/ClassLoader.h"
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

ClassLoader::ClassLoader() = default;

ClassLoader &ClassLoader::getDefaultClassLoader() {
  static ClassLoader ret;
  // populate ret
  return ret;
}

uint16_t ClassLoader::registerResource(const std::string &resource, const std::string &resourceFunction) {
  void *resource_ptr = nullptr;
  if (resource.empty()) {
    dlclose(dlopen(0, RTLD_LAZY | RTLD_GLOBAL));
    resource_ptr = dlopen(0, RTLD_NOW | RTLD_GLOBAL);
  } else {
    dlclose(dlopen(resource.c_str(), RTLD_LAZY | RTLD_GLOBAL));
    resource_ptr = dlopen(resource.c_str(), RTLD_NOW | RTLD_GLOBAL);
  }
  if (!resource_ptr) {
    return RESOURCE_FAILURE;
  } else {
    std::lock_guard<std::mutex> lock(internal_mutex_);
    dl_handles_.push_back(resource_ptr);
  }

  // reset errors
  dlerror();

  // load the symbols
  createFactory* create_factory_func = reinterpret_cast<createFactory*>(dlsym(resource_ptr, resourceFunction.c_str()));
  const char* dlsym_error = dlerror();
  if ((dlsym_error != nullptr && strlen(dlsym_error) > 0) || create_factory_func == nullptr) {
    return RESOURCE_FAILURE;
  }

  ObjectFactory *factory = create_factory_func();

  std::lock_guard<std::mutex> lock(internal_mutex_);

  auto initializer = factory->getInitializer();
  if (initializer != nullptr) {
    if (!initializer->initialize()) {
      delete factory;
      return RESOURCE_FAILURE;
    }
    initializers_.emplace_back(std::move(initializer));
  }

  for (auto class_name : factory->getClassNames()) {
    loaded_factories_[class_name] = std::unique_ptr<ObjectFactory>(factory->assign(class_name));
  }

  delete factory;

  return RESOURCE_SUCCESS;
}

ClassLoader::~ClassLoader() {
  for (auto& initializer : initializers_) {
    initializer->deinitialize();
  }
  loaded_factories_.clear();
  for (auto ptr : dl_handles_) {
    dlclose(ptr);
  }
}

#ifdef WIN32

void ClassLoader::store_error() {
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

void* ClassLoader::dlsym(void *handle, const char *name) {
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

const char* ClassLoader::dlerror(void) {
  std::lock_guard<std::mutex> lock(internal_mutex_);

  error_str_ = current_error_;

  current_error_ = "";

  return error_str_.c_str();
}

void* ClassLoader::dlopen(const char *file, int mode) {
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
        resource_mapping_.insert(std::make_pair((void*)allModules[i], "minifi-system"));
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
    resource_mapping_.insert(std::make_pair((void*)object, lpFileName));
  }

  /* Return to previous state of the error-mode bit flags. */
  SetErrorMode(uMode);

  return reinterpret_cast<void*>(object);
}

int ClassLoader::dlclose(void *handle) {
  std::lock_guard<std::mutex> lock(internal_mutex_);

  HMODULE object = (HMODULE)handle;
  BOOL ret;

  current_error_ = "";
  ret = FreeLibrary(object);

  resource_mapping_.erase((void*)object);

  ret = !ret;

  return static_cast<int>(ret);
}
#endif

} /* namespace core */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
