#include "init.h"

namespace Global {
    std::ifstream pre_file("./third_party/larry");
    const std::string START = "START";
    int production_index = 0;
    bool enable_solve_left_recursion = false;
    bool enable_debug = false;
    bool is_left_recursion = false;
    std::string s = "";
    std::string start_symbol = "";
    std::unordered_map<std::string, Symbol> symbols;
    std::unordered_map<std::string, Epsilon> can_be_epsilon;
    std::vector<std::string> terminal_symbols;
    std::vector<std::string> non_terminal_symbols;
    std::vector<std::vector<std::string>> sentence;
    std::unordered_map<std::string, std::vector<std::pair<int, Production>>> productions;
    std::unordered_map<std::string, std::set<std::string>> first;
    std::unordered_map<std::string, std::set<std::string>> follow;
}

bool parse_terminalsymbol(const std::string &s) {
    const std::string model = "Terminal Symbol";
    if (s != model) return false;
    std::string line, temp;
    std::getline(Global::pre_file, line);
    line = trim(line);
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ' ') {
            if (temp == "") return false;
            Global::symbols[temp] = Symbol::TerminalSymbol;
            Global::terminal_symbols.push_back(temp);
            temp = "";
        } else {
            temp += line[i];
        }
    }
    Global::symbols[temp] = Symbol::TerminalSymbol;
    Global::terminal_symbols.push_back(temp);
    return true;
}

bool parse_non_terminalsymbol(const std::string &s) {
    const std::string model = "Non-Terminal Symbol";
    if (s != model) return false;
    std::string line, temp;
    std::getline(Global::pre_file, line);
    line = trim(line);
    for (size_t i = 0; i < line.size(); ++i) {
        if (line[i] == ' ') {
            if (temp == "") return false;
            Global::symbols[temp] = Symbol::Non_TerminalSymbol;
            Global::non_terminal_symbols.push_back(temp);
            Global::can_be_epsilon[temp] = Epsilon::Uncertained;
            temp = "";
        } else {
            temp += line[i];
        }
    }
    Global::symbols[temp] = Symbol::Non_TerminalSymbol;
    Global::non_terminal_symbols.push_back(temp);
    Global::can_be_epsilon[temp] = Epsilon::Uncertained;
    return true;
}

bool parse_startsymbol(const std::string &s) {
    const std::string model = "Start Symbol";
    if (s != model) return false;
    std::string line;
    std::getline(Global::pre_file, line);
    line = trim(line);
    if (is_terminal_symbols(line)) {
        puts("Start symbol is not non-terminalsymbol.");
        return false;
    }
    Global::start_symbol = line;
    return true;
}

bool parse_productions(const std::string &s) {
    enum class production: int { Left, Arrow, Right };
    const std::string model = "Productions";
    if (s != model) return false;
    production state = production::Left;
    std::string line, temp, now;
    while(std::getline(Global::pre_file, line)) {
        if (isblank(line)) return true;
        line = trim(line);
        std::vector<std::string> production;
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == ' ') {
                if (state == production::Left) {
                    if (Global::symbols[temp] != Symbol::Non_TerminalSymbol) {
                        return false;
                    }
                    state = production::Arrow;
                    now = temp;
                } else {
                    if (state == production::Arrow) {
                        if (temp != "\\to") {
                            return false;
                        }
                        state = production::Right;
                    } else {
                        if (Global::symbols.find(temp) == Global::symbols.end()) {
                            return false;
                        }
                        if (temp == "\\epsilon") {
                            Global::can_be_epsilon[now] = Epsilon::Yes;
                        }
                        production.push_back(temp);
                    }
                }
                temp = "";
            } else {
                temp += line[i];
            }
        }
        production.push_back(temp);
        Global::productions[now].push_back({++Global::production_index, production});
        state = production::Left;
        temp = "";
    }
    return true;
}

bool parse_enablesolveleftrecursion(const std::string &s) {
    const std::string model = "Enable Solve Left Recursion";
    if (s != model) return false;
    std::string line;
    std::getline(Global::pre_file, line);
    line = trim(line);
    if (line == "true") {
        Global::enable_solve_left_recursion = true;
    }
    return true;
}

bool parse_enabledebug(const std::string &s) {
    const std::string model = "Enable Debug";
    if (s != model) return false;
    std::string line;
    std::getline(Global::pre_file, line);
    line = trim(line);
    if (line == "true") {
        Global::enable_debug = true;
    }
    return true;
}

bool parse_sentence(const std::string &s) {
    const std::string model = "Sentence";
    if (s != model) return false;
    std::string line, temp;
    std::vector<std::string> sentence;
    while(std::getline(Global::pre_file, line)) {
        if (isblank(line)) return true;
        line = trim(line);
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == ' ') {
                sentence.push_back(temp);
                temp = "";
            } else {
                temp += line[i];
            }
        }
        sentence.push_back(temp);
        Global::sentence.push_back(sentence);
        sentence.clear();
        temp = "";
    }
    return true;
}

bool initial() {
    State state = State::TerminalSymbol;
    while(std::getline(Global::pre_file, Global::s)) {
        if (isblank(Global::s)) continue;
        if (Global::s.substr(0, 2) == "//") continue;
        Global::s = trim(Global::s);
        switch (state) {
            case State::TerminalSymbol:
                if (!parse_terminalsymbol(Global::s)) {
                    puts("Parse terminal symbol error!");
                    return false;
                }
                state = State::Non_TerminalSymbol;
                break;
            case State::Non_TerminalSymbol:
                if (!parse_non_terminalsymbol(Global::s)) {
                    puts("Parse non-terminal symbol error!");
                    return false;
                }
                state = State::StartSymbol;
                break;
            case State::StartSymbol:
                if (!parse_startsymbol(Global::s)) {
                    puts("Parse start symbol error!");
                    return false;
                }
                state = State::Productions;
                break;
            case State::Productions:
                if (!parse_productions(Global::s)) {
                    puts("Parse productions error!");
                    return false;
                }
                state = State::EnableSolveLeftRecursion;
                break;
            case State::EnableSolveLeftRecursion:
                if (!parse_enablesolveleftrecursion(Global::s)) {
                    puts("Parse enable solve left recursion error!");
                    return false;
                }
                state = State::EnableDebug;
                break;
            case State::EnableDebug:
                if (!parse_enabledebug(Global::s)) {
                    puts("Parse enable debug error!");
                    return false;
                }
                state = State::Sentence;
                break;
            case State::Sentence:
                if (!parse_sentence(Global::s)) {
                    puts("Parse Sentence error!");
                    return false;
                }
                state = State::End;
                break;
            case State::End:
                break;
        }
    }
    return true;
}
