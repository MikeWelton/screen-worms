#include <iostream>
#include <utility>
#include <unistd.h>
#include "aux.cpp"

using namespace std;
#define MIN_TURNING_SPEED 1
#define MAX_TURNING_SPEED 100
#define MIN_ROUNDS_PER_SEC 1
#define MAX_ROUNDS_PER_SEC 500
#define MIN_WIDTH 192
#define MAX_WIDTH 1920
#define MIN_HEIGHT 144
#define MAX_HEIGHT 1080

class LimitException : public exception {
private:
    string msg;

public:
    explicit LimitException(string str) : msg(std::move(str)) {};

    string get_msg() {
        return msg;
    }
};

class Rng {
public:
    long value;

    explicit Rng(int seed) : value(seed) {}

    long get_random() {
        long ret = value;
        value = (value * 279410273) % 4294967291;
        return ret;
    }
};

class GameState {

};

class Server {
private:
    /* Function checks if value is given limits (both are inclusive) and throws exception if
     * limits are violated. */
    static void check_limits(int value, int lower_bound, int upper_bound, const string &value_name) {
        if (value < lower_bound || upper_bound < value) {
            throw LimitException(value_name + " " + to_string(value) + " violates given limits "
            + to_string(lower_bound) + "-" + to_string(upper_bound));
        }
    }

public:
    int port_num = 2021;
    Rng rng = Rng(time(nullptr));
    int turning_speed = 6;
    int rounds_per_sec = 50;
    int width = 640;
    int height = 480;

    void set_turning_speed(int _turning_speed) {
        check_limits(_turning_speed, MIN_TURNING_SPEED, MAX_TURNING_SPEED, "Turning speed");
        turning_speed = _turning_speed;
    }

    void set_rounds_per_sec(int _rounds_per_sec) {
        check_limits(_rounds_per_sec, MIN_ROUNDS_PER_SEC, MAX_ROUNDS_PER_SEC, "Rounds per sec");
        rounds_per_sec = _rounds_per_sec;
    }

    void set_width(int _width) {
        check_limits(_width, MIN_WIDTH, MAX_WIDTH, "Width");
        width = _width;
    }

    void set_height(int _height) {
        check_limits(_height, MIN_HEIGHT, MAX_HEIGHT, "Height");
        height = _height;
    }
};

/* Returns true in case of success or false otherwise. */
bool parse_args(int argc, char **argv, Server &server) {
    int opt;

    while ((opt = getopt(argc, argv, "pstvwh:"))) {
        try {
            switch (opt) {
                case 'p':
                    server.port_num = string_to_int(optarg);
                    break;
                case 's':
                    server.rng = Rng(string_to_int(optarg));
                    break;
                case 't':
                    server.set_turning_speed(string_to_int(optarg));
                    break;
                case 'v':
                    server.set_rounds_per_sec(string_to_int(optarg));
                    break;
                case 'w':
                    server.set_width(string_to_int(optarg));
                    break;
                case 'h':
                    server.set_height(string_to_int(optarg));
                    break;
                default: // Unknown option, input incorrect
                    return false;
            }
        }
        catch (LimitException &e) { // catch if value violates limits
            exit_error(e.get_msg());
        }
        catch (exception &e) { // catch conversion exception
            return false;
        }
    }
    return (optind >= argc); // we do not accept non option arguments
}

int main(int argc, char **argv) {
    Server server;
    if (!parse_args(argc, argv, server)) {
        exit_error("Usage: " + string(argv[0]) + " -p port_num -s seed -t turning_speed "
        + "-v rounds_per_sec -w width -h height");
    }
    return 0;
}
