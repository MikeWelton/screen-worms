#ifndef SCREEN_WORMS_ID_MANAGER_H
#define SCREEN_WORMS_ID_MANAGER_H

#include <cstdint>

class IdManager {
private:
    uint32_t id = 0;

public:
    IdManager() = default;

    uint32_t get_next_id() {
        return id++;
    }
};

#endif //SCREEN_WORMS_ID_MANAGER_H
