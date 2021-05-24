#include <map>
#include <utility>
#include <algorithm>
#include "../common/exceptions.h"
#include "../common/events.h"
#include "../common/messages.h"
#include "../utils/timer.h"
#include "../utils/rng.h"
#include "../utils/id_manager.h"

#define MIN_SEED 0
#define MAX_SEED UINT32_MAX
#define MIN_TURNING_SPEED 1
#define MAX_TURNING_SPEED 90
#define MIN_ROUNDS_PER_SEC 1
#define MAX_ROUNDS_PER_SEC 240
#define MIN_WIDTH 32
#define MAX_WIDTH 1920
#define MIN_HEIGHT 32
#define MAX_HEIGHT 1080

class PlayerData {
public:
    uint8_t number{};
    string name;
    double x{};
    double y{};
    uint8_t move_direction{};
    uint8_t turn_direction{};
    bool disconnected = false;
    bool playing = false;
    bool ready = false;
    Timer timer;

    PlayerData() = default;

    explicit PlayerData(string _name) : name(std::move(_name)) {
        timer.start();
    }
};

class GameState {
public:
    bool started = false;
    uint32_t game_id{};
    vector<Event> events;
    vector<vector<bool>> eaten_pixels; // true if pixel eaten
    uint32_t first_not_reported_event = 0;

    GameState() = default;

    GameState(uint32_t id, uint32_t width, uint32_t height) :
            started(true),
            game_id(id),
            events(),
            eaten_pixels(width, vector<bool>(height, false)) {}

    void add_event(Event &event) {
        event.event_no = events.size();
        events.push_back(event);
    }

    size_t get_last_event_num() {
        if (events.empty()) {
            return 0;
        }
        return events.back().event_no;
    }

    vector<Event> get_missing_events(size_t next_exp_event_no) {
        if (next_exp_event_no == first_not_reported_event || first_not_reported_event == 0) {
            return vector<Event>();
        }
        return vector<Event>(events.begin() + next_exp_event_no, events.end());
    };
};

class GameManager {
private:
    bool is_on_board(uint32_t x, uint32_t y) {
        return 0 <= x && x < width && 0 <= y && y < height;
    }

    ServerMsg create_server_msg(vector<Event> &events) {
        return ServerMsg(game_state.game_id, events);
    }

    ServerMsg create_server_msg_to_all() {
        vector<Event> &events = game_state.events;
        uint32_t first_to_report = game_state.first_not_reported_event;
        game_state.first_not_reported_event = events.size();
        return ServerMsg(game_state.game_id,
                         vector<Event>(events.begin() + first_to_report, events.end()), true);
    }

    void generate_new_game() {
        game_state = GameState(rng.get_random(), width, height);
        playing = ready;
        ready = 0;

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

    void generate_player_eliminated(PlayerData &player) {
        --playing;
        player.playing = false;
        PlayerEliminatedData data(player.number);
        Event event = Event(PLAYER_ELIMINATED, make_shared<PlayerEliminatedData>(data));
        game_state.add_event(event);
    }

    void generate_pixel(uint8_t player_num, uint32_t x, uint32_t y) {
        game_state.eaten_pixels[x][y] = true;
        PixelData data(player_num, x, y);
        Event event = Event(PIXEL, make_shared<PixelData>(data));
        game_state.add_event(event);
    }

    void generate_game_over() {
        game_state.started = false;
        ready = 0;
        playing = 0;

        for(auto &iter: players_data) {
            PlayerData &player = iter.second;
            player.playing = false;
        }

        GameOverData data;
        Event event = Event(GAME_OVER, make_shared<GameOverData>(data));
        game_state.add_event(event);
    }

    ServerMsg new_game(const ClientToServerMsg &msg) {
        generate_new_game();

        IdManager id_manager;
        for(auto &iter: players_data) {
            PlayerData &player = iter.second;
            player.ready = false;
            player.playing = true;
            player.number = id_manager.get_next_id();
            player.x = (rng.get_random() % width) + 0.5;
            player.y = (rng.get_random() % height) + 0.5;
            player.move_direction = rng.get_random() % 360;
            if (game_state.eaten_pixels[player.x][player.y]) {
                generate_player_eliminated(player);
            }
            else {
                generate_pixel(player.number, player.x, player.y);
            }
        }

        if (playing == 1) {
            generate_game_over();
        }

        return create_server_msg_to_all();
    }

    ServerMsg new_player(const ClientToServerMsg &msg, const string &name) {
        players_data[name].turn_direction = msg.turn_direction;
        if (msg.turn_direction != 0) {
            players_data[name].ready = true;
            ++ready;
        }

        if (game_state.started) {
            return create_server_msg(game_state.events);
        }
        else {
            if (ready >= 2 && ready == players_data.size()) { // game ready to start
                return new_game(msg);
            }
            else {
                return create_server_msg(game_state.events);
            }
        }
    }

public:
    Rng rng = Rng(time(nullptr));
    uint32_t turning_speed = 6;
    uint32_t rounds_per_sec = 50;
    uint32_t width = 640;
    uint32_t height = 480;
    GameState game_state = GameState();
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

    void set_rng(int seed) {
        check_limits(seed, MIN_SEED, MAX_SEED, "Seed");
        rng = Rng(seed);
    }

    ServerMsg new_message(const ClientToServerMsg &msg, const string &name) {
        auto iter = players_data.find(name);
        if (iter != players_data.end()) { // not observer
            PlayerData &player = iter->second;
            player.turn_direction = msg.turn_direction;
            if (!game_state.started && msg.turn_direction > 0 && !player.ready) {
                player.ready = true;
                ++ready;
            }

            if (ready >= 2 && ready == players_data.size() && !game_state.started) { // game ready to start
                return new_game(msg);
            }
            else if (game_state.get_last_event_num() >= msg.next_expected_event_no) {
                return ServerMsg(game_state.game_id,
                                 game_state.get_missing_events(msg.next_expected_event_no));
            }
        }
        return ServerMsg();
    }

    ServerMsg new_participant(const ClientToServerMsg &msg, const string &name) {
        players_data[name] = PlayerData(msg.player_name); // TODO check if valid
        if (!msg.player_name.empty()) {
            return new_player(msg, name);
        }
        // otherwise it's observer and we don't care about what he is doing
        return ServerMsg();
    }

    void player_disconnected(const string &name) {
        if (!name.empty()) {
            players_data[name].disconnected = true;
        }
    }

    ServerMsg cyclic_activities(Timer &timer) {
        // game has't started yet or started but we should still wait
        if (!game_state.started || !timer.timeout(1 / rounds_per_sec)) {
            return ServerMsg();
        }

        for (auto &iter: players_data) {
            PlayerData &player = iter.second;
            if (player.playing) {
                if (player.turn_direction == 1) {
                    player.move_direction += turning_speed;
                }
                else if (player.turn_direction == 2) {
                    player.move_direction -= turning_speed;
                }
                Coord vec = normalized_vector(player.move_direction);
                Coord old = Coord(player.x, player.y);
                player.x += vec.first;
                player.y += vec.second;

                if ((uint32_t) player.x == (uint32_t) old.first
                        && (uint32_t) player.y == (uint32_t) old.second) {
                    continue;
                }

                if (game_state.eaten_pixels[player.x][player.y]
                        || !is_on_board(player.x, player.y)) {
                    generate_player_eliminated(player);
                }
                else {
                    generate_pixel(player.number, player.x, player.y);
                }
            }
        }
        if (playing < 2) {
            generate_game_over();
        }
        timer.start();
        return create_server_msg_to_all();
    }
};
