/**
 * @file Processor.cpp
 * Processor class implementation
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
#include "core/Processor.h"

#include <ctime>
#include <random>
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "Connection.h"
#include "core/Connectable.h"
#include "core/logging/LoggerConfiguration.h"
#include "core/ProcessorConfig.h"
#include "core/ProcessContext.h"
#include "core/ProcessSession.h"
#include "core/ProcessSessionFactory.h"
#include "io/StreamFactory.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

Processor::Processor(std::string name)
    : Connectable(name),
      ConfigurableComponent(),
      logger_(logging::LoggerFactory<Processor>::getLogger()) {
  has_work_.store(false);
  // Setup the default values
  state_ = DISABLED;
  strategy_ = TIMER_DRIVEN;
  loss_tolerant_ = false;
  _triggerWhenEmpty = false;
  scheduling_period_nano_ = MINIMUM_SCHEDULING_NANOS;
  run_duration_nano_ = DEFAULT_RUN_DURATION;
  yield_period_msec_ = DEFAULT_YIELD_PERIOD_SECONDS * 1000;
  _penalizationPeriodMsec = DEFAULT_PENALIZATION_PERIOD_SECONDS * 1000;
  max_concurrent_tasks_ = DEFAULT_MAX_CONCURRENT_TASKS;
  active_tasks_ = 0;
  yield_expiration_ = 0;
  incoming_connections_Iter = this->_incomingConnections.begin();
  logger_->log_debug("Processor %s created UUID %s", name_, uuidStr_);
}

Processor::Processor(std::string name, utils::Identifier &uuid)
    : Connectable(name, uuid),
      ConfigurableComponent(),
      logger_(logging::LoggerFactory<Processor>::getLogger()) {
  has_work_.store(false);
  // Setup the default values
  state_ = DISABLED;
  strategy_ = TIMER_DRIVEN;
  loss_tolerant_ = false;
  _triggerWhenEmpty = false;
  scheduling_period_nano_ = MINIMUM_SCHEDULING_NANOS;
  run_duration_nano_ = DEFAULT_RUN_DURATION;
  yield_period_msec_ = DEFAULT_YIELD_PERIOD_SECONDS * 1000;
  _penalizationPeriodMsec = DEFAULT_PENALIZATION_PERIOD_SECONDS * 1000;
  max_concurrent_tasks_ = DEFAULT_MAX_CONCURRENT_TASKS;
  active_tasks_ = 0;
  yield_expiration_ = 0;
  incoming_connections_Iter = this->_incomingConnections.begin();
  logger_->log_debug("Processor %s created UUID %s with uuid %s", name_, uuidStr_, uuid.to_string());
}

bool Processor::isRunning() {
  return (state_ == RUNNING && active_tasks_ > 0);
}

void Processor::setScheduledState(ScheduledState state) {
  state_ = state;
  if (state == STOPPED) {
    notifyStop();
  }
}

bool Processor::addConnection(std::shared_ptr<Connectable> conn) {
  bool ret = false;

  if (isRunning()) {
    logger_->log_warn("Can not add connection while the process %s is running", name_);
    return false;
  }
  std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);
  std::lock_guard<std::mutex> lock(mutex_);

  utils::Identifier srcUUID;
  utils::Identifier destUUID;

  connection->getSourceUUID(srcUUID);
  connection->getDestinationUUID(destUUID);
  std::string my_uuid = uuid_.to_string();
  std::string destination_uuid = destUUID.to_string();
  if (my_uuid == destination_uuid) {
    // Connection is destination to the current processor
    if (_incomingConnections.find(connection) == _incomingConnections.end()) {
      _incomingConnections.insert(connection);
      connection->setDestination(shared_from_this());
      logger_->log_debug("Add connection %s into Processor %s incoming connection", connection->getName(), name_);
      incoming_connections_Iter = this->_incomingConnections.begin();
      ret = true;
    }
  }
  std::string source_uuid = srcUUID.to_string();
  if (my_uuid == source_uuid) {
    const auto &rels = connection->getRelationships();
    for (auto i = rels.begin(); i != rels.end(); i++) {
      const auto relationship = (*i).getName();
      // Connection is source from the current processor
      auto &&it = out_going_connections_.find(relationship);
      if (it != out_going_connections_.end()) {
        // We already has connection for this relationship
        std::set<std::shared_ptr<Connectable>> existedConnection = it->second;
        if (existedConnection.find(connection) == existedConnection.end()) {
          // We do not have the same connection for this relationship yet
          existedConnection.insert(connection);
          connection->setSource(shared_from_this());
          out_going_connections_[relationship] = existedConnection;
          logger_->log_debug("Add connection %s into Processor %s outgoing connection for relationship %s", connection->getName(), name_, relationship);
          ret = true;
        }
      } else {
        // We do not have any outgoing connection for this relationship yet
        std::set<std::shared_ptr<Connectable>> newConnection;
        newConnection.insert(connection);
        connection->setSource(shared_from_this());
        out_going_connections_[relationship] = newConnection;
        logger_->log_debug("Add connection %s into Processor %s outgoing connection for relationship %s", connection->getName(), name_, relationship);
        ret = true;
      }
    }
  }
  return ret;
}

void Processor::removeConnection(std::shared_ptr<Connectable> conn) {
  if (isRunning()) {
    logger_->log_warn("Can not remove connection while the process %s is running", name_);
    return;
  }

  std::lock_guard<std::mutex> lock(mutex_);

  utils::Identifier srcUUID;
  utils::Identifier destUUID;

  std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);

  connection->getSourceUUID(srcUUID);
  connection->getDestinationUUID(destUUID);

  if (uuid_ == destUUID) {
    // Connection is destination to the current processor
    if (_incomingConnections.find(connection) != _incomingConnections.end()) {
      _incomingConnections.erase(connection);
      connection->setDestination(NULL);
      logger_->log_debug("Remove connection %s into Processor %s incoming connection", connection->getName(), name_);
      incoming_connections_Iter = this->_incomingConnections.begin();
    }
  }

  if (uuid_ == srcUUID) {
    const auto &rels = connection->getRelationships();
    for (auto i = rels.begin(); i != rels.end(); i++) {
      const auto relationship = (*i).getName();
      // Connection is source from the current processor
      auto &&it = out_going_connections_.find(relationship);
      if (it != out_going_connections_.end()) {
        if (out_going_connections_[relationship].find(connection) != out_going_connections_[relationship].end()) {
          out_going_connections_[relationship].erase(connection);
          connection->setSource(NULL);
          logger_->log_debug("Remove connection %s into Processor %s outgoing connection for relationship %s", connection->getName(), name_, relationship);
        }
      }
    }
  }
}

bool Processor::flowFilesQueued() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (_incomingConnections.size() == 0)
    return false;

  for (auto &&conn : _incomingConnections) {
    std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);
    if (connection->getQueueSize() > 0)
      return true;
  }

  return false;
}

bool Processor::flowFilesOutGoingFull() {
  std::lock_guard<std::mutex> lock(mutex_);

  for (const auto& connection_pair : out_going_connections_) {
    // We already has connection for this relationship
    std::set<std::shared_ptr<Connectable>> existedConnection = connection_pair.second;
    const bool has_full_connection = std::any_of(begin(existedConnection), end(existedConnection), [](const std::shared_ptr<Connectable>& conn) {
      return std::static_pointer_cast<Connection>(conn)->isFull();
    });
    if (has_full_connection) { return true; }
  }

  return false;
}

double Processor::getExecutionProbability() {
  std::lock_guard<std::mutex> lock(mutex_);
  return execution_probability_;
}

double Processor::updateAndFetchExecutionProbability() {
  initialize_connection_weights();
  std::lock_guard<std::mutex> lock(mutex_);

  congestion_history_.emplace_front(HistoryItem(calculateIncomingCongestions(), calculateOutgoingCongestions(), std::move(curr_polled_connections_)));

  if(congestion_history_.size() < 50) return execution_probability_;

  // serialize history for debugging
  std::stringstream ss;
  for(auto& item : congestion_history_) {
    ss << "Outgoing:\n";
    for(auto& out : item.outgoing) {
      ss << "\t" << out.first->getName() << ": " << out.second.getValue() << "\n";
    }
    ss << "Incoming:\n";
    for(auto& in : item.incoming) {
      ss << "\t" << in.first->getName() << ": " << in.second.getValue() << "\n";
    }
    ss << "Polled:\n";
    for(auto& session: item.polled) {
      ss << "\t[";
      for(auto& conn : session){
        ss << conn->getName() << ", ";
      }
      ss << "]\n";
    }
  }

  std::ofstream{"/Users/adamdebreceni/work/debug"} << ss.str();

  // establish correlation between which incoming we pick and if we make things worse

  struct Configuration{
    std::shared_ptr<Connection> polled;
    std::shared_ptr<Connection> not_polled;
    bool operator==(const Configuration& other) const {
      return polled == other.polled && not_polled == other.not_polled;
    }
  };

  struct conf_hash {
    std::size_t operator () (const Configuration& p) const {
      auto h1 = std::hash<std::shared_ptr<Connection>>{}(p.polled);
      auto h2 = std::hash<std::shared_ptr<Connection>>{}(p.not_polled);
      return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
    }
  };

  struct Votes{
    int makes_it_worse = 0;
    int makes_it_better = 0;
    int neither = 0;
  };

  std::vector<std::shared_ptr<Connection>> incoming;
  for (auto& conn : _incomingConnections) {
    auto connection = std::dynamic_pointer_cast<Connection>(conn);
    if(connection)incoming.emplace_back(connection);
  }

  // picked the first item but did not pick the second
  std::unordered_map<Configuration, Votes, conf_hash> discriminator;

  auto didPoll = [](const std::vector<std::unordered_set<std::shared_ptr<Connection>>>& polled, const std::shared_ptr<Connection>& conn) -> bool {
    for (const auto& set : polled){
      if(set.find(conn) != set.end())return true;
    }
    return false;
  };

  bool trigger_makes_worse = false;
  bool some_not_getting_better = false;

  struct Statistics{
    double n = 0;
    double sum_X = 0;
    double sum_Y = 0;
    double sum_X_sqr = 0;
    double sum_XY = 0;
    double a = 0;
    double b = 0;
  };
  std::unordered_map<std::shared_ptr<Connection>, Statistics> stats{};

  for (std::size_t idx = 0; idx < congestion_history_.size() - 1; ++idx) {
    auto& curr = congestion_history_[idx];
    auto& prev = congestion_history_[idx + 1];

    for (auto& conn : curr.outgoing) {
      auto &stat = stats[conn.first];
      ++stat.n;
      stat.sum_X += idx;
      stat.sum_Y += conn.second.getValue();
      stat.sum_X_sqr += idx * idx;
      stat.sum_XY += conn.second.getValue() * idx;
    }
    // check if worse better or the same
    // on the outgoing connections
    bool increasedOutgoingCongestion = std::any_of(curr.outgoing.begin(), curr.outgoing.end(), [&](const std::pair<std::shared_ptr<Connection>, Connection::Congestion>& outgoing){
        return outgoing.second.getValue() > prev.outgoing[outgoing.first].getValue();
    });
    // through our execution we could mess up our incoming connection (if we have a loop)
    /*bool increasedIncomingCongestion = std::any_of(curr.incoming.begin(), curr.incoming.end(), [&](const std::pair<std::shared_ptr<Connection>, Connection::Congestion>& incoming){
      // if (didPoll(curr.polled, incoming.first)) return false;
      return incoming.second.getValue() > prev.incoming[incoming.first].getValue();
    }); */
    bool decreasedOutgoingCongestion = std::all_of(curr.outgoing.begin(), curr.outgoing.end(), [&](const std::pair<std::shared_ptr<Connection>, Connection::Congestion>& outgoing){
        // steadily increase probability in case all of them are not full
        return outgoing.second.getValue() < prev.outgoing[outgoing.first].getValue();
    });
    for (auto& polled : incoming) {
      for (auto& not_polled : incoming) {
        if(polled == not_polled) continue;
        int discriminatorCount = std::count_if(curr.polled.begin(), curr.polled.end(), [&](const std::unordered_set<std::shared_ptr<Connection>>& polledSet){
            // this is the proper discriminator configuration, one is polled and one is not
          return polledSet.find(polled) != polledSet.end() && polledSet.find(not_polled) == polledSet.end();
        });
        // this is the proper discriminator configuration, one is polled and one is not
        if (increasedOutgoingCongestion) {
          // should penalize polled
          discriminator[Configuration{polled, not_polled}].makes_it_worse += discriminatorCount;
        } else if (decreasedOutgoingCongestion) {
          // we did good polling these connections, we should give a chance for other connections as well
          discriminator[Configuration{polled, not_polled}].makes_it_better += discriminatorCount;
        } else {
          // neither good nor bad, maintain the status quo
          discriminator[Configuration{polled, not_polled}].neither += discriminatorCount;
        }
      }
    }
  }

  // fit line
  for (auto& statIt : stats) {
    auto& stat = statIt.second;
    double det = stat.sum_X * stat.sum_X - stat.sum_X_sqr * stat.n;
    // ax + b = y
    double a = (stat.sum_X * stat.sum_Y  - stat.n * stat.sum_XY) / det;
    double b = (-stat.sum_X_sqr * stat.sum_Y + stat.sum_X * stat.sum_XY) / det;

    stat.a = a;
    stat.b = b;

    std::stringstream ss;
    ss << statIt.first->getName() << " : [a=" << a << "]x + [b=" << b << "]";
    std::string st = ss.str();

    if (a < -0.0001) {
      trigger_makes_worse = true;
      some_not_getting_better = true;
    }
    if (a < 0.0001 || b > 1.0001) {
      some_not_getting_better = true;
    }
  }

  bool shouldMakeProcessorLevelDecision = true;
  std::vector<std::shared_ptr<Connection>> penalized_connections{};
  // check if we can act on the votes on a connection-level
  ([&]() {
      for (auto &polled : incoming) {
        for (auto &not_polled : incoming) {
          if (polled == not_polled) continue;
          shouldMakeProcessorLevelDecision = false;
          auto &polledVotes = discriminator[Configuration{polled, not_polled}];
          auto &notPolledVotes = discriminator[Configuration{not_polled, polled}];
          int penalize_polled = polledVotes.makes_it_worse + polledVotes.makes_it_better;
          int penalize_not_polled = notPolledVotes.makes_it_worse + notPolledVotes.makes_it_better;
          int neither = polledVotes.neither + notPolledVotes.neither;
          int totalVotes = penalize_polled + penalize_not_polled + neither;
          if (totalVotes == neither) continue;
          if (penalize_polled > 2 * penalize_not_polled) {
            // what if all of them are "makes_it_better"? => then we make a processor-level decision to increase the
            // execution prob
            // penalize polled
            penalized_connections.emplace_back(polled);
          } else if (penalize_not_polled <= 2 * penalize_polled) {
            // we couldn't decide which one to penalize
            shouldMakeProcessorLevelDecision = true;
            return;
          }
        }
      }
  })();

  if (penalized_connections.size() == 0) {
    shouldMakeProcessorLevelDecision = true;
  }

  if (shouldMakeProcessorLevelDecision) {
    if (trigger_makes_worse) {
      execution_probability_ /= 2;
    } else if (!some_not_getting_better) {
      execution_probability_ = std::min(execution_probability_ * 2, 1.0);
    }
  } else {
    for (auto& conn : penalized_connections) {
      auto& weight = incoming_connection_weights_[conn];
      weight /= 2;
    }
    // adjust the weight
    double weight_sum = 0.0;
    for (auto& incoming : incoming_connection_weights_) {
      weight_sum += incoming.second;
    }
    for (auto& incoming : incoming_connection_weights_) {
      incoming.second /= weight_sum;
    }
  }

  // discard old items
  congestion_history_.erase(congestion_history_.begin() + 25, congestion_history_.end());

  return execution_probability_;
}

