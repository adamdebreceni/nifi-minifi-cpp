/**
 * @file CongestionTest.cpp
 * ProcessSession class implementation
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

#include <unordered_map>
#include <string>
#include <random>
#include <YamlConfiguration.h>
#include "core/Processor.h"
#include "TestBase.h"

std::atomic_bool route_to_banana{false};

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

static core::Relationship Apple{"apple", ""};
static core::Relationship Banana{"banana", ""};
// The probability that this processor routes to Apple
static core::Property AppleProbability = core::PropertyBuilder::createProperty("AppleProbability")->withDefaultValue<int>(100)->build();
// The probability that this processor routes to Banana
static core::Property BananaProbability = core::PropertyBuilder::createProperty("BananaProbability")->withDefaultValue<int>(0)->build();


class TestProcessor : public org::apache::nifi::minifi::core::Processor {
 public:
  TestProcessor(std::string name, utils::Identifier &uuid) : Processor(name, uuid) {}
  TestProcessor(std::string name) : Processor(name) {}
  void initialize() override {
    setSupportedProperties({AppleProbability, BananaProbability});
    setSupportedRelationships({Apple, Banana});
  }
  void onTrigger(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSession> &session) override {
    auto flowFile = session->get();
    if (!flowFile) return;
    if (route_to_banana) {
      session->transfer(flowFile, Banana);
      return;
    }
    std::random_device rd{};
    std::uniform_int_distribution<int> dis(0, 100);
    int rand = dis(rd);
    if (rand <= apple_probability_) {
      session->transfer(flowFile, Apple);
      return;
    }
    rand -= apple_probability_;
    if (rand <= banana_probability_) {
      session->transfer(flowFile, Banana);
      return;
    }
    throw std::runtime_error("Couldn't route file");
  }
  void onSchedule(const std::shared_ptr<core::ProcessContext> &context, const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) override {
    int apple;
    assert(context->getProperty(AppleProbability.getName(), apple));
    int banana;
    assert(context->getProperty(BananaProbability.getName(), banana));
    apple_probability_ = apple;
    banana_probability_ = banana;
  }
  std::atomic<int> apple_probability_;
  std::atomic<int> banana_probability_;
};

REGISTER_RESOURCE(TestProcessor, "Processor used for testing");

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

/*
const char* flowConfigurationYaml =
R"(
Flow Controller:
  name: MiNiFi Flow
  id: 2438e3c8-015a-1001-79ca-83af40ec1990
Processors:
  - name: A_GenerateFlowFile
    id: 2438e3c8-015a-1001-79ca-83af40ec1991
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
  - name: A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1992
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
      - banana
    Properties:
      AppleProbability: 100
      BananaProbability: 0

Connections:
  - name: A_GenerateFlowFile/success/A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1993
    source name: A_GenerateFlowFile
    source relationship name: success
    destination name: A_TestProcessor
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: A_TestProcessor/apple/A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1994
    source name: A_TestProcessor
    destination name: A_TestProcessor
    source relationship name: apple
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0

Remote Processing Groups:
)";

const char* flowConfigurationYaml =
    R"(
Flow Controller:
  name: MiNiFi Flow
  id: 2438e3c8-015a-1001-79ca-83af40ec1990
Processors:
  - name: A_GenerateFlowFile
    id: 2438e3c8-015a-1001-79ca-83af40ec1991
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
  - name: A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1992
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 50
      BananaProbability: 50
  - name: B_GenerateFlowFile
    id: 2438e3c8-015a-1001-79ca-83af40ec1998
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
  - name: B_TestProcessor1
    id: 2438e3c8-015a-1001-79ca-83af40ec1999
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 100
      BananaProbability: 0
  - name: B_TestProcessor2
    id: 2438e3c8-015a-1001-79ca-83af40ec2000
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 100
      BananaProbability: 0

Connections:
  - name: A_GenerateFlowFile/success/A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1993
    source name: A_GenerateFlowFile
    source relationship name: success
    destination name: A_TestProcessor
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: A_TestProcessor/apple/A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1994
    source name: A_TestProcessor
    destination name: A_TestProcessor
    source relationship name: apple
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: A_TestProcessor/banana/A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1995
    source name: A_TestProcessor
    destination name: A_TestProcessor
    source relationship name: banana
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: B_GenerateFlowFile/success/B_TestProcessor1
    id: 2438e3c8-015a-1001-79ca-83af40ec2001
    source name: B_GenerateFlowFile
    source relationship name: success
    destination name: B_TestProcessor1
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: B_TestProcessor1/apple/B_TestProcessor2
    id: 2438e3c8-015a-1001-79ca-83af40ec2002
    source name: B_TestProcessor1
    destination name: B_TestProcessor2
    source relationship name: apple
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: B_TestProcessor2/apple/B_TestProcessor1
    id: 2438e3c8-015a-1001-79ca-83af40ec2003
    source name: B_TestProcessor2
    destination name: B_TestProcessor1
    source relationship name: apple
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0

Remote Processing Groups:
)";
 */
