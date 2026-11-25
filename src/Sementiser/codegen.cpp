#include "codegen.h"

int CodeGen::get_slot(const std::string& name) {
    if (stack_offset.find(name) == stack_offset.end()) {
        current_offset -= 4;
        stack_offset[name] = current_offset;
    }
    return stack_offset[name];
}

std::string CodeGen::generate() {
    asm_out.str("");
    asm_out << "  .text\n";
    asm_out << "  .globl _start\n";
    asm_out << "_start:\n";
    asm_out << "  call main\n";
    asm_out << "  li a7, 93\n";
    asm_out << "  ecall\n";
    optimize_cg(insts);
    serialize_asm();
    return asm_out.str();
}

void CodeGen::serialize_asm() {
    static const std::unordered_set<std::string> r_type = {
        "add", "addw", "sub", "subw",
        "mul", "mulw", "mulh", "mulhu", "mulhsu",
        "div", "divw", "divu", "divuw",
        "rem", "remw", "remu", "remuw",
        "and", "or", "xor",
        "sll", "sllw", "srl", "srlw", "sra", "sraw",
        "slt", "sltu"
    };
    static const std::unordered_set<std::string> i_type = {
        "addi", "addiw",
        "andi", "ori", "xori",
        "slli", "slliw", "srli", "srliw", "srai", "sraiw",
        "slti", "sltiu"
    };
    static const std::unordered_set<std::string> loads = {
        "lb", "lbu", "lh", "lhu", "lw", "lwu", "ld"
    };
    static const std::unordered_set<std::string> stores = {
        "sb", "sh", "sw", "sd"
    };
    static const std::unordered_set<std::string> unary_ops = {
        "mv", "sext.w", "neg", "negw", "not",
        "seqz", "snez", "sltz", "sgtz"
    };
    static const std::unordered_set<std::string> branch_1op = {
        "beqz", "bnez", "bltz", "bgez", "blez", "bgtz"
    };
    static const std::unordered_set<std::string> branch_2op = {
        "beq", "bne", "blt", "bge", "bltu", "bgeu"
    };
    for (const auto& i : insts) {
        if (i.op == "") {
            continue;
        } else if (i.op == "globl") {
            asm_out << "  .globl " << i.label << "\n" << "  .type " << i.label << ", @function\n";
        } else if (i.op == "label") {
            asm_out << i.label << ":\n";
        } else if (i.op == "raw") {
            asm_out << "  " << i.label << "\n";
        } else if (i.op == "ret" || i.op == "nop" || i.op == "ecall") {
            asm_out << "  " << i.op << "\n";
        } else if (i.op == "j" || i.op == "call") {
            asm_out << "  " << i.op << " " << i.label << "\n";
        } else if (i.op == "li") {
            asm_out << "  li " << i.rd << ", " << i.imm << "\n";
        } else if (i.op == "la") {
            asm_out << "  la " << i.rd << ", " << i.label << "\n";
        } else if (r_type.count(i.op)) {
            asm_out << "  " << i.op << " " << i.rd << ", " << i.rs1 << ", " << i.rs2 << "\n";
        } else if (i_type.count(i.op)) {
            asm_out << "  " << i.op << " " << i.rd << ", " << i.rs1 << ", " << i.imm << "\n";
        } else if (loads.count(i.op)) {
            asm_out << "  " << i.op << " " << i.rd << ", " << i.imm << "(" << i.rs1 << ")\n";
        } else if (stores.count(i.op)) {
            asm_out << "  " << i.op << " " << i.rs2 << ", " << i.imm << "(" << i.rs1 << ")\n";
        } else if (unary_ops.count(i.op)) {
            asm_out << "  " << i.op << " " << i.rd << ", " << i.rs1 << "\n";
        } else if (branch_1op.count(i.op)) {
            asm_out << "  " << i.op << " " << i.rs1 << ", " << i.label << "\n";
        } else if (branch_2op.count(i.op)) {
            asm_out << "  " << i.op << " " << i.rs1 << ", " << i.rs2 << ", " << i.label << "\n";
        }
    }
}

void CodeGen::linear_scan_allocate(std::vector<LiveInterval>& intervals) {
    RegAllocResult res;
    std::sort(intervals.begin(), intervals.end(), [](const LiveInterval& a, const LiveInterval& b) {
        if (a.start != b.start) return a.start < b.start;
        return a.end < b.end;
    });
    std::vector<std::string> free_caller = {
        "a7", "a6", "a5", "a4", "a3", "a2", "a1", "a0", "t4", "t3", "t2", "t1", "t0"
    };
    std::vector<std::string> free_callee = {
        "s11", "s10", "s9", "s8", "s7", "s6", "s5", "s4", "s3", "s2", "s1"
    };
    std::unordered_set<std::string> recorded_callee;
    auto alloc_from = [](std::vector<std::string>& pool) -> std::string {
        std::string r = pool.back();
        pool.pop_back();
        return r;
    };
    auto release_reg = [&](const std::string& r) {
        if (r[0] == 's') free_callee.push_back(r);
        else free_caller.push_back(r);
    };
    auto assign_phys = [&](LiveInterval& iv, const std::string& r) {
        iv.phys_reg = r;
        res.reg_map[iv.reg] = r;
        if (r[0] == 's' && !recorded_callee.count(r)) {
            recorded_callee.insert(r);
            res.used_callee_regs.push_back(r);
        }
    };
    auto assign_spill = [&](LiveInterval& iv) {
        res.spill_bytes += 8;
        iv.spill_slot = res.spill_bytes;
        res.spill_slot[iv.reg] = iv.spill_slot;
    };
    std::vector<LiveInterval*> active;
    for (auto& cur : intervals) {
        for (auto it = active.begin(); it != active.end(); ) {
            if ((*it)->end <= cur.start) {
                release_reg((*it)->phys_reg);
                it = active.erase(it);
            } else {
                ++it;
            }
        }
        std::string chosen;
        if (!cur.crosses_call && !free_caller.empty()) {
            chosen = alloc_from(free_caller);
        } else if (!free_callee.empty()) {
            chosen = alloc_from(free_callee);
        }
        if (!chosen.empty()) {
            assign_phys(cur, chosen);
            active.push_back(&cur);
            std::sort(active.begin(), active.end(), [](const auto* a, const auto* b) {
                return a->end < b->end;
            });
        } else {
            LiveInterval* victim = nullptr;
            for (auto* act : active) {
                if (cur.crosses_call && act->phys_reg[0] != 's') continue;
                if (!victim || act->end > victim->end) {
                    victim = act;
                }
            }
            if (victim && victim->end > cur.end) {
                std::string stolen = victim->phys_reg;
                victim->phys_reg.clear();
                res.reg_map.erase(victim->reg);
                assign_spill(*victim);
                active.erase(std::find(active.begin(), active.end(), victim));
                assign_phys(cur, stolen);
                active.push_back(&cur);
                std::sort(active.begin(), active.end(), [](const auto* a, const auto* b) {
                    return a->end < b->end;
                });
            } else {
                assign_spill(cur);
            }
        }
    }
    int base_offset = -16 - 8 * static_cast<int>(res.used_callee_regs.size());
    for (auto& [vreg, rel_bytes] : res.spill_slot) {
        res.spill_slot[vreg] = base_offset - rel_bytes;
    }
    alloc = res;
}

