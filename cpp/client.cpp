#include <boost/program_options.hpp>

#include <iostream>
#include "structures.hpp"

using namespace std;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::ip::udp;
using boost::asio::ip::tcp;

namespace po = boost::program_options;

#define MAX_UDP_DATA_SIZE 65527 // because of IPv6

/* -------------------------------------------------------------------------
   Useful templates and structures
   ------------------------------------------------------------------------- */
/* helper template for variant::visit */
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };

/* structure storing data about client */
struct Client {
    bool in_lobby = false;
    bool in_game = false;
    bool join_request_sent = false;

    map<BombId, Bomb> bombs;
    set<Position> blocks;
};
/* structure storing address and port as strings */
struct sockaddrStr {
    string addr;
    string port;
};

/* -------------------------------------------------------------------------
   Global variables for less argument passing
   ------------------------------------------------------------------------- */
po::variables_map program_params;
boost::asio::streambuf UDP_buffer;

Client client;
Hello settings;
Lobby lobby;
Game game;

sockaddrStr gui;
sockaddrStr server;
string player_name;
string port;

/* -------------------------------------------------------------------------
   Parsing & helper functions
   ------------------------------------------------------------------------- */
sockaddrStr get_sockaddr_str(string &str) {
    auto delimiter = str.find_last_of(':');
    sockaddrStr addr_str = {
        str.substr(0, delimiter),
        str.substr(delimiter + 1, str.size() - delimiter),
    };
    return addr_str;
}

bool process_command_line(int argc, char** argv) {
    string gui_sockaddr_str;
    string server_sockaddr_str;
    uint16_t port_u16;

    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("gui-address,d", po::value<string>(&gui_sockaddr_str)->required(), "gui address:port")
            ("help,h", "help message")
            ("player-name,n", po::value<string>(&player_name)->required(), "player name")
            ("port,p", po::value<uint16_t>(&port_u16)->required(), "port for comms from gui")
            ("server-address,s", po::value<string>(&server_sockaddr_str)->required(), 
                "server address:port");

        po::store(po::parse_command_line(argc, argv, desc), program_params);

        if (program_params.count("help")) {
            cout << desc << "\n";
            exit(0);
        }

        po::notify(program_params);
    }
    catch(exception &e) {
        cerr << "error: " << e.what() << "\n";
        return false;
    }
    catch(...) {
        cerr << "Exception of unknown type!\n";
        return false;
    }

    gui = get_sockaddr_str(gui_sockaddr_str);
    server = get_sockaddr_str(server_sockaddr_str);
    port = to_string(port_u16);

    return true;
}

void setup() {
    lobby.server_name = settings.server_name;
    lobby.players_count = settings.players_count;
    lobby.size_x = settings.size_x;
    lobby.size_y = settings.size_y;
    lobby.game_length = settings.game_length;
    lobby.explosion_radius = settings.explosion_radius;
    lobby.bomb_timer = settings.bomb_timer;
    lobby.players.clear();

    game.server_name = settings.server_name;
    game.size_x = settings.size_x;
    game.size_y = settings.size_y;
    game.game_length = settings.game_length;
    game.turn = 0;
    game.players.clear();
    game.player_positions.clear();
    game.blocks.clear();
    game.bombs.clear();
    game.explosions.clear();
    game.scores.clear();

    client.in_lobby = false;
    client.in_game = false;
    client.join_request_sent = false;
    client.bombs.clear();
    client.blocks.clear();
}

void accept_player(AcceptedPlayer &accepted_player) {
    lobby.players[accepted_player.id] = accepted_player.player;
    game.players[accepted_player.id] = accepted_player.player;
    game.scores[accepted_player.id] = (Score) 0;
}

void update_client_bomb_timers() {
    for (auto &[id, bomb] : client.bombs) {
        bomb.timer--;
    }
}

bool validate_position(SignedPosition &position) {
return (position.x >= 0 && position.x < settings.size_x &&
        position.y >= 0 && position.y < settings.size_y);
}

