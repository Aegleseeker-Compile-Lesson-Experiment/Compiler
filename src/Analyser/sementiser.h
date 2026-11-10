#ifndef SEMENTISER_H
#define SEMENTISER_H

#include "init.h"

class Sementiser {
private:
    std::vector<std::pair<int, std::string>> err_msg;
    std::vector<Func> functions;
    Block rootBlock;
public:
    Sementiser() = default;
    ~Sementiser() = default;
    std::vector<std::pair<int, std::string>> check_error();
    void add_error_message(int line, std::string msg);
    TableSymbol* find_in_block(TableSymbol symbol, Block *block);
    TableSymbol* find_in_blocks(TableSymbol symbol, Block *block);
    Func find_in_functions(std::string function);
    void dfs_block(ASTNode *node, Block *block);
    void dfs_stmt_list(ASTNode *node, Block *block);
    void dfs_stmt(ASTNode *node, Block *block, int add_loop_layer);
    void dfs_op(ASTNode *node, Block *block);
    void dfs_func_rparams(ASTNode *node, Block *block);
};

#endif // SEMENTISER_H