/*
const char* flowConfigurationYaml =
    R"(
Flow Controller:
  name: MiNiFi Flow
  id: 2438e3c8-015a-1001-79ca-83af40ec1990
Processors:
  - name: A_GenerateFlowFile
    id: 2438e3c8-015a-1001-79ca-83af40ec1991
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
  - name: A_TestProcessor
    id: 2438e3c8-015a-1001-79ca-83af40ec1992
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 50
      BananaProbability: 50

Connections:
  - name: A_Gen
    id: 2438e3c8-015a-1001-79ca-83af40ec1993
    source name: A_GenerateFlowFile
    source relationship name: success
    destination name: A_TestProcessor
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: A_Apple
    id: 2438e3c8-015a-1001-79ca-83af40ec1994
    source name: A_TestProcessor
    destination name: A_TestProcessor
    source relationship name: apple
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: A_Banana
    id: 2438e3c8-015a-1001-79ca-83af40ec1995
    source name: A_TestProcessor
    destination name: A_TestProcessor
    source relationship name: banana
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0

Remote Processing Groups:
)";*/

const char* flowConfigurationYaml =
    R"(
Flow Controller:
  name: MiNiFi Flow
  id: 2438e3c8-015a-1001-79ca-83af40ec1990
Processors:
  - name: B_GenerateFlowFile
    id: 2438e3c8-015a-1001-79ca-83af40ec1998
    class: org.apache.nifi.processors.standard.GenerateFlowFile
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
  - name: B_TestProcessor1
    id: 2438e3c8-015a-1001-79ca-83af40ec1999
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 100
      BananaProbability: 0
  - name: B_TestProcessor2
    id: 2438e3c8-015a-1001-79ca-83af40ec2000
    class: org.apache.nifi.processors.TestProcessor
    max concurrent tasks: 1
    scheduling strategy: TIMER_DRIVEN
    scheduling period: 100 ms
    penalization period: 300 ms
    yield period: 100 ms
    run duration nanos: 0
    auto-terminated relationships list:
    Properties:
      AppleProbability: 100
      BananaProbability: 0

Connections:
  - name: B_Gen
    id: 2438e3c8-015a-1001-79ca-83af40ec2001
    source name: B_GenerateFlowFile
    source relationship name: success
    destination name: B_TestProcessor1
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: B_Apple_FORWARD
    id: 2438e3c8-015a-1001-79ca-83af40ec2002
    source name: B_TestProcessor1
    destination name: B_TestProcessor2
    source relationship name: apple
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0
  - name: B_Apple_BACKWARD
    id: 2438e3c8-015a-1001-79ca-83af40ec2003
    source name: B_TestProcessor2
    destination name: B_TestProcessor1
    source relationship name: apple
    max work queue size: 100
    max work queue data size: 1 MB
    flowfile expiration: 0

