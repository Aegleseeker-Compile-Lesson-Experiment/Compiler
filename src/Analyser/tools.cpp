#include "tools.h"

bool isblank(const std::string &s) {
    for (char c : s) {
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            return false;
        }
    }
    return true;
}

std::string trim(const std::string &s) {
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return "";
    }
    size_t last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, (last - first + 1));
}

bool is_terminal_symbols(const std::string &s) {
    return Global::symbols[s] == Symbol::TerminalSymbol;
}

bool is_non_terminal_symbols(const std::string &s) {
    return Global::symbols[s] == Symbol::Non_TerminalSymbol;
}

int getindex(Production production) {
    auto equal_ignoring_dot = [](const Production& left, const Production& right) {
        auto i = left.begin();
        auto j = right.begin();
        while (true) {
            while (i != left.end() && *i == "\\cdot") ++i;
            while (j != right.end() && *j == "\\cdot") ++j;
            if (i == left.end() || j == right.end()) {
                return i == left.end() && j == right.end();
            }
            if (*i != *j) return false;
            ++i;
            ++j;
        }
    };
    for (auto p : Global::productions) {
        if (p.first != production[0]) continue;
        for (auto production_:p.second) {
            if (equal_ignoring_dot(production_.second, Production(production.begin() + std::min<std::size_t>(2, production.size()), production.end()))) {
                return production_.first;
            }
        }
    }
    puts("Get index error!");
    return -1;
}

std::pair<std::string, Production> getproduction(int index) {
    for (const auto &non_terminal_symbol : Global::non_terminal_symbols) {
        for (const auto &production : Global::productions[non_terminal_symbol]) {
            if (production.first == index) {
                return {non_terminal_symbol, production.second};
            }
        }
    }
    if (Global::enable_debug) {
        puts("Get production error!");
    }
    return {};
}

void confirm_epsilon() {
    for (size_t i = 1; i <= Global::productions.size(); ++i) {
        bool finished = true;
        for (auto non_terminal_symbol : Global::non_terminal_symbols) {
            if (Global::can_be_epsilon[non_terminal_symbol] != Epsilon::Uncertained) continue;
            finished = false;
            bool is_no = true;
            for (auto production:Global::productions[non_terminal_symbol]) {
                auto is_epsilon = Epsilon::Yes;
                for (auto symbol:production.second) {
                    if (Global::symbols[symbol] == Symbol::TerminalSymbol) {
                        if (symbol == "\\epsilon") {
                            is_epsilon = Epsilon::Yes;
                        } else {
                            is_epsilon = Epsilon::No;
                        }
                        break;
                    }
                    if (Global::can_be_epsilon[symbol] == Epsilon::No) {
                        is_epsilon = Epsilon::No;
                        break;
                    }
                    if (Global::can_be_epsilon[symbol] == Epsilon::Uncertained) {
                        is_epsilon = Epsilon::Uncertained;
                    }
                }
                if (is_epsilon == Epsilon::Yes) {
                    Global::can_be_epsilon[non_terminal_symbol] = Epsilon::Yes;
                    is_no = false;
                    break;
                }
                if (is_epsilon == Epsilon::Uncertained) {
                    is_no = false;
                }
            }
            if (is_no) {
                Global::can_be_epsilon[non_terminal_symbol] = Epsilon::No;
            }
        }
        if (finished) break;
    }
    for (auto &p : Global::can_be_epsilon) {
        if (p.second == Epsilon::Uncertained) {
            p.second = Epsilon::No;
        }
    }
}

