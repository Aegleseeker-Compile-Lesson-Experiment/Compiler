#include "sementiser.h"

namespace {
    struct ArrayMeta {
        std::vector<int> dims;
        std::vector<int> strides;
        bool is_param = false;
        std::vector<int> const_vals;
    };
    struct FuncParamMeta {
        bool is_array = false;
        std::vector<int> dims;
    };
    std::unordered_map<std::string, ArrayMeta> array_meta_map;
    std::unordered_map<std::string, std::vector<FuncParamMeta>> func_param_map;
    std::vector<int> compute_strides(const std::vector<int>& dims) {
        std::vector<int> strides(dims.size(), 4);
        for (int i = static_cast<int>(dims.size()) - 2; i >= 0; --i) {
            strides[i] = strides[i + 1] * dims[i + 1];
        }
        return strides;
    }
    void flatten_init_list(
        Sementiser* self,
        ASTNode* init_node,
        Block* block,
        const std::vector<int>& dims,
        size_t dim_idx,
        std::vector<std::string>& flat_vals,
        std::vector<int>& flat_const_vals,
        bool& all_const) {
        int sub_cap = 1;
        for (size_t i = dim_idx; i < dims.size(); ++i) {
            sub_cap *= dims[i];
        }
        if (init_node->type != NodeType::INITVALLIST) {
            std::string val = self->dfs_op(init_node, block);
            flat_vals.push_back(val);
            if (init_node->has_const_value) {
                flat_const_vals.push_back(init_node->const_value);
            } else {
                flat_const_vals.push_back(0);
                all_const = false;
            }
            return;
        }
        size_t start_sz = flat_vals.size();
        int elem_stride = (dim_idx + 1 < dims.size()) ? (sub_cap / dims[dim_idx]) : 1;
        for (ASTNode* item : init_node->children) {
            if (static_cast<int>(flat_vals.size() - start_sz) >= sub_cap) {
                self->add_error_message(item->line, "Excess elements in array initializer.");
                break;
            }
            if (item->type == NodeType::INITVALLIST) {
                if (dim_idx + 1 >= dims.size()) {
                    self->add_error_message(item->line, "Too many braces in scalar initializer.");
                    continue;
                }
                int offset = static_cast<int>(flat_vals.size() - start_sz);
                int rem = offset % elem_stride;
                if (rem != 0) {
                    int pad = elem_stride - rem;
                    flat_vals.insert(flat_vals.end(), pad, "0");
                    flat_const_vals.insert(flat_const_vals.end(), pad, 0);
                }
                flatten_init_list(self, item, block, dims, dim_idx + 1, flat_vals, flat_const_vals, all_const);
            } else {
                std::string val = self->dfs_op(item, block);
                flat_vals.push_back(val);
                if (item->has_const_value) {
                    flat_const_vals.push_back(item->const_value);
                } else {
                    flat_const_vals.push_back(0);
                    all_const = false;
                }
            }
        }
        int filled = static_cast<int>(flat_vals.size() - start_sz);
        if (filled < sub_cap) {
            int pad = sub_cap - filled;
            flat_vals.insert(flat_vals.end(), pad, "0");
            flat_const_vals.insert(flat_const_vals.end(), pad, 0);
        }
    }
    struct LValAddrResult {
        std::string addr_reg;
        size_t idx_cnt = 0;
        bool all_idx_const = true;
        int flat_const_idx = 0;
    };
    LValAddrResult eval_array_lval_addr(
        Sementiser* self,
        IRGenerator& ir,
        ASTNode* lval,
        Block* block,
        const TableSymbol* sym,
        const ArrayMeta& meta) {
        LValAddrResult out;
        out.idx_cnt = lval->children.size() - 1;
        if (out.idx_cnt > meta.dims.size()) {
            self->add_error_message(lval->line, "Subscripted value is not an array.");
        }
        std::string offset_op = "0";
        for (size_t j = 0; j < out.idx_cnt && j < meta.dims.size(); ++j) {
            ASTNode* idx_node = lval->children[j + 1];
            std::string idx_val = self->dfs_op(idx_node, block);
            int stride = meta.strides[j];
            if (idx_node->has_const_value) {
                out.flat_const_idx += idx_node->const_value * (stride / 4);
            } else {
                out.all_idx_const = false;
            }
            std::string term;
            if (idx_node->has_const_value) {
                long long byte_c = static_cast<long long>(idx_node->const_value) * stride;
                term = std::to_string(byte_c);
            } else if (stride == 1) {
                term = idx_val;
            } else {
                term = ir.new_reg();
                ir.emit(term + " = mul i32 " + idx_val + ", " + std::to_string(stride));
                ir.emit_inst({IROp::MUL, term, idx_val, std::to_string(stride), {}, {}});
            }
            if (offset_op == "0") {
                offset_op = term;
            } else if (term != "0") {
                if (offset_op[0] != '%' && term[0] != '%') {
                    offset_op = std::to_string(std::stoll(offset_op) + std::stoll(term));
                } else {
                    auto next_off = ir.new_reg();
                    ir.emit(next_off + " = add i32 " + offset_op + ", " + term);
                    ir.emit_inst({IROp::ADD, next_off, offset_op, term, {}, {}});
                    offset_op = next_off;
                }
            }
        }
        if (out.idx_cnt == 0) {
            out.addr_reg = sym->ptr_name;
        } else {
            out.addr_reg = ir.new_reg();
            ir.emit(out.addr_reg + " = getelementptr i8, ptr " + sym->ptr_name + ", i32 " + offset_op);
            ir.emit_inst({IROp::GEP, out.addr_reg, sym->ptr_name, offset_op, {}, {}});
        }
        return out;
    }
}

