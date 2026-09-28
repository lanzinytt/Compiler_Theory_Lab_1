#include "analyzer.h"

#include <iostream>
#include <stdexcept>

int main(int argc, char* argv[]) {
    try {
        const mini::RunOptions options = mini::parseOptions(argc, argv);
        return mini::Analyzer(std::move(options)).run();
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        std::cerr << mini::usage(argv[0]);
        return 1;
    }
}
