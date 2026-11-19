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
        insts.push_back(AsmInst{ "li", "t5", "", "", offset, "" });
        insts.push_back(AsmInst{ "add", "t5", base, "t5", 0, "" });
        insts.push_back(AsmInst{ "lw", rd, "t5", "", 0, "" });
    }
}

void CodeGen::emit_sw(const std::string& rs, int offset, const std::string& base) {
    if (offset >= -2048 && offset <= 2047) {
        insts.push_back(AsmInst{ "sw", "", base, rs, offset, "" });
    } else {
        insts.push_back(AsmInst{ "li", "t5", "", "", offset, "" });
        insts.push_back(AsmInst{ "add", "t5", base, "t5", 0, "" });
        insts.push_back(AsmInst{ "sw", "", "t5", rs, 0, "" });
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
    // for (const auto& fn : funcs) {
    //     gen_func(fn);
    // }
    // optimize_cg(insts);
    optimize_cg(debug_insts);
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
    asm_out << "\n";
    for (const auto& i : debug_insts) {
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
        case IROp::MOV:
            load_operand(inst.src1, "t0");
            emit_sw("t0", get_slot(inst.dst));
            break;
        default:
            exit(0);
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

RegAllocResult CodeGen::linear_scan_allocate(std::vector<LiveInterval>& intervals) {
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
        res.spill_bytes += 4;
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
    return res;
}

void CodeGen::debug_emit_ld(const std::string& rd, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        debug_insts.push_back({"ld", rd, "s0", "", offset, ""});
    } else {
        debug_insts.push_back({"li", "t6", "", "", offset, ""});
        debug_insts.push_back({"add", "t6", "s0", "t6", 0, ""});
        debug_insts.push_back({"ld", rd, "t6", "", 0, ""});
    }
}

void CodeGen::debug_emit_lw(const std::string& rd, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        debug_insts.push_back({"lw", rd, "s0", "", offset, ""});
    } else {
        debug_insts.push_back({"li", "t6", "", "", offset, ""});
        debug_insts.push_back({"add", "t6", "s0", "t6", 0, ""});
        debug_insts.push_back({"lw", rd, "t6", "", 0, ""});
    }
}

void CodeGen::debug_emit_sd(const std::string& rs, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        debug_insts.push_back({"sd", "", "s0", rs, offset, ""});
    } else {
        debug_insts.push_back({"li", "t6", "", "", offset, ""});
        debug_insts.push_back({"add", "t6", "t6", "s0", 0, ""});
        debug_insts.push_back({"sd", "", "t6", rs, 0, ""});
    }
}

void CodeGen::debug_emit_sw(const std::string& rs, int offset) {
    if (offset >= -2048 && offset <= 2047) {
        debug_insts.push_back({"sw", "", "s0", rs, offset, ""});
    } else {
        debug_insts.push_back({"li", "t6", "", "", offset, ""});
        debug_insts.push_back({"add", "t6", "t6", "s0", 0, ""});
        debug_insts.push_back({"sw", "", "t6", rs, 0, ""});
    }
}