std::vector<std::pair<int, std::string>> Sementiser::check_error(bool gen_code) {
    auto root = Global::ASTRoot;
    rootBlock = Block{{}, 0, nullptr};
    func_param_map.clear();
    functions.push_back(Func{FuncType::INT, "getint", {}});
    functions.push_back(Func{FuncType::VOID, "putint", {"a"}});
    func_param_map["getint"] = {};
    func_param_map["putint"] = {{false, {}}};
    std::ostringstream module_ir;
    for (auto &function : root->children) {
        module_ir << dfs_func(function, &rootBlock);
    }
    std::ofstream ir_file("output.ll");
    ir_file << module_ir.str();
    ir_file.close();
    for (auto& ir_fn : functions_ir) {
        eliminate_tail_recursion(ir_fn);
        optimize_ir(ir_fn.insts);
    }
    inline_functions(functions_ir);
    std::vector<IRUnit> units;
    for (auto& ir_fn : functions_ir) {
        ir.blocks.clear();
        ir.label_block.clear();
        ir.ssa_id = 0;
        ir.cur_block = 0;
        ir.construct_blocks(ir_fn);
        auto idom = ir.mem2reg();
        run_cse(ir.blocks, idom);
        run_linear_combine(ir.blocks, idom, ir.ssa_id);
        run_licm(ir.blocks, idom);
        reassociate_loop_adds(ir.blocks, idom);
        run_licm(ir.blocks, idom);
        run_ivsr(ir.blocks, idom, ir.ssa_id);
        run_licm(ir.blocks, idom);
        run_cse(ir.blocks, idom);
        units.push_back({&ir_fn, std::move(ir.blocks), std::move(idom), {}});
    }
    analyze_interprocedural_non_negative(units);
    for (auto& unit : units) {
        ir.blocks = std::move(unit.blocks);
        ir.ssa_id = 0;
        cg.non_neg_regs = unit.non_neg_vars;
        ir.split_critical_edges();
        ir.eliminate_phis();
        auto intervals = ir.build_live_intervals(unit.fn->params, ir.blocks);
        cg.linear_scan_allocate(intervals);
        cg.gen_func(ir.blocks, *unit.fn);
    }
    if (gen_code) {
        std::cout << cg.generate();
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
    if (node->children.empty()) {
        node->has_return = false;
        return;
    }
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
        for (const auto& def : node->children[0]->children) {
            TableSymbol temp;
            temp.name = def->children[0]->text;
            temp.type = DataType::INT;
            temp.is_const = def->has_const_value;
            temp.may_have_been_changed = false;
            auto res = find_in_block(temp, block);
            if (res->name != "") {
                add_error_message(def->line, "Symbol has been used in current scope.");
            } else {
                delete res;
            }
            if (def->type == NodeType::DEC_ARRAY_DEF || def->type == NodeType::DECASSIGN_ARRAY_DEF) {
                std::vector<int> dims;
                long long total_elems = 1;
                for (ASTNode* dim_node : def->children[1]->children) {
                    int d = dim_node->const_value;
                    if (d <= 0) {
                        add_error_message(dim_node->line, "Array size must be positive.");
                        d = 1;
                    }
                    dims.push_back(d);
                    total_elems *= d;
                }
                int total_bytes = static_cast<int>(total_elems * 4);
                auto ptr = "%" + temp.name + ".addr_" + std::to_string(ir.reg_id++);
                temp.ptr_name = ptr;
                temp.may_have_been_changed = !temp.is_const;
                ir.entry_allocas << "  " << ptr << " = alloca [" << total_elems << " x i32]\n";
                ir.entry_allocas_insts.push_back({IROp::ALLOCA_ARR, ptr, std::to_string(total_bytes), "", {}, {}});
                ArrayMeta meta;
                meta.dims = dims;
                meta.strides = compute_strides(dims);
                meta.is_param = false;
                if (def->type == NodeType::DECASSIGN_ARRAY_DEF) {
                    std::vector<std::string> flat_vals;
                    std::vector<int> flat_const_vals;
                    bool all_const = true;
                    flatten_init_list(this, def->children[2], block, dims, 0, flat_vals, flat_const_vals, all_const);
                    if (temp.is_const && !all_const) {
                        add_error_message(def->line, "Const array initializer must be constant.");
                    }
                    for (int idx = 0; idx < static_cast<int>(flat_vals.size()); ++idx) {
                        auto elem_addr = ir.new_reg();
                        std::string byte_off = std::to_string(idx * 4);
                        ir.emit(elem_addr + " = getelementptr i8, ptr " + ptr + ", i32 " + byte_off);
                        ir.emit_inst({IROp::GEP, elem_addr, ptr, byte_off, {}, {}});
                        ir.emit("store i32 " + flat_vals[idx] + ", ptr " + elem_addr);
                        ir.emit_inst({IROp::MEM_STORE, "", flat_vals[idx], elem_addr, {}, {}});
                    }
                    if (temp.is_const) {
                        meta.const_vals = std::move(flat_const_vals);
                    }
                }
                array_meta_map[ptr] = std::move(meta);
            } else {
                auto ptr = "%" + temp.name + ".addr_" + std::to_string(ir.reg_id++);
                temp.ptr_name = ptr;
                ir.entry_allocas << "  " << ptr << " = alloca i32\n";
                ir.entry_allocas_insts.push_back({IROp::ALLOCA, ptr, "", "", {}, {}});
                if (def->type == NodeType::DECASSIGN_DEF) {
                    auto init_val = dfs_op(def->children[1], block);
                    ir.emit("store i32 " + init_val + ", ptr " + ptr);
                    ir.emit_inst({IROp::STORE, "", init_val, ptr, {}, {}});
                    temp.initial_value = def->children[1]->const_value;
                    temp.may_have_been_changed = !def->children[1]->has_const_value;
                }
            }
            block->SymbolTable.push_back(temp);
        }
    } else if (node->type == NodeType::ASSIGN_STMT) {
        ASTNode* lval = node->children[0];
        ASTNode* ident_node = (lval->type == NodeType::LVAL) ? lval->children[0] : lval;
        TableSymbol temp;
        temp.name = ident_node->text;
        auto res = find_in_blocks(temp, block);
        if (res->name == "") {
            add_error_message(node->line, "Symbol not declared before.");
            dfs_op(node->children[1], block);
            delete res;
            return;
        }
        if (res->is_const) {
            add_error_message(ident_node->line, "Const value should not be changed.");
        }
        auto arr_it = array_meta_map.find(res->ptr_name);
        if (arr_it != array_meta_map.end()) {
            const auto& meta = arr_it->second;
            auto addr_res = eval_array_lval_addr(this, ir, lval, block, res, meta);
            if (addr_res.idx_cnt != meta.dims.size()) {
                add_error_message(lval->line, "Cannot assign to an array sub-object.");
            }
            auto val = dfs_op(node->children[1], block);
            ir.emit("store i32 " + val + ", ptr " + addr_res.addr_reg);
            ir.emit_inst({IROp::MEM_STORE, "", val, addr_res.addr_reg, {}, {}});
        } else {
            if (lval->type == NodeType::LVAL && lval->children.size() > 1) {
                add_error_message(lval->line, "Subscripted value is not an array.");
            }
            res->type = DataType::INT;
            auto val = dfs_op(node->children[1], block);
            res->may_have_been_changed = !node->children[1]->has_const_value;
            res->initial_value = node->children[1]->const_value;
            ir.emit("store i32 " + val + ", ptr " + res->ptr_name);
            ir.emit_inst({IROp::STORE, "", val, res->ptr_name, {}, {}});
        }
    } else if (node->type == NodeType::IF_STMT) {
        auto then_label = ir.new_label();
        auto end_label = ir.new_label();
        auto cond_val = dfs_op(node->children[0], block);
        auto cond_i1 = ir.new_reg();
        ir.emit(cond_i1 + " = icmp ne i32 " + cond_val + ", 0");
        ir.emit("br i1 " + cond_i1 + ", label %" + then_label + ", label %" + end_label);
        ir.emit_inst({IROp::BR_COND, then_label, cond_val, end_label, {}, {}});
        ir.block_terminated = true;
        ir.emit_label(then_label);
        dfs_stmt(node->children[1], block, 0);
        if (!ir.block_terminated) {
            ir.emit("br label %" + end_label);
            ir.emit_inst({IROp::JMP, end_label, "", "", {}, {}});
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
        ir.emit_inst({IROp::BR_COND, then_label, cond_val, else_label, {}, {}});
        ir.block_terminated = true;
        ir.emit_label(then_label);
        dfs_stmt(node->children[1], block, 0);
        if (!ir.block_terminated) {
            ir.emit("br label %" + end_label);
            ir.emit_inst({IROp::JMP, end_label, "", "", {}, {}});
            ir.block_terminated = true;
        }
        ir.emit_label(else_label);
        dfs_stmt(node->children[2], block, 0);
        if (!ir.block_terminated) {
            ir.emit("br label %" + end_label);
            ir.emit_inst({IROp::JMP, end_label, "", "", {}, {}});
            ir.block_terminated = true;
        }
        ir.emit_label(end_label);
        node->has_return = node->children[1]->has_return && node->children[2]->has_return;
    } else if (node->type == NodeType::WHILE_STMT) {
        auto cond_label = ir.new_label();
        auto body_label = ir.new_label();
        auto end_label = ir.new_label();
        ir.emit("br label %" + cond_label);
        ir.emit_inst({IROp::JMP, cond_label, "", "", {}, {}});
        ir.block_terminated = true;
        ir.emit_label(cond_label);
        auto cond_val = dfs_op(node->children[0], block);
        auto cond_i1 = ir.new_reg();
        ir.emit(cond_i1 + " = icmp ne i32 " + cond_val + ", 0");
        ir.emit("br i1 " + cond_i1 + ", label %" + body_label + ", label %" + end_label);
        ir.emit_inst({IROp::BR_COND, body_label, cond_val, end_label, {}, {}});
        ir.block_terminated = true;
        ir.loop_stack.push_back({cond_label, end_label});
        ir.emit_label(body_label);
        dfs_stmt(node->children[1], block, 1);
        if (!ir.block_terminated) {
            ir.emit("br label %" + cond_label);
            ir.emit_inst({IROp::JMP, cond_label, "", "", {}, {}});
            ir.block_terminated = true;
        }
        ir.loop_stack.pop_back();
        ir.emit_label(end_label);
    } else if (node->type == NodeType::FOR_STMT) {
        auto _ = new Block;
        _->fth = block;
        _->loop_layers = block->loop_layers;
        dfs_stmt(node->children[0], _, 0);
        auto cond_label = ir.new_label();
        auto body_label = ir.new_label();
        auto step_label = ir.new_label();
        auto end_label = ir.new_label();
        ir.emit("br label %" + cond_label);
        ir.emit_inst({IROp::JMP, cond_label, "", "", {}, {}});
        ir.block_terminated = true;
        ir.emit_label(cond_label);
        if (node->children[1]->type == NodeType::EMPTY_STMT) {
            ir.emit("br label %" + body_label);
            ir.emit_inst({IROp::JMP, body_label, "", "", {}, {}});
            ir.block_terminated = true;
        } else {
            auto cond_val = dfs_op(node->children[1], _);
            auto cond_i1 = ir.new_reg();
            ir.emit(cond_i1 + " = icmp ne i32 " + cond_val + ", 0");
            ir.emit("br i1 " + cond_i1 + ", label %" + body_label + ", label %" + end_label);
            ir.emit_inst({IROp::BR_COND, body_label, cond_val, end_label, {}, {}});
            ir.block_terminated = true;
        }
        ir.loop_stack.push_back({step_label, end_label});
        ir.emit_label(body_label);
        dfs_stmt(node->children[3], _, 1);
        if (!ir.block_terminated) {
            ir.emit("br label %" + step_label);
            ir.emit_inst({IROp::JMP, step_label, "", "", {}, {}});
            ir.block_terminated = true;
        }
        ir.loop_stack.pop_back();
        ir.emit_label(step_label);
        dfs_stmt(node->children[2], _, 0);
        delete _;
        ir.emit("br label %" + cond_label);
        ir.emit_inst({IROp::JMP, cond_label, "", "", {}, {}});
        ir.block_terminated = true;
        ir.emit_label(end_label);
    } else if (node->type == NodeType::RETURN_STMT) {
        if (node->children.empty()) {
            if (functions.back().type != FuncType::VOID) {
                add_error_message(node->line, "Return void but function returns int.");
            }
            ir.emit("ret void");
            ir.emit_inst({IROp::RET, "", "", "", {}, {}});
        } else {
            if (functions.back().type != FuncType::INT) {
                add_error_message(node->line, "Return int but function returns void.");
            }
            auto ret_val = dfs_op(node->children[0], block);
            ir.emit("ret i32 " + ret_val);
            ir.emit_inst({IROp::RET, "", ret_val, "", {}, {}});
        }
        ir.block_terminated = true;
        node->has_return = true;
    } else if (node->type == NodeType::BREAK_STMT) {
        if (block->loop_layers == 0) {
            add_error_message(node->line, "Break happens outside of loop.");
        } else {
            ir.emit("br label %" + ir.loop_stack.back().second);
            ir.emit_inst({IROp::JMP, ir.loop_stack.back().second, "", "", {}, {}});
            ir.block_terminated = true;
        }
    } else if (node->type == NodeType::CONTINUE_STMT) {
        if (block->loop_layers == 0) {
            add_error_message(node->line, "Continue happens outside of loop.");
        } else {
            ir.emit("br label %" + ir.loop_stack.back().first);
            ir.emit_inst({IROp::JMP, ir.loop_stack.back().first, "", "", {}, {}});
            ir.block_terminated = true;
        }
    } else if (node->type == NodeType::FUNC_CALL) {
        dfs_op(node, block);
    } else if (node->type != NodeType::EMPTY_STMT) {
        dfs_op(node, block);
    }
}

std::string Sementiser::dfs_func(ASTNode *node, Block *block) {
    ir.entry_allocas.str("");
    ir.entry_allocas.clear();
    ir.ir_body.str("");
    ir.ir_body.clear();
    ir.entry_allocas_insts.clear();
    ir.body_insts.clear();
    ir.reg_id = 0;
    ir.block_terminated = false;
    array_meta_map.clear();
    Func temp;
    temp.type = node->children[0]->data_type == DataType::INT ? FuncType::INT : FuncType::VOID;
    temp.name = node->children[1]->text;
    IRFunc ir_fn;
    ir_fn.name = temp.name;
    ir_fn.is_void = (temp.type == FuncType::VOID);
    if (temp.name == "main") {
        if (temp.type != FuncType::INT) {
            add_error_message(node->children[0]->line, "Main returns void.");
        }
    }
    block->SymbolTable.clear();
    std::string param_sig;
    std::vector<FuncParamMeta> pmeta_list;
    if (node->children.size() == 3) {
        functions.push_back(temp);
        func_param_map[temp.name] = {};
        dfs_block(node->children[2], block);
        if (node->children[0]->data_type == DataType::INT && !node->children[2]->has_return) {
            add_error_message(node->children[2]->line, "Function may return nothing.");
        }
    } else {
        bool first_p = true;
        for (const auto &parameter : node->children[2]->children) {
            auto p_name = parameter->children[0]->text;
            auto param_reg = "%" + p_name;
            ir_fn.params.push_back(param_reg);
            if (first_p) {
                first_p = false;
            } else {
                param_sig += ", ";
            }
            if (parameter->type == NodeType::ARRAYPARAM) {
                param_sig += "ptr " + param_reg;
                std::vector<int> dims = {-1};
                if (parameter->children.size() > 1) {
                    for (ASTNode* dim_node : parameter->children[1]->children) {
                        int d = dim_node->const_value;
                        if (d <= 0) {
                            add_error_message(dim_node->line, "Array parameter dimension must be positive.");
                            d = 1;
                        }
                        dims.push_back(d);
                    }
                }
                ArrayMeta meta;
                meta.dims = dims;
                meta.strides = compute_strides(dims);
                meta.is_param = true;
                array_meta_map[param_reg] = std::move(meta);
                block->SymbolTable.push_back(TableSymbol(DataType::INT, p_name, param_reg, 0, false, true));
                pmeta_list.push_back({true, dims});
            } else {
                param_sig += "i32 " + param_reg;
                auto ptr = "%" + p_name + ".addr_" + std::to_string(ir.reg_id++);
                ir.entry_allocas << "  " << ptr << " = alloca i32\n";
                ir.entry_allocas_insts.push_back({IROp::ALLOCA, ptr, "", "", {}, {}});
                ir.emit("store i32 " + param_reg + ", ptr " + ptr);
                ir.emit_inst({IROp::STORE, "", param_reg, ptr, {}, {}});

                block->SymbolTable.push_back(TableSymbol(DataType::INT, p_name, ptr, 0, false, true));
                pmeta_list.push_back({false, {}});
            }
            temp.parameters.push_back(p_name);
        }
        functions.push_back(temp);
        func_param_map[temp.name] = std::move(pmeta_list);
        dfs_block(node->children[3], block);
        if (node->children[0]->data_type == DataType::INT && !node->children[3]->has_return) {
            add_error_message(node->children[3]->line, "Function may return nothing.");
        }
    }
    if (!ir.block_terminated) {
        if (node->children[0]->data_type == DataType::VOID) {
            ir.emit("ret void");
            ir.emit_inst({IROp::RET, "", "", "", {}, {}});
        } else {
            ir.emit("ret i32 0");
            ir.emit_inst({IROp::RET, "", "0", "", {}, {}});
        }
    }
    std::ostringstream full_func;
    full_func << "define " << (node->children[0]->data_type == DataType::INT ? "i32" : "void")
              << " @" << node->children[1]->text << "(" << param_sig << ") {\n";
    full_func << "entry:\n";
    full_func << ir.entry_allocas.str();
    full_func << ir.ir_body.str();
    full_func << "}\n\n";
    ir_fn.insts.insert(ir_fn.insts.end(), ir.entry_allocas_insts.begin(), ir.entry_allocas_insts.end());
    ir_fn.insts.insert(ir_fn.insts.end(), ir.body_insts.begin(), ir.body_insts.end());
    functions_ir.push_back(ir_fn);
    return full_func.str();
}

std::string Sementiser::dfs_op(ASTNode *node, Block *block, bool block_error) {
    if (node->type == NodeType::FUNC_CALL) {
        auto func = find_in_functions(node->children[0]->text);
        if (func.type == FuncType::NOTFOUND) {
            if (!block_error) {
                add_error_message(node->children[0]->line, "Function not defined.");
            }
        }
        if ((!func.parameters.empty() && (node->children.size() == 1 || func.parameters.size() != node->children[1]->children.size())) ||
            (func.parameters.empty() && node->children.size() != 1)) {
            if (!block_error) {
                add_error_message(node->children[0]->line, "Number of function parameters not matched.");
            }
        }
        std::vector<std::string> args;
        if (!func.parameters.empty() && node->children.size() != 1) {
            args = dfs_func_rparams(node->children[1], block);
        }
        node->has_const_value = false;
        const auto& expected_params = func_param_map[func.name];
        std::string arg_str;
        for (size_t i = 0; i < args.size(); ++i) {
            if (i > 0) arg_str += ", ";
            bool is_ptr_arg = (i < expected_params.size() && expected_params[i].is_array);
            arg_str += (is_ptr_arg ? "ptr " : "i32 ") + args[i];
        }
        if (func.type == FuncType::VOID) {
            ir.emit("call void @" + func.name + "(" + arg_str + ")");
            ir.emit_inst({IROp::CALL, "", func.name, "", args, {}});
            return "";
        } else {
            auto reg = ir.new_reg();
            ir.emit(reg + " = call i32 @" + func.name + "(" + arg_str + ")");
            ir.emit_inst({IROp::CALL, reg, func.name, "", args, {}});
            return reg;
        }
    } else if (node->type == NodeType::CONST_INT) {
        return std::to_string(node->const_value);
    } else if (node->type == NodeType::LVAL || node->type == NodeType::IDENT) {
        ASTNode* ident_node = (node->type == NodeType::LVAL) ? node->children[0] : node;
        TableSymbol temp;
        temp.name = ident_node->text;
        auto res = find_in_blocks(temp, block);
        if (res->name == "") {
            if (!block_error) {
                add_error_message(ident_node->line, "Symbol not declared before.");
            }
            delete res;
            return "0";
        }
        auto arr_it = array_meta_map.find(res->ptr_name);
        if (arr_it != array_meta_map.end()) {
            const auto& meta = arr_it->second;
            auto addr_res = eval_array_lval_addr(this, ir, node, block, res, meta);
            if (addr_res.idx_cnt < meta.dims.size()) {
                node->has_const_value = false;
                return addr_res.addr_reg;
            }
            if (res->is_const && addr_res.all_idx_const &&
                addr_res.flat_const_idx >= 0 &&
                addr_res.flat_const_idx < static_cast<int>(meta.const_vals.size())) {
                node->has_const_value = true;
                node->const_value = meta.const_vals[addr_res.flat_const_idx];
                return std::to_string(node->const_value);
            }
            node->has_const_value = false;
            auto reg = ir.new_reg();
            ir.emit(reg + " = load i32, ptr " + addr_res.addr_reg);
            ir.emit_inst({IROp::MEM_LOAD, reg, addr_res.addr_reg, "", {}, {}});
            return reg;
        } else {
            if (node->type == NodeType::LVAL && node->children.size() > 1) {
                if (!block_error) {
                    add_error_message(node->line, "Subscripted value is not an array.");
                }
            }
            if (res->is_const) {
                node->has_const_value = true;
                node->const_value = res->initial_value;
                return std::to_string(res->initial_value);
            }
            node->has_const_value = false;
            auto reg = ir.new_reg();
            ir.emit(reg + " = load i32, ptr " + res->ptr_name);
            ir.emit_inst({IROp::LOAD, reg, res->ptr_name, "", {}, {}});
            return reg;
        }
    } else if (node->type == NodeType::SINGLE_OP) {
        auto val = dfs_op(node->children[0], block);
        if (node->children[0]->has_const_value) {
            node->has_const_value = true;
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
            ir.emit_inst({IROp::SUB, reg, "0", val, {}, {}});
            return reg;
        }
        if (node->text == "!") {
            auto cmp = ir.new_reg();
            ir.emit(cmp + " = icmp eq i32 " + val + ", 0");
            ir.emit_inst({IROp::ICMP_EQ, cmp, val, "0", {}, {}});
            return cmp;
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
            ir.emit(reg + " = icmp slt i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::ICMP_SLT, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == ">") {
            node->const_value = value0 > value1;
            ir.emit(reg + " = icmp sgt i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::ICMP_SGT, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "<=") {
            node->const_value = value0 <= value1;
            ir.emit(reg + " = icmp sle i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::ICMP_SLE, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == ">=") {
            node->const_value = value0 >= value1;
            ir.emit(reg + " = icmp sge i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::ICMP_SGE, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "==") {
            node->const_value = value0 == value1;
            ir.emit(reg + " = icmp eq i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::ICMP_EQ, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "!=") {
            node->const_value = value0 != value1;
            ir.emit(reg + " = icmp ne i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::ICMP_NE, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "+") {
            node->const_value = value0 + value1;
            ir.emit(reg + " = add i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::ADD, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "-") {
            node->const_value = value0 - value1;
            ir.emit(reg + " = sub i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::SUB, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "*") {
            node->const_value = value0 * value1;
            ir.emit(reg + " = mul i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::MUL, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "/") {
            if (node->has_const_value) {
                if (value1 == 0) {
                    if (!block_error) {
                        add_error_message(node->children[1]->line, "0 is appeared as division number.");
                    }
                    node->has_const_value = false;
                } else {
                    node->const_value = value0 / value1;
                }
            }
            ir.emit(reg + " = sdiv i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::SDIV, reg, lhs, rhs, {}, {}});
            return reg;
        }
        if (node->text == "%") {
            if (node->has_const_value) {
                if (value1 == 0) {
                    if (!block_error) {
                        add_error_message(node->children[1]->line, "0 is appeared as division number.");
                    }
                    node->has_const_value = false;
                } else {
                    node->const_value = value0 % value1;
                }
            }
            ir.emit(reg + " = srem i32 " + lhs + ", " + rhs);
            ir.emit_inst({IROp::SREM, reg, lhs, rhs, {}, {}});
            return reg;
        }
    }
    return "";
}

std::string Sementiser::dfs_logical_op(ASTNode *node, Block *block) {
    bool is_and = (node->text == "&&");
    auto res_ptr = "%sc.addr_" + std::to_string(ir.reg_id++);
    ir.entry_allocas << "  " << res_ptr << " = alloca i32\n";
    ir.entry_allocas_insts.push_back({IROp::ALLOCA, res_ptr, "", "", {}, {}});
    auto rhs_label = ir.new_label();
    auto end_label = ir.new_label();
    auto lhs = dfs_op(node->children[0], block);
    auto cond1 = ir.new_reg();
    ir.emit(cond1 + " = icmp ne i32 " + lhs + ", 0");
    if (is_and) {
        ir.emit("store i32 0, ptr " + res_ptr);
        ir.emit("br i1 " + cond1 + ", label %" + rhs_label + ", label %" + end_label);
        ir.emit_inst({IROp::STORE, "", "0", res_ptr, {}, {}});
        ir.emit_inst({IROp::BR_COND, rhs_label, lhs, end_label, {}, {}});
    } else {
        ir.emit("store i32 1, ptr " + res_ptr);
        ir.emit("br i1 " + cond1 + ", label %" + end_label + ", label %" + rhs_label);
        ir.emit_inst({IROp::STORE, "", "1", res_ptr, {}, {}});
        ir.emit_inst({IROp::BR_COND, end_label, lhs, rhs_label, {}, {}});
    }
    ir.block_terminated = true;
    ir.emit_label(rhs_label);
    std::string rhs;
    if ((node->text == "||" && node->children[0]->has_const_value && node->children[0]->const_value) ||
        (node->text == "&&" && node->children[0]->has_const_value && !node->children[0]->const_value)) {
        rhs = dfs_op(node->children[1], block, true);
    } else {
        rhs = dfs_op(node->children[1], block, false);
    }
    auto cond2 = ir.new_reg();
    auto ext2 = ir.new_reg();
    ir.emit(cond2 + " = icmp ne i32 " + rhs + ", 0");
    ir.emit(ext2 + " = zext i1 " + cond2 + " to i32");
    ir.emit("store i32 " + ext2 + ", ptr " + res_ptr);
    ir.emit("br label %" + end_label);
    ir.emit_inst({IROp::ICMP_NE, cond2, rhs, "0", {}, {}});
    ir.emit_inst({IROp::STORE, "", cond2, res_ptr, {}, {}});
    ir.emit_inst({IROp::JMP, end_label, "", "", {}, {}});
    ir.block_terminated = true;
    ir.emit_label(end_label);
    auto final_reg = ir.new_reg();
    ir.emit(final_reg + " = load i32, ptr " + res_ptr);
    ir.emit_inst({IROp::LOAD, final_reg, res_ptr, "", {}, {}});
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
        args.push_back(dfs_op(parameter, block));
    }
    return args;
}
