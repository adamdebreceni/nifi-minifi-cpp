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
#include <mutex>
#include "properties/Properties.h"
#include "utils/OptionalUtils.h"
#include "properties/AgentProperty.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {

// TODO(adebreceni): eliminate this class in a separate PR
class Configuration : public Properties {
 public:
  Configuration() : Properties("MiNiFi configuration") {}

  // nifi.flow.configuration.file
  static inline const AgentProperty nifi_default_directory{"nifi.default.directory"};
  static inline const AgentProperty nifi_flow_configuration_file{"nifi.flow.configuration.file"};
  static inline const AgentProperty nifi_flow_configuration_encrypt{"nifi.flow.configuration.encrypt"};
  static inline const AgentProperty nifi_flow_configuration_file_exit_failure{"nifi.flow.configuration.file.exit.onfailure"};
  static inline const AgentProperty nifi_flow_configuration_file_backup_update{"nifi.flow.configuration.backup.on.update"};
  static inline const AgentProperty nifi_flow_engine_threads{"nifi.flow.engine.threads"};
  static inline const AgentProperty nifi_flow_engine_alert_period{"nifi.flow.engine.alert.period"};
  static inline const AgentProperty nifi_flow_engine_event_driven_time_slice{"nifi.flow.engine.event.driven.time.slice"};
  static inline const AgentProperty nifi_administrative_yield_duration{"nifi.administrative.yield.duration"};
  static inline const AgentProperty nifi_bored_yield_duration{"nifi.bored.yield.duration"};
  static inline const AgentProperty nifi_graceful_shutdown_seconds{"nifi.flowcontroller.graceful.shutdown.period"};
  static inline const AgentProperty nifi_flowcontroller_drain_timeout{"nifi.flowcontroller.drain.timeout"};
  static inline const AgentProperty nifi_log_level{"nifi.log.level"};
  static inline const AgentProperty nifi_server_name{"nifi.server.name"};
  static inline const AgentProperty nifi_configuration_class_name{"nifi.flow.configuration.class.name"};
  static inline const AgentProperty nifi_flow_repository_class_name{"nifi.flowfile.repository.class.name"};
  static inline const AgentProperty nifi_content_repository_class_name{"nifi.content.repository.class.name"};
  static inline const AgentProperty nifi_volatile_repository_options{"nifi.volatile.repository.options."};
  static inline const AgentProperty nifi_provenance_repository_class_name{"nifi.provenance.repository.class.name"};
  static inline const AgentProperty nifi_server_port{"nifi.server.port"};
  static inline const AgentProperty nifi_server_report_interval{"nifi.server.report.interval"};
  static inline const AgentProperty nifi_provenance_repository_max_storage_size{"nifi.provenance.repository.max.storage.size"};
  static inline const AgentProperty nifi_provenance_repository_max_storage_time{"nifi.provenance.repository.max.storage.time"};
  static inline const AgentProperty nifi_provenance_repository_directory_default{"nifi.provenance.repository.directory.default"};
  static inline const AgentProperty nifi_flowfile_repository_max_storage_size{"nifi.flowfile.repository.max.storage.size"};
  static inline const AgentProperty nifi_flowfile_repository_max_storage_time{"nifi.flowfile.repository.max.storage.time"};
  static inline const AgentProperty nifi_flowfile_repository_directory_default{"nifi.flowfile.repository.directory.default"};
  static inline const AgentProperty nifi_dbcontent_repository_directory_default{"nifi.database.content.repository.directory.default"};
  static inline const AgentProperty nifi_remote_input_secure{"nifi.remote.input.secure"};
  static inline const AgentProperty nifi_remote_input_http{"nifi.remote.input.http.enabled"};
  static inline const AgentProperty nifi_security_need_ClientAuth{"nifi.security.need.ClientAuth"};
  // site2site security config
  static inline const AgentProperty nifi_security_client_certificate{"nifi.security.client.certificate"};
  static inline const AgentProperty nifi_security_client_private_key{"nifi.security.client.private.key"};
  static inline const AgentProperty nifi_security_client_pass_phrase{"nifi.security.client.pass.phrase"};
  static inline const AgentProperty nifi_security_client_ca_certificate{"nifi.security.client.ca.certificate"};
  static inline const AgentProperty nifi_security_use_system_cert_store{"nifi.security.use.system.cert.store"};
  static inline const AgentProperty nifi_security_windows_cert_store_location{"nifi.security.windows.cert.store.location"};
  static inline const AgentProperty nifi_security_windows_server_cert_store{"nifi.security.windows.server.cert.store"};
  static inline const AgentProperty nifi_security_windows_client_cert_store{"nifi.security.windows.client.cert.store"};
  static inline const AgentProperty nifi_security_windows_client_cert_cn{"nifi.security.windows.client.cert.cn"};
  static inline const AgentProperty nifi_security_windows_client_cert_key_usage{"nifi.security.windows.client.cert.key.usage"};

