/*
 * Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#include "cmd_object_pool.h"
#include "base_cmd.h"
#include "cmd_admin.h"
#include "cmd_hash.h"
#include "cmd_keys.h"
#include "cmd_kv.h"
#include "cmd_list.h"
#include "cmd_set.h"
#include "cmd_zset.h"
#include "pikiwidb.h"

extern std::unique_ptr<PikiwiDB> g_pikiwidb;

namespace pikiwidb {

// thread_local variable
thread_local std::unique_ptr<std::vector<SmallObject>> CmdObjectPool::tl_local_pool = nullptr;
thread_local uint64_t CmdObjectPool::tl_counter = 0;

#define ADD_COMMAND(cmd, argc)                                                                                        \
  do {                                                                                                                \
    g_pikiwidb->GetCmdObjectPool()->SetNewObjectFunc(                                                                 \
        kCmdName##cmd, []() -> std::unique_ptr<BaseCmd> { return std::make_unique<cmd##Cmd>(kCmdName##cmd, argc); }); \
  } while (0)

// subcommand name is baseCmdName + subCmdName
// for example: ConfigGetCmd is the subcommand of configGet
#define ADD_SUB_COMMAND(group, subCmd, argc)                                                                       \
  do {                                                                                                             \
    g_pikiwidb->GetCmdObjectPool()->SetNewObjectFunc(kCmdName##group + #subCmd, []() -> std::unique_ptr<BaseCmd> { \
      return std::make_unique<group##subCmd##Cmd>(kCmdName##group + #subCmd, argc);                                \
    });                                                                                                            \
  } while (0)

void CmdObjectPool::InitCommand() {
  // admin
  ADD_COMMAND(Config, -2);
  ADD_SUB_COMMAND(Config, Get, -3);
  ADD_SUB_COMMAND(Config, Set, -4);

  // server
  ADD_COMMAND(Flushdb, 1);
  ADD_COMMAND(Flushall, 1);
  ADD_COMMAND(Select, 2);
  ADD_COMMAND(Shutdown, 1);

  // keyspace
  ADD_COMMAND(Del, -2);
  ADD_COMMAND(Exists, -2);
  ADD_COMMAND(Type, 2);
  ADD_COMMAND(Expire, 3);
  ADD_COMMAND(Ttl, 2);
  ADD_COMMAND(PExpire, 3);
  ADD_COMMAND(Expireat, 3);
  ADD_COMMAND(PExpireat, 3);
  ADD_COMMAND(Pttl, 2);
  ADD_COMMAND(Persist, 2);
  ADD_COMMAND(Keys, 2);

  // kv
  ADD_COMMAND(Get, 2);
  ADD_COMMAND(Set, -3);
  ADD_COMMAND(MGet, -2);
  ADD_COMMAND(MSet, -3);
  ADD_COMMAND(GetSet, 3);
  ADD_COMMAND(SetNX, 3);
  ADD_COMMAND(Append, 3);
  ADD_COMMAND(Strlen, 2);
  ADD_COMMAND(Incr, 2);
  ADD_COMMAND(Incrby, 3);
  ADD_COMMAND(Decrby, 3);
  ADD_COMMAND(IncrbyFloat, 3);
  ADD_COMMAND(SetEx, 4);
  ADD_COMMAND(PSetEx, 4);
  ADD_COMMAND(BitOp, -4);
  ADD_COMMAND(BitCount, -2);
  ADD_COMMAND(GetBit, 3);
  ADD_COMMAND(GetRange, 4);
  ADD_COMMAND(SetRange, 4);
  ADD_COMMAND(Decr, 2);
  ADD_COMMAND(SetBit, 4);
  ADD_COMMAND(MSetnx, -3);

  // hash
  ADD_COMMAND(HSet, -4);
  ADD_COMMAND(HGet, 3);
  ADD_COMMAND(HDel, -3);
  ADD_COMMAND(HMSet, -4);
  ADD_COMMAND(HMGet, -3);
  ADD_COMMAND(HGetAll, 2);
  ADD_COMMAND(HKeys, 2);
  ADD_COMMAND(HLen, 2);
  ADD_COMMAND(HStrLen, 3);
  ADD_COMMAND(HScan, -3);
  ADD_COMMAND(HVals, 2);
  ADD_COMMAND(HIncrbyFloat, 4);
  ADD_COMMAND(HSetNX, 4);
  ADD_COMMAND(HIncrby, 4);
  ADD_COMMAND(HRandField, -2);
  ADD_COMMAND(HExists, 3);

  // set
  ADD_COMMAND(SIsMember, 3);
  ADD_COMMAND(SAdd, -3);
  ADD_COMMAND(SUnionStore, -3);
  ADD_COMMAND(SRem, -3);
  ADD_COMMAND(SInter, -2);
  ADD_COMMAND(SUnion, -2);
  ADD_COMMAND(SInterStore, -3);
  ADD_COMMAND(SCard, 2);
  ADD_COMMAND(SMove, 4);
  ADD_COMMAND(SRandMember, -2);  // Added the count argument since Redis 3.2.0
  ADD_COMMAND(SPop, -2);
  ADD_COMMAND(SMembers, 2);
  ADD_COMMAND(SDiff, -2);
  ADD_COMMAND(SDiffstore, -3);
  ADD_COMMAND(SScan, -3);

  // list
  ADD_COMMAND(LPush, -3);
  ADD_COMMAND(RPush, -3);
  ADD_COMMAND(RPop, 2);
  ADD_COMMAND(LRem, 4);
  ADD_COMMAND(LRange, 4);
  ADD_COMMAND(LTrim, 4);
  ADD_COMMAND(LSet, 4);
  ADD_COMMAND(LInsert, 5);
  ADD_COMMAND(LPushx, -3);
  ADD_COMMAND(RPushx, -3);
  ADD_COMMAND(LPop, 2);
  ADD_COMMAND(LIndex, 3);
  ADD_COMMAND(LLen, 2);

  // zset
  ADD_COMMAND(ZAdd, -4);
  ADD_COMMAND(ZRevrange, -4);
  ADD_COMMAND(ZRangebyscore, -4);
  ADD_COMMAND(ZRemrangebyscore, 4);
  ADD_COMMAND(ZRemrangebyrank, 4);
  ADD_COMMAND(ZRevrangebyscore, -4);
  ADD_COMMAND(ZCard, 2);
  ADD_COMMAND(ZScore, 3);
  ADD_COMMAND(ZRange, -4);
  ADD_COMMAND(ZRangebylex, -3);
  ADD_COMMAND(ZRevrangebylex, -3);
  ADD_COMMAND(ZRank, 3);
  ADD_COMMAND(ZRevrank, 3);
  ADD_COMMAND(ZRem, -3);
  ADD_COMMAND(ZIncrby, 4);
}

void CmdObjectPool::InitObjectPool() {
  for (const auto &object : create_object_) {
    pool_.emplace(object.first, object.second());
  }
}

std::unique_ptr<BaseCmd> pikiwidb::CmdObjectPool::GetObject(const std::string &key) {
  for (auto &obj : *tl_local_pool) {
    if (obj.key_ == key && obj.object_) {
      ++obj.count_;
      return std::move(obj.object_);
    }
  }

  // if you don't find it go to the global search
  return GetObjectByGlobal(key);
}

void CmdObjectPool::PutObject(std::string &&key, std::unique_ptr<BaseCmd> &&v) {
  //  std::cout << std::this_thread::get_id() << " >> " << (&local_pool) << std::endl;
  ++tl_counter;

  bool needPush = true;
  for (auto &obj : *tl_local_pool) {
    if (obj.key_ == key && !obj.object_) {
      // if the keys are the same and the pointer is empty
      obj.object_ = std::move(v);
      needPush = false;
      break;
    }
  }

  if (needPush) {
    // put the used object back into localPool
    tl_local_pool->emplace_back(tl_counter, std::move(key), std::move(v));
  }

  // whether to delete rarely used objects
  bool reclaim_rarely_used = tl_counter % (check_rate_ * less_use_check_rate_) == 0;

  if (tl_counter % check_rate_ != 0 || (!reclaim_rarely_used && tl_local_pool->size() <= local_max_)) {
    return;
  }

  // The version number is sorted from largest to smallest
  std::sort(tl_local_pool->begin(), tl_local_pool->end(),
            [](const SmallObject &a, const SmallObject &b) { return a.count_ > b.count_; });

  //  std::cout << "-------------------------------------------" << std::endl;
  //  for (const auto &smallObj : *localPool) {
  //    std::cout << "!!" << smallObj.key_ << "!!" << std::endl;
  //  }

  // delete the last few
  for (auto it = tl_local_pool->rbegin(); it != tl_local_pool->rend();) {
    if (reclaim_rarely_used && it->count_ > 1) {
      // If you need to delete a low frequency of use, but the current number of uses is greater than the minimum,
      // you can jump out of the loop
      break;
    }

    PutObjectBackGlobal(it->key_, it->object_);
    it = std::vector<SmallObject>::reverse_iterator(tl_local_pool->erase((++it).base()));

    //    std::cout << "smallPool.size:" << local_pool->size() << " smallPool.cap:" << local_pool->capacity() <<
    //    std::endl;

    if (!reclaim_rarely_used && tl_local_pool->size() <= local_max_) {
      // If you don't need to delete the low frequency of use, and the size of the local pool is less than the maximum,
      // you can jump out of the loop
      break;
    }
  }

  // debug打印
  //  for (const auto &smallObj : *localPool) {
  //    std::cout << "!!" << smallObj.key_ << "!!" << std::endl;
  //  }
  //  std::cout << "-------------------------------------------" << std::endl;
}

std::unique_ptr<BaseCmd> CmdObjectPool::GetObjectByGlobal(const std::string &key) {
  {
    std::lock_guard l(mutex_);
    auto it = pool_.find(key);
    if (it != pool_.end()) {
      std::unique_ptr<BaseCmd> obj = std::move(it->second);
      pool_.erase(it);
      return obj;
    }
  }

  // If you can't find it, use the func function to create a corresponding object
  auto func = create_object_.find(key);
  if (func == create_object_.end()) {
    return nullptr;
  }
  return func->second();
}

void CmdObjectPool::PutObjectBackGlobal(const std::string &key, std::unique_ptr<BaseCmd> &v) {
  std::lock_guard l(mutex_);
  pool_[key] = std::move(v);
}

std::pair<std::unique_ptr<BaseCmd>, CmdRes::CmdRet> CmdObjectPool::GetCommand(const std::string &cmdName,
                                                                              PClient *client) {
  auto cmd = GetObject(cmdName);
  if (cmd == nullptr) {
    return std::pair(nullptr, CmdRes::kSyntaxErr);
  }

  if (cmd->HasSubCommand()) {
    if (client->argv_.size() < 2) {
      return std::pair(nullptr, CmdRes::kInvalidParameter);
    }
    auto subCmd = GetObject(cmdName + ProcessSubcommandName(client->argv_[1]));
    return std::pair(std::move(subCmd), CmdRes::kSyntaxErr);
  }
  return std::pair(std::move(cmd), CmdRes::kSyntaxErr);
}

}  // namespace pikiwidb
