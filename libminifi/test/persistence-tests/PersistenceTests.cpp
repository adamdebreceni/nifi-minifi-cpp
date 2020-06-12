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

#include <chrono>
#include <map>
#include <memory>
#include <string>
#include <thread>

#include "core/Core.h"
#include "core/repository/AtomicRepoEntries.h"
#include "core/RepositoryFactory.h"
#include "FlowFileRecord.h"
#include "FlowFileRepository.h"
#include "properties/Configure.h"
#include "../unit/ProvenanceTestHelper.h"
#include "../TestBase.h"
#include "../../extensions/libarchive/MergeContent.h"
#include "../test/BufferReader.h"

using Connection = minifi::Connection;
using MergeContent = minifi::processors::MergeContent;

struct TestFlow{
  TestFlow(const std::shared_ptr<core::repository::FlowFileRepository>& ff_repository, const std::shared_ptr<core::ContentRepository>& content_repo, const std::shared_ptr<core::Repository>& prov_repo)
      : ff_repository(ff_repository), content_repo(content_repo), prov_repo(prov_repo) {

    std::shared_ptr<core::controller::ControllerServiceProvider> controller_services_provider = nullptr;

    // setup MERGE processor
    {
      merge = std::make_shared<MergeContent>("MergeContent", mergeProcUUID());
      merge->initialize();
      merge->setAutoTerminatedRelationships({{"original", "d"}});

      merge->setProperty(MergeContent::MergeFormat, MERGE_FORMAT_CONCAT_VALUE);
      merge->setProperty(MergeContent::MergeStrategy, MERGE_STRATEGY_BIN_PACK);
      merge->setProperty(MergeContent::DelimiterStratgey, DELIMITER_STRATEGY_TEXT);
      merge->setProperty(MergeContent::MinEntries, "3");
      merge->setProperty(MergeContent::Header, "_Header_");
      merge->setProperty(MergeContent::Footer, "_Footer_");
      merge->setProperty(MergeContent::Demarcator, "_Demarcator_");
      merge->setProperty(MergeContent::MaxBinAge, "1 h");

      std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(merge);
      mergeContext = std::make_shared<core::ProcessContext>(node, controller_services_provider, prov_repo, ff_repository, content_repo);
    }

    // setup INPUT processor
    {
      inputProcessor = std::make_shared<core::Processor>("source", inputProcUUID());
      std::shared_ptr<core::ProcessorNode> node = std::make_shared<core::ProcessorNode>(inputProcessor);
      inputContext = std::make_shared<core::ProcessContext>(node, controller_services_provider, prov_repo,
                                                            ff_repository, content_repo);
    }

    // setup Input Connection
    {
      input = std::make_shared<Connection>(ff_repository, content_repo, "Input", inputConnUUID());
      input->setRelationship({"input", "d"});
      input->setDestinationUUID(mergeProcUUID());
      input->setSourceUUID(inputProcUUID());
      inputProcessor->addConnection(input);
    }

    // setup Output Connection
    {
      output = std::make_shared<Connection>(ff_repository, content_repo, "Output", outputConnUUID());
      output->setRelationship(MergeContent::Merge);
      output->setSourceUUID(mergeProcUUID());
    }

    // setup ProcessGroup
    {
      root = std::make_shared<core::ProcessGroup>(core::ProcessGroupType::ROOT_PROCESS_GROUP, "root");
      root->addProcessor(merge);
      root->addConnection(input);
      root->addConnection(output);
    }

    // prepare Merge Processor for execution
    merge->setScheduledState(core::ScheduledState::RUNNING);
    merge->onSchedule(mergeContext.get(), new core::ProcessSessionFactory(mergeContext));
  }
  void write(const std::string& data) {
    minifi::io::DataStream stream(reinterpret_cast<const uint8_t*>(data.c_str()), data.length());
    core::ProcessSession sessionGenFlowFile(inputContext);
    std::shared_ptr<core::FlowFile> flow = std::static_pointer_cast<core::FlowFile>(sessionGenFlowFile.create());
    sessionGenFlowFile.importFrom(stream, flow);
    sessionGenFlowFile.transfer(flow, {"input", "d"});
    sessionGenFlowFile.commit();
  }
  std::string read(const std::shared_ptr<core::FlowFile>& file) {
    core::ProcessSession session(mergeContext);
    std::vector<uint8_t> buffer;
    BufferReader reader(buffer);
    session.read(file, &reader);
    return {buffer.data(), buffer.data() + buffer.size()};
  }
  void trigger() {
    auto session = std::make_shared<core::ProcessSession>(mergeContext);
    merge->onTrigger(mergeContext, session);
    session->commit();
  }