void CodeGen::emit_ld(const std::string& rd, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        insts.push_back({"ld", rd, "s0", "", offset, ""});
    } else {
        insts.push_back({"li", "t6", "", "", offset, ""});
        insts.push_back({"add", "t6", "s0", "t6", 0, ""});
        insts.push_back({"ld", rd, "t6", "", 0, ""});
    }
}

void CodeGen::emit_lw(const std::string& rd, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        insts.push_back({"lw", rd, "s0", "", offset, ""});
    } else {
        insts.push_back({"li", "t6", "", "", offset, ""});
        insts.push_back({"add", "t6", "s0", "t6", 0, ""});
        insts.push_back({"lw", rd, "t6", "", 0, ""});
    }
}

void CodeGen::emit_sd(const std::string& rs, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        insts.push_back({"sd", "", "s0", rs, offset, ""});
    } else {
        insts.push_back({"li", "t6", "", "", offset, ""});
        insts.push_back({"add", "t6", "t6", "s0", 0, ""});
        insts.push_back({"sd", "", "t6", rs, 0, ""});
    }
}

void CodeGen::emit_sw(const std::string& rs, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        insts.push_back({"sw", "", "s0", rs, offset, ""});
    } else {
        insts.push_back({"li", "t6", "", "", offset, ""});
        insts.push_back({"add", "t6", "t6", "s0", 0, ""});
        insts.push_back({"sw", "", "t6", rs, 0, ""});
    }
}

