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

#pragma once

#include <memory>

#include "core/logging/Logger.h"
#include "utils/gsl.h"
#include "properties/Configure.h"
#include "ExtensionInterface.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

class Extension {
  friend class ExtensionManager;
  Extension(std::string name, std::string library_path, gsl::owner<void*> handle);

 public:
  ~Extension();

 private:
  static std::unique_ptr<Extension> load(std::string name, std::string library_path);

  bool initialize(const std::shared_ptr<Configure>& config);

  std::string name_;
  std::string library_path_;
  gsl::owner<void*> handle_;

  std::atomic_bool initialized_{false};
  std::mutex mtx_;
  std::decay<decltype(initializeExtension)>::type initializer_;
  std::decay<decltype(deinitializeExtension)>::type deinitializer_;

  static std::shared_ptr<logging::Logger> logger_;
};

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
