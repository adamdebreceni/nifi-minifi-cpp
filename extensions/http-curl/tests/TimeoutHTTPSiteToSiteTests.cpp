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

#define CURLOPT_SSL_VERIFYPEER_DISABLE 1
#include <sys/stat.h>
#undef NDEBUG
#include <cassert>
#include <utility>
#include <chrono>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include <sstream>
#include "HTTPClient.h"
#include "CivetServer.h"
#include "sitetosite/HTTPProtocol.h"
#include "InvokeHTTP.h"
#include "TestBase.h"
#include "utils/StringUtils.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/ProcessGroup.h"
#include "core/yaml/YamlConfiguration.h"
#include "FlowController.h"
#include "properties/Configure.h"
#include "io/StreamFactory.h"
#include "RemoteProcessorGroupPort.h"
#include "core/ConfigurableComponent.h"
#include "TestServer.h"
#include "HTTPIntegrationBase.h"
#include "HTTPHandlers.h"
#include "client/HTTPStream.h"

class SiteToSiteTestHarness : public CoapIntegrationBase {
public:
  explicit SiteToSiteTestHarness(bool isSecure, std::chrono::milliseconds waitTime = std::chrono::milliseconds{1000})
      : CoapIntegrationBase(waitTime.count()), isSecure(isSecure) {
    char format[] = "/tmp/ssth.XXXXXX";
    dir = testController.createTempDirectory(format);
  }

  void testSetup() override {
    LogTestController::getInstance().setTrace<minifi::RemoteProcessorGroupPort>();
    LogTestController::getInstance().setTrace<minifi::sitetosite::HttpSiteToSiteClient>();
    LogTestController::getInstance().setTrace<minifi::sitetosite::SiteToSiteClient>();
    LogTestController::getInstance().setTrace<utils::HTTPClient>();
    LogTestController::getInstance().setTrace<minifi::controllers::SSLContextService>();
    LogTestController::getInstance().setInfo<minifi::FlowController>();
    LogTestController::getInstance().setDebug<core::ConfigurableComponent>();
    LogTestController::getInstance().setTrace<utils::HttpStreamingCallback>();

    std::fstream file;
    ss << dir << utils::file::FileUtils::get_separator() << "tstFile.ext";
    file.open(ss.str(), std::ios::out);
    file << "tempFile";
    file.close();

    configuration->set("nifi.c2.enable", "false");
    configuration->set("nifi.remote.input.http.enabled", "true");
    configuration->set("nifi.remote.input.socket.port", "8099");
  }

  void cleanup() override {}

  void runAssertions() override {}

protected:
  bool isSecure;
  std::string dir;
  std::stringstream ss;
  TestController testController;
};

struct defaulted_handler{
  ServerAwareHandler* handler = nullptr;
  ServerAwareHandler* get(ServerAwareHandler *def) const {
    if(handler)return handler;
    return def;
  }
  void set(std::vector<std::chrono::milliseconds> timeout) {
    handler = new TimeoutingHTTPHandler(timeout);
  }
};

/**
 * Determines which responders will timeout
 */
struct timeout_test_profile{
  defaulted_handler base_;
  defaulted_handler transaction_;
  defaulted_handler flow_;
  defaulted_handler peer_;
  defaulted_handler delete_;
};

void run_timeout_variance(std::string test_file_location, bool isSecure, std::string url, const timeout_test_profile &profile) {
  SiteToSiteTestHarness harness(isSecure);

  std::string in_port = "471deef6-2a6e-4a7d-912a-81cc17e3a204";

  TransactionResponder *transaction_response = new TransactionResponder(url, in_port, true);

  std::string transaction_id = transaction_response->getTransactionId();

  harness.setKeyDir("");

  std::string baseUrl = url + "/site-to-site";
  SiteToSiteBaseResponder *base = new SiteToSiteBaseResponder(baseUrl);

  harness.setUrl(baseUrl, profile.base_.get(base));

  std::string transaction_url = url + "/data-transfer/input-ports/" + in_port + "/transactions";
  std::string action_url = url + "/site-to-site/input-ports/" + in_port + "/transactions";

  harness.setUrl(transaction_url, profile.transaction_.get(transaction_response));

  std::string peer_url = url + "/site-to-site/peers";

  PeerResponder *peer_response = new PeerResponder(url);

  harness.setUrl(peer_url, profile.peer_.get(peer_response));

  std::string flow_url = action_url + "/" + transaction_id + "/flow-files";

  FlowFileResponder *flowResponder = new FlowFileResponder(true);
  flowResponder->setFlowUrl(flow_url);

  harness.setUrl(flow_url, profile.flow_.get(flowResponder));

  std::string delete_url = transaction_url + "/" + transaction_id;
  DeleteTransactionResponder *deleteResponse = new DeleteTransactionResponder(delete_url, "201 OK", 12);
  harness.setUrl(delete_url, profile.delete_.get(deleteResponse));

  harness.run(test_file_location);

  assert(LogTestController::getInstance().contains("limit (200ms) reached, terminating connection") == true);

  LogTestController::getInstance().reset();
}

int main(int argc, char **argv) {
  transaction_id = 0;
  transaction_id_output = 0;
  std::string key_dir, test_file_location, url;
  if (argc > 1) {
    test_file_location = argv[1];
    key_dir = argv[2];
    url = argv[3];
  }

  bool isSecure = false;
  if (url.find("https") != std::string::npos) {
    isSecure = true;
  }

#ifdef WIN32
  if (url.find("localhost") != std::string::npos) {
	  std::string port, scheme, path;
	  parse_http_components(url, port, scheme, path);
	  url = scheme + "://" + org::apache::nifi::minifi::io::Socket::getMyHostName() + ":" + port +  path;
  }
#endif

  const auto timeout = std::chrono::milliseconds{500};

  {
    timeout_test_profile profile;
    profile.base_.set({timeout});
    run_timeout_variance(test_file_location, isSecure, url, profile);
  }

  {
    timeout_test_profile profile;
    profile.flow_.set({timeout});
    run_timeout_variance(test_file_location, isSecure, url, profile);
  }

  {
    timeout_test_profile profile;
    profile.transaction_.set({timeout});
    run_timeout_variance(test_file_location, isSecure, url, profile);
  }

  {
    timeout_test_profile profile;
    profile.delete_.set({timeout});
    run_timeout_variance(test_file_location, isSecure, url, profile);
  }

  {
    timeout_test_profile profile;
    profile.peer_.set({timeout});
    run_timeout_variance(test_file_location, isSecure, url, profile);
  }

  return 0;
}
