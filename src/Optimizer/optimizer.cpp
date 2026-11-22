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

void run_licm(std::vector<IRBlock> blocks, std::vector<int>& idom) {
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
            if (is_leaf) {
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
