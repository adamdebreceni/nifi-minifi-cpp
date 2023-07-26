#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.
#
include(FetchContent)
set(ABSL_PROPAGATE_CXX_STD ON CACHE INTERNAL absl-propagate-cxx)
set(ABSL_ENABLE_INSTALL ON CACHE INTERNAL "")
set(BUILD_TESTING OFF CACHE STRING "" FORCE)
FetchContent_Declare(
        absl
        URL      https://github.com/abseil/abseil-cpp/archive/refs/tags/20220623.1.tar.gz
        URL_HASH SHA256=91ac87d30cc6d79f9ab974c51874a704de9c2647c40f6932597329a282217ba8
)
FetchContent_MakeAvailable(absl)
