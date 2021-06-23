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
#include "DynamicLibrary.h"
#include "properties/Configure.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {
namespace extension {

static constexpr const char* nifi_extension_directory = "nifi.extension.directory";

class ExtensionManager {
  ExtensionManager();

 public:
  static ExtensionManager &instance();

  static bool initialize(const std::shared_ptr<Configure>& config);

  void registerExtension(Extension* extension);
  void unregisterExtension(Extension* extension);

 private:
  std::vector<std::unique_ptr<Module>> modules_;

  Module* active_module_;

  static std::shared_ptr<logging::Logger> logger_;
};

}  // namespace extension
}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org