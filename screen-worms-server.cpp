#include <iostream>
#include <utility>
#include <unistd.h>
#include <cstring>
#include <map>
#include <uv.h>
#include "common_func.cpp"
#include "aux.cpp"

#define MIN_TURNING_SPEED 1
#define MAX_TURNING_SPEED 100
#define MIN_ROUNDS_PER_SEC 1
#define MAX_ROUNDS_PER_SEC 240
#define MIN_WIDTH 192
#define MAX_WIDTH 1920
#define MIN_HEIGHT 144
#define MAX_HEIGHT 1080

#define PLAYERS_LIMIT 25

using namespace std;
using ClientId = pair<in_port_t, struct in6_addr>;

class LimitException : public exception {
private:
    string msg;

public:
    explicit LimitException(string str) : msg(std::move(str)) {};

    string get_msg() {
        return msg;
    }
};

struct cmp_ids {
    bool operator()(const ClientId left, const ClientId &right) const {
        char l_str[INET6_ADDRSTRLEN], r_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &left.second, l_str, sizeof(l_str));
        inet_ntop(AF_INET6, &left.second, r_str, sizeof(r_str));
        return left.first < right.first
               || (left.first == right.first && string(l_str) < string(r_str));
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

class PlayerData {
public:
    uint8_t number{};
    string name;
    double x{};
    double y{};
    uint8_t direction{};
    bool disconnected = false;
    bool playing = false;

    PlayerData() = default;

    explicit PlayerData(const string &_name) {
        name = _name;
    }
};

class GameState {
public:
    bool started = false;
    uint32_t game_id{};
    vector<Event> events;
    vector<vector<bool>> eaten_pixels; // true if pixel eaten
};

class GameManager {
private:
    /* Function checks if value is given limits (both are inclusive) and throws exception if
     * limits are violated. */
    static void check_limits(int value, int lower_bound, int upper_bound, const string &value_name) {
        if (value < lower_bound || upper_bound < value) {
            throw LimitException(value_name + " " + to_string(value) + " violates given limits "
                                 + to_string(lower_bound) + "-" + to_string(upper_bound));
        }
    }

    ServerMsg create_server_msg() {
        return ServerMsg(game_state.game_id, game_state.events);
    }

    ServerMsg new_game(const ClientMsg &msg) {

    }

    ServerMsg new_observer(const ClientMsg &msg) {
        ++observers;
        return create_server_msg();
    }

    ServerMsg new_non_observer(const ClientMsg &msg) {
        ++non_observers;
        if (msg.turn_direction != 0) {
            ++ready;
        }

        if (game_state.started) {
            return create_server_msg();
        }
        else {
            if (non_observers >= 2 && ready == non_observers) { // game ready to start
                return new_game(msg);
            }
            else {
                return create_server_msg();
            }
        }
    }

public:
    Rng rng = Rng(time(nullptr));
    uint32_t turning_speed = 6;
    uint32_t rounds_per_sec = 50;
    uint32_t width = 640;
    uint32_t height = 480;
    GameState game_state;
    map<uint64_t, PlayerData> players_data;
    uint32_t ready = 0;
    uint32_t non_observers = 0;
    uint32_t observers = 0;

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

    ServerMsg new_message(const ClientMsg &msg) {

    }

    ServerMsg new_participant(const ClientMsg &msg) {
        players_data[msg.session_id] = PlayerData(msg.player_name);
        if (msg.player_name.empty()) {
            return new_observer(msg);
        }
        else {
            return new_non_observer(msg);
        }
    }

    void player_disconnected(uint64_t session_id) {
        players_data[session_id].disconnected = true;
    }
};

class Server {
public:
    GameManager game_manager;
    int sock{};
    int port_num = 2021;
    map<ClientId, uint64_t, cmp_ids> clients;

