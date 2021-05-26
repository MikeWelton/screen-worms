#ifndef SCREEN_WORMS_TIMER_H
#define SCREEN_WORMS_TIMER_H

#include <chrono>

using namespace std;

class Timer {
private:
    chrono::time_point<chrono::system_clock> start_time;

public:
    Timer() = default;

    static uint32_t get_session_id() {
        return chrono::duration_cast<chrono::microseconds>(
                chrono::system_clock::now().time_since_epoch()).count();
    }

    void start() {
        start_time = chrono::system_clock::now();
    }

    bool timeout(uint32_t millis) {
        auto end = chrono::system_clock::now();
        auto diff = chrono::duration_cast<chrono::milliseconds>(
                end - this->start_time).count();
        return diff >= millis;
    }
};

#endif //SCREEN_WORMS_TIMER_H
