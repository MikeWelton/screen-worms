#ifndef SCREEN_WORMS_MESSAGES_H
#define SCREEN_WORMS_MESSAGES_H

#include <string>
#include <cstring>
#include <utility>
#include <iostream>
#include <uv.h>
#include "../utils/util_func.h"
#include "events.h"
#include "const.h"

using namespace std;

/* Message send from client to server. */
class ClientToServerMsg {
public:
    uint64_t session_id; // 8 bajtów, liczba bez znaku
    uint8_t turn_direction; // 1 bajt, liczba bez znaku, wartość 0 → prosto, wartość 1 → w prawo, wartość 2 → w lewo
    uint32_t next_expected_event_no; // 4 bajty, liczba bez znaku
    string player_name; // 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone

    ClientToServerMsg(uint64_t _session_id, uint8_t _turn_direction,
                      uint32_t _next_expected_event_no, string _player_name) :
                  session_id(_session_id),
                  turn_direction(_turn_direction),
                  next_expected_event_no(_next_expected_event_no),
                  player_name(std::move(_player_name)) {}

    ClientToServerMsg(const char *buffer, size_t size) {
        string msg(buffer, size);
        session_id = deserialize64(msg.substr(0, sizeof(uint64_t)));
        turn_direction = deserialize8(msg.substr(sizeof(uint64_t), 1));
        next_expected_event_no = deserialize32(
                msg.substr(sizeof(uint64_t) + 1, sizeof(uint32_t)));

        size_t num_size = sizeof(uint32_t) + sizeof(uint8_t) + sizeof(uint64_t);
        player_name = msg.substr(num_size, size - num_size);
        //cerr << "ClientToServerMsg: " << session_id << " " << turn_direction << " " << next_expected_event_no << " " << player_name << endl;
    }

    string serialize() {
        return serialize64(session_id) + serialize8(turn_direction) +
            serialize32(next_expected_event_no) + player_name;
    }
};

/* Message send from server to client. */
class ServerMsg {
public:
    uint32_t game_id{}; // 4 bajty, liczba bez znaku
    vector<Event> events; // zmienna liczba rekordów, zgodnych z poniższą specyfikacją
    bool to_all = false;

    explicit ServerMsg() = default;

    ServerMsg(const char *buffer, size_t size) {
        size_t len;
        string msg(buffer, size);
        Event event;

        if (msg.size() > sizeof(uint32_t)) {
            game_id = deserialize32(msg.substr(0, 4));
            msg = msg.substr(4, msg.length() - 4);
            while (!msg.empty()) {
                len = deserialize32(msg.substr(0, 4)) + 2 * sizeof(uint32_t);
                try {
                     event = Event(msg.substr(0, len));
                }
                catch (IncorrectCrc32Exception &e) {
                    return;
                }
                catch (UnknownEventTypeException &e) {
                    msg = msg.substr(len, msg.length() - len);
                    continue;
                }
                events.push_back(event);
                msg = msg.substr(len, msg.length() - len);
            }
        }
    }

    ServerMsg(uint32_t _game_id, const vector<Event> &_events, bool to_all = false) :
            game_id(_game_id),
            events(_events),
            to_all(to_all) {}

    bool empty() {
        return events.empty();
    }

    vector<string> get_datagrams() {
        if (empty()) {
            return vector<string>();
        }

        vector<string> answers;
        string game_id_serialized = serialize32(game_id);
        answers.push_back(game_id_serialized);
        for (auto &event: events) {
            string event_str = event.serialize();
            if (event_str.length() > DATAGRAM_SIZE - answers.back().length()) {
                answers.push_back(game_id_serialized);
            }
            answers.back().append(event_str);
        }

        return answers;
    }
};

#endif //SCREEN_WORMS_MESSAGES_H
