#include <cstdint>
#include <string>
#include <vector>
#include <iostream>

using namespace std;

/* Returns result int if conversion is successful or throws exception if result is <= 0
 * or passes exception if stoi threw one. */
int string_to_int(const string &str) {
    int result = stoi(str);
    if (result <= 0) {
        throw exception();
    }
    return result;
}

string resolve_host(string &addr) {

}

void exit_error(string str) {
    cout << str << endl;
    exit(EXIT_FAILURE);
}

enum EventType {
    NEW_GAME = 0,
    PIXEL = 1,
    PLAYER_ELIMINATED = 2,
    GAME_OVER = 3,
};

class NewGameData {
    uint32_t maxx; // 4 bajty, szerokość planszy w pikselach, liczba bez znaku
    uint32_t maxy; // 4 bajty, wysokość planszy w pikselach, liczba bez znaku
    // następnie lista nazw graczy zawierająca dla każdego z graczy player_name, jak w punkcie „2.1. Komunikaty od klienta do serwera”, oraz znak '\0'
    vector<string> player_names; // 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone
};

class PixelData {
    uint8_t player_number; // 1 bajt
    uint32_t x; // 4 bajty, odcięta, liczba bez znaku
    uint32_t y; // 4 bajty, rzędna, liczba bez znaku
};

class PlayerEliminatedData {
    uint8_t player_number; // 1 bajt;
};

union EventData {
    NewGameData new_game;
    PixelData pixel;
    PlayerEliminatedData player_eliminated;
    void *game_over;
};

class Event {
    uint32_t len; // 4 bajty, liczba bez znaku, sumaryczna długość pól event_*
    uint32_t event_no; // 4 bajty, liczba bez znaku, dla każdej partii kolejne wartości, począwszy od zera
    EventType event_type; // 1 bajt
    EventData event_data; // zależy od typu, patrz opis poniżej
    uint32_t crc32; // 4 bajty, liczba bez znaku, suma kontrolna obejmująca pola od pola len do event_data włącznie, obliczona standardowym algorytmem CRC-32-IEEE
};

/* Message send from client. */
class ClientMsg {
    uint64_t session_id; // 8 bajtów, liczba bez znaku
    uint8_t turn_direction; // 1 bajt, liczba bez znaku, wartość 0 → prosto, wartość 1 → w prawo, wartość 2 → w lewo
    uint32_t next_expected_event_no; // 4 bajty, liczba bez znaku
    string player_name; // 0–20 znaków ASCII o wartościach z przedziału 33–126, w szczególności spacje nie są dozwolone
};

/* Message send from server. */
class ServerMsg {
    uint32_t game_id; // 4 bajty, liczba bez znaku
    vector<Event> events; // zmienna liczba rekordów, zgodnych z poniższą specyfikacją
};
