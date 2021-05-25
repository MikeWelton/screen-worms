#ifndef SCREEN_WORMS_UTIL_FUNC_H
#define SCREEN_WORMS_UTIL_FUNC_H

#include <cstdint>
#include <string>
#include <vector>
#include "../common/const.h"

uint32_t crc32(const char *data, uint32_t size);

int64_t string_to_int(const std::string &str);

void check_limits(int64_t value, int64_t lower_bound, int64_t upper_bound,
                  const std::string &value_name);

bool player_name_valid(const std::string &name);

struct addrinfo resolve_host(const std::string& addr, int type, const std::string &port);

std::vector<std::string> split(const std::string &str, const std::string &delimiter);

Coord normalized_vector(uint32_t angle);

uint32_t angle(uint32_t curr_angle, int angle_change);

std::string serialize8(uint8_t num);

std::string serialize32(uint32_t num);

std::string serialize64(uint64_t num);

uint8_t deserialize8(const std::string &str);

uint32_t deserialize32(const std::string &str);

uint64_t deserialize64(const std::string &str);

void exit_error(const std::string &msg);

#endif //SCREEN_WORMS_UTIL_FUNC_H