void CodeGen::gen_func(const std::vector<IRBlock>& blocks, const IRFunc& func) {
    ptr_vregs.clear();
    for (const auto& blk : blocks) {
        for (const auto& inst : blk.insts) {
            if (inst.op == IROp::ALLOCA_ARR || inst.op == IROp::GEP) {
                if (!inst.dst.empty()) ptr_vregs.insert(inst.dst);
            }
            if (inst.op == IROp::GEP || inst.op == IROp::MEM_LOAD) {
                if (!inst.src1.empty() && inst.src1[0] == '%') ptr_vregs.insert(inst.src1);
            }
            if (inst.op == IROp::MEM_STORE) {
                if (!inst.src2.empty() && inst.src2[0] == '%') ptr_vregs.insert(inst.src2);
            }
        }
    }
    bool ptr_changed = true;
    while (ptr_changed) {
        ptr_changed = false;
        for (const auto& blk : blocks) {
            for (const auto& inst : blk.insts) {
                if (inst.op == IROp::MOV && !inst.src1.empty() && inst.src1[0] == '%') {
                    if (ptr_vregs.count(inst.src1) && !ptr_vregs.count(inst.dst)) {
                        ptr_vregs.insert(inst.dst);
                        ptr_changed = true;
                    } else if (ptr_vregs.count(inst.dst) && !ptr_vregs.count(inst.src1)) {
                        ptr_vregs.insert(inst.src1);
                        ptr_changed = true;
                    }
                }
            }
        }
    }
    int used_top_bytes = 16 + 8 * static_cast<int>(alloc.used_callee_regs.size()) + alloc.spill_bytes;
    int local_arr_bytes = 0;
    std::unordered_map<std::string, int> arr_offset_map;
    for (const auto& blk : blocks) {
        for (const auto& inst : blk.insts) {
            if (inst.op == IROp::ALLOCA_ARR) {
                int bytes = std::stoi(inst.src1);
                int aligned_bytes = (bytes + 7) & ~7;
                local_arr_bytes += aligned_bytes;
                arr_offset_map[inst.dst] = -(used_top_bytes + local_arr_bytes);
            }
        }
    }

    int max_stack_args = 0;
    for (const auto& blk : blocks) {
        for (const auto& inst : blk.insts) {
            if (inst.op == IROp::CALL && inst.args.size() > 8) {
                max_stack_args = std::max(max_stack_args, static_cast<int>(inst.args.size()) - 8);
            }
        }
    }
    int raw_size = used_top_bytes + local_arr_bytes + 8 * max_stack_args;
    int frame_size = (raw_size + 15) & ~15;
    std::string epilogue_label = "Lret_" + func.name;
    insts.push_back(AsmInst{ "globl", "", "", "", 0, func.name });
    insts.push_back(AsmInst{ "label", "", "", "", 0, func.name });
    if (frame_size <= 2048) {
        insts.push_back(AsmInst{ "addi", "sp", "sp", "", -frame_size, "" });
        insts.push_back(AsmInst{ "sd", "", "sp", "ra", frame_size - 8, "" });
        insts.push_back(AsmInst{ "sd", "", "sp", "s0", frame_size - 16, "" });
        insts.push_back(AsmInst{ "addi", "s0", "sp", "", frame_size, "" });
    } else {
        insts.push_back(AsmInst{ "li", "t3", "", "", -frame_size, "" });
        insts.push_back(AsmInst{ "add", "sp", "sp", "t3", 0, "" });
        insts.push_back(AsmInst{ "li", "t3", "", "", frame_size, "" });
        insts.push_back(AsmInst{ "add", "t3", "sp", "t3", 0, "" });
        insts.push_back(AsmInst{ "sd", "", "t3", "ra", -8, "" });
        insts.push_back(AsmInst{ "sd", "", "t3", "s0", -16, "" });
        insts.push_back(AsmInst{ "mv", "s0", "t3", "", 0, "" });
    }
    for (size_t i = 0; i < alloc.used_callee_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        emit_sd(alloc.used_callee_regs[i], off);
    }
    std::vector<std::pair<std::string, std::string>> reg_param_moves;
    for (size_t i = 0; i < func.params.size() && i < 8; ++i) {
        const std::string& p = func.params[i];
        std::string arg_reg = "a" + std::to_string(i);
        if (alloc.reg_map.count(p)) {
            std::string dst_reg = alloc.reg_map[p];
            if (dst_reg != arg_reg) {
                reg_param_moves.push_back({dst_reg, arg_reg});
            }
        } else if (alloc.spill_slot.count(p)) {
            if (ptr_vregs.count(p)) emit_sd(arg_reg, alloc.spill_slot[p]);
            else emit_sw(arg_reg, alloc.spill_slot[p]);
        }
    }
    while (!reg_param_moves.empty()) {
        bool progress = false;
        for (size_t i = 0; i < reg_param_moves.size(); ++i) {
            bool dst_is_needed = false;
            for (size_t j = 0; j < reg_param_moves.size(); ++j) {
                if (i != j && reg_param_moves[j].second == reg_param_moves[i].first) {
                    dst_is_needed = true;
                    break;
                }
            }
            if (!dst_is_needed) {
                insts.push_back({"mv", reg_param_moves[i].first, reg_param_moves[i].second, "", 0, ""});
                reg_param_moves.erase(reg_param_moves.begin() + i);
                progress = true;
                break;
            }
        }
        if (!progress) {
            std::string cycle_reg = reg_param_moves[0].second;
            insts.push_back({"mv", "t5", cycle_reg, "", 0, ""});
            for (auto& mv : reg_param_moves) {
                if (mv.second == cycle_reg) mv.second = "t5";
            }
        }
    }
    for (size_t i = 8; i < func.params.size(); ++i) {
        std::string p = func.params[i];
        if (!p.empty() && p[0] != '%') p = "%" + p;
        int caller_stack_off = static_cast<int>(i - 8) * 8;
        bool is_ptr = ptr_vregs.count(p) > 0;
        if (alloc.reg_map.count(p)) {
            if (is_ptr) emit_ld(alloc.reg_map[p], caller_stack_off);
            else emit_lw(alloc.reg_map[p], caller_stack_off);
        } else if (alloc.spill_slot.count(p)) {
            if (is_ptr) {
                emit_ld("t5", caller_stack_off);
                emit_sd("t5", alloc.spill_slot[p]);
            } else {
                emit_lw("t5", caller_stack_off);
                emit_sw("t5", alloc.spill_slot[p]);
            }
        }
    }
    std::unordered_map<std::string, int> vreg_uses;
    for (const auto& blk : blocks) {
        for (const auto& inst : blk.insts) {
            for (const auto& u : get_uses(inst)) {
                vreg_uses[u]++;
            }
        }
    }
    auto is_icmp = [](IROp op) {
        return op == IROp::ICMP_EQ  || op == IROp::ICMP_NE  ||
               op == IROp::ICMP_SLT || op == IROp::ICMP_SGT ||
               op == IROp::ICMP_SLE || op == IROp::ICMP_SGE;
    };
    for (size_t b = 0; b < blocks.size(); ++b) {
        const auto& block = blocks[b];
        if (b > 0) {
            insts.push_back({"label", "", "", "", 0, "." + func.name + "_" + block.label});
        }
        for (size_t i = 0; i < block.insts.size(); ++i) {
            const auto& inst = block.insts[i];
            if (i + 1 < block.insts.size() && is_icmp(inst.op) && block.insts[i + 1].op == IROp::BR_COND && block.insts[i + 1].src1 == inst.dst && vreg_uses[inst.dst] == 1) {
                const auto& br = block.insts[i + 1];
                std::string L_true = "." + func.name + "_" + br.dst;
                std::string L_false = "." + func.name + "_" + br.src2;
                if (inst.src2 == "0") {
                    std::string r1 = use_operand(inst.src1, "t5");
                    switch (inst.op) {
                        case IROp::ICMP_EQ:  insts.push_back({"beqz", "", r1, "", 0, L_true}); break;
                        case IROp::ICMP_NE:  insts.push_back({"bnez", "", r1, "", 0, L_true}); break;
                        case IROp::ICMP_SLT: insts.push_back({"bltz", "", r1, "", 0, L_true}); break;
                        case IROp::ICMP_SGE: insts.push_back({"bgez", "", r1, "", 0, L_true}); break;
                        case IROp::ICMP_SGT: insts.push_back({"bgtz", "", r1, "", 0, L_true}); break;
                        case IROp::ICMP_SLE: insts.push_back({"blez", "", r1, "", 0, L_true}); break;
                        default: break;
                    }
                } else if (inst.src1 == "0") {
                    std::string r2 = use_operand(inst.src2, "t5");
                    switch (inst.op) {
                        case IROp::ICMP_EQ:  insts.push_back({"beqz", "", r2, "", 0, L_true}); break;
                        case IROp::ICMP_NE:  insts.push_back({"bnez", "", r2, "", 0, L_true}); break;
                        case IROp::ICMP_SLT: insts.push_back({"bgtz", "", r2, "", 0, L_true}); break;
                        case IROp::ICMP_SGE: insts.push_back({"blez", "", r2, "", 0, L_true}); break;
                        case IROp::ICMP_SGT: insts.push_back({"bltz", "", r2, "", 0, L_true}); break;
                        case IROp::ICMP_SLE: insts.push_back({"bgez", "", r2, "", 0, L_true}); break;
                        default: break;
                    }
                } else {
                    std::string r1 = use_operand(inst.src1, "t5");
                    std::string r2 = use_operand(inst.src2, "t6");
                    switch (inst.op) {
                        case IROp::ICMP_EQ:  insts.push_back({"beq", "", r1, r2, 0, L_true}); break;
                        case IROp::ICMP_NE:  insts.push_back({"bne", "", r1, r2, 0, L_true}); break;
                        case IROp::ICMP_SLT: insts.push_back({"blt", "", r1, r2, 0, L_true}); break;
                        case IROp::ICMP_SGE: insts.push_back({"bge", "", r1, r2, 0, L_true}); break;
                        case IROp::ICMP_SGT: insts.push_back({"blt", "", r2, r1, 0, L_true}); break;
                        case IROp::ICMP_SLE: insts.push_back({"bge", "", r2, r1, 0, L_true}); break;
                        default: break;
                    }
                }
                insts.push_back({"j", "", "", "", 0, L_false});
                ++i;
                continue;
            }
            if (inst.op == IROp::GEP && i + 1 < block.insts.size() && vreg_uses[inst.dst] == 1 && !inst.src2.empty() && inst.src2[0] != '%') {
                int64_t off = std::stoll(inst.src2);
                const auto& next = block.insts[i + 1];
                if (off >= -2048 && off <= 2047) {
                    if (next.op == IROp::MEM_LOAD && next.src1 == inst.dst) {
                        std::string r_base = use_operand(inst.src1, "t5");
                        std::string rd = def_operand(next.dst, "t5");
                        insts.push_back({"lw", rd, r_base, "", static_cast<int>(off), ""});
                        commit_dst(next.dst, "t5");
                        ++i;
                        continue;
                    } else if (next.op == IROp::MEM_STORE && next.src2 == inst.dst && next.src1 != inst.dst) {
                        std::string r_val = use_operand(next.src1, "t5");
                        std::string r_base = use_operand(inst.src1, "t6");
                        insts.push_back({"sw", "", r_base, r_val, static_cast<int>(off), ""});
                        ++i;
                        continue;
                    }
                }
            }
            switch (inst.op) {
                case IROp::ALLOCA_ARR: {
                    int off = arr_offset_map[inst.dst];
                    std::string rd = def_operand(inst.dst, "t5");
                    if (off >= -2048 && off <= 2047) {
                        insts.push_back({"addi", rd, "s0", "", off, ""});
                    } else {
                        insts.push_back({"li", "t6", "", "", off, ""});
                        insts.push_back({"add", rd, "s0", "t6", 0, ""});
                    }
                    commit_dst(inst.dst, "t5");
                    break;
                }
                case IROp::GEP: {
                    std::string r_base = use_operand(inst.src1, "t5");
                    if (!inst.src2.empty() && inst.src2[0] != '%') {
                        int64_t off = std::stoll(inst.src2);
                        std::string rd = def_operand(inst.dst, "t5");
                        if (off == 0) {
                            if (rd != r_base) {
                                insts.push_back({"mv", rd, r_base, "", 0, ""});
                            }
                        } else if (off >= -2048 && off <= 2047) {
                            insts.push_back({"addi", rd, r_base, "", static_cast<int>(off), ""});
                        } else {
                            insts.push_back({"li", "t6", "", "", static_cast<int>(off), ""});
                            insts.push_back({"add", rd, r_base, "t6", 0, ""});
                        }
                        commit_dst(inst.dst, "t5");
                    } else {
                        std::string r_off = use_operand(inst.src2, "t6");
                        std::string rd = def_operand(inst.dst, "t5");
                        insts.push_back({"sext.w", "t6", r_off, "", 0, ""});
                        insts.push_back({"add", rd, r_base, "t6", 0, ""});
                        commit_dst(inst.dst, "t5");
                    }
                    break;
                }
                case IROp::MEM_LOAD: {
                    std::string r_addr = use_operand(inst.src1, "t5");
                    std::string rd = def_operand(inst.dst, "t5");
                    insts.push_back({"lw", rd, r_addr, "", 0, ""});
                    commit_dst(inst.dst, "t5");
                    break;
                }
                case IROp::MEM_STORE: {
                    std::string r_val = use_operand(inst.src1, "t5");
                    std::string r_addr = use_operand(inst.src2, "t6");
                    insts.push_back({"sw", "", r_addr, r_val, 0, ""});
                    break;
                }
                case IROp::ADD: {
                    bool c1 = !inst.src1.empty() && inst.src1[0] != '%';
                    bool c2 = !inst.src2.empty() && inst.src2[0] != '%';
                    if (c2 || c1) {
                        const std::string& var_op = c2 ? inst.src1 : inst.src2;
                        int64_t imm = std::stoll(c2 ? inst.src2 : inst.src1);
                        if (imm >= -2048 && imm <= 2047) {
                            std::string r1 = use_operand(var_op, "t5");
                            std::string rd = def_operand(inst.dst, "t5");
                            insts.push_back({"addiw", rd, r1, "", static_cast<int>(imm), ""});
                            commit_dst(inst.dst, "t5");
                            break;
                        }
                    }
                    std::string r1 = use_operand(inst.src1, "t5");
                    std::string r2 = use_operand(inst.src2, "t6");
                    std::string rd = def_operand(inst.dst, "t5");
                    insts.push_back({"addw", rd, r1, r2, 0, ""});
                    commit_dst(inst.dst, "t5");
                    break;
                }
                case IROp::SUB: {
                    bool c2 = !inst.src2.empty() && inst.src2[0] != '%';
                    if (c2) {
                        int64_t imm = std::stoll(inst.src2);
                        if (imm >= -2047 && imm <= 2048) {
                            std::string r1 = use_operand(inst.src1, "t5");
                            std::string rd = def_operand(inst.dst, "t5");
                            insts.push_back({"addiw", rd, r1, "", static_cast<int>(-imm), ""});
                            commit_dst(inst.dst, "t5");
                            break;
                        }
                    }
                    std::string r1 = use_operand(inst.src1, "t5");
                    std::string r2 = use_operand(inst.src2, "t6");
                    std::string rd = def_operand(inst.dst, "t5");
                    insts.push_back({"subw", rd, r1, r2, 0, ""});
                    commit_dst(inst.dst, "t5");
                    break;
                }
                case IROp::MUL: {
                    bool c1 = !inst.src1.empty() && inst.src1[0] != '%';
                    bool c2 = !inst.src2.empty() && inst.src2[0] != '%';
                    if (c1 && c2) {
                        int32_t prod = static_cast<int32_t>(std::stoll(inst.src1) * std::stoll(inst.src2));
                        std::string rd = def_operand(inst.dst, "t5");
                        insts.push_back({"li", rd, "", "", prod, ""});
                        commit_dst(inst.dst, "t5");
                    } else if (c2) {
                        gen_mul(inst.dst, inst.src1, std::stoi(inst.src2));
                    } else if (c1) {
                        gen_mul(inst.dst, inst.src2, std::stoi(inst.src1));
                    } else {
                        std::string r1 = use_operand(inst.src1, "t5");
                        std::string r2 = use_operand(inst.src2, "t6");
                        std::string rd = def_operand(inst.dst, "t5");
                        insts.push_back({"mulw", rd, r1, r2, 0, ""});
                        commit_dst(inst.dst, "t5");
                    }
                    break;
                }
                case IROp::SDIV:
                case IROp::SREM: {
                    bool is_div = (inst.op == IROp::SDIV);
                    bool c1 = !inst.src1.empty() && inst.src1[0] != '%';
                    bool c2 = !inst.src2.empty() && inst.src2[0] != '%';
                    if (c1 && c2) {
                        int32_t a = static_cast<int32_t>(std::stoll(inst.src1));
                        int32_t b = static_cast<int32_t>(std::stoll(inst.src2));
                        if (b != 0 && !(a == ~(1 << 31) && b == -1)) {
                            int32_t res = is_div ? (a / b) : (a % b);
                            std::string rd = def_operand(inst.dst, "t5");
                            insts.push_back({"li", rd, "", "", res, ""});
                            commit_dst(inst.dst, "t5");
                            break;
                        }
                    }
                    if (c2) {
                        int val = static_cast<int>(std::stoll(inst.src2));
                        bool is_non_neg = non_neg_regs.count(inst.src1) > 0;
                        if (is_div) gen_div(inst.dst, inst.src1, val, true, is_non_neg);
                        else gen_rem(inst.dst, inst.src1, val, true, is_non_neg);
                    } else if (c1) {
                        int val = static_cast<int>(std::stoll(inst.src1));
                        if (is_div) gen_div(inst.dst, inst.src2, val, false, false);
                        else gen_rem(inst.dst, inst.src2, val, false, false);
                    } else {
                        std::string r1 = use_operand(inst.src1, "t5");
                        std::string r2 = use_operand(inst.src2, "t6");
                        std::string rd = def_operand(inst.dst, "t5");
                        insts.push_back({is_div ? "divw" : "remw", rd, r1, r2, 0, ""});
                        commit_dst(inst.dst, "t5");
                    }
                    break;
                }
                case IROp::ICMP_EQ:
                case IROp::ICMP_NE:
                case IROp::ICMP_SLT:
                case IROp::ICMP_SGT:
                case IROp::ICMP_SLE:
                case IROp::ICMP_SGE: {
                    std::string r1 = use_operand(inst.src1, "t5");
                    std::string r2 = use_operand(inst.src2, "t6");
                    std::string rd = def_operand(inst.dst, "t5");
                    if (inst.op == IROp::ICMP_SLT) {
                        insts.push_back({"slt", rd, r1, r2, 0, ""});
                    } else if (inst.op == IROp::ICMP_SGT) {
                        insts.push_back({"slt", rd, r2, r1, 0, ""});
                    } else if (inst.op == IROp::ICMP_SLE) {
                        insts.push_back({"slt", rd, r2, r1, 0, ""});
                        insts.push_back({"xori", rd, rd, "", 1, ""});
                    } else if (inst.op == IROp::ICMP_SGE) {
                        insts.push_back({"slt", rd, r1, r2, 0, ""});
                        insts.push_back({"xori", rd, rd, "", 1, ""});
                    } else if (inst.op == IROp::ICMP_EQ) {
                        insts.push_back({"subw", rd, r1, r2, 0, ""});
                        insts.push_back({"seqz", rd, rd, "", 0, ""});
                    } else if (inst.op == IROp::ICMP_NE) {
                        insts.push_back({"subw", rd, r1, r2, 0, ""});
                        insts.push_back({"snez", rd, rd, "", 0, ""});
                    }
                    commit_dst(inst.dst, "t5");
                    break;
                }
                case IROp::MOV: {
                    if (!inst.src1.empty() && inst.src1[0] != '%') {
                        std::string rd = def_operand(inst.dst, "t5");
                        insts.push_back({"li", rd, "", "", std::stoi(inst.src1), ""});
                        commit_dst(inst.dst, "t5");
                    } else {
                        std::string rs = use_operand(inst.src1, "t5");
                        std::string rd = def_operand(inst.dst, "t5");
                        if (rd != rs) {
                            bool is_ptr = ptr_vregs.count(inst.dst) || ptr_vregs.count(inst.src1);
                            insts.push_back({is_ptr ? "mv" : "sext.w", rd, rs, "", 0, ""});
                        }
                        commit_dst(inst.dst, "t5");
                    }
                    break;
                }
                case IROp::JMP: {
                    insts.push_back({"j", "", "", "", 0, "." + func.name + "_" + inst.dst});
                    break;
                }
                case IROp::BR_COND: {
                    std::string r_cond = use_operand(inst.src1, "t5");
                    insts.push_back({"bnez", "", r_cond, "", 0, "." + func.name + "_" + inst.dst});
                    insts.push_back({"j", "", "", "", 0, "." + func.name + "_" + inst.src2});
                    break;
                }
                case IROp::CALL: {
                    for (size_t k = 8; k < inst.args.size(); ++k) {
                        std::string val_reg = use_operand(inst.args[k], "t5");
                        int sp_offset = static_cast<int>(k - 8) * 8;
                        const char* store_op = ptr_vregs.count(inst.args[k]) ? "sd" : "sw";
                        if (sp_offset <= 2047) {
                            insts.push_back({store_op, "", "sp", val_reg, sp_offset, ""});
                        } else {
                            insts.push_back({"li", "t6", "", "", sp_offset, ""});
                            insts.push_back({"add", "t6", "sp", "t6", 0, ""});
                            insts.push_back({store_op, "", "t6", val_reg, 0, ""});
                        }
                    }
                    std::vector<std::pair<std::string, std::string>> arg_moves;
                    for (size_t k = 0; k < inst.args.size() && k < 8; ++k) {
                        arg_moves.push_back({"a" + std::to_string(k), inst.args[k]});
                    }
                    emit_parallel_moves(arg_moves);
                    insts.push_back({"call", "", "", "", 0, inst.src1});
                    if (!inst.dst.empty()) {
                        if (alloc.reg_map.count(inst.dst)) {
                            if (alloc.reg_map[inst.dst] != "a0") {
                                insts.push_back({"mv", alloc.reg_map[inst.dst], "a0", "", 0, ""});
                            }
                        } else if (alloc.spill_slot.count(inst.dst)) {
                            emit_sw("a0", alloc.spill_slot[inst.dst]);
                        }
                    }
                    break;
                }
                case IROp::RET: {
                    if (!inst.src1.empty()) {
                        if (inst.src1[0] != '%') {
                            insts.push_back({"li", "a0", "", "", std::stoi(inst.src1), ""});
                        } else if (alloc.reg_map.count(inst.src1)) {
                            if (alloc.reg_map[inst.src1] != "a0") {
                                insts.push_back({"mv", "a0", alloc.reg_map[inst.src1], "", 0, ""});
                            }
                        } else {
                            emit_lw("a0", alloc.spill_slot[inst.src1]);
                        }
                    }
                    insts.push_back({"j", "", "", "", 0, "." + func.name + "_" + epilogue_label});
                    break;
                }
                default:
                    break;
            }
        }
    }
    insts.push_back(AsmInst{ "label", "", "", "", 0, "." + func.name + "_" + epilogue_label });
    for (size_t i = 0; i < alloc.used_callee_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        emit_ld(alloc.used_callee_regs[i], off);
    }
    insts.push_back(AsmInst{ "ld", "ra", "s0", "", -8, "" });
    insts.push_back(AsmInst{ "ld", "s0", "s0", "", -16, "" });
    if (frame_size <= 2047) {
        insts.push_back(AsmInst{ "addi", "sp", "sp", "", frame_size, "" });
    } else {
        insts.push_back(AsmInst{ "li", "t3", "", "", frame_size, "" });
        insts.push_back(AsmInst{ "add", "sp", "sp", "t3", 0, "" });
    }
    insts.push_back(AsmInst{ "ret", "", "", "", 0, "" });
}

