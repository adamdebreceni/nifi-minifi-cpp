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

#include "Utils.h"

#include <vector>
#include <algorithm>
#include  <cctype>
#include  <regex>
#include  <sstream>

#include "utils/StringUtils.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {

std::vector<std::string> inputStringToList(const std::string& str) {
  std::vector<std::string> fragments = StringUtils::split(str, ",");
  for (auto& item : fragments) {
    item = StringUtils::toLower(StringUtils::trim(item));
  }
  fragments.erase(std::remove(fragments.begin(), fragments.end(), ""), fragments.end());

  return fragments;
}

std::string escape(std::string str) {
  return StringUtils::replaceMap(std::move(str), {
    {"&", "&amp;"}, {"=", "&equals;"}, {",", "&comma;"}
  });
}

std::string unescape(std::string str) {
  return StringUtils::replaceMap(std::move(str), {
      {"&amp;", "&"}, {"&equals;", "="}, {"&comma;", ","}
  });
}

} /* namespace utils */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */
