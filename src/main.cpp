#include "analyzer.h"

#include <iostream>
#include <stdexcept>

namespace {

mini::RunMode parseMode(const std::string& value) {
    if (value == "lex") {
        return mini::RunMode::Lex;
    }
    if (value == "parse") {
        return mini::RunMode::Parse;
    }
    if (value == "all") {
        return mini::RunMode::All;
    }
    throw std::invalid_argument("mode must be lex, parse, or all");
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        std::cerr << mini::usage(argv[0]);
        return 1;
    }

    try {
        mini::RunOptions options;
        options.mode = parseMode(argv[1]);
        options.input = argv[2];
        options.outputDirectory = argc == 4 ? argv[3] : options.input.parent_path();
        return mini::Analyzer(std::move(options)).run();
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}
