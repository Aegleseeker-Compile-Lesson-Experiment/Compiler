#include "sementiser.h"

std::vector<std::pair<int, std::string>> Sementiser::check_error() {
    auto root = Global::ASTRoot;
    rootBlock = Block{{}, 0, nullptr};
    functions.push_back(Func{FuncType::INT, "getint", {}});
    functions.push_back(Func{FuncType::VOID, "putint", {"a"}});
    std::ostringstream module_ir;
    // module_ir << "declare i32 @getint()\n";
    // module_ir << "declare void @putint(i32)\n\n";
    for (auto &function : root->children) {
        module_ir << dfs_func(function, &rootBlock);
    }
    std::ofstream ir_file("output.ll");
    ir_file << module_ir.str();
    ir_file.close();
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
    if (ir.block_terminated) return;
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
        auto ptr = "%" + node->children[0]->text + ".addr_" + std::to_string(ir.reg_id++);
        temp.ptr_name = ptr;
        ir.entry_allocas << "  " << ptr << " = alloca i32\n";
        if (res->name != "") {
            add_error_message(node->line, "Symbol has been used in current scope.");
        } else {
            delete res;
        }
        if (node->children.size() > 1) {
            auto init_val = dfs_op(node->children[1], block);
            ir.emit("store i32 " + init_val + ", ptr " + ptr);
            temp.initial_value = node->children[1]->const_value;
            temp.may_have_been_changed = !node->children[1]->has_const_value;
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
        auto val = dfs_op(node->children[1], block);
        res->may_have_been_changed = !node->children[1]->has_const_value;
        res->initial_value = node->children[1]->const_value;
        ir.emit("store i32 " + val + ", ptr " + res->ptr_name);
    } else if (node->type == NodeType::IF_STMT) {
        auto then_label = ir.new_label();
        auto end_label = ir.new_label();
        auto cond_val = dfs_op(node->children[0], block);
        auto cond_i1 = ir.new_reg();
        ir.emit(cond_i1 + " = icmp ne i32 " + cond_val + ", 0");
        ir.emit("br i1 " + cond_i1 + ", label %" + then_label + ", label %" + end_label);
        ir.block_terminated = true;
        ir.emit_label(then_label);
        dfs_stmt(node->children[1], block, 0);
        if (!ir.block_terminated) {
            ir.emit("br label %" + end_label);
            ir.block_terminated = true;
        }
        ir.emit_label(end_label);
    } else if (node->type == NodeType::IFELSE_STMT) {
        auto then_label = ir.new_label();
        auto else_label = ir.new_label();
        auto end_label = ir.new_label();
        auto cond_val = dfs_op(node->children[0], block);
        auto cond_i1 = ir.new_reg();
        ir.emit(cond_i1 + " = icmp ne i32 " + cond_val + ", 0");
        ir.emit("br i1 " + cond_i1 + ", label %" + then_label + ", label %" + else_label);
        ir.block_terminated = true;
        ir.emit_label(then_label);
        dfs_stmt(node->children[1], block, 0);
        if (!ir.block_terminated) {
            ir.emit("br label %" + end_label);
            ir.block_terminated = true;
        }
        ir.emit_label(else_label);
        dfs_stmt(node->children[2], block, 0);
        if (!ir.block_terminated) {
            ir.emit("br label %" + end_label);
            ir.block_terminated = true;
        }
        ir.emit_label(end_label);
        node->has_return = node->children[1]->has_return && node->children[2]->has_return;
    } else if (node->type == NodeType::WHILE_STMT) {
        auto cond_label = ir.new_label();
        auto body_label = ir.new_label();
        auto end_label = ir.new_label();
        ir.emit("br label %" + cond_label);
        ir.block_terminated = true;
        ir.emit_label(cond_label);
        auto cond_val = dfs_op(node->children[0], block);
        auto cond_i1 = ir.new_reg();
        ir.emit(cond_i1 + " = icmp ne i32 " + cond_val + ", 0");
        ir.emit("br i1 " + cond_i1 + ", label %" + body_label + ", label %" + end_label);
        ir.block_terminated = true;
        ir.loop_stack.push_back({cond_label, end_label});
        ir.emit_label(body_label);
        dfs_stmt(node->children[1], block, 1);
        if (!ir.block_terminated) {
            ir.emit("br label %" + cond_label);
            ir.block_terminated = true;
        }
        ir.loop_stack.pop_back();
        ir.emit_label(end_label);
    } else if (node->type == NodeType::RETURN_STMT) {
        if (node->children.empty()) {
            if (functions.back().type != FuncType::VOID) {
                add_error_message(node->line, "Return void but function returns int.");
                ir.emit("ret void");
            }
        } else {
            if (functions.back().type != FuncType::INT) {
                add_error_message(node->line, "Return int but function returns void.");
            }
            auto ret_val = dfs_op(node->children[0], block);
            ir.emit("ret i32 " + ret_val);
        }
        ir.block_terminated = true;
        node->has_return = true;
    } else if (node->type == NodeType::BREAK_STMT) {
        if (block->loop_layers == 0) {
            add_error_message(node->line, "Break happens outside of loop.");
        }
        ir.emit("br label %" + ir.loop_stack.back().second);
        ir.block_terminated = true;
    } else if (node->type == NodeType::CONTINUE_STMT) {
        if (block->loop_layers == 0) {
            add_error_message(node->line, "Continue happens outside of loop.");
        }
        ir.emit("br label %" + ir.loop_stack.back().first);
        ir.block_terminated = true;
    } else if (node->type == NodeType::FUNC_CALL) {
        auto func = find_in_functions(node->children[0]->text);
        if (func.type == FuncType::NOTFOUND) {
            add_error_message(node->children[0]->line, "Function not defined.");
        } else if ((!func.parameters.empty() && (node->children.size() == 1 || func.parameters.size() != node->children[1]->children.size())) ||
            (func.parameters.empty() && node->children.size() != 1)) {
            add_error_message(node->children[0]->line, "Number of function parameters not matched.");
        }
        std::vector<std::string> args;
        if (!func.parameters.empty() && node->children.size() != 1) {
            args = dfs_func_rparams(node->children[1], block);
        }
        std::string arg_str;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) arg_str += ", ";
            arg_str += args[i];
        }
        if (func.type == FuncType::VOID) {
            ir.emit("call void @" + func.name + "(" + arg_str + ")");
        } else {
            auto reg = ir.new_reg();
            ir.emit(reg + " = call i32 @" + func.name + "(" + arg_str + ")");
        }
    } else {
        // EMPTY_STMT
        /* Nothing */
    }
}

