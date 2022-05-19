#include <boost/asio.hpp>
#include "include/boost/pfr/core.hpp"

#include <iostream>
#include <stdio.h>
#include <concepts>

#include "enums.hpp"

using namespace std;

/* -------------------------------------------------------------------------
   Aliases and simple wrapper structures
   ------------------------------------------------------------------------- */
using PlayerId = uint8_t;
using BombId = uint32_t;
using Score = uint32_t;

struct Position {
    uint16_t x;
    uint16_t y;
};

struct Bomb {
    Position position;
    uint16_t timer;
};

struct Player {
    string name;
    string address;
};


/* -------------------------------------------------------------------------
   Structures for communication from client to server 
   ------------------------------------------------------------------------- */
struct Join {
    string name;
};

struct PlaceBomb {};

struct PlaceBlock {};

struct Move {
    Direction direction;
};

/* -------------------------------------------------------------------------
   Structures for communication from server to client 
   ------------------------------------------------------------------------- */
struct Hello {
    string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
};

struct AcceptedPlayer {
    PlayerId id;
    Player player;
};

struct GameStarted {
    map<PlayerId, Player> players;
};

struct Turn {
    uint16_t turn;
    // TODO! events
};

struct GameEnded {
    map<PlayerId, Score> scores;
};

/* -------------------------------------------------------------------------
   Structures for communication from client to gui
   ------------------------------------------------------------------------- */
struct Lobby {
    string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    map<PlayerId, Player> players;
};

struct Game {
    string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t turn;
    map<PlayerId, Player> players;
    map<PlayerId, Position> player_positions;
    vector<Position> blocks;
    vector<Bomb> bombs;
    vector<Position> explosions;
    map<PlayerId, Score> scores;
};


/* -------------------------------------------------------------------------
   Template functions for serializing all program structures
   ------------------------------------------------------------------------- */
template <class T>
void serialize(T &arg, boost::asio::streambuf &sb) {
    if (is_aggregate_v<T>) {
        cout << "some aggregate at least\n";
        boost::pfr::for_each_field(arg, [&sb](auto &field) { serialize(field, sb); });
    }
    else {
        throw invalid_argument("cannot serialize a non-aggregate object");
    }
}

template <>
void serialize(uint8_t &arg, boost::asio::streambuf &sb) {
    cout << "uint8_t specialization!\nvalue is: " << (int) arg << "\n";
    uint8_t net_arg = arg;
    sb.sputn((const char *) &net_arg, sizeof(net_arg));
}

template <>
void serialize(uint16_t &arg, boost::asio::streambuf &sb) {
    cout << "uint16_t specialization!\nvalue is: " << arg << "\n";
    uint16_t net_arg = htons(arg);
    sb.sputn((const char *) &net_arg, sizeof(net_arg));
}

template <>
void serialize(uint32_t &arg, boost::asio::streambuf &sb) {
    cout << "uint32_t specialization!\nvalue is: " << arg << "\n";
    uint32_t net_arg = htonl(arg);
    sb.sputn((const char *) &net_arg, sizeof(net_arg));
}

template <>
void serialize(string &arg, boost::asio::streambuf &sb) {
    cout << "string specialization!\nvalue is: " << arg << "\tsize is: " << arg.size() << "\n";
    if (arg.size() > UINT8_MAX) {
        throw length_error("string length above 255");
    }
    uint8_t arg_size = (uint8_t) arg.size();
    sb.sputn((const char *) &arg_size, sizeof(arg_size));
    sb.sputn((const char *) arg.data(), arg_size);
}

template <class T>
void serialize(vector<T> &vec, boost::asio::streambuf &sb) {
    cout << "vector specialization!\nsize is: " << vec.size() << "\n";
    uint32_t vec_size = (uint32_t) vec.size();
    uint32_t net_vec_size = htonl(vec_size);
    sb.sputn((const char *) &net_vec_size, sizeof(net_vec_size));
    for (T &elem : vec) {
        serialize(elem, sb);
    }
}

template <class K, class V>
void serialize(map<K, V> &map, boost::asio::streambuf &sb) {
    uint32_t map_size = (uint32_t) map.size();
    uint32_t net_map_size = htonl(map_size);
    sb.sputn((const char *) &net_map_size, sizeof(net_map_size));
    for (auto &[const_key, val] : map) {
        K key = const_key;
        serialize(key, sb);
        serialize(val, sb);
    }
}
