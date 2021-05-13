#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <regex>
#include "aux.cpp"

bool player_name_valid(string &name) {
    const int MIN_LEN = 1,
            MAX_LEN = 20,
            ASCII_LOWER_BOUND = 33,
            ASCII_UPPER_BOUND = 126;

    bool correct_len = (MIN_LEN <= name.length() && name.length() <= MAX_LEN);

    if (!correct_len) {
        return false;
    }

    return all_of(name.begin(), name.end(), [](char c) {
        return (ASCII_LOWER_BOUND <= c && c <= ASCII_UPPER_BOUND);
    });
}

class Client {
public:
    string player_name;
    string game_server;
    int game_server_port = 2021;
    string gui_server = "localhost"; // TODO
    int gui_server_port = 20210;
};

/* Returns true in case of success or false otherwise. */
bool parse_args(int argc, char **argv, Client &client) {
    int opt, value;
    string arg;

    while ((opt = getopt(argc, argv, "npir:"))) {
        try {
            switch (opt) {
                case 'n':
                    arg = optarg;
                    if (!player_name_valid(arg)) {
                        return false;
                    }
                    client.player_name = optarg; //TODO
                    break;
                case 'p':
                    client.game_server_port = string_to_int(optarg);
                    break;
                case 'i':
                    client.gui_server = optarg; // TODO
                    break;
                case 'r':
                    client.gui_server_port = string_to_int(optarg);
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
    if (index >= 0 || ++index < argc) { // 0 or mora than 1 non-option argument
        return false;
    }
    client.game_server = argv[optind]; // TODO
    return true;
}

int main(int argc, char **argv) {
    Client client;
    parse_args(argc, argv, client);
    return 0;
}