std::string Sementiser::dfs_func(ASTNode *node, Block *block) {
    ir.entry_allocas.str("");
    ir.entry_allocas.clear();
    ir.ir_body.str("");
    ir.ir_body.clear();
    ir.reg_id = 0;
    ir.label_id = 0;
    ir.block_terminated = false;
    Func temp;
    temp.type = node->children[0]->data_type == DataType::INT ? FuncType::INT : FuncType::VOID;
    temp.name = node->children[1]->text;
    if (temp.name == "main") {
        if (temp.type != FuncType::INT) {
            add_error_message(node->children[0]->line, "Main returns void.");
        }
    }
    block->SymbolTable.clear();
    std::string param_sig;
    if (node->children.size() == 3) {
        functions.push_back(temp);
        dfs_block(node->children[2], block);
        if (node->children[0]->data_type == DataType::INT && !node->children[2]->has_return) {
            add_error_message(node->children[2]->line, "Function may return nothing.");
        }
    } else {
        bool first_p = true;
        for (const auto &parameter : node->children[2]->children) {
            auto p_name = parameter->children[0]->text;
            if (first_p) {
                first_p = false;
            } else {
                param_sig += ", ";
            }
            param_sig += "i32 %" + p_name;
            auto ptr = "%" + p_name + ".addr_" + std::to_string(ir.reg_id++);
            ir.entry_allocas << "  " << ptr << " = alloca i32\n";
            ir.emit("store i32 %" + p_name + ", ptr " + ptr);
            rootBlock.SymbolTable.push_back(TableSymbol(DataType::INT, parameter->children[0]->text, ptr, 0, false, true));
            temp.parameters.push_back(parameter->children[0]->text);
        }
        functions.push_back(temp);
        dfs_block(node->children[3], block);
        if (node->children[0]->data_type == DataType::INT && !node->children[3]->has_return) {
            add_error_message(node->children[3]->line, "Function may return nothing.");
        }
    }
    if (!ir.block_terminated) {
        if (node->children[0]->data_type == DataType::VOID) {
            ir.emit("ret void");
        } else {
            ir.emit("ret i32 0");
        }
    }
    std::ostringstream full_func;
    full_func << "define " << (node->children[0]->data_type == DataType::INT ? "i32" : "void") << " @" << node->children[1]->text << "(" << param_sig << ") {\n";
    full_func << "entry:\n";
    full_func << ir.entry_allocas.str();
    full_func << ir.ir_body.str();
    full_func << "}\n\n";
    return full_func.str();
}

