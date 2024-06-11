/*
 * Copyright (c) 2023-present, Qihoo, Inc.  All rights reserved.
 * This source code is licensed under the BSD-style license found in the
 * LICENSE file in the root directory of this source tree. An additional grant
 * of patent rights can be found in the PATENTS file in the same directory.
 */

#pragma once

namespace net {

#ifdef __linux__
#  define HAVE_EPOLL 1
#endif

#if (defined(__APPLE__) && defined(MAC_OS_X_VERSION_10_6)) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
#  define HAVE_KQUEUE 1
#endif

#ifdef __linux__
#  define HAVE_ACCEPT4 1
#endif

}  // namespace net
