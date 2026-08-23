#include "smac/formats/data_directory.hpp"

#include <iostream>
int main(int argc, char** argv) {
    if (argc == 3 && std::string(argv[1]) == "--data-dir") {
        auto r = smac::formats::validate_data_directory(argv[2]);
        if (!r.valid()) {
            std::cerr
                << "The data directory is incomplete; run smac-tool verify-data for details.\n";
            return 1;
        }
    }
    std::cerr << "This build has no SDL3 client. Install SDL3 3.4.14, SDL3_image 3.4.4, and "
                 "SDL3_ttf 3.2.2, or configure with -DSMAC_FETCH_DEPENDENCIES=ON.\n";
    return 1;
}
