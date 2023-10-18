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

include(FetchContent)

FetchContent_Declare(jolt_tests
        URL      https://github.com/bazaarvoice/jolt/archive/refs/tags/jolt-0.1.8.tar.gz
        URL_HASH SHA256=7423c5b98244260f89a975f5e21150c02a6a1fa88e3af07c90d43fef0eebdcbb
        )

FetchContent_GetProperties(jolt_tests)
if (NOT jolt_tests_POPULATED)
    FetchContent_Populate(jolt_tests)
endif()
