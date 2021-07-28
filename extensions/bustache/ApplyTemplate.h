/**
 * @file ApplyTemplate.h
 * ApplyTemplate class declaration
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

#include <memory>
#include <string>

#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/FlowFile.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

/**
 * Applies a mustache template using incoming attributes as template parameters.
 */
class ApplyTemplate : public core::Processor {
 public:
  /*!
   * Create a new processor
   */
  explicit ApplyTemplate(const std::string& name, const utils::Identifier& uuid = {})
      : Processor(name, uuid),
        logger_(logging::LoggerFactory<ApplyTemplate>::getLogger()) {}
  ~ApplyTemplate() = default;
  static constexpr char const *ProcessorName = "ApplyTemplate";

  //! Supported Properties
  static core::Property Template;

  //! Supported Relationships
  static core::Relationship Success;

  void onTrigger(const std::shared_ptr<core::ProcessContext> &context,
                 const std::shared_ptr<core::ProcessSession> &session);
  void initialize(void);

  //! Write callback for outputting files generated by applying template to input
  class WriteCallback : public OutputStreamCallback {
   public:
    WriteCallback(const std::string &templateFile, const std::shared_ptr<core::FlowFile> &flow_file);
    int64_t process(const std::shared_ptr<io::BaseStream>& stream);

   private:
    std::shared_ptr<logging::Logger> logger_;
    std::string template_file_;
    std::shared_ptr<core::FlowFile> flow_file_;
  };

 private:
  std::shared_ptr<logging::Logger> logger_;
};

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
