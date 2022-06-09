#include <boost/program_options.hpp>

#include <iostream>
#include <random>
#include "structures.hpp"

using namespace std;

namespace po = boost::program_options;

/* -------------------------------------------------------------------------
   Useful structures and declarations
   ------------------------------------------------------------------------- */
/* structure for storing initial server params */
struct Settings {
    uint16_t bomb_timer;
    uint8_t players_count;
    uint64_t turn_duration;
    uint16_t explosion_radius;
    uint16_t initial_blocks;
    uint16_t game_length;
    string server_name;
    uint16_t port;
    uint32_t seed = static_cast<uint32_t>(chrono::system_clock::now().time_since_epoch().count());
    uint16_t size_x;
    uint16_t size_y;
};

/* structure for storing game state */
struct GameState {
    bool is_active = false;
    uint16_t turn_number;
    map<PlayerId, Player> players;
    map<PlayerId, Position> robot_positions;
    map<PlayerId, Score> scores;
    map<BombId, Bomb> bombs;
    set<Position> blocks;

    void reset();
};

/* structure for storing runtime data, game state and settings */
struct Server {
    Settings settings;
    GameState game_state;
    PlayerId next_player_id;
    BombId next_bomb_id;
    
    map<PlayerId, ClientMessageServer> players_messages;
    vector<AcceptedPlayer> accepted_players;
    vector<Turn> completed_turns;

    template <Unsigned T>
    T random(T mod);
    Position random_position();

    void reset_game_state();
    Hello hello_message();
    AcceptedPlayer add_accepted_player(Player &player);
    Turn simulate_turn();
    GameStarted start_game();
    GameEnded end_game();
};

/* -------------------------------------------------------------------------
   Global variables for less argument passing
   ------------------------------------------------------------------------- */
po::variables_map program_params;

Server server;

/* -------------------------------------------------------------------------
   Parsing & helper functions
   ------------------------------------------------------------------------- */
bool process_command_line(int argc, char **argv) {
    try {
        uint16_t players_count;

        po::options_description desc("Allowed options");
        desc.add_options()
            ("bomb-timer,b", po::value<uint16_t>(&server.settings.bomb_timer)->required())
            ("players-count,c", po::value<uint16_t>(&players_count)->required())
            ("turn-duration,d", po::value<uint64_t>(&server.settings.turn_duration)->required(),
                "milliseconds")
            ("explosion-radius,e", po::value<uint16_t>(&server.settings.explosion_radius)->required())
            ("help,h", "help message")
            ("initial-blocks,k", po::value<uint16_t>(&server.settings.initial_blocks)->required())
            ("game-length,l", po::value<uint16_t>(&server.settings.game_length)->required())
            ("server-name,n", po::value<string>(&server.settings.server_name)->required())
            ("port,p", po::value<uint16_t>(&server.settings.port)->required())
            ("seed,s", po::value<uint32_t>(&server.settings.seed), "optional")
            ("size-x,x", po::value<uint16_t>(&server.settings.size_x)->required())
            ("size-y,y", po::value<uint16_t>(&server.settings.size_y)->required());

        po::store(po::parse_command_line(argc, argv, desc), program_params);

        if (program_params.count("help")) {
            cout << desc << "\n";
            return false;
        }

        po::notify(program_params);

        if (players_count > UINT8_MAX) {
            throw invalid_argument("players-count value overflow");
        }
        server.settings.players_count = static_cast<uint8_t>(players_count);

    }
    catch(exception &e) {
        cerr << "error: " << e.what() << "\n";
        return false;
    }
    catch(...) {
        cerr << "Exception of unknown type!\n";
        return false;
    }

    return true;
}

void GameState::reset() {
    turn_number = 0;
    players = { };
    robot_positions = { };
    scores = { };
    bombs = { };
    blocks = { };
}

template <Unsigned T>
T Server::random(T mod) {
    minstd_rand random(settings.seed);
    return static_cast<T>(random() % static_cast<uint_fast32_t>(mod));
}

Position Server::random_position() {
    return Position {
        .x = random(settings.size_x),
        .y = random(settings.size_y),
    };
}

void Server::reset_game_state() {
    game_state.reset();
    next_bomb_id = 0;
    next_player_id = 0;

    players_messages = { };
    accepted_players = { };
    completed_turns = { };

    return;
}

Hello Server::hello_message() {
    return Hello {
        .server_name = settings.server_name,
        .players_count = settings.players_count,
        .size_x = settings.size_x,
        .size_y = settings.size_y,
        .game_length = settings.game_length,
        .explosion_radius = settings.explosion_radius,
        .bomb_timer = settings.bomb_timer,
    };
}

AcceptedPlayer Server::add_accepted_player(Player &player) {
    game_state.players.insert({next_player_id, player});
    
    AcceptedPlayer accepted_player = {
        .id = next_player_id,
        .player = player,
    };
    accepted_players.push_back(accepted_player);
    next_player_id++;

    return accepted_player;
}

GameStarted Server::start_game() {
    game_state.is_active = true;
    return GameStarted {
        .players = game_state.players,
    };
}

Turn Server::simulate_turn() {
    vector<Event> events = { };
    set<Position> blocks_destroyed = { };
    set<PlayerId> robots_destroyed = { };

    Turn turn_result;

    if (game_state.turn_number == 0) {
        for (auto const &[id, player] : game_state.players) {
            Position position = random_position();
            game_state.robot_positions.insert({id, position});
            events.push_back(PlayerMoved {id, position});
        }

        for (uint16_t block = 0; block < settings.initial_blocks; block++) {
            Position position = random_position();
            game_state.blocks.insert(position);
            events.push_back(BlockPlaced {position});
        }

        turn_result = {
            .turn = game_state.turn_number,
            .events = events,
        };
    }
    else { // game_state.turn_number > 0

    }

    game_state.turn_number++;
    players_messages = { };
    completed_turns.push_back(turn_result);

    return turn_result;
}

GameEnded Server::end_game() {
    game_state.is_active = false;
    return GameEnded {
        .scores = game_state.scores,
    };
}

/* -------------------------------------------------------------------------
   Main function
   ------------------------------------------------------------------------- */
int main(int argc, char *argv[]) {

    /* Parse command line parameters */
    bool parsing_result = process_command_line(argc, argv);

    if (!parsing_result) {
        cerr << "Failed to parse parameters\n";
        return 1;
    }

    try {
        boost::asio::io_context io_context;
    }
    catch (exception &e) {
        cerr << "error: " << e.what() << "\n";
        return 1;
    }
    catch(...) {
        cerr << "Exception of unknown type!\n";
        return 1;
    }

    return 0;
}