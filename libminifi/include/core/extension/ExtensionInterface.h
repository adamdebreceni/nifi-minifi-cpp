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

#include <memory>
#include <vector>
#include "properties/Configure.h"

#ifndef WIN32
#define DLL_EXPORT
#else
#define DLL_EXPORT __declspec(dllexport)
#endif

extern "C" DLL_EXPORT bool initializeExtension(const std::shared_ptr<org::apache::nifi::minifi::Configure>& config);
extern "C" DLL_EXPORT void deinitializeExtension();
