#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <regex>
#include <iostream>
#include <netinet/tcp.h>
#include "../utils/util_func.h"
#include "../utils/timer.h"
#include "../common/messages.h"

#define MILLIS 3000

static const map<string, uint8_t> KEY_TO_DIR = {
        {"LEFT_KEY_DOWN", 2},
        {"LEFT_KEY_UP", 0},
        {"RIGHT_KEY_DOWN", 1},
        {"RIGHT_KEY_UP", 0}
};


class Connection {
private:
    static int create_connection(struct addrinfo host, int type, int protocol) {
        int sock = socket(host.ai_family, type, protocol);
        if (sock < 0) {
            exit_error("Socket error");
        }

        int set = 1;
        if (type == SOCK_STREAM) {
            if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &set, sizeof(int)) < 0) {
                exit_error("No delay option");
            }
        }

        if (connect(sock, host.ai_addr, host.ai_addrlen) < 0) {
            exit_error("Connect");
        }
        return sock;
    }

public:
    string game_server;
    string game_server_port = "2021";
    string gui_server = "localhost";
    string gui_server_port = "20210";

    int get_game_server_sock() {
        return create_connection(resolve_host(game_server, SOCK_DGRAM, game_server_port),
                                 SOCK_DGRAM, IPPROTO_UDP);
    }

    int get_gui_server_sock() {
        return create_connection(resolve_host(gui_server, SOCK_STREAM, gui_server_port),
                                 SOCK_STREAM, IPPROTO_TCP);
    }
};

class Client {
private:
    static uint8_t get_direction(const string &action) {
        return KEY_TO_DIR.find(action)->second;
    }

    string create_msg_to_server() {
        return ClientToServerMsg(session_id, direction,
                                 next_expected_event_no, player_name).to_string();
    }

    vector<string> create_msgs_to_gui(char *buffer, size_t len) {
        ServerMsg msg(buffer, len);
        vector<string> ret;
        for (auto &event: msg.events) {
            ret.push_back(event.event_data->to_gui_msg(player_name));
        }
        return ret;
    }

public:
    uint32_t session_id = Timer::get_session_id();
    uint8_t direction{};
    uint32_t next_expected_event_no = 0;
    string player_name;
    int game_server_sock{};
    int gui_server_sock{};
    Connection conn;
    Timer timer;

    /* Returns true in case of success or false otherwise. */
    bool parse_args(int argc, char **argv) {
        int opt;
        string arg;

        while ((opt = getopt(argc, argv, "n:p:i:r:")) != -1) {
            try {
                switch (opt) {
                    case 'n':
                        arg = optarg;
                        if (!player_name_valid(arg)) {
                            return false;
                        }
                        player_name = optarg;
                        break;
                    case 'p':
                        conn.game_server_port = optarg;
                        break;
                    case 'i':
                        conn.gui_server = optarg;
                        break;
                    case 'r':
                        conn.gui_server_port = optarg;
                        break;
                    default: // Unknown option, input incorrect
                        return false;
                }
            }
            catch (exception &e) {
                return false;
            }
        }
        int index = optind;
        if (index >= argc || ++index < argc) { // 0 or more than 1 non-option argument
            return false;
        }
        conn.game_server = argv[optind];
        return true;
    }

    void prepare() {
        game_server_sock = conn.get_game_server_sock();
        gui_server_sock = conn.get_gui_server_sock();
    }

    [[noreturn]] void run() {
        char buffer[DATAGRAM_SIZE];
        socklen_t rcv_len;
        ServerMsg answer;

        timer.start();
        for (;;) {
            memset(buffer, 0, DATAGRAM_SIZE);
            if ((rcv_len = read(gui_server_sock, buffer, DATAGRAM_SIZE)) > 0) {
                direction = get_direction(string(buffer, rcv_len));
            }

            if (timer.timeout(MILLIS)) {
                string msg = create_msg_to_server();
                memcpy(buffer, msg.c_str(), msg.length());
                send(game_server_sock, buffer, msg.length(), 0);
                timer.start();
            }

            while ((rcv_len = recv(game_server_sock, buffer, DATAGRAM_SIZE, 0)) > 0) {
                vector<string> msgs_to_gui = create_msgs_to_gui(buffer, rcv_len);

                for (auto &msg: msgs_to_gui) {
                    memcpy(buffer, msg.c_str(), msg.length());
                    write(gui_server_sock, buffer, msg.length()); // TODO error check, broken pipe etc.
                }
            }
        }
    }
};

int main(int argc, char **argv) {
    Client client;
    if (!client.parse_args(argc, argv)) {
        exit_error("Usage: " + string(argv[0]) + " player_name -n player_name "
                   + "-p server_port -i gui_server_address -r gui_server_port");
    }
    client.prepare();
    client.run();
    return 0;
}
