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

function(use_bundled_llamacpp SOURCE_DIR BINARY_DIR)
    set(PC "${Patch_EXECUTABLE}" -p1 -i "${SOURCE_DIR}/thirdparty/llamacpp/metal.patch")

    set(BYPRODUCTS
        "lib/libllama.a"
        "lib/libggml.a"
    )

    set(LLAMACPP_CMAKE_ARGS ${PASSTHROUGH_CMAKE_ARGS}
        "-DCMAKE_INSTALL_PREFIX=${BINARY_DIR}/thirdparty/llamacpp-install"
        -DBUILD_SHARED_LIBS=OFF
        -DLLAMA_BUILD_TESTS=OFF
        -DLLAMA_BUILD_EXAMPLES=OFF
        -DLLAMA_BUILD_SERVER=OFF
    )

    append_third_party_passthrough_args(LLAMACPP_CMAKE_ARGS "${LLAMACPP_CMAKE_ARGS}")

    ExternalProject_Add(
            llamacpp-external
            URL https://github.com/ggerganov/llama.cpp/archive/refs/tags/b3899.tar.gz
            URL_HASH "SHA256=9b5b466c67ebc097c10e427a2c48f4a76e9a518f1148c7abfd73d363acab2381"
            SOURCE_DIR "${BINARY_DIR}/thirdparty/llamacpp-src"
            CMAKE_ARGS ${LLAMACPP_CMAKE_ARGS}
            BUILD_BYPRODUCTS ${BYPRODUCTS}
            PATCH_COMMAND ${PC}
            EXCLUDE_FROM_ALL TRUE
    )

    set(LLAMACPP_FOUND "YES" CACHE STRING "" FORCE)
    set(LLAMACPP_INCLUDE_DIR "${BINARY_DIR}/thirdparty/llamacpp-install/include" CACHE STRING "" FORCE)
    set(LLAMACPP_LIBRARIES "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libllama.a;${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml.a" CACHE STRING "" FORCE)

    add_library(LlamaCpp::llama STATIC IMPORTED)
    set_target_properties(LlamaCpp::llama PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libllama.a")
    add_dependencies(LlamaCpp::llama llamacpp-external)

    add_library(LlamaCpp::ggml STATIC IMPORTED)
    set_target_properties(LlamaCpp::ggml PROPERTIES IMPORTED_LOCATION "${BINARY_DIR}/thirdparty/llamacpp-install/lib/libggml.a")
    add_dependencies(LlamaCpp::ggml llamacpp-external)

    add_library(llamacpp INTERFACE)
    target_link_libraries(llamacpp INTERFACE LlamaCpp::llama)
    target_link_libraries(llamacpp INTERFACE LlamaCpp::ggml)
    file(MAKE_DIRECTORY ${LLAMACPP_INCLUDE_DIR})
    target_include_directories(llamacpp INTERFACE ${LLAMACPP_INCLUDE_DIR})

    if (APPLE)
        target_link_libraries(llamacpp INTERFACE "-framework Metal" "-framework CoreFoundation" "-framework Foundation" "-framework Accelerate")
    endif()
endfunction()