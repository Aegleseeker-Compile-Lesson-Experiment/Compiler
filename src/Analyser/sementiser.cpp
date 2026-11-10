#include "sementiser.h"

std::vector<std::pair<int, std::string>> Sementiser::check_error() {
    auto root = Global::ASTRoot;
    rootBlock = Block{{}, 0, nullptr};
    for (auto &function : root->children) {
        Func temp;
        temp.type = function->children[0]->data_type == DataType::INT ? FuncType::INT : FuncType::VOID;
        temp.name = function->children[1]->text;
        if (temp.name == "main") {
            if (temp.type != FuncType::INT) {
                add_error_message(function->children[0]->line, "Main returns void.");
            }
        }
        rootBlock.SymbolTable.clear();
        if (function->children.size() == 3) {
            functions.push_back(temp);
            dfs_block(function->children[2], &rootBlock);
            if (function->children[0]->data_type == DataType::INT && !function->children[2]->has_return) {
                add_error_message(function->children[2]->line, "Function may return nothing.");
            }
        } else {
            for (const auto &parameter : function->children[2]->children) {
                rootBlock.SymbolTable.push_back(TableSymbol(DataType::INT, parameter->children[0]->text, 0, false, true));
                temp.parameters.push_back(parameter->children[0]->text);
            }
            functions.push_back(temp);
            dfs_block(function->children[3], &rootBlock);
            if (function->children[0]->data_type == DataType::INT && !function->children[3]->has_return) {
                add_error_message(function->children[3]->line, "Function may return nothing.");
            }
        }
    }
    return err_msg;
}

void Sementiser::add_error_message(int line, std::string msg) {
    err_msg.push_back({line, msg});
}

TableSymbol* Sementiser::find_in_block(TableSymbol symbol, Block *block) {
    for (auto &t : block->SymbolTable) {
        if (t.name == symbol.name) return &t;
    }
    return new TableSymbol("");
}

TableSymbol* Sementiser::find_in_blocks(TableSymbol symbol, Block *block) {
    do {
        auto res = find_in_block(symbol, block);
        block = block->fth;
        if (res->name != "") {
            return res;
        }
        delete res;
    } while (block != nullptr);
    return new TableSymbol("");
}

Func Sementiser::find_in_functions(std::string function) {
    for (const auto &func : functions) {
        if (function == func.name) {
            return func;
        }
    }
    return Func{FuncType::NOTFOUND, "", {}};
}

void Sementiser::dfs_block(ASTNode *node, Block *block) {
    dfs_stmt_list(node->children[0], block);
    node->has_return = node->children[0]->has_return;
}

void Sementiser::dfs_stmt_list(ASTNode *node, Block *block) {
    bool has_return = false;
    for (const auto &stmt : node->children) {
        dfs_stmt(stmt, block, 0);
        if (stmt->has_return) has_return = true;
    }
    node->has_return = has_return;
}

void Sementiser::dfs_stmt(ASTNode *node, Block *block, int add_loop_layer) {
    if (node->type == NodeType::BLOCK) {
        auto _ = new Block;
        _->fth = block;
        _->loop_layers = block->loop_layers + add_loop_layer;
        dfs_block(node, _);
        delete _;
    } else if (node->type == NodeType::VARDEC_STMT) {
        TableSymbol temp;
        temp.name = node->children[0]->text;
        temp.type = DataType::UNKNOWN;
        if (node->children.size() > 1) {
            temp.type = DataType::INT;
        }
        temp.is_const = node->has_const_value;
        temp.may_have_been_changed = false;
        auto res = find_in_block(temp, block);
        if (res->name != "") {
            add_error_message(node->line, "Symbol has been used in current scope.");
        } else {
            delete res;
        }
        if (node->children.size() > 1) {
            dfs_op(node->children[1], block);
            temp.initial_value = node->children[1]->const_value;
            temp.may_have_been_changed = !node->has_const_value;
        }
        block->SymbolTable.push_back(temp);
    } else if (node->type == NodeType::ASSIGN_STMT) {
        TableSymbol temp;
        temp.name = node->children[0]->text;
        auto res = find_in_blocks(temp, block);
        if (res->name == "") {
            add_error_message(node->line, "Symbol not declared before.");
            delete res;
        } else {
            if (res->is_const == true) {
                add_error_message(node->children[0]->line, "Const value should not be changed.");
            }
            res->type = DataType::INT;
        }
        dfs_op(node->children[1], block);
        res->may_have_been_changed = !node->children[1]->has_const_value;
        res->initial_value = node->children[1]->const_value;
    } else if (node->type == NodeType::IF_STMT) {
        dfs_op(node->children[0], block);
        dfs_stmt(node->children[1], block, 0);
    } else if (node->type == NodeType::IFELSE_STMT) {
        dfs_op(node->children[0], block);
        dfs_stmt(node->children[1], block, 0);
        dfs_stmt(node->children[2], block, 0);
        node->has_return = node->children[1]->has_return && node->children[2]->has_return;
    } else if (node->type == NodeType::WHILE_STMT) {
        dfs_op(node->children[0], block);
        dfs_stmt(node->children[1], block, 1);
    } else if (node->type == NodeType::RETURN_STMT) {
        if (node->children.empty()) {
            if (functions.back().type != FuncType::VOID) {
                add_error_message(node->line, "Return void but function returns int.");
            }
        } else {
            if (functions.back().type != FuncType::INT) {
                add_error_message(node->line, "Return int but function returns void.");
            }
            dfs_op(node->children[0], block);
        }
        node->has_return = true;
    } else if (node->type == NodeType::BREAK_STMT) {
        if (block->loop_layers == 0) {
            add_error_message(node->line, "Break happens outside of loop.");
        }
    } else if (node->type == NodeType::CONTINUE_STMT) {
        if (block->loop_layers == 0) {
            add_error_message(node->line, "Continue happens outside of loop.");
        }
    } else if (node->type == NodeType::FUNC_CALL) {
        auto func = find_in_functions(node->children[0]->text);
        if (func.type == FuncType::NOTFOUND) {
            add_error_message(node->children[0]->line, "Function not defined.");
        } else if ((!func.parameters.empty() && (node->children.size() == 1 || func.parameters.size() != node->children[1]->children.size())) ||
            (func.parameters.empty() && node->children.size() != 1)) {
            add_error_message(node->children[0]->line, "Number of function parameters not matched.");
        }
        if (!func.parameters.empty() && node->children.size() != 1) {
            dfs_func_rparams(node->children[1], block);
        }
    } else {
        // EMPTY_STMT
        /* Nothing */
    }
}

