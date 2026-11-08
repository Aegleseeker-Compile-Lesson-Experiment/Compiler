#ifndef INIT_H
#define INIT_H

#define Production std::vector<std::string>
#define Action std::pair<SLRAction, int>
#define FollowSymbols std::unordered_set<std::string>
#define Entry std::pair<std::string, std::string>
#define ClosureEntry std::pair<int, std::string>

#include "tools.h"
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <map>
#include <stack>
#include <set>

enum class State: int { TerminalSymbol, Non_TerminalSymbol, StartSymbol, Productions, EnableSolveLeftRecursion, Sentence, End };
enum class Symbol: int { TerminalSymbol, Non_TerminalSymbol };
enum class Epsilon: int {Yes, No, Uncertained};
enum class SLRAction: int {S, R, Accept};
enum class Recursion: int {True, False, Replace};

namespace Global {
    extern std::ifstream pre_file;
    extern const std::string START;
    extern int production_index;
    extern bool enable_solve_left_recursion;
    extern bool is_left_recursion;
    extern std::string s;
    extern std::string start_symbol;
    extern std::unordered_map<std::string, Symbol> symbols;
    extern std::unordered_map<std::string, Epsilon> can_be_epsilon;
    extern std::vector<std::string> terminal_symbols;
    extern std::vector<std::string> non_terminal_symbols;
    extern std::vector<std::vector<std::string>> sentence;
    extern std::unordered_map<std::string, std::vector<std::pair<int, Production>>> productions;
    extern std::unordered_map<std::string, std::set<std::string>> first;
    extern std::unordered_map<std::string, std::set<std::string>> follow;
}

bool parse_terminalsymbol(const std::string &s);
bool parse_non_terminalsymbol(const std::string &s);
bool parse_startsymbol(const std::string &s);
bool parse_productions(const std::string &s);
bool parse_enablesolveleftrecursion(const std::string &s);
bool parse_sentence(const std::string &s);
bool initial();

#endif // INIT_H
