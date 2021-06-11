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
#ifndef LIBMINIFI_INCLUDE_CONTROLLERS_SSLCONTEXTSERVICE_H_
#define LIBMINIFI_INCLUDE_CONTROLLERS_SSLCONTEXTSERVICE_H_

#include <iostream>
#include <memory>
#include <string>
#include "core/Resource.h"
#include "utils/StringUtils.h"
#include "utils/tls/ExtendedKeyUsage.h"
#include "io/validation.h"
#include "../core/controller/ControllerService.h"
#include "core/logging/LoggerConfiguration.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

/**
 * SSLContextService provides a configurable controller service from
 * which we can provide an SSL Context or component parts that go
 * into creating one.
 *
 * Justification: Abstracts SSL support out of processors into a
 * configurable controller service.
 */
class SSLContextService : public core::controller::ControllerService {
 public:
  explicit SSLContextService(const std::string &name, const utils::Identifier &uuid = {});

  explicit SSLContextService(const std::string &name, const std::shared_ptr<Configure> &configuration);

  static std::shared_ptr<SSLContextService> make_shared(const std::string& name, const std::shared_ptr<Configure> &configuration);

  virtual void initialize();

  const std::string &getCertificateFile();

  const std::string &getPassphrase();

  const std::string &getPassphraseFile();

  const std::string &getPrivateKeyFile();

  const std::string &getCACertificate();

  void yield() {
  }

  bool isRunning() {
    return getState() == core::controller::ControllerServiceState::ENABLED;
  }

  bool isWorkAvailable() {
    return false;
  }

  virtual void onEnable();

  static const core::Property ClientCertificate;
  static const core::Property PrivateKey;
  static const core::Property Passphrase;
  static const core::Property CACertificate;
  static const core::Property UseSystemCertStore;
#ifdef WIN32
  static const core::Property CertStoreLocation;
  static const core::Property ServerCertStore;
  static const core::Property ClientCertStore;
  static const core::Property ClientCertCN;
  static const core::Property ClientCertKeyUsage;
#endif  // WIN32

 protected:
  virtual void initializeProperties();

  std::mutex initialization_mutex_;
  std::atomic<bool> initialized_;
  std::atomic<bool> valid_;
  std::string certificate_;
  std::string private_key_;
  std::string passphrase_;
  std::string passphrase_file_;
  std::string ca_certificate_;
  bool use_system_cert_store_ = false;
#ifdef WIN32
  std::string cert_store_location_;
  std::string server_cert_store_;
  std::string client_cert_store_;
  std::string client_cert_cn_;
  utils::tls::ExtendedKeyUsage client_cert_key_usage_;
#endif  // WIN32

  static bool isFileTypeP12(const std::string& filename) {
    return utils::StringUtils::endsWithIgnoreCase(filename, "p12");
  }

  std::shared_ptr<logging::Logger> logger_;
};
typedef int (SSLContextService::*ptr)(char *, int, int, void *);

#define REGISTER_SSL_CONTEXT_SERVICE(Clazz) \
  REGISTER_RESOURCE_AS(Clazz, SSLContextService, "Controller service that provides SSL/TLS capabilities to consuming interfaces")

#ifndef OPENSSL_SUPPORT
REGISTER_SSL_CONTEXT_SERVICE(SSLContextService);
#endif

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_CONTROLLERS_SSLCONTEXTSERVICE_H_
