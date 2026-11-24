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

void optimize_ir(std::vector<IRInst>& insts) {
    /* Const value
     * Delete unused variables
     * Delete dead circles
    */
    std::unordered_map<std::string, std::string> const_map;
    auto resolve = [&](std::string& op) {
        while (const_map.count(op)) op = const_map[op];
    };
    for (auto& inst : insts) {
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
            inst = {IROp::JMP, target, "", "", {}, {}};
        }
        if (inst.op == IROp::MOV && is_int_literal(inst.src1)) {
            const_map[inst.dst] = inst.src1;
        }
    }
    bool cfg_changed = true;
    while (cfg_changed) {
        cfg_changed = false;
        std::unordered_set<std::string> referenced_labels;
        for (const auto& inst : insts) {
            if (inst.op == IROp::JMP) {
                referenced_labels.insert(inst.dst);
            } else if (inst.op == IROp::BR_COND) {
                referenced_labels.insert(inst.dst);
                referenced_labels.insert(inst.src2);
            }
        }
        std::vector<IRInst> cleaned;
        bool dead_region = false;
        for (size_t i = 0; i < insts.size(); ++i) {
            const auto& inst = insts[i];
            if (inst.op == IROp::LABEL) {
                bool prev_is_term = (i > 0) && (insts[i - 1].op == IROp::JMP || insts[i - 1].op == IROp::RET);
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
        insts = std::move(cleaned);
    }
    auto run_dce = [&]() {
        bool changed = true;
        while (changed) {
            changed = false;
            std::unordered_map<std::string, int> load_cnt;
            std::unordered_map<std::string, int> use_cnt;
            for (const auto& inst : insts) {
                if (inst.op == IROp::LOAD) load_cnt[inst.src1]++;
                if (!inst.src1.empty() && inst.src1[0] == '%') use_cnt[inst.src1]++;
                if (!inst.src2.empty() && inst.src2[0] == '%') use_cnt[inst.src2]++;
                for (const auto& a : inst.args) {
                    if (!a.empty() && a[0] == '%') use_cnt[a]++;
                }
            }
            std::vector<IRInst> next_insts;
            next_insts.reserve(insts.size());
            for (const auto& inst : insts) {
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
                                inst.op == IROp::ICMP_SLE || inst.op == IROp::ICMP_SGE ||
                                inst.op == IROp::MOV || inst.op == IROp::PHI);
                if (is_pure && use_cnt[inst.dst] == 0) {
                    changed = true;
                    continue;
                }
                next_insts.push_back(inst);
            }
            insts = std::move(next_insts);
        }
    };
    run_dce();
    bool loop_removed = true;
    while (loop_removed) {
        loop_removed = false;
        for (size_t c = 0; c + 4 < insts.size(); ++c) {
            if (insts[c].op != IROp::LABEL) continue;
            std::string l_cond = insts[c].dst;
            size_t b = 0, e = 0;
            bool valid_shape = false;
            for (size_t i = c + 1; i < insts.size(); ++i) {
                auto op = insts[i].op;
                if (op == IROp::CALL || op == IROp::RET) break;
                if (op == IROp::BR_COND && b == 0) {
                    if (i + 1 < insts.size() && insts[i + 1].op == IROp::LABEL && insts[i + 1].dst == insts[i].dst) {
                        b = ++i;
                    } else break;
                } else if (op == IROp::JMP && b != 0) {
                    if (insts[i].dst == l_cond && i + 1 < insts.size() &&
                        insts[i + 1].op == IROp::LABEL && insts[i + 1].dst == insts[b - 1].src2) {
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
                if (insts[i].op == IROp::LOAD) cond_loads.insert(insts[i].src1);
            }
            bool has_progress = false;
            std::unordered_map<std::string, std::string> loaded_from;
            for (size_t i = b + 1; i < e - 1; ++i) {
                const auto& inst = insts[i];
                if (inst.op == IROp::LOAD) loaded_from[inst.dst] = inst.src1;
                if (inst.op == IROp::STORE) {
                    loop_stores.insert(inst.src2);
                    if (cond_loads.count(inst.src2)) {
                        for (size_t k = b + 1; k < i; ++k) {
                            if (insts[k].dst == inst.src1 &&
                                (insts[k].op == IROp::ADD || insts[k].op == IROp::SUB) &&
                                loaded_from[insts[k].src1] == inst.src2 &&
                                is_int_literal(insts[k].src2) && std::stoi(insts[k].src2) != 0) {
                                has_progress = true;
                            }
                        }
                    }
                }
            }
            if (!has_progress || loop_stores.empty()) continue;
            bool used_outside = false;
            for (size_t i = 0; i < insts.size(); ++i) {
                if (i >= c && i <= e) continue;
                if (insts[i].op == IROp::LOAD && loop_stores.count(insts[i].src1)) {
                    used_outside = true;
                    break;
                }
            }
            if (used_outside) continue;
            std::string l_end = insts[e].dst;
            insts.erase(insts.begin() + c + 1, insts.begin() + e);
            insts.insert(insts.begin() + c + 1, {IROp::JMP, l_end, "", "", {}, {}});
            run_dce();
            loop_removed = true;
            break;
        }
    }
}

void optimize_cg(std::vector<AsmInst>& insts) {
    /* Store-Load forwarding
     * Redundancy folding between registers
     * Eliminate redirects by using direct links
     * Self-copying instruction
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

void eliminate_tail_recursion(IRFunc& fn) {
    bool has_tail_call = false;
    for (size_t i = 0; i + 1 < fn.insts.size(); ++i) {
        const auto& c = fn.insts[i];
        const auto& r = fn.insts[i + 1];
        if (c.op == IROp::CALL && c.src1 == fn.name && r.op == IROp::RET && c.dst == r.src1) {
            has_tail_call = true;
            break;
        }
    }
    if (!has_tail_call) return;
    std::unordered_set<std::string> alloca_set;
    std::unordered_map<std::string, std::string> param_to_slot;
    size_t body_start = 0;
    for (size_t i = 0; i < fn.insts.size(); ++i) {
        const auto& inst = fn.insts[i];
        if (inst.op == IROp::ALLOCA) {
            alloca_set.insert(inst.dst);
            body_start = i + 1;
        } else if (inst.op == IROp::STORE && alloca_set.count(inst.src2)) {
            param_to_slot[inst.src1] = inst.src2;
            body_start = i + 1;
        } else {
            break;
        }
    }
    std::vector<std::string> param_slots(fn.params.size());
    std::vector<IRInst> extra_allocas;
    std::vector<IRInst> extra_init_stores;
    for (size_t k = 0; k < fn.params.size(); ++k) {
        std::string p = fn.params[k];
        if (!p.empty() && p[0] != '%') p = "%" + p;
        if (param_to_slot.count(p)) {
            param_slots[k] = param_to_slot[p];
        } else {
            std::string slot = "%tre_param_" + std::to_string(k);
            param_slots[k] = slot;
            extra_allocas.push_back({IROp::ALLOCA, slot, "", "", {}, {}});
            extra_init_stores.push_back({IROp::STORE, "", p, slot, {}, {}});
        }
    }
    std::string header_label = "L_tre_header_" + fn.name;
    std::vector<IRInst> new_insts;
    new_insts.reserve(fn.insts.size() + 8);
    new_insts.insert(new_insts.end(), extra_allocas.begin(), extra_allocas.end());
    new_insts.insert(new_insts.end(), fn.insts.begin(), fn.insts.begin() + body_start);
    new_insts.insert(new_insts.end(), extra_init_stores.begin(), extra_init_stores.end());
    new_insts.push_back({IROp::JMP, header_label, "", "", {}, {}});
    new_insts.push_back({IROp::LABEL, header_label, "", "", {}, {}});
    for (size_t i = body_start; i < fn.insts.size(); ++i) {
        if (i + 1 < fn.insts.size()) {
            const auto& c = fn.insts[i];
            const auto& r = fn.insts[i + 1];
            if (c.op == IROp::CALL && c.src1 == fn.name && r.op == IROp::RET && c.dst == r.src1) {
                for (size_t k = 0; k < param_slots.size() && k < c.args.size(); ++k) {
                    new_insts.push_back({IROp::STORE, "", c.args[k], param_slots[k], {}, {}});
                }
                new_insts.push_back({IROp::JMP, header_label, "", "", {}, {}});
                ++i;
                continue;
            }
        }
        new_insts.push_back(fn.insts[i]);
    }

    fn.insts = std::move(new_insts);
}

void run_licm(std::vector<IRBlock>& blocks, std::vector<int>& idom) {
    int N = static_cast<int>(blocks.size());
    auto dominates = [&](int a, int b) -> bool {
        if (a == b) return true;
        int curr = b;
        while (curr != 0 && curr != -1) {
            if (curr == a) return true;
            curr = idom[curr];
        }
        return curr == a;
    };
    struct NaturalLoop {
        int header;
        std::unordered_set<int> body;
    };
    std::vector<NaturalLoop> loops;
    for (int u = 0; u < N; ++u) {
        for (int h : blocks[u].succs) {
            if (!dominates(h, u)) continue;
            std::unordered_set<int> body = {h, u};
            std::vector<int> worklist = {u};
            while (!worklist.empty()) {
                int curr = worklist.back();
                worklist.pop_back();
                if (curr == h) continue;
                for (int p : blocks[curr].preds) {
                    if (!body.count(p)) {
                        body.insert(p);
                        worklist.push_back(p);
                    }
                }
            }
            bool merged = false;
            for (auto& lp : loops) {
                if (lp.header == h) {
                    lp.body.insert(body.begin(), body.end());
                    merged = true;
                    break;
                }
            }
            if (!merged) loops.push_back({h, std::move(body)});
        }
    }
    std::sort(loops.begin(), loops.end(), [](const NaturalLoop& a, const NaturalLoop& b) {
        return a.body.size() < b.body.size();
    });
    auto is_pure_alu = [](const IRInst& inst) -> bool {
        switch (inst.op) {
            case IROp::ADD: case IROp::SUB: case IROp::MUL:
            case IROp::ICMP_EQ: case IROp::ICMP_NE:
            case IROp::ICMP_SLT: case IROp::ICMP_SGT:
            case IROp::ICMP_SLE: case IROp::ICMP_SGE:
                return true;
            case IROp::SDIV: case IROp::SREM:
                return !inst.src2.empty() && inst.src2[0] != '%' && std::stoll(inst.src2) != 0;
            default:
                return false;
        }
    };
    int preheader_cnt = 0;
    for (auto& lp : loops) {
        int h = lp.header;
        std::unordered_set<std::string> defined_in_loop;
        for (int b : lp.body) {
            for (const auto& inst : blocks[b].insts) {
                if (!inst.dst.empty() && inst.dst[0] == '%' &&
                    inst.op != IROp::JMP && inst.op != IROp::BR_COND && inst.op != IROp::LABEL) {
                    defined_in_loop.insert(inst.dst);
                }
            }
        }
        std::vector<IRInst> hoisted;
        std::unordered_set<std::string> hoisted_regs;
        bool changed = true;
        while (changed) {
            changed = false;
            for (int b : lp.body) {
                for (const auto& inst : blocks[b].insts) {
                    if (hoisted_regs.count(inst.dst) || !is_pure_alu(inst)) continue;
                    bool op1_inv = inst.src1.empty() || inst.src1[0] != '%' || !defined_in_loop.count(inst.src1);
                    bool op2_inv = inst.src2.empty() || inst.src2[0] != '%' || !defined_in_loop.count(inst.src2);
                    if (op1_inv && op2_inv) {
                        hoisted.push_back(inst);
                        hoisted_regs.insert(inst.dst);
                        defined_in_loop.erase(inst.dst);
                        changed = true;
                    }
                }
            }
        }
        if (hoisted.empty()) continue;
        for (int b : lp.body) {
            std::vector<IRInst> remain;
            remain.reserve(blocks[b].insts.size());
            for (auto& inst : blocks[b].insts) {
                if (!hoisted_regs.count(inst.dst)) {
                    remain.push_back(std::move(inst));
                }
            }
            blocks[b].insts = std::move(remain);
        }
        std::vector<int> outside_preds;
        for (int p : blocks[h].preds) {
            if (!lp.body.count(p)) outside_preds.push_back(p);
        }
        int preheader = -1;
        if (outside_preds.size() == 1 && blocks[outside_preds[0]].succs.size() == 1) {
            preheader = outside_preds[0];
        } else {
            preheader = static_cast<int>(blocks.size());
            std::string pre_lbl = ".Lpreheader_" + std::to_string(preheader_cnt++);
            std::string h_lbl = blocks[h].label;
            IRBlock pre_blk(pre_lbl);
            pre_blk.insts.push_back({IROp::JMP, h_lbl, "", "", {}, {}});
            pre_blk.preds = outside_preds;
            pre_blk.succs = {h};
            blocks.push_back(std::move(pre_blk));
            idom.push_back(idom[h]);
            idom[h] = preheader;
            std::unordered_set<std::string> outside_labels;
            for (int p : outside_preds) {
                outside_labels.insert(blocks[p].label);
                auto& term = blocks[p].insts.back();
                if (term.op == IROp::JMP && term.dst == h_lbl) term.dst = pre_lbl;
                if (term.op == IROp::BR_COND) {
                    if (term.dst == h_lbl) term.dst = pre_lbl;
                    if (term.src2 == h_lbl) term.src2 = pre_lbl;
                }
                for (int& s : blocks[p].succs) {
                    if (s == h) { s = preheader; break; }
                }
            }
            std::vector<int> new_h_preds = {preheader};
            for (int p : blocks[h].preds) {
                if (lp.body.count(p)) new_h_preds.push_back(p);
            }
            blocks[h].preds = std::move(new_h_preds);
            for (auto& inst : blocks[h].insts) {
                if (inst.op == IROp::PHI) {
                    for (auto& [lbl, val] : inst.phi_in) {
                        if (outside_labels.count(lbl)) lbl = pre_lbl;
                    }
                }
            }
        }
        auto& p_insts = blocks[preheader].insts;
        auto pos = p_insts.empty() ? p_insts.end() : (p_insts.end() - 1);
        p_insts.insert(pos, hoisted.begin(), hoisted.end());
    }
}

void inline_functions(std::vector<IRFunc>& funcs) {
    int inline_id = 0;
    bool changed = true;
    while (changed) {
        changed = false;
        std::unordered_map<std::string, IRFunc> leaf_map;
        for (const auto& fn : funcs) {
            bool is_leaf = true;
            for (const auto& inst : fn.insts) {
                if (inst.op == IROp::CALL) {
                    is_leaf = false;
                    break;
                }
            }
            std::unordered_set<std::string> seen_labels;
            bool has_loop = false;
            for (const auto& inst : fn.insts) {
                if (inst.op == IROp::LABEL) {
                    seen_labels.insert(inst.dst);
                } else if (inst.op == IROp::JMP && seen_labels.count(inst.dst)) {
                    has_loop = true;
                    break;
                } else if (inst.op == IROp::BR_COND &&
                        (seen_labels.count(inst.dst) || seen_labels.count(inst.src2))) {
                    has_loop = true;
                    break;
                }
            }
            if (is_leaf && !has_loop) {
                leaf_map[fn.name] = fn;
            }
        }
        if (leaf_map.empty()) break;
        for (auto& caller : funcs) {
            bool caller_changed = false;
            std::vector<IRInst> hoisted_allocas;
            std::vector<IRInst> new_insts;
            for (const auto& call_inst : caller.insts) {
                if (call_inst.op != IROp::CALL || call_inst.src1 == caller.name || !leaf_map.count(call_inst.src1)) {
                    new_insts.push_back(call_inst);
                    continue;
                }
                caller_changed = true;
                changed = true;
                const auto& callee = leaf_map[call_inst.src1];
                std::string suffix = ".inl_" + std::to_string(inline_id++);
                int ret_cnt = 0;
                for (const auto& inst : callee.insts) {
                    if (inst.op == IROp::RET) ++ret_cnt;
                }
                bool single_tail_ret = (ret_cnt == 1 && !callee.insts.empty() && callee.insts.back().op == IROp::RET);
                std::unordered_map<std::string, std::string> val_map;
                for (size_t k = 0; k < callee.params.size() && k < call_inst.args.size(); ++k) {
                    const std::string& p = callee.params[k];
                    val_map[p] = call_inst.args[k];
                    if (!p.empty() && p[0] != '%') val_map["%" + p] = call_inst.args[k];
                    else if (p.size() > 1) val_map[p.substr(1)] = call_inst.args[k];
                }
                auto rename_val = [&](const std::string& v) -> std::string {
                    if (v.empty()) return "";
                    if (val_map.count(v)) return val_map[v];
                    if (v[0] == '%') return v + suffix;
                    return v;
                };
                auto rename_lbl = [&](const std::string& l) -> std::string {
                    return l.empty() ? "" : (l + suffix);
                };
                if (single_tail_ret) {
                    for (const auto& inst : callee.insts) {
                        if (inst.op == IROp::ALLOCA) {
                            hoisted_allocas.push_back({IROp::ALLOCA, rename_val(inst.dst), "", "", {}, {}});
                        } else if (inst.op == IROp::LABEL) {
                            new_insts.push_back({IROp::LABEL, rename_lbl(inst.dst), "", "", {}, {}});
                        } else if (inst.op == IROp::JMP) {
                            new_insts.push_back({IROp::JMP, rename_lbl(inst.dst), "", "", {}, {}});
                        } else if (inst.op == IROp::BR_COND) {
                            new_insts.push_back({IROp::BR_COND, rename_lbl(inst.dst), rename_val(inst.src1), rename_lbl(inst.src2), {}, {}});
                        } else if (inst.op == IROp::RET) {
                            if (!call_inst.dst.empty() && !inst.src1.empty()) {
                                new_insts.push_back({IROp::MOV, call_inst.dst, rename_val(inst.src1), "", {}, {}});
                            }
                        } else {
                            new_insts.push_back({inst.op, rename_val(inst.dst), rename_val(inst.src1), rename_val(inst.src2), {}, {}});
                        }
                    }
                } else {
                    std::string exit_label = "L_inl_end" + suffix;
                    std::string ret_ptr;
                    if (!call_inst.dst.empty()) {
                        ret_ptr = "%inl_ret" + suffix;
                        hoisted_allocas.push_back({IROp::ALLOCA, ret_ptr, "", "", {}, {}});
                    }
                    for (const auto& inst : callee.insts) {
                        if (inst.op == IROp::ALLOCA) {
                            hoisted_allocas.push_back({IROp::ALLOCA, rename_val(inst.dst), "", "", {}, {}});
                        } else if (inst.op == IROp::LABEL) {
                            new_insts.push_back({IROp::LABEL, rename_lbl(inst.dst), "", "", {}, {}});
                        } else if (inst.op == IROp::JMP) {
                            new_insts.push_back({IROp::JMP, rename_lbl(inst.dst), "", "", {}, {}});
                        } else if (inst.op == IROp::BR_COND) {
                            new_insts.push_back({IROp::BR_COND, rename_lbl(inst.dst), rename_val(inst.src1), rename_lbl(inst.src2), {}, {}});
                        } else if (inst.op == IROp::RET) {
                            if (!ret_ptr.empty() && !inst.src1.empty()) {
                                new_insts.push_back({IROp::STORE, "", rename_val(inst.src1), ret_ptr, {}, {}});
                            }
                            new_insts.push_back({IROp::JMP, exit_label, "", "", {}, {}});
                        } else {
                            new_insts.push_back({inst.op, rename_val(inst.dst), rename_val(inst.src1), rename_val(inst.src2), {}, {}});
                        }
                    }
                    new_insts.push_back({IROp::LABEL, exit_label, "", "", {}, {}});
                    if (!ret_ptr.empty()) {
                        new_insts.push_back({IROp::LOAD, call_inst.dst, ret_ptr, "", {}, {}});
                    }
                }
            }
            if (caller_changed) {
                if (!hoisted_allocas.empty()) {
                    new_insts.insert(new_insts.begin(), hoisted_allocas.begin(), hoisted_allocas.end());
                }
                caller.insts = std::move(new_insts);
            }
        }
    }
}

void run_cse(std::vector<IRBlock>& blocks, const std::vector<int>& idom) {
    int N = static_cast<int>(blocks.size());
    std::vector<std::vector<int>> dom_children(N);
    for (int b = 1; b < N; ++b) {
        if (blocks[b].used && idom[b] >= 0 && idom[b] < N) {
            dom_children[idom[b]].push_back(b);
        }
    }
    struct ExprKey {
        IROp op;
        std::string s1;
        std::string s2;
        bool operator==(const ExprKey& o) const {
            return op == o.op && s1 == o.s1 && s2 == o.s2;
        }
    };
    struct ExprHash {
        size_t operator()(const ExprKey& k) const {
            size_t h1 = std::hash<int>{}(static_cast<int>(k.op));
            size_t h2 = std::hash<std::string>{}(k.s1);
            size_t h3 = std::hash<std::string>{}(k.s2);
            return h1 ^ (h2 * 1315423911u) ^ (h3 * 2654435761u);
        }
    };
    auto is_cse_candidate = [](IROp op) -> bool {
        switch (op) {
            case IROp::ADD: case IROp::SUB: case IROp::MUL:
            case IROp::SDIV: case IROp::SREM:
                return true;
            default:
                return false;
        }
    };
    std::unordered_map<ExprKey, std::string, ExprHash> expr_table;
    std::unordered_map<std::string, std::string> alias;
    auto resolve = [&](std::string& op) {
        while (alias.count(op)) op = alias[op];
    };
    auto dfs_cse = [&](auto& self, int b) -> void {
        if (!blocks[b].used) return;
        std::vector<ExprKey> added_keys;
        std::vector<IRInst> kept_insts;
        kept_insts.reserve(blocks[b].insts.size());
        for (auto& inst : blocks[b].insts) {
            if (inst.op == IROp::PHI) {
                for (auto& [lbl, val] : inst.phi_in) {
                    resolve(val);
                }
                kept_insts.push_back(std::move(inst));
                continue;
            }
            resolve(inst.src1);
            resolve(inst.src2);
            for (auto& arg : inst.args) {
                resolve(arg);
            }
            if (is_cse_candidate(inst.op) && !inst.dst.empty() && inst.dst[0] == '%') {
                std::string op1 = inst.src1;
                std::string op2 = inst.src2;
                if (inst.op == IROp::ADD || inst.op == IROp::MUL ||
                    inst.op == IROp::ICMP_EQ || inst.op == IROp::ICMP_NE) {
                    if (op1 > op2) std::swap(op1, op2);
                }
                ExprKey key{inst.op, op1, op2};
                auto it = expr_table.find(key);
                if (it != expr_table.end()) {
                    alias[inst.dst] = it->second;
                    continue;
                } else {
                    expr_table.emplace(key, inst.dst);
                    added_keys.push_back(std::move(key));
                }
            }
            kept_insts.push_back(std::move(inst));
        }
        blocks[b].insts = std::move(kept_insts);
        for (int child : dom_children[b]) {
            self(self, child);
        }
        for (const auto& k : added_keys) {
            expr_table.erase(k);
        }
    };
    dfs_cse(dfs_cse, 0);
    if (!alias.empty()) {
        for (auto& blk : blocks) {
            if (!blk.used) continue;
            for (auto& inst : blk.insts) {
                if (inst.op == IROp::PHI) {
                    for (auto& [lbl, val] : inst.phi_in) {
                        resolve(val);
                    }
                } else {
                    resolve(inst.src1);
                    resolve(inst.src2);
                    for (auto& arg : inst.args) {
                        resolve(arg);
                    }
                }
            }
        }
    }
}

void analyze_interprocedural_non_negative(std::vector<IRUnit>& units) {
    const long long I32_MIN = -(1LL << 31);
    const long long I32_MAX = 0x7fffffff;
    struct Range {
        long long lo = I32_MIN;
        long long hi = I32_MAX;
        bool operator==(const Range& o) const { return lo == o.lo && hi == o.hi; }
    };
    auto clamp_i32 = [](long long lo, long long hi) -> Range {
        if (hi < I32_MIN || lo > I32_MAX || lo > hi) return {I32_MIN, I32_MAX};
        return {std::max(lo, -(1LL << 31)), std::min(hi, 0x7fffffffll)};
    };
    auto union_range = [](Range a, Range b) -> Range {
        return {std::min(a.lo, b.lo), std::max(a.hi, b.hi)};
    };
    std::unordered_map<std::string, IRUnit*> unit_map;
    for (auto& u : units) {
        unit_map[u.fn->name] = &u;
    }
    std::unordered_set<std::string> has_internal_caller;
    for (const auto& u : units) {
        for (const auto& blk : u.blocks) {
            if (!blk.used) continue;
            for (const auto& inst : blk.insts) {
                if (inst.op == IROp::CALL && unit_map.count(inst.src1)) {
                    has_internal_caller.insert(inst.src1);
                }
            }
        }
    }
    auto is_external_entry = [&](const std::string& name) -> bool {
        return name == "main" || name == "toyc_entry" || !has_internal_caller.count(name);
    };
    std::unordered_map<std::string, std::vector<Range>> param_ranges;
    std::unordered_map<std::string, std::vector<bool>> param_known;
    std::unordered_map<std::string, Range> ret_ranges;
    for (const auto& u : units) {
        size_t nparams = u.fn->params.size();
        param_ranges[u.fn->name] = std::vector<Range>(nparams, {I32_MIN, I32_MAX});
        bool ext = is_external_entry(u.fn->name);
        param_known[u.fn->name] = std::vector<bool>(nparams, ext);
    }
    auto all_params_known = [&](const std::string& name) -> bool {
        auto it = param_known.find(name);
        if (it == param_known.end()) return true;
        for (bool k : it->second) {
            if (!k) return false;
        }
        return true;
    };
    auto analyze_one_func = [&](
        IRUnit& u,
        std::unordered_map<std::string, std::vector<Range>>& next_param_ranges,
        std::unordered_map<std::string, std::vector<bool>>& param_seen,
        std::unordered_map<std::string, Range>& next_ret_ranges,
        std::unordered_map<std::string, bool>& ret_seen) -> std::unordered_map<std::string, Range> {
        int N = static_cast<int>(u.blocks.size());
        std::unordered_map<std::string, int> label_to_id;
        for (int b = 0; b < N; ++b) {
            if (u.blocks[b].used && !u.blocks[b].label.empty()) {
                label_to_id[u.blocks[b].label] = b;
            }
        }
        std::unordered_map<std::string, Range> ranges;
        if (param_ranges.count(u.fn->name)) {
            const auto& pr = param_ranges[u.fn->name];
            const auto& pk = param_known[u.fn->name];
            for (size_t k = 0; k < u.fn->params.size() && k < pr.size(); ++k) {
                if (!pk[k]) continue;
                const std::string& raw_p = u.fn->params[k];
                if (raw_p.empty()) continue;
                std::string norm_p = (raw_p[0] == '%') ? raw_p : ("%" + raw_p);
                ranges[norm_p] = pr[k];
                ranges[raw_p] = pr[k];
            }
        }
        std::unordered_map<std::string, const IRInst*> def_inst;
        std::unordered_map<std::string, int> def_block;
        for (int b = 0; b < N; ++b) {
            if (!u.blocks[b].used) continue;
            for (const auto& inst : u.blocks[b].insts) {
                if (!inst.dst.empty() && inst.dst[0] == '%') {
                    def_inst[inst.dst] = &inst;
                    def_block[inst.dst] = b;
                }
            }
        }
        auto dominates = [&](int a, int b) -> bool {
            int x = b;
            while (x >= 0) {
                if (x == a) return true;
                if (x == 0) break;
                x = u.idom[x];
            }
            return false;
        };
        auto edge_dominates_target = [&](int p, int curr) -> bool {
            bool has_p = false;
            for (int q : u.blocks[curr].preds) {
                if (!u.blocks[q].used) continue;
                if (q == p) has_p = true;
                else if (!dominates(curr, q)) return false;
            }
            return has_p;
        };
        auto base_range = [&](const std::string& op) -> std::pair<bool, Range> {
            if (op.empty()) return {false, {}};
            if (op[0] != '%') {
                long long v = std::stoll(op);
                return {true, {v, v}};
            }
            auto it = ranges.find(op);
            if (it == ranges.end()) return {false, {}};
            return {true, it->second};
        };
        auto apply_icmp_bound = [&](Range r, IROp op, bool is_true, Range rhs_r) -> Range {
            switch (op) {
                case IROp::ICMP_SLT:
                    if (is_true) r.hi = std::min(r.hi, rhs_r.hi - 1);
                    else r.lo = std::max(r.lo, rhs_r.lo);
                    break;
                case IROp::ICMP_SLE:
                    if (is_true) r.hi = std::min(r.hi, rhs_r.hi);
                    else r.lo = std::max(r.lo, rhs_r.lo + 1);
                    break;
                case IROp::ICMP_SGT:
                    if (is_true) r.lo = std::max(r.lo, rhs_r.lo + 1);
                    else r.hi = std::min(r.hi, rhs_r.hi);
                    break;
                case IROp::ICMP_SGE:
                    if (is_true) r.lo = std::max(r.lo, rhs_r.lo);
                    else r.hi = std::min(r.hi, rhs_r.hi - 1);
                    break;
                case IROp::ICMP_EQ:
                    if (is_true) {
                        r.lo = std::max(r.lo, rhs_r.lo);
                        r.hi = std::min(r.hi, rhs_r.hi);
                    }
                    break;
                case IROp::ICMP_NE:
                    if (!is_true) {
                        r.lo = std::max(r.lo, rhs_r.lo);
                        r.hi = std::min(r.hi, rhs_r.hi);
                    }
                    break;
                default:
                    break;
            }
            return (r.lo <= r.hi) ? r : Range{I32_MIN, I32_MAX};
        };
        auto narrow_by_cond = [&](auto& self, const std::string& var, Range r,
                          const std::string& cond_var, bool is_true, int depth) -> Range {
            if (depth > 32 || cond_var.empty() || cond_var[0] != '%') return r;
            auto dit = def_inst.find(cond_var);
            if (dit == def_inst.end()) return r;
            const IRInst* cond = dit->second;
            if (cond->op == IROp::MOV) {
                return self(self, var, r, cond->src1, is_true, depth + 1);
            }
            if (cond->op == IROp::ICMP_NE || cond->op == IROp::ICMP_EQ) {
                bool flip = (cond->op == IROp::ICMP_EQ);
                if (cond->src2 == "0" && cond->src1 != var && !cond->src1.empty() && cond->src1[0] == '%') {
                    return self(self, var, r, cond->src1, flip ? !is_true : is_true, depth + 1);
                }
                if (cond->src1 == "0" && cond->src2 != var && !cond->src2.empty() && cond->src2[0] == '%') {
                    return self(self, var, r, cond->src2, flip ? !is_true : is_true, depth + 1);
                }
            }
            if (cond->src1 == var) {
                auto [ok_rhs, rhs_r] = base_range(cond->src2);
                if (!ok_rhs) rhs_r = {I32_MIN, I32_MAX};
                return apply_icmp_bound(r, cond->op, is_true, rhs_r);
            }
            if (cond->src2 == var) {
                auto [ok_lhs, lhs_r] = base_range(cond->src1);
                if (!ok_lhs) lhs_r = {I32_MIN, I32_MAX};
                IROp flipped = cond->op;
                if (flipped == IROp::ICMP_SLT) flipped = IROp::ICMP_SGT;
                else if (flipped == IROp::ICMP_SGT) flipped = IROp::ICMP_SLT;
                else if (flipped == IROp::ICMP_SLE) flipped = IROp::ICMP_SGE;
                else if (flipped == IROp::ICMP_SGE) flipped = IROp::ICMP_SLE;
                return apply_icmp_bound(r, flipped, is_true, lhs_r);
            }
            if (cond->op == IROp::PHI) {
                std::string unique_pred_lbl;
                std::string unique_val;
                int match_cnt = 0;
                for (const auto& [in_lbl, val] : cond->phi_in) {
                    bool can_match = true;
                    if (!val.empty() && val[0] != '%') {
                        long long v = std::stoll(val);
                        can_match = is_true ? (v != 0) : (v == 0);
                    } else {
                        auto [ok, vr] = base_range(val);
                        if (ok) {
                            can_match = is_true ? (vr.lo != 0 || vr.hi != 0) : (vr.lo <= 0 && vr.hi >= 0);
                        }
                    }
                    if (can_match) {
                        unique_pred_lbl = in_lbl;
                        unique_val = val;
                        ++match_cnt;
                    }
                }
                if (match_cnt == 1) {
                    if (!unique_val.empty() && unique_val[0] == '%') {
                        r = self(self, var, r, unique_val, is_true, depth + 1);
                    }
                    auto lit = label_to_id.find(unique_pred_lbl);
                    if (lit != label_to_id.end()) {
                        int cur = lit->second;
                        int phi_blk = def_block.count(cond_var) ? def_block[cond_var] : -1;
                        int stop_dom = (phi_blk > 0) ? u.idom[phi_blk] : -1;
                        while (cur > 0 && cur != stop_dom && u.blocks[cur].preds.size() == 1) {
                            int p_prev = u.blocks[cur].preds[0];
                            if (p_prev >= 0 && p_prev < N && u.blocks[p_prev].used && !u.blocks[p_prev].insts.empty()) {
                                const auto& p_br = u.blocks[p_prev].insts.back();
                                if (p_br.op == IROp::BR_COND && p_br.dst != p_br.src2) {
                                    if (p_br.dst == u.blocks[cur].label) {
                                        r = self(self, var, r, p_br.src1, true, depth + 1);
                                    } else if (p_br.src2 == u.blocks[cur].label) {
                                        r = self(self, var, r, p_br.src1, false, depth + 1);
                                    }
                                }
                            }
                            cur = p_prev;
                        }
                    }
                }
            }
            return r;
        };
        auto narrow_on_edge = [&](const std::string& var, Range r, int pred, const std::string& succ_label) -> Range {
            if (pred < 0 || pred >= N || !u.blocks[pred].used) return r;
            const auto& p_insts = u.blocks[pred].insts;
            if (p_insts.empty()) return r;
            const auto& br = p_insts.back();
            if (br.op != IROp::BR_COND || br.dst == br.src2) return r;
            if (br.dst == succ_label) return narrow_by_cond(narrow_by_cond, var, r, br.src1, true, 0);
            if (br.src2 == succ_label) return narrow_by_cond(narrow_by_cond, var, r, br.src1, false, 0);
            return r;
        };
        auto eval_in_block = [&](const std::string& op, int b) -> std::pair<bool, Range> {
            auto [ok, r] = base_range(op);
            if (!ok || op[0] != '%') return {ok, r};
            int curr = b;
            while (curr > 0 && u.idom[curr] >= 0) {
                int p = u.idom[curr];
                if (u.blocks[p].succs.size() == 2 && edge_dominates_target(p, curr)) {
                    r = narrow_on_edge(op, r, p, u.blocks[curr].label);
                }
                curr = p;
            }
            return {true, r};
        };
        bool changed = true;
        int iter = 0;
        while (changed && iter < 20) {
            changed = false;
            ++iter;
            for (int b = 0; b < N; ++b) {
                if (!u.blocks[b].used) continue;
                for (const auto& inst : u.blocks[b].insts) {
                    if (inst.dst.empty() || inst.dst[0] != '%') continue;
                    bool has_new = false;
                    Range nr{I32_MIN, I32_MAX};
                    if (inst.op == IROp::PHI) {
                        long long lo = I32_MAX, hi = I32_MIN;
                        for (const auto& [pred_lbl, val] : inst.phi_in) {
                            auto pit = label_to_id.find(pred_lbl);
                            auto [ok, r] = (pit != label_to_id.end())
                                            ? eval_in_block(val, pit->second)
                                            : base_range(val);
                            if (!ok) continue;
                            if (pit != label_to_id.end()) {
                                r = narrow_on_edge(val, r, pit->second, u.blocks[b].label);
                            }
                            lo = std::min(lo, r.lo);
                            hi = std::max(hi, r.hi);
                            has_new = true;
                        }
                        if (has_new) nr = {lo, hi};
                    } else if (inst.op == IROp::MOV) {
                        auto [ok, r] = eval_in_block(inst.src1, b);
                        if (ok) { nr = r; has_new = true; }
                    } else if (inst.op == IROp::ADD) {
                        auto [ok1, r1] = eval_in_block(inst.src1, b);
                        auto [ok2, r2] = eval_in_block(inst.src2, b);
                        if (ok1 && ok2) { nr = clamp_i32(r1.lo + r2.lo, r1.hi + r2.hi); has_new = true; }
                    } else if (inst.op == IROp::SUB) {
                        auto [ok1, r1] = eval_in_block(inst.src1, b);
                        auto [ok2, r2] = eval_in_block(inst.src2, b);
                        if (ok1 && ok2) { nr = clamp_i32(r1.lo - r2.hi, r1.hi - r2.lo); has_new = true; }
                    } else if (inst.op == IROp::MUL) {
                        auto [ok1, r1] = eval_in_block(inst.src1, b);
                        auto [ok2, r2] = eval_in_block(inst.src2, b);
                        if (ok1 && ok2) {
                            long long p1 = r1.lo * r2.lo, p2 = r1.lo * r2.hi;
                            long long p3 = r1.hi * r2.lo, p4 = r1.hi * r2.hi;
                            nr = clamp_i32(std::min({p1, p2, p3, p4}), std::max({p1, p2, p3, p4}));
                            has_new = true;
                        }
                    } else if (inst.op == IROp::SDIV) {
                        auto [ok1, r1] = eval_in_block(inst.src1, b);
                        auto [ok2, r2] = eval_in_block(inst.src2, b);
                        if (ok1 && ok2 && r2.lo > 0) {
                            long long lo = (r1.lo >= 0) ? (r1.lo / r2.hi) : (r1.lo / r2.lo);
                            long long hi = (r1.hi >= 0) ? (r1.hi / r2.lo) : (r1.hi / r2.hi);
                            nr = clamp_i32(lo, hi);
                            has_new = true;
                        }
                    } else if (inst.op == IROp::SREM) {
                        auto [ok1, r1] = eval_in_block(inst.src1, b);
                        auto [ok2, r2] = eval_in_block(inst.src2, b);
                        if (ok1 && ok2 && r2.lo == r2.hi && r2.lo != 0) {
                            long long ad = std::abs(r2.lo);
                            if (r1.lo >= 0) nr = {0, std::min(r1.hi, ad - 1)};
                            else            nr = {-(ad - 1), ad - 1};
                            has_new = true;
                        }
                    } else if (inst.op == IROp::CALL) {
                        if (!unit_map.count(inst.src1)) {
                            nr = {I32_MIN, I32_MAX};
                            has_new = true;
                        } else if (ret_ranges.count(inst.src1)) {
                            nr = ret_ranges[inst.src1];
                            has_new = true;
                        }
                    } else if (inst.op == IROp::LOAD) {
                        nr = {I32_MIN, I32_MAX};
                        has_new = true;
                    }
                    if (!has_new) continue;
                    auto it = ranges.find(inst.dst);
                    if (it == ranges.end()) {
                        ranges[inst.dst] = nr;
                        changed = true;
                    } else {
                        if (inst.op == IROp::PHI && iter > 8) {
                            if (nr.lo < it->second.lo) nr.lo = (nr.lo >= 0) ? 0 : I32_MIN;
                            if (nr.hi > it->second.hi) nr.hi = I32_MAX;
                        }
                        if (!(it->second == nr)) {
                            it->second = nr;
                            changed = true;
                        }
                    }
                }
            }
        }
        for (int b = 0; b < N; ++b) {
            if (!u.blocks[b].used) continue;
            for (const auto& inst : u.blocks[b].insts) {
                if (inst.op == IROp::CALL && unit_map.count(inst.src1) && !is_external_entry(inst.src1)) {
                    const std::string& callee = inst.src1;
                    size_t nparams = unit_map[callee]->fn->params.size();
                    next_param_ranges.try_emplace(callee, nparams, Range{I32_MIN, I32_MAX});
                    param_seen.try_emplace(callee, nparams, false);
                    for (size_t k = 0; k < nparams && k < inst.args.size(); ++k) {
                        auto [ok, r] = eval_in_block(inst.args[k], b);
                        if (!ok) continue;
                        if (!param_seen[callee][k]) {
                            next_param_ranges[callee][k] = r;
                            param_seen[callee][k] = true;
                        } else {
                            next_param_ranges[callee][k] = union_range(next_param_ranges[callee][k], r);
                        }
                    }
                } else if (inst.op == IROp::RET && !inst.src1.empty()) {
                    auto [ok, r] = eval_in_block(inst.src1, b);
                    if (!ok) continue;
                    if (!ret_seen[u.fn->name]) {
                        next_ret_ranges[u.fn->name] = r;
                        ret_seen[u.fn->name] = true;
                    } else {
                        next_ret_ranges[u.fn->name] = union_range(next_ret_ranges[u.fn->name], r);
                    }
                }
            }
        }
        return ranges;
    };
    bool inter_changed = true;
    int outer_iter = 0;
    int max_outer_iter = std::max(12, static_cast<int>(units.size()) * 2 + 6);
    while (inter_changed && outer_iter < max_outer_iter) {
        inter_changed = false;
        ++outer_iter;
        std::unordered_map<std::string, std::vector<Range>> next_param_ranges;
        std::unordered_map<std::string, std::vector<bool>> param_seen;
        std::unordered_map<std::string, Range> next_ret_ranges;
        std::unordered_map<std::string, bool> ret_seen;
        for (auto& u : units) {
            if (!all_params_known(u.fn->name)) continue;
            analyze_one_func(u, next_param_ranges, param_seen, next_ret_ranges, ret_seen);
        }
        for (auto& [fname, seen_vec] : param_seen) {
            auto& cur_vec = param_ranges[fname];
            auto& known_vec = param_known[fname];
            const auto& new_vec = next_param_ranges[fname];
            for (size_t k = 0; k < seen_vec.size() && k < cur_vec.size(); ++k) {
                if (!seen_vec[k]) continue;
                if (!known_vec[k]) {
                    cur_vec[k] = new_vec[k];
                    known_vec[k] = true;
                    inter_changed = true;
                } else {
                    Range merged = union_range(cur_vec[k], new_vec[k]);
                    if (outer_iter > 5) {
                        if (merged.lo < cur_vec[k].lo) merged.lo = (merged.lo >= 0) ? 0 : I32_MIN;
                        if (merged.hi > cur_vec[k].hi) merged.hi = I32_MAX;
                    }
                    if (!(cur_vec[k] == merged)) {
                        cur_vec[k] = merged;
                        inter_changed = true;
                    }
                }
            }
        }
        for (auto& [fname, seen] : ret_seen) {
            if (!seen) continue;
            const Range& new_r = next_ret_ranges[fname];
            auto it = ret_ranges.find(fname);
            if (it == ret_ranges.end()) {
                ret_ranges[fname] = new_r;
                inter_changed = true;
            } else {
                Range merged = union_range(it->second, new_r);
                if (outer_iter > 5) {
                    if (merged.lo < it->second.lo) merged.lo = (merged.lo >= 0) ? 0 : I32_MIN;
                    if (merged.hi > it->second.hi) merged.hi = I32_MAX;
                }
                if (!(it->second == merged)) {
                    it->second = merged;
                    inter_changed = true;
                }
            }
        }
    }
    std::unordered_map<std::string, std::vector<Range>> dummy_p;
    std::unordered_map<std::string, std::vector<bool>> dummy_ps;
    std::unordered_map<std::string, Range> dummy_r;
    std::unordered_map<std::string, bool> dummy_rs;
    for (auto& u : units) {
        if (!all_params_known(u.fn->name)) continue;
        auto final_ranges = analyze_one_func(u, dummy_p, dummy_ps, dummy_r, dummy_rs);
        for (const auto& [var, r] : final_ranges) {
            if (r.lo >= 0) {
                u.non_neg_vars.insert(var);
            }
        }
    }
}

void reassociate_loop_adds(std::vector<IRBlock>& blocks, const std::vector<int>& idom) {
    int N = static_cast<int>(blocks.size());
    auto dominates = [&](int a, int b) -> bool {
        if (a == b) return true;
        int curr = b;
        while (curr != 0 && curr != -1) {
            if (curr == a) return true;
            curr = idom[curr];
        }
        return curr == a;
    };
    std::unordered_map<std::string, int> use_cnt;
    for (int b = 0; b < N; ++b) {
        if (!blocks[b].used) continue;
        for (const auto& inst : blocks[b].insts) {
            if (inst.op == IROp::PHI) {
                for (const auto& [lbl, val] : inst.phi_in) {
                    if (!val.empty() && val[0] == '%') use_cnt[val]++;
                }
            } else {
                if (!inst.src1.empty() && inst.src1[0] == '%') use_cnt[inst.src1]++;
                if (!inst.src2.empty() && inst.src2[0] == '%') use_cnt[inst.src2]++;
                for (const auto& arg : inst.args) {
                    if (!arg.empty() && arg[0] == '%') use_cnt[arg]++;
                }
            }
        }
    }
    std::unordered_map<int, std::vector<int>> header_latches;
    for (int u = 0; u < N; ++u) {
        if (!blocks[u].used) continue;
        for (int h : blocks[u].succs) {
            if (dominates(h, u)) header_latches[h].push_back(u);
        }
    }
    struct LoopInfo {
        int header;
        std::unordered_set<int> body;
    };
    std::vector<LoopInfo> loops;
    loops.reserve(header_latches.size());
    for (const auto& [h, latches] : header_latches) {
        std::unordered_set<int> body = {h};
        std::vector<int> wl;
        for (int u : latches) {
            if (body.insert(u).second) wl.push_back(u);
        }
        while (!wl.empty()) {
            int cur = wl.back();
            wl.pop_back();
            if (cur == h) continue;
            for (int p : blocks[cur].preds) {
                if (body.insert(p).second) wl.push_back(p);
            }
        }
        loops.push_back({h, std::move(body)});
    }
    std::sort(loops.begin(), loops.end(), [](const LoopInfo& a, const LoopInfo& b) {
        return a.body.size() < b.body.size();
    });
    for (const auto& lp : loops) {
        std::unordered_set<std::string> def_in_loop;
        for (int b : lp.body) {
            for (const auto& inst : blocks[b].insts) {
                if (!inst.dst.empty() && inst.dst[0] == '%') def_in_loop.insert(inst.dst);
            }
        }
        auto is_inv = [&](const std::string& op) {
            return op.empty() || op[0] != '%' || !def_in_loop.count(op);
        };
        for (int b : lp.body) {
            bool changed = true;
            while (changed) {
                changed = false;
                std::unordered_map<std::string, size_t> def_idx;
                for (size_t i = 0; i < blocks[b].insts.size(); ++i) {
                    const auto& inst = blocks[b].insts[i];
                    if (!inst.dst.empty() && inst.dst[0] == '%') def_idx[inst.dst] = i;
                }
                for (size_t i = 0; i < blocks[b].insts.size(); ++i) {
                    auto& inst2 = blocks[b].insts[i];
                    if (inst2.op != IROp::ADD) continue;
                    auto try_reassoc = [&](std::string sub_vreg, std::string outer_inv) -> bool {
                        if (sub_vreg.empty() || sub_vreg[0] != '%') return false;
                        if (!is_inv(outer_inv)) return false;
                        auto u_it = use_cnt.find(sub_vreg);
                        if (u_it == use_cnt.end() || u_it->second != 1) return false;
                        auto it = def_idx.find(sub_vreg);
                        if (it == def_idx.end() || it->second >= i) return false;

                        size_t idx1 = it->second;
                        auto& inst1 = blocks[b].insts[idx1];
                        if (inst1.op != IROp::ADD) return false;

                        std::string inner_var, inner_inv;
                        if (!is_inv(inst1.src1) && is_inv(inst1.src2)) {
                            inner_var = inst1.src1;
                            inner_inv = inst1.src2;
                        } else if (is_inv(inst1.src1) && !is_inv(inst1.src2)) {
                            inner_var = inst1.src2;
                            inner_inv = inst1.src1;
                        } else {
                            return false;
                        }
                        inst1.src1 = inner_inv;
                        inst1.src2 = outer_inv;
                        inst2.src1 = inner_var;
                        inst2.src2 = sub_vreg;
                        def_in_loop.erase(sub_vreg);
                        if (idx1 + 1 < i) {
                            std::rotate(blocks[b].insts.begin() + idx1,
                                        blocks[b].insts.begin() + idx1 + 1,
                                        blocks[b].insts.begin() + i);
                        }
                        return true;
                    };
                    if (try_reassoc(inst2.src1, inst2.src2) || try_reassoc(inst2.src2, inst2.src1)) {
                        changed = true;
                        break;
                    }
                }
            }
        }
    }
}

void run_ivsr(std::vector<IRBlock>& blocks, const std::vector<int>& idom, int& ssa_reg_counter) {
    int N = static_cast<int>(blocks.size());
    auto new_reg = [&]() { return "%iv_" + std::to_string(ssa_reg_counter++); };
    auto dominates = [&](int a, int b) -> bool {
        if (a == b) return true;
        int curr = b;
        while (curr != 0 && curr != -1) {
            if (curr == a) return true;
            curr = idom[curr];
        }
        return curr == a;
    };
    auto is_cheap_const_mul = [](long long c) -> bool {
        if (c == 0 || c == 1 || c == -1) return true;
        unsigned long long ac = (c < 0)
            ? static_cast<unsigned long long>(-c)
            : static_cast<unsigned long long>(c);
        return (ac & (ac - 1)) == 0;
    };
    std::unordered_map<int, std::vector<int>> header_latches;
    for (int u = 0; u < N; ++u) {
        if (!blocks[u].used) continue;
        for (int h : blocks[u].succs) {
            if (h != 0 && dominates(h, u)) header_latches[h].push_back(u);
        }
    }
    struct LoopInfo {
        int header;
        std::unordered_set<int> body;
    };
    std::vector<LoopInfo> loops;
    loops.reserve(header_latches.size());
    for (const auto& [h, latches] : header_latches) {
        std::unordered_set<int> body = {h};
        std::vector<int> wl;
        for (int u : latches) {
            if (body.insert(u).second) wl.push_back(u);
        }
        while (!wl.empty()) {
            int cur = wl.back();
            wl.pop_back();
            if (cur == h) continue;
            for (int p : blocks[cur].preds) {
                if (body.insert(p).second) wl.push_back(p);
            }
        }
        loops.push_back({h, std::move(body)});
    }
    std::sort(loops.begin(), loops.end(), [](const LoopInfo& a, const LoopInfo& b) {
        return a.body.size() < b.body.size();
    });
    for (const auto& lp : loops) {
        int h = lp.header;
        const auto& body = lp.body;
        int preheader = -1, latch = -1, out_cnt = 0, in_cnt = 0;
        for (int p : blocks[h].preds) {
            if (!body.count(p)) { preheader = p; ++out_cnt; }
            else { latch = p; ++in_cnt; }
        }
        if (out_cnt != 1 || in_cnt != 1 || preheader < 0 || latch < 0) continue;
        if (blocks[preheader].insts.empty() || blocks[latch].insts.empty()) continue;
        std::string pre_lbl = blocks[preheader].label;
        std::string latch_lbl = blocks[latch].label;
        std::unordered_set<std::string> def_in_loop;
        for (int b : body) {
            for (const auto& inst : blocks[b].insts) {
                if (!inst.dst.empty() && inst.dst[0] == '%') def_in_loop.insert(inst.dst);
            }
        }
        auto is_inv = [&](const std::string& op) {
            return op.empty() || op[0] != '%' || !def_in_loop.count(op);
        };
        std::unordered_map<std::string, int> use_cnt;
        for (int b = 0; b < N; ++b) {
            if (!blocks[b].used) continue;
            for (const auto& inst : blocks[b].insts) {
                if (inst.op == IROp::PHI) {
                    for (const auto& [lbl, val] : inst.phi_in) {
                        if (!val.empty() && val[0] == '%') use_cnt[val]++;
                    }
                } else {
                    if (!inst.src1.empty() && inst.src1[0] == '%') use_cnt[inst.src1]++;
                    if (!inst.src2.empty() && inst.src2[0] == '%') use_cnt[inst.src2]++;
                    for (const auto& arg : inst.args) {
                        if (!arg.empty() && arg[0] == '%') use_cnt[arg]++;
                    }
                }
            }
        }
        struct BasicIV {
            std::string phi_reg;
            std::string init_val;
            std::string next_reg;
            long long step;
        };
        std::unordered_map<std::string, BasicIV> basic_ivs;
        for (const auto& inst : blocks[h].insts) {
            if (inst.op != IROp::PHI || inst.phi_in.size() != 2) continue;
            std::string init_v, next_v;
            for (const auto& [lbl, val] : inst.phi_in) {
                if (lbl == pre_lbl) init_v = val;
                else if (lbl == latch_lbl) next_v = val;
            }
            if (init_v.empty() || next_v.empty() || next_v[0] != '%') continue;
            for (const auto& l_inst : blocks[latch].insts) {
                if (l_inst.dst != next_v) continue;
                long long step_v = 0;
                if (l_inst.op == IROp::ADD) {
                    if (l_inst.src1 == inst.dst && try_parse_i64(l_inst.src2, step_v)) {
                        basic_ivs[inst.dst] = {inst.dst, init_v, next_v, step_v};
                    } else if (l_inst.src2 == inst.dst && try_parse_i64(l_inst.src1, step_v)) {
                        basic_ivs[inst.dst] = {inst.dst, init_v, next_v, step_v};
                    }
                } else if (l_inst.op == IROp::SUB && l_inst.src1 == inst.dst &&
                           try_parse_i64(l_inst.src2, step_v)) {
                    basic_ivs[inst.dst] = {inst.dst, init_v, next_v, -step_v};
                }
            }
        }
        if (basic_ivs.empty()) continue;
        struct Cand {
            int blk;
            std::string mul_dst;
            std::string iv_reg;
            std::string factor;
            bool has_bias = false;
            bool bias_is_sub = false;
            std::string bias_dst;
            std::string bias_op;
        };
        std::vector<Cand> cands;
        for (int b : body) {
            for (const auto& inst : blocks[b].insts) {
                if (inst.op != IROp::MUL || inst.dst.empty() || inst.dst[0] != '%') continue;
                std::string iv_op, fac_op;
                if (basic_ivs.count(inst.src1) && is_inv(inst.src2)) {
                    iv_op = inst.src1; fac_op = inst.src2;
                } else if (basic_ivs.count(inst.src2) && is_inv(inst.src1)) {
                    iv_op = inst.src2; fac_op = inst.src1;
                } else {
                    continue;
                }
                long long fac_c = 0;
                if (try_parse_i64(fac_op, fac_c) && is_cheap_const_mul(fac_c)) continue;
                Cand cand{b, inst.dst, iv_op, fac_op, false, false, "", ""};
                auto u_it = use_cnt.find(inst.dst);
                if (u_it != use_cnt.end() && u_it->second == 1) {
                    bool found = false;
                    for (int ub : body) {
                        for (const auto& u_inst : blocks[ub].insts) {
                            if (u_inst.dst.empty() || u_inst.dst[0] != '%') continue;
                            if (u_inst.op == IROp::ADD) {
                                if (u_inst.src1 == inst.dst && is_inv(u_inst.src2)) {
                                    cand.has_bias = true;
                                    cand.bias_is_sub = false;
                                    cand.bias_dst = u_inst.dst;
                                    cand.bias_op = u_inst.src2;
                                    found = true;
                                    break;
                                } else if (u_inst.src2 == inst.dst && is_inv(u_inst.src1)) {
                                    cand.has_bias = true;
                                    cand.bias_is_sub = false;
                                    cand.bias_dst = u_inst.dst;
                                    cand.bias_op = u_inst.src1;
                                    found = true;
                                    break;
                                }
                            } else if (u_inst.op == IROp::SUB &&
                                       u_inst.src1 == inst.dst && is_inv(u_inst.src2)) {
                                cand.has_bias = true;
                                cand.bias_is_sub = true;
                                cand.bias_dst = u_inst.dst;
                                cand.bias_op = u_inst.src2;
                                found = true;
                                break;
                            }
                        }
                        if (found) break;
                    }
                }
                cands.push_back(std::move(cand));
                if (cands.size() >= 3) break;
            }
            if (cands.size() >= 3) break;
        }
        if (cands.empty()) continue;
        std::unordered_map<std::string, std::string> replace_map;
        std::unordered_set<std::string> erase_dsts;
        std::vector<IRInst> pre_inserts, header_phis, latch_inserts;
        for (const auto& c : cands) {
            const auto& biv = basic_ivs[c.iv_reg];
            std::string aux_phi = new_reg();
            std::string aux_next = new_reg();
            long long init_c = 0, fac_c = 0;
            bool init_is_const = try_parse_i64(biv.init_val, init_c);
            bool fac_is_const = try_parse_i64(c.factor, fac_c);
            std::string mul_init;
            if (init_is_const && init_c == 0) {
                mul_init = "0";
            } else if (init_is_const && fac_is_const) {
                int32_t v0 = static_cast<int32_t>(
                    static_cast<unsigned long long>(init_c) * static_cast<unsigned long long>(fac_c));
                mul_init = std::to_string(v0);
            } else {
                mul_init = new_reg();
                pre_inserts.push_back({IROp::MUL, mul_init, biv.init_val, c.factor, {}, {}});
            }
            std::string aux_init = mul_init;
            if (c.has_bias) {
                long long m0 = 0, b0 = 0;
                bool m_const = try_parse_i64(mul_init, m0);
                bool b_const = try_parse_i64(c.bias_op, b0);
                if (m_const && m0 == 0 && !c.bias_is_sub) {
                    aux_init = c.bias_op;
                } else if (m_const && b_const) {
                    unsigned long long res = c.bias_is_sub
                        ? (static_cast<unsigned long long>(m0) - static_cast<unsigned long long>(b0))
                        : (static_cast<unsigned long long>(m0) + static_cast<unsigned long long>(b0));
                    aux_init = std::to_string(static_cast<int32_t>(res));
                } else {
                    aux_init = new_reg();
                    pre_inserts.push_back({c.bias_is_sub ? IROp::SUB : IROp::ADD, aux_init, mul_init, c.bias_op, {}, {}});
                }
            }
            std::string stride_op;
            if (fac_is_const) {
                int32_t st = static_cast<int32_t>(
                    static_cast<unsigned long long>(biv.step) * static_cast<unsigned long long>(fac_c));
                stride_op = std::to_string(st);
            } else if (biv.step == 1) {
                stride_op = c.factor;
            } else {
                stride_op = new_reg();
                pre_inserts.push_back({IROp::MUL, stride_op, std::to_string(biv.step), c.factor, {}, {}});
            }
            IRInst phi{};
            phi.op = IROp::PHI;
            phi.dst = aux_phi;
            phi.phi_in = {{pre_lbl, aux_init}, {latch_lbl, aux_next}};
            header_phis.push_back(std::move(phi));
            latch_inserts.push_back({IROp::ADD, aux_next, aux_phi, stride_op, {}, {}});
            erase_dsts.insert(c.mul_dst);
            if (c.has_bias) {
                erase_dsts.insert(c.bias_dst);
                replace_map[c.bias_dst] = aux_phi;
            } else {
                replace_map[c.mul_dst] = aux_phi;
            }
        }
        auto& p_insts = blocks[preheader].insts;
        p_insts.insert(p_insts.end() - 1, pre_inserts.begin(), pre_inserts.end());
        blocks[h].insts.insert(blocks[h].insts.begin(), header_phis.begin(), header_phis.end());
        auto& l_insts = blocks[latch].insts;
        l_insts.insert(l_insts.end() - 1, latch_inserts.begin(), latch_inserts.end());
        for (int b : body) {
            std::vector<IRInst> kept;
            kept.reserve(blocks[b].insts.size());
            for (auto& inst : blocks[b].insts) {
                if (erase_dsts.count(inst.dst)) continue;
                kept.push_back(std::move(inst));
            }
            blocks[b].insts = std::move(kept);
        }
        for (int b = 0; b < N; ++b) {
            if (!blocks[b].used) continue;
            for (auto& inst : blocks[b].insts) {
                if (inst.op == IROp::PHI) {
                    for (auto& [lbl, val] : inst.phi_in) {
                        auto it = replace_map.find(val);
                        if (it != replace_map.end()) val = it->second;
                    }
                } else {
                    auto it1 = replace_map.find(inst.src1);
                    if (it1 != replace_map.end()) inst.src1 = it1->second;
                    auto it2 = replace_map.find(inst.src2);
                    if (it2 != replace_map.end()) inst.src2 = it2->second;
                    for (auto& arg : inst.args) {
                        auto ita = replace_map.find(arg);
                        if (ita != replace_map.end()) arg = ita->second;
                    }
                }
            }
        }
    }
}

void run_linear_combine(std::vector<IRBlock>& blocks, const std::vector<int>& idom, int& ssa_reg_counter) {
    int N = static_cast<int>(blocks.size());
    auto new_reg = [&]() { return "%lc_" + std::to_string(ssa_reg_counter++); };
    std::vector<int> dom_depth(N, 0);
    for (int b = 0; b < N; ++b) {
        if (!blocks[b].used) continue;
        int d = 0, cur = b;
        while (cur > 0 && cur < N && idom[cur] >= 0 && idom[cur] != cur) {
            cur = idom[cur];
            ++d;
            if (d > N) break;
        }
        dom_depth[b] = d;
    }
    std::unordered_map<std::string, int> use_cnt;
    std::unordered_map<std::string, int> global_def_blk;
    std::unordered_map<std::string, const IRInst*> global_def_inst;
    for (int b = 0; b < N; ++b) {
        if (!blocks[b].used) continue;
        for (const auto& inst : blocks[b].insts) {
            if (!inst.dst.empty() && inst.dst[0] == '%') {
                global_def_blk[inst.dst] = b;
                global_def_inst[inst.dst] = &inst;
            }
            if (inst.op == IROp::PHI) {
                for (const auto& [lbl, val] : inst.phi_in) {
                    if (!val.empty() && val[0] == '%') use_cnt[val]++;
                }
            } else {
                if (!inst.src1.empty() && inst.src1[0] == '%') use_cnt[inst.src1]++;
                if (!inst.src2.empty() && inst.src2[0] == '%') use_cnt[inst.src2]++;
                for (const auto& arg : inst.args) {
                    if (!arg.empty() && arg[0] == '%') use_cnt[arg]++;
                }
            }
        }
    }
    std::unordered_set<std::string> basic_iv_set;
    for (int b = 0; b < N; ++b) {
        if (!blocks[b].used) continue;
        for (const auto& inst : blocks[b].insts) {
            if (inst.op != IROp::PHI || inst.dst.empty() || inst.dst[0] != '%') continue;
            for (const auto& [lbl, val] : inst.phi_in) {
                auto dit = global_def_inst.find(val);
                if (dit == global_def_inst.end()) continue;
                const IRInst* d = dit->second;
                long long dummy = 0;
                if (d->op == IROp::ADD &&
                    ((d->src1 == inst.dst && try_parse_i64(d->src2, dummy)) ||
                     (d->src2 == inst.dst && try_parse_i64(d->src1, dummy)))) {
                    basic_iv_set.insert(inst.dst);
                    break;
                }
                if (d->op == IROp::SUB && d->src1 == inst.dst && try_parse_i64(d->src2, dummy)) {
                    basic_iv_set.insert(inst.dst);
                    break;
                }
            }
        }
    }
    auto var_depth = [&](const std::string& v) -> int {
        auto it = global_def_blk.find(v);
        if (it == global_def_blk.end()) return 0;
        return dom_depth[it->second] + 1;
    };
    std::unordered_map<std::string, std::string> global_replace_map;
    for (int b = 0; b < N; ++b) {
        if (!blocks[b].used || blocks[b].insts.empty()) continue;
        auto& insts = blocks[b].insts;
        size_t M = insts.size();
        std::unordered_map<std::string, size_t> def_idx;
        for (size_t i = 0; i < M; ++i) {
            if (!insts[i].dst.empty() && insts[i].dst[0] == '%') {
                def_idx[insts[i].dst] = i;
            }
        }
        std::vector<bool> erased(M, false);
        std::unordered_map<size_t, std::vector<IRInst>> replacements_at;
        for (size_t i = M; i-- > 0;) {
            if (erased[i]) continue;
            const auto& root = insts[i];
            if (root.op != IROp::ADD && root.op != IROp::SUB) continue;
            if (root.dst.empty() || root.dst[0] != '%') continue;
            unsigned int const_sum = 0;
            std::unordered_map<std::string, unsigned int> coeffs;
            std::unordered_map<std::string, size_t> first_order;
            size_t order_tick = 0;
            std::vector<size_t> tree_indices;
            std::unordered_set<size_t> visited_idx;
            size_t old_sub_count = 0;
            std::vector<std::pair<int, int>> orig_var_ranks;
            int min_var_depth = 0x07fffffff;
            int max_depth_before_const = -1;
            bool saw_const = false;
            auto record_leaf = [&](const std::string& v, unsigned int scale) {
                coeffs[v] += scale;
                if (!first_order.count(v)) first_order[v] = order_tick++;
                int d = var_depth(v);
                int non_iv = basic_iv_set.count(v) ? 0 : 1;
                orig_var_ranks.push_back({d, non_iv});
                if (d < min_var_depth) min_var_depth = d;
            };
            auto collect = [&](auto& self, const std::string& op, unsigned int scale, size_t bound_idx, bool from_mul) -> void {
                if (op.empty() || scale == 0) return;
                long long cv = 0;
                if (try_parse_i64(op, cv)) {
                    const_sum += scale * static_cast<unsigned int>(cv);
                    saw_const = true;
                    if (!orig_var_ranks.empty()) {
                        max_depth_before_const = std::max(max_depth_before_const, orig_var_ranks.back().first);
                    }
                    return;
                }
                if (op[0] != '%') return;
                auto dit = def_idx.find(op);
                if (dit == def_idx.end() || dit->second >= bound_idx || erased[dit->second]) {
                    record_leaf(op, scale);
                    return;
                }
                size_t idx = dit->second;
                auto uit = use_cnt.find(op);
                if (uit == use_cnt.end() || uit->second != 1 || visited_idx.count(idx)) {
                    record_leaf(op, scale);
                    return;
                }
                const auto& ins = insts[idx];
                if (ins.op == IROp::MOV) {
                    visited_idx.insert(idx);
                    tree_indices.push_back(idx);
                    self(self, ins.src1, scale, idx, from_mul);
                    return;
                }
                if (!from_mul && ins.op == IROp::ADD) {
                    visited_idx.insert(idx);
                    tree_indices.push_back(idx);
                    self(self, ins.src1, scale, idx, false);
                    self(self, ins.src2, scale, idx, false);
                    return;
                }
                if (!from_mul && ins.op == IROp::SUB) {
                    visited_idx.insert(idx);
                    tree_indices.push_back(idx);
                    ++old_sub_count;
                    self(self, ins.src1, scale, idx, false);
                    self(self, ins.src2, 0u - scale, idx, false);
                    return;
                }
                if (ins.op == IROp::MUL) {
                    long long c1 = 0, c2 = 0;
                    if (try_parse_i64(ins.src2, c2)) {
                        visited_idx.insert(idx);
                        tree_indices.push_back(idx);
                        self(self, ins.src1, scale * static_cast<unsigned int>(c2), idx, true);
                        return;
                    }
                    if (try_parse_i64(ins.src1, c1)) {
                        visited_idx.insert(idx);
                        tree_indices.push_back(idx);
                        self(self, ins.src2, scale * static_cast<unsigned int>(c1), idx, true);
                        return;
                    }
                }
                record_leaf(op, scale);
            };
            visited_idx.insert(i);
            tree_indices.push_back(i);
            if (root.op == IROp::ADD) {
                collect(collect, root.src1, 1u, i, false);
                collect(collect, root.src2, 1u, i, false);
            } else {
                ++old_sub_count;
                collect(collect, root.src1, 1u, i, false);
                collect(collect, root.src2, 0u - 1u, i, false);
            }
            struct Term {
                std::string var;
                int32_t coeff;
                bool is_neg;
                int depth;
                int non_iv;
                size_t order;
            };
            std::vector<Term> terms;
            terms.reserve(coeffs.size());
            for (const auto& [v, u_c] : coeffs) {
                if (u_c == 0) continue;
                int32_t c = static_cast<int32_t>(u_c);
                bool neg = (c < 0 && c != (1 << 31));
                terms.push_back({v, c, neg, var_depth(v), basic_iv_set.count(v) ? 0 : 1, first_order[v]});
            }
            std::sort(terms.begin(), terms.end(), [](const Term& a, const Term& b) {
                return std::tie(a.depth, a.is_neg, a.non_iv, a.order) <
                       std::tie(b.depth, b.is_neg, b.non_iv, b.order);
            });
            int32_t const_c = static_cast<int32_t>(const_sum);
            size_t old_inst_count = tree_indices.size();
            if (terms.empty()) {
                for (size_t idx : tree_indices) erased[idx] = true;
                global_replace_map[root.dst] = std::to_string(const_c);
                continue;
            }
            if (terms.size() == 1 && !terms[0].is_neg && terms[0].coeff == 1 && const_c == 0) {
                for (size_t idx : tree_indices) erased[idx] = true;
                global_replace_map[root.dst] = terms[0].var;
                continue;
            }
            std::vector<IRInst> new_insts;
            size_t new_sub_count = 0;
            auto materialize = [&](const Term& t) -> std::string {
                if (t.coeff == 1 || t.coeff == -1) return t.var;
                if (t.coeff == (1 << 31)) {
                    std::string tmp = new_reg();
                    new_insts.push_back({IROp::MUL, tmp, t.var, std::to_string((1 << 31)), {}, {}});
                    return tmp;
                }
                int32_t mag = t.is_neg ? -t.coeff : t.coeff;
                std::string tmp = new_reg();
                new_insts.push_back({IROp::MUL, tmp, t.var, std::to_string(mag), {}, {}});
                return tmp;
            };
            std::string acc;
            size_t start_k = 0;
            if (!terms[0].is_neg) {
                acc = materialize(terms[0]);
                start_k = 1;
                if (const_c != 0) {
                    std::string tmp = new_reg();
                    new_insts.push_back({IROp::ADD, tmp, acc, std::to_string(const_c), {}, {}});
                    acc = tmp;
                }
            } else {
                acc = std::to_string(const_c);
                start_k = 0;
            }
            for (size_t k = start_k; k < terms.size(); ++k) {
                std::string rhs = materialize(terms[k]);
                std::string tmp = new_reg();
                if (!terms[k].is_neg) {
                    new_insts.push_back({IROp::ADD, tmp, acc, rhs, {}, {}});
                } else {
                    new_insts.push_back({IROp::SUB, tmp, acc, rhs, {}, {}});
                    ++new_sub_count;
                }
                acc = tmp;
            }
            bool has_order_improvement = false;
            if (old_inst_count >= 2) {
                for (size_t k = 1; k < orig_var_ranks.size(); ++k) {
                    if (orig_var_ranks[k] < orig_var_ranks[k - 1]) {
                        has_order_improvement = true;
                        break;
                    }
                }
                if (saw_const && min_var_depth != 0x7fffffff && max_depth_before_const > min_var_depth) {
                    has_order_improvement = true;
                }
            }
            size_t new_inst_count = new_insts.size();
            bool profitable =
                (new_inst_count < old_inst_count) ||
                (new_sub_count < old_sub_count && new_inst_count <= old_inst_count) ||
                (has_order_improvement && new_inst_count == old_inst_count && new_sub_count <= old_sub_count);

            if (!profitable || new_insts.empty()) continue;
            new_insts.back().dst = root.dst;
            for (size_t idx : tree_indices) erased[idx] = true;
            replacements_at[i] = std::move(new_insts);
        }
        if (!replacements_at.empty() || std::any_of(erased.begin(), erased.end(), [](bool e) { return e; })) {
            std::vector<IRInst> rebuilt;
            rebuilt.reserve(M);
            for (size_t idx = 0; idx < M; ++idx) {
                auto rit = replacements_at.find(idx);
                if (rit != replacements_at.end()) {
                    for (auto& ni : rit->second) rebuilt.push_back(std::move(ni));
                } else if (!erased[idx]) {
                    rebuilt.push_back(std::move(insts[idx]));
                }
            }
            insts = std::move(rebuilt);
        }
    }
    if (!global_replace_map.empty()) {
        auto resolve = [&](std::string& op) {
            auto it = global_replace_map.find(op);
            while (it != global_replace_map.end()) {
                op = it->second;
                it = global_replace_map.find(op);
            }
        };
        for (int b = 0; b < N; ++b) {
            if (!blocks[b].used) continue;
            for (auto& inst : blocks[b].insts) {
                if (inst.op == IROp::PHI) {
                    for (auto& [lbl, val] : inst.phi_in) resolve(val);
                } else {
                    resolve(inst.src1);
                    resolve(inst.src2);
                    for (auto& arg : inst.args) resolve(arg);
                }
            }
        }
    }
}