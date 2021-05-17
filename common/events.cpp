#include <cstdint>
#include <string>
#include <vector>
#include <cstring>
#include <memory>
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
    virtual size_t size() = 0;

    virtual string to_string() = 0;

    virtual string to_gui_msg(const string &player_name) = 0;
};

class NewGameData : public EventData {
public:
    string name = "NEW_GAME";
    uint32_t maxx; // 4 bajty, szerokość planszy w pikselach, liczba bez znaku
    uint32_t maxy; // 4 bajty, wysokość planszy w pikselach, liczba bez znaku
    // następnie lista nazw graczy zawierająca dla każdego z graczy player_name, jak w punkcie „2.1. Komunikaty od klienta do serwera”, oraz znak '\0'
    vector<string> player_names; // 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone

    NewGameData(const string &str) {
        memcpy(&maxx, (uint32_t *) str.substr(0, 4).c_str(), 4);
        memcpy(&maxy, (uint32_t *) str.substr(4, 4).c_str(), 4);
        string names = str.substr(8, str.length() - 8);
        player_names = split(names, "\0");
    }

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
            ret.append(str + '\0');
        }
        return ret;
    }

    string to_gui_msg(const string &player_name) override {
        string ret = name + " " + std::to_string(maxx) + " " + std::to_string(maxy);
        for (auto &str: player_names) {
            ret.append(" " + str);
        }
        ret.append("\n");
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
        memcpy(&player_number, (uint8_t *) str.c_str(), 1);
        memcpy(&x, (uint32_t *) str.substr(1, 4).c_str(), 4);
        memcpy(&y, (uint32_t *) str.substr(5, 4).c_str(), 4);
    }

    PixelData(uint8_t player_number, uint32_t x, uint32_t y) : player_number(player_number),
                                                               x(x), y(y) {}

    size_t size() override {
        return sizeof(uint8_t) + 2 * sizeof(uint32_t);
    }

    string to_string() override {
        return std::to_string(player_number) + std::to_string(x) + std::to_string(y);
    }

    string to_gui_msg(const string &player_name) override {
        return name + " " + std::to_string(x) + " " + std::to_string(y) + "" + player_name + "\n";
    }
};

class PlayerEliminatedData : public EventData {
public:
    string name = "PLAYER_ELIMINATED";
    uint8_t player_number; // 1 bajt;

    PlayerEliminatedData(const string &str) {
        memcpy(&player_number, (uint8_t *) str.c_str(), 1);
    }

    explicit PlayerEliminatedData(uint8_t player_number) : player_number(player_number) {}

    size_t size() override {
        return sizeof(uint8_t);
    }

    string to_string() override {
        return std::to_string(player_number);
    }

    string to_gui_msg(const string &player_name) override {
        return name + " " + player_name + "\n";
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
private:
    string body_string() {
        string ret = std::to_string(len) + std::to_string(event_no);
        ret.append((char *) &event_type, 1);
        ret.append(event_data->to_string());
        return ret;
    }

    bool correct_event_type(uint8_t type) {
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

    Event(const string &msg) {
        len = string_to_int(msg.substr(0, 4));
        event_no = string_to_int(msg.substr(4, 4));
        uint8_t event_t = string_to_int(msg.substr(8, 1));
        if (!correct_event_type(event_t) || event_t == GAME_OVER) {
            throw UnknownEventTypeException();
        }
        event_type = (EventType) event_t;
        string body = msg.substr(0, msg.length() - 4);
        uint32_t crc = string_to_int(msg.substr(msg.length() - 4, 4));
        if (crc != ::crc32(body.c_str(), body.length())) {
            throw IncorrectCrc32Exception();
        }
        crc32 = crc;

        string data_str = body.substr(9, body.length() - 9);
        if (event_type == NEW_GAME) {
            event_data = make_shared<NewGameData>(NewGameData(data_str));
        }
        else if (event_type == PIXEL) {
            event_data = make_shared<PixelData>(PixelData(data_str));
        }
        else if (event_type == PLAYER_ELIMINATED) {
            event_data = make_shared<PlayerEliminatedData>(PlayerEliminatedData(data_str));
        }

    }

    Event(EventType _event_type, const shared_ptr<EventData> &_event_data) :
            event_type(_event_type),
            event_data(_event_data),
            crc32(0) {
        len = sizeof(uint32_t) + 1 + event_data->size();
    }

    uint32_t calc_crc32() {
        string s = body_string();
        return ::crc32(s.c_str(), s.length()); // c_str adds \0 but length() doesn't count this
    }

    string to_string() {
        string ret = body_string();
        crc32 = calc_crc32();
        ret.append(std::to_string(crc32));
        return ret;
    }
};
