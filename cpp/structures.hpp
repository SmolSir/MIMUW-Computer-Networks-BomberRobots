#include <boost/asio.hpp>
#include "include/boost/pfr/core.hpp"

#include <iostream>
#include <stdio.h>
#include <concepts>
#include <variant>

using namespace std;
using boost::asio::awaitable;
using boost::asio::use_awaitable;

#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
# define use_awaitable \
  boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

/* -------------------------------------------------------------------------
   General structures, enums and aliases
   ------------------------------------------------------------------------- */
/* Declarations */
enum class Direction : uint8_t;

struct Position;
struct Bomb;
struct Player;

using PlayerId = uint8_t;
using BombId = uint32_t;
using Score = uint32_t;

/* Definitions */
enum class Direction : uint8_t {
    Up,
    Right,
    Down,
    Left
};

struct Position {
    uint16_t x;
    uint16_t y;
    bool operator==(const Position &other) const = default;
};

struct Bomb {
    Position position;
    uint16_t timer;
    bool operator==(const Bomb &other) const = default;
};

struct Player {
    string name;
    string address;
    bool operator==(const Player &other) const = default;
};

/* -------------------------------------------------------------------------
   Structures for communication [gui -> client] & [client -> server] 
   ------------------------------------------------------------------------- */
/* Declarations */
struct Join;
struct PlaceBomb;
struct PlaceBlock;
struct Move;

/* Definitions */
struct Join {
    string name;
};

struct PlaceBomb {};

struct PlaceBlock {};

struct Move {
    Direction direction;
};

/* -------------------------------------------------------------------------
   Structures for communication [server -> client]
   ------------------------------------------------------------------------- */
/* Declarations */
struct Hello;
struct AcceptedPlayer;
struct GameStarted;
struct Turn;
struct GameEnded;

/* Event subtypes declarations */
struct BombPlaced;
struct BombExploded;
struct PlayerMoved;
struct BlockPlaced;

using Event = variant<BombPlaced, BombExploded, PlayerMoved, BlockPlaced>;

/* Definitions */
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
    vector<Event> events;
};

struct GameEnded {
    map<PlayerId, Score> scores;
};

/* Event subtypes definitions */
struct BombPlaced {
    BombId id;
    Position position;
};

struct BombExploded {
    BombId id;
    vector<PlayerId> robots_destroyed;
    vector<Position> blocks_destroyed;
};

struct PlayerMoved {
    PlayerId id;
    Position position;
};

struct BlockPlaced {
    Position position;
};
/* -------------------------------------------------------------------------
   Structures for communication [client -> gui]
   ------------------------------------------------------------------------- */
/* Declarations */
struct Lobby;
struct Game;

/* Definitions */
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
    bool operator==(const Game &other) const = default;
};

/* -------------------------------------------------------------------------
   Aliases for communication [FROM -> TO]
   ------------------------------------------------------------------------- */
using ClientMessageServer = variant<Join, PlaceBomb, PlaceBlock, Move>;
using ServerMessageClient = variant<Hello, AcceptedPlayer, GameStarted, Turn, GameEnded>;
using ClientMessageGui = variant<Lobby, Game>;
using GuiMessageClient = variant<PlaceBomb, PlaceBlock, Move>;

/* -------------------------------------------------------------------------
   Concepts for template serializing and deserializing functions
   ------------------------------------------------------------------------- */
template <class T>
concept Aggregate = is_aggregate_v<T>;
template <class T>
concept Enum = is_enum_v<T>;
template <class T>
concept Unsigned = is_unsigned_v<T>;

/* -------------------------------------------------------------------------
   Template functions for serializing all program structures
   ------------------------------------------------------------------------- */
/* Declarations */
template <Aggregate T>
void serialize(T const &arg, boost::asio::streambuf &sb);

template <Enum T>
void serialize(T const &arg, boost::asio::streambuf &sb);

template <Unsigned T>
void serialize(T const &arg, boost::asio::streambuf &sb);

template <class... Ts>
void serialize(variant<Ts...> const &arg, boost::asio::streambuf &sb);

template <class T>
void serialize(vector<T> const &vec, boost::asio::streambuf &sb);

template <class T>
void serialize(set<T> const &st, boost::asio::streambuf &sb);

template <class K, class V>
void serialize(map<K, V> const &map, boost::asio::streambuf &sb);

void serialize(string const &arg, boost::asio::streambuf &sb);

