#ifndef SCREEN_WORMS_EVENTS_H
#define SCREEN_WORMS_EVENTS_H

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <memory>
#include <cassert>
#include "../utils/util_func.h"
#include "../common/exceptions.h"

using namespace std;

enum EventType {
    NEW_GAME = 0,
    PIXEL = 1,
    PLAYER_ELIMINATED = 2,
    GAME_OVER = 3,
};

class EventData {
public:
    virtual string get_name() = 0;

    virtual size_t size() = 0;

    virtual string serialize() = 0;

    virtual string to_gui_msg(vector<string> &pl_names) = 0;
};

class NewGameData : public EventData {
public:
    string name = "NEW_GAME";
    uint32_t maxx; // 4 bajty, szerokość planszy w pikselach, liczba bez znaku
    uint32_t maxy; // 4 bajty, wysokość planszy w pikselach, liczba bez znaku
    // następnie lista nazw graczy zawierająca dla każdego z graczy player_name, jak w punkcie „2.1. Komunikaty od klienta do serwera”, oraz znak '\0'
    vector<string> player_names; // 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone

    NewGameData(const string &str) {
        maxx = deserialize32(str.substr(0, 4));
        maxy = deserialize32(str.substr(4, 4));
        string names = str.substr(8, str.length() - 8);
        player_names = split(names, string("\0", 1));
    }

    NewGameData(uint32_t maxx, uint32_t maxy, const vector<string> &player_names) :
            maxx(maxx),
            maxy(maxy),
            player_names(player_names) {}

    string get_name() override {
        return name;
    }

    size_t size() override {
        size_t vec_size = 0;
        for (auto &str : player_names) {
            vec_size += str.size() + sizeof('\0');
        }
        return 2 * sizeof(uint32_t) + vec_size;
    }

    string serialize() override {
        string ret = serialize32(maxx) + serialize32(maxy);
        for (auto &str: player_names) {
            ret.append(str + string("\0", 1));
        }
        return ret;
    }

    string to_gui_msg(vector<string> &pl_names) override {
        pl_names = player_names;
        string ret = name + " " + std::to_string(maxx) + " " + std::to_string(maxy);
        for (auto &str: player_names) {
            if (!player_name_valid(str) || str.empty()) {
                exit_error("Received incorrect player name");
            }
            ret.append(" " + str);
        }
        return ret;
    }
};

class PixelData : public EventData {
public:
    string name = "PIXEL";
    uint8_t player_number; // 1 bajt
    uint32_t x; // 4 bajty, odcięta, liczba bez znaku
    uint32_t y; // 4 bajty, rzędna, liczba bez znaku

    PixelData(const string &str) {
        player_number = deserialize8(str.substr(0, 1));
        x = deserialize32(str.substr(1, 4));
        y = deserialize32(str.substr(5, 4));
    }

    PixelData(uint8_t player_number, uint32_t x, uint32_t y) : player_number(player_number),
                                                               x(x), y(y) {}

    string get_name() override {
        return name;
    }

    size_t size() override {
        return sizeof(uint8_t) + 2 * sizeof(uint32_t);
    }

    string serialize() override {
        return serialize8(player_number) + serialize32(x) + serialize32(y);
    }

    string to_gui_msg(vector<string> &pl_names) override {
        if (player_number >= pl_names.size()) {
            exit_error("Received bad player number");
        }
        return name + " " + std::to_string(x) + " " + std::to_string(y) + " " + pl_names[player_number];
    }
};

class PlayerEliminatedData : public EventData {
public:
    string name = "PLAYER_ELIMINATED";
    uint8_t player_number; // 1 bajt;

    PlayerEliminatedData(const string &str) {
        player_number = deserialize8(str.substr(0, 1));
    }

    explicit PlayerEliminatedData(uint8_t player_number) : player_number(player_number) {}

    string get_name() override {
        return name;
    }

    size_t size() override {
        return sizeof(uint8_t);
    }

    string serialize() override {
        return serialize8(player_number);
    }

    string to_gui_msg(vector<string> &pl_names) override {
        if (player_number >= pl_names.size()) {
            exit_error("Received bad player number");
        }
        return name + " " + pl_names[player_number];
    }
};

class GameOverData : public EventData {
public:
    string name = "GAME_OVER";

    GameOverData() = default;

    string get_name() override {
        return name;
    }

    size_t size() override {
        return 0;
    }

    string serialize() override {
        return string();
    }

    string to_gui_msg(vector<string> &names) override {
        return string();
    }
};

class Event {
private:
    string body_serialize() {
        assert((uint8_t) event_type == event_type); // TODO
        return serialize32(len) + serialize32(event_no) +
            serialize8(event_type) + event_data->serialize();
    }

    static bool correct_event_type(uint8_t type) {
        for (uint8_t t = NEW_GAME; t <= GAME_OVER; ++t) {
            if (type == t) {
                return true;
            }
        }
        return false;
    }

public:
    uint32_t len; // 4 bajty, liczba bez znaku, sumaryczna długość pól event_*
    uint32_t event_no{}; // 4 bajty, liczba bez znaku, dla każdej partii kolejne wartości, począwszy od zera
    EventType event_type; // 1 bajt
    shared_ptr<EventData> event_data; // zależy od typu, patrz opis poniżej
    uint32_t crc32; // 4 bajty, liczba bez znaku, suma kontrolna obejmująca pola od pola len do event_data włącznie, obliczona standardowym algorytmem CRC-32-IEEE

    Event() = default;

    explicit Event(const string &msg) {
        len = deserialize32(msg.substr(0, 4));
        event_no = deserialize32(msg.substr(4, 4));
        event_type = (EventType) deserialize8(msg.substr(8, 1));

        if (!correct_event_type(event_type)) {
            throw UnknownEventTypeException();
        }

        string body = msg.substr(0, msg.length() - 4);
        crc32 = deserialize32(msg.substr(msg.length() - 4, 4));
        if (crc32 != ::crc32(body.c_str(), body.length())) {
            throw IncorrectCrc32Exception();
        }

        size_t num_size = 2 * sizeof(uint32_t) + sizeof(uint8_t);
        string data_str = body.substr(num_size, body.length() - num_size);
        if (event_type == NEW_GAME) {
            event_data = make_shared<NewGameData>(NewGameData(data_str));
        }
        else if (event_type == PIXEL) {
            event_data = make_shared<PixelData>(PixelData(data_str));
        }
        else if (event_type == PLAYER_ELIMINATED) {
            event_data = make_shared<PlayerEliminatedData>(PlayerEliminatedData(data_str));
        }
        else { // GAME_OVER
            event_data = make_shared<GameOverData>(GameOverData());
        }
    }

    Event(EventType _event_type, const shared_ptr<EventData> &_event_data) :
            event_type(_event_type),
            event_data(_event_data),
            crc32(0) {
        len = sizeof(uint32_t) + 1 + event_data->size();
    }

    uint32_t calc_crc32(const string &body_str) {
        return ::crc32(body_str.c_str(), body_str.length()); // c_str adds \0 but length() doesn't count this
    }

    string serialize() {
        string body_str = body_serialize();
        crc32 = calc_crc32(body_str);
        return body_str + serialize32(crc32);
    }
};

#endif //SCREEN_WORMS_EVENTS_H