std::string Sementiser::dfs_op(ASTNode *node, Block *block) {
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
        std::vector<std::string> args;
        if (!func.parameters.empty() && node->children.size() != 1) {
            args = dfs_func_rparams(node->children[1], block);
        }
        node->has_const_value = false;
        std::string arg_str;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) arg_str += ", ";
            arg_str += args[i];
        }
        if (func.type == FuncType::VOID) {
            ir.emit("call void @" + func.name + "(" + arg_str + ")");
            return "";
        } else {
            auto reg = ir.new_reg();
            ir.emit(reg + " = call i32 @" + func.name + "(" + arg_str + ")");
            return reg;
        }
    } else if (node->type == NodeType::CONST_INT) {
        return std::to_string(node->const_value);
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
        if (res->is_const) {
            return std::to_string(res->initial_value);
        }
        auto reg = ir.new_reg();
        ir.emit(reg + " = load i32, ptr " + res->ptr_name);
        return reg;
    } else if (node->type == NodeType::SINGLE_OP) {
        auto val = dfs_op(node->children[0], block);
        if (node->children[0]->has_const_value) {
            if (node->text == "+") node->const_value = node->children[0]->const_value;
            if (node->text == "-") node->const_value = -node->children[0]->const_value;
            if (node->text == "!") node->const_value = !node->children[0]->const_value;
        } else {
            node->has_const_value = false;
        }
        if (node->text == "+") return val;
        if (node->text == "-") {
            auto reg = ir.new_reg();
            ir.emit(reg + " = sub i32 0, " + val);
            return reg;
        }
        if (node->text == "!") {
            auto cmp = ir.new_reg();
            auto ext = ir.new_reg();
            ir.emit(cmp + " = icmp eq i32 " + val + ", 0");
            ir.emit(ext + " = zext i1 " + cmp + " to i32");
            return ext;
        }
        return val;
    } else {
        // LOGIC_OP, REL_OP, ADD_OP, MUL_OP
        if (node->text == "||" || node->text == "&&") {
            return dfs_logical_op(node, block);
        }
        auto lhs = dfs_op(node->children[0], block);
        auto rhs = dfs_op(node->children[1], block);
        auto reg = ir.new_reg();
        int value0 = node->children[0]->const_value;
        int value1 = node->children[1]->const_value;
        node->has_const_value = node->children[0]->has_const_value && node->children[1]->has_const_value;
        if (node->text == "<") {
            node->const_value = value0 < value1;
            auto ext = ir.new_reg();
            ir.emit(reg + " = icmp slt i32 " + lhs + ", " + rhs);
            ir.emit(ext + " = zext i1 " + reg + " to i32");
            return ext;
        }
        if (node->text == ">") {
            node->const_value = value0 > value1;
            auto ext = ir.new_reg();
            ir.emit(reg + " = icmp sgt i32 " + lhs + ", " + rhs);
            ir.emit(ext + " = zext i1 " + reg + " to i32");
            return ext;
        }
        if (node->text == "<=") {
            node->const_value = value0 <= value1;
            auto ext = ir.new_reg();
            ir.emit(reg + " = icmp sle i32 " + lhs + ", " + rhs);
            ir.emit(ext + " = zext i1 " + reg + " to i32");
            return ext;
        }
        if (node->text == ">=") {
            node->const_value = value0 >= value1;
            auto ext = ir.new_reg();
            ir.emit(reg + " = icmp sge i32 " + lhs + ", " + rhs);
            ir.emit(ext + " = zext i1 " + reg + " to i32");
            return ext;
        }
        if (node->text == "==") {
            node->const_value = value0 == value1;
            auto ext = ir.new_reg();
            ir.emit(reg + " = icmp eq i32 " + lhs + ", " + rhs);
            ir.emit(ext + " = zext i1 " + reg + " to i32");
            return ext;
        }
        if (node->text == "!=") {
            node->const_value = value0 != value1;
            auto ext = ir.new_reg();
            ir.emit(reg + " = icmp ne i32 " + lhs + ", " + rhs);
            ir.emit(ext + " = zext i1 " + reg + " to i32");
            return ext;
        }
        if (node->text == "+") {
            node->const_value = value0 + value1;
            ir.emit(reg + " = add i32 " + lhs + ", " + rhs);
            return reg;
        }
        if (node->text == "-") {
            node->const_value = value0 - value1;
            ir.emit(reg + " = sub i32 " + lhs + ", " + rhs);
            return reg;
        }
        if (node->text == "*") {
            node->const_value = value0 * value1;
            ir.emit(reg + " = mul i32 " + lhs + ", " + rhs);
            return reg;
        }
        if (node->text == "/") {
            if (value1 == 0) {
                add_error_message(node->children[1]->line, "0 is appeared as division number.");
                node->has_const_value = false;
            } else {
                node->const_value = value0 / value1;
            }
            ir.emit(reg + " = sdiv i32 " + lhs + ", " + rhs);
            return reg;
        }
        if (node->text == "%") {
            if (value1 == 0) {
                add_error_message(node->children[1]->line, "0 is appeared as division number.");
                node->has_const_value = false;
            } else {
                node->const_value = value0 % value1;
            }
            ir.emit(reg + " = srem i32 " + lhs + ", " + rhs);
            return reg;
        }
    }
    return "";
}