void getfirst() {
    std::unordered_map<std::string, FollowSymbols> out;
    for (const auto& non_terminal_symbol : Global::non_terminal_symbols) {
        for (const auto& production : Global::productions[non_terminal_symbol]) {
            for (const auto& in_symbol : production.second) {
                if (is_terminal_symbols(in_symbol)) {
                    Global::first[non_terminal_symbol].insert(in_symbol);
                    break;
                } else {
                    if (in_symbol != non_terminal_symbol) {
                        out[in_symbol].insert(non_terminal_symbol);
                    }
                    if (Global::can_be_epsilon[in_symbol] == Epsilon::Yes) {
                        continue;
                    } else {
                        break;
                    }
                }
            }
        }
    }
    std::unordered_map<std::string, int> dfn, low;
    std::unordered_map<std::string, bool> in_stack;
    std::vector<std::string> stk;
    int dfn_cnt = 0;
    int scc_cnt = 0;
    std::unordered_map<std::string, int> scc_id;
    std::vector<std::vector<std::string>> scc_nodes;
    auto tarjan = [&](auto& self, const std::string& u) -> void {
        dfn[u] = low[u] = ++dfn_cnt;
        stk.push_back(u);
        in_stack[u] = true;
        if (out.count(u)) {
            for (const auto& v : out[u]) {
                if (!dfn.count(v)) {
                    self(self, v);
                    low[u] = std::min(low[u], low[v]);
                } else if (in_stack[v]) {
                    low[u] = std::min(low[u], dfn[v]);
                }
            }
        }
        if (dfn[u] == low[u]) {
            scc_nodes.emplace_back();
            while (true) {
                std::string v = stk.back();
                stk.pop_back();
                in_stack[v] = false;
                scc_id[v] = scc_cnt;
                scc_nodes[scc_cnt].push_back(v);
                if (u == v) break;
            }
            if (scc_nodes[scc_cnt].size() > 1) {
                Global::is_left_recursion = true;
            }
            scc_cnt++;
        }
    };
    for (const auto& nt : Global::non_terminal_symbols) {
        if (!dfn.count(nt)) {
            tarjan(tarjan, nt);
        }
    }
    std::vector<std::set<std::string>> scc_first(scc_cnt);
    for (int i = 0; i < scc_cnt; ++i) {
        for (const auto& nt : scc_nodes[i]) {
            scc_first[i].insert(Global::first[nt].begin(), Global::first[nt].end());
        }
    }
    std::vector<std::unordered_set<int>> dag_out(scc_cnt);
    std::vector<int> dag_in(scc_cnt, 0);
    for (const auto& item:out) {
        int u_id = scc_id[item.first];
        for (const auto& v:item.second) {
            int v_id = scc_id[v];
            if (u_id != v_id) {
                if (dag_out[u_id].insert(v_id).second) {
                    dag_in[v_id]++;
                }
            }
        }
    }
    std::queue<int> q;
    for (int i = 0; i < scc_cnt; ++i) {
        if (dag_in[i] == 0) {
            q.push(i);
        }
    }
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int v : dag_out[u]) {
            scc_first[v].insert(scc_first[u].begin(), scc_first[u].end());
            dag_in[v]--;
            if (dag_in[v] == 0) {
                q.push(v);
            }
        }
    }
    for (const auto& nt : Global::non_terminal_symbols) {
        int id = scc_id[nt];
        Global::first[nt] = scc_first[id];
    }
    if (Global::enable_debug) {
        puts("FIRST");
        for (auto non_terminal_symbol : Global::non_terminal_symbols) {
            std::cout << non_terminal_symbol << ": ";
            for (auto f:Global::first[non_terminal_symbol]) {
                if (f == "\\epsilon") f = "ε";
                std::cout << f << " ";
            }
            puts("");
        }
        puts("");
    }
}

