#include <boost/program_options.hpp>

#include <iostream>
#include "structures.hpp"

using namespace std;

namespace po = boost::program_options;

/* -------------------------------------------------------------------------
   Useful structures
   ------------------------------------------------------------------------- */
/* structures for storing data about server */
struct Settings {
    uint16_t bomb_timer;
    uint8_t players_count;
    uint64_t turn_duration;
    uint16_t explosion_radius;
    uint16_t initial_blocks;
    uint16_t game_length;
};

struct Server {

};

/* -------------------------------------------------------------------------
   Global variables for less argument passing
   ------------------------------------------------------------------------- */



/* -------------------------------------------------------------------------
   Parsing & helper functions
   ------------------------------------------------------------------------- */
bool process_command_line(int argc, char **argv) {

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