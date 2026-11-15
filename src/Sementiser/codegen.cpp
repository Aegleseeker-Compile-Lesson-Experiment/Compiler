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
        asm_out << "  lw " << rd << ", " << offset << "(" << base << ")\n";
    } else {
        asm_out << "  li t3, " << offset << "\n";
        asm_out << "  add t3, " << base << ", t3\n";
        asm_out << "  lw " << rd << ", 0(t3)\n";
    }
}

void CodeGen::emit_sw(const std::string& rs, int offset, const std::string& base) {
    if (offset >= -2048 && offset <= 2047) {
        asm_out << "  sw " << rs << ", " << offset << "(" << base << ")\n";
    } else {
        asm_out << "  li t3, " << offset << "\n";
        asm_out << "  add t3, " << base << ", t3\n";
        asm_out << "  sw " << rs << ", 0(t3)\n";
    }
}

void CodeGen::load_operand(const std::string& op, const std::string& reg) {
    if (op[0] == '%') {
        emit_lw(reg, get_slot(op));
    } else {
        asm_out << "  li " << reg << ", " << op << "\n";
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
    return asm_out.str();
}

void CodeGen::gen_func(const IRFunc& fn) {
    stack_offset.clear();
    current_offset = -16;
    epilogue_label = ".Lret_" + fn.name;
    for (const auto& p : fn.params) get_slot(p);
    for (const auto& inst : fn.insts) {
        if (!inst.dst.empty() && inst.dst[0] == '%') {
            get_slot(inst.dst);
        }
    }
    int raw_size = -current_offset;
    frame_size = (raw_size + 15) & ~15;
    asm_out << "  .globl " << fn.name << "\n";
    asm_out << "  .type " << fn.name << ", @function\n";
    asm_out << fn.name << ":\n";
    if (frame_size <= 2048) {
        asm_out << "  addi sp, sp, -" << frame_size << "\n";
    } else {
        asm_out << "  li t3, -" << frame_size << "\n";
        asm_out << "  add sp, sp, t3\n";
    }
    // emit_sw("ra", frame_size - 4, "sp");
    // emit_sw("s0", frame_size - 8, "sp");
    asm_out << "  sd ra, " << (frame_size - 8) << "(sp)\n";
    asm_out << "  sd s0, " << (frame_size - 16) << "(sp)\n";
    if (frame_size <= 2047) {
        asm_out << "  addi s0, sp, " << frame_size << "\n";
    } else {
        asm_out << "  li t3, " << frame_size << "\n";
        asm_out << "  add s0, sp, t3\n";
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
    for (const auto& inst : fn.insts) {
        gen_inst(inst);
    }
    asm_out << epilogue_label << ":\n";
    // emit_lw("ra", -4, "s0");
    // emit_lw("s0", -8, "s0");
    asm_out << "  ld ra, -8(s0)\n";
    asm_out << "  ld s0, -16(s0)\n";
    if (frame_size <= 2047) {
        asm_out << "  addi sp, sp, " << frame_size << "\n";
    } else {
        asm_out << "  li t3, " << frame_size << "\n";
        asm_out << "  add sp, sp, t3\n";
    }
    asm_out << "  ret\n\n";
}

void CodeGen::gen_inst(const IRInst& inst) {
    switch (inst.op) {
        case IROp::ALLOCA:
            break;
        case IROp::LOAD: {
            emit_lw("t0", get_slot(inst.src1));
            emit_sw("t0", get_slot(inst.dst));
            break;
        }
        case IROp::STORE: {
            load_operand(inst.src1, "t0");
            emit_sw("t0", get_slot(inst.src2));
            break;
        }
        case IROp::ADD:
        case IROp::SUB:
        case IROp::MUL:
        case IROp::SDIV:
        case IROp::SREM: {
            load_operand(inst.src1, "t0");
            load_operand(inst.src2, "t1");
            if      (inst.op == IROp::ADD)  asm_out << "  add t2, t0, t1\n";
            else if (inst.op == IROp::SUB)  asm_out << "  sub t2, t0, t1\n";
            else if (inst.op == IROp::MUL)  asm_out << "  mul t2, t0, t1\n";
            else if (inst.op == IROp::SDIV) asm_out << "  div t2, t0, t1\n";
            else if (inst.op == IROp::SREM) asm_out << "  rem t2, t0, t1\n";
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
                asm_out << "  xor t2, t0, t1\n";
                asm_out << "  seqz t2, t2\n";
            } else if (inst.op == IROp::ICMP_NE) {
                asm_out << "  xor t2, t0, t1\n";
                asm_out << "  snez t2, t2\n";
            } else if (inst.op == IROp::ICMP_SLT) {
                asm_out << "  slt t2, t0, t1\n";
            } else if (inst.op == IROp::ICMP_SGT) {
                asm_out << "  slt t2, t1, t0\n";
            } else if (inst.op == IROp::ICMP_SLE) {
                asm_out << "  slt t2, t1, t0\n";
                asm_out << "  xori t2, t2, 1\n";
            } else if (inst.op == IROp::ICMP_SGE) {
                asm_out << "  slt t2, t0, t1\n";
                asm_out << "  xori t2, t2, 1\n";
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::LABEL:
            asm_out << "." << inst.dst << ":\n";
            break;
        case IROp::JMP:
            asm_out << "  j ." << inst.dst << "\n";
            break;
        case IROp::BR_COND:
            load_operand(inst.src1, "t0");
            asm_out << "  bnez t0, ." << inst.dst << "\n";
            asm_out << "  j ." << inst.src2 << "\n";
            break;
        case IROp::CALL: {
            int extra_args = std::max(0, static_cast<int>(inst.args.size()) - 8);
            int arg_stack_bytes = ((extra_args * 4) + 15) & ~15;
            if (arg_stack_bytes > 0) {
                asm_out << "  addi sp, sp, -" << arg_stack_bytes << "\n";
                for (int i = 8; i < static_cast<int>(inst.args.size()); ++i) {
                    load_operand(inst.args[i], "t0");
                    emit_sw("t0", (i - 8) * 4, "sp");
                }
            }
            for (int i = 0; i < std::min(8, static_cast<int>(inst.args.size())); ++i) {
                load_operand(inst.args[i], "a" + std::to_string(i));
            }
            asm_out << "  call " << inst.src1 << "\n";
            if (arg_stack_bytes > 0) {
                asm_out << "  addi sp, sp, " << arg_stack_bytes << "\n";
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
            asm_out << "  j " << epilogue_label << "\n";
            break;
    }
}
