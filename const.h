#ifndef SCREEN_WORMS_CONST_H
#define SCREEN_WORMS_CONST_H

#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <algorithm>
#include <iostream>

#define DATAGRAM_SIZE 550
#define MIN_CLIENT_MSG_LEN 13
#define MAX_CLIENT_MSG_LEN 33

using Coord = std::pair<uint32_t, uint32_t>;

#endif //SCREEN_WORMS_CONST_H