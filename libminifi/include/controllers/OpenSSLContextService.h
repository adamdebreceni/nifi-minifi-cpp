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

#ifdef OPENSSL_SUPPORT

#pragma once

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

#include <openssl/err.h>
#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/pkcs12.h>
#include <iostream>
#include <memory>
#include <string>
#include "SSLContextService.h"

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
class OpenSSLContextService : public SSLContextService {
 public:
  using SSLContextService::SSLContextService;

  bool configure_ssl_context(SSL_CTX *ctx);

 protected:
  static std::string getLatestOpenSSLErrorString();

 private:
  bool addP12CertificateToSSLContext(SSL_CTX* ctx) const;
  bool addPemCertificateToSSLContext(SSL_CTX* ctx) const;
  bool addClientCertificateFromSystemStoreToSSLContext(SSL_CTX* ctx) const;
  bool addServerCertificatesFromSystemStoreToSSLContext(SSL_CTX* ctx) const;
#ifdef WIN32
  bool useClientCertificate(SSL_CTX* ctx, PCCERT_CONTEXT certificate) const;
  void addServerCertificateToSSLStore(X509_STORE* ssl_store, PCCERT_CONTEXT certificate) const;
#endif  // WIN32
};

REGISTER_SSL_CONTEXT_SERVICE(OpenSSLContextService);

}  // namespace controllers
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // OPENSSL_SUPPORT
