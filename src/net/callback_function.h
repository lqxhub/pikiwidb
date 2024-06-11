/*
 * Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <functional>
#include <memory>
#include "socket_addr.h"

namespace net {

template <typename T>
struct IsPointer : std::false_type {};

template <typename T>
struct IsPointer<T *> : std::true_type {};

template <typename T>
struct IsPointer<std::shared_ptr<T>> : std::true_type {};

template <typename T>
struct IsPointer<std::unique_ptr<T>> : std::true_type {};

template <typename T>
constexpr bool IsPointer_v = IsPointer<T>::value;

// 初始化原生指针的特化模板
template <typename T>
void InitPointer(T *&t) {
  t = new T();
}

// 初始化 std::shared_ptr 的特化模板
template <typename T>
void InitPointer(std::shared_ptr<T> &t) {
  t = std::make_shared<T>();
}

// 初始化 std::unique_ptr 的特化模板
template <typename T>
void InitPointer(std::unique_ptr<T> &t) {
  t = std::make_unique<T>();
}

template <typename T>
concept HasSetFdFunction = requires(T t, int id, int8_t index) {
  // If T is of pointer type, then dereference and call the member function
  { (*t).SetFd(id) } -> std::same_as<void>;              // SetFd return type is void
  { (*t).GetFd() } -> std::same_as<int>;                 // GetFd return type is int
  { (*t).SetThreadIndex(index) } -> std::same_as<void>;  // SetThreadIndex return type is void
  { (*t).GetThreadIndex() } -> std::same_as<int8_t>;     // GetThreadIndex return type is int8_t
  //  { (*t).SetSocketAddr(addr) } -> std::same_as<void>;    // GetThreadIndex return type is int8_t
} || std::is_class_v<T>;  // If T is an ordinary class, the member function is called directly

template <typename T>
requires HasSetFdFunction<T>
using OnCreate = std::function<void(int fd, T *t, const SocketAddr &addr)>;

template <typename T>
requires HasSetFdFunction<T>
using OnMessage = std::function<void(std::string &&msg, T &t)>;

template <typename T>
requires HasSetFdFunction<T>
using OnClose = std::function<void(T &t, std::string &&err)>;

// class BaseEvent;

class NetEvent;

// class SocketAddr;

// Auxiliary structure
struct Connection {
  explicit Connection(std::unique_ptr<NetEvent> netEvent) : netEvent_(std::move(netEvent)), addr_(0, 0) {}

  ~Connection() = default;

  //  std::shared_ptr<BaseEvent> poll_;
  std::unique_ptr<NetEvent> netEvent_;

  SocketAddr addr_;

  int fd_ = 0;
};

}  // namespace net
