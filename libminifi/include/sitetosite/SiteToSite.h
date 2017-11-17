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
#ifndef LIBMINIFI_INCLUDE_CORE_SITETOSITE_SITETOSITE_H_
#define LIBMINIFI_INCLUDE_CORE_SITETOSITE_SITETOSITE_H_

#include "controllers/SSLContextService.h"
#include "Peer.h"
#include "core/Property.h"
#include "properties/Configure.h"
#include "io/CRCStream.h"
#include "io/StreamFactory.h"
#include "utils/Id.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace sitetosite {

// Resource Negotiated Status Code
#define RESOURCE_OK 20
#define DIFFERENT_RESOURCE_VERSION 21
#define NEGOTIATED_ABORT 255
// ! Max attributes
#define MAX_NUM_ATTRIBUTES 25000

// Respond Code Sequence Pattern
static const uint8_t CODE_SEQUENCE_VALUE_1 = (uint8_t) 'R';
static const uint8_t CODE_SEQUENCE_VALUE_2 = (uint8_t) 'C';

/**
 * Enumeration of Properties that can be used for the Site-to-Site Socket
 * Protocol.
 */
typedef enum {
  /**
   * Boolean value indicating whether or not the contents of a FlowFile should
   * be GZipped when transferred.
   */
  GZIP,
  /**
   * The unique identifier of the port to communicate with
   */
  PORT_IDENTIFIER,
  /**
   * Indicates the number of milliseconds after the request was made that the
   * client will wait for a response. If no response has been received by the
   * time this value expires, the server can move on without attempting to
   * service the request because the client will have already disconnected.
   */
  REQUEST_EXPIRATION_MILLIS,
  /**
   * The preferred number of FlowFiles that the server should send to the
   * client when pulling data. This property was introduced in version 5 of
   * the protocol.
   */
  BATCH_COUNT,
  /**
   * The preferred number of bytes that the server should send to the client
   * when pulling data. This property was introduced in version 5 of the
   * protocol.
   */
  BATCH_SIZE,
  /**
   * The preferred amount of time that the server should send data to the
   * client when pulling data. This property was introduced in version 5 of
   * the protocol. Value is in milliseconds.
   */
  BATCH_DURATION,
  MAX_HANDSHAKE_PROPERTY
} HandshakeProperty;

// HandShakeProperty Str
static const char *HandShakePropertyStr[MAX_HANDSHAKE_PROPERTY] = {
/**
 * Boolean value indicating whether or not the contents of a FlowFile should
 * be GZipped when transferred.
 */
"GZIP",
/**
 * The unique identifier of the port to communicate with
 */
"PORT_IDENTIFIER",
/**
 * Indicates the number of milliseconds after the request was made that the
 * client will wait for a response. If no response has been received by the
 * time this value expires, the server can move on without attempting to
 * service the request because the client will have already disconnected.
 */
"REQUEST_EXPIRATION_MILLIS",
/**
 * The preferred number of FlowFiles that the server should send to the
 * client when pulling data. This property was introduced in version 5 of
 * the protocol.
 */
"BATCH_COUNT",
/**
 * The preferred number of bytes that the server should send to the client
 * when pulling data. This property was introduced in version 5 of the
 * protocol.
 */
"BATCH_SIZE",
/**
 * The preferred amount of time that the server should send data to the
 * client when pulling data. This property was introduced in version 5 of
 * the protocol. Value is in milliseconds.
 */
"BATCH_DURATION" };

typedef enum {
  RAW,
  HTTP
} CLIENT_TYPE;

/**
 * An enumeration for specifying the direction in which data should be
 * transferred between a client and a remote NiFi instance.
 */
typedef enum {
  /**
   * * The client is to send data to the remote instance.
   * */
  SEND,
  /**
   * * The client is to receive data from the remote instance.
   * */
  RECEIVE
} TransferDirection;

// Peer State
typedef enum {
  /**
   * * IDLE
   * */
  IDLE = 0,
  /**
   * * Socket Established
   * */
  ESTABLISHED,
  /**
   * * HandShake Done
   * */
  HANDSHAKED,
  /**
   * * After CodeDec Completion
   * */
  READY
} PeerState;