void update_bombs_explosions_blocks(vector<BombExploded> &bombs_exploded, 
    vector<BlockPlaced> &blocks_placed, 
    uint16_t &current_turn) 
{
    set<Position> explosions = { };
    set<Position> all_blocks_destroyed = { };
    set<PlayerId> all_robots_destroyed = { };

    for (BombExploded const &exploded : bombs_exploded) {
        Bomb bomb = client.bombs[exploded.id];
        vector<SignedPosition> explosion_direction = {{0, 1}, {1, 0}, {0, -1}, {-1, 0}};

        for (SignedPosition direction : explosion_direction) {
            SignedPosition check = {bomb.position.x, bomb.position.y};
            for (
                uint16_t radius = 0; 
                radius <= settings.explosion_radius && validate_position(check);
                radius++)
            {
                Position checked = {(uint16_t) check.x, (uint16_t) check.y};
                explosions.insert(checked);
                if (client.blocks.contains(checked)) {
                    break;
                }
                check += direction;
            }
        }

        for (PlayerId const &id : exploded.robots_destroyed) {
            all_robots_destroyed.insert(id);
        }
        for (Position const &position : exploded.blocks_destroyed) {
            all_blocks_destroyed.insert(position);
        }
        client.bombs.erase(exploded.id);
        
    }

    /* update blocks and copy the blocks from client to game */
    for (Position const &position : all_blocks_destroyed) {
        client.blocks.erase(position);
    }

    for (BlockPlaced const &block : blocks_placed) {
        client.blocks.insert(block.position);
    }
    game.blocks.assign(client.blocks.begin(), client.blocks.end());

    /* copy the bombs from client to game */
    game.bombs.clear();
    for (auto const &[id, bomb] : client.bombs) {
        game.bombs.push_back(bomb);
    }

    /* copy the explosions to game */
    game.explosions.assign(explosions.begin(), explosions.end());

    /* increment the scores of players whose robots were destroyed */
    for (PlayerId const &id : all_robots_destroyed) {
        game.scores[id]++;
    }

    /* update game turn */
    game.turn = current_turn;
}

/* -------------------------------------------------------------------------
   Coroutine function for communication from gui to server
   ------------------------------------------------------------------------- */
awaitable<void> gui_listener(tcp::socket &server_socket, udp::socket &client_socket) {
    boost::asio::streambuf read_streambuf;
    boost::asio::streambuf send_streambuf;

    auto read_UDP = [&](void* arg, size_t size) -> awaitable<void> {
        read_streambuf.sgetn((char *) arg, size);
        co_return;
    };

    for (;;) {
        /* ensure both streambufs are empty before operating on them */
        read_streambuf.consume(read_streambuf.size());
        send_streambuf.consume(send_streambuf.size());
        GuiMessageClient gui_message;
        try {
            boost::asio::streambuf::mutable_buffers_type mut_read_streambuf = 
                read_streambuf.prepare(MAX_UDP_DATA_SIZE);
            size_t receive_size = 
                co_await client_socket.async_receive(mut_read_streambuf, use_awaitable);
            read_streambuf.commit(receive_size);
            co_await deserialize(gui_message, read_UDP);
            if (read_streambuf.in_avail()) {
                throw length_error("leftover message bytes from gui, IGNORED\n");
            }
        }
        catch(exception &e) {
            cerr << "error: " << e.what() << " from gui, IGNORED\n";
            continue;
        }
        catch(...) {
            cerr << "Exception of unknown type!\n";
            continue;
        }

        if (client.in_lobby && !client.join_request_sent) {
            ClientMessageServer join = Join { .name = player_name };
            serialize(join, send_streambuf);
            co_await server_socket.async_send(send_streambuf.data(), use_awaitable);
            client.join_request_sent = true;
        }
        
        ClientMessageServer client_message;
        if (client.in_game) {
            visit(overloaded {
                [&](PlaceBomb) {
                    client_message = PlaceBomb { };
                },
                [&](PlaceBlock) {
                    client_message = PlaceBlock { };
                },
                [&](Move message) {
                    client_message = Move { .direction = message.direction };
                }
            }, gui_message);

            serialize(client_message, send_streambuf);
            co_await server_socket.async_send(send_streambuf.data(), use_awaitable);
        }
    }

    co_return;
}

/* -------------------------------------------------------------------------
   Coroutine function for communication from server to gui
   ------------------------------------------------------------------------- */
