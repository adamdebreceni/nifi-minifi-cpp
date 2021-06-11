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
#ifndef LIBMINIFI_INCLUDE_IO_CLIENTSOCKET_H_
#define LIBMINIFI_INCLUDE_IO_CLIENTSOCKET_H_

#include <utility>
#include <cstdint>
#include <mutex>
#include <atomic>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include "io/BaseStream.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "io/validation.h"
#include "properties/Configure.h"
#include "io/NetworkPrioritizer.h"
#include "utils/gsl.h"
#include "internal/SocketDescriptor.h"

struct addrinfo;
struct fd_set;

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

#ifdef WIN32
#else
#undef INVALID_SOCKET
static constexpr SocketDescriptor INVALID_SOCKET = -1;
#undef SOCKET_ERROR
static constexpr int SOCKET_ERROR = -1;
#endif /* WIN32 */

/**
 * Return the last socket error message, based on errno on posix and WSAGetLastError() on windows
 */
std::string get_last_socket_error_message();

/**
 * Context class for socket. This is currently only used as a parent class for TLSContext.  It is necessary so the Socket and TLSSocket constructors
 * can be the same.  It also gives us a common place to set timeouts, etc from the Configure object in the future.
 */
class SocketContext {
 public:
  SocketContext(const std::shared_ptr<Configure>& /*configure*/) { // NOLINT
  }
};
/**
 * Socket class.
 * Purpose: Provides a general purpose socket interface that abstracts
 * connecting information from users
 * Design: Extends DataStream and allows us to perform most streaming
 * operations against a BSD socket
 */
class Socket : public BaseStream {
  class FdSet {
   public:
    FdSet();
    FdSet(const FdSet&) = delete;
    FdSet& operator=(const FdSet&);
    FdSet(FdSet&& other) noexcept;
    FdSet& operator=(FdSet&& other) noexcept;
    ~FdSet();

    bool isSet(SocketDescriptor fd);
    void set(SocketDescriptor fd);
    void clear(SocketDescriptor fd);

    fd_set* get() {
      return impl_;
    }

   private:
    gsl::owner<fd_set*> impl_;
  };
 public:
  /**
   * Constructor that creates a client socket.
   * @param context the SocketContext
   * @param hostname hostname we are connecting to.
   * @param port port we are connecting to.
   */
  Socket(const std::shared_ptr<SocketContext> &context, std::string hostname, uint16_t port);

  Socket(const Socket&) = delete;
  Socket(Socket&&) noexcept;

  Socket& operator=(const Socket&) = delete;
  Socket& operator=(Socket&& other) noexcept;

  /**
   * Static function to return the current machine's host name
   */
  static std::string getMyHostName() {
    static const std::string HOSTNAME = init_hostname();
    return HOSTNAME;
  }

  /**
   * Destructor
   */

  ~Socket() override;

  void close() override;
  /**
   * Initializes the socket
   * @return result of the creation operation.
   */
  int initialize() override;

  virtual void setInterface(io::NetworkInterface interface) {
    local_network_interface_ = std::move(interface);
  }

  /**
   * Sets the non blocking flag on the file descriptor.
   */
  void setNonBlocking();

  std::string getHostname() const;

  /**
   * Return the port for this socket
   * @returns port
   */
  uint16_t getPort() const {
    return port_;
  }

  void setPort(uint16_t port) {
    port_ = port;
  }

  using BaseStream::write;
  using BaseStream::read;

  int write(const uint8_t *value, int size) override;

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   * @param retrieve_all_bytes determines if we should read all bytes before returning
   */
  int read(uint8_t *buf, int buflen) override {
    return read(buf, buflen, true);
  }

  /**
   * Reads data and places it into buf
   * @param buf buffer in which we extract data
   * @param buflen
   * @param retrieve_all_bytes determines if we should read all bytes before returning
   */
  virtual int read(uint8_t *buf, int buflen, bool retrieve_all_bytes);

 protected:
  /**
   * Constructor that accepts host name, port and listeners. With this
   * contructor we will be creating a server socket
   * @param context the SocketContext
   * @param hostname our host name
   * @param port connecting port
   * @param listeners number of listeners in the queue
   */
  explicit Socket(const std::shared_ptr<SocketContext> &context, std::string hostname, uint16_t port, uint16_t listeners);

  /**
   * Iterates through {@code destination_addresses} and tries to connect to each address until it succeeds.
   * Supports both IPv4 and IPv6.
   * @param destination_addresses Destination addresses, typically from {@code getaddrinfo}.
   * @return 0 on success, -1 on error
   */
  virtual int8_t createConnection(const addrinfo *destination_addresses);

  /**
   * Sets socket options depending on the instance.
   * @param sock socket file descriptor.
   */
  virtual int16_t setSocketOptions(SocketDescriptor sock);

  /**
   * Attempt to select the socket file descriptor
   * @param msec timeout interval to wait
   * @returns file descriptor
   */
  virtual int16_t select_descriptor(uint16_t msec);

  std::recursive_mutex selection_mutex_;

  std::string requested_hostname_;
  std::string canonical_hostname_;
  uint16_t port_{ 0 };

  bool is_loopback_only_{ false };
  io::NetworkInterface local_network_interface_;

  // connection information
  SocketDescriptor socket_file_descriptor_ = SocketDescriptor::Invalid;

  FdSet total_list_{};
  FdSet read_fds_{};
  std::atomic<uint16_t> socket_max_{ 0 };
  std::atomic<uint64_t> total_written_{ 0 };
  std::atomic<uint64_t> total_read_{ 0 };
  uint16_t listeners_{ 0 };

  bool nonBlocking_{ false };

  std::shared_ptr<logging::Logger> logger_;

 private:
  static void initialize_socket();

  static std::string init_hostname();
};

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_IO_CLIENTSOCKET_H_