std::string CodeGen::use_operand(const std::string& op, const std::string& scratch) {
    if (op.empty()) return "";
    if (op[0] != '%') {
        insts.push_back({"li", scratch, "", "", std::stoi(op), ""});
        return scratch;
    }
    if (alloc.reg_map.count(op)) {
        return alloc.reg_map[op];
    }
    if (ptr_vregs.count(op)) {
        emit_ld(scratch, alloc.spill_slot[op]);
    } else {
        emit_lw(scratch, alloc.spill_slot[op]);
    }
    return scratch;
}

std::string CodeGen::def_operand(const std::string& dst, const std::string& scratch) {
    if (alloc.reg_map.count(dst)) {
        return alloc.reg_map[dst];
    }
    return scratch;
}

void CodeGen::commit_dst(const std::string& dst, const std::string& scratch) {
    if (!alloc.reg_map.count(dst)) {
        if (ptr_vregs.count(dst)) {
            emit_sd(scratch, alloc.spill_slot[dst]);
        } else {
            emit_sw(scratch, alloc.spill_slot[dst]);
        }
    }
}

void CodeGen::emit_parallel_moves(const std::vector<std::pair<std::string, std::string>>& moves) {
    struct MoveTask {
        std::string dst_reg;
        std::string src_reg;
        std::string raw_src;
    };
    std::vector<MoveTask> reg_moves;
    std::vector<MoveTask> other_moves;
    for (const auto& [dst_r, src] : moves) {
        if (!src.empty() && src[0] == '%' && alloc.reg_map.count(src)) {
            std::string sr = alloc.reg_map[src];
            if (dst_r != sr) reg_moves.push_back({dst_r, sr, src});
        } else {
            other_moves.push_back({dst_r, "", src});
        }
    }
    while (!reg_moves.empty()) {
        bool progress = false;
        for (size_t i = 0; i < reg_moves.size(); ++i) {
            bool dst_is_needed = false;
            for (size_t j = 0; j < reg_moves.size(); ++j) {
                if (i != j && reg_moves[j].src_reg == reg_moves[i].dst_reg) {
                    dst_is_needed = true;
                    break;
                }
            }
            if (!dst_is_needed) {
                insts.push_back({"mv", reg_moves[i].dst_reg, reg_moves[i].src_reg, "", 0, ""});
                reg_moves.erase(reg_moves.begin() + i);
                progress = true;
                break;
            }
        }
        if (!progress) {
            std::string cycle_reg = reg_moves[0].src_reg;
            insts.push_back({"mv", "t5", cycle_reg, "", 0, ""});
            for (auto& task : reg_moves) {
                if (task.src_reg == cycle_reg) task.src_reg = "t5";
            }
        }
    }
    for (const auto& task : other_moves) {
        if (!task.raw_src.empty() && task.raw_src[0] != '%') {
            insts.push_back({"li", task.dst_reg, "", "", std::stoi(task.raw_src), ""});
        } else if (ptr_vregs.count(task.raw_src)) {
            emit_ld(task.dst_reg, alloc.spill_slot[task.raw_src]);
        } else {
            emit_lw(task.dst_reg, alloc.spill_slot[task.raw_src]);
        }
    }
}

