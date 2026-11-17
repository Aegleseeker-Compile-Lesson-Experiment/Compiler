#include "codegen.h"

int CodeGen::get_slot(const std::string& name) {
    if (stack_offset.find(name) == stack_offset.end()) {
        current_offset -= 4;
        stack_offset[name] = current_offset;
    }
    return stack_offset[name];
}

void CodeGen::emit_lw(const std::string& rd, int offset, const std::string& base) {
    if (offset >= -2048 && offset <= 2047) {
        insts.push_back(AsmInst{ "lw", rd, base, "", offset, "" });
    } else {
        insts.push_back(AsmInst{ "li", "t3", "", "", offset, "" });
        insts.push_back(AsmInst{ "add", "t3", base, "t3", 0, "" });
        insts.push_back(AsmInst{ "lw", rd, "t3", "", 0, "" });
    }
}

void CodeGen::emit_sw(const std::string& rs, int offset, const std::string& base) {
    if (offset >= -2048 && offset <= 2047) {
        insts.push_back(AsmInst{ "sw", "", base, rs, offset, "" });
    } else {
        insts.push_back(AsmInst{ "li", "t3", "", "", offset, "" });
        insts.push_back(AsmInst{ "add", "t3", base, "t3", 0, "" });
        insts.push_back(AsmInst{ "lw", rs, "t3", "", 0, "" });
    }
}

void CodeGen::emit_branch(const std::string& op, const std::string& rs1, const std::string& rs2, const std::string& label) {
    insts.push_back(AsmInst{ op, "", rs1, rs2, 0, "." + label });
}

void CodeGen::emit_jmp(const std::string& label) {
    insts.push_back(AsmInst{ "j", "", "", "", 0, "." + label });
}

void CodeGen::load_operand(const std::string& op, const std::string& reg) {
    if (op[0] == '%') {
        emit_lw(reg, get_slot(op));
    } else {
        insts.push_back(AsmInst{ "li", reg, "", "", std::stoi(op), "" });
    }
}

