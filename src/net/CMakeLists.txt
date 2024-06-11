# Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
# This source code is licensed under the BSD-style license found in the
# LICENSE file in the root directory of this source tree. An additional grant
# of patent rights can be found in the PATENTS file in the same directory.

cmake_minimum_required(VERSION 3.25)

set(CMAKE_CXX_STANDARD 20)

project(net)
AUX_SOURCE_DIRECTORY(. NET_SRC)

ADD_LIBRARY(net ${NET_SRC})

TARGET_LINK_LIBRARIES(net)