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

#pragma once

#include <string>
#include "Core.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace core {

/**
 * Class used to provide a global initialization and deinitialization function for an ObjectFactory.
 * Calls to instances of all ObjectFactoryInitializers are done under a unique lock.
 */
class ObjectFactoryInitializer {
 public:
  virtual ~ObjectFactoryInitializer() = default;

  /**
   * This function is be called before the ObjectFactory is used.
   * @return whether the initialization was successful. If false, deinitialize will NOT be called.
   */
  virtual bool initialize() = 0;

  /**
   * This function will be called after the ObjectFactory is not needed anymore.
   */
  virtual void deinitialize() = 0;
};

/**
 * Factory that is used as an interface for
 * creating processors from shared objects.
 */
class ObjectFactory {
 public:
  ObjectFactory(const std::string &group) // NOLINT
      : group_(group) {
  }

  ObjectFactory() = default;

  /**
   * Virtual destructor.
   */
  virtual ~ObjectFactory() = default;

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string& /*name*/) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent *createRaw(const std::string& /*name*/) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return nullptr;
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent* createRaw(const std::string& /*name*/, const utils::Identifier& /*uuid*/) {
    return nullptr;
  }

  /**
   * Returns an initializer for the factory.
   */
  virtual std::unique_ptr<ObjectFactoryInitializer> getInitializer() {
    return nullptr;
  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() = 0;

  virtual std::string getGroupName() const {
    return group_;
  }

  virtual std::string getClassName() = 0;
  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::vector<std::string> getClassNames() = 0;

  virtual std::unique_ptr<ObjectFactory> assign(const std::string &class_name) = 0;

 private:
  std::string group_;
};
/**
 * Factory that is used as an interface for
 * creating processors from shared objects.
 */
template<class T>
class DefautObjectFactory : public ObjectFactory {
 public:
  DefautObjectFactory() {
    className = core::getClassName<T>();
  }

  DefautObjectFactory(const std::string &group_name) // NOLINT
      : ObjectFactory(group_name) {
    className = core::getClassName<T>();
  }
  /**
   * Virtual destructor.
   */
  virtual ~DefautObjectFactory() = default;

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string &name) {
    std::shared_ptr<T> ptr = std::make_shared<T>(name);
    return std::static_pointer_cast<CoreComponent>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual std::shared_ptr<CoreComponent> create(const std::string &name, const utils::Identifier &uuid) {
    std::shared_ptr<T> ptr = std::make_shared<T>(name, uuid);
    return std::static_pointer_cast<CoreComponent>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent* createRaw(const std::string &name) {
    T *ptr = new T(name);
    return dynamic_cast<CoreComponent*>(ptr);
  }

  /**
   * Create a shared pointer to a new processor.
   */
  virtual CoreComponent* createRaw(const std::string &name, const utils::Identifier &uuid) {
    T *ptr = new T(name, uuid);
    return dynamic_cast<CoreComponent*>(ptr);
  }

  /**
   * Gets the name of the object.
   * @return class name of processor
   */
  virtual std::string getName() {
    return className;
  }

  /**
   * Gets the class name for the object
   * @return class name for the processor.
   */
  virtual std::string getClassName() {
    return className;
  }

  virtual std::vector<std::string> getClassNames() {
    std::vector<std::string> container;
    container.push_back(className);
    return container;
  }

  virtual std::unique_ptr<ObjectFactory> assign(const std::string& /*class_name*/) {
    return nullptr;
  }

 protected:
  std::string className;
};

}  // namespace core
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
