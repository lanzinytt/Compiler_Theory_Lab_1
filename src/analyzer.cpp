#include "analyzer.h"

#include <cctype>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace mini {
namespace {

struct Token {
    std::string text;
    int code = 0;
    int line = 1;
};

struct Symbol {
    std::string name;
    std::string procedure;
    int kind = 0;
    std::string type = "integer";
    int level = 0;
    int address = 0;
};

struct Procedure {
    std::string name;
    std::string type = "integer";
    int level = 0;
    int firstAddress = 0;
    int lastAddress = -1;
};

const std::map<std::string, int> tokenCodes = {
    {"begin", 1}, {"end", 2}, {"integer", 3}, {"if", 4},
    {"then", 5}, {"else", 6}, {"function", 7}, {"read", 8},
    {"write", 9}, {"=", 12}, {"<>", 13}, {"<=", 14},
    {"<", 15}, {">=", 16}, {">", 17}, {"-", 18}, {"*", 19},
    {":=", 20}, {"(", 21}, {")", 22}, {";", 23},
};

bool isIdentifierStart(char value) {
    return std::isalpha(static_cast<unsigned char>(value)) != 0;
}

bool isIdentifierPart(char value) {
    return std::isalnum(static_cast<unsigned char>(value)) != 0;
}

std::string baseName(const std::filesystem::path& path) {
    return path.stem().string();
}

std::vector<Token> readDyd(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open dyd file: " + path.string());
    }

    std::vector<Token> tokens;
    std::string text;
    int code = 0;
    int line = 1;
    while (input >> text >> code) {
        if (text == "EOLN") {
            ++line;
            continue;
        }
        if (text == "EOF") {
            break;
        }
        tokens.push_back({text, code, line});
    }
    tokens.push_back({"EOF", 25, line});
    return tokens;
}

class Lexer {
public:
    explicit Lexer(const std::filesystem::path& inputPath) : inputPath_(inputPath) {}

    int run(const std::filesystem::path& outputDirectory) {
        std::ifstream input(inputPath_);
        if (!input) {
            throw std::runtime_error("cannot open source file: " + inputPath_.string());
        }
        std::ofstream dyd(outputDirectory / (baseName(inputPath_) + ".dyd"));
        std::ofstream errors(outputDirectory / (baseName(inputPath_) + ".lex.err"));
        if (!dyd || !errors) {
            throw std::runtime_error("cannot create lexical output files");
        }

        std::string line;
        int lineNumber = 0;
        int errorCount = 0;
        while (std::getline(input, line)) {
            ++lineNumber;
            scanLine(line, lineNumber, dyd, errors, errorCount);
            dyd << "EOLN 24\n";
        }
        dyd << "EOF 25\n";
        return errorCount == 0 ? 0 : 2;
    }

private:
    void scanLine(const std::string& line, int lineNumber, std::ofstream& dyd,
                  std::ofstream& errors, int& errorCount) const {
        std::size_t index = 0;
        while (index < line.size()) {
            if (std::isspace(static_cast<unsigned char>(line[index]))) {
                ++index;
                continue;
            }

            const std::size_t start = index;
            if (isIdentifierStart(line[index])) {
                ++index;
                while (index < line.size() && isIdentifierPart(line[index])) {
                    ++index;
                }
                emit(line.substr(start, index - start), dyd);
                continue;
            }
            if (std::isdigit(static_cast<unsigned char>(line[index]))) {
                ++index;
                while (index < line.size() && std::isdigit(static_cast<unsigned char>(line[index]))) {
                    ++index;
                }
                dyd << line.substr(start, index - start) << " 11\n";
                continue;
            }

            std::string candidate(1, line[index]);
            if (index + 1 < line.size()) {
                const std::string two = line.substr(index, 2);
                if (tokenCodes.find(two) != tokenCodes.end()) {
                    candidate = two;
                }
            }
            if (tokenCodes.find(candidate) != tokenCodes.end()) {
                dyd << candidate << ' ' << tokenCodes.at(candidate) << '\n';
                index += candidate.size();
                continue;
            }

            errors << "***LINE:" << lineNumber << " Illegal word '" << line[index] << "'\n";
            ++errorCount;
            ++index;
        }
    }

