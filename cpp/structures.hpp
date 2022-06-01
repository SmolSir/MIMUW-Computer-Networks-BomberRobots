#include <boost/asio.hpp>
#include "include/boost/pfr/core.hpp"

#include <stdio.h>
#include <concepts>
#include <variant>

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
struct SignedPosition;
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
    auto operator<=>(const Position &) const = default;
};

struct SignedPosition {
    int32_t x;
    int32_t y;
    SignedPosition& operator+=(const SignedPosition &other) {
        this->x += other.x;
        this->y += other.y;
        return *this;
    };
};

struct Bomb {
    Position position;
    uint16_t timer;
};

struct Player {
    std::string name;
    std::string address;
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
    std::string name;
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

using Event = std::variant<BombPlaced, BombExploded, PlayerMoved, BlockPlaced>;

/* Definitions */
struct Hello {
    std::string server_name;
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
    std::map<PlayerId, Player> players;
};

struct Turn {
    uint16_t turn;
    std::vector<Event> events;
};

struct GameEnded {
    std::map<PlayerId, Score> scores;
};

/* Event subtypes definitions */
struct BombPlaced {
    BombId id;
    Position position;
};

struct BombExploded {
    BombId id;
    std::vector<PlayerId> robots_destroyed;
    std::vector<Position> blocks_destroyed;
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
    std::string server_name;
    uint8_t players_count;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t explosion_radius;
    uint16_t bomb_timer;
    std::map<PlayerId, Player> players;
};

struct Game {
    std::string server_name;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t game_length;
    uint16_t turn;
    std::map<PlayerId, Player> players;
    std::map<PlayerId, Position> player_positions;
    std::vector<Position> blocks;
    std::vector<Bomb> bombs;
    std::vector<Position> explosions;
    std::map<PlayerId, Score> scores;
};

/* -------------------------------------------------------------------------
   Aliases for communication [FROM -> TO]
   ------------------------------------------------------------------------- */
using ClientMessageServer = std::variant<Join, PlaceBomb, PlaceBlock, Move>;
using ServerMessageClient = std::variant<Hello, AcceptedPlayer, GameStarted, Turn, GameEnded>;
using ClientMessageGui = std::variant<Lobby, Game>;
using GuiMessageClient = std::variant<PlaceBomb, PlaceBlock, Move>;

/* -------------------------------------------------------------------------
   Concepts for template serializing and deserializing functions
   ------------------------------------------------------------------------- */
template <class T>
concept Aggregate = std::is_aggregate_v<T>;
template <class T>
concept Enum = std::is_enum_v<T>;
template <class T>
concept Unsigned = std::is_unsigned_v<T>;

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
void serialize(std::variant<Ts...> const &arg, boost::asio::streambuf &sb);

template <class T>
void serialize(std::vector<T> const &vec, boost::asio::streambuf &sb);

template <class K, class V>
void serialize(std::map<K, V> const &map, boost::asio::streambuf &sb);

void serialize(std::string const &arg, boost::asio::streambuf &sb);

/* Definitions */
template <Aggregate T>
void serialize(T const &arg, boost::asio::streambuf &sb) {
    boost::pfr::for_each_field(arg, [&sb](auto const &field) { serialize(field, sb); });
}

template <Enum T>
void serialize(T const &arg, boost::asio::streambuf &sb) {
    uint8_t code = (uint8_t) arg;
    sb.sputn((const char *) &code, sizeof(code));
}

template <Unsigned T>
void serialize(T const &arg, boost::asio::streambuf &sb) {
    if constexpr (sizeof(T) == sizeof(uint8_t)) {
        uint8_t net_arg = arg;
        sb.sputn((const char *) &net_arg, sizeof(net_arg));
        return;
    }
    if constexpr (sizeof(T) == sizeof(uint16_t)) {
        uint16_t net_arg = htons(arg);
        sb.sputn((const char *) &net_arg, sizeof(net_arg));
        return;
    }
    if constexpr (sizeof(T) == sizeof(uint32_t)) {
        uint32_t net_arg = htonl(arg);
        sb.sputn((const char *) &net_arg, sizeof(net_arg));
        return;
    }
    throw std::invalid_argument("unknown unsigned type\n");
}

template <class... Ts>
void serialize(std::variant<Ts...> const &arg, boost::asio::streambuf &sb) {
    uint8_t code = (uint8_t) arg.index();
    sb.sputn((const char *) &code, sizeof(code));
    visit([&sb](auto const &a) {
        serialize(a, sb);
    }, arg);
}

template <class T>
void serialize(std::vector<T> const &vec, boost::asio::streambuf &sb) {
    uint32_t vec_size = (uint32_t) vec.size();
    uint32_t net_vec_size = htonl(vec_size);
    sb.sputn((const char *) &net_vec_size, sizeof(net_vec_size));
    for (T const &elem : vec) {
        serialize(elem, sb);
    }
}

template <class K, class V>
void serialize(std::map<K, V> const &map, boost::asio::streambuf &sb) {
    uint32_t map_size = (uint32_t) map.size();
    uint32_t net_map_size = htonl(map_size);
    sb.sputn((const char *) &net_map_size, sizeof(net_map_size));
    for (auto const &[const_key, val] : map) {
        K key = const_key;
        serialize(key, sb);
        serialize(val, sb);
    }
}

void serialize(std::string const &arg, boost::asio::streambuf &sb) {
    if (arg.size() > UINT8_MAX) {
        throw std::length_error("std::string length above 255");
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
awaitable<void> deserialize(std::variant<Ts...> &arg, F &read);

template <class T, typename F>
awaitable<void> deserialize(std::vector<T> &vec, F &read);

template <class K, class V, typename F>
awaitable<void> deserialize(std::map<K, V> &map, F &read);

template <typename F>
awaitable<void> deserialize(std::string &arg, F &read);

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
    if constexpr (sizeof(T) == sizeof(uint8_t)) {
        co_await read(&arg, sizeof(uint8_t));
        co_return;
    }
    if constexpr (sizeof(T) == sizeof(uint16_t)) {
        co_await read(&arg, sizeof(uint16_t));
        arg = ntohs(arg);
        co_return;
    }
    if constexpr (sizeof(T) == sizeof(uint32_t)) {
        co_await read(&arg, sizeof(uint32_t));
        arg = ntohl(arg);
        co_return;
    }
    throw std::invalid_argument("unknown unsigned type\n");
}

template <class... Ts, typename F>
awaitable<void> deserialize(std::variant<Ts...> &arg, F &read) {
    uint8_t code;
    co_await deserialize(code, read);
    if (code >= std::variant_size_v<std::variant<Ts...>>) {
        throw std::invalid_argument("unknown variant type ID\n");
    }
    /* we need as many ifs as there are codes possible, here from 0 to 4 */
    if (code == 0) { // All variants have defined code 0
        std::variant_alternative_t<0, std::variant<Ts...>> alt;
        co_await deserialize(alt, read);
        arg = alt;
    }
    if (code == 1) { // All variants have defined code 1
        std::variant_alternative_t<1, std::variant<Ts...>> alt;
        co_await deserialize(alt, read);
        arg = alt;
    }
    if constexpr (std::is_same_v<std::variant<Ts...>, ClientMessageServer> ||
                 std::is_same_v<std::variant<Ts...>, ServerMessageClient> ||
                 std::is_same_v<std::variant<Ts...>, GuiMessageClient> ||
                 std::is_same_v<std::variant<Ts...>, Event>
    ) {
        if (code == 2) {
            std::variant_alternative_t<2, std::variant<Ts...>> alt;
            co_await deserialize(alt, read);
            arg = alt;
        }
    }
    if constexpr (std::is_same_v<std::variant<Ts...>, ClientMessageServer> ||
                 std::is_same_v<std::variant<Ts...>, ServerMessageClient> ||
                 std::is_same_v<std::variant<Ts...>, Event>
    ) {
        if (code == 3) {
            std::variant_alternative_t<3, std::variant<Ts...>> alt;
            co_await deserialize(alt, read);
            arg = alt;
        }
    }
    if constexpr (std::is_same_v<std::variant<Ts...>, ServerMessageClient>) {
        if (code == 4) {
            std::variant_alternative_t<4, std::variant<Ts...>> alt;
            co_await deserialize(alt, read);
            arg = alt;
        }
    }
    co_return;
}

template <class T, typename F>
awaitable<void> deserialize(std::vector<T> &vec, F &read) {
    uint32_t size;
    co_await deserialize(size, read);
    vec.resize(size);
    for (T &elem : vec) {
        co_await deserialize(elem, read);
    }
    co_return;
}

template <class K, class V, typename F>
awaitable<void> deserialize(std::map<K, V> &map, F &read) {
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
awaitable<void> deserialize(std::string &arg, F &read) {
    uint8_t size;
    co_await deserialize(size, read);
    arg.resize(size);
    co_await read(arg.data(), size);
    co_return;
}
