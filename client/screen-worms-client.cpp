#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <regex>
#include <iostream>
#include <netinet/tcp.h>
#include <poll.h>
#include "../utils/util_func.h"
#include "../utils/timer.h"
#include "../common/messages.h"

#define MILLIS 30
#define UNKNOWN_GUI_COMMAND 3

static const map<string, uint8_t> KEY_TO_DIR = {
        {"LEFT_KEY_DOWN", 2},
        {"LEFT_KEY_UP", 0},
        {"RIGHT_KEY_DOWN", 1},
        {"RIGHT_KEY_UP", 0}
};


class Connection {
private:
    static pollfd create_connection(struct addrinfo host, int type, int protocol) {
        pollfd pol{};

        pol.events = POLLIN;
        pol.fd = socket(host.ai_family, type, protocol);
        if (pol.fd < 0) {
            exit_error("Socket error");
        }

        int set = 1;
        if (type == SOCK_STREAM) {
            if (setsockopt(pol.fd, IPPROTO_TCP, TCP_NODELAY, &set, sizeof(int)) < 0) {
                exit_error("No delay option");
            }
        }

        if (connect(pol.fd, host.ai_addr, host.ai_addrlen) < 0) {
            exit_error("Connect");
        }

        return pol;
    }

public:
    string game_server;
    string game_server_port = "2021";
    string gui_server = "localhost";
    string gui_server_port = "20210";

    pollfd get_game_server_poll() {
        return create_connection(resolve_host(game_server, SOCK_DGRAM, game_server_port),
                                 SOCK_DGRAM, IPPROTO_UDP);
    }

    pollfd get_gui_server_poll() {
        return create_connection(resolve_host(gui_server, SOCK_STREAM, gui_server_port),
                                 SOCK_STREAM, IPPROTO_TCP);
    }
};

class Client {
private:
    static uint8_t get_direction(const string &action) {
        auto iter = KEY_TO_DIR.find(action.substr(0, action.length() - 1));
        if (iter == KEY_TO_DIR.end()) {
            return UNKNOWN_GUI_COMMAND;
        }
        return iter->second;
    }

    string create_msg_to_server() {
        return ClientToServerMsg(session_id, direction,
                                 next_expected_event_no, player_name).serialize();
    }

    string create_msgs_to_gui(char *buffer, size_t len) {
        ServerMsg msg(buffer, len);
        string ret;
        for (auto &event: msg.events) {
            if (event.event_type == GAME_OVER) {
                next_expected_event_no = 0;
            }
            else {
                next_expected_event_no = event.event_no + 1;
                ret.append(event.event_data->to_gui_msg(names) + "\n");
            }
        }
        return ret;
    }

    int read_from_socket(char *buffer, int sock, const string &err_msg) {
        int rcv_len;
        memset(buffer, 0, DATAGRAM_SIZE);
        if ((rcv_len = read(sock, buffer, DATAGRAM_SIZE)) <= 0) {
            exit_error(err_msg);
        }
        return rcv_len;
    }

    void write_to_socket(const string &msg, int sock, const string &err_msg) {
        char buffer[DATAGRAM_SIZE];
        memcpy(buffer, msg.c_str(), msg.length());
        if (write(sock, buffer, msg.length()) <= 0) {
            exit_error(err_msg);
        }
    }

public:
    uint32_t session_id = Timer::get_session_id();
    uint8_t direction{};
    uint32_t next_expected_event_no = 0;
    vector<string> names;
    string player_name;
    pollfd game_server{};
    pollfd gui_server{};
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
        game_server = conn.get_game_server_poll();
        gui_server = conn.get_gui_server_poll();
    }

    [[noreturn]] void run() {
        int ret;
        char buffer[DATAGRAM_SIZE];
        socklen_t rcv_len;
        ServerMsg answer;

        timer.start();
        for (;;) {
            gui_server.revents = 0;
            game_server.revents = 0;

            ret = poll(&gui_server, 1, 0);
            if (ret < 0) {
                exit_error("Gui poll error");
            }
            else if (ret > 0 && (gui_server.revents & (POLLIN | POLLERR))) {
                rcv_len = read_from_socket(buffer, gui_server.fd, "Error read from gui");
                cerr << "From gui: " << buffer << endl;
                uint8_t dir = get_direction(string(buffer, rcv_len));
                direction = dir == UNKNOWN_GUI_COMMAND ? direction : dir;
            }

            if (timer.timeout(MILLIS)) {
                string msg = create_msg_to_server();
                //cerr << "To server: " << msg << endl;
                write_to_socket(msg, game_server.fd, "Send to game server error");
                timer.start();
            }

            ret = poll(&game_server, 1, 0);
            if (ret < 0) {
                exit_error("Server poll error");
            }
            else if (ret > 0 && (game_server.revents & (POLLIN | POLLERR))) {
                rcv_len = read_from_socket(buffer, game_server.fd, "Error read from game server");
                cerr << "From server: " << buffer << endl;
                string msgs_to_gui = create_msgs_to_gui(buffer, rcv_len);

                cerr << "To gui: " << msgs_to_gui << endl;
                write_to_socket(msgs_to_gui, gui_server.fd, "Error write to gui");
            }
        }
    }
};

int main(int argc, char **argv) {
    Client client;
    if (!client.parse_args(argc, argv)) {
        exit_error("Usage: " + string(argv[0]) + " game_server -n player_name "
                   + "-p server_port -i gui_server_address -r gui_server_port");
    }
    client.prepare();
    client.run();
    return 0;
}