    static void emit(const std::string& word, std::ofstream& dyd) {
        const auto keyword = tokenCodes.find(word);
        if (keyword != tokenCodes.end()) {
            dyd << word << ' ' << keyword->second << '\n';
        } else {
            dyd << word << " 10\n";
        }
    }

    std::filesystem::path inputPath_;
};

class Parser {
public:
    Parser(std::vector<Token> tokens, const std::filesystem::path& outputDirectory,
           const std::string& name)
        : tokens_(std::move(tokens)), outputDirectory_(outputDirectory), name_(name) {}

    int run() {
        errors_.open(outputDirectory_ / (name_ + ".parse.err"));
        parseProgram();
        if (!at("EOF")) {
            report(current(), "unexpected token after program");
        }
        writeTables();
        return errorsFound_ == 0 ? 0 : 3;
    }

private:
    const Token& current() const { return tokens_[position_]; }

    bool at(std::string_view value) const { return current().text == value; }

    bool accept(std::string_view value) {
        if (!at(value)) {
            return false;
        }
        ++position_;
        return true;
    }

    void expect(std::string_view value) {
        if (!accept(value)) {
            report(current(), "expected '" + std::string(value) + "'");
        }
    }

    void report(const Token& token, const std::string& message) {
        errors_ << "***LINE:" << token.line << ' ' << message << "\n";
        ++errorsFound_;
    }

    void parseProgram() {
        expect("begin");
        currentProcedure_ = "main";
        procedures_.push_back({"main", "integer", 0, 0, -1});
        parseDeclarations();
        parseStatements();
        expect("end");
        procedures_.front().lastAddress = static_cast<int>(symbols_.size()) - 1;
    }

    void parseDeclarations() {
        while (at("integer")) {
            accept("integer");
            if (accept("function")) {
                parseFunction();
            } else {
                declareVariable(false);
            }
            if (at(";")) {
                accept(";");
            } else if (!at("begin") && !at("read") && !at("write") && !at("if") && !isIdentifier()) {
                report(current(), "missing ';' after declaration");
                recoverTo(";");
                accept(";");
            }
        }
    }

    void parseFunction() {
        const std::string name = consumeIdentifier("function name");
        const int level = 1;
        procedures_.push_back({name, "integer", level, static_cast<int>(symbols_.size()), -1});
        const std::string previousProcedure = currentProcedure_;
        const int previousLevel = currentLevel_;
        currentProcedure_ = name;
        currentLevel_ = level;
        symbols_.push_back({name, name, 0, "integer", level, static_cast<int>(symbols_.size())});
        expect("(");
        if (isIdentifier()) {
            declareVariable(true);
        } else {
            report(current(), "expected function parameter");
        }
        expect(")");
        expect(";");
        expect("begin");
        parseDeclarations();
        parseStatements();
        expect("end");
        procedures_.back().lastAddress = static_cast<int>(symbols_.size()) - 1;
        currentProcedure_ = previousProcedure;
        currentLevel_ = previousLevel;
    }

    void parseStatements() {
        while (!at("end") && !at("else") && !at("EOF")) {
            parseStatement();
            if (accept(";")) {
                continue;
            }
            if (!at("end") && !at("else")) {
                report(current(), "missing ';' between statements");
                recoverTo(";");
                accept(";");
            }
        }
    }

    void parseStatement() {
        if (accept("read")) {
            expect("(");
            consumeIdentifier("read variable");
            expect(")");
        } else if (accept("write")) {
            expect("(");
            consumeIdentifier("write variable");
            expect(")");
        } else if (accept("if")) {
            parseExpression();
            if (at("<=") || at("<") || at(">=") || at(">") || at("=") || at("<>")) {
                ++position_;
            } else {
                report(current(), "expected relational operator");
            }
            parseExpression();
            expect("then");
            parseStatement();
            accept(";");
            expect("else");
            parseStatement();
        } else if (isIdentifier()) {
            consumeIdentifier("assignment variable");
            expect(":=");
            parseExpression();
        } else {
            report(current(), "unexpected token in statement");
            if (!at("EOF")) {
                ++position_;
            }
        }
    }

    void parseExpression() {
        parseTerm();
        while (accept("-")) {
            parseTerm();
        }
    }

