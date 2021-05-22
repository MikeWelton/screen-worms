#ifndef SCREEN_WORMS_UTIL_FUNC_H
#define SCREEN_WORMS_UTIL_FUNC_H

#include <cstdint>
#include <string>
#include <vector>
#include "../common/const.h"

uint32_t crc32(const char *data, uint32_t size);

int string_to_int(const std::string &str);

bool player_name_valid(const std::string &name);

struct addrinfo resolve_host(const std::string& addr, int type, const std::string &port);

std::vector<std::string> split(const std::string &str, const std::string &delimiter);

Coord normalized_vector(uint32_t angle);

void exit_error(const std::string &msg);

#endif //SCREEN_WORMS_UTIL_FUNC_H