Remote Processing Groups:
)";

int main() {
  TestController testController;

  LogTestController::getInstance().setTrace<core::Processor>();
  LogTestController::getInstance().setTrace<minifi::Connection>();
  LogTestController::getInstance().setTrace<core::ProcessSession>();

  char format[] = "/tmp/flow.XXXXXX";
  std::string dir = testController.createTempDirectory(format);

  std::string yamlPath = utils::file::FileUtils::concat_path(dir, "config.yml");
  std::ofstream{yamlPath} << flowConfigurationYaml;

  std::shared_ptr<minifi::Configure> configuration = std::make_shared<minifi::Configure>();
  std::shared_ptr<core::Repository> prov_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<core::Repository> ff_repo = std::make_shared<core::repository::VolatileContentRepository>();
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::VolatileContentRepository>();

  configuration->set(minifi::Configure::nifi_flow_configuration_file, yamlPath);

  std::shared_ptr<minifi::io::StreamFactory> stream_factory = minifi::io::StreamFactory::getInstance(configuration);
  content_repo->initialize(configuration);

  std::unique_ptr<core::FlowConfiguration> flow = utils::make_unique<core::YamlConfiguration>(prov_repo, ff_repo, content_repo, stream_factory, configuration, yamlPath);

  std::shared_ptr<minifi::FlowController> controller = std::make_shared<minifi::FlowController>(prov_repo, ff_repo, configuration,
      utils::make_unique<core::YamlConfiguration>(prov_repo, ff_repo, content_repo, stream_factory, configuration, yamlPath),
      content_repo, DEFAULT_ROOT_GROUP_NAME, true);

  std::shared_ptr<core::ProcessGroup> root = flow->getRoot();
  std::vector<std::shared_ptr<core::Processor>> processors;
  root->getAllProcessors(processors);
  std::map<std::string, std::shared_ptr<minifi::Connection>> connections;
  {
    std::map<std::string, std::shared_ptr<minifi::Connection>> all_connections;
    root->getConnections(all_connections);
    // deduplicate connections
    for (auto &it : all_connections) {
      if (connections.find(it.second->getName()) != connections.end()) continue;
      connections.emplace(it.second->getName(), it.second);
    }
  }

  struct Flow{
    std::vector<std::shared_ptr<core::Processor>> processors;
    std::map<std::string, std::shared_ptr<minifi::Connection>> connections;
  };

  std::unordered_map<std::string, Flow> flows;
  for (auto& proc : processors) {
    flows[std::string{proc->getName()[0]}].processors.emplace_back(proc);
  }
  for (auto& conn : connections) {
    flows[std::string{conn.first[0]}].connections.emplace(conn);
  }

  std::string statusPath = "/Users/adamdebreceni/work/output";

  std::atomic_bool running{true};

  auto statusThread = std::thread([&] {
    while(running) {
      {
        std::ofstream out{statusPath};
        for (auto& flow : flows) {
          out << "Flow_" << flow.first << ":\n\n";
          out << "\tExecution Probabilities:\n\n";
          for (auto &proc : flow.second.processors) {
            out << "\t\t[" << proc->getName() << "]: " << proc->getExecutionProbability() << "\n";
            for (auto &input : proc->getIncomingWeights()) {
              out << "\t\t\t[" << input.first->getName() << "]: " << input.second << "\n";
            }
          }
          out << "\n" << "\tConnection Congestions:\n\n";
          for (auto &it : flow.second.connections) {
            out << "\t\t[" << it.second->getName() << "] count: " << it.second->getQueueSize() << " fullness: "
                << it.second->getCongestion().getValue() << "\n";
          }
          out << "\n";
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{1000});
    }
  });

  controller->load(root);
  controller->start();

  //std::this_thread::sleep_for(std::chrono::milliseconds{60000});
  //route_to_banana = true;

  std::this_thread::sleep_for(std::chrono::milliseconds{2000000});

  running = false;
  statusThread.join();
}