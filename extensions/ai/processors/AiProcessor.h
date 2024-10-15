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

#include "core/Processor.h"
#include "core/logging/LoggerFactory.h"
#include "core/PropertyDefinitionBuilder.h"
#include "llama.h"

namespace org::apache::nifi::minifi::processors {

class AiProcessor : public core::Processor {
 public:
  explicit AiProcessor(std::string_view name, const utils::Identifier& uuid = {})
      : core::Processor(name, uuid) {
  }
  ~AiProcessor() override = default;

  EXTENSIONAPI static constexpr const char* Description = "AI processor";

  EXTENSIONAPI static constexpr auto ModelName = core::PropertyDefinitionBuilder<>::createProperty("AI Model Name")
      .withDescription("The name of the AI model")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Prompt = core::PropertyDefinitionBuilder<>::createProperty("AI Prompt")
      .withDescription("The prompt for the AI model")
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
                                                                                             ModelName,
                                                                                             Prompt,
                                                                                         });


  EXTENSIONAPI static constexpr auto Malformed = core::RelationshipDefinition{"malformed", "Malformed output that could not be parsed"};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Malformed};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = true;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;
  void initialize() override;
  void notifyStop() override;

 private:
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<AiProcessor>::getLogger(uuid_);

  std::string model_name_;
  std::string prompt_;
  std::string full_prompt_;

  llama_model* llama_model_{nullptr};
  llama_context* llama_ctx_{nullptr};
};

}  // namespace org::apache::nifi::minifi::processors
