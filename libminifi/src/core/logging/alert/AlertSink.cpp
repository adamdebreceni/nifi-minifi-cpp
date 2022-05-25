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

#include "core/logging/alert/AlertSink.h"
#include "core/TypedValues.h"
#include "core/ClassLoader.h"
#include "utils/HTTPClient.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/writer.h"

using std::chrono_literals::operator""s;
using std::chrono_literals::operator""min;

namespace org::apache::nifi::minifi::core::logging {

AlertSink::AlertSink(const std::shared_ptr<LoggerProperties>& logger_properties, std::shared_ptr<Logger> logger): logger_(std::move(logger)) {
  Config config;
  if (auto url = logger_properties->getString("alert.url")) {
    config.url = url.value();
  } else {
    logger_->log_info("Missing 'alert.url' value, network logging won't be available");
    return;
  }
  if (!DataSizeValue::StringToInt(logger_properties->getString("alert.batch.size").value_or("100 KB"), config.batch_size)) {
    config.batch_size = 100_KiB;
    logger_->log_error("Invalid 'alert.batch.size' value, using default 100 KB");
  }
  if (auto period = TimePeriodValue::fromString(logger_properties->getString("alert.flush.period").value_or("5 s"))) {
    config.flush_period = period->getMilliseconds();
  } else {
    config.flush_period = 5s;
    logger_->log_error("Invalid 'alert.flush.period' value, using default 5 seconds");
  }

  if (auto rate = TimePeriodValue::fromString(logger_properties->getString("alert.rate.limit").value_or("10 min"))) {
    config.rate_limit = rate->getMilliseconds();
  } else {
    config.rate_limit = 10min;
    logger_->log_error("Invalid 'alert.rate.limit' value, using default 10 minutes");
  }

  if (!DataSizeValue::StringToInt(logger_properties->getString("alert.buffer.limit").value_or("1 MB"), config.buffer_limit)) {
    config.buffer_limit = 1_MiB;
    logger_->log_error("Invalid 'alert.buffer.limit' value, using default 1 MB");
  }

  if (auto filter_str = logger_properties->getString("alert.filter")) {
    try {
      config.filter = filter_str.value();
    } catch (const std::regex_error&) {
      logger_->log_error("Invalid 'alert.filter' value, network logging won't be available");
      return;
    }
  } else {
    logger_->log_error("Missing 'alert.filter' value, network logging won't be available");
    return;
  }

  config.ssl_service_name = logger_properties->getString("alert.ssl.context.service");

  config_ = std::move(config);

  task_id_ = utils::IdGenerator::getIdGenerator()->generate();
  utils::Worker<utils::TaskRescheduleInfo> functor(std::bind(&AlertSink::run, this), task_id_.to_string(), std::make_unique<utils::ComplexMonitor>());
  std::future<utils::TaskRescheduleInfo> future;
  thread_pool_.execute(std::move(functor), future);
}

void AlertSink::initialize(core::controller::ControllerServiceProvider* controller, std::shared_ptr<Configure> agent_config) {
  agent_config_ = std::move(agent_config);
  if (!config_ || thread_pool_.isRunning()) {
    return;
  }

  if (config_->ssl_service_name) {
    if (!controller) {
      logger_->log_error("Could not find service '%s': no service provider", config_->ssl_service_name.value());
      return;
    }
    if (auto service = controller->getControllerService(config_->ssl_service_name.value())) {
      if (auto ssl_service = std::dynamic_pointer_cast<controllers::SSLContextService>(service)) {
        ssl_service_ = ssl_service;
      } else {
        logger_->log_error("Service '%s' is not an SSLContextService", config_->ssl_service_name.value());
        return;
      }
    } else {
      logger_->log_error("Could not find service '%s'", config_->ssl_service_name.value());
      return;
    }
  }


  thread_pool_.start();
}

void AlertSink::sink_it_(const spdlog::details::log_msg& msg) {
  // this method is protected upstream in base_sink by a mutex
  if (!config_) return;

  auto now = std::chrono::steady_clock::now();
  auto limit = now - config_->rate_limit;
  while (!ordered_hashes_.empty() && ordered_hashes_.front().first < limit) {
    ignored_hashes_.erase(ordered_hashes_.front().second);
    ordered_hashes_.pop_front();
  }

  std::string_view payload(msg.payload.data(), msg.payload.size());
  if (!std::regex_match(payload.begin(), payload.end(), config_->filter)) {
    return;
  }
  size_t hash = std::hash<std::string_view>{}(payload);
  if (!ignored_hashes_.insert(hash).second) {
    return;
  }

  spdlog::memory_buf_t formatted;
  formatter_->format(msg, formatted);

  ordered_hashes_.emplace_back(now, hash);
  std::lock_guard<std::mutex> log_guard(log_mtx_);
  buffer_size_ += formatted.size();
  buffer_.emplace_back(std::string{formatted.data(), formatted.size()}, hash);
  while (!buffer_.empty() && buffer_size_ > config_->buffer_limit) {
    buffer_size_ -= buffer_.front().first.size();
    ignored_hashes_.erase(buffer_.front().second);
    buffer_.pop_front();
  }
}

void AlertSink::flush_() {}

utils::TaskRescheduleInfo AlertSink::run() {
  std::deque<std::pair<std::string, size_t>> logs;
  {
    std::lock_guard<std::mutex> log_guard(log_mtx_);
    if (buffer_.empty()) {
      return utils::TaskRescheduleInfo::RetryIn(config_->flush_period);
    }
    logs = std::move(buffer_);
    buffer_size_ = 0;
  }
  auto client = core::ClassLoader::getDefaultClassLoader().instantiate<utils::BaseHTTPClient>("HTTPClient", "HTTPClient");
  if (!client) {
    logger_->log_error("Could not instantiate a HTTPClient object");
    return utils::TaskRescheduleInfo::RetryIn(config_->flush_period);
  }
  client->initialize("PUT", config_->url, ssl_service_);

  rapidjson::Document doc(rapidjson::kObjectType);
  std::string agent_id = agent_config_->getAgentIdentifier();
  doc.AddMember("agentId", rapidjson::Value(agent_id.data(), agent_id.length()), doc.GetAllocator());
  doc.AddMember("alerts", rapidjson::Value(rapidjson::kArrayType), doc.GetAllocator());
  for (const auto& [log, hash] : logs) {
    doc["alerts"].PushBack(rapidjson::Value(log.data(), log.size()), doc.GetAllocator());
  }
  rapidjson::StringBuffer buffer;
  rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
  doc.Accept(writer);

  auto data_input = std::make_unique<utils::ByteInputCallback>();
  auto data_cb = std::make_unique<utils::HTTPUploadCallback>();
  data_input->write(std::string(buffer.GetString(), buffer.GetSize()));
  data_cb->ptr = data_input.get();
  client->setUploadCallback(data_cb.get());
  client->setContentType("application/json");

  client->submit();

  int64_t resp_code = client->getResponseCode();
  const bool clientError = 400 <= resp_code && resp_code < 500;
  const bool serverError = 500 <= resp_code && resp_code < 600;
  if (clientError || serverError) {
    logger_->log_error("Error response code '" "%" PRId64 "' from '%s'", resp_code, config_->url);
  } else {
    logger_->log_debug("Response code '" "%" PRId64 "' from '%s'", resp_code, config_->url);
  }

  return utils::TaskRescheduleInfo::RetryIn(config_->flush_period);
}

}  // namespace org::apache::nifi::minifi::core::logging
