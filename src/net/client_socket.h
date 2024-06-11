/*
 * Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

// #include "base_socket.h"
#include "stream_socket.h"

namespace net {

class ClientSocket : public StreamSocket {
 public:
  explicit ClientSocket(const SocketAddr& addr) : StreamSocket(0, SOCKET_TCP), addr_(addr){};

  ~ClientSocket() override = default;

  bool Connect();

  inline void SetFailCallback(const std::function<void(std::string)>& cb) { onConnectFail_ = cb; }

  //  inline void SetOnCreate(std::function<void(int fd, const std::shared_ptr<Connection>)> &onCreate) {
  //    onCreate_ = std::move(onCreate);
  //  }

 private:
  SocketAddr addr_;
  std::function<void(std::string)> onConnectFail_;
  std::function<void(int fd, const std::shared_ptr<Connection>)> onCreate_;
};

}  // namespace net