  std::shared_ptr<Connection> input;
  std::shared_ptr<Connection> output;
  std::shared_ptr<core::ProcessGroup> root;
 private:
  static utils::Identifier& mergeProcUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& inputProcUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& inputConnUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}
  static utils::Identifier& outputConnUUID() {static auto id = utils::IdGenerator::getIdGenerator()->generate(); return id;}

  std::shared_ptr<core::Processor> inputProcessor;
  std::shared_ptr<core::Processor> merge;
  std::shared_ptr<core::repository::FlowFileRepository> ff_repository;
  std::shared_ptr<core::ContentRepository> content_repo;
  std::shared_ptr<core::Repository> prov_repo;
  std::shared_ptr<core::ProcessContext> inputContext;
  std::shared_ptr<core::ProcessContext> mergeContext;
};

TEST_CASE("Processors Can Store FlowFiles", "[TestP1]") {
  TestController testController;
  LogTestController::getInstance().setDebug<core::ContentRepository>();
  LogTestController::getInstance().setTrace<core::repository::FileSystemRepository>();
  LogTestController::getInstance().setTrace<minifi::ResourceClaim>();
  LogTestController::getInstance().setTrace<minifi::FlowFileRecord>();

  char format[] = "/tmp/test.XXXXXX";
  auto dir = testController.createTempDirectory(format);

  auto config = std::make_shared<minifi::Configure>();
  config->set(minifi::Configure::nifi_dbcontent_repository_directory_default, utils::file::FileUtils::concat_path(dir, "content_repository"));
  config->set(minifi::Configure::nifi_flowfile_repository_directory_default, utils::file::FileUtils::concat_path(dir, "flowfile_repository"));

  std::shared_ptr<core::Repository> prov_repo = std::make_shared<TestRepository>();
  std::shared_ptr<core::repository::FlowFileRepository> ff_repository = std::make_shared<core::repository::FlowFileRepository>("flowFileRepository");
  std::shared_ptr<core::ContentRepository> content_repo = std::make_shared<core::repository::FileSystemRepository>();
  ff_repository->initialize(config);
  content_repo->initialize(config);

  auto flowConfig = std::unique_ptr<core::FlowConfiguration>{new core::FlowConfiguration(prov_repo, ff_repository, content_repo, nullptr, config, "")};
  auto flowController = std::make_shared<minifi::FlowController>(prov_repo, ff_repository, config, std::move(flowConfig), content_repo, "", true);

  {
    TestFlow flow(ff_repository, content_repo, prov_repo);

    flowController->load(flow.root);
    ff_repository->start();

    // write two files into the input
    flow.write("one");
    flow.write("two");
    // capture them with the Merge Processor
    flow.trigger();
    flow.trigger();

    ff_repository->stop();
    flowController->unload();

    // check if the processor has taken ownership
    std::set<std::shared_ptr<core::FlowFile>> expired;
    auto file = flow.input->poll(expired);
    REQUIRE(!file);
    REQUIRE(expired.empty());

    file = flow.output->poll(expired);
    REQUIRE(!file);
    REQUIRE(expired.empty());
  }

  // swap the ProcessGroup and restart the FlowController
  {
    TestFlow flow(ff_repository, content_repo, prov_repo);

    flowController->load(flow.root);
    ff_repository->start();
    // wait for FlowFileRepository to start and notify the owners of
    // the resurrected FlowFiles
    std::this_thread::sleep_for(std::chrono::milliseconds{100});

    // write two third file into the input
    flow.write("three");

    flow.trigger();
    ff_repository->stop();
    flowController->unload();

    std::set<std::shared_ptr<core::FlowFile>> expired;
    auto file = flow.output->poll(expired);
    REQUIRE(file);
    REQUIRE(expired.empty());

    auto content = flow.read(file);
    auto isOneOfPossibleResults =
        Catch::Equals("_Header_one_Demarcator_two_Demarcator_three_Footer_")
        || Catch::Equals("_Header_two_Demarcator_one_Demarcator_three_Footer_");

    REQUIRE_THAT(content, isOneOfPossibleResults);
  }
}
