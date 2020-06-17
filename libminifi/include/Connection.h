/**
 * @file Connection.h
 * Connection class declaration
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
#ifndef LIBMINIFI_INCLUDE_CONNECTION_H_
#define LIBMINIFI_INCLUDE_CONNECTION_H_

#include <memory>
#include <set>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include <mutex>
#include <atomic>
#include <algorithm>
#include "core/Core.h"
#include "core/Connectable.h"
#include "core/logging/Logger.h"
#include "core/Relationship.h"
#include "core/FlowFile.h"
#include "core/Repository.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
// Connection Class

namespace core {
class ProcessSession;
}

class Connection : public core::Connectable, public std::enable_shared_from_this<Connection> {
  friend class Transaction;
 public:
  // Constructor
  /*
   * Create a new processor
   */
  explicit Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name);
  explicit Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name, utils::Identifier & uuid);
  explicit Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name, utils::Identifier & uuid,
                      utils::Identifier & srcUUID);
  explicit Connection(const std::shared_ptr<core::Repository> &flow_repository, const std::shared_ptr<core::ContentRepository> &content_repo, std::string name, utils::Identifier & uuid,
                      utils::Identifier & srcUUID, utils::Identifier & destUUID);
  // Destructor
  virtual ~Connection() {
  }

  // Set Source Processor UUID
  void setSourceUUID(utils::Identifier &uuid) {
    src_uuid_ = uuid;
  }
  // Set Destination Processor UUID
  void setDestinationUUID(utils::Identifier &uuid) {
    dest_uuid_ = uuid;
  }
  // Get Source Processor UUID
  void getSourceUUID(utils::Identifier &uuid) {
    uuid = src_uuid_;
  }
  // Get Destination Processor UUID
  void getDestinationUUID(utils::Identifier &uuid) {
    uuid = dest_uuid_;
  }

  // Set Connection Source Processor
  void setSource(std::shared_ptr<core::Connectable> source) {
    source_connectable_ = source;
  }
  // ! Get Connection Source Processor
  std::shared_ptr<core::Connectable> getSource() {
    return source_connectable_;
  }
  // Set Connection Destination Processor
  void setDestination(std::shared_ptr<core::Connectable> dest) {
    dest_connectable_ = dest;
  }
  // ! Get Connection Destination Processor
  std::shared_ptr<core::Connectable> getDestination() {
    return dest_connectable_;
  }

  /**
   * Deprecated function
   * Please use addRelationship.
   */
  void setRelationship(core::Relationship relationship) {
    relationships_.insert(relationship);
  }

  // Set Connection relationship
  void addRelationship(core::Relationship relationship) {
    relationships_.insert(relationship);
  }
  // ! Get Connection relationship
  const std::set<core::Relationship> &getRelationships() const {
    return relationships_;
  }
  // Set Max Queue Size
  void setMaxQueueSize(uint64_t size) {
    max_queue_size_ = size;
  }
  // Get Max Queue Size
  uint64_t getMaxQueueSize() {
    return max_queue_size_;
  }
  // Set Max Queue Data Size
  void setMaxQueueDataSize(uint64_t size) {
    max_data_queue_size_ = size;
  }
  // Get Max Queue Data Size
  uint64_t getMaxQueueDataSize() {
    return max_data_queue_size_;
  }
  // Set Flow expiration duration in millisecond
  void setFlowExpirationDuration(uint64_t duration) {
    expired_duration_ = duration;
  }
  // Get Flow expiration duration in millisecond
  uint64_t getFlowExpirationDuration() {
    return expired_duration_;
  }

  void setDropEmptyFlowFiles(bool drop) {
    drop_empty_ = drop;
  }

  bool getDropEmptyFlowFiles() const {
    return drop_empty_;
  }

  // Check whether the queue is empty
  bool isEmpty();
  // Check whether the queue is full to apply back pressure
  bool isFull();
  // Get queue size
  uint64_t getQueueSize() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
  }
  // Get queue data size
  uint64_t getQueueDataSize() {
    return queued_data_size_;
  }
  void put(std::shared_ptr<core::Connectable> flow) override {
    std::shared_ptr<core::FlowFile> ff = std::static_pointer_cast<core::FlowFile>(flow);
    if (nullptr != ff) {
      put(ff);
    }
  }

  class Transaction{
    friend class core::ProcessSession;
    enum class State{
      ACTIVE,
      COMMITTED,
      ROLLED_BACK,
      INVALID
    };
    Transaction(const std::shared_ptr<Connection>& connection) : connection_(connection) {}
    Transaction(Transaction&&) = delete;
    Transaction(const Transaction&) = delete;
    Transaction& operator=(Transaction&&) = delete;
    Transaction& operator=(const Transaction&) = delete;

    std::shared_ptr<core::FlowFile> poll(std::set<std::shared_ptr<core::FlowFile>>& expired) {
      if (state_ != State::ACTIVE) {
        state_ = State::INVALID;
        throw std::runtime_error("Fatal error: cannot modify connection through a non-active transaction");
      }
      auto flowFile = connection_->poll(expired);
      if (flowFile) {
        removed_data_size_ += flowFile->getSize();
        ++removed_item_count_;
      }
    }

    void commit() {
      if (state_ != State::ACTIVE) {
        state_ = State::INVALID;
        throw std::runtime_error("Fatal error: cannot commit non-active transaction");
      }
      std::lock_guard<std::mutex> lock(connection_->mutex_);
      connection_->temp_removed_item_count_ -= removed_item_count_;
      connection_->temp_removed_data_size_ -= removed_data_size_;
      state_ = State::COMMITTED;
    }

    void rollback() {
      if (state_ != State::ACTIVE) {
        state_ = State::INVALID;
        throw std::runtime_error("Fatal error: cannot rollback non-active transaction");
      }
      state_ = State::ROLLED_BACK;
    }

    ~Transaction() {
      if (state_ == State::ACTIVE) {
        throw std::runtime_error("Fatal error: transaction was neither committed nor rolled back");
      }
    }

    std::shared_ptr<Connection> connection_;
    State state_{State::ACTIVE};
    uint64_t removed_data_size_{0};
    uint64_t removed_item_count_{0};
  };

  struct Congestion{
    Congestion() = default;
    explicit Congestion(double value): value{value} {}
    double getValue() const {return value;}
    bool isFree() const {return value == 1.0;}
   private:
    double value{1.0};
  };

  // Put the flow file into queue
  void put(std::shared_ptr<core::FlowFile> flow);

  // Put multiple flowfiles into the queue
  void multiPut(std::vector<std::shared_ptr<core::FlowFile>>& flows);
  // Poll the flow file from queue, the expired flow file record also being returned
  std::shared_ptr<core::FlowFile> poll(std::set<std::shared_ptr<core::FlowFile>> &expiredFlowRecords);
  // Drain the flow records
  void drain();

  void yield() override {}

  bool isWorkAvailable() override {
    return !isEmpty();
  }

  bool isRunning() override {
    return true;
  }

  Congestion getCongestion();

 protected:
  // Source Processor UUID
  utils::Identifier src_uuid_;
  // Destination Processor UUID
  utils::Identifier dest_uuid_;
  // Relationship for this connection
  std::set<core::Relationship> relationships_;
  // Source Processor (ProcessNode/Port)
  std::shared_ptr<core::Connectable> source_connectable_;
  // Destination Processor (ProcessNode/Port)
  std::shared_ptr<core::Connectable> dest_connectable_;
  // Max queue size to apply back pressure
  std::atomic<uint64_t> max_queue_size_;
  // Max queue data size to apply back pressure
  std::atomic<uint64_t> max_data_queue_size_;
  // Flow File Expiration Duration in= MilliSeconds
  std::atomic<uint64_t> expired_duration_;
  // flow file repository
  std::shared_ptr<core::Repository> flow_repository_;
  // content repository reference.
  std::shared_ptr<core::ContentRepository> content_repo_;

 private:
  bool drop_empty_;
  // Mutex for protection
  std::mutex mutex_;
  // Queued data size
  std::atomic<uint64_t> queued_data_size_;
  // Queue for the Flow File
  std::queue<std::shared_ptr<core::FlowFile>> queue_;
  // Session
  std::atomic<uint64_t> temp_removed_data_size_{0};
  std::atomic<uint64_t> temp_removed_item_count_{0};
  // flow repository
  // Logger
  std::shared_ptr<logging::Logger> logger_;
  // Prevent default copy constructor and assignment operation
  // Only support pass by reference or pointer
  Connection(const Connection &parent);
  Connection &operator=(const Connection &parent);
};
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
#endif  // LIBMINIFI_INCLUDE_CONNECTION_H_