void CodeGen::debug_generate(const std::vector<IRBlock>& blocks, const IRFunc& func) {
    int max_stack_args = 0;
    for (const auto& blk : blocks) {
        for (const auto& inst : blk.insts) {
            if (inst.op == IROp::CALL && inst.args.size() > 8) {
                max_stack_args = std::max(max_stack_args, static_cast<int>(inst.args.size()) - 8);
            }
        }
    }
    int raw_size = 16 + 8 * static_cast<int>(alloc.used_callee_regs.size()) + alloc.spill_bytes + 8 * max_stack_args;
    int frame_size = (raw_size + 15) & ~15;
    std::string epilogue_label = "Lret_" + func.name;
    debug_insts.push_back(AsmInst{ "globl", "", "", "", 0, func.name });
    debug_insts.push_back(AsmInst{ "label", "", "", "", 0, func.name });
    if (frame_size <= 2048) {
        debug_insts.push_back(AsmInst{ "addi", "sp", "sp", "", -frame_size, "" });
        debug_insts.push_back(AsmInst{ "sd", "", "sp", "ra", frame_size - 8, "" });
        debug_insts.push_back(AsmInst{ "sd", "", "sp", "s0", frame_size - 16, "" });
        debug_insts.push_back(AsmInst{ "addi", "s0", "sp", "", frame_size, "" });
    } else {
        debug_insts.push_back(AsmInst{ "li", "t3", "", "", -frame_size, "" });
        debug_insts.push_back(AsmInst{ "add", "sp", "sp", "t3", 0, "" });
        debug_insts.push_back(AsmInst{ "li", "t3", "", "", frame_size, "" });
        debug_insts.push_back(AsmInst{ "add", "t3", "sp", "t3", 0, "" });
        debug_insts.push_back(AsmInst{ "sd", "", "t3", "ra", -8, "" });
        debug_insts.push_back(AsmInst{ "sd", "", "t3", "s0", -16, "" });
        debug_insts.push_back(AsmInst{ "mv", "s0", "t3", "", 0, "" });
    }
    for (size_t i = 0; i < alloc.used_callee_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        debug_emit_sd(alloc.used_callee_regs[i], off);
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
            debug_emit_sw(arg_reg, alloc.spill_slot[p]);
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
                debug_insts.push_back({"mv", reg_param_moves[i].first, reg_param_moves[i].second, "", 0, ""});
                reg_param_moves.erase(reg_param_moves.begin() + i);
                progress = true;
                break;
            }
        }
        if (!progress) {
            std::string cycle_reg = reg_param_moves[0].second;
            debug_insts.push_back({"mv", "t5", cycle_reg, "", 0, ""});
            for (auto& mv : reg_param_moves) {
                if (mv.second == cycle_reg) mv.second = "t5";
            }
        }
    }
    for (size_t i = 8; i < func.params.size(); ++i) {
        std::string p = func.params[i];
        if (!p.empty() && p[0] != '%') p = "%" + p;
        int caller_stack_off = static_cast<int>(i - 8) * 8;
        if (alloc.reg_map.count(p)) {
            debug_emit_lw(alloc.reg_map[p], caller_stack_off);
        } else if (alloc.spill_slot.count(p)) {
            debug_emit_lw("t5", caller_stack_off);
            debug_emit_sw("t5", alloc.spill_slot[p]);
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
            debug_insts.push_back({"label", "", "", "", 0, "." + func.name + "_" + block.label});
        }
        for (size_t i = 0; i < block.insts.size(); ++i) {
            const auto& inst = block.insts[i];
            if (i + 1 < block.insts.size() && is_icmp(inst.op) &&
                block.insts[i + 1].op == IROp::BR_COND &&
                block.insts[i + 1].src1 == inst.dst && vreg_uses[inst.dst] == 1) {
                const auto& br = block.insts[i + 1];
                std::string r1 = use_operand(inst.src1, "t5");
                std::string r2 = use_operand(inst.src2, "t6");
                std::string L_true = "." + func.name + "_" + br.dst;
                std::string L_false = "." + func.name + "_" + br.src2;
                switch (inst.op) {
                    case IROp::ICMP_EQ: debug_insts.push_back({"beq", "", r1, r2, 0, L_true}); break;
                    case IROp::ICMP_NE: debug_insts.push_back({"bne", "", r1, r2, 0, L_true}); break;
                    case IROp::ICMP_SLT: debug_insts.push_back({"blt", "", r1, r2, 0, L_true}); break;
                    case IROp::ICMP_SGE: debug_insts.push_back({"bge", "", r1, r2, 0, L_true}); break;
                    case IROp::ICMP_SGT: debug_insts.push_back({"blt", "", r2, r1, 0, L_true}); break;
                    case IROp::ICMP_SLE: debug_insts.push_back({"bge", "", r2, r1, 0, L_true}); break;
                    default: break;
                }
                debug_insts.push_back({"j", "", "", "", 0, L_false});
                ++i;
                continue;
            }
            switch (inst.op) {
                case IROp::ADD:
                case IROp::SUB: {
                    std::string r1 = use_operand(inst.src1, "t5");
                    std::string r2 = use_operand(inst.src2, "t6");
                    std::string rd = def_operand(inst.dst, "t5");
                    std::string riscv_op = (inst.op == IROp::ADD) ? "addw" : "subw";
                    debug_insts.push_back({riscv_op, rd, r1, r2, 0, ""});
                    commit_dst(inst.dst, "t5");
                    break;
                }
                case IROp::MUL: {
                    bool c1 = !inst.src1.empty() && inst.src1[0] != '%';
                    bool c2 = !inst.src2.empty() && inst.src2[0] != '%';
                    if (c1 && c2) {
                        int32_t prod = static_cast<int32_t>(std::stoll(inst.src1) * std::stoll(inst.src2));
                        std::string rd = def_operand(inst.dst, "t5");
                        debug_insts.push_back({"li", rd, "", "", prod, ""});
                        commit_dst(inst.dst, "t5");
                    } else if (c2) {
                        debug_gen_mul(inst.dst, inst.src1, std::stoi(inst.src2));
                    } else if (c1) {
                        debug_gen_mul(inst.dst, inst.src2, std::stoi(inst.src1));
                    } else {
                        std::string r1 = use_operand(inst.src1, "t5");
                        std::string r2 = use_operand(inst.src2, "t6");
                        std::string rd = def_operand(inst.dst, "t5");
                        debug_insts.push_back({"mulw", rd, r1, r2, 0, ""});
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
                            debug_insts.push_back({"li", rd, "", "", res, ""});
                            commit_dst(inst.dst, "t5");
                            break;
                        }
                    }
                    if (c2) {
                        int val = static_cast<int>(std::stoll(inst.src2));
                        if (is_div) debug_gen_div(inst.dst, inst.src1, val, true);
                        else debug_gen_rem(inst.dst, inst.src1, val, true);
                    } else if (c1) {
                        int val = static_cast<int>(std::stoll(inst.src1));
                        if (is_div) debug_gen_div(inst.dst, inst.src2, val, false);
                        else debug_gen_rem(inst.dst, inst.src2, val, false);
                    } else {
                        std::string r1 = use_operand(inst.src1, "t5");
                        std::string r2 = use_operand(inst.src2, "t6");
                        std::string rd = def_operand(inst.dst, "t5");
                        debug_insts.push_back({is_div ? "divw" : "remw", rd, r1, r2, 0, ""});
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
                        debug_insts.push_back({"slt", rd, r1, r2, 0, ""});
                    } else if (inst.op == IROp::ICMP_SGT) {
                        debug_insts.push_back({"slt", rd, r2, r1, 0, ""});
                    } else if (inst.op == IROp::ICMP_SLE) {
                        debug_insts.push_back({"slt", rd, r2, r1, 0, ""});
                        debug_insts.push_back({"xori", rd, rd, "", 1, ""});
                    } else if (inst.op == IROp::ICMP_SGE) {
                        debug_insts.push_back({"slt", rd, r1, r2, 0, ""});
                        debug_insts.push_back({"xori", rd, rd, "", 1, ""});
                    } else if (inst.op == IROp::ICMP_EQ) {
                        debug_insts.push_back({"subw", rd, r1, r2, 0, ""});
                        debug_insts.push_back({"seqz", rd, rd, "", 0, ""});
                    } else if (inst.op == IROp::ICMP_NE) {
                        debug_insts.push_back({"subw", rd, r1, r2, 0, ""});
                        debug_insts.push_back({"snez", rd, rd, "", 0, ""});
                    }
                    commit_dst(inst.dst, "t5");
                    break;
                }
                case IROp::MOV: {
                    if (!inst.src1.empty() && inst.src1[0] != '%') {
                        std::string rd = def_operand(inst.dst, "t5");
                        debug_insts.push_back({"li", rd, "", "", std::stoi(inst.src1), ""});
                        commit_dst(inst.dst, "t5");
                    } else {
                        std::string rs = use_operand(inst.src1, "t5");
                        std::string rd = def_operand(inst.dst, "t5");
                        if (rd != rs) {
                            debug_insts.push_back({"sext.w", rd, rs, "", 0, ""});
                        }
                        commit_dst(inst.dst, "t5");
                    }
                    break;
                }
                case IROp::JMP: {
                    debug_insts.push_back({"j", "", "", "", 0, "." + func.name + "_" + inst.dst});
                    break;
                }
                case IROp::BR_COND: {
                    std::string r_cond = use_operand(inst.src1, "t5");
                    debug_insts.push_back({"bnez", "", r_cond, "", 0, "." + func.name + "_" + inst.dst});
                    debug_insts.push_back({"j", "", "", "", 0, "." + func.name + "_" + inst.src2});
                    break;
                }
                case IROp::CALL: {
                    for (size_t k = 8; k < inst.args.size(); ++k) {
                        std::string val_reg = use_operand(inst.args[k], "t5");
                        int sp_offset = static_cast<int>(k - 8) * 8;
                        if (sp_offset <= 2047) {
                            debug_insts.push_back({"sw", "", "sp", val_reg, sp_offset, ""});
                        } else {
                            debug_insts.push_back({"li", "t6", "", "", sp_offset, ""});
                            debug_insts.push_back({"add", "t6", "sp", "t6", 0, ""});
                            debug_insts.push_back({"sw", "", "t6", val_reg, 0, ""});
                        }
                    }
                    std::vector<std::pair<std::string, std::string>> arg_moves;
                    for (size_t k = 0; k < inst.args.size() && k < 8; ++k) {
                        arg_moves.push_back({"a" + std::to_string(k), inst.args[k]});
                    }
                    emit_parallel_moves(arg_moves);
                    debug_insts.push_back({"call", "", "", "", 0, inst.src1});
                    if (!inst.dst.empty()) {
                        if (alloc.reg_map.count(inst.dst)) {
                            if (alloc.reg_map[inst.dst] != "a0") {
                                debug_insts.push_back({"mv", alloc.reg_map[inst.dst], "a0", "", 0, ""});
                            }
                        } else if (alloc.spill_slot.count(inst.dst)) {
                            debug_emit_sw("a0", alloc.spill_slot[inst.dst]);
                        }
                    }
                    break;
                }
                case IROp::RET: {
                    if (!inst.src1.empty()) {
                        if (inst.src1[0] != '%') {
                            debug_insts.push_back({"li", "a0", "", "", std::stoi(inst.src1), ""});
                        } else if (alloc.reg_map.count(inst.src1)) {
                            if (alloc.reg_map[inst.src1] != "a0") {
                                debug_insts.push_back({"mv", "a0", alloc.reg_map[inst.src1], "", 0, ""});
                            }
                        } else {
                            debug_emit_lw("a0", alloc.spill_slot[inst.src1]);
                        }
                    }
                    debug_insts.push_back({"j", "", "", "", 0, "." + func.name + "_" + epilogue_label});
                    break;
                }
                default:
                    break;
            }
        }
    }
    debug_insts.push_back(AsmInst{ "label", "", "", "", 0, "." + func.name + "_" + epilogue_label });
    for (size_t i = 0; i < alloc.used_callee_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        debug_emit_ld(alloc.used_callee_regs[i], off);
    }
    debug_insts.push_back(AsmInst{ "ld", "ra", "s0", "", -8, "" });
    debug_insts.push_back(AsmInst{ "ld", "s0", "s0", "", -16, "" });
    if (frame_size <= 2047) {
        debug_insts.push_back(AsmInst{ "addi", "sp", "sp", "", frame_size, "" });
    } else {
        debug_insts.push_back(AsmInst{ "li", "t3", "", "", frame_size, "" });
        debug_insts.push_back(AsmInst{ "add", "sp", "sp", "t3", 0, "" });
    }
    debug_insts.push_back(AsmInst{ "ret", "", "", "", 0, "" });
}