void Sementiser::dfs_op(ASTNode *node, Block *block) {
    if (node->type == NodeType::FUNC_CALL) {
        auto func = find_in_functions(node->children[0]->text);
        if (func.type == FuncType::NOTFOUND) {
            add_error_message(node->children[0]->line, "Function not defined.");
        }
        if (func.type == FuncType::VOID) {
            add_error_message(node->children[0]->line, "Function returns void but used as value.");
        }
        if ((!func.parameters.empty() && (node->children.size() == 1 || func.parameters.size() != node->children[1]->children.size())) ||
            (func.parameters.empty() && node->children.size() != 1)) {
            add_error_message(node->children[0]->line, "Number of function parameters not matched.");
        }
        if (!func.parameters.empty() && node->children.size() != 1) {
            dfs_func_rparams(node->children[1], block);
        }
        node->has_const_value = false;
    } else if (node->type == NodeType::CONST_INT) {
        /* Nothing */
    } else if (node->type == NodeType::IDENT) {
        TableSymbol temp;
        temp.name = node->text;
        auto res = find_in_blocks(temp, block);
        if (res->name == "") {
            add_error_message(node->line, "Symbol not declared before.");
            delete res;
        }
        if (res->may_have_been_changed) {
            node->has_const_value = false;
        } else {
            node->has_const_value = true;
            node->const_value = res->initial_value;
        }
    } else if (node->type == NodeType::SINGLE_OP) {
        dfs_op(node->children[0], block);
        if (node->children[0]->has_const_value) {
            if (node->text == "+") node->const_value = node->children[0]->const_value;
            if (node->text == "-") node->const_value = -node->children[0]->const_value;
            if (node->text == "!") node->const_value = !node->children[0]->const_value;
        } else {
            node->has_const_value = false;
        }
    } else {
        // LOGIC_OP, REL_OP, ADD_OP, MUL_OP
        dfs_op(node->children[0], block);
        dfs_op(node->children[1], block);
        if (node->children[0]->has_const_value && node->children[1]->has_const_value) {
            int value0 = node->children[0]->const_value;
            int value1 = node->children[1]->const_value;
            node->has_const_value = true;
            if (node->text == "||") node->const_value = value0 || value1;
            if (node->text == "&&") node->const_value = value0 && value1;
            if (node->text == "<") node->const_value = value0 < value1;
            if (node->text == ">") node->const_value = value0 > value1;
            if (node->text == "<=") node->const_value = value0 <= value1;
            if (node->text == ">=") node->const_value = value0 >= value1;
            if (node->text == "==") node->const_value = value0 == value1;
            if (node->text == "!=") node->const_value = value0 != value1;
            if (node->text == "+") node->const_value = value0 + value1;
            if (node->text == "-") node->const_value = value0 - value1;
            if (node->text == "*") node->const_value = value0 * value1;
            if (node->text == "/") {
                if (value1 == 0) {
                    add_error_message(node->children[1]->line, "0 is appeared as division number.");
                    node->has_const_value = false;
                } else {
                    node->const_value = value0 / value1;
                }
            }
            if (node->text == "%") {
                if (value1 == 0) {
                    add_error_message(node->children[1]->line, "0 is appeared as division number.");
                    node->has_const_value = false;
                } else {
                    node->const_value = value0 % value1;
                }
            }
        } else {
            node->has_const_value = false;
        }   
    }
}

void Sementiser::dfs_func_rparams(ASTNode *node, Block *block) {
    for (const auto &parameter : node->children) {
        dfs_op(parameter, block);
    }
}
