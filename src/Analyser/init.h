#ifndef INIT_H
#define INIT_H

#define Production std::vector<std::string>
#define Action std::pair<SLRAction, int>
#define FollowSymbols std::unordered_set<std::string>
#define Entry std::pair<std::string, std::string>
#define ClosureEntry std::pair<int, std::string>
#define ANSI_GREEN  "\033[32m"
#define ANSI_YELLOW "\033[33m"
#define ANSI_CYAN   "\033[36m"
#define ANSI_RESET  "\033[0m"

#include "../Tokenizer/tokernizer.h"
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

enum class State: int { TerminalSymbol, Non_TerminalSymbol, StartSymbol, Productions, EnableSolveLeftRecursion, EnableDebug, End };
enum class Symbol: int { TerminalSymbol, Non_TerminalSymbol };
enum class Epsilon: int {Yes, No, Uncertained};
enum class SLRAction: int {S, R, Accept};
enum class Recursion: int {True, False, Replace};
enum class DataType { INT, VOID, UNKNOWN };
enum class FuncType { INT, VOID, NOTFOUND };
enum class NodeType {
    COMP_UNIT, FUNC_DEF, FUNC_TYPE, FUNC_FPARAMS, PARAM, BLOCK, STMT_LIST,
    IF_STMT, IFELSE_STMT, WHILE_STMT, ASSIGN_STMT, VARDEC_STMT, EMPTY_STMT, RETURN_STMT, BREAK_STMT, CONTINUE_STMT,
    LOGIC_OP, REL_OP, ADD_OP, MUL_OP, SINGLE_OP,
    FUNC_CALL, FUNC_RPARAMS,
    CONST_INT, IDENT,
    TYPE, RAW_TOKEN
};

struct ASTNode {
    NodeType type;
    std::string text;
    DataType data_type = DataType::UNKNOWN;
    bool has_const_value = false;
    bool has_return = false;
    int const_value = 0;
    int line = 0;
    int column = 0;
    std::vector<ASTNode*> children;
    ASTNode(NodeType t, std::string text = "", int line = 0, int column = 0): type(t), text(text), line(line), column(column) {}
};

struct Func {
    FuncType type;
    std::string name;
    std::vector<std::string> parameters;
};

struct TableSymbol {
    DataType type;
    std::string name;
    std::string ptr_name;
    int initial_value;
    bool is_const;
    bool may_have_been_changed; // 不能因被判定为 0 触发除零错误, 如果初值未确定, 也为 true
    TableSymbol() {}
    TableSymbol(std::string name): name(name) {}
    TableSymbol(DataType type, std::string name, std::string ptr_name, int initial_value, bool is_const, bool may_have_been_changed): 
                type(type), name(name), ptr_name(ptr_name), initial_value(initial_value), is_const(is_const), may_have_been_changed(may_have_been_changed) {}
};

struct Block {
    std::vector<TableSymbol> SymbolTable;
    int loop_layers;
    Block *fth;
};

namespace Global {
    extern std::ifstream pre_file;
    extern const std::string START;
    extern int production_index;
    extern bool enable_solve_left_recursion;
    extern bool enable_debug;
    extern bool is_left_recursion;
    extern std::string s;
    extern std::string start_symbol;
    extern std::unordered_map<std::string, Symbol> symbols;
    extern std::unordered_map<std::string, Epsilon> can_be_epsilon;
    extern std::vector<std::string> terminal_symbols;
    extern std::vector<std::string> non_terminal_symbols;
    extern std::vector<std::vector<Token>> sentence;
    extern std::unordered_map<std::string, std::vector<std::pair<int, Production>>> productions;
    extern std::unordered_map<std::string, std::set<std::string>> first;
    extern std::unordered_map<std::string, std::set<std::string>> follow;
    extern ASTNode* ASTRoot;
}

bool parse_terminalsymbol(const std::string &s);
bool parse_non_terminalsymbol(const std::string &s);
bool parse_startsymbol(const std::string &s);
bool parse_productions(const std::string &s);
bool parse_enablesolveleftrecursion(const std::string &s);
bool parse_enabledebug(const std::string &s);
bool initial();

#endif // INIT_H
