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
#ifndef LIBMINIFI_INCLUDE_CORE_PROPERTYVALUE_H_
#define LIBMINIFI_INCLUDE_CORE_PROPERTYVALUE_H_

#include "state/Value.h"
#include "PropertyValidation.h"
#include "CachedValueValidator.h"
#include <typeindex>

#include "PropertyValidation.h"
#include "state/Value.h"
#include "TypedValues.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

static inline std::shared_ptr<state::response::Value> convert(const std::shared_ptr<state::response::Value> &prior, const std::string &ref) {
  if (prior->getTypeIndex() == state::response::Value::UINT64_TYPE) {
    // there are specializations, so check them first
    if (std::dynamic_pointer_cast<TimePeriodValue>(prior)) {
      return std::make_shared<TimePeriodValue>(ref);
    } else if (std::dynamic_pointer_cast<DataSizeValue>(prior)) {
      return std::make_shared<DataSizeValue>(ref);
    } else {
      return std::make_shared<state::response::UInt64Value>(ref);
    }
  } else if (prior->getTypeIndex() == state::response::Value::INT64_TYPE) {
    return std::make_shared<state::response::Int64Value>(ref);
  } else if (prior->getTypeIndex() == state::response::Value::UINT32_TYPE) {
    return std::make_shared<state::response::UInt32Value>(ref);
  } else if (prior->getTypeIndex() == state::response::Value::INT_TYPE) {
    return std::make_shared<state::response::IntValue>(ref);
  } else if (prior->getTypeIndex() == state::response::Value::BOOL_TYPE) {
    return std::make_shared<state::response::BoolValue>(ref);
  } else {
    return std::make_shared<state::response::Value>(ref);
  }
}

/**
 * Purpose and Design: PropertyValue extends ValueNode, bringing with it value_.
 * The reason for this is that there are additional features to support validation
 * and value translation.
 */
class PropertyValue : public state::response::ValueNode {
 public:
  PropertyValue()
      : type_id(std::type_index(typeid(std::string))) {
    validator_ = StandardValidators::VALID;
  }

  PropertyValue(const PropertyValue &o) = default;
  PropertyValue(PropertyValue &&o) noexcept = default;

  void setValidator(const std::shared_ptr<PropertyValidator> &val) {
    validator_ = val;
  }

  std::shared_ptr<PropertyValidator> getValidator() const {
    return *validator_;
  }

  ValidationResult validate(const std::string &subject) const {
    auto cachedResult = validator_.isValid();
    if(cachedResult == CachedValueValidator::Result::SUCCESS){
      return ValidationResult::Builder::createBuilder().isValid(true).build();
    }
    if(cachedResult == CachedValueValidator::Result::FAILURE){
      return ValidationResult::Builder::createBuilder().withSubject(subject).withInput(getValue()->getStringValue()).isValid(false).build();
    }
    auto result = validator_->validate(subject, getValue());
    validator_.setValidationResult(result.valid());
    return result;
  }

  operator uint64_t() const {
    if(!isValueUsable())throw utils::InvalidValueException("Cannot convert invalid value");
    uint64_t res;
    if (value_->convertValue(res)) {
      return res;
    }
    throw utils::ConversionException("Invalid conversion to uint64_t for" + value_->getStringValue());
  }

  operator int64_t() const {
    if(!isValueUsable())throw utils::InvalidValueException("Cannot convert invalid value");
    int64_t res;
    if (value_->convertValue(res)) {
      return res;
    }
    throw utils::ConversionException("Invalid conversion to int64_t");
  }

  operator uint32_t() const {
    if(!isValueUsable())throw utils::InvalidValueException("Cannot convert invalid value");
    uint32_t res;
    if (value_->convertValue(res)) {
      return res;
    }
    throw utils::ConversionException("Invalid conversion to uint32_t for" + value_->getStringValue());
  }

  operator int() const {
    if(!isValueUsable())throw utils::InvalidValueException("Cannot convert invalid value");
    int res;
    if (value_->convertValue(res)) {
      return res;
    }
    throw utils::ConversionException("Invalid conversion to int ");
  }

  operator bool() const {
    if(!isValueUsable())throw utils::InvalidValueException("Cannot convert invalid value");
    bool res;
    if (value_->convertValue(res)) {
      return res;
    }
    throw utils::ConversionException("Invalid conversion to bool");
  }