void getfollow() {
    std::unordered_map<std::string, std::set<std::string>> out;
    Global::follow[Global::start_symbol].insert("$");
    for (const auto& non_terminal_symbol : Global::non_terminal_symbols) {
        for (const auto& production : Global::productions[non_terminal_symbol]) {
            for (size_t i = 0; i < production.second.size(); ++i) {
                auto now = production.second[i];
                if (is_terminal_symbols(now)) {
                    continue;
                }
                bool keep_epsilon = true;
                for (size_t j = i + 1; j < production.second.size(); ++j) {
                    auto symbol = production.second[j];
                    if (is_terminal_symbols(symbol)) {
                        Global::follow[now].insert(symbol);
                        keep_epsilon = false;
                        break;
                    } else {
                        Global::follow[now].insert(Global::first[symbol].begin(), Global::first[symbol].end());
                        Global::follow[now].erase("\\epsilon");
                        if (Global::can_be_epsilon[symbol] == Epsilon::No) {
                            keep_epsilon = false;
                            break;
                        }
                    }
                }
                if (keep_epsilon && non_terminal_symbol != now) {
                    out[non_terminal_symbol].insert(now);
                }
            }
        }
    }
    std::unordered_map<std::string, int> dfn, low;
    std::unordered_map<std::string, bool> in_stack;
    std::vector<std::string> stk;
    int dfn_cnt = 0;
    int scc_cnt = 0;
    std::unordered_map<std::string, int> scc_id;
    std::vector<std::vector<std::string>> scc_nodes;
    auto tarjan = [&](auto& self, const std::string& u) -> void {
        dfn[u] = low[u] = ++dfn_cnt;
        stk.push_back(u);
        in_stack[u] = true;
        if (out.count(u)) {
            for (const auto& v : out[u]) {
                if (!dfn.count(v)) {
                    self(self, v);
                    low[u] = std::min(low[u], low[v]);
                } else if (in_stack[v]) {
                    low[u] = std::min(low[u], dfn[v]);
                }
            }
        }
        if (dfn[u] == low[u]) {
            scc_nodes.emplace_back();
            while (true) {
                std::string v = stk.back();
                stk.pop_back();
                in_stack[v] = false;
                scc_id[v] = scc_cnt;
                scc_nodes[scc_cnt].push_back(v);
                if (u == v) break;
            }
            scc_cnt++;
        }
    };
    for (const auto& nt : Global::non_terminal_symbols) {
        if (!dfn.count(nt)) {
            tarjan(tarjan, nt);
        }
    }
    std::vector<std::set<std::string>> scc_follow(scc_cnt);
    for (int i = 0; i < scc_cnt; ++i) {
        for (const auto& nt : scc_nodes[i]) {
            scc_follow[i].insert(Global::follow[nt].begin(), Global::follow[nt].end());
        }
    }
    std::vector<std::set<int>> dag_out(scc_cnt);
    std::vector<int> dag_in(scc_cnt, 0);
    for (const auto& item : out) {
        int u_id = scc_id[item.first];
        for (const auto& v : item.second) {
            int v_id = scc_id[v];
            if (u_id != v_id) {
                if (dag_out[u_id].insert(v_id).second) {
                    dag_in[v_id]++;
                }
            }
        }
    }
    std::queue<int> q;
    for (int i = 0; i < scc_cnt; ++i) {
        if (dag_in[i] == 0) {
            q.push(i);
        }
    }
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int v : dag_out[u]) {
            scc_follow[v].insert(scc_follow[u].begin(), scc_follow[u].end());
            dag_in[v]--;
            if (dag_in[v] == 0) {
                q.push(v);
            }
        }
    }
    for (const auto& nt : Global::non_terminal_symbols) {
        int id = scc_id[nt];
        Global::follow[nt] = scc_follow[id];
    }
    if (Global::enable_debug) {
        puts("FOLLOW");
        for (const auto &non_terminal_symbol : Global::non_terminal_symbols) {
            std::cout << non_terminal_symbol << ": ";
            for (auto f : Global::follow[non_terminal_symbol]) {
                std::cout << f << " ";
            }
            puts("");
        }
        puts("");
    }
}

void argumentation_grammar() {
    Global::symbols[Global::START] = Symbol::Non_TerminalSymbol;
    Global::can_be_epsilon[Global::START] = Global::can_be_epsilon[Global::start_symbol];
    Global::non_terminal_symbols.push_back(Global::START);
    Global::productions[Global::START].push_back({0, {Global::start_symbol}});
    Global::first[Global::START] = Global::first[Global::start_symbol];
    Global::follow[Global::START] = Global::follow[Global::start_symbol];
}

