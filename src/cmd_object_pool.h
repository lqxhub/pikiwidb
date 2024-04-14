/*
 * Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <algorithm>
#include <iostream>
#include <memory>

#include "base_cmd.h"

namespace pikiwidb {

struct SmallObject {
  // Version, after each use, a new version number is assigned
  SmallObject(uint64_t version, std::string key, std::unique_ptr<BaseCmd> object)
      : count_(version), key_(std::move(key)), object_(std::move(object)) {}
  uint64_t count_;
  std::string key_;
  std::unique_ptr<BaseCmd> object_;
};

class CmdObjectPool {
 public:
  inline void SetNewObjectFunc(const std::string &key, std::function<std::unique_ptr<BaseCmd>()> &&func) {
    create_object_.emplace(key, std::move(func));
  }

  inline void SetNewObjectFunc(
      std::unordered_map<std::string, std::function<std::unique_ptr<BaseCmd>()>> &createObject) {
    create_object_.insert(createObject.begin(), createObject.end());
  }

  void InitCommand();

  // take all the objects and create them first
  void InitObjectPool();

  // Get the object from localPool first, if not found, get it from the global pool
  std::unique_ptr<BaseCmd> GetObject(const std::string &key);

  // recycle an object
  void PutObject(std::string &&key, std::unique_ptr<BaseCmd> &&v);

  // get the command object from the object pool
  std::pair<std::unique_ptr<BaseCmd>, CmdRes::CmdRet> GetCommand(const std::string &cmdName, PClient *client);

  thread_local static std::unique_ptr<std::vector<SmallObject>> tl_local_pool;
  thread_local static uint64_t tl_counter;

 private:
  // get the object from the global pool
  std::unique_ptr<BaseCmd> GetObjectByGlobal(const std::string &key);

  // put the object back to the global pool
  void PutObjectBackGlobal(const std::string &key, std::unique_ptr<BaseCmd> &v);

  inline std::string ProcessSubcommandName(const std::string &cmdName) {
    auto newName = cmdName;
    std::transform(newName.begin(), newName.begin() + 1, newName.begin(), ::toupper);
    return newName;
  }

 private:
  // cache objects
  std::unordered_map<std::string, std::unique_ptr<BaseCmd>> pool_;

  // Based on the key, find the function that creates the object
  std::unordered_map<std::string, std::function<std::unique_ptr<BaseCmd>()>> create_object_;
  std::mutex mutex_;

  // thread local pool size
  const uint64_t local_max_ = 10;
  // How many operations have passed, check if you need to put the extra objects back into the global pool
  const uint64_t check_rate_ = 20;

  // After multiple checks, if the number of times the object is used is still 1,
  // it is also removed from the local_pool
  const uint64_t less_use_check_rate_ = 3;
};

}  // namespace pikiwidb
