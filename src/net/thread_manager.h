/*
 * Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

#include "callback_function.h"
#include "io_thread.h"

#include "config.h"

#if defined(HAVE_EPOLL)

#  include "epoll_event.h"

#elif defined(HAVE_KQUEUE)

#  include "kqueue_event.h"

#endif

namespace net {

template <typename T>
requires HasSetFdFunction<T>
class ThreadManager {
 public:
  explicit ThreadManager(int8_t index, bool rwSeparation = true) : index_(index), rwSeparation_(rwSeparation) {}

  ~ThreadManager();

  // set new connect callback function
  inline void SetOnCreate(const OnCreate<T> &func) { onCreate_ = func; }

  inline void SetOnConnect(const OnCreate<T> &func) { onConnect_ = func; }

  // set read message callback function
  inline void SetOnMessage(const OnMessage<T> &func) { onMessage_ = func; }

  // set close connect callback function
  inline void SetOnClose(const OnClose<T> &func) { onClose_ = func; }

  // Start the thread and initialize the event
  bool Start(const std::shared_ptr<NetEvent> &listen, const std::shared_ptr<Timer> &timer);

  // Stop the thread
  void Stop();

  // Create a new connection callback function
  void OnNetEventCreate(int fd, const std::shared_ptr<Connection> &conn);

  // Read message callback function
  void OnNetEventMessage(int fd, std::string &&readData);

  // Close connection callback function
  void OnNetEventClose(int fd, std::string &&err);

  // Server actively closes the connection
  void CloseConnection(int fd);

  void TCPConnect(const SocketAddr &addr, std::unique_ptr<NetEvent> netEvent);

  void TCPConnect(const SocketAddr &addr, std::unique_ptr<NetEvent> netEvent, OnCreate<T> onConnect);

  void Wait();

  // Send message to the client
  void SendPacket(const T &conn, std::string &&msg);

 private:
  // Create read thread
  bool CreateReadThread(const std::shared_ptr<NetEvent> &listen, const std::shared_ptr<Timer> &timer);

  // Create write thread if rwSeparation_ is true
  bool CreateWriteThread();

  void DoTCPConnect(T &t, int fd, const std::shared_ptr<Connection> &conn);

 private:
  const bool rwSeparation_ = true;    // Whether to separate read and write threads
  const int8_t index_ = 0;            // The index of the thread
  std::atomic<bool> running_ = true;  // Whether the thread is running

  std::unique_ptr<IOThread> readThread_;   // Read thread
  std::unique_ptr<IOThread> writeThread_;  // Write thread

  // All connections for the current thread
  std::unordered_map<int, std::pair<T, std::shared_ptr<Connection>>> connections_;

  std::shared_mutex mutex_;

  OnCreate<T> onCreate_;

  OnCreate<T> onConnect_;

  OnMessage<T> onMessage_;

  OnClose<T> onClose_;
};

template <typename T>
requires HasSetFdFunction<T>
ThreadManager<T>::~ThreadManager() {
  Stop();
}

template <typename T>
requires HasSetFdFunction<T>
bool ThreadManager<T>::Start(const std::shared_ptr<NetEvent> &listen, const std::shared_ptr<Timer> &timer) {
  if (!CreateReadThread(listen, timer)) {
    return false;
  }
  if (rwSeparation_) {
    return CreateWriteThread();
  }
  return true;
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::Stop() {
  bool expected = true;
  if (running_.compare_exchange_strong(expected, false)) {
    readThread_->Stop();
    if (rwSeparation_) {
      writeThread_->Stop();
    }
  }
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::OnNetEventCreate(int fd, const std::shared_ptr<Connection> &conn) {
  T t;

  if constexpr (IsPointer_v<T>) {
    InitPointer(t);
    t->SetFd(fd);
    t->SetThreadIndex(index_);
  } else {
    t.SetFd(fd);
    t.SetThreadIndex(index_);
  }

  {
    std::lock_guard lock(mutex_);
    connections_.emplace(fd, std::make_pair(t, conn));
  }
  readThread_->AddNewEvent(conn->fd_, BaseEvent::EVENT_READ | BaseEvent::EVENT_ERROR | BaseEvent::EVENT_HUB);

  onCreate_(fd, &t, conn->addr_);
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::OnNetEventMessage(int fd, std::string &&readData) {
  T t;
  {
    std::shared_lock lock(mutex_);
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
      return;
    }
    t = iter->second.first;
  }
  onMessage_(std::move(readData), t);
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::OnNetEventClose(int fd, std::string &&err) {
  std::lock_guard lock(mutex_);
  auto iter = connections_.find(fd);
  if (iter == connections_.end()) {
    return;
  }
  onClose_(iter->second.first, std::move(err));
  iter->second.second->netEvent_->Close();  // close socket
  connections_.erase(iter);
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::CloseConnection(int fd) {
  readThread_->CloseConnection(fd);
  if (rwSeparation_) {
    writeThread_->CloseConnection(fd);
  }
  OnNetEventClose(fd, "");
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::TCPConnect(const SocketAddr &addr, std::unique_ptr<NetEvent> netEvent) {
  auto newConn = std::make_shared<Connection>(std::move(netEvent));
  newConn->addr_ = addr;
  T t;
  if constexpr (IsPointer_v<T>) {
    InitPointer(t);
  }
  DoTCPConnect(t, newConn->netEvent_->Fd(), newConn);
  onConnect_(newConn->netEvent_->Fd(), &t, addr);
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::TCPConnect(const SocketAddr &addr, std::unique_ptr<NetEvent> netEvent, OnCreate<T> onConnect) {
  auto newConn = std::make_shared<Connection>(std::move(netEvent));
  newConn->addr_ = addr;
  T t;
  if constexpr (IsPointer_v<T>) {
    InitPointer(t);
  }
  DoTCPConnect(t, newConn->netEvent_->Fd(), newConn);
  onConnect(newConn->netEvent_->Fd(), &t, addr);
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::Wait() {
  readThread_->Wait();
  if (rwSeparation_) {
    writeThread_->Wait();
  }
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::SendPacket(const T &conn, std::string &&msg) {
  std::shared_lock lock(mutex_);
  int fd = 0;
  if constexpr (IsPointer_v<T>) {
    fd = conn->GetFd();
  } else {
    fd = conn.GetFd();
  }
  std::shared_ptr<Connection> connPtr;
  {
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
      return;
    }
    connPtr = iter->second.second;
  }

  connPtr->netEvent_->SendPacket(std::move(msg));

  if (rwSeparation_) {
    writeThread_->SetWriteEvent(fd);
  } else {
    readThread_->SetWriteEvent(fd);
  }
}

template <typename T>
requires HasSetFdFunction<T>
bool ThreadManager<T>::CreateReadThread(const std::shared_ptr<NetEvent> &listen, const std::shared_ptr<Timer> &timer) {
  std::shared_ptr<BaseEvent> event;
  int8_t eventMode = BaseEvent::EVENT_MODE_READ;
  if (!rwSeparation_) {
    eventMode |= BaseEvent::EVENT_MODE_WRITE;
  }

#if defined(HAVE_EPOLL)
  event = std::make_shared<EpollEvent>(listen, eventMode);
#elif defined(HAVE_KQUEUE)
  event = std::make_shared<KqueueEvent>(listen, eventMode);
#endif

  event->AddTimer(timer);

  event->SetOnCreate([this](int fd, const std::shared_ptr<Connection> &conn) { OnNetEventCreate(fd, conn); });

  event->SetOnMessage([this](int fd, std::string &&readData) { OnNetEventMessage(fd, std::move(readData)); });

  event->SetOnClose([this](int fd, std::string &&err) { OnNetEventClose(fd, std::move(err)); });

  event->SetGetConn([this](int fd) -> std::shared_ptr<Connection> {
    std::shared_lock lock(mutex_);
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
      return nullptr;
    }
    return iter->second.second;
  });

  readThread_ = std::make_unique<IOThread>(event);
  return readThread_->Run();
}

template <typename T>
requires HasSetFdFunction<T>
bool ThreadManager<T>::CreateWriteThread() {
  std::shared_ptr<BaseEvent> event;

#if defined(HAVE_EPOLL)
  event = std::make_shared<EpollEvent>(nullptr, BaseEvent::EVENT_MODE_WRITE);
#elif defined(HAVE_KQUEUE)
  event = std::make_shared<KqueueEvent>(nullptr, BaseEvent::EVENT_MODE_WRITE);
#endif

  event->SetOnClose([this](int fd, std::string &&msg) { OnNetEventClose(fd, std::move(msg)); });
  event->SetGetConn([this](int fd) -> std::shared_ptr<Connection> {
    std::shared_lock lock(mutex_);
    auto iter = connections_.find(fd);
    if (iter == connections_.end()) {
      return nullptr;
    }
    return iter->second.second;
  });

  writeThread_ = std::make_unique<IOThread>(event);
  return writeThread_->Run();
}

template <typename T>
requires HasSetFdFunction<T>
void ThreadManager<T>::DoTCPConnect(T &t, int fd, const std::shared_ptr<Connection> &conn) {
  if constexpr (IsPointer_v<T>) {
    t->SetFd(fd);
    t->SetThreadIndex(index_);
  } else {
    t.SetFd(fd);
    t.SetThreadIndex(index_);
  }

  {
    std::lock_guard lock(mutex_);
    connections_.emplace(fd, std::make_pair(t, conn));
  }
  readThread_->AddNewEvent(fd, BaseEvent::EVENT_READ | BaseEvent::EVENT_ERROR | BaseEvent::EVENT_HUB);
}

}  // namespace net
