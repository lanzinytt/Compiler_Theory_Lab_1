#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mini {

enum class RunMode { Lex, Parse, All };

struct RunOptions {
    RunMode mode = RunMode::All;
    std::filesystem::path input;
    std::filesystem::path outputDirectory;
};

class Analyzer {
public:
    explicit Analyzer(RunOptions options);
    int run();

private:
    RunOptions options_;
};

std::string usage(const std::string& programName);

} // namespace mini
