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

#include "controllers/SSLContextService.h"

#include <fstream>
#include <string>
#include <memory>
#include <set>

#include "core/Property.h"
#include "io/validation.h"
#include "properties/Configure.h"
#include "utils/gsl.h"
#include "utils/tls/CertificateUtils.h"
#include "utils/tls/TLSUtils.h"
#include "utils/tls/DistinguishedName.h"
#include "utils/tls/WindowsCertStoreLocation.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace controllers {

const core::Property SSLContextService::ClientCertificate(
    core::PropertyBuilder::createProperty("Client Certificate")
        ->withDescription("Client Certificate")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::PrivateKey(
    core::PropertyBuilder::createProperty("Private Key")
        ->withDescription("Private Key file")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::Passphrase(
    core::PropertyBuilder::createProperty("Passphrase")
        ->withDescription("Client passphrase. Either a file or unencrypted text")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::CACertificate(
    core::PropertyBuilder::createProperty("CA Certificate")
        ->withDescription("CA certificate file")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::UseSystemCertStore(
    core::PropertyBuilder::createProperty("Use System Cert Store")
        ->withDescription("Whether to use the certificates in the OS's certificate store")
        ->isRequired(false)
        ->withDefaultValue<bool>(false)
        ->build());

#ifdef WIN32
const core::Property SSLContextService::CertStoreLocation(
    core::PropertyBuilder::createProperty("Certificate Store Location")
        ->withDescription("One of the Windows certificate store locations, eg. LocalMachine or CurrentUser")
        ->withAllowableValues(utils::tls::WindowsCertStoreLocation::allowedLocations())
        ->isRequired(false)
        ->withDefaultValue(utils::tls::WindowsCertStoreLocation::defaultLocation())
        ->build());

const core::Property SSLContextService::ServerCertStore(
    core::PropertyBuilder::createProperty("Server Cert Store")
        ->withDescription("The name of the certificate store which contains the server certificate")
        ->isRequired(false)
        ->withDefaultValue("ROOT")
        ->build());

const core::Property SSLContextService::ClientCertStore(
    core::PropertyBuilder::createProperty("Client Cert Store")
        ->withDescription("The name of the certificate store which contains the client certificate")
        ->isRequired(false)
        ->withDefaultValue("MY")
        ->build());

const core::Property SSLContextService::ClientCertCN(
    core::PropertyBuilder::createProperty("Client Cert CN")
        ->withDescription("The CN that the client certificate is required to match; default: use the first available client certificate in the store")
        ->isRequired(false)
        ->build());

const core::Property SSLContextService::ClientCertKeyUsage(
    core::PropertyBuilder::createProperty("Client Cert Key Usage")
        ->withDescription("Comma-separated list of enhanced key usage values that the client certificate is required to have")
        ->isRequired(false)
        ->withDefaultValue("Client Authentication")
        ->build());
#endif  // WIN32

SSLContextService::SSLContextService(const std::string &name, const utils::Identifier &uuid)
    : ControllerService(name, uuid),
      initialized_(false),
      valid_(false),
      logger_(logging::LoggerFactory<SSLContextService>::getLogger()) {
}

SSLContextService::SSLContextService(const std::string &name, const std::shared_ptr<Configure> &configuration)
    : ControllerService(name),
      initialized_(false),
      valid_(false),
      logger_(logging::LoggerFactory<SSLContextService>::getLogger()) {
  setConfiguration(configuration);
  initialize();

  // set the properties based on the configuration
  std::string value;
  if (configuration_->get(Configure::nifi_security_client_certificate, value)) {
    setProperty(ClientCertificate.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_client_private_key, value)) {
    setProperty(PrivateKey.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_client_pass_phrase, value)) {
    setProperty(Passphrase.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_client_ca_certificate, value)) {
    setProperty(CACertificate.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_use_system_cert_store, value)) {
    setProperty(UseSystemCertStore.getName(), value);
  }

#ifdef WIN32
  if (configuration_->get(Configure::nifi_security_windows_cert_store_location, value)) {
    setProperty(CertStoreLocation.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_windows_server_cert_store, value)) {
    setProperty(ServerCertStore.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_windows_client_cert_store, value)) {
    setProperty(ClientCertStore.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_windows_client_cert_cn, value)) {
    setProperty(ClientCertCN.getName(), value);
  }

  if (configuration_->get(Configure::nifi_security_windows_client_cert_key_usage, value)) {
    setProperty(ClientCertKeyUsage.getName(), value);
  }
#endif  // WIN32
}

#ifndef OPENSSL_SUPPORT

std::shared_ptr<SSLContextService> SSLContextService::make_shared(const std::string& name, const std::shared_ptr<Configure> &configuration) {
  return std::make_shared<SSLContextService>(name, configuration);
}

#endif

void SSLContextService::initialize() {
  if (initialized_)
    return;

  std::lock_guard<std::mutex> lock(initialization_mutex_);

  ControllerService::initialize();

  initializeProperties();

  initialized_ = true;
}

const std::string &SSLContextService::getCertificateFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return certificate_;
}

const std::string &SSLContextService::getPassphrase() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return passphrase_;
}

const std::string &SSLContextService::getPassphraseFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return passphrase_file_;
}

const std::string &SSLContextService::getPrivateKeyFile() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return private_key_;
}