std::string CodeGen::generate(const std::vector<IRFunc>& funcs) {
    asm_out.str("");
    asm_out << "  .text\n";
    asm_out << "  .globl _start\n";
    asm_out << "_start:\n";
    asm_out << "  call main\n";
    asm_out << "  li a7, 93\n";
    asm_out << "  ecall\n";
    for (const auto& fn : funcs) {
        gen_func(fn);
    }
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

void CodeGen::gen_func(const IRFunc& fn) {
    stack_offset.clear();
    var_reg.clear();
    used_s_regs.clear();
    for (const auto& inst : fn.insts) {
        if (inst.op == IROp::ALLOCA && used_s_regs.size() < 11) {
            std::string sreg = "s" + std::to_string(used_s_regs.size() + 1);
            var_reg[inst.dst] = sreg;
            used_s_regs.push_back(sreg);
        }
    }
    current_offset = -16 - 8 * static_cast<int>(used_s_regs.size());
    epilogue_label = "Lret_" + fn.name;
    for (const auto& p : fn.params) get_slot(p);
    for (const auto& inst : fn.insts) {
        if (inst.op == IROp::ALLOCA && var_reg.count(inst.dst)) continue;
        if (!inst.dst.empty() && inst.dst[0] == '%') {
            get_slot(inst.dst);
        }
    }
    int raw_size = -current_offset;
    frame_size = (raw_size + 15) & ~15;
    insts.push_back(AsmInst{ "globl", "", "", "", 0, fn.name });
    insts.push_back(AsmInst{ "label", "", "", "", 0, fn.name });
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
    for (size_t i = 0; i < used_s_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        if (off >= -2048) {
            insts.push_back(AsmInst{ "sd", "", "s0", used_s_regs[i], off, "" });
        } else {
            insts.push_back(AsmInst{ "li", "t3", "", "", off, "" });
            insts.push_back(AsmInst{ "add", "t3", "s0", "t3", 0, "" });
            insts.push_back(AsmInst{ "sd", "", "t3", used_s_regs[i], 0, "" });
        }
    }
    for (size_t i = 0; i < fn.params.size(); ++i) {
        int slot = get_slot(fn.params[i]);
        if (i < 8) {
            emit_sw("a" + std::to_string(i), slot);
        } else {
            int caller_arg_offset = (i - 8) * 4;
            emit_lw("t0", caller_arg_offset, "s0");
            emit_sw("t0", slot);
        }
    }
    std::unordered_map<std::string, int> reg_uses;
    for (const auto& inst : fn.insts) {
        if (!inst.src1.empty() && inst.src1[0] == '%') reg_uses[inst.src1]++;
        if (!inst.src2.empty() && inst.src2[0] == '%') reg_uses[inst.src2]++;
        for (const auto& a : inst.args) if (!a.empty() && a[0] == '%') reg_uses[a]++;
    }
    for (size_t i = 0; i < fn.insts.size(); ++i) {
        const auto& inst = fn.insts[i];
        if (i + 1 < fn.insts.size() && (inst.op == IROp::ICMP_EQ || inst.op == IROp::ICMP_NE || 
            inst.op == IROp::ICMP_SGE || inst.op == IROp::ICMP_SGT || inst.op == IROp::ICMP_SLE ||
            inst.op == IROp::ICMP_SLT) && fn.insts[i + 1].op == IROp::BR_COND &&
            fn.insts[i + 1].src1 == inst.dst && reg_uses[inst.dst] == 1) {
            const auto& br = fn.insts[i + 1];
            load_operand(inst.src1, "t0");
            load_operand(inst.src2, "t1");
            auto L_true = br.dst;
            auto L_false = br.src2;
            switch (inst.op) {
                case IROp::ICMP_EQ: emit_branch("beq", "t0", "t1", L_true); break;
                case IROp::ICMP_NE: emit_branch("bne", "t0", "t1", L_true); break;
                case IROp::ICMP_SLT: emit_branch("blt", "t0", "t1", L_true); break;
                case IROp::ICMP_SGE: emit_branch("bge", "t0", "t1", L_true); break;
                case IROp::ICMP_SGT: emit_branch("blt", "t1", "t0", L_true); break;
                case IROp::ICMP_SLE: emit_branch("bge", "t1", "t0", L_true); break;
                default: break;
            }
            emit_jmp(L_false);
            ++i;
            continue;
        }
        gen_inst(inst);
    }
    insts.push_back(AsmInst{ "label", "", "", "", 0, "." + epilogue_label });
    for (size_t i = 0; i < used_s_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        if (off >= -2048) {
            insts.push_back(AsmInst{ "ld", used_s_regs[i], "s0", "", off, "" });
        } else {
            insts.push_back(AsmInst{ "li", "t3", "", "", off, "" });
            insts.push_back(AsmInst{ "add", "t3", "s0", "t3", 0, "" });
            insts.push_back(AsmInst{ "ld", used_s_regs[i], "", "t3", 0, "" });
        }
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

void CodeGen::gen_inst(const IRInst& inst) {
    switch (inst.op) {
        case IROp::ALLOCA:
            break;
        case IROp::LOAD: {
            if (var_reg.count(inst.src1)) {
                emit_sw(var_reg[inst.src1], get_slot(inst.dst));
            } else {
                emit_lw("t0", get_slot(inst.src1));
                emit_sw("t0", get_slot(inst.dst));
            }
            break;
        }
        case IROp::STORE: {
            if (var_reg.count(inst.src2)) {
                load_operand(inst.src1, var_reg[inst.src2]);
            } else {
                load_operand(inst.src1, "t0");
                emit_sw("t0", get_slot(inst.src2));
            }
            break;
        }
        case IROp::ADD: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    insts.push_back(AsmInst{ "li", "t2", "", "", std::stoi(inst.src1) + std::stoi(inst.src2), "" });
                } else {
                    load_operand(inst.src2, "t1");
                    auto value = std::stoi(inst.src1);
                    if (value < -2048 || value > 2047) {
                        load_operand(inst.src1, "t0");
                        insts.push_back(AsmInst{ "add", "t2", "t0", "t1", 0, "" });
                    } else {
                        insts.push_back(AsmInst{ "addi", "t2", "t1", "", value, "" });
                    }
                }
            } else {
                if (inst.src2[0] != '%') {
                    load_operand(inst.src1, "t0");
                    auto value = std::stoi(inst.src2);
                    if (value < -2048 || value > 2047) {
                        load_operand(inst.src2, "t1");
                        insts.push_back(AsmInst{ "add", "t2", "t0", "t1", 0, "" });
                    } else {
                        insts.push_back(AsmInst{ "addi", "t2", "t0", "", value, "" });
                    }
                } else {
                    load_operand(inst.src1, "t0");
                    load_operand(inst.src2, "t1");
                    insts.push_back(AsmInst{ "add", "t2", "t0", "t1", 0, "" });
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::SUB: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    insts.push_back(AsmInst{ "li", "t2", "", "", std::stoi(inst.src1) - std::stoi(inst.src2), "" });
                } else {
                    load_operand(inst.src2, "t1");
                    auto value = std::stoi(inst.src1);
                    if (-value < -2048 || -value > 2047) {
                        load_operand(inst.src2, "t1");
                        insts.push_back(AsmInst{ "sub", "t2", "t0", "t1", 0, "" });
                    } else {
                        insts.push_back(AsmInst{ "addi", "t2", "t1", "", -value, "" });
                        insts.push_back(AsmInst{ "sub", "t2", "x0", "t2", 0, "" });
                    }
                }
            } else {
                if (inst.src2[0] != '%') {
                    load_operand(inst.src1, "t0");
                    auto value = std::stoi(inst.src2);
                    if (-value < -2048 || -value > 2047) {
                        load_operand(inst.src2, "t1");
                        insts.push_back(AsmInst{ "sub", "t2", "t0", "t1", 0, "" });
                    } else {
                        insts.push_back(AsmInst{ "addi", "t2", "t0", "", -value, "" });
                    }
                } else {
                    load_operand(inst.src1, "t0");
                    load_operand(inst.src2, "t1");
                    insts.push_back(AsmInst{ "sub", "t2", "t0", "t1", 0, "" });
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::MUL: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    insts.push_back(AsmInst{ "li", "t2", "", "", std::stoi(inst.src1) * std::stoi(inst.src2), "" });
                } else {
                    load_operand(inst.src2, "t0");
                    auto value = std::stoi(inst.src1);
                    gen_mul(value);
                }
            } else {
                if (inst.src2[0] != '%') {
                    load_operand(inst.src1, "t0");
                    auto value = std::stoi(inst.src2);
                    gen_mul(value);
                } else {
                    load_operand(inst.src1, "t0");
                    load_operand(inst.src2, "t1");
                    insts.push_back(AsmInst{ "mul", "t2", "t0", "t1", 0, "" });
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::SDIV: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    insts.push_back(AsmInst{ "li", "t2", "", "", std::stoi(inst.src1) / std::stoi(inst.src2), "" });
                } else {
                    load_operand(inst.src2, "t1");
                    auto value = std::stoi(inst.src1);
                    gen_div(value, false);
                }
            } else {
                if (inst.src2[0] != '%') {
                    load_operand(inst.src1, "t0");
                    auto value = std::stoi(inst.src2);
                    gen_div(value, true);
                } else {
                    load_operand(inst.src1, "t0");
                    load_operand(inst.src2, "t1");
                    insts.push_back(AsmInst{ "div", "t2", "t0", "t1", 0, "" });
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::SREM: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    insts.push_back(AsmInst{ "li", "t2", "", "", std::stoi(inst.src1) % std::stoi(inst.src2), "" });
                } else {
                    load_operand(inst.src2, "t1");
                    auto value = std::stoi(inst.src1);
                    gen_rem(value, false);
                }
            } else {
                if (inst.src2[0] != '%') {
                    load_operand(inst.src1, "t0");
                    auto value = std::stoi(inst.src2);
                    gen_rem(value, true);
                } else {
                    load_operand(inst.src1, "t0");
                    load_operand(inst.src2, "t1");
                    insts.push_back(AsmInst{ "rem", "t2", "t0", "t1", 0, "" });
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::ICMP_EQ:
        case IROp::ICMP_NE:
        case IROp::ICMP_SLT:
        case IROp::ICMP_SGT:
        case IROp::ICMP_SLE:
        case IROp::ICMP_SGE: {
            load_operand(inst.src1, "t0");
            load_operand(inst.src2, "t1");
            if (inst.op == IROp::ICMP_EQ) {
                insts.push_back(AsmInst{ "xor", "t2", "t0", "t1", 0, "" });
                insts.push_back(AsmInst{ "seqz", "t2", "t2", "", 0, "" });
            } else if (inst.op == IROp::ICMP_NE) {
                insts.push_back(AsmInst{ "xor", "t2", "t0", "t1", 0, "" });
                insts.push_back(AsmInst{ "snez", "t2", "t2", "", 0, "" });
            } else if (inst.op == IROp::ICMP_SLT) {
                insts.push_back(AsmInst{ "slt", "t2", "t0", "t1", 0, "" });
            } else if (inst.op == IROp::ICMP_SGT) {
                insts.push_back(AsmInst{ "slt", "t2", "t1", "t0", 0, "" });
            } else if (inst.op == IROp::ICMP_SLE) {
                insts.push_back(AsmInst{ "slt", "t2", "t1", "t0", 0, "" });
                insts.push_back(AsmInst{ "xori", "t2", "t2", "", 1, "" });
            } else if (inst.op == IROp::ICMP_SGE) {
                insts.push_back(AsmInst{ "slt", "t2", "t0", "t1", 0, "" });
                insts.push_back(AsmInst{ "xori", "t2", "t2", "", 1, "" });
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::LABEL:
            insts.push_back(AsmInst{ "label", "", "", "", 0, "." + inst.dst });
            break;
        case IROp::JMP:
            emit_jmp(inst.dst);
            break;
        case IROp::BR_COND:
            load_operand(inst.src1, "t0");
            insts.push_back(AsmInst{ "bnez", "", "t0", "", 0, "." + inst.dst });
            emit_jmp(inst.src2);
            break;
        case IROp::CALL: {
            int extra_args = std::max(0, static_cast<int>(inst.args.size()) - 8);
            int arg_stack_bytes = ((extra_args * 4) + 15) & ~15;
            if (arg_stack_bytes > 0) {
                insts.push_back(AsmInst{ "addi", "sp", "sp", "", -arg_stack_bytes, "" });
                for (int i = 8; i < static_cast<int>(inst.args.size()); ++i) {
                    load_operand(inst.args[i], "t0");
                    emit_sw("t0", (i - 8) * 4, "sp");
                }
            }
            for (int i = 0; i < std::min(8, static_cast<int>(inst.args.size())); ++i) {
                load_operand(inst.args[i], "a" + std::to_string(i));
            }
            insts.push_back(AsmInst{ "call", "", "", "", 0, inst.src1 });
            if (arg_stack_bytes > 0) {
                insts.push_back(AsmInst{ "addi", "sp", "sp", "", arg_stack_bytes, "" });
            }
            if (!inst.dst.empty()) {
                emit_sw("a0", get_slot(inst.dst));
            }
            break;
        }
        case IROp::RET:
            if (!inst.src1.empty()) {
                load_operand(inst.src1, "a0");
            }
            emit_jmp(epilogue_label);
            break;
    }
}

void CodeGen::gen_mul(const int& value) {
    constexpr bool allow_t1 = true;
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
    if (step_cnt > 0) {
        const char* regs[] = {"zero", "t0", "t1", "t2"};
        for (int i = 0; i < step_cnt; ++i) {
            auto [op, dst, s1, s2] = plan[i];
            if (op == 0) {
                insts.push_back(AsmInst{ "add", regs[dst], regs[s1], regs[s2], 0, "" });
            } else if (op == 1) {
                insts.push_back(AsmInst{ "sub", regs[dst], regs[s1], regs[s2], 0, "" });
            } else if (op == 2) {
                insts.push_back(AsmInst{ "slli", regs[dst], regs[s1], "", s2, "" });
            }
        }
    } else {
        insts.push_back(AsmInst{ "li", "t1", "", "", value, "" });
        insts.push_back(AsmInst{ "mul", "t2", "t0", "t1", 0, "" });
    }
}

void CodeGen::gen_div(const int& value, const bool& divided) {
    struct Inst { int op, dst, s1, s2; } plan[8];
    int cnt = 0;
    if (!divided) {
        if (value == 0) {
            plan[cnt++] = {0, 3, 0, 0};
        } else {
            plan[cnt++] = {5, 1, value, 0};
            plan[cnt++] = {7, 3, 1, 2};
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
    const char* regs[] = {"zero", "t0", "t1", "t2"};
    for (int i = 0; i < cnt; ++i) {
        auto [op, dst, s1, s2] = plan[i];
        if (op == 0) {
            insts.push_back(AsmInst{ "add", regs[dst], regs[s1], regs[s2], 0, "" });
        } else if (op == 1) {
            insts.push_back(AsmInst{ "sub", regs[dst], regs[s1], regs[s2], 0, "" });
        } else if (op == 2) {
            insts.push_back(AsmInst{ "slli", regs[dst], regs[s1], "", s2, "" });
        } else if (op == 3) {
            insts.push_back(AsmInst{ "srliw", regs[dst], regs[s1], "", s2, "" });
        } else if (op == 4) {
            insts.push_back(AsmInst{ "srai", regs[dst], regs[s1], "", s2, "" });
        } else if (op == 5) {
            insts.push_back(AsmInst{ "li", regs[dst], "", "", s1, "" });
        } else if (op == 6) {
            insts.push_back(AsmInst{ "mul", regs[dst], regs[s1], regs[s2], 0, "" });
        } else if (op == 7) {
            insts.push_back(AsmInst{ "divw", regs[dst], regs[s1], regs[s2], 0, "" });
        }
    }
}

void CodeGen::gen_rem(const int& value, const bool& divided) {
    struct Inst { int op, dst, s1, s2; } plan[10];
    int cnt = 0;
    if (!divided) {
        if (value == 0) {
            plan[cnt++] = {0, 3, 0, 0};
        } else {
            plan[cnt++] = {5, 1, value, 0};
            plan[cnt++] = {9, 3, 1, 2};
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
    const char* regs[] = {"zero", "t0", "t1", "t2"};
    for (int i = 0; i < cnt; ++i) {
        auto [op, dst, s1, s2] = plan[i];
        if (op == 0) {
            insts.push_back(AsmInst{ "add", regs[dst], regs[s1], regs[s2], 0, "" });
        } else if (op == 1) {
            insts.push_back(AsmInst{ "sub", regs[dst], regs[s1], regs[s2], 0, "" });
        } else if (op == 2) {
            insts.push_back(AsmInst{ "slli", regs[dst], regs[s1], "", s2, "" });
        } else if (op == 3) {
            insts.push_back(AsmInst{ "srliw", regs[dst], regs[s1], "", s2, "" });
        } else if (op == 4) {
            insts.push_back(AsmInst{ "srai", regs[dst], regs[s1], "", s2, "" });
        } else if (op == 5) {
            insts.push_back(AsmInst{ "li", regs[dst], "", "", s1, "" });
        } else if (op == 6) {
            insts.push_back(AsmInst{ "mul", regs[dst], regs[s1], regs[s2], 0, "" });
            insts.push_back(AsmInst{ "srai", regs[dst], regs[dst], "", 32, "" });
        } else if (op == 7) {
            insts.push_back(AsmInst{ "mul", regs[dst], regs[s1], regs[s2], 0, "" });
        } else if (op == 8) {
            insts.push_back(AsmInst{ "andi", regs[dst], regs[s1], "", s2, "" });
        } else if (op == 9) {
            insts.push_back(AsmInst{ "remw", regs[dst], regs[s1], regs[s2], 0, "" });
        }
    }
}
