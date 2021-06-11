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
#include <tuple>
#include <chrono>
#include <functional>
#include <system_error>
#include <memory>
#include "core/logging/Logger.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace utils {
namespace file {

namespace FileUtils = ::org::apache::nifi::minifi::utils::file;

namespace detail {
int platform_create_dir(const std::string& path);
}  // namespace detail

/*
 * Get the platform-specific path separator.
 * @param force_posix returns the posix path separator ('/'), even when not on posix. Useful when dealing with remote posix paths.
 * @return the path separator character
 */
char get_separator(bool force_posix = false);

std::string normalize_path_separators(std::string path, bool force_posix = false);

std::string get_temp_directory();

int64_t delete_dir(const std::string &path, bool delete_files_recursively = true);

uint64_t last_write_time(const std::string &path);

std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds> last_write_time_point(const std::string &path);

uint64_t file_size(const std::string &path);

bool set_last_write_time(const std::string &path, uint64_t write_time);

#ifndef WIN32
bool get_permissions(const std::string &path, uint32_t &permissions);
int set_permissions(const std::string &path, const uint32_t permissions);
#endif

#ifndef WIN32
bool get_uid_gid(const std::string &path, uint64_t &uid, uint64_t &gid);
#endif

bool is_directory(const char * path);

bool exists(const std::string& path);

int create_dir(const std::string& path, bool recursive = true);

int copy_file(const std::string &path_from, const std::string& dest_path);

void addFilesMatchingExtension(const std::shared_ptr<core::logging::Logger> &logger, const std::string &originalPath, const std::string &extension, std::vector<std::string> &accruedFiles);

/*
 * Provides a platform-independent function to list a directory
 * Callback is called for every file found: first argument is the path of the directory, second is the filename
 * Return value of the callback is used to continue (true) or stop (false) listing
 */
void list_dir(const std::string& dir, std::function<bool(const std::string&, const std::string&)> callback,
                     const std::shared_ptr<core::logging::Logger> &logger, bool recursive = true);

std::vector<std::pair<std::string, std::string>> list_dir_all(const std::string& dir, const std::shared_ptr<core::logging::Logger> &logger, bool recursive = true);

std::string concat_path(const std::string& root, const std::string& child, bool force_posix = false);

std::string create_temp_directory(char* format);

std::tuple<std::string /*parent_path*/, std::string /*child_path*/> split_path(const std::string& path, bool force_posix = false);

std::string get_parent_path(const std::string& path, bool force_posix = false);

std::string get_child_path(const std::string& path, bool force_posix = false);

bool is_hidden(const std::string& path);

/*
 * Returns the absolute path of the current executable
 */
std::string get_executable_path();

std::string resolve(const std::string& base, const std::string& path);

/*
 * Returns the absolute path to the directory containing the current executable
 */
std::string get_executable_dir();

int close(int file_descriptor);

int access(const char *path_name, int mode);

#ifdef WIN32
std::error_code hide_file(const char* const file_name);
#endif /* WIN32 */

uint64_t computeChecksum(const std::string &file_name, uint64_t up_to_position);

std::string get_file_content(const std::string &file_name);

}  // namespace file
}  // namespace utils
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
