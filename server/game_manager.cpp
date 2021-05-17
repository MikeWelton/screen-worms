#include <map>
#include <utility>
#include <algorithm>
#include "../common/exceptions.h"
#include "../common/events.cpp"
#include "../common/messages.cpp"
#include "../utils/timer.h"
#include "../utils/rng.h"
#include "../utils/id_manager.h"

#define MIN_TURNING_SPEED 1
#define MAX_TURNING_SPEED 360
#define MIN_ROUNDS_PER_SEC 1
#define MAX_ROUNDS_PER_SEC 240
#define MIN_WIDTH 192
#define MAX_WIDTH 1920
#define MIN_HEIGHT 144
#define MAX_HEIGHT 1080


class PlayerData {
public:
    uint8_t number{};
    string name;
    double x{};
    double y{};
    uint8_t direction{};
    bool disconnected = false;
    bool playing = false;
    Timer timer;

    PlayerData() = default;

    explicit PlayerData(string _name) : name(std::move(_name)) {
        timer.start();
    }

    void init(uint32_t width, uint32_t height, Rng &rng) {
        // TODO here
    }
};

class GameState {
public:
    bool started = false;
    uint32_t game_id{};
    vector<Event> events;
    vector<vector<bool>> eaten_pixels; // true if pixel eaten

    GameState() = default;

    GameState(uint32_t id, uint32_t width, uint32_t height) :
            game_id(id),
            events(),
            eaten_pixels(width, vector<bool>(height, false)) {}

    void add_event(Event &event) {
        event.event_no = events.size();
        events.push_back(event);
    }
};

class GameManager {
private:
    /* Function checks if value is given limits (both are inclusive) and throws exception if
     * limits are violated. */
    static void check_limits(int value, int lower_bound, int upper_bound, const string &value_name) {
        if (value < lower_bound || upper_bound < value) {
            throw LimitException(value_name + " " + to_string(value) + " violates given limits "
                                 + to_string(lower_bound) + "-" + to_string(upper_bound));
        }
    }

    /*vector<PlayerData> sort_players_by_name() {
        vector<PlayerData> players;*/
        /* Extract values from map and sort them by name. */
        /*transform(players_data.begin(), players_data.end(), back_inserter(players),
                  [](const map<uint32_t, PlayerData>::value_type& val) {
            return val.second;
        });
        sort(players.begin(), players.end(),
             [](const PlayerData &data1, const PlayerData &data2) {
                 return data1.name < data2.name;
        });
        return players;
    }*/

    ServerMsg create_server_msg() {
        return ServerMsg(game_state.game_id, game_state.events);
    }

    ServerMsg create_server_msg_to_all() {
        return ServerMsg(game_state.game_id, game_state.events, true);
    }

    void generate_new_game() {
        vector<string> names;
        names.reserve(ready);
        for(auto &iter: players_data) {
            PlayerData &player = iter.second;
            names.push_back(player.name);
        }
        NewGameData data(width, height, names);
        Event event = Event(NEW_GAME, make_shared<NewGameData>(data));
        game_state.add_event(event);
    }

    void generate_player_eliminated(uint8_t player_num) {
        PlayerEliminatedData data(player_num);
        Event event = Event(PLAYER_ELIMINATED, make_shared<PlayerEliminatedData>(data));
    }

    void generate_pixel(uint8_t player_num, uint32_t x, uint32_t y) {
        PixelData data(player_num, x, y);
        Event event = Event(PIXEL, make_shared<PixelData>(data));
    }

    void generate_game_over() {

    }

    ServerMsg new_game(const ClientToServerMsg &msg) {
        game_state = GameState(rng.get_random(), width, height);
        playing = ready;
        ready = 0;

        generate_new_game();

        IdManager id_manager;
        for(auto &iter: players_data) {
            PlayerData &player = iter.second;
            player.playing = true;
            player.number = id_manager.get_next_id();
            player.x = (rng.get_random() % width) + 0.5;
            player.y = (rng.get_random() % height) + 0.5;
            player.direction = rng.get_random() % 360;
            if (game_state.eaten_pixels[player.x][player.y]) {
                --playing;
                player.playing = false;
                generate_player_eliminated(player.number);
            }
            else {
                game_state.eaten_pixels[player.x][player.y] = true;
                generate_pixel(player.number, player.x, player.y);
            }
        }

        if (playing == 1) {
            generate_game_over();
        }

        return create_server_msg_to_all();
    }

    ServerMsg new_player(const ClientToServerMsg &msg) {
        if (msg.turn_direction != 0) {
            ++ready;
        }

        if (game_state.started) {
            return create_server_msg();
        }
        else {
            if (ready >= 2 && ready == players_data.size()) { // game ready to start
                return new_game(msg);
            }
            else {
                return create_server_msg();
            }
        }
    }

public:
    Rng rng = Rng(time(nullptr));
    uint32_t turning_speed = 6;
    uint32_t rounds_per_sec = 50;
    uint32_t width = 640;
    uint32_t height = 480;
    GameState game_state;
    map<string, PlayerData> players_data;
    uint32_t ready = 0;
    uint32_t playing = 0;


    void set_turning_speed(int _turning_speed) {
        check_limits(_turning_speed, MIN_TURNING_SPEED, MAX_TURNING_SPEED, "Turning speed");
        turning_speed = _turning_speed;
    }

    void set_rounds_per_sec(int _rounds_per_sec) {
        check_limits(_rounds_per_sec, MIN_ROUNDS_PER_SEC, MAX_ROUNDS_PER_SEC, "Rounds per sec");
        rounds_per_sec = _rounds_per_sec;
    }

    void set_width(int _width) {
        check_limits(_width, MIN_WIDTH, MAX_WIDTH, "Width");
        width = _width;
    }

    void set_height(int _height) {
        check_limits(_height, MIN_HEIGHT, MAX_HEIGHT, "Height");
        height = _height;
    }

    ServerMsg new_message(const ClientToServerMsg &msg, const string &name) {
        auto iter = players_data.find(name);
        if (iter != players_data.end()) { // not observer
            PlayerData &player = iter->second;
            player.direction = msg.turn_direction;

            // TODO check event number
        }
    }

    ServerMsg new_participant(const ClientToServerMsg &msg, const string &name) {
        players_data[name] = PlayerData(msg.player_name); // TODO check if valid
        if (!msg.player_name.empty()) {
            return new_player(msg);
        }
        // otherwise it's observer and we don't care about what he is doing
    }

    void player_disconnected(const string &name) {
        players_data[name].disconnected = true;
    }

    ServerMsg cyclic_activities(Timer &timer) {
        if (timer.timeout(1 / rounds_per_sec)) {
            for (auto &iter: players_data) {
                PlayerData &player = iter.second;
                if (player.disconnected && player.playing) {

                    // TODO calculate stuff
                }
            }
            timer.start();
        }

        return ServerMsg();
    }
};
