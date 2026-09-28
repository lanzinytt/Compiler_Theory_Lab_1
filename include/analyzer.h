#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

namespace mini {

enum class RunMode { Lex, Parse, All };

struct RunOptions {
    RunMode mode = RunMode::All;
    std::filesystem::path input;
    std::filesystem::path outputDirectory;
};

inline RunMode parseMode(const std::string& value) {
    if (value == "lex") {
        return RunMode::Lex;
    }
    if (value == "parse") {
        return RunMode::Parse;
    }
    if (value == "all") {
        return RunMode::All;
    }
    throw std::invalid_argument("mode must be lex, parse, or all");
}

inline RunOptions parseOptions(int argc, char* argv[]) {
    if (argc < 3 || argc > 4) {
        throw std::invalid_argument("expected: <lex|parse|all> <input-file> [output-directory]");
    }

    RunOptions options;
    options.mode = parseMode(argv[1]);
    options.input = argv[2];
    options.outputDirectory = argc == 4 ? argv[3] : options.input.parent_path();
    return options;
}

class Analyzer {
public:
    explicit Analyzer(RunOptions options);
    int run();

private:
    RunOptions options_;
};

std::string usage(const std::string& programName);

} // namespace mini