/* Definitions */
template <Aggregate T>
void serialize(T const &arg, boost::asio::streambuf &sb) {
    // cout << "aggregate specialization!\n";
    boost::pfr::for_each_field(arg, [&sb](auto const &field) { serialize(field, sb); });
}

template <Enum T>
void serialize(T const &arg, boost::asio::streambuf &sb) {
    // cout << "enum specialization!\ncode is: " << (int) arg << "\n";
    uint8_t code = (uint8_t) arg;
    sb.sputn((const char *) &code, sizeof(code));
}

template <Unsigned T>
void serialize(T const &arg, boost::asio::streambuf &sb) {
    if constexpr(sizeof(T) == sizeof(uint8_t)) {
        // cout << "uint8_t specialization!\nvalue is: " << (int) arg << "\n";
        uint8_t net_arg = arg;
        sb.sputn((const char *) &net_arg, sizeof(net_arg));
        return;
    }
    if constexpr(sizeof(T) == sizeof(uint16_t)) {
        // cout << "uint16_t specialization!\nvalue is: " << arg << "\n";
        uint16_t net_arg = htons(arg);
        sb.sputn((const char *) &net_arg, sizeof(net_arg));
        return;
    }
    if constexpr(sizeof(T) == sizeof(uint32_t)) {
        // cout << "uint32_t specialization!\nvalue is: " << arg << "\n";
        uint32_t net_arg = htonl(arg);
        sb.sputn((const char *) &net_arg, sizeof(net_arg));
        return;
    }
    throw invalid_argument("unknown unsigned type\n");
}

template <class... Ts>
void serialize(variant<Ts...> const &arg, boost::asio::streambuf &sb) {
    // cout << "variant specialization!\ncode is: " << (int) arg.index() << "\n";
    uint8_t code = (uint8_t) arg.index();
    sb.sputn((const char *) &code, sizeof(code));
    visit([&sb](auto const &a) {
        serialize(a, sb);
    }, arg);
}

template <class T>
void serialize(vector<T> const &vec, boost::asio::streambuf &sb) {
    // cout << "vector specialization!\nsize is: " << vec.size() << "\n";
    uint32_t vec_size = (uint32_t) vec.size();
    uint32_t net_vec_size = htonl(vec_size);
    sb.sputn((const char *) &net_vec_size, sizeof(net_vec_size));
    for (T const &elem : vec) {
        serialize(elem, sb);
    }
}

template <class T>
void serialize(set<T> const &st, boost::asio::streambuf &sb) {
    // cout << "set specialization!\nsize is: " << st.size() << "\n";
    uint32_t st_size = (uint32_t) st.size();
    uint32_t net_st_size = htonl(st_size);
    sb.sputn((const char *) &net_st_size, sizeof(net_st_size));
    for (T const &elem : st) {
        serialize(elem, sb);
    }
}

template <class K, class V>
void serialize(map<K, V> const &map, boost::asio::streambuf &sb) {
    // cout << "map specialization!\nsize is: " << map.size() << "\n";
    uint32_t map_size = (uint32_t) map.size();
    uint32_t net_map_size = htonl(map_size);
    sb.sputn((const char *) &net_map_size, sizeof(net_map_size));
    for (auto const &[const_key, val] : map) {
        K key = const_key;
        serialize(key, sb);
        serialize(val, sb);
    }
}

void serialize(string const &arg, boost::asio::streambuf &sb) {
    // cout << "string specialization!\nvalue is: " << arg << "\tsize is: " << arg.size() << "\n";
    if (arg.size() > UINT8_MAX) {
        throw length_error("string length above 255");
    }
    uint8_t arg_size = (uint8_t) arg.size();
    sb.sputn((const char *) &arg_size, sizeof(arg_size));
    sb.sputn((const char *) arg.data(), arg_size);
}

/* -------------------------------------------------------------------------
   Template functions for deserializing all program structures
   ------------------------------------------------------------------------- */
/* Declarations */
template <Aggregate T, typename F>
awaitable<void> deserialize(T &arg, F &read);

template <Enum T, typename F>
awaitable<void> deserialize(T &arg, F &read);

template <Unsigned T, typename F>
awaitable<void> deserialize(T &arg, F &read);

template <class... Ts, typename F>
awaitable<void> deserialize(variant<Ts...> &arg, F &read);