std::string CodeGen::use_operand(const std::string& op, const std::string& scratch) {
    if (op.empty()) return "";
    if (op[0] != '%') {
        debug_insts.push_back({"li", scratch, "", "", std::stoi(op), ""});
        return scratch;
    }
    if (alloc.reg_map.count(op)) {
        return alloc.reg_map[op];
    }
    debug_emit_lw(scratch, alloc.spill_slot[op]);
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
        debug_emit_sw(scratch, alloc.spill_slot[dst]);
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
                debug_insts.push_back({"mv", reg_moves[i].dst_reg, reg_moves[i].src_reg, "", 0, ""});
                reg_moves.erase(reg_moves.begin() + i);
                progress = true;
                break;
            }
        }
        if (!progress) {
            std::string cycle_reg = reg_moves[0].src_reg;
            debug_insts.push_back({"mv", "t5", cycle_reg, "", 0, ""});
            for (auto& task : reg_moves) {
                if (task.src_reg == cycle_reg) task.src_reg = "t5";
            }
        }
    }
    for (const auto& task : other_moves) {
        if (task.raw_src[0] != '%') {
            debug_insts.push_back({"li", task.dst_reg, "", "", std::stoi(task.raw_src), ""});
        } else {
            debug_emit_lw(task.dst_reg, alloc.spill_slot[task.raw_src]);
        }
    }
}