std::shared_ptr<Connectable> Processor::pickRandomIncomingConnection(bool& random) {
  initialize_connection_weights();
  double weight_sum = 0.0;
  std::unordered_map<std::shared_ptr<Connectable>, double> weights;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto &conn : _incomingConnections) {
      auto connection = std::dynamic_pointer_cast<Connection>(conn);
      if (!connection) {
        continue;
      }
      if(connection->isEmpty()){
        continue;
      }
      double weight = incoming_connection_weights_[connection];
      weight_sum += weight;
      weights[conn] = weight;
    }
  }
  if (weights.empty()) {
    return nullptr;
  }
  if (weights.size() == 1) {
    random = false;
  } else {
    random = true;
  }
  std::random_device rd{};
  std::uniform_real_distribution<double> dis(0.0, weight_sum);
  double rand = dis(rd);
  for (auto& conn : weights) {
    if (rand <= conn.second) {
      return conn.first;
    }
    rand -= conn.second;
  }
  return weights.cbegin()->first;
}

void Processor::onTrigger(ProcessContext *context, ProcessSessionFactory *sessionFactory) {
  auto session = sessionFactory->createSession();

  try {
    // Call the virtual trigger function
    //auto currentOutgoing = calculateOutgoingCongestions();
    //auto currentIncoming = calculateIncomingCongestions();
    onTrigger(context, session.get());
    session->commit();
    {
      //std::lock_guard<std::mutex> guard(mutex_);
      //++trigger_count_;
      //outgoing_congestions_ = currentOutgoing;
      //incoming_congestions_ = currentIncoming;
      //got_triggered_ = true;
    }
  } catch (std::exception &exception) {
    logger_->log_warn("Caught Exception %s during Processor::onTrigger of processor: %s (%s)", exception.what(), getUUIDStr(), getName());
    session->rollback();
    throw;
  } catch (...) {
    logger_->log_warn("Caught Exception during Processor::onTrigger of processor: %s (%s)", getUUIDStr(), getName());
    session->rollback();
    throw;
  }
}