std::string Sementiser::dfs_logical_op(ASTNode *node, Block *block) {
    bool is_and = (node->text == "&&");
    auto res_ptr = "%sc.addr_" + std::to_string(ir.reg_id++);
    ir.entry_allocas << "  " << res_ptr << " = alloca i32\n";
    auto rhs_label = ir.new_label();
    auto end_label = ir.new_label();
    auto lhs = dfs_op(node->children[0], block);
    auto cond1 = ir.new_reg();
    ir.emit(cond1 + " = icmp ne i32 " + lhs + ", 0");
    if (is_and) {
        ir.emit("store i32 0, ptr " + res_ptr);
        ir.emit("br i1 " + cond1 + ", label %" + rhs_label + ", label %" + end_label);
    } else {
        ir.emit("store i32 1, ptr " + res_ptr);
        ir.emit("br i1 " + cond1 + ", label %" + end_label + ", label %" + rhs_label);
    }
    ir.block_terminated = true;
    ir.emit_label(rhs_label);
    auto rhs = dfs_op(node->children[1], block);
    auto cond2 = ir.new_reg();
    auto ext2 = ir.new_reg();
    ir.emit(cond2 + " = icmp ne i32 " + rhs + ", 0");
    ir.emit("store i32 " + ext2 + ", ptr " + res_ptr);
    ir.emit("br label %" + end_label);
    ir.block_terminated = true;
    ir.emit_label(end_label);
    auto final_reg = ir.new_reg();
    ir.emit(final_reg + " = load i32, ptr " + res_ptr);
    if (node->children[0]->has_const_value && node->children[1]->has_const_value) {
        int value0 = node->children[0]->const_value;
        int value1 = node->children[1]->const_value;
        node->has_const_value = true;
        if (node->text == "||") node->const_value = value0 || value1;
        if (node->text == "&&") node->const_value = value0 && value1;
    } else {
        node->has_const_value = false;
    }
    return final_reg;
}

std::vector<std::string> Sementiser::dfs_func_rparams(ASTNode *node, Block *block) {
    std::vector<std::string> args;
    for (const auto &parameter : node->children) {
        args.push_back("i32 " + dfs_op(parameter, block));
    }
    return args;
}