  // nifi rest api user name and password
  static inline const AgentProperty nifi_rest_api_user_name{"nifi.rest.api.user.name"};
  static inline const AgentProperty nifi_rest_api_password{"nifi.rest.api.password"};
  // c2 options
  static inline const AgentProperty nifi_c2_enable{"nifi.c2.enable"};
  static inline const AgentProperty nifi_c2_file_watch{"nifi.c2.file.watch"};
  static inline const AgentProperty nifi_c2_flow_id{"nifi.c2.flow.id"};
  static inline const AgentProperty nifi_c2_flow_url{"nifi.c2.flow.url"};
  static inline const AgentProperty nifi_c2_flow_base_url{"nifi.c2.flow.base.url"};
  static inline const AgentProperty nifi_c2_full_heartbeat{"nifi.c2.full.heartbeat"};
  static inline const AgentProperty nifi_c2_root_classes{"nifi.c2.root.classes"};
  static inline const AgentProperty nifi_c2_root_class_definitions{"nifi.c2.root.class.definitions"};
  static inline const AgentProperty nifi_c2_agent_heartbeat_period{"nifi.c2.agent.heartbeat.period"};
  static inline const AgentProperty nifi_c2_agent_class{"nifi.c2.agent.class"};
  static inline const AgentProperty nifi_c2_agent_identifier{"nifi.c2.agent.identifier"};
  static inline const AgentProperty nifi_c2_agent_protocol_class{"nifi.c2.agent.protocol.class"};
  static inline const AgentProperty nifi_c2_agent_heartbeat_reporter_classes{"nifi.c2.agent.heartbeat.reporter.classes"};
  // c2 mqtt
  static inline const AgentProperty nifi_c2_mqtt_connector_service{"nifi.c2.mqtt.connector.service"};
  static inline const AgentProperty nifi_c2_mqtt_heartbeat_topic{"nifi.c2.mqtt.heartbeat.topic"};
  static inline const AgentProperty nifi_c2_mqtt_update_topic{"nifi.c2.mqtt.update.topic"};
  // c2 rest
  static inline const AgentProperty nifi_c2_rest_url{"nifi.c2.rest.url"};
  static inline const AgentProperty nifi_c2_rest_url_ack{"nifi.c2.rest.url.ack"};
  static inline const AgentProperty nifi_c2_rest_ssl_context_service{"nifi.c2.rest.ssl.context.service"};
  static inline const AgentProperty nifi_c2_rest_heartbeat_minimize_updates{"nifi.c2.rest.heartbeat.minimize.updates"};
  // c2 coap
  static inline const AgentProperty nifi_c2_agent_coap_host{"nifi.c2.agent.coap.host"};
  static inline const AgentProperty nifi_c2_agent_coap_port{"nifi.c2.agent.coap.port"};

  // state management options
  static inline const AgentProperty nifi_state_management_provider_local{"nifi.state.management.provider.local"};
  static inline const AgentProperty nifi_state_management_provider_local_class_name{"nifi.state.management.provider.local.class.name"};
  static inline const AgentProperty nifi_state_management_provider_local_always_persist{"nifi.state.management.provider.local.always.persist"};
  static inline const AgentProperty nifi_state_management_provider_local_auto_persistence_interval{"nifi.state.management.provider.local.auto.persistence.interval"};
  static inline const AgentProperty nifi_state_management_provider_local_path{"nifi.state.management.provider.local.path"};

  // disk space watchdog options
  static inline const AgentProperty minifi_disk_space_watchdog_enable{"minifi.disk.space.watchdog.enable"};
  static inline const AgentProperty minifi_disk_space_watchdog_interval{"minifi.disk.space.watchdog.interval"};
  static inline const AgentProperty minifi_disk_space_watchdog_stop_threshold{"minifi.disk.space.watchdog.stop.threshold"};
  static inline const AgentProperty minifi_disk_space_watchdog_restart_threshold{"minifi.disk.space.watchdog.restart.threshold"};
};

}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
