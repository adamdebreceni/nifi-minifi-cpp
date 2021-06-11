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

#undef NDEBUG
#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "provenance/Provenance.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "../TestBase.h"
#include "YamlConfiguration.h"
#include "CustomProcessors.h"
#include "TestControllerWithFlow.h"

const char* yamlConfig =
    R"(
Flow Controller:
  name: MiNiFi Flow
Processors:
  - name: Generator
    id: 00000000-0000-0000-0000-000000000001
    class: org.apache.nifi.processors.standard.TestFlowFileGenerator
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
      - success
    Properties:
      Batch Size: '500'
      File Size: 2 kB
)";

TEST_CASE("Performance test", "[TestFlow1]") {
  TestControllerWithFlow testController(yamlConfig);
  auto controller = testController.controller_;
  auto root = testController.root_;

  testController.startFlow();

  // wait for the generator to create some files
  std::this_thread::sleep_for(std::chrono::milliseconds{1000000});
}