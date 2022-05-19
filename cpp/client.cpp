#include <boost/program_options.hpp>

#include "structures.hpp"

using boost::asio::awaitable;
using boost::asio::use_awaitable;
using boost::asio::co_spawn;
using boost::asio::detached;
using boost::asio::ip::udp;
using boost::asio::ip::tcp;

namespace po = boost::program_options;
namespace this_coro = boost::asio::this_coro;

#if defined(BOOST_ASIO_ENABLE_HANDLER_TRACKING)
# define use_awaitable \
  boost::asio::use_awaitable_t(__FILE__, __LINE__, __PRETTY_FUNCTION__)
#endif

/* structure storing address and port as strings */
struct sockaddr_str {
    string addr;
    string port;
};

string make_string(boost::asio::streambuf& streambuf) {
    return {buffers_begin(streambuf.data()), buffers_end(streambuf.data())};
}

void print_streambuf(boost::asio::streambuf &streambuf) {
    string content = {buffers_begin(streambuf.data()), buffers_end(streambuf.data())};
    cout << "buffer has " << content.size() << " byte(s)\n";
    cout << "ASCII:\n";
    for (auto c : content) cout << (int) c << "\t";
    cout << "\n";
    cout << "CHARS:\n";
    for (auto c : content) cout << (char) c << "\t";
    cout << "\n";
}

sockaddr_str get_sockaddr_str(string &str) {
    auto delimiter = str.find_last_of(':');
    sockaddr_str addr_str = {
        str.substr(0, delimiter),
        str.substr(delimiter + 1, str.size() - delimiter),
    };
    return addr_str;
}

bool process_command_line(int argc, char** argv, 
                          sockaddr_str &gui, 
                          sockaddr_str &srv, 
                          string &name, 
                          string &port) 
{
    string gui_str;
    string srv_str;
    uint16_t port_u16;

    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("gui-address,d", po::value<string>(&gui_str)->required(), "gui address:port")
            ("help,h", "help message")
            ("player-name,n", po::value<string>(&name)->required(), "player name")
            ("port,p", po::value<uint16_t>(&port_u16)->required(), "port for comms from gui")
            ("server-address,s", po::value<string>(&srv_str)->required(), "server address:port")
        ;

        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);

        if (vm.count("help")) {
            cout << desc << "\n";
            return false;
        }

        po::notify(vm);
    }
    catch(exception &e) {
        cerr << "error: " << e.what() << "\n";
        return false;
    }
    catch(...) {
        cerr << "Exception of unknown type!\n";
        return false;
    }

    gui = get_sockaddr_str(gui_str);
    srv = get_sockaddr_str(srv_str);
    port = to_string(port_u16);

    return true;
}

awaitable<void> gui_listener() {
    for (int i = 0; i < 10; i++) {
        boost::asio::deadline_timer timer(co_await boost::asio::this_coro::executor, boost::posix_time::seconds(2));
        co_await timer.async_wait(use_awaitable);
        cout << "gui_listener" << endl;
    }
}

awaitable<void> server_listener() {
    for (int i = 0; i < 4; i++) {
        boost::asio::deadline_timer timer(co_await boost::asio::this_coro::executor, boost::posix_time::seconds(5));
        co_await timer.async_wait(use_awaitable);
        cout << "server_listener" << endl;
    }
}

int main(int argc, char *argv[]) {

    sockaddr_str gui;
    sockaddr_str srv;
    string player_name;
    string port;

    /* parsing command line parameters */
    bool parsing_result = process_command_line(argc, argv, gui, srv, player_name, port);

    if (!parsing_result) {
        cerr << "Failed to parse parameters.\n";
        return 1;
    }

    cout << gui.addr << ":" << gui.port << "    "
         << srv.addr << ":" << srv.port << "    "
         << player_name << "    "
         << port << "\n";

    try {
        boost::asio::io_context io_context;
        
        udp::resolver gui_resolver(io_context);
        udp::endpoint gui_endpoint = *gui_resolver.resolve(gui.addr, gui.port).begin();
        udp::socket gui_socket(io_context);
        gui_socket.open(gui_endpoint.protocol());

        tcp::resolver srv_resolver(io_context);
        tcp::resolver::results_type srv_endpoints = srv_resolver.resolve(srv.addr, srv.port);
        tcp::socket srv_socket(io_context);
//        boost::asio::connect(srv_socket, srv_endpoints);

        boost::asio::signal_set signals(io_context, SIGINT, SIGTERM);
        signals.async_wait([&](auto, auto){ io_context.stop(); });

        // TEST 1
        // boost::asio::streambuf buf;
        // uint8_t message1 = 8;
        // uint16_t message2 = 16;
        // uint32_t message3 = 32;
        // string message4 = "256";
        // vector<uint8_t> message5 = {89, 69, 83};
        // uint16_t one = 1, two = 2;
        // map<uint16_t, string> message6 = {{one, "Ola"}, {two, "Bart"}};
        // serialize(message1, buf);
        // serialize(message2, buf);
        // serialize(message3, buf);
        // serialize(message4, buf);
        // serialize(message5, buf);
        // serialize(message6, buf);
        // print_streambuf(buf);

        // gui_socket.send_to(buf.data(), gui_endpoint);

        // TEST 2
        boost::asio::streambuf buf2;
        const char code = (char) DrawMessage::Lobby;
        buf2.sputn(&code, sizeof(code));
        Lobby lobby = {
            "Hello, world!",
            1,
            10,
            10,
            100,
            2,
            5,
            {{1, {"SmolSir", "127.0.0.1:10022"}}}
        };
        serialize(lobby, buf2);
        print_streambuf(buf2);

        gui_socket.send_to(buf2.data(), gui_endpoint);

        co_spawn(io_context, gui_listener(), detached);
        co_spawn(io_context, server_listener(), detached);

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