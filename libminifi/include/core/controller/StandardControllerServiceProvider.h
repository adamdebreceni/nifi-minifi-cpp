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
#include <utility>
#include <iostream>
#include <memory>
#include <vector>
#include "core/ProcessGroup.h"
#include "SchedulingAgent.h"
#include "core/ClassLoader.h"
#include "ControllerService.h"
#include "ControllerServiceMap.h"
#include "ControllerServiceNode.h"
#include "StandardControllerServiceNode.h"
#include "ControllerServiceProvider.h"
#include "core/logging/LoggerFactory.h"
#include "SchedulingAgent.h"

namespace org::apache::nifi::minifi::core::controller {

class StandardControllerServiceProvider : public ControllerServiceProvider, public std::enable_shared_from_this<StandardControllerServiceProvider> {
 public:
  explicit StandardControllerServiceProvider(std::shared_ptr<ControllerServiceMap> services, std::shared_ptr<Configure> configuration, ClassLoader &loader =
                                                 ClassLoader::getDefaultClassLoader())
      : ControllerServiceProvider(services),
        extension_loader_(loader),
        configuration_(configuration),
        logger_(logging::LoggerFactory<StandardControllerServiceProvider>::getLogger()) {
  }

  StandardControllerServiceProvider(const StandardControllerServiceProvider &other) = delete;
  StandardControllerServiceProvider(StandardControllerServiceProvider &&other) = delete;

  StandardControllerServiceProvider& operator=(const StandardControllerServiceProvider &other) = delete;
  StandardControllerServiceProvider& operator=(StandardControllerServiceProvider &&other) = delete;

  std::shared_ptr<ControllerServiceNode> createControllerService(const std::string &type, const std::string &fullType, const std::string &id, bool /*firstTimeAdded*/) {
    std::shared_ptr<ControllerService> new_controller_service = extension_loader_.instantiate<ControllerService>(type, id);

    if (nullptr == new_controller_service) {
      new_controller_service = extension_loader_.instantiate<ControllerService>("ExecuteJavaControllerService", id);
      if (new_controller_service != nullptr) {
        new_controller_service->initialize();
        new_controller_service->setProperty("NiFi Controller Service", fullType);
      } else {
        return nullptr;
      }
    }

    std::shared_ptr<ControllerServiceNode> new_service_node = std::make_shared<StandardControllerServiceNode>(new_controller_service,
                                                                                                              std::static_pointer_cast<ControllerServiceProvider>(shared_from_this()), id,
                                                                                                              configuration_);

    controller_map_->put(id, new_service_node);
    return new_service_node;
  }

  virtual void enableAllControllerServices() {
    logger_->log_info("Enabling %u controller services", controller_map_->getAllControllerServices().size());
    for (auto service : controller_map_->getAllControllerServices()) {
      logger_->log_info("Enabling %s", service->getName());
      if (!service->canEnable()) {
        logger_->log_warn("Service %s cannot be enabled", service->getName());
        continue;
      }
      if (!service->enable()) {
        logger_->log_warn("Could not enable %s", service->getName());
      }
    }
  }

  virtual void disableAllControllerServices() {
    logger_->log_info("Disabling %u controller services", controller_map_->getAllControllerServices().size());
    for (auto service : controller_map_->getAllControllerServices()) {
      logger_->log_info("Disabling %s", service->getName());
      if (!service->enabled()) {
        logger_->log_warn("Service %s is not enabled", service->getName());
        continue;
      }
      if (!service->disable()) {
        logger_->log_warn("Could not disable %s", service->getName());
      }
    }
  }

  void clearControllerServices() {
    controller_map_->clear();
  }

 protected:
  bool canEdit() {
    return false;
  }

  ClassLoader &extension_loader_;

  std::shared_ptr<Configure> configuration_;

 private:
  std::shared_ptr<logging::Logger> logger_;
};

}  // namespace org::apache::nifi::minifi::core::controller
