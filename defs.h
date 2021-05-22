#ifndef ROBAKI_DEFS_H
#define ROBAKI_DEFS_H

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cassert>
#include <cstring>

#include <chrono>
#include <string>
#include <vector>
#include <set>
#include <map>

#include "err.h"


namespace Worms {
    using timestamp_t = std::chrono::time_point<std::chrono::system_clock,
        std::chrono::microseconds>;
}

#endif //ROBAKI_DEFS_H
