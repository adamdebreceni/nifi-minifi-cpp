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

#include "core/state/ProcessorController.h"
#include "core/ProcessContextBuilder.h"
#include "core/ProcessSessionFactory.h"
#include "core/repository/VolatileContentRepository.h"
#include "core/RepositoryFactory.h"
#include <memory>
#include <utility>
#include "processors/ProcessorUtils.h"

namespace org::apache::nifi::minifi::state {

ProcessorController::ProcessorController(core::Processor& processor, SchedulingAgent& scheduler)
    : processor_(&processor),
      scheduler_(&scheduler) {
}

ProcessorController::~ProcessorController() = default;
/**
 * Start the client
 */
int16_t ProcessorController::start() {
  processor_->setScheduledState(core::ScheduledState::RUNNING);
  scheduler_->schedule(processor_);
  return 0;
}
/**
 * Stop the client
 */
int16_t ProcessorController::stop() {
  scheduler_->unschedule(processor_);
  return 0;
}

bool ProcessorController::isRunning() const {
  return processor_->isRunning();
}

int16_t ProcessorController::pause() {
  return stop();
}

int16_t ProcessorController::resume() {
  return start();
}

namespace {

class SyntheticProcessSession : public core::ProcessSession {
 public:
  SyntheticProcessSession(std::shared_ptr<core::ProcessContext> process_context, std::deque<StateController::FlowFileData>& flow_files, ProcessorController::TriggerResult& result)
      : ProcessSession(process_context, nullptr), process_context_(process_context), flow_files_(flow_files), result_(result) {}

  std::shared_ptr<core::FlowFile> get() override {
    if (flow_files_.empty()) {
      return nullptr;
    }
    auto ff = std::make_shared<FlowFileRecord>();
    for (auto& [attr, val] : flow_files_.front().attributes) {
      ff->setAttribute(attr, val);
    }
    writeBuffer(ff, flow_files_.front().content);
    ff->setDeleted(false);
    std::shared_ptr<core::FlowFile> snapshot = std::make_shared<FlowFileRecord>();
    *snapshot = *ff;
    utils::Identifier uuid = ff->getUUID();
    updated_flowfiles_[uuid] = {ff, snapshot};
    auto flow_version = process_context_->getProcessorNode()->getFlowIdentifier();
    if (flow_version != nullptr) {
      ff->setAttribute(core::SpecialFlowAttribute::FLOW_ID, flow_version->getFlowId());
    }
    flow_files_.pop_front();
    ++result_.processed_input;
    return ff;
  }

  void transfer(const std::shared_ptr<core::FlowFile>& flow, const core::Relationship& relationship) override {
    ProcessSession::transfer(flow, relationship);
    StateController::FlowFileData data;
    for (auto& [attr, val] : flow->getAttributes()) {
      data.attributes[attr] = val;
    }
    auto read_result = readBuffer(flow);
    data.content = std::string{reinterpret_cast<const char*>(read_result.buffer.data()), read_result.buffer.size()};
    result_.output[relationship].push_back(std::move(data));
  }

  // explicit call of these is discouraged, but if these are called from onTrigger ignore them
  void commit() override {
    // pass
  }

  void rollback() override {
    // pass
  }

  std::shared_ptr<core::ProcessContext> process_context_;
  std::deque<StateController::FlowFileData>& flow_files_;
  ProcessorController::TriggerResult& result_;
};

}  // namespace

ProcessorController::RunResult ProcessorController::run(const std::optional<ProcessorState>& state, const std::vector<ProcessorController::TriggerInput>& triggers) {
  RunResult result;
  auto processor_class = processor_->getClassName();
  if (!processor_class) {
    result.schedule_error = "Cannot acquire processor type information";
    return result;
  }
  processor_class = utils::string::split(processor_class.value(), "::").back();
  auto processor_copy = processors::ProcessorUtils::createProcessor(processor_class.value(), processor_class.value(), processor_->getUUID());
  if (!processor_copy) {
    result.schedule_error = fmt::format("Failed to instanciate processor copy for '{}'", processor_class.value());
    return result;
  }
  processor_copy->copyPropertiesFrom(*processor_);
  auto process_context = scheduler_->buildProcessContext(processor_copy.get(), core::createContentRepository("volatilecontentrepository"), core::createRepository("NoOpRepository"), core::createRepository("NoOpRepository"));
  auto* state_manager = process_context->getStateManager();
  if (!state_manager && state) {
    result.schedule_error = "Explicit state was specified but no state manager available";
    return result;
  }
  if (state_manager) {
    state_manager->beginTransaction();
    if (state) {
      state_manager->set(state.value());
    }
  }
  auto state_manager_guard = gsl::finally([&] {if (state_manager) state_manager->rollback();});

  auto session_factory = std::make_shared<core::ProcessSessionFactory>(process_context);

  try {
    processor_copy->onSchedule(*process_context, *session_factory);
  } catch (std::exception &exception) {
    result.schedule_error = fmt::format("Caught Exception during onSchedule, type: {}, what: {}", typeid(exception).name(), exception.what());
    return result;
  } catch (...) {
    result.schedule_error = fmt::format("Caught Exception during onSchedule, type: {}", getCurrentExceptionTypeName());
    return result;
  }

  std::deque<ProcessorController::FlowFileData> flow_files;

  for (auto& trigger : triggers) {
    result.results.emplace_back();
    auto process_session = std::make_shared<SyntheticProcessSession>(process_context, flow_files, result.results.back());
    for (auto& ff : trigger.inputs) {
      flow_files.push_back(ff);
    }
    try {
      processor_copy->onTrigger(*process_context, *process_session);
      if (state_manager) {
        result.results.back().end_state = state_manager->get();
      }
    } catch (std::exception &exception) {
      result.trigger_error = fmt::format("Caught Exception during onTrigger, type: {}, what: {}", typeid(exception).name(), exception.what());
      if (state_manager) {
        result.results.back().end_state = state_manager->get();
      }
      return result;
    } catch (...) {
      result.trigger_error = fmt::format("Caught Exception during onSchedule, type: {}", getCurrentExceptionTypeName());
      if (state_manager) {
        result.results.back().end_state = state_manager->get();
      }
      return result;
    }
  }
  return result;
}

}  // namespace org::apache::nifi::minifi::state
