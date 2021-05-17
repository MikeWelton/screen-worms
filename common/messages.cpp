#include <string>
#include <cstring>
#include <utility>
#include "events.cpp"
#include "const.h"
#include "../common/exceptions.h"

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

    ClientToServerMsg(char *msg, size_t size) {
        memcpy(&session_id, msg, sizeof(session_id));
        turn_direction = msg[sizeof(session_id)];
        memcpy(&next_expected_event_no, &msg[sizeof(session_id) + 1],
               sizeof(next_expected_event_no));
        char *name = nullptr;
        memcpy(name, &msg[MIN_CLIENT_MSG_LEN], size - MIN_CLIENT_MSG_LEN);
        player_name = name;
    }

    string to_string() {
        return ::to_string(session_id) + ::to_string(turn_direction)
            + ::to_string(next_expected_event_no) + player_name;
    }
};

/* Message send from server to client. */
class ServerMsg {
public:
    uint32_t game_id{}; // 4 bajty, liczba bez znaku
    vector<Event> events; // zmienna liczba rekordów, zgodnych z poniższą specyfikacją
    bool to_all = false;

    explicit ServerMsg() = default;

    ServerMsg(char *buffer, size_t size) {
        size_t len;
        string msg(buffer, size);
        Event event;

        if (msg.size() > sizeof(uint32_t)) {
            game_id = string_to_int(msg.substr(0, 4));
            msg = msg.substr(4, msg.length() - 4);
            while (!msg.empty()) {
                len = string_to_int(msg.substr(0, 4)) + 8;
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

    ServerMsg(uint32_t _game_id, const vector<Event> &_events) :
            game_id(_game_id),
            events(_events) {}

    ServerMsg(uint32_t _game_id, const vector<Event> &_events, bool to_all) :
            game_id(_game_id),
            events(_events),
            to_all(true) {}

    bool empty() {
        return events.empty();
    }

    vector<string> get_datagrams() {
        if (empty()) {
            return vector<string>();
        }

        vector<string> answers(1);
        for (auto &event: events) {
            string event_str = event.to_string();
            if (event_str.length() < DATAGRAM_SIZE - answers.back().length()) {
                answers.back().append(event_str);
            }
            else {
                answers.push_back(event_str);
            }
        }

        return answers;
    }
};