    /* Returns true in case of success or false otherwise. */
    bool parse_args(int argc, char **argv) {
        int opt;

        while ((opt = getopt(argc, argv, "p:s:t:v:w:h:")) != -1) {
            try {
                switch (opt) {
                    case 'p':
                        this->port_num = string_to_int(optarg);
                        break;
                    case 's':
                        game_manager.rng = Rng(string_to_int(optarg));
                        break;
                    case 't':
                        game_manager.set_turning_speed(string_to_int(optarg));
                        break;
                    case 'v':
                        game_manager.set_rounds_per_sec(string_to_int(optarg));
                        break;
                    case 'w':
                        game_manager.set_width(string_to_int(optarg));
                        break;
                    case 'h':
                        game_manager.set_height(string_to_int(optarg));
                        break;
                    default: // Unknown option or '?' - input incorrect
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

    void prepare() {
        struct sockaddr_in6 local_addr{};

        this->sock = socket(AF_INET6, SOCK_DGRAM, 0);
        if (sock < 0) {
            exit_error("Socket error");
        }

        memset(&local_addr, 0, sizeof(local_addr));
        local_addr.sin6_family = AF_INET6;
        local_addr.sin6_port = htons(port_num);
        local_addr.sin6_addr = in6addr_any;

        if (bind(this->sock, (struct sockaddr *) &local_addr, sizeof(local_addr)) < 0) {
            exit_error("Bind error");
        }
    }

    [[noreturn]] void run() {
        struct sockaddr_in6 client_addr{};
        char buffer[DATAGRAM_SIZE];
        socklen_t rcv_len;
        ServerMsg answer;

        for(;;) {
            memset(buffer, 0, DATAGRAM_SIZE);
            // TODO error check
            recvfrom(sock, buffer, DATAGRAM_SIZE, 0, (struct sockaddr *) &client_addr, &rcv_len);

            if (rcv_len < MIN_CLIENT_MSG_LEN || MAX_CLIENT_MSG_LEN < rcv_len) { // TODO
                continue;
            }

            answer = manage_message(client_addr.sin6_port, client_addr.sin6_addr, buffer, rcv_len);
            if (!answer.empty()) {
                send_answer(answer, buffer, client_addr);
            }
        }

        if (close(sock) < 0) {
            exit_error("Socket close error");
        }
    }

private:
    uint64_t get_session_id(char *buffer) {
        uint64_t id;
        memcpy(&id, buffer, 8);
        return id;
    }

    ServerMsg manage_message(in_port_t client_port, struct in6_addr ip6_addr, char *buffer, size_t size) {
        auto client_id = ClientId(client_port, ip6_addr);
        auto iter = clients.find(client_id);
        uint64_t session_id = get_session_id(buffer);
        if (iter == clients.end()) {
            if (clients.size() >= PLAYERS_LIMIT) {
                return ServerMsg();
            }
            clients[client_id] = session_id;
            return game_manager.new_participant(ClientMsg(buffer, size));
        }
        else if (iter->second == session_id) {
            return game_manager.new_message(ClientMsg(buffer, size));
        }
        else if (iter->second > session_id) {
            game_manager.player_disconnected(iter->second);
            return game_manager.new_participant(ClientMsg(buffer, size));
        }
        return ServerMsg();
    }

    bool send_answer(ServerMsg &answer, char *buffer, struct sockaddr_in6 &client_addr) {
        for (auto &datagram: answer.get_datagrams()) {
            memset(buffer, 0, DATAGRAM_SIZE);
            memcpy(buffer, datagram.c_str(), datagram.length());

            // TODO error check
            sendto(sock, buffer, DATAGRAM_SIZE, 0, (struct sockaddr *) &client_addr, (socklen_t) sizeof(client_addr));
        }
        return true;
    }
};

int main(int argc, char **argv) {
    Server server;
    if (!server.parse_args(argc, argv)) {
        exit_error("Usage: " + string(argv[0]) + " -p port_num -s seed -t turning_speed "
        + "-v rounds_per_sec -w width -h height");
    }
    server.prepare();
    server.run();
    return 0;
}
