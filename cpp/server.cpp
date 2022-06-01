#include <boost/program_options.hpp>

#include <iostream>
#include <random>
#include "structures.hpp"

using namespace std;

namespace po = boost::program_options;

/* -------------------------------------------------------------------------
   Useful structures
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
    uint32_t seed = (uint32_t) chrono::system_clock::now().time_since_epoch().count();
    uint16_t size_x;
    uint16_t size_y;
};

/* structure for storing runtime data and settings */
struct Server {
    Settings settings;
};

/* -------------------------------------------------------------------------
   Global variables for less argument passing
   ------------------------------------------------------------------------- */
po::variables_map program_params;

Server server;

/* -------------------------------------------------------------------------
   Parsing & helper functions
   ------------------------------------------------------------------------- */
template <Unsigned T>
T random(T mod) {
    minstd_rand random(server.settings.seed);
    return random() % mod;
}

bool process_command_line(int argc, char **argv) {
    try {
        po::options_description desc("Allowed options");
        desc.add_options()
            ("bomb-timer,b", po::value<uint16_t>(&server.settings.bomb_timer)->required())
            ("players-count,c", po::value<uint8_t>(&server.settings.players_count)->required())
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



    return 0;
}