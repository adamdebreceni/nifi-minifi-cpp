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
#ifndef LIBMINIFI_INCLUDE_CORE_STATE_NODES_DEVICEINFORMATION_H_
#define LIBMINIFI_INCLUDE_CORE_STATE_NODES_DEVICEINFORMATION_H_

#include "core/Resource.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <set>
#include <string>
#include <vector>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>

#include "../nodes/MetricsBase.h"
#include "Connection.h"
#include "io/ClientSocket.h"
#include "utils/OsUtils.h"
#include "utils/SystemCpuUsageTracker.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace state {
namespace response {

class Device {
 public:
  Device() {
    initialize();
  }
  void initialize();

  std::string canonical_hostname_;
  std::string ip_;
  std::string device_id_;

 protected:
  std::vector<std::string> getIpAddresses();

  std::string getDeviceId();

  // connection information
  int32_t socket_file_descriptor_;
};

/**
 * Justification and Purpose: Provides Device Information
 */
class DeviceInfoNode : public DeviceInformation {
 public:
  DeviceInfoNode(const std::string& name, const utils::Identifier& uuid)
      : DeviceInformation(name, uuid) {
    static Device device;
    hostname_ = device.canonical_hostname_;
    ip_ = device.ip_;
    device_id_ = device.device_id_;
  }

  DeviceInfoNode(const std::string &name) // NOLINT
      : DeviceInformation(name) {
    static Device device;
    hostname_ = device.canonical_hostname_;
    ip_ = device.ip_;
    device_id_ = device.device_id_;
  }

  std::string getName() const {
    return "deviceInfo";
  }

  std::vector<SerializedResponseNode> serialize() {
    std::vector<SerializedResponseNode> serialized;

    serialized.push_back(serializeIdentifier());
    serialized.push_back(serializeSystemInfo());
    serialized.push_back(serializeNetworkInfo());

    return serialized;
  }

 protected:
  SerializedResponseNode serializeIdentifier() const {
    SerializedResponseNode identifier;
    identifier.name = "identifier";
    identifier.value = device_id_;
    return identifier;
  }

  SerializedResponseNode serializeVCoreInfo() const {
    SerializedResponseNode v_cores;
    v_cores.name = "vCores";
    v_cores.value = std::thread::hardware_concurrency();
    return v_cores;
  }

  SerializedResponseNode serializeOperatingSystemType() const {
    SerializedResponseNode os_type;
    os_type.name = "operatingSystem";
    os_type.value = getOperatingSystem();
    return os_type;
  }

  SerializedResponseNode serializeTotalPhysicalMemoryInformation() const {
    SerializedResponseNode total_physical_memory;
    total_physical_memory.name = "physicalMem";
    total_physical_memory.value = utils::OsUtils::getSystemTotalPhysicalMemory();
    return total_physical_memory;
  }

  SerializedResponseNode serializePhysicalMemoryUsageInformation() const {
    SerializedResponseNode used_physical_memory;
    used_physical_memory.name = "memoryUsage";
    used_physical_memory.value = utils::OsUtils::getSystemPhysicalMemoryUsage();
    return used_physical_memory;
  }

  SerializedResponseNode serializeSystemCPUUsageInformation() const {
    double system_cpu_usage = -1.0;
    {
      std::lock_guard<std::mutex> guard(cpu_load_tracker_mutex_);
      system_cpu_usage = cpu_load_tracker_.getCpuUsageAndRestartCollection();
    }
    SerializedResponseNode cpu_usage;
    cpu_usage.name = "cpuUtilization";
    cpu_usage.value = system_cpu_usage;
    return cpu_usage;
  }

  SerializedResponseNode serializeArchitectureInformation() const {
    SerializedResponseNode arch;
    arch.name = "machinearch";
    arch.value = utils::OsUtils::getMachineArchitecture();
    return arch;
  }

  SerializedResponseNode serializeSystemInfo() const {
    SerializedResponseNode systemInfo;
    systemInfo.name = "systemInfo";

    systemInfo.children.push_back(serializeVCoreInfo());
    systemInfo.children.push_back(serializeOperatingSystemType());
    systemInfo.children.push_back(serializeTotalPhysicalMemoryInformation());
    systemInfo.children.push_back(serializeArchitectureInformation());
    systemInfo.children.push_back(serializePhysicalMemoryUsageInformation());
    systemInfo.children.push_back(serializeSystemCPUUsageInformation());

    return systemInfo;
  }

  SerializedResponseNode serializeHostNameInfo() const {
    SerializedResponseNode hostname;
    hostname.name = "hostname";
    hostname.value = hostname_;
    return hostname;
  }

  SerializedResponseNode serializeIPAddress() const {
    SerializedResponseNode ip;
    ip.name = "ipAddress";
    ip.value = !ip_.empty() ? ip_ : "127.0.0.1";
    return ip;
  }

  SerializedResponseNode serializeNetworkInfo() const {
    SerializedResponseNode network_info;
    network_info.name = "networkInfo";
    network_info.children.push_back(serializeHostNameInfo());
    network_info.children.push_back(serializeIPAddress());
    return network_info;
  }

  /**
   * Have found various ways of identifying different operating system variants
   * so these were either pulled from header files or online.
   */
  static inline std::string getOperatingSystem() {
    /**
     * We define WIN32, but *most* compilers should provide _WIN32.
     */
#if defined(WIN32) || defined(_WIN32) || defined(_WIN64)
    return "Windows";
#elif defined(__APPLE__) || defined(__MACH__)
    return "Mac OSX";
#elif defined(__linux__)
    return "Linux";
#elif defined(__unix) || defined(__unix__) || defined(__FreeBSD__)
    return "Unix";
#else
    return "Other";
#endif
  }

  std::string hostname_;
  std::string ip_;
  std::string device_id_;
  static utils::SystemCpuUsageTracker cpu_load_tracker_;
  static std::mutex cpu_load_tracker_mutex_;
};

REGISTER_RESOURCE(DeviceInfoNode, "Node part of an AST that defines device characteristics to the C2 protocol");

}  // namespace response
}  // namespace state
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_STATE_NODES_DEVICEINFORMATION_H_