awaitable<void> server_listener(
    tcp::socket &server_socket,
    udp::socket &gui_socket,
    udp::endpoint &gui_endpoint) 
{
    boost::asio::streambuf read_streambuf;
    boost::asio::streambuf send_streambuf;

    auto read_TCP = [&](void* arg, size_t size) -> awaitable<void> {
        co_await boost::asio::async_read(
            server_socket, 
            read_streambuf, 
            boost::asio::transfer_exactly(size), 
            use_awaitable
        );
        read_streambuf.sgetn((char *) arg, size);
    };

    for (;;) {
        read_streambuf.consume(read_streambuf.size());
        send_streambuf.consume(send_streambuf.size());
        
        ServerMessageClient server_message;
        co_await deserialize(server_message, read_TCP);

        ClientMessageGui client_message;
        bool send_message = false;
        visit(overloaded {
            [&](Hello message) {
                if (!client.in_lobby && !client.in_game) {
                    settings = message;
                    setup();
                    client_message = lobby;
                    send_message = true;
                    client.in_lobby = true;
                }
            },
            [&](AcceptedPlayer message) {
                if (client.in_lobby) {
                    accept_player(message);
                    client_message = lobby;
                    send_message = true;
                }
            },
            [&](GameStarted message) {
                if (!client.in_game) {
                    for (auto &[id, player] : message.players) {
                        AcceptedPlayer new_player = {id, player};
                        accept_player(new_player);
                    }
                    client.in_lobby = false;
                    client.in_game = true;
                }
            },
            [&](Turn message) {
                if (client.in_game) {
                    vector<BombExploded> turn_bombs_exploded = { };
                    vector<BlockPlaced> turn_blocks_placed = { };

                    update_client_bomb_timers();

                    for (Event const &event : message.events) {
                        visit(overloaded {
                            [&](BombPlaced placed) {
                                client.bombs[placed.id] = 
                                    (Bomb) { placed.position, settings.bomb_timer };
                            },
                            [&](BombExploded exploded) {
                                turn_bombs_exploded.push_back(exploded);
                            },
                            [&](PlayerMoved player) {
                                game.player_positions[player.id] = player.position;
                            },
                            [&](BlockPlaced block) {
                                turn_blocks_placed.push_back(block);
                            }
                        }, event);
                    }

                    update_bombs_explosions_blocks(turn_bombs_exploded, turn_blocks_placed,
                        message.turn);
   
                    client_message = game;
                    send_message = true;
                }
            },
            [&](GameEnded) {
                if (client.in_game) {
                    setup();
                    client_message = lobby;
                    send_message = true;
                    client.in_lobby = true;
                    client.in_game = false;
                    client.join_request_sent = false; // to allow client to join again
                }
            }
        }, server_message);

        if (send_message) {
            serialize(client_message, send_streambuf);
            co_await gui_socket.async_send_to(send_streambuf.data(), gui_endpoint, use_awaitable);
        }
    }

    co_return;
}

/* -------------------------------------------------------------------------
   Main function for parsing, opening connections and starting coroutines
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
        
        /* Prepare socket for receiving data from gui */
        udp::socket client_socket(io_context);
        client_socket.open(boost::asio::ip::udp::v6());
        client_socket.bind({boost::asio::ip::udp::v6(), program_params["port"].as<uint16_t>()});

        /* Prepare socket & endpoint for sending data to gui */
        udp::resolver gui_resolver(io_context);
        udp::endpoint gui_endpoint = *gui_resolver.resolve(gui.addr, gui.port).begin();
        udp::socket gui_socket(io_context);
        gui_socket.open(gui_endpoint.protocol());

        /* Prepare socket for exchanging data with server */
        tcp::resolver server_resolver(io_context);
        tcp::resolver::results_type server_endpoints =
            server_resolver.resolve(server.addr, server.port);
        tcp::socket server_socket(io_context);
        boost::asio::connect(server_socket, server_endpoints);
        server_socket.set_option(tcp::no_delay(true)); // set the TCP_NODELAY flag

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        co_spawn(io_context, gui_listener(server_socket, client_socket), rethrow_exception);
        co_spawn(
            io_context, 
            server_listener(server_socket, gui_socket, gui_endpoint), 
            rethrow_exception
        );

        io_context.run();
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
