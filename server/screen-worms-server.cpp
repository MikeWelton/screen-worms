#include <iostream>
#include <utility>
#include <unistd.h>
#include <cstring>
#include <map>
#include <uv.h>
#include "game_manager.cpp"
#include "../utils/rng.h"

#define PLAYERS_LIMIT 25

using namespace std;
using ClientId = pair<in_port_t, struct in6_addr>;

struct cmp_ids {
    bool operator()(const ClientId left, const ClientId &right) const {
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
    map<ClientId, uint64_t, cmp_ids> clients;
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
            return game_manager.new_participant(ClientToServerMsg(buffer, size));
        }
        else if (iter->second == session_id) {
            return game_manager.new_message(ClientToServerMsg(buffer, size));
        }
        else if (iter->second > session_id) {
            game_manager.player_disconnected(iter->second);
            return game_manager.new_participant(ClientToServerMsg(buffer, size));
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