void solve_left_recursion() {
    std::vector<std::string> new_symbols;
    for (size_t i = 0; i < Global::non_terminal_symbols.size(); ++i) {
        auto non_terminal_symbol = Global::non_terminal_symbols[i];
        auto left_index = std::distance(Global::non_terminal_symbols.begin(), std::find(Global::non_terminal_symbols.begin(), Global::non_terminal_symbols.end(), non_terminal_symbol));
        std::vector<Recursion> happen_recrusion;
        bool allow_recrusion = false;
        for (size_t i = 0; i < Global::productions[non_terminal_symbol].size(); ++i) {
            auto production = Global::productions[non_terminal_symbol][i];
            auto symbol = production.second[0];
            if (is_terminal_symbols(symbol)) {
                happen_recrusion.push_back(Recursion::False);
                continue;
            }
            auto right_index = std::distance(Global::non_terminal_symbols.begin(), std::find(Global::non_terminal_symbols.begin(), Global::non_terminal_symbols.end(), symbol));
            if (right_index < left_index) {
                auto right_symbol = Global::non_terminal_symbols[right_index];
                bool the_first_production = true;
                for (auto right_production:Global::productions[right_symbol]) {
                    Production temp;
                    for (auto s:right_production.second) {
                        temp.push_back(s);
                    }
                    for (size_t j = 1; j < production.second.size(); ++j) {
                        temp.push_back(production.second[j]);
                    }
                    if (the_first_production) {
                        Global::productions[non_terminal_symbol].push_back({right_production.first, temp});
                        happen_recrusion.push_back(Recursion::Replace);
                        the_first_production = false;
                    } else {
                        Global::productions[non_terminal_symbol].push_back({++Global::production_index, temp});
                    }
                }
            }
            if (right_index == left_index) {
                happen_recrusion.push_back(Recursion::True);
                allow_recrusion = true;
            }
            if (right_index > left_index) {
                happen_recrusion.push_back(Recursion::False);
            }
        }
        auto new_symbol = non_terminal_symbol + "-EXTERNAL";
        if (allow_recrusion) {
            new_symbols.push_back(new_symbol);
            std::vector<std::pair<int, Production>> to_change_production;
            for (size_t i = 0; i < happen_recrusion.size(); ++i) {
                if (happen_recrusion[i] == Recursion::True) {
                    auto index = Global::productions[non_terminal_symbol][i].first;
                    auto production = Global::productions[non_terminal_symbol][i].second;
                    Production temp;
                    for (size_t j = 1; j < production.size(); ++j) {
                        temp.push_back(production[j]);
                    }
                    temp.push_back(new_symbol);
                    Global::productions[new_symbol].push_back({index, temp});
                }
                if (happen_recrusion[i] == Recursion::False) {
                    to_change_production.push_back(Global::productions[non_terminal_symbol][i]);
                    if (to_change_production[to_change_production.size() - 1].second.back() == "\\epsilon") {
                        to_change_production[to_change_production.size() - 1].second.pop_back();
                    }
                    to_change_production[to_change_production.size() - 1].second.push_back(new_symbol);
                }
            }
            Global::productions[non_terminal_symbol].clear();
            for (const auto &production:to_change_production) {
                Global::productions[non_terminal_symbol].push_back(production);
            }
            Global::productions[new_symbol].push_back({++Global::production_index, {"\\epsilon"}});
            Global::symbols[new_symbol] = Symbol::Non_TerminalSymbol;
            Global::can_be_epsilon[new_symbol] = Epsilon::Yes;
            Global::non_terminal_symbols.push_back(new_symbol);
        }
    }
    if (Global::enable_debug) {
        puts("After solve left recursion:");
        for (const auto &non_terminal_symbol:Global::non_terminal_symbols) {
            for (const auto &production:Global::productions[non_terminal_symbol]) {
                std::cout << non_terminal_symbol << " → ";
                for (const auto &symbol:production.second) {
                    auto output = symbol;
                    if (output == "\\epsilon") output = "ε";
                    std::cout << output << " ";
                }
                puts("");
            }
        }
        puts("");
    }
}