void Processor::onTrigger(const std::shared_ptr<ProcessContext> &context, const std::shared_ptr<ProcessSessionFactory> &sessionFactory) {
  auto session = sessionFactory->createSession();

  try {
    // Call the virtual trigger function
    //auto currentOutgoing = calculateOutgoingCongestions();
    //auto currentIncoming = calculateIncomingCongestions();
    onTrigger(context, session);
    session->commit();
    /*{
      std::lock_guard<std::mutex> guard(mutex_);
      outgoing_congestions_ = currentOutgoing;
      incoming_congestions_ = currentIncoming;
    }*/
  } catch (std::exception &exception) {
    logger_->log_warn("Caught Exception %s during Processor::onTrigger of processor: %s (%s)", exception.what(), getUUIDStr(), getName());
    session->rollback();
    throw;
  } catch (...) {
    logger_->log_warn("Caught Exception during Processor::onTrigger of processor: %s (%s)", getUUIDStr(), getName());
    session->rollback();
    throw;
  }
}

bool Processor::isWorkAvailable() {
  // We have work if any incoming connection has work
  std::lock_guard<std::mutex> lock(mutex_);
  bool hasWork = false;

  try {
    for (const auto &conn : _incomingConnections) {
      std::shared_ptr<Connection> connection = std::static_pointer_cast<Connection>(conn);
      if (connection->getQueueSize() > 0) {
        hasWork = true;
        break;
      }
    }
  } catch (...) {
    logger_->log_error("Caught an exception while checking if work is available;"
                       " unless it was positively determined that work is available, assuming NO work is available!");
  }

  return hasWork;
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