void CodeGen::gen_mul(const std::string& dst_vreg, const std::string& var_op, int value) {
    std::string r_in = use_operand(var_op, "t5");
    std::string rd = def_operand(dst_vreg, "t5");
    if (value == 0) {
        insts.push_back({"li", rd, "", "", 0, ""});
        commit_dst(dst_vreg, "t5");
        return;
    }
    if (value == 1) {
        if (rd != r_in) {
            insts.push_back({"sext.w", rd, r_in, "", 0, ""});
        }
        commit_dst(dst_vreg, "t5");
        return;
    }
    if (value == -1) {
        insts.push_back({"subw", rd, "zero", r_in, 0, ""});
        commit_dst(dst_vreg, "t5");
        return;
    }
    bool allow_t1 = (r_in != "t5");
    struct Inst { int op, dst, s1, s2; } plan[4];
    int step_cnt = 0;
    unsigned int target = static_cast<unsigned int>(value);
    auto dfs = [&](auto& self, int d, int max_d, bool h1, unsigned int v1, bool h2, unsigned int v2) -> bool {
        if (d == max_d) return h2 && v2 == target;
        int rem = max_d - d;
        int sr[4] = {0, 1}, nsrc = 2;
        unsigned int sv[4] = {0, 1};
        if (h1) { sr[nsrc] = 2; sv[nsrc++] = v1; }
        if (h2) { sr[nsrc] = 3; sv[nsrc++] = v2; }
        int dsts[2] = {3, 2};
        int ndst = (rem == 1 || !allow_t1 || (!h1 && !h2)) ? 1 : 2;
        for (int di = 0; di < ndst; ++di) {
            int dst = dsts[di];
            auto try_step = [&](int op, int s1, int s2, unsigned int res) -> bool {
                bool nh1 = h1 || (dst == 2), nh2 = h2 || (dst == 3);
                unsigned int nv1 = (dst == 2) ? res : v1, nv2 = (dst == 3) ? res : v2;
                if (nh1 == h1 && nh2 == h2 && nv1 == v1 && nv2 == v2) return false;
                plan[d] = {op, dst, s1, s2};
                return self(self, d + 1, max_d, nh1, nv1, nh2, nv2);
            };
            for (int i = 0; i < nsrc; ++i) {
                if (!sv[i]) continue;
                if (rem == 1) {
                    if (target) {
                        int sh = __builtin_ctz(target) - __builtin_ctz(sv[i]);
                        if (sh >= 1 && sh <= 31 && (sv[i] << sh) == target && try_step(2, sr[i], sh, target)) {
                            return true;
                        }
                    }
                } else {
                    for (int sh = 1; sh <= 31 && (sv[i] << sh); ++sh) {
                        if (try_step(2, sr[i], sh, sv[i] << sh)) {
                            return true;
                        }
                    }
                }
            }
            for (int i = 0; i < nsrc; ++i) {
                for (int j = i; j < nsrc; ++j) {
                    if ((sr[i] != 0 || rem == 1) && try_step(0, sr[j], sr[i], sv[i] + sv[j])) {
                        return true;
                    }
                }
            }
            for (int i = 0; i < nsrc; ++i) {
                for (int j = 0; j < nsrc; ++j) {
                    if (i != j && sr[j] != 0 && try_step(1, sr[i], sr[j], sv[i] - sv[j])) {
                        return true;
                    }
                }
            }
        }
        return false;
    };
    for (int max_d = 1; max_d <= 4; ++max_d) {
        if (dfs(dfs, 0, max_d, false, 0, false, 0)) {
            step_cnt = max_d;
            break;
        }
    }
    const char* regs[] = {"zero", r_in.c_str(), "t5", "t6"};
    bool dst_in_phys = alloc.reg_map.count(dst_vreg) > 0;
    if (step_cnt > 0) {
        for (int i = 0; i < step_cnt; ++i) {
            auto [op, dst, s1, s2] = plan[i];
            std::string out_reg = (i == step_cnt - 1 && dst_in_phys) ? rd : regs[dst];
            if (op == 0) {
                insts.push_back({"addw", out_reg, regs[s1], regs[s2], 0, ""});
            } else if (op == 1) {
                insts.push_back({"subw", out_reg, regs[s1], regs[s2], 0, ""});
            } else if (op == 2) {
                insts.push_back({"slliw", out_reg, regs[s1], "", s2, ""});
            }
        }
        if (!dst_in_phys) {
            insts.push_back({"sext.w", "t5", "t6", "", 0, ""});
            commit_dst(dst_vreg, "t5");
        }
    } else {
        insts.push_back({"li", "t6", "", "", value, ""});
        if (dst_in_phys) {
            insts.push_back({"mulw", rd, r_in, "t6", 0, ""});
        } else {
            insts.push_back({"mulw", "t5", r_in, "t6", 0, ""});
            commit_dst(dst_vreg, "t5");
        }
    }
}