// Transaction State
typedef enum {
  /**
   * * Transaction has been started but no data has been sent or received.
   * */
  TRANSACTION_STARTED,
  /**
   * * Transaction has been started and data has been sent or received.
   * */
  DATA_EXCHANGED,
  /**
   * * Data that has been transferred has been confirmed via its CRC.
   * * Transaction is ready to be completed.
   * */
  TRANSACTION_CONFIRMED,
  /**
   * * Transaction has been successfully completed.
   * */
  TRANSACTION_COMPLETED,
  /**
   * * The Transaction has been canceled.
   * */
  TRANSACTION_CANCELED,

  /**
   * * Transaction has been successfully closed.
   * */
  TRANSACTION_CLOSED,
  /**
   * * The Transaction ended in an error.
   * */
  TRANSACTION_ERROR
} TransactionState;

// Request Type
typedef enum {
  NEGOTIATE_FLOWFILE_CODEC = 0,
  REQUEST_PEER_LIST,
  SEND_FLOWFILES,
  RECEIVE_FLOWFILES,
  SHUTDOWN,
  MAX_REQUEST_TYPE
} RequestType;

// Request Type Str
static const char *RequestTypeStr[MAX_REQUEST_TYPE] = { "NEGOTIATE_FLOWFILE_CODEC", "REQUEST_PEER_LIST", "SEND_FLOWFILES", "RECEIVE_FLOWFILES", "SHUTDOWN" };

// Respond Code
typedef enum {
  RESERVED = 0,
  // ResponseCode, so that we can indicate a 0 followed by some other bytes

  // handshaking properties
  PROPERTIES_OK = 1,
  UNKNOWN_PROPERTY_NAME = 230,
  ILLEGAL_PROPERTY_VALUE = 231,
  MISSING_PROPERTY = 232,
  // transaction indicators
  CONTINUE_TRANSACTION = 10,
  FINISH_TRANSACTION = 11,
  CONFIRM_TRANSACTION = 12,  // "Explanation" of this code is the checksum
  TRANSACTION_FINISHED = 13,
  TRANSACTION_FINISHED_BUT_DESTINATION_FULL = 14,
  CANCEL_TRANSACTION = 15,
  BAD_CHECKSUM = 19,
  // data availability indicators
  MORE_DATA = 20,
  NO_MORE_DATA = 21,
  // port state indicators
  UNKNOWN_PORT = 200,
  PORT_NOT_IN_VALID_STATE = 201,
  PORTS_DESTINATION_FULL = 202,
  // authorization
  UNAUTHORIZED = 240,
  // error indicators
  ABORT = 250,
  UNRECOGNIZED_RESPONSE_CODE = 254,
  END_OF_STREAM = 255
} RespondCode;

// Respond Code Class
typedef struct {
  RespondCode code;
  const char *description;bool hasDescription;
} RespondCodeContext;

// Respond Code Context
static RespondCodeContext respondCodeContext[] = { { RESERVED, "Reserved for Future Use", false }, { PROPERTIES_OK, "Properties OK", false }, { UNKNOWN_PROPERTY_NAME, "Unknown Property Name", true },
    { ILLEGAL_PROPERTY_VALUE, "Illegal Property Value", true }, { MISSING_PROPERTY, "Missing Property", true }, { CONTINUE_TRANSACTION, "Continue Transaction", false }, { FINISH_TRANSACTION,
        "Finish Transaction", false }, { CONFIRM_TRANSACTION, "Confirm Transaction", true }, { TRANSACTION_FINISHED, "Transaction Finished", false }, { TRANSACTION_FINISHED_BUT_DESTINATION_FULL,
        "Transaction Finished But Destination is Full", false }, { CANCEL_TRANSACTION, "Cancel Transaction", true }, { BAD_CHECKSUM, "Bad Checksum", false }, { MORE_DATA, "More Data Exists", false },
    { NO_MORE_DATA, "No More Data Exists", false }, { UNKNOWN_PORT, "Unknown Port", false }, { PORT_NOT_IN_VALID_STATE, "Port Not in a Valid State", true }, { PORTS_DESTINATION_FULL,
        "Port's Destination is Full", false }, { UNAUTHORIZED, "User Not Authorized", true }, { ABORT, "Abort", true }, { UNRECOGNIZED_RESPONSE_CODE, "Unrecognized Response Code", false }, {
        END_OF_STREAM, "End of Stream", false } };

