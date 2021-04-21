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

#include "c2/HeartBeatJSONSerializer.h"
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace c2 {

std::string HeartBeatJSONSerializer::getOperation(const C2Payload& payload) {
  switch (payload.getOperation()) {
    case Operation::ACKNOWLEDGE:
      return "acknowledge";
    case Operation::HEARTBEAT:
      return "heartbeat";
    case Operation::RESTART:
      return "restart";
    case Operation::DESCRIBE:
      return "describe";
    case Operation::STOP:
      return "stop";
    case Operation::START:
      return "start";
    case Operation::UPDATE:
      return "update";
    case Operation::PAUSE:
      return "pause";
    case Operation::RESUME:
      return "resume";
    default:
      return "heartbeat";
  }
}

static void serializeOperationInfo(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc) {
  gsl_Expects(target.IsObject());

  target.AddMember("operation", HeartBeatJSONSerializer::getOperation(payload), alloc);

  std::string id = payload.getIdentifier();
  if (id.empty()) {
    return;
  }

  target.AddMember("operationId", id, alloc);
  std::string state_str;
  switch (payload.getStatus().getState()) {
    case state::UpdateState::FULLY_APPLIED:
      state_str = "FULLY_APPLIED";
      break;
    case state::UpdateState::PARTIALLY_APPLIED:
      state_str = "PARTIALLY_APPLIED";
      break;
    case state::UpdateState::READ_ERROR:
      state_str = "OPERATION_NOT_UNDERSTOOD";
      break;
    case state::UpdateState::SET_ERROR:
    default:
      state_str = "NOT_APPLIED";
  }

  rapidjson::Value state(rapidjson::kObjectType);

  state.AddMember("state", state_str, alloc);
  const auto details = payload.getRawData();

  state.AddMember("details", std::string(details.data(), details.size()), alloc);

  target.AddMember("operationState", state, alloc);
  target.AddMember("identifier", id, alloc);
}

static void setJsonStr(const std::string& key, const state::response::ValueNode& value, rapidjson::Value& parent, rapidjson::Document::AllocatorType& alloc) {  // NOLINT
  rapidjson::Value valueVal;
  auto base_type = value.getValue();

  auto type_index = base_type->getTypeIndex();
  if (auto sub_type = std::dynamic_pointer_cast<core::TransformableValue>(base_type)) {
    valueVal.SetString(base_type->getStringValue(), alloc);
  } else {
    if (type_index == state::response::Value::BOOL_TYPE) {
      bool value = false;
      base_type->convertValue(value);
      valueVal.SetBool(value);
    } else if (type_index == state::response::Value::INT_TYPE) {
      int value = 0;
      base_type->convertValue(value);
      valueVal.SetInt(value);
    } else if (type_index == state::response::Value::UINT32_TYPE) {
      uint32_t value = 0;
      base_type->convertValue(value);
      valueVal.SetUint(value);
    } else if (type_index == state::response::Value::INT64_TYPE) {
      int64_t value = 0;
      base_type->convertValue(value);
      valueVal.SetInt64(value);
    } else if (type_index == state::response::Value::UINT64_TYPE) {
      int64_t value = 0;
      base_type->convertValue(value);
      valueVal.SetInt64(value);
    } else {
      valueVal.SetString(base_type->getStringValue(), alloc);
    }
  }
  parent.AddMember(rapidjson::Value(key, alloc), valueVal, alloc);
}

static void mergePayloadContent(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc) {
  const std::vector<C2ContentResponse>& content = payload.getContent();
  if (content.empty()) {
    return;
  }
  const bool all_empty = [&] {
    for (const auto& payload_content : content) {
      for (const auto& op_arg : payload_content.operation_arguments) {
        if (!op_arg.second.empty()) {
          return false;
        }
      }
    }
    return true;
  }();

  if (all_empty) {
    if (!target.IsArray()) {
      target.SetArray();
    }
    for (const auto& payload_content : content) {
      for (const auto& op_arg : payload_content.operation_arguments) {
        target.PushBack(rapidjson::Value(op_arg.first, alloc), alloc);
      }
    }
    return;
  }
  for (const auto& payload_content : content) {
    if (payload_content.op == payload.getOperation()) {
      for (const auto& op_arg : payload_content.operation_arguments) {
        if (!op_arg.second.empty()) {
          setJsonStr(op_arg.first, op_arg.second, target, alloc);
        }
      }
    }
  }
}

static rapidjson::Value serializeConnectionQueues(const C2Payload& payload, std::string& label, rapidjson::Document::AllocatorType& alloc) {
  rapidjson::Value json_payload(payload.isContainer() ? rapidjson::kArrayType : rapidjson::kObjectType);

  C2Payload adjusted(payload.getOperation(), payload.getIdentifier(), payload.isRaw());

  auto name = payload.getLabel();
  std::string uuid;
  C2ContentResponse updatedContent(payload.getOperation());
  for (const C2ContentResponse &content : payload.getContent()) {
    for (const auto& op_arg : content.operation_arguments) {
      if (op_arg.first == "uuid") {
        uuid = op_arg.second.to_string();
      }
      updatedContent.operation_arguments.insert(op_arg);
    }
  }
  updatedContent.name = uuid;
  adjusted.setLabel(uuid);
  adjusted.setIdentifier(uuid);
  c2::AnnotatedValue nd;
  // name should be what was previously the TLN ( top level node )
  nd = name;
  updatedContent.operation_arguments.insert(std::make_pair("name", nd));
  // the rvalue reference is an unfortunate side effect of the underlying API decision.
  adjusted.addContent(std::move(updatedContent), true);
  mergePayloadContent(json_payload, adjusted, alloc);
  label = uuid;
  return json_payload;
}

std::string HeartBeatJSONSerializer::serializeJsonRootPayload(const C2Payload& payload) {
  rapidjson::Document json_payload(payload.isContainer() ? rapidjson::kArrayType : rapidjson::kObjectType);
  rapidjson::Document::AllocatorType &alloc = json_payload.GetAllocator();

  serializeOperationInfo(json_payload, payload, alloc);

  mergePayloadContent(json_payload, payload, alloc);

  for (const auto &nested_payload : payload.getNestedPayloads()) {
    serializeNestedPayload(json_payload, nested_payload, alloc);
  }

  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  json_payload.Accept(writer);
  return buffer.GetString();
}

void HeartBeatJSONSerializer::serializeNestedPayload(rapidjson::Value& target, const C2Payload& payload, rapidjson::Document::AllocatorType& alloc) {
  target.AddMember(rapidjson::Value(payload.getLabel(), alloc), serializeJsonPayload(payload, alloc), alloc);
}

struct NamedValue {
  std::string name;
  std::vector<rapidjson::Value> values;

  rapidjson::Value to_array(rapidjson::Document::AllocatorType& alloc) {
    rapidjson::Value arr(rapidjson::kArrayType);
    spread_into(arr, alloc);
    return arr;
  }

  void spread_into(rapidjson::Value& target_array, rapidjson::Document::AllocatorType& alloc) {
    gsl_Expects(target_array.IsArray());
    for (auto& child : values) {
      target_array.PushBack(child, alloc);
    }
  }

  void move_into(rapidjson::Value& target, rapidjson::Document::AllocatorType& alloc) {
    if (values.empty()) {
      return;
    }
    rapidjson::Value member_key(name, alloc);
    if (values.size() > 1) {
      if (target.IsArray()) {
        spread_into(target, alloc);
      } else {
        target.AddMember(member_key, to_array(alloc), alloc);
      }
      return;
    }
    rapidjson::Value& val = [&] () -> rapidjson::Value& {
      return values[0].IsObject() && values[0].HasMember(member_key) ? values[0][member_key] : values[0];
    }();
    if (target.IsArray()) {
      target.PushBack(val, alloc);
    } else {
      target.AddMember(member_key, val, alloc);
    }
  }
};

class NamedValueMap {
  using Container = std::vector<NamedValue>;

 public:
  Container::iterator find(const std::string& key) {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      if (it->name == key) return it;
    }
    return data_.end();
  }
  Container::const_iterator find(const std::string& key) const {
    for (auto it = data_.begin(); it != data_.end(); ++it) {
      if (it->name == key) return it;
    }
    return data_.end();
  }
  std::vector<rapidjson::Value>& operator[](const std::string& key) {
    auto it = find(key);
    if (it == end()) {
      data_.push_back(NamedValue{key, {}});
      it = find(key);
    }
    return it->values;
  }
  std::vector<rapidjson::Value>& push_back(std::string key) {
    data_.emplace_back(NamedValue{std::move(key), {}});
    return data_.back().values;
  }
  Container::iterator begin() {
    return data_.begin();
  }
  Container::iterator end() {
    return data_.end();
  }

 private:
  Container data_;
};

rapidjson::Value HeartBeatJSONSerializer::serializeJsonPayload(const C2Payload& payload, rapidjson::Document::AllocatorType& alloc) {
  // get the name from the content
  rapidjson::Value json_payload(payload.isContainer() ? rapidjson::kArrayType : rapidjson::kObjectType);

  NamedValueMap children;

  const bool isQueue = payload.getLabel() == "queues";

  for (const auto &nested_payload : payload.getNestedPayloads()) {
    std::string label = nested_payload.getLabel();
    rapidjson::Value child_payload(isQueue ? serializeConnectionQueues(nested_payload, label, alloc) : serializeJsonPayload(nested_payload, alloc));

    if (nested_payload.isCollapsible()) {
      children[label].push_back(std::move(child_payload));
    } else {
      children.push_back(label).push_back(std::move(child_payload));
    }
  }

  for (auto& child : children) {
    child.move_into(json_payload, alloc);
  }

  mergePayloadContent(json_payload, payload, alloc);
  return json_payload;
}

}  // namespace c2
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org