void CodeGen::gen_div(const std::string& dst_vreg, const std::string& var_op, int value, bool divided, bool non_neg) {
    std::string r_in = use_operand(var_op, "t5");
    std::string rd = def_operand(dst_vreg, "t5");
    bool dst_in_phys = alloc.reg_map.count(dst_vreg) > 0;
    if (divided && non_neg && value != 0 && value != 1 && value != -1 && value != (1 << 31)) {
        unsigned int ad = (value < 0) ? (0u - static_cast<unsigned int>(value)) : static_cast<unsigned int>(value);
        if ((ad & (ad - 1)) == 0) {
            int k = __builtin_ctz(ad);
            if (value > 0) {
                insts.push_back({"srliw", rd, r_in, "", k, ""});
            } else {
                insts.push_back({"srliw", "t6", r_in, "", k, ""});
                insts.push_back({"subw", rd, "zero", "t6", 0, ""});
            }
            commit_dst(dst_vreg, "t5");
            return;
        }
    }
    if (r_in == "t5") {
        if (!divided && value == 0) {
            insts.push_back({"li", "t5", "", "", 0, ""});
        } else {
            insts.push_back({"li", "t6", "", "", value, ""});
            if (divided) {
                insts.push_back({"divw", "t5", "t5", "t6", 0, ""});
            } else {
                insts.push_back({"divw", "t5", "t6", "t5", 0, ""});
            }
        }
        if (dst_in_phys) {
            insts.push_back({"sext.w", rd, "t5", "", 0, ""});
        } else {
            commit_dst(dst_vreg, "t5");
        }
        return;
    }
    struct Inst { int op, dst, s1, s2; } plan[8];
    int cnt = 0;
    if (!divided) {
        if (value == 0) {
            plan[cnt++] = {0, 3, 0, 0};
        } else {
            plan[cnt++] = {5, 2, value, 0};
            plan[cnt++] = {7, 3, 2, 1};
        }
    } else {
        if (value == 0) {
            plan[cnt++] = {7, 3, 1, 0};
        } else if (value == 1) {
            plan[cnt++] = {0, 3, 1, 0};
        } else if (value == -1) {
            plan[cnt++] = {1, 3, 0, 1};
        } else if (value == -2147483648) {
            plan[cnt++] = {5, 2, value, 0};
            plan[cnt++] = {7, 3, 1, 2};
        } else {
            unsigned int ad = (value < 0) ? (0u - static_cast<unsigned int>(value)) : static_cast<unsigned int>(value);
            if ((ad & (ad - 1)) == 0) {
                int k = __builtin_ctz(ad);
                if (k == 1) {
                    plan[cnt++] = {3, 2, 1, 31};
                    plan[cnt++] = {0, 3, 1, 2};
                    plan[cnt++] = {4, 3, 3, 1};
                } else {
                    plan[cnt++] = {4, 2, 1, 31};
                    plan[cnt++] = {3, 2, 2, 32 - k};
                    plan[cnt++] = {0, 3, 1, 2};
                    plan[cnt++] = {4, 3, 3, k};
                }
                if (value < 0) {
                    plan[cnt++] = {1, 3, 0, 3};
                }
            } else {
                auto calc_magic = [](int d) -> std::pair<int, int> {
                    int p = 31;
                    unsigned int ad = (d < 0) ? (0u - static_cast<unsigned int>(d)) : static_cast<unsigned int>(d);
                    unsigned int t = 0x80000000u + (static_cast<unsigned int>(d) >> 31);
                    unsigned int anc = t - 1 - t % ad;
                    unsigned int q1 = 0x80000000u / anc, r1 = 0x80000000u - q1 * anc;
                    unsigned int q2 = 0x80000000u / ad,  r2 = 0x80000000u - q2 * ad;
                    unsigned int delta;
                    do {
                        ++p;
                        q1 <<= 1; r1 <<= 1;
                        if (r1 >= anc) { ++q1; r1 -= anc; }
                        q2 <<= 1; r2 <<= 1;
                        if (r2 >= ad)  { ++q2; r2 -= ad;  }
                        delta = ad - r2;
                    } while (q1 < delta || (q1 == delta && r1 == 0));
                    unsigned int mu = q2 + 1;
                    if (d < 0) mu = 0u - mu;
                    return {static_cast<int>(mu), p - 32};
                };
                auto [M, s] = calc_magic(value);
                plan[cnt++] = {5, 2, M, 0};
                plan[cnt++] = {6, 3, 1, 2};
                if (value > 0 && M < 0) {
                    plan[cnt++] = {4, 3, 3, 32};
                    plan[cnt++] = {0, 3, 3, 1};
                    if (s > 0) plan[cnt++] = {4, 3, 3, s};
                } else if (value < 0 && M > 0) {
                    plan[cnt++] = {4, 3, 3, 32};
                    plan[cnt++] = {1, 3, 3, 1};
                    if (s > 0) plan[cnt++] = {4, 3, 3, s};
                } else {
                    plan[cnt++] = {4, 3, 3, 32 + s};
                }
                plan[cnt++] = {3, 2, 3, 31};
                plan[cnt++] = {0, 3, 3, 2};
            }
        }
    }
    const char* regs[] = {"zero", r_in.c_str(), "t5", "t6"};
    for (int i = 0; i < cnt; ++i) {
        auto [op, dst, s1, s2] = plan[i];
        std::string out_reg = (i == cnt - 1 && dst_in_phys) ? rd : regs[dst];
        if (op == 0) {
            insts.push_back({"addw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 1) {
            insts.push_back({"subw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 2) {
            insts.push_back({"slliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 3) {
            insts.push_back({"srliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 4) {
            insts.push_back({"srai", out_reg, regs[s1], "", s2, ""});
        } else if (op == 5) {
            insts.push_back({"li", out_reg, "", "", s1, ""});
        } else if (op == 6) {
            insts.push_back({"mul", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 7) {
            insts.push_back({"divw", out_reg, regs[s1], regs[s2], 0, ""});
        }
    }
    if (!dst_in_phys) {
        insts.push_back({"sext.w", "t5", "t6", "", 0, ""});
        commit_dst(dst_vreg, "t5");
    }
}

void CodeGen::gen_rem(const std::string& dst_vreg, const std::string& var_op, int value, bool divided, bool non_neg) {
    std::string r_in = use_operand(var_op, "t5");
    std::string rd = def_operand(dst_vreg, "t5");
    bool dst_in_phys = alloc.reg_map.count(dst_vreg) > 0;
    if (divided && non_neg && value != 0 && value != 1 && value != -1 && value != (1 << 31)) {
        unsigned int ad = (value < 0) ? (0u - static_cast<unsigned int>(value)) : static_cast<unsigned int>(value);
        if ((ad & (ad - 1)) == 0) {
            int mask = static_cast<int>(ad - 1);
            if (mask <= 2047) {
                insts.push_back({"andi", rd, r_in, "", mask, ""});
            } else {
                insts.push_back({"li", "t6", "", "", mask, ""});
                insts.push_back({"and", rd, r_in, "t6", 0, ""});
            }
            commit_dst(dst_vreg, "t5");
            return;
        }
    }
    if (r_in == "t5") {
        if (!divided && value == 0) {
            insts.push_back({"li", "t5", "", "", 0, ""});
        } else {
            insts.push_back({"li", "t6", "", "", value, ""});
            if (divided) {
                insts.push_back({"remw", "t5", "t5", "t6", 0, ""});
            } else {
                insts.push_back({"remw", "t5", "t6", "t5", 0, ""});
            }
        }
        if (dst_in_phys) {
            insts.push_back({"sext.w", rd, "t5", "", 0, ""});
        } else {
            commit_dst(dst_vreg, "t5");
        }
        return;
    }
    struct Inst { int op, dst, s1, s2; } plan[10];
    int cnt = 0;
    if (!divided) {
        if (value == 0) {
            plan[cnt++] = {0, 3, 0, 0};
        } else {
            plan[cnt++] = {5, 2, value, 0};
            plan[cnt++] = {9, 3, 2, 1};
        }
    } else {
        if (value == 0) {
            plan[cnt++] = {9, 3, 1, 0};
        } else if (value == 1 || value == -1) {
            plan[cnt++] = {0, 3, 0, 0};
        } else {
            unsigned int ad = (value < 0) ? (0u - static_cast<unsigned int>(value)) : static_cast<unsigned int>(value);
            if ((ad & (ad - 1)) == 0) {
                int k = __builtin_ctz(ad);
                if (k == 1) {
                    plan[cnt++] = {3, 2, 1, 31};
                    plan[cnt++] = {0, 3, 1, 2};
                    plan[cnt++] = {8, 3, 3, -2};
                } else if (k <= 11) {
                    plan[cnt++] = {4, 2, 1, 31};
                    plan[cnt++] = {3, 2, 2, 32 - k};
                    plan[cnt++] = {0, 3, 1, 2};
                    plan[cnt++] = {8, 3, 3, -(1 << k)};
                } else {
                    plan[cnt++] = {4, 2, 1, 31};
                    plan[cnt++] = {3, 2, 2, 32 - k};
                    plan[cnt++] = {0, 3, 1, 2};
                    plan[cnt++] = {3, 3, 3, k};
                    plan[cnt++] = {2, 3, 3, k};
                }
                plan[cnt++] = {1, 3, 1, 3};
            } else {
                auto calc_magic = [](int d) -> std::pair<int, int> {
                    int p = 31;
                    unsigned int ad = (d < 0) ? (0u - static_cast<unsigned int>(d)) : static_cast<unsigned int>(d);
                    unsigned int t = 0x80000000u + (static_cast<unsigned int>(d) >> 31);
                    unsigned int anc = t - 1 - t % ad;
                    unsigned int q1 = 0x80000000u / anc, r1 = 0x80000000u - q1 * anc;
                    unsigned int q2 = 0x80000000u / ad,  r2 = 0x80000000u - q2 * ad;
                    unsigned int delta;
                    do {
                        ++p;
                        q1 <<= 1; r1 <<= 1;
                        if (r1 >= anc) { ++q1; r1 -= anc; }
                        q2 <<= 1; r2 <<= 1;
                        if (r2 >= ad)  { ++q2; r2 -= ad;  }
                        delta = ad - r2;
                    } while (q1 < delta || (q1 == delta && r1 == 0));
                    unsigned int mu = q2 + 1;
                    if (d < 0) mu = 0u - mu;
                    return {static_cast<int>(mu), p - 32};
                };
                auto [M, s] = calc_magic(value);
                plan[cnt++] = {5, 2, M, 0};
                plan[cnt++] = {6, 3, 1, 2};
                if (value > 0 && M < 0) {
                    plan[cnt++] = {0, 3, 3, 1};
                } else if (value < 0 && M > 0) {
                    plan[cnt++] = {1, 3, 3, 1};
                }
                if (s > 0) {
                    plan[cnt++] = {4, 3, 3, s};
                }
                plan[cnt++] = {3, 2, 3, 31};
                plan[cnt++] = {0, 3, 3, 2};
                plan[cnt++] = {5, 2, value, 0};
                plan[cnt++] = {7, 3, 3, 2};
                plan[cnt++] = {1, 3, 1, 3};
            }
        }
    }
    const char* regs[] = {"zero", r_in.c_str(), "t5", "t6"};
    for (int i = 0; i < cnt; ++i) {
        auto [op, dst, s1, s2] = plan[i];
        std::string out_reg = (i == cnt - 1 && dst_in_phys) ? rd : regs[dst];
        if (op == 0) {
            insts.push_back({"addw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 1) {
            insts.push_back({"subw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 2) {
            insts.push_back({"slliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 3) {
            insts.push_back({"srliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 4) {
            insts.push_back({"srai", out_reg, regs[s1], "", s2, ""});
        } else if (op == 5) {
            insts.push_back({"li", out_reg, "", "", s1, ""});
        } else if (op == 6) {
            insts.push_back({"mul", regs[dst], regs[s1], regs[s2], 0, ""});
            insts.push_back({"srai", out_reg, regs[dst], "", 32, ""});
        } else if (op == 7) {
            insts.push_back({"mulw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 8) {
            insts.push_back({"andi", out_reg, regs[s1], "", s2, ""});
        } else if (op == 9) {
            insts.push_back({"remw", out_reg, regs[s1], regs[s2], 0, ""});
        }
    }
    if (!dst_in_phys) {
        insts.push_back({"sext.w", "t5", "t6", "", 0, ""});
        commit_dst(dst_vreg, "t5");
    }
}
