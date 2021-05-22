#include <iostream>
#include <utility>
#include <unistd.h>
#include <cstring>
#include <map>
#include <uv.h>
#include "game_manager.cpp"

#define PLAYERS_LIMIT 25
#define TIMEOUT_MILLIS 2000

using namespace std;
using ClientSock = pair<in_port_t, struct in6_addr>;
using ClientId = pair<uint64_t, string>;

struct cmp_ids {
    bool operator()(const ClientSock left, const ClientSock &right) const {
        char l_str[INET6_ADDRSTRLEN], r_str[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, &left.second, l_str, sizeof(l_str));
        inet_ntop(AF_INET6, &left.second, r_str, sizeof(r_str));
        return left.first < right.first
               || (left.first == right.first && string(l_str) < string(r_str));
    }
};

class Server {
public:
    GameManager game_manager;
    int sock{};
    int port_num = 2021;
    map<ClientSock, ClientId, cmp_ids> clients;
    map<ClientSock, Timer, cmp_ids> timers;
    Timer timer;

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

        struct timeval read_timeout{};
        read_timeout.tv_sec = 0;
        read_timeout.tv_usec = 10;
        if (setsockopt(this->sock, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout) < 0) {
            exit_error("Set not blocking error");
        }
    }

    [[noreturn]] void run() {
        struct sockaddr_in6 client_addr{};
        char buffer[DATAGRAM_SIZE];
        socklen_t rcv_len;
        ServerMsg answer;

        timer.start();
        for(;;) {
            memset(buffer, 0, DATAGRAM_SIZE);
            // TODO error check
            recvfrom(sock, buffer, DATAGRAM_SIZE, 0, (struct sockaddr *) &client_addr, &rcv_len);

            if (rcv_len < MIN_CLIENT_MSG_LEN || MAX_CLIENT_MSG_LEN < rcv_len) { // TODO
                continue;
            }

            answer = manage_message(client_addr.sin6_port, client_addr.sin6_addr, buffer, rcv_len);
            manage_answer(answer, buffer, client_addr);

            check_timeouts();
            answer = game_manager.cyclic_activities(timer);
            manage_answer(answer, buffer, client_addr);
        }
    }

private:
    static uint64_t get_session_id(char *buffer) {
        uint64_t id;
        memcpy(&id, buffer, 8);
        return id;
    }

    ServerMsg manage_message(in_port_t client_port, struct in6_addr ip6_addr, char *buffer, size_t size) {
        auto client_sock = ClientSock(client_port, ip6_addr);
        auto iter = clients.find(client_sock);
        uint64_t session_id = get_session_id(buffer);

        if (iter == clients.end()) {
            if (clients.size() >= PLAYERS_LIMIT) {
                return ServerMsg();
            }
            ClientToServerMsg msg(buffer, size);
            clients[client_sock] = ClientId(session_id, msg.player_name);
            timers[client_sock] = Timer();
            timers[client_sock].start();
            return game_manager.new_participant(msg, msg.player_name);
        }
        else if (iter->second.first == session_id) {
            timers[client_sock].start();
            return game_manager.new_message(ClientToServerMsg(buffer, size), iter->second.second);
        }
        else if (iter->second.first > session_id) {
            ClientToServerMsg msg(buffer, size);
            clients[client_sock] = ClientId(iter->second.first, msg.player_name);
            timers[client_sock] = Timer();
            timers[client_sock].start();
            game_manager.player_disconnected(iter->second.second);
            return game_manager.new_participant(ClientToServerMsg(buffer, size), msg.player_name);
        }
        else {
            timers[client_sock].start();
            return ServerMsg();
        }
    }

    void check_timeouts() {
        for (auto iter: timers) {
            auto &client_sock = iter.first;
            auto &client_timer = iter.second;
            if (client_timer.timeout(TIMEOUT_MILLIS)) {
                game_manager.player_disconnected(clients[client_sock].second);
                clients.erase(client_sock);
                timers.erase(client_sock);
            }
        }
    }

    bool manage_answer(ServerMsg &answer, char *buffer, struct sockaddr_in6 &client_addr) {
        if (!answer.empty()) {
            if (answer.to_all) {
                send_answer_to_all(answer, buffer);
            }
            else {
                send_answer(answer, buffer, client_addr);
            }
        }
    }

    bool send_answer_to_all(ServerMsg &answer, char *buffer) {
        for (auto &iter: clients) {
            struct sockaddr_in6 client_addr{};
            client_addr.sin6_port = iter.first.first;
            client_addr.sin6_addr = iter.first.second;
            send_answer(answer, buffer, client_addr);
        }
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