const std::string &SSLContextService::getCACertificate() {
  std::lock_guard<std::mutex> lock(initialization_mutex_);
  return ca_certificate_;
}

void SSLContextService::onEnable() {
  valid_ = true;
  std::string default_dir;

  if (nullptr != configuration_)
    configuration_->get(Configure::nifi_default_directory, default_dir);

  logger_->log_trace("onEnable()");

  bool has_certificate_property = getProperty(ClientCertificate.getName(), certificate_);
  if (has_certificate_property) {
    std::ifstream cert_file(certificate_);
    if (!cert_file.good()) {
      logger_->log_warn("Cannot open certificate file %s", certificate_);
      std::string test_cert = default_dir + certificate_;
      std::ifstream cert_file_test(test_cert);
      if (cert_file_test.good()) {
        certificate_ = test_cert;
        logger_->log_info("Using certificate file %s", certificate_);
      } else {
        logger_->log_error("Cannot open certificate file %s", test_cert);
        valid_ = false;
      }
      cert_file_test.close();
    }
    cert_file.close();
  } else {
    logger_->log_debug("Certificate empty");
  }

  if (has_certificate_property && !isFileTypeP12(certificate_)) {
    if (getProperty(PrivateKey.getName(), private_key_)) {
      std::ifstream priv_file(private_key_);
      if (!priv_file.good()) {
        logger_->log_warn("Cannot open private key file %s", private_key_);
        std::string test_priv = default_dir + private_key_;
        std::ifstream private_file_test(test_priv);
        if (private_file_test.good()) {
          private_key_ = test_priv;
          logger_->log_info("Using private key file %s", private_key_);
        } else {
          logger_->log_error("Cannot open private key file %s", test_priv);
          valid_ = false;
        }
        private_file_test.close();
      }
      priv_file.close();
    } else {
      logger_->log_debug("Private key empty");
    }
  }

  if (!getProperty(Passphrase.getName(), passphrase_)) {
    logger_->log_debug("No pass phrase for %s", certificate_);
  } else {
    std::ifstream passphrase_file(passphrase_);
    if (passphrase_file.good()) {
      passphrase_file_ = passphrase_;
      // we should read it from the file
      passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file)), std::istreambuf_iterator<char>());
    } else {
      std::string test_passphrase = default_dir + passphrase_;
      std::ifstream passphrase_file_test(test_passphrase);
      if (passphrase_file_test.good()) {
        passphrase_ = test_passphrase;
        passphrase_file_ = test_passphrase;
        passphrase_.assign((std::istreambuf_iterator<char>(passphrase_file_test)), std::istreambuf_iterator<char>());
      } else {
        // not an invalid file since we support a passphrase of unencrypted text
      }
      passphrase_file_test.close();
    }
    passphrase_file.close();
  }

  if (getProperty(CACertificate.getName(), ca_certificate_)) {
    std::ifstream cert_file(ca_certificate_);
    if (!cert_file.good()) {
      std::string test_ca_cert = default_dir + ca_certificate_;
      std::ifstream ca_cert_file_file_test(test_ca_cert);
      if (ca_cert_file_file_test.good()) {
        ca_certificate_ = test_ca_cert;
      } else {
        valid_ = false;
      }
      ca_cert_file_file_test.close();
    }
    cert_file.close();
  }

  getProperty(UseSystemCertStore.getName(), use_system_cert_store_);

#ifdef WIN32
  getProperty(CertStoreLocation.getName(), cert_store_location_);
  getProperty(ServerCertStore.getName(), server_cert_store_);
  getProperty(ClientCertStore.getName(), client_cert_store_);
  getProperty(ClientCertCN.getName(), client_cert_cn_);

  std::string client_cert_key_usage;
  getProperty(ClientCertKeyUsage.getName(), client_cert_key_usage);
  client_cert_key_usage_ = utils::tls::ExtendedKeyUsage{client_cert_key_usage};
#endif  // WIN32
}

void SSLContextService::initializeProperties() {
  std::set<core::Property> supportedProperties;
  supportedProperties.insert(ClientCertificate);
  supportedProperties.insert(PrivateKey);
  supportedProperties.insert(Passphrase);
  supportedProperties.insert(CACertificate);
  supportedProperties.insert(UseSystemCertStore);
#ifdef WIN32
  supportedProperties.insert(CertStoreLocation);
  supportedProperties.insert(ServerCertStore);
  supportedProperties.insert(ClientCertStore);
  supportedProperties.insert(ClientCertCN);
  supportedProperties.insert(ClientCertKeyUsage);
#endif  // WIN32
  setSupportedProperties(supportedProperties);
}

} /* namespace controllers */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
