#ifndef SCREEN_WORMS_RNG_H
#define SCREEN_WORMS_RNG_H

#include <cstdint>

class Rng {
private:
    long value;

public:
    explicit Rng(int seed) : value(seed) {}

    uint32_t get_random() {
        uint64_t ret = value;
        value = (value * 279410273) % 4294967291;
        return ret;
    }
};

#endif //SCREEN_WORMS_RNG_H