  operator std::string() const {
    if(!isValueUsable())throw utils::InvalidValueException("Cannot convert invalid value");
    return to_string();
  }

  PropertyValue &operator=(PropertyValue &&o) = default;
  PropertyValue &operator=(const PropertyValue &o) = default;

  std::type_index getTypeInfo() const {
    return type_id;
  }
  /**
   * Define the representations and eventual storage relationships through
   * createValue
   */
  template<typename T>
  auto operator=(const T ref) -> typename std::enable_if<std::is_same<T, std::string>::value,PropertyValue&>::type {
    validator_.clearValidationResult();
    return WithAssignmentGuard(ref, [&] () -> PropertyValue& {
      if (value_ == nullptr) {
        type_id = std::type_index(typeid(T));
        value_ = minifi::state::response::createValue(ref);
      } else {
        type_id = std::type_index(typeid(T));
        auto ret = convert(value_, ref);
        if (ret != nullptr) {
          value_ = ret;
        } else {
          /**
           * This is a protection mechanism that allows us to fail properties that are strictly defined.
           * To maintain backwards compatibility we allow strings to be set by way of the internal API
           * We then rely on the developer of the processor to perform the conversion. We want to get away from
           * this, so this exception will throw an exception, forcefully, when they specify types in properties.
           */
          throw utils::ConversionException("Invalid conversion");
        }
      }
      return *this;
    });
  }

  template<typename T>
  auto operator=(const T ref) -> typename std::enable_if<std::is_same<T, int >::value ||
  std::is_same<T, uint32_t >::value ||
  std::is_same<T, uint64_t >::value ||
  std::is_same<T, int64_t >::value ||
  std::is_same<T, bool >::value,PropertyValue&>::type {
    validator_.clearValidationResult();
    if (value_ == nullptr) {
      type_id = std::type_index(typeid(T));
      value_ = minifi::state::response::createValue(ref);
    } else {
      if (std::dynamic_pointer_cast<DataSizeValue>(value_)) {
        value_ = std::make_shared<DataSizeValue>(ref);
        type_id = DataSizeValue::type_id;
      } else if (std::dynamic_pointer_cast<TimePeriodValue>(value_)) {
        value_ = std::make_shared<TimePeriodValue>(ref);
        type_id = TimePeriodValue::type_id;
      } else if (type_id == std::type_index(typeid(T))) {
        value_ = minifi::state::response::createValue(ref);
      } else {
        // this is not the place to perform translation. There are other places within
        // the code where we can do assignments and transformations from "10" to (int)10;
        throw utils::ConversionException("Assigning invalid types");
      }
    }
    return *this;
  }

  template<typename T>
  auto operator=(const T ref) -> typename std::enable_if<
  std::is_same<T, char* >::value ||
  std::is_same<T, const char* >::value, PropertyValue&>::type {
    // translated these into strings
    return operator=<std::string>(std::string(ref));
  }

  template<typename T>
  auto operator=(const std::string &ref) -> typename std::enable_if<
  std::is_same<T, DataSizeValue >::value ||
  std::is_same<T, TimePeriodValue >::value,PropertyValue&>::type {
    validator_.clearValidationResult();
    return WithAssignmentGuard(ref, [&] () -> PropertyValue& {
      value_ = std::make_shared<T>(ref);
      type_id = value_->getTypeIndex();
      return *this;
    });
  }

 private:

  bool isValueUsable() const {
    if (!value_) return false;
    if (validator_.isValid() == CachedValueValidator::Result::FAILURE) return false;
    if (validator_.isValid() == CachedValueValidator::Result::SUCCESS) return true;
    return validate("__unknown__").valid();
  }

  template<typename Fn>
  auto WithAssignmentGuard(const std::string& ref, Fn&& functor) -> decltype(std::forward<Fn>(functor)()) {
    // TODO(adebreceni): as soon as c++17 comes jump to a RAII implementation
    // as we will need std::uncaught_exceptions()
    try{
      return std::forward<Fn>(functor)();
    }catch(const utils::ValueException&){
      type_id = std::type_index(typeid(std::string));
      value_ = minifi::state::response::createValue(ref);
      throw;
    }
  }

 protected:
  std::type_index type_id;
  CachedValueValidator validator_;
};

inline std::string conditional_conversion(const PropertyValue &v) {
  return v.getValue()->getStringValue();
}

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org

#endif  // LIBMINIFI_INCLUDE_CORE_PROPERTYVALUE_H_