FollowSymbols get_follow_symbols(ptrdiff_t cdot_position, const Production &production, const std::unordered_set<std::string> &follow_symbols) {
    FollowSymbols new_follow_symbols;
    bool need_follow_symbols = true;
    for (size_t k = cdot_position + 2; k < production.size(); ++k) {
        auto symbol = production[k];
        // 暂时没考虑 \\epsilon
        if (is_terminal_symbols(symbol)) {
            new_follow_symbols.insert(symbol);
            need_follow_symbols = false;
            break;
        }
        if (is_non_terminal_symbols(symbol)) {
            bool has_epsilon = false;
            for (auto first_symbol : Global::first[symbol]) {
                if (first_symbol == "\\epsilon") {
                    has_epsilon = true;
                } else {
                    new_follow_symbols.insert(first_symbol);
                }
            }
            if (!has_epsilon) {
                need_follow_symbols = false;
                break;
            }
        }
    }
    if (need_follow_symbols) {
        for (auto first_symbol : follow_symbols) {
            new_follow_symbols.insert(first_symbol);
        }
    }
    return new_follow_symbols;
}

ASTNode* token_to_astnode(const Token &token) {
    auto *node = new ASTNode(NodeType::RAW_TOKEN, token.lexeme, token.line, token.column);
    if (token.type == TokenType::CONSTANT) {
        node->type = NodeType::CONST_INT;
        node->has_const_value = true;
        node->const_value = token.value;
    } else if (token.type == TokenType::IDENTIFIER) {
        node->type = NodeType::IDENT;
    } else if (token.lexeme == "int" || token.lexeme == "void") {
        node->type = NodeType::TYPE;
    } else {
        node->type = NodeType::RAW_TOKEN;
    }
    return node;
}