    void parseTerm() {
        parseFactor();
        while (accept("*")) {
            parseFactor();
        }
    }

    void parseFactor() {
        if (isIdentifier()) {
            const std::string name = current().text;
            ++position_;
            if (accept("(")) {
                parseExpression();
                expect(")");
            }
            (void)name;
        } else if (current().code == 11) {
            ++position_;
        } else {
            report(current(), "expected identifier or constant");
            if (!at("EOF")) {
                ++position_;
            }
        }
    }

    bool isIdentifier() const { return current().code == 10; }

    std::string consumeIdentifier(const std::string& context) {
        if (!isIdentifier()) {
            report(current(), "expected identifier for " + context);
            return "";
        }
        const std::string value = current().text;
        ++position_;
        if (context == "function name" || context == "read variable" || context == "write variable" ||
            context == "assignment variable") {
            if (context == "function name") {
                return value;
            }
        }
        if (context == "read variable" || context == "write variable" || context == "assignment variable") {
            ensureSymbol(value, context == "assignment variable" ? 0 : 0);
        }
        return value;
    }

    void declareVariable(bool parameter) {
        const std::string name = consumeIdentifier(parameter ? "parameter" : "variable");
        if (name.empty()) {
            return;
        }
        const int address = static_cast<int>(symbols_.size());
        symbols_.push_back({name, currentProcedure_, parameter ? 1 : 0, "integer", currentLevel_, address});
    }

    void ensureSymbol(const std::string& name, int kind) {
        for (const auto& symbol : symbols_) {
            if (symbol.name == name) {
                return;
            }
        }
        report(current(), "undeclared identifier '" + name + "'");
        (void)kind;
    }

    void recoverTo(std::string_view value) {
        while (!at(value) && !at("EOF") && !at("end")) {
            ++position_;
        }
    }

    void writeTables() {
        std::ofstream var(outputDirectory_ / (name_ + ".var"));
        std::ofstream pro(outputDirectory_ / (name_ + ".pro"));
        if (!var || !pro) {
            throw std::runtime_error("cannot create syntax output files");
        }
        for (const auto& symbol : symbols_) {
            var << symbol.name << ' ' << symbol.procedure << ' ' << symbol.kind << ' '
                << symbol.type << ' ' << symbol.level << ' ' << symbol.address << '\n';
        }
        for (const auto& procedure : procedures_) {
            pro << procedure.name << ' ' << procedure.type << ' ' << procedure.level << ' '
                << procedure.firstAddress << ' ' << procedure.lastAddress << '\n';
        }
    }

    std::vector<Token> tokens_;
    std::filesystem::path outputDirectory_;
    std::string name_;
    std::ofstream errors_;
    std::size_t position_ = 0;
    int errorsFound_ = 0;
    int currentLevel_ = 0;
    std::string currentProcedure_ = "main";
    std::vector<Symbol> symbols_;
    std::vector<Procedure> procedures_;
};

} // namespace

Analyzer::Analyzer(RunOptions options) : options_(std::move(options)) {}

int Analyzer::run() {
    std::filesystem::create_directories(options_.outputDirectory);
    const std::string name = baseName(options_.input);
    const auto dydPath = options_.outputDirectory / (name + ".dyd");

    int result = 0;
    if (options_.mode == RunMode::Lex || options_.mode == RunMode::All) {
        result = Lexer(options_.input).run(options_.outputDirectory);
        if (options_.mode == RunMode::Lex) {
            return result;
        }
    }
    if (options_.mode == RunMode::Parse || options_.mode == RunMode::All) {
        result = Parser(readDyd(options_.mode == RunMode::Parse ? options_.input : dydPath),
                        options_.outputDirectory,
                        options_.mode == RunMode::Parse ? name : name).run();
    }
    return result;
}

std::string usage(const std::string& programName) {
    std::ostringstream output;
    output << "Usage: " << programName << " <lex|parse|all> <input-file> [output-directory]\n"
           << "  lex   tokenize a MINI source file and create .dyd/.lex.err\n"
           << "  parse parse a .dyd file and create .var/.pro/.parse.err\n"
           << "  all   run lex followed by parse\n";
    return output.str();
}

} // namespace mini
