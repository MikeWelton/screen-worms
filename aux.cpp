#include <cstdint>
#include <string>
#include <vector>
#include <iostream>
#include <netdb.h>
#include <cstring>
#include <algorithm>
#include <memory>
#include "const.h"

using namespace std;

enum EventType {
    NEW_GAME = 0,
    PIXEL = 1,
    PLAYER_ELIMINATED = 2,
    GAME_OVER = 3,
};

class EventData {
public:
    virtual size_t size() = 0;

    virtual string to_string() = 0;
};

class NewGameData : public EventData {
public:
    uint32_t maxx; // 4 bajty, szerokość planszy w pikselach, liczba bez znaku
    uint32_t maxy; // 4 bajty, wysokość planszy w pikselach, liczba bez znaku
    // następnie lista nazw graczy zawierająca dla każdego z graczy player_name, jak w punkcie „2.1. Komunikaty od klienta do serwera”, oraz znak '\0'
    vector<string> player_names; // 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone

    NewGameData(uint32_t maxx, uint32_t maxy, const vector<string> &player_names) :
            maxx(maxx),
            maxy(maxy),
            player_names(player_names) {}

    size_t size() override {
        size_t vec_size = 0;
        for (auto &str : player_names) {
            vec_size += str.size();
        }
        return 2 * sizeof(uint32_t) + vec_size;
    }

    string to_string() override {
        string ret = std::to_string(maxx) + std::to_string(maxy);
        for (auto &str: player_names) {
            ret.append(str);
        }
        ret += '\0';
        return ret;
    }
};

class PixelData : public EventData {
public:
    uint8_t player_number; // 1 bajt
    uint32_t x; // 4 bajty, odcięta, liczba bez znaku
    uint32_t y; // 4 bajty, rzędna, liczba bez znaku

    PixelData(uint8_t player_number, uint32_t x, uint32_t y) : player_number(player_number),
                                                               x(x), y(y) {}

    size_t size() override {
        return sizeof(uint8_t) + 2 * sizeof(uint32_t);
    }

    string to_string() override {
        return std::to_string(player_number) + std::to_string(x) + std::to_string(y);
    }
};

class PlayerEliminatedData : public EventData {
public:
    uint8_t player_number; // 1 bajt;

    explicit PlayerEliminatedData(uint8_t player_number) : player_number(player_number) {}

    size_t size() override {
        return sizeof(uint8_t);
    }

    string to_string() override {
        return std::to_string(player_number);
    }
};

class GameOverData : public EventData {
    size_t size() override {
        return 0;
    }

    string to_string() override {
        return string();
    }
};

class Event {
public:
    uint32_t len; // 4 bajty, liczba bez znaku, sumaryczna długość pól event_*
    uint32_t event_no; // 4 bajty, liczba bez znaku, dla każdej partii kolejne wartości, począwszy od zera
    EventType event_type; // 1 bajt
    shared_ptr<EventData> event_data; // zależy od typu, patrz opis poniżej
    uint32_t crc32; // 4 bajty, liczba bez znaku, suma kontrolna obejmująca pola od pola len do event_data włącznie, obliczona standardowym algorytmem CRC-32-IEEE

    Event(uint32_t _event_no, EventType _event_type, const shared_ptr<EventData> &_event_data,
          uint32_t _crc32) :
            event_no(_event_no),
            event_type(_event_type),
            event_data(_event_data),
            crc32(_crc32) {
        len = sizeof(uint32_t) + 1 + event_data->size();
    }

    string to_string() {
        string ret = std::to_string(len) + std::to_string(event_no);
        ret.append((char *) &event_type, 1);
        ret.append(event_data->to_string());
        ret.append(std::to_string(crc32));
        return ret;
    }
};

/* Message send from client. */
class ClientMsg {
public:
    uint64_t session_id; // 8 bajtów, liczba bez znaku
    uint8_t turn_direction; // 1 bajt, liczba bez znaku, wartość 0 → prosto, wartość 1 → w prawo, wartość 2 → w lewo
    uint32_t next_expected_event_no; // 4 bajty, liczba bez znaku
    string player_name; // 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone

    ClientMsg(char *msg, size_t size) {
        memcpy(&session_id, msg, sizeof(session_id));
        turn_direction = msg[sizeof(session_id)];
        memcpy(&next_expected_event_no, &msg[sizeof(session_id) + 1],
               sizeof(next_expected_event_no));
        char *name = nullptr;
        memcpy(name, &msg[MIN_CLIENT_MSG_LEN], size - MIN_CLIENT_MSG_LEN);
        player_name = name;
    }
};

/* Message send from server. */
class ServerMsg {
public:
    uint32_t game_id{}; // 4 bajty, liczba bez znaku
    vector<Event> events; // zmienna liczba rekordów, zgodnych z poniższą specyfikacją
    bool to_all = false;

    explicit ServerMsg() = default;

    ServerMsg(uint32_t _game_id, const vector<Event> &_events) :
            game_id(_game_id),
            events(_events) {}

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