void CodeGen::debug_gen_mul(const std::string& dst_vreg, const std::string& var_op, int value) {
    std::string r_in = use_operand(var_op, "t5");
    std::string rd = def_operand(dst_vreg, "t5");
    if (value == 0) {
        debug_insts.push_back({"li", rd, "", "", 0, ""});
        commit_dst(dst_vreg, "t5");
        return;
    }
    if (value == 1) {
        if (rd != r_in) {
            debug_insts.push_back({"sext.w", rd, r_in, "", 0, ""});
        }
        commit_dst(dst_vreg, "t5");
        return;
    }
    if (value == -1) {
        debug_insts.push_back({"subw", rd, "zero", r_in, 0, ""});
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
                debug_insts.push_back({"addw", out_reg, regs[s1], regs[s2], 0, ""});
            } else if (op == 1) {
                debug_insts.push_back({"subw", out_reg, regs[s1], regs[s2], 0, ""});
            } else if (op == 2) {
                debug_insts.push_back({"slliw", out_reg, regs[s1], "", s2, ""});
            }
        }
        if (!dst_in_phys) {
            debug_insts.push_back({"sext.w", "t5", "t6", "", 0, ""});
            commit_dst(dst_vreg, "t5");
        }
    } else {
        debug_insts.push_back({"li", "t6", "", "", value, ""});
        if (dst_in_phys) {
            debug_insts.push_back({"mulw", rd, r_in, "t6", 0, ""});
        } else {
            debug_insts.push_back({"mulw", "t5", r_in, "t6", 0, ""});
            commit_dst(dst_vreg, "t5");
        }
    }
}

