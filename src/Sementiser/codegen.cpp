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
    epilogue_label = ".Lret_" + fn.name;
    for (const auto& p : fn.params) get_slot(p);
    for (const auto& inst : fn.insts) {
        if (inst.op == IROp::ALLOCA && var_reg.count(inst.dst)) continue;
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
        asm_out << "  sd ra, " << (frame_size - 8) << "(sp)\n";
        asm_out << "  sd s0, " << (frame_size - 16) << "(sp)\n";
        asm_out << "  addi s0, sp, " << frame_size << "\n";
    } else {
        asm_out << "  li t3, -" << frame_size << "\n";
        asm_out << "  add sp, sp, t3\n";
        asm_out << "  li t3, " << frame_size << "\n";
        asm_out << "  add t3, sp, t3\n";
        asm_out << "  sd ra, -8(t3)\n";
        asm_out << "  sd s0, -16(t3)\n";
        asm_out << "  mv s0, t3\n";
    }
    for (size_t i = 0; i < used_s_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        if (off >= -2048) {
            asm_out << "  sd " << used_s_regs[i] << ", " << off << "(s0)\n";
        } else {
            asm_out << "  li t3, " << off << "\n  add t3, s0, t3\n  sd " << used_s_regs[i] << ", 0(t3)\n";
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
    for (const auto& inst : fn.insts) {
        gen_inst(inst);
    }
    asm_out << epilogue_label << ":\n";
    for (size_t i = 0; i < used_s_regs.size(); ++i) {
        int off = -24 - static_cast<int>(i) * 8;
        if (off >= -2048) {
            asm_out << "  ld " << used_s_regs[i] << ", " << off << "(s0)\n";
        } else {
            asm_out << "  li t3, " << off << "\n  add t3, s0, t3\n  ld " << used_s_regs[i] << ", 0(t3)\n";
        }
    }
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
                    asm_out << "  li t2, " << std::stoi(inst.src1) + std::stoi(inst.src2) << "\n";
                } else {
                    load_operand(inst.src2, "t1");
                    auto value = std::stoi(inst.src1);
                    if (value < -2048 || value > 2047) {
                        load_operand(inst.src1, "t0");
                        asm_out << "  add t2, t0, t1\n";
                    } else {
                        asm_out << "  addi t2, " << "t1, " << value << "\n";
                    }
                }
            } else {
                if (inst.src2[0] != '%') {
                    load_operand(inst.src1, "t0");
                    auto value = std::stoi(inst.src2);
                    if (value < -2048 || value > 2047) {
                        load_operand(inst.src2, "t1");
                        asm_out << "  add t2, t0, t1\n";
                    } else {
                        asm_out << "  addi t2, " << "t0, " << value << "\n";
                    }
                } else {
                    load_operand(inst.src1, "t0");
                    load_operand(inst.src2, "t1");
                    asm_out << "  add t2, t0, t1\n";
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::SUB: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    asm_out << "  li t2, " << std::stoi(inst.src1) - std::stoi(inst.src2) << "\n";
                } else {
                    load_operand(inst.src2, "t1");
                    auto value = std::stoi(inst.src1);
                    if (-value < -2048 || -value > 2047) {
                        load_operand(inst.src2, "t1");
                        asm_out << "  sub t2, t0, t1\n";
                    } else {
                        asm_out << "  addi t2, " << "t1, " << -value << "\n";
                        asm_out << "  sub t2, x0, t2\n";
                    }
                }
            } else {
                if (inst.src2[0] != '%') {
                    load_operand(inst.src1, "t0");
                    auto value = std::stoi(inst.src2);
                    if (-value < -2048 || -value > 2047) {
                        load_operand(inst.src2, "t1");
                        asm_out << "  sub t2, t0, t1\n";
                    } else {
                        asm_out << "  addi t2, " << "t0, " << -value << "\n";
                    }
                } else {
                    load_operand(inst.src1, "t0");
                    load_operand(inst.src2, "t1");
                    asm_out << "  sub t2, t0, t1\n";
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::MUL: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    asm_out << "  li t2, " << std::stoi(inst.src1) * std::stoi(inst.src2) << "\n";
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
                    asm_out << "  mul t2, t0, t1\n";
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::SDIV: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    asm_out << "  li t2, " << std::stoi(inst.src1) / std::stoi(inst.src2) << "\n";
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
                    asm_out << "  div t2, t0, t1\n";
                }
            }
            emit_sw("t2", get_slot(inst.dst));
            break;
        }
        case IROp::SREM: {
            if (inst.src1[0] != '%') {
                if (inst.src2[0] != '%') {
                    asm_out << "  li t2, " << std::stoi(inst.src1) % std::stoi(inst.src2) << "\n";
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
                    asm_out << "  rem t2, t0, t1\n";
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
                asm_out << "  add " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
            } else if (op == 1) {
                asm_out << "  sub " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
            } else if (op == 2) {
                asm_out << "  slli " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
            }
        }
    } else {
        asm_out << "  li t1, " << value << "\n";
        asm_out << "  mul t2, t0, t1\n";
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
                    plan[cnt++] = {0, 3, 3, 1};
                } else if (value < 0 && M > 0) {
                    plan[cnt++] = {1, 3, 3, 1};
                }
                if (s > 0) {
                    plan[cnt++] = {4, 3, 3, s};
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
            asm_out << "  add " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
        } else if (op == 1) {
            asm_out << "  sub " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
        } else if (op == 2) {
            asm_out << "  slli " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
        } else if (op == 3) {
            asm_out << "  srli " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
        } else if (op == 4) {
            asm_out << "  srai " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
        } else if (op == 5) {
            asm_out << "  li " << regs[dst] << ", " << s1 << "\n";
        } else if (op == 6) {
            asm_out << "  mulh " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
        } else if (op == 7) {
            asm_out << "  div " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
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
            asm_out << "  add " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
        } else if (op == 1) {
            asm_out << "  sub " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
        } else if (op == 2) {
            asm_out << "  slli " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
        } else if (op == 3) {
            asm_out << "  srliw " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
        } else if (op == 4) {
            asm_out << "  srai " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
        } else if (op == 5) {
            asm_out << "  li " << regs[dst] << ", " << s1 << "\n";
        } else if (op == 6) {
            asm_out << "  mul " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
            asm_out << "  srai " << regs[dst] << ", " << regs[dst] << ", 32\n";
        } else if (op == 7) {
            asm_out << "  mul " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
        } else if (op == 8) {
            asm_out << "  andi " << regs[dst] << ", " << regs[s1] << ", " << s2 << "\n";
        } else if (op == 9) {
            asm_out << "  remw " << regs[dst] << ", " << regs[s1] << ", " << regs[s2] << "\n";
        }
    }
}