// Transaction Class
class Transaction {
 public:
  // Constructor
  /*!
   * Create a new transaction
   */
  explicit Transaction(TransferDirection direction, org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> &stream)
      : crcStream(std::move(stream)), closed_(false) {
    _state = TRANSACTION_STARTED;
    _direction = direction;
    _dataAvailable = false;
    current_transfers_ = 0;
    total_transfers_ = 0;
    _bytes = 0;

    char uuidStr[37];

    // Generate the global UUID for the transaction
    id_generator_->generate(uuid_);
    uuid_unparse_lower(uuid_, uuidStr);
    uuid_str_ = uuidStr;
  }
  // Destructor
  virtual ~Transaction() {
  }
  // getUUIDStr
  std::string getUUIDStr() {
    return uuid_str_;
  }

  void setTransactionId(const std::string str) {
    setUUIDStr(str);
  }

  void setUUIDStr(const std::string &str) {
    uuid_str_ = str;
    uuid_parse(str.c_str(), uuid_);
  }


  // getState
  TransactionState getState() {
    return _state;
  }
  // isDataAvailable
  bool isDataAvailable() {
    return _dataAvailable;
  }
  // setDataAvailable()
  void setDataAvailable(bool value) {
    _dataAvailable = value;
  }
  // getDirection
  TransferDirection getDirection() {
    return _direction;
  }
  // getCRC
  long getCRC() {
    return crcStream.getCRC();
  }
  // updateCRC
  void updateCRC(uint8_t *buffer, uint32_t length) {
    crcStream.updateCRC(buffer, length);
  }

  org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> &getStream() {
    return crcStream;
  }

  Transaction(const Transaction &parent) = delete;
  Transaction &operator=(const Transaction &parent) = delete;

  // Number of current transfers
  int current_transfers_;
  // number of total seen transfers
  int total_transfers_;

  // Number of content bytes
  uint64_t _bytes;

  // Transaction State
  TransactionState _state;

  bool closed_;

  // Whether received data is available
  bool _dataAvailable;

 protected:

  org::apache::nifi::minifi::io::CRCStream<SiteToSitePeer> crcStream;

 private:

  // Transaction Direction
  TransferDirection _direction;

  // A global unique identifier
  uuid_t uuid_;
  // UUID string
  std::string uuid_str_;

  static std::shared_ptr<utils::IdGenerator> id_generator_;
};

class SiteToSiteClientConfiguration {
 public:
  SiteToSiteClientConfiguration(std::shared_ptr<io::StreamFactory> stream_factory, const std::shared_ptr<Peer> &peer, CLIENT_TYPE type = RAW)
      : stream_factory_(stream_factory),
        peer_(peer),
        ssl_service_(nullptr) {
    client_type_ = type;
  }

  SiteToSiteClientConfiguration(const SiteToSiteClientConfiguration &other) = delete;

  CLIENT_TYPE getClientType() const {
    return client_type_;
  }

  const std::shared_ptr<Peer> &getPeer() const {
    return peer_;
  }

  void setSecurityContext(const std::shared_ptr<controllers::SSLContextService> &ssl_service) {
    ssl_service_ = ssl_service;
  }

  const std::shared_ptr<controllers::SSLContextService> &getSecurityContext() const {
    return ssl_service_;
  }

  const std::shared_ptr<io::StreamFactory> &getStreamFactory() const {
    return stream_factory_;
  }

 protected:

  std::shared_ptr<io::StreamFactory> stream_factory_;

  std::shared_ptr<Peer> peer_;

  CLIENT_TYPE client_type_;

  // secore comms

  std::shared_ptr<controllers::SSLContextService> ssl_service_;
};

} /* namespace sitetosite */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif /* LIBMINIFI_INCLUDE_CORE_SITETOSITE_SITETOSITE_H_ */