void CodeGen::debug_gen_div(const std::string& dst_vreg, const std::string& var_op, int value, bool divided) {
    std::string r_in = use_operand(var_op, "t5");
    std::string rd = def_operand(dst_vreg, "t5");
    bool dst_in_phys = alloc.reg_map.count(dst_vreg) > 0;
    if (r_in == "t5") {
        if (!divided && value == 0) {
            debug_insts.push_back({"li", "t5", "", "", 0, ""});
        } else {
            debug_insts.push_back({"li", "t6", "", "", value, ""});
            if (divided) {
                debug_insts.push_back({"divw", "t5", "t5", "t6", 0, ""});
            } else {
                debug_insts.push_back({"divw", "t5", "t6", "t5", 0, ""});
            }
        }
        if (dst_in_phys) {
            debug_insts.push_back({"sext.w", rd, "t5", "", 0, ""});
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
            debug_insts.push_back({"addw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 1) {
            debug_insts.push_back({"subw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 2) {
            debug_insts.push_back({"slliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 3) {
            debug_insts.push_back({"srliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 4) {
            debug_insts.push_back({"srai", out_reg, regs[s1], "", s2, ""});
        } else if (op == 5) {
            debug_insts.push_back({"li", out_reg, "", "", s1, ""});
        } else if (op == 6) {
            debug_insts.push_back({"mul", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 7) {
            debug_insts.push_back({"divw", out_reg, regs[s1], regs[s2], 0, ""});
        }
    }
    if (!dst_in_phys) {
        debug_insts.push_back({"sext.w", "t5", "t6", "", 0, ""});
        commit_dst(dst_vreg, "t5");
    }
}

void CodeGen::debug_gen_rem(const std::string& dst_vreg, const std::string& var_op, int value, bool divided) {
    std::string r_in = use_operand(var_op, "t5");
    std::string rd = def_operand(dst_vreg, "t5");
    bool dst_in_phys = alloc.reg_map.count(dst_vreg) > 0;
    if (r_in == "t5") {
        if (!divided && value == 0) {
            debug_insts.push_back({"li", "t5", "", "", 0, ""});
        } else {
            debug_insts.push_back({"li", "t6", "", "", value, ""});
            if (divided) {
                debug_insts.push_back({"remw", "t5", "t5", "t6", 0, ""});
            } else {
                debug_insts.push_back({"remw", "t5", "t6", "t5", 0, ""});
            }
        }
        if (dst_in_phys) {
            debug_insts.push_back({"sext.w", rd, "t5", "", 0, ""});
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
            debug_insts.push_back({"addw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 1) {
            debug_insts.push_back({"subw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 2) {
            debug_insts.push_back({"slliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 3) {
            debug_insts.push_back({"srliw", out_reg, regs[s1], "", s2, ""});
        } else if (op == 4) {
            debug_insts.push_back({"srai", out_reg, regs[s1], "", s2, ""});
        } else if (op == 5) {
            debug_insts.push_back({"li", out_reg, "", "", s1, ""});
        } else if (op == 6) {
            debug_insts.push_back({"mul", regs[dst], regs[s1], regs[s2], 0, ""});
            debug_insts.push_back({"srai", out_reg, regs[dst], "", 32, ""});
        } else if (op == 7) {
            debug_insts.push_back({"mulw", out_reg, regs[s1], regs[s2], 0, ""});
        } else if (op == 8) {
            debug_insts.push_back({"andi", out_reg, regs[s1], "", s2, ""});
        } else if (op == 9) {
            debug_insts.push_back({"remw", out_reg, regs[s1], regs[s2], 0, ""});
        }
    }
    if (!dst_in_phys) {
        debug_insts.push_back({"sext.w", "t5", "t6", "", 0, ""});
        commit_dst(dst_vreg, "t5");
    }
}