template <class T, typename F>
awaitable<void> deserialize(vector<T> &vec, F &read);

template <class T, typename F>
awaitable<void> deserialize(set<T> &st, F &read);

template <class K, class V, typename F>
awaitable<void> deserialize(map<K, V> &map, F &read);

template <typename F>
awaitable<void> deserialize(string &arg, F &read);

/* Definitions */
template <Aggregate T, typename F>
awaitable<void> deserialize(T &arg, F &read) {
    co_await std::apply([&read] (auto &... fields) -> awaitable<void> {
        (co_await deserialize(fields, read), ...);
    }, boost::pfr::structure_tie(arg));
    co_return;
}

template <Enum T, typename F>
awaitable<void> deserialize(T &arg, F &read) {
    co_await read(&arg, sizeof(uint8_t));
    co_return;
}

template <Unsigned T, typename F>
awaitable<void> deserialize(T &arg, F &read) {
    if constexpr(sizeof(T) == sizeof(uint8_t)) {
        co_await read(&arg, sizeof(uint8_t));
        co_return;
    }
    if constexpr(sizeof(T) == sizeof(uint16_t)) {
        co_await read(&arg, sizeof(uint16_t));
        arg = ntohs(arg);
        co_return;
    }
    if constexpr(sizeof(T) == sizeof(uint32_t)) {
        co_await read(&arg, sizeof(uint32_t));
        arg = ntohl(arg);
        co_return;
    }
    throw invalid_argument("unknown unsigned type\n");
}

template <class... Ts, typename F>
awaitable<void> deserialize(variant<Ts...> &arg, F &read) {
    uint8_t code;
    co_await deserialize(code, read);
    if (code >= variant_size_v<variant<Ts...>>) {
        throw invalid_argument("unknown variant type ID\n");
    }
    /* we need as many ifs as there are codes possible, here from 0 to 4 */
    if (code == 0) { // All variants have defined code 0
        variant_alternative_t<0, variant<Ts...>> alt;
        co_await deserialize(alt, read);
        arg = alt;
    }
    if (code == 1) { // All variants have defined code 1
        variant_alternative_t<1, variant<Ts...>> alt;
        co_await deserialize(alt, read);
        arg = alt;
    }
    if constexpr(is_same_v<variant<Ts...>, ClientMessageServer> ||
                 is_same_v<variant<Ts...>, ServerMessageClient> ||
                 is_same_v<variant<Ts...>, GuiMessageClient> ||
                 is_same_v<variant<Ts...>, Event>
    ) {
        if (code == 2) {
            variant_alternative_t<2, variant<Ts...>> alt;
            co_await deserialize(alt, read);
            arg = alt;
        }
    }
    if constexpr(is_same_v<variant<Ts...>, ClientMessageServer> ||
                 is_same_v<variant<Ts...>, ServerMessageClient> ||
                 is_same_v<variant<Ts...>, Event>
    ) {
        if (code == 3) {
            variant_alternative_t<3, variant<Ts...>> alt;
            co_await deserialize(alt, read);
            arg = alt;
        }
    }
    if constexpr(is_same_v<variant<Ts...>, ServerMessageClient>) {
        if (code == 4) {
            variant_alternative_t<4, variant<Ts...>> alt;
            co_await deserialize(alt, read);
            arg = alt;
        }
    }
    co_return;
}

template <class T, typename F>
awaitable<void> deserialize(vector<T> &vec, F &read) {
    uint32_t size;
    co_await deserialize(size, read);
    vec.resize(size);
    for (T &elem : vec) {
        co_await deserialize(elem, read);
    }
    co_return;
}

template <class T, typename F>
awaitable<void> deserialize(set<T> &st, F &read) {
    uint32_t size;
    co_await deserialize(size, read);
    while (size--) {
        T elem;
        co_await deserialize(elem, read);
        st.insert(elem);
    }
}


template <class K, class V, typename F>
awaitable<void> deserialize(map<K, V> &map, F &read) {
    uint32_t size;
    co_await deserialize(size, read);
    while (size--) {
        K key;
        V value;
        co_await deserialize(key, read);
        co_await deserialize(value, read);
        map.insert({key, value});
    }
    co_return;
}

template <typename F>
awaitable<void> deserialize(string &arg, F &read) {
    uint8_t size;
    co_await deserialize(size, read);
    arg.resize(size);
    co_await read(arg.data(), size);
    co_return;
}
