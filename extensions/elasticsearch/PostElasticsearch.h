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

#include <string>
#include <memory>
#include <vector>

#include "controllers/SSLContextService.h"
#include "ElasticsearchCredentialsControllerService.h"
#include "core/Processor.h"
#include "core/PropertyDefinition.h"
#include "core/PropertyDefinitionBuilder.h"
#include "core/PropertyType.h"
#include "core/RelationshipDefinition.h"
#include "utils/Enum.h"
#include "http/HTTPClient.h"

namespace org::apache::nifi::minifi::extensions::elasticsearch {

class PostElasticsearch : public core::ProcessorImpl {
 public:
  EXTENSIONAPI static constexpr const char* Description = "An Elasticsearch/Opensearch post processor that uses the Elasticsearch/Opensearch _bulk REST API.";

  explicit PostElasticsearch(const std::string& name, const utils::Identifier& uuid = {})
      : ProcessorImpl(name, uuid) {
  }
  ~PostElasticsearch() override = default;

  EXTENSIONAPI static constexpr auto Action = core::PropertyDefinitionBuilder<>::createProperty("Action")
      .withDescription("The type of the operation used to index (create, delete, index, update, upsert)")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto MaxBatchSize = core::PropertyDefinitionBuilder<>::createProperty("Max Batch Size")
      .withDescription("The maximum number of flow files to process at a time.")
      .withPropertyType(core::StandardPropertyTypes::UNSIGNED_LONG_TYPE)
      .withDefaultValue("100")
      .build();
  EXTENSIONAPI static constexpr auto ElasticCredentials = core::PropertyDefinitionBuilder<>::createProperty("Elasticsearch Credentials Provider Service")
      .withDescription("The Controller Service used to obtain Elasticsearch credentials.")
      .isRequired(true)
      .withAllowedTypes<ElasticsearchCredentialsControllerService>()
      .build();
  EXTENSIONAPI static constexpr auto SSLContext = core::PropertyDefinitionBuilder<>::createProperty("SSL Context Service")
      .withDescription("The SSL Context Service used to provide client certificate "
          "information for TLS/SSL (https) connections.")
      .isRequired(false)
      .withAllowedTypes<minifi::controllers::SSLContextService>()
      .build();
  EXTENSIONAPI static constexpr auto Hosts = core::PropertyDefinitionBuilder<>::createProperty("Hosts")
      .withDescription("A comma-separated list of HTTP hosts that host Elasticsearch query nodes. Currently only supports a single host.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Index = core::PropertyDefinitionBuilder<>::createProperty("Index")
      .withDescription("The name of the index to use.")
      .supportsExpressionLanguage(true)
      .isRequired(true)
      .build();
  EXTENSIONAPI static constexpr auto Identifier = core::PropertyDefinitionBuilder<>::createProperty("Identifier")
      .withDescription("If the Action is \"index\" or \"create\", this property may be left empty or evaluate to an empty value, "
                      "in which case the document's identifier will be auto-generated by Elasticsearch. "
                      "For all other Actions, the attribute must evaluate to a non-empty value.")
      .supportsExpressionLanguage(true)
      .build();
  EXTENSIONAPI static constexpr auto Properties = std::to_array<core::PropertyReference>({
      Action,
      MaxBatchSize,
      ElasticCredentials,
      SSLContext,
      Hosts,
      Index,
      Identifier
  });

  EXTENSIONAPI static constexpr auto Success = core::RelationshipDefinition{"success", "All flowfiles that succeed in being transferred into Elasticsearch go here."};
  EXTENSIONAPI static constexpr auto Failure = core::RelationshipDefinition{"failure", "All flowfiles that fail for reasons unrelated to server availability go to this relationship."};
  EXTENSIONAPI static constexpr auto Error = core::RelationshipDefinition{"error", "All flowfiles that Elasticsearch responded to with an error go to this relationship."};
  EXTENSIONAPI static constexpr auto Relationships = std::array{Success, Failure, Error};

  EXTENSIONAPI static constexpr bool SupportsDynamicProperties = false;
  EXTENSIONAPI static constexpr bool SupportsDynamicRelationships = false;
  EXTENSIONAPI static constexpr core::annotation::Input InputRequirement = core::annotation::Input::INPUT_REQUIRED;
  EXTENSIONAPI static constexpr bool IsSingleThreaded = true;

  ADD_COMMON_VIRTUAL_FUNCTIONS_FOR_PROCESSORS

  void initialize() override;
  void onSchedule(core::ProcessContext& context, core::ProcessSessionFactory& session_factory) override;
  void onTrigger(core::ProcessContext& context, core::ProcessSession& session) override;

 private:
  std::string collectPayload(core::ProcessContext&, core::ProcessSession&, std::vector<std::shared_ptr<core::FlowFile>>&) const;

  uint64_t max_batch_size_ = 100;
  std::string host_url_;
  std::shared_ptr<ElasticsearchCredentialsControllerService> credentials_service_;
  http::HTTPClient client_;
  std::shared_ptr<core::logging::Logger> logger_ = core::logging::LoggerFactory<PostElasticsearch>::getLogger(uuid_);
};

}  // namespace org::apache::nifi::minifi::extensions::elasticsearch
