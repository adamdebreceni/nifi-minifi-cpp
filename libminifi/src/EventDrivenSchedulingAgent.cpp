/**
 * @file EventDrivenSchedulingAgent.cpp
 * EventDrivenSchedulingAgent class implementation
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
#include "EventDrivenSchedulingAgent.h"
#include <chrono>
#include "core/Processor.h"
#include "core/ProcessContext.h"
#include "core/ProcessSessionFactory.h"
#include "core/Property.h"

using namespace std::literals::chrono_literals;

namespace org::apache::nifi::minifi {

void EventDrivenSchedulingAgent::schedule(core::Processor* processor) {
  if (!processor->hasIncomingConnections()) {
    throw Exception(PROCESS_SCHEDULE_EXCEPTION, "EventDrivenSchedulingAgent cannot schedule processor without incoming connection!");
  }
  ThreadedSchedulingAgent::schedule(processor);
}

utils::TaskRescheduleInfo EventDrivenSchedulingAgent::run(core::Processor* processor, const std::shared_ptr<core::ProcessContext> &processContext,
                                         const std::shared_ptr<core::ProcessSessionFactory> &sessionFactory) {
  if (this->running_) {
    auto start_time = std::chrono::steady_clock::now();
    // trigger processor until it has work to do, but no more than the configured nifi.flow.engine.event.driven.time.slice
    while (processor->isRunning() && (std::chrono::steady_clock::now() - start_time < time_slice_)) {
      this->onTrigger(processor, processContext, sessionFactory);
      if (processor->isYield()) {
        return utils::TaskRescheduleInfo::RetryIn(processor->getYieldTime());
      }
    }
    return utils::TaskRescheduleInfo::RetryImmediately();  // Let's continue work as soon as a thread is available
  }
  return utils::TaskRescheduleInfo::Done();
}

}  // namespace org::apache::nifi::minifi
