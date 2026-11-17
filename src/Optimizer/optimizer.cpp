#include "optimizer.h"

bool is_int_literal(const std::string& s) {
    if (s.empty()) return false;
    size_t start = (s[0] == '-' || s[0] == '+') ? 1 : 0;
    if (start == s.size()) return false;
    for (size_t i = start; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

void optimize_ir(IRFunc& fn) {
    /* Const value
     * Delete unused variables
     * Delete dead circles
    */
    std::unordered_map<std::string, std::string> const_map;
    auto resolve = [&](std::string& op) {
        while (const_map.count(op)) op = const_map[op];
    };
    for (auto& inst : fn.insts) {
        resolve(inst.src1);
        resolve(inst.src2);
        for (auto& a : inst.args) resolve(a);
        if (is_int_literal(inst.src1) && is_int_literal(inst.src2)) {
            int a = std::stoi(inst.src1);
            int b = std::stoi(inst.src2);
            bool folded = true;
            int res = 0;
            switch (inst.op) {
                case IROp::ADD: res = a + b; break;
                case IROp::SUB: res = a - b; break;
                case IROp::MUL: res = a * b; break;
                case IROp::SDIV: if (b != 0) res = a / b; else folded = false; break;
                case IROp::SREM: if (b != 0) res = a % b; else folded = false; break;
                case IROp::ICMP_EQ: res = (a == b); break;
                case IROp::ICMP_NE: res = (a != b); break;
                case IROp::ICMP_SLT: res = (a < b);  break;
                case IROp::ICMP_SGT: res = (a > b);  break;
                case IROp::ICMP_SLE: res = (a <= b); break;
                case IROp::ICMP_SGE: res = (a >= b); break;
                default: folded = false; break;
            }
            if (folded) {
                const_map[inst.dst] = std::to_string(res);
            }
        }
        if (inst.op == IROp::BR_COND && is_int_literal(inst.src1)) {
            int cond = std::stoi(inst.src1);
            std::string target = (cond != 0) ? inst.dst : inst.src2;
            inst = {IROp::JMP, target, "", "", {}};
        }
    }
    bool cfg_changed = true;
    while (cfg_changed) {
        cfg_changed = false;
        std::unordered_set<std::string> referenced_labels;
        for (const auto& inst : fn.insts) {
            if (inst.op == IROp::JMP) {
                referenced_labels.insert(inst.dst);
            } else if (inst.op == IROp::BR_COND) {
                referenced_labels.insert(inst.dst);
                referenced_labels.insert(inst.src2);
            }
        }
        std::vector<IRInst> cleaned;
        bool dead_region = false;
        for (size_t i = 0; i < fn.insts.size(); ++i) {
            const auto& inst = fn.insts[i];
            if (inst.op == IROp::LABEL) {
                bool prev_is_term = (i > 0) && (fn.insts[i - 1].op == IROp::JMP || fn.insts[i - 1].op == IROp::RET);
                if (prev_is_term && !referenced_labels.count(inst.dst)) {
                    dead_region = true;
                    cfg_changed = true;
                    continue;
                } else {
                    dead_region = false;
                }
            }
            if (dead_region) continue;
            cleaned.push_back(inst);
            if (inst.op == IROp::JMP || inst.op == IROp::RET) {
                dead_region = true;
            }
        }
        fn.insts = std::move(cleaned);
    }
    auto run_dce = [&]() {
        bool changed = true;
        while (changed) {
            changed = false;
            std::unordered_map<std::string, int> load_cnt;
            std::unordered_map<std::string, int> use_cnt;
            for (const auto& inst : fn.insts) {
                if (inst.op == IROp::LOAD) load_cnt[inst.src1]++;
                if (!inst.src1.empty() && inst.src1[0] == '%') use_cnt[inst.src1]++;
                if (!inst.src2.empty() && inst.src2[0] == '%') use_cnt[inst.src2]++;
                for (const auto& a : inst.args) {
                    if (!a.empty() && a[0] == '%') use_cnt[a]++;
                }
            }
            std::vector<IRInst> next_insts;
            next_insts.reserve(fn.insts.size());
            for (const auto& inst : fn.insts) {
                if (inst.op == IROp::ALLOCA && load_cnt[inst.dst] == 0) {
                    changed = true;
                    continue;
                }
                if (inst.op == IROp::STORE && !inst.src2.empty() && inst.src2[0] == '%' && load_cnt[inst.src2] == 0) {
                    changed = true;
                    continue;
                }
                bool is_pure = (inst.op == IROp::LOAD || inst.op == IROp::ADD || inst.op == IROp::SUB ||
                                inst.op == IROp::MUL  || inst.op == IROp::SDIV || inst.op == IROp::SREM ||
                                inst.op == IROp::ICMP_EQ || inst.op == IROp::ICMP_NE ||
                                inst.op == IROp::ICMP_SLT || inst.op == IROp::ICMP_SGT ||
                                inst.op == IROp::ICMP_SLE || inst.op == IROp::ICMP_SGE);
                if (is_pure && use_cnt[inst.dst] == 0) {
                    changed = true;
                    continue;
                }
                next_insts.push_back(inst);
            }
            fn.insts = std::move(next_insts);
        }
    };
    run_dce();
    bool loop_removed = true;
    while (loop_removed) {
        loop_removed = false;
        for (size_t c = 0; c + 4 < fn.insts.size(); ++c) {
            if (fn.insts[c].op != IROp::LABEL) continue;
            std::string l_cond = fn.insts[c].dst;
            size_t b = 0, e = 0;
            bool valid_shape = false;
            for (size_t i = c + 1; i < fn.insts.size(); ++i) {
                auto op = fn.insts[i].op;
                if (op == IROp::CALL || op == IROp::RET) break;
                if (op == IROp::BR_COND && b == 0) {
                    if (i + 1 < fn.insts.size() && fn.insts[i + 1].op == IROp::LABEL && fn.insts[i + 1].dst == fn.insts[i].dst) {
                        b = ++i;
                    } else break;
                } else if (op == IROp::JMP && b != 0) {
                    if (fn.insts[i].dst == l_cond && i + 1 < fn.insts.size() &&
                        fn.insts[i + 1].op == IROp::LABEL && fn.insts[i + 1].dst == fn.insts[b - 1].src2) {
                        e = i + 1;
                        valid_shape = true;
                    }
                    break;
                } else if (op == IROp::LABEL || op == IROp::JMP || op == IROp::BR_COND) {
                    break;
                }
            }
            if (!valid_shape) continue;
            std::unordered_set<std::string> loop_stores;
            std::unordered_set<std::string> cond_loads;
            for (size_t i = c + 1; i < b - 1; ++i) {
                if (fn.insts[i].op == IROp::LOAD) cond_loads.insert(fn.insts[i].src1);
            }
            bool has_progress = false;
            std::unordered_map<std::string, std::string> loaded_from;
            for (size_t i = b + 1; i < e - 1; ++i) {
                const auto& inst = fn.insts[i];
                if (inst.op == IROp::LOAD) loaded_from[inst.dst] = inst.src1;
                if (inst.op == IROp::STORE) {
                    loop_stores.insert(inst.src2);
                    if (cond_loads.count(inst.src2)) {
                        for (size_t k = b + 1; k < i; ++k) {
                            if (fn.insts[k].dst == inst.src1 &&
                                (fn.insts[k].op == IROp::ADD || fn.insts[k].op == IROp::SUB) &&
                                loaded_from[fn.insts[k].src1] == inst.src2 &&
                                is_int_literal(fn.insts[k].src2) && std::stoi(fn.insts[k].src2) != 0) {
                                has_progress = true;
                            }
                        }
                    }
                }
            }
            if (!has_progress || loop_stores.empty()) continue;
            bool used_outside = false;
            for (size_t i = 0; i < fn.insts.size(); ++i) {
                if (i >= c && i <= e) continue;
                if (fn.insts[i].op == IROp::LOAD && loop_stores.count(fn.insts[i].src1)) {
                    used_outside = true;
                    break;
                }
            }
            if (used_outside) continue;
            std::string l_end = fn.insts[e].dst;
            fn.insts.erase(fn.insts.begin() + c + 1, fn.insts.begin() + e);
            fn.insts.insert(fn.insts.begin() + c + 1, {IROp::JMP, l_end, "", "", {}});
            run_dce();
            loop_removed = true;
            break;
        }
    }
}

void optimize_cg(std::vector<AsmInst>& insts) {
    /* 
     * 
     * 
    */
   auto get_next = [&](size_t index) {
        for (size_t i = index + 1; i < insts.size(); ++i) {
            if (insts[i].op != "") {
                return i;
            }
        }
        return (size_t)0;
   };
    bool changed;
    do {
        changed = false;
        for (size_t i = 0; i < insts.size(); ++i) {
            auto& inst = insts[i];
            if (insts[i].op == "") continue;
            auto next_index = get_next(i);
            if (next_index == 0) continue;
            auto& next_inst = insts[next_index];
            if (((inst.op == "sw" && next_inst.op == "lw") || (inst.op == "sd" && next_inst.op == "ld")) &&
                inst.imm == next_inst.imm && inst.rs1 == next_inst.rs1) {
                if (inst.rs2 == next_inst.rd) {
                    next_inst = AsmInst {"", "", "", "", 0, ""};
                } else {
                    next_inst = AsmInst{ "sext.w", next_inst.rd, inst.rs2, "", 0, "" };
                }
                changed = true;
            }
            if ((inst.op == "sext.w" || inst.op == "mv") && (next_inst.op == "sw" || next_inst.op == "sd") && inst.rd == next_inst.rs2
                && inst.rd[0] == 't' && inst.rs1[0] == 't') {
                next_inst = AsmInst {next_inst.op, "", next_inst.rs1, inst.rs1, next_inst.imm, ""};
                inst = AsmInst {"", "", "", "", 0, ""};
                changed = true;
            }
            if (inst.op == "j" && inst.label == next_inst.label) {
                inst = AsmInst {"", "", "", "", 0, ""};
                changed = true;
            }
            if ((inst.op == "sext.w" || inst.op == "mv") && inst.op == next_inst.op &&
                inst.rd == next_inst.rd && inst.rs1 == next_inst.rs1 && inst.rs2 == next_inst.rs2) {
                next_inst = AsmInst {"", "", "", "", 0, ""};
                changed = true;
            }
        }
    } while (changed);
}