void reduce_to_astnode(std::stack<ASTNode*> &sem_stack, std::string lhs, Production rhs) {
    int len = rhs.size();
    std::vector<ASTNode*> children(len);
    for (int i = len - 1; i >= 0; --i) {
        children[i] = sem_stack.top();
        sem_stack.pop();
    }
    ASTNode* result = nullptr;
    if (lhs == "CompUnit") {
        if (len == 2) {
            // CompUnit -> CompUnit FuncDef
            result = children[0];
            result->children.push_back(children[1]);
        } else {
            // CompUnit -> FuncDef
            result = new ASTNode(NodeType::COMP_UNIT, "CompUnit", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
        }
    } else if (lhs == "FuncDef") {
        if (len == 5) {
            // FuncDef -> FuncType ID ( ) Block
            result = new ASTNode(NodeType::FUNC_DEF, "FuncDef", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[1]);
            result->children.push_back(children[4]);
            delete children[2], delete children[3];
        } else {
            // FuncDef -> FuncType ID ( FuncFParams ) Block
            result = new ASTNode(NodeType::FUNC_DEF, "FuncDef", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[1]);
            result->children.push_back(children[3]);
            result->children.push_back(children[5]);
            delete children[2], delete children[4];
        }
    } else if (lhs == "FuncType") {
        if (rhs[0] == "int" || rhs[0] == "void") {
            // FuncType -> int | void
            result = new ASTNode(NodeType::FUNC_TYPE, rhs[0], children[0]->line, children[0]->column);
            result->data_type = rhs[0] == "int" ? DataType::INT : DataType::VOID;
            delete children[0];
        }
    } else if (lhs == "FuncFParams") {
        if (len == 3) {
            // FuncFParams -> FuncFParams , Param
            result = children[0];
            result->children.push_back(children[2]);
            delete children[1];
        } else {
            // FuncFParams -> Param
            result = new ASTNode(NodeType::FUNC_FPARAMS, "FuncFParams", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
        }
    } else if (lhs == "Param") {
        // Param -> int ID
        result = new ASTNode(NodeType::PARAM, "Param", children[0]->line, children[0]->column);
        result->children.push_back(children[1]);
        delete children[0];
    } else if (lhs == "Block") {
        if (len == 2) {
            // Block -> { }
            result = new ASTNode(NodeType::BLOCK, "EmptyBlock", children[0]->line, children[0]->column);
            delete children[0], delete children[1];
        } else {
            // Block -> { StmtList }
            result = new ASTNode(NodeType::BLOCK, "Block", children[0]->line, children[0]->column);
            result->children.push_back(children[1]);
            delete children[0], delete children[2];
        }
    } else if (lhs == "StmtList") {
        if (len == 2) {
            // StmtList -> StmtList Stmt
            result = children[0];
            result->children.push_back(children[1]);
        } else {
            // StmtList -> Stmt
            result = new ASTNode(NodeType::STMT_LIST, "StmtList", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
        }
    } else if (lhs == "Stmt") {
        // Stmt -> MatchedStmt
        // Stmt -> OpenStmt
        result = children[0];
    } else if (lhs == "MatchedStmt") {
        if (len == 7) {
            // MatchedStmt -> if ( Expr ) MatchedStmt else MatchedStmt
            result = new ASTNode(NodeType::IFELSE_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[2]);
            result->children.push_back(children[4]);
            result->children.push_back(children[6]);
            delete children[0], delete children[1], delete children[3], delete children[5];
        } else if (rhs[0] == "while") {
            // MatchedStmt -> while ( Expr ) MatchedStmt
            result = new ASTNode(NodeType::WHILE_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[2]);
            result->children.push_back(children[4]);
            delete children[0], delete children[1], delete children[3];
        } else if (rhs[0] == "ID") {
            // MatchedStmt -> ID = Expr ;
            result = new ASTNode(NodeType::ASSIGN_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[2]);
            delete children[1], delete children[3];
        } else if (len == 5) {
            // MatchedStmt -> int ID = Expr ;
            result = new ASTNode(NodeType::VARDEC_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[1]);
            result->children.push_back(children[3]);
            delete children[0], delete children[2], delete children[4];
        } else if (rhs[0] == "const") {
            // MatchedStmt -> const int ID = Expr ;
            result = new ASTNode(NodeType::VARDEC_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[2]);
            result->children.push_back(children[4]);
            result->has_const_value = true;
            delete children[0], delete children[1], delete children[3], delete children[5];
        } else if (rhs[0] == "int") {
            // MatchedStmt -> int ID ;
            result = new ASTNode(NodeType::VARDEC_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[1]);
            delete children[0], delete children[2];
        } else if (rhs[0] == "return" && len == 2) {
            // MatchedStmt -> return ;
            result = new ASTNode(NodeType::RETURN_STMT, "", children[0]->line, children[0]->column);
            delete children[1];
        } else if (rhs[0] == "return" && len == 3) {
            // MatchedStmt -> return Expr ;
            result = new ASTNode(NodeType::RETURN_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[1]);
            delete children[0], delete children[2];
        } else if (rhs[0] == "break") {
            // MatchedStmt -> break ;
            result = new ASTNode(NodeType::BREAK_STMT, "", children[0]->line, children[0]->column);
            delete children[0], delete children[1];
        } else if (rhs[0] == "continue") {
            // MatchedStmt -> continue ;
            result = new ASTNode(NodeType::CONTINUE_STMT, "", children[0]->line, children[0]->column);
            delete children[0], delete children[1];
        } else if (rhs[0] == "Block") {
            // MatchedStmt -> Block
            result = children[0];
        } else if (rhs[0] == ";") {
            // MatchedStmt -> ;
            result = new ASTNode(NodeType::EMPTY_STMT, ";", children[0]->line, children[0]->column);
            delete children[0];
        } else {
            // MatchedStmt -> Expr ;
            result = children[0];
        }
    } else if (lhs == "OpenStmt") {
        if (len == 7) {
            // OpenStmt -> if ( Expr ) MatchedStmt else OpenStmt
            result = new ASTNode(NodeType::IFELSE_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[2]);
            result->children.push_back(children[4]);
            result->children.push_back(children[6]);
            delete children[0], delete children[1], delete children[3], delete children[5];
        } else if (rhs[0] == "while") {
            // OpenStmt -> while ( Expr ) OpenStmt
            result = new ASTNode(NodeType::WHILE_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[2]);
            result->children.push_back(children[4]);
            delete children[0], delete children[1], delete children[3];
        } else if (rhs[0] == "if") {
            // OpenStmt -> if ( Expr ) Stmt
            result = new ASTNode(NodeType::IF_STMT, "", children[0]->line, children[0]->column);
            result->children.push_back(children[2]);
            result->children.push_back(children[4]);
            delete children[0], delete children[1], delete children[3];
        }
    } else if (lhs == "Expr") {
        // Expr -> LOrExpr
        result = children[0];
    } else if (lhs == "LOrExpr") {
        if (len == 1) {
            // LOrExpr -> LAndExpr
            result = children[0];
        } else {
            // LOrExpr -> LOrExpr || LAndExpr
            result = new ASTNode(NodeType::LOGIC_OP, "||", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[2]);
            delete children[1];
        }
    } else if (lhs == "LAndExpr") {
        if (len == 1) {
            // LAndExpr -> RelExpr
            result = children[0];
        } else {
            // LAndExpr -> LAndExpr && RelExpr
            result = new ASTNode(NodeType::LOGIC_OP, "&&", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[2]);
            delete children[1];
        }
    } else if (lhs == "RelExpr") {
        if (len == 1) {
            // RelExpr -> AddExpr
            result = children[0];
        } else {
            // RelExpr -> RelExpr < AddExpr
            // RelExpr -> RelExpr > AddExpr
            // RelExpr -> RelExpr <= AddExpr
            // RelExpr -> RelExpr >= AddExpr
            // RelExpr -> RelExpr == AddExpr
            // RelExpr -> RelExpr != AddExpr
            result = new ASTNode(NodeType::REL_OP, rhs[1], children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[2]);
            delete children[1];
        }
    } else if (lhs == "AddExpr") {
        if (len == 1) {
            // AddExpr -> MulExpr
            result = children[0];
        } else {
            // AddExpr -> AddExpr + MulExpr
            // AddExpr -> AddExpr - MulExpr
            result = new ASTNode(NodeType::ADD_OP, rhs[1], children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[2]);
            delete children[1];
        }
    } else if (lhs == "MulExpr") {
        if (len == 1) {
            // MulExpr -> UnaryExpr
            result = children[0];
        } else {
            // MulExpr -> MulExpr * UnaryExpr
            // MulExpr -> MulExpr / UnaryExpr
            // MulExpr -> MulExpr % UnaryExpr
            result = new ASTNode(NodeType::MUL_OP, rhs[1], children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[2]);
            delete children[1];
        }
    } else if (lhs == "UnaryExpr") {
        if (len == 1) {
            // UnaryExpr -> PrimaryExpr
            result = children[0];
        } else {
            // UnaryExpr -> + UnaryExpr
            // UnaryExpr -> - UnaryExpr
            // UnaryExpr -> ! UnaryExpr
            result = new ASTNode(NodeType::SINGLE_OP, rhs[0], children[0]->line, children[0]->column);
            result->children.push_back(children[1]);
            delete children[0];
        }
    } else if (lhs == "PrimaryExpr") {
        if (len == 3 && rhs[1] == "Expr") {
            // PrimaryExpr -> ( Expr )
            result = children[1];
            delete children[0], delete children[2];
        } else if (len == 3) {
            // PrimaryExpr -> ID ( )
            result = new ASTNode(NodeType::FUNC_CALL, "FuncCall", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            delete children[1], delete children[2];
        } else if (len == 4) {
            // PrimaryExpr -> ID ( FuncRParams )
            result = new ASTNode(NodeType::FUNC_CALL, "FuncCall", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
            result->children.push_back(children[2]);
            delete children[1], delete children[3];
        } else if (rhs[0] == "ID" || rhs[0] == "NUMBER") {
            // PrimaryExpr -> ID
            // PrimaryExpr -> NUMBER
            result = children[0];
        }
    } else {
        if (len == 1) {
            // FuncRParams -> Expr
            result = new ASTNode(NodeType::FUNC_RPARAMS, "FuncRparams", children[0]->line, children[0]->column);
            result->children.push_back(children[0]);
        }
        if (len == 3) {
            // FuncRParams -> FuncRParams , Expr
            result = children[0];
            result->children.push_back(children[2]);
        }
    }
    sem_stack.push(result);
}
