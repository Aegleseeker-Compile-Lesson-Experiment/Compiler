#include "irgen.h"

inline bool is_vreg(const std::string& s) {
    return !s.empty() && s[0] == '%';
}

std::vector<std::string> get_defs(const IRInst& inst) {
    switch (inst.op) {
        case IROp::ADD: case IROp::SUB: case IROp::MUL:
        case IROp::SDIV: case IROp::SREM:
        case IROp::ICMP_EQ: case IROp::ICMP_NE:
        case IROp::ICMP_SLT: case IROp::ICMP_SGT:
        case IROp::ICMP_SLE: case IROp::ICMP_SGE:
        case IROp::MOV: case IROp::LOAD:
            if (is_vreg(inst.dst)) return {inst.dst};
            break;
        case IROp::CALL:
            if (is_vreg(inst.dst)) return {inst.dst};
            break;
        default:
            break;
    }
    return {};
}

std::vector<std::string> get_uses(const IRInst& inst) {
    std::vector<std::string> res;
    auto add_if_vreg = [&](const std::string& op) {
        if (is_vreg(op)) res.push_back(op);
    };
    switch (inst.op) {
        case IROp::ADD: case IROp::SUB: case IROp::MUL:
        case IROp::SDIV: case IROp::SREM:
        case IROp::ICMP_EQ: case IROp::ICMP_NE:
        case IROp::ICMP_SLT: case IROp::ICMP_SGT:
        case IROp::ICMP_SLE: case IROp::ICMP_SGE:
            add_if_vreg(inst.src1);
            add_if_vreg(inst.src2);
            break;
        case IROp::MOV:
        case IROp::BR_COND:
        case IROp::RET:
        case IROp::LOAD:
            add_if_vreg(inst.src1);
            break;
        case IROp::STORE:
            add_if_vreg(inst.src1);
            add_if_vreg(inst.src2);
            break;
        case IROp::CALL:
            for (const auto& arg : inst.args) add_if_vreg(arg);
            break;
        default:
            break;
    }
    return res;
}

void IRGenerator::emit(const std::string& inst) {
    if (!block_terminated) {
        ir_body << "  " << inst << "\n";
    }
}

void IRGenerator::emit_inst(const IRInst& inst) {
    if (!block_terminated) {
        body_insts.push_back(inst);
    }
}

void IRGenerator::emit_label(const std::string& label) {
    ir_body << label << ":\n";
    body_insts.push_back({IROp::LABEL, label, "", "", {}, {}});
    block_terminated = false;
}

int IRGenerator::get_block(const std::string& label) {
    if (label_block.find(label) == label_block.end()) {
        label_block[label] = blocks.size();
        blocks.push_back(IRBlock(label));
        return blocks.size() - 1;
    } else {
        return label_block[label];
    }
}

void IRGenerator::construct_blocks(const IRFunc& func) {
    blocks.push_back(IRBlock(func.name));
    for (const auto& inst : func.insts) {
        if (inst.op == IROp::LABEL) {
            cur_block = get_block(inst.dst);
            continue;
        } else if (inst.op == IROp::JMP) {
            auto to = get_block(inst.dst);
            blocks[cur_block].succs.push_back(to);
            blocks[to].preds.push_back(cur_block);
        } else if (inst.op == IROp::BR_COND) {
            auto to = get_block(inst.dst);
            blocks[cur_block].succs.push_back(to);
            blocks[to].preds.push_back(cur_block);
            to = get_block(inst.src2);
            blocks[cur_block].succs.push_back(to);
            blocks[to].preds.push_back(cur_block);
        }
        blocks[cur_block].insts.push_back(inst);
    }
}

std::vector<LiveInterval> IRGenerator::mem2reg(const std::vector<std::string> params) {
    dfs_block(0);
    for (auto &block : blocks) {
        if (!block.used && block.label != "main") {
            block = IRBlock("");
        }
    }
    auto _ = cooper();
    auto DF = _.first;
    auto idom = _.second;
    // run_licm(idom);
    std::vector<std::string> alloca_vars;
    std::unordered_set<std::string> alloca_set;
    for (const auto& inst : blocks[0].insts) {
        if (inst.op == IROp::ALLOCA) {
            alloca_vars.push_back(inst.dst);
            alloca_set.insert(inst.dst);
        }
    }
    std::unordered_map<std::string, std::unordered_set<int>> def_blocks;
    for (int b = 0; b < static_cast<int>(blocks.size()); ++b) {
        for (const auto& inst : blocks[b].insts) {
            if (inst.op == IROp::STORE) {
                if (alloca_set.count(inst.src2)) {
                    def_blocks[inst.src2].insert(b);
                }
            }
        }
    }
    create_phi(DF, alloca_vars, def_blocks);
    rename_var(alloca_vars, idom);
    split_critical_edges();
    eliminate_phis();
    int N = static_cast<int>(blocks.size());
    std::vector<int> block_start(N, 0);
    std::vector<int> block_end(N, -1);
    std::vector<int> call_indices;
    int cur_idx = 0;
    for (int b = 0; b < N; ++b) {
        block_start[b] = 2 * cur_idx;
        for (const auto& inst : blocks[b].insts) {
            if (inst.op == IROp::CALL) {
                call_indices.push_back(2 * cur_idx);
            }
            cur_idx++;
        }
        block_end[b] = (2 * cur_idx > block_start[b]) ? (2 * cur_idx - 1) : block_start[b];
    }
    auto live_out = live_inout();
    auto intervals = construct_intervals(params, block_start, block_end, call_indices, live_out);
    return intervals;
}

void IRGenerator::dfs_block(int id) {
    if (blocks[id].used) {
        return;
    } else {
        blocks[id].used = true;
        for (const auto i : blocks[id].succs) {
            dfs_block(i);
        }
    }
}

std::pair<std::vector<std::unordered_set<int>>, std::vector<int>> IRGenerator::cooper() {
    std::vector<int> rpo_order;
    std::vector<bool> vis(blocks.size(), false);
    auto dfs_rpo = [&](auto& self, int u) -> void {
        vis[u] = true;
        for (int v : blocks[u].succs) {
            if (!vis[v]) self(self, v);
        }
        rpo_order.push_back(u);
    };
    dfs_rpo(dfs_rpo, 0);
    std::reverse(rpo_order.begin(), rpo_order.end());
    auto temp_blocks = new std::vector<IRBlock>;
    std::vector<int> rpo_order_mir(blocks.size(), 0);
    int cnt = 0;
    for (const auto& o : rpo_order) {
        temp_blocks->push_back(blocks[o]);
        rpo_order_mir[o] = cnt++;
    }
    blocks = *temp_blocks;
    for (auto& block : blocks) {
        for (auto& p : block.preds) p = rpo_order_mir[p];
        for (auto& p : block.succs) p = rpo_order_mir[p];
    }
    int N = blocks.size();
    std::vector<int> idom(N, -1);
    idom[0] = 0;
    auto intersect = [&](int b1, int b2) {
        while (b1 != b2) {
            while (b1 > b2) b1 = idom[b1];
            while (b2 > b1) b2 = idom[b2];
        }
        return b1;
    };
    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = 1; b < N; ++b) {
            int new_idom = -1;
            for (int p : blocks[b].preds) {
                if (idom[p] != -1) {
                    new_idom = (new_idom == -1) ? p : intersect(p, new_idom);
                }
            }
            if (new_idom != -1 && idom[b] != new_idom) {
                idom[b] = new_idom;
                changed = true;
            }
        }
    }
    std::vector<std::unordered_set<int>> DF(N);
    for (int b = 0; b < N; ++b) {
        if (blocks[b].preds.size() >= 2) {
            for (int p : blocks[b].preds) {
                int runner = p;
                while (runner != idom[b]) {
                    DF[runner].insert(b);
                    runner = idom[runner];
                }
            }
        }
    }
    return std::make_pair(DF, idom);
}

void IRGenerator::create_phi(std::vector<std::unordered_set<int>> &DF, std::vector<std::string> &alloca_vars, std::unordered_map<std::string, std::unordered_set<int>> &def_blocks) {
    for (const auto& v : alloca_vars) {
        std::queue<int> worklist;
        std::unordered_set<int> has_phi, in_worklist;
        for (int b : def_blocks[v]) {
            worklist.push(b);
            in_worklist.insert(b);
        }
        while (!worklist.empty()) {
            int b = worklist.front();
            worklist.pop();
            for (int y : DF[b]) {
                if (!has_phi.count(y)) {
                    IRInst phi;
                    phi.op = IROp::PHI;
                    phi.dst = new_ssa_reg();
                    phi.src1 = v;
                    blocks[y].insts.insert(blocks[y].insts.begin(), phi);
                    has_phi.insert(y);
                    if (!in_worklist.count(y)) {
                        worklist.push(y);
                        in_worklist.insert(y);
                    }
                }
            }
        }
    }
}

void IRGenerator::rename_var(std::vector<std::string> &alloca_vars, std::vector<int> &idom) {
    std::vector<std::vector<int>> dom_children(blocks.size());
    for (size_t b = 1; b < blocks.size(); ++b) {
        dom_children[idom[b]].push_back(b);
    }
    std::unordered_map<std::string, std::vector<std::string>> ver_stack;
    for (const auto& v : alloca_vars) ver_stack[v].push_back("0");
    std::unordered_map<std::string, std::string> alias;
    auto resolve_alias = [&](std::string& op) {
        while (alias.count(op)) op = alias[op];
    };
    auto rename_dfs = [&](auto& self, int b) -> void {
        std::unordered_map<std::string, size_t> saved_height;
        for (const auto& v : alloca_vars) saved_height[v] = ver_stack[v].size();
        std::vector<IRInst> kept_insts;
        for (auto& inst : blocks[b].insts) {
            if (inst.op != IROp::PHI) {
                resolve_alias(inst.src1);
                resolve_alias(inst.src2);
                for (auto& arg : inst.args) resolve_alias(arg);
            }
            if (inst.op == IROp::ALLOCA) {
                continue;
            } else if (inst.op == IROp::PHI) {
                std::string v = inst.src1;
                ver_stack[v].push_back(inst.dst);
                kept_insts.push_back(inst);
            } else if (inst.op == IROp::LOAD) {
                alias[inst.dst] = ver_stack[inst.src1].back();
                continue;
            } else if (inst.op == IROp::STORE) {
                ver_stack[inst.src2].push_back(inst.src1);
                continue;
            } else {
                kept_insts.push_back(inst);
            }
        }
        blocks[b].insts = std::move(kept_insts);
        for (int succ : blocks[b].succs) {
            for (auto& inst : blocks[succ].insts) {
                if (inst.op == IROp::PHI) {
                    std::string v = inst.src1;
                    inst.phi_in.push_back({blocks[b].label, ver_stack[v].back()});
                }
            }
        }
        for (int child : dom_children[b]) {
            self(self, child);
        }
        for (const auto& v : alloca_vars) {
            ver_stack[v].resize(saved_height[v]);
        }
    };
    rename_dfs(rename_dfs, 0);
}

void IRGenerator::split_critical_edges() {
    int split_id = 0;
    int orig_count = static_cast<int>(blocks.size());
    auto has_phi = [](const IRBlock& blk) {
        for (const auto& inst : blk.insts) {
            if (inst.op == IROp::PHI) return true;
        }
        return false;
    };
    for (int p = 0; p < orig_count; ++p) {
        if (blocks[p].succs.size() <= 1) continue;
        std::vector<int> old_succs = blocks[p].succs;
        for (int s : old_succs) {
            if (blocks[s].preds.size() <= 1 || !has_phi(blocks[s])) continue;
            int m = static_cast<int>(blocks.size());
            std::string split_label = "Lsplit_" + std::to_string(split_id++);
            std::string s_label = blocks[s].label;
            std::string p_label = blocks[p].label;
            auto mid_blk = IRBlock(split_label);
            mid_blk.insts.push_back({IROp::JMP, s_label, "", "", {}, {}});
            mid_blk.preds = {p};
            mid_blk.succs = {s};
            blocks.push_back(std::move(mid_blk));
            auto& term = blocks[p].insts.back();
            if (term.op == IROp::BR_COND) {
                if (term.dst == s_label) term.dst = split_label;
                if (term.src2 == s_label) term.src2 = split_label;
            }
            for (auto& inst : blocks[s].insts) {
                if (inst.op == IROp::PHI) {
                    for (auto& [pred_lbl, val] : inst.phi_in) {
                        if (pred_lbl == p_label) {
                            pred_lbl = split_label;
                        }
                    }
                }
            }
            for (int& succ_id : blocks[p].succs) {
                if (succ_id == s) { succ_id = m; break; }
            }
            for (int& pred_id : blocks[s].preds) {
                if (pred_id == p) { pred_id = m; break; }
            }
        }
    }
}

void IRGenerator::eliminate_phis() {
    std::unordered_map<std::string, int> label_to_id;
    for (int i = 0; i < static_cast<int>(blocks.size()); ++i) {
        label_to_id[blocks[i].label] = i;
    }
    std::unordered_map<int, std::vector<std::pair<std::string, std::string>>> pending_copies;
    for (auto& blk : blocks) {
        std::vector<IRInst> non_phi_insts;
        for (const auto& inst : blk.insts) {
            if (inst.op == IROp::PHI) {
                for (const auto& [pred_label, val] : inst.phi_in) {
                    if (inst.dst == val) continue;
                    int pred_id = label_to_id[pred_label];
                    pending_copies[pred_id].push_back({inst.dst, val});
                }
            } else {
                non_phi_insts.push_back(inst);
            }
        }
        blk.insts = std::move(non_phi_insts);
    }
    for (auto& [pred_id, copies] : pending_copies) {
        if (copies.empty()) continue;
        std::vector<IRInst> mov_insts;
        if (copies.size() == 1) {
            mov_insts.push_back({IROp::MOV, copies[0].first, copies[0].second, "", {}, {}});
        } else {
            std::vector<std::string> tmps;
            for (const auto& [dst, src] : copies) {
                std::string tmp = "%phi_tmp_" + std::to_string(ssa_id++);
                tmps.push_back(tmp);
                mov_insts.push_back({IROp::MOV, tmp, src, "", {}, {}});
            }
            for (size_t i = 0; i < copies.size(); ++i) {
                mov_insts.push_back({IROp::MOV, copies[i].first, tmps[i], "", {}, {}});
            }
        }
        auto& p_insts = blocks[pred_id].insts;
        auto insert_pos = p_insts.empty() ? p_insts.end() : (p_insts.end() - 1);
        p_insts.insert(insert_pos, mov_insts.begin(), mov_insts.end());
    }
}

std::vector<std::unordered_set<std::string>> IRGenerator::live_inout() {
    auto N = static_cast<int>(blocks.size());
    std::vector<std::unordered_set<std::string>> ue_use(N), block_def(N);
    std::vector<std::unordered_set<std::string>> live_in(N), live_out(N);
    for (int b = 0; b < N; ++b) {
        for (const auto& inst : blocks[b].insts) {
            for (const auto& u : get_uses(inst)) {
                if (!block_def[b].count(u)) {
                    ue_use[b].insert(u);
                }
            }
            for (const auto& d : get_defs(inst)) {
                block_def[b].insert(d);
            }
        }
    }
    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = N - 1; b >= 0; --b) {
            std::unordered_set<std::string> new_out;
            for (int s : blocks[b].succs) {
                new_out.insert(live_in[s].begin(), live_in[s].end());
            }
            std::unordered_set<std::string> new_in = ue_use[b];
            for (const auto& v : new_out) {
                if (!block_def[b].count(v)) {
                    new_in.insert(v);
                }
            }
            if (new_out != live_out[b] || new_in != live_in[b]) {
                live_out[b] = std::move(new_out);
                live_in[b] = std::move(new_in);
                changed = true;
            }
        }
    }
    return live_out;
}

std::vector<LiveInterval> IRGenerator::construct_intervals(const std::vector<std::string>& params, const std::vector<int>& block_start, const std::vector<int>& block_end, const std::vector<int>& call_indices, std::vector<std::unordered_set<std::string>> live_out) {
    auto N = static_cast<int>(blocks.size());
    std::unordered_map<std::string, LiveInterval> interval_map;
    auto add_range = [&](const std::string& reg, int from, int to) {
        auto& iv = interval_map[reg];
        iv.reg = reg;
        iv.start = std::min(iv.start, from);
        iv.end = std::max(iv.end, to);
    };
    for (int b = N - 1; b >= 0; --b) {
        for (const auto& v : live_out[b]) {
            add_range(v, block_start[b], block_end[b]);
        }
        int inst_idx = (block_end[b] - 1) / 2;
        for (int i = static_cast<int>(blocks[b].insts.size()) - 1; i >= 0; --i, --inst_idx) {
            const auto& inst = blocks[b].insts[i];
            int use_pos = 2 * inst_idx;
            int def_pos = 2 * inst_idx + 1;
            for (const auto& d : get_defs(inst)) {
                auto& iv = interval_map[d];
                iv.reg = d;
                if (iv.end == -1) iv.end = def_pos;
                if (iv.start == block_start[b]) {
                    iv.start = def_pos;
                } else {
                    iv.start = std::min(iv.start, def_pos);
                }
            }
            for (const auto& u : get_uses(inst)) {
                add_range(u, block_start[b], use_pos);
            }
        }
    }
    for (auto param : params) {
        if (!param.empty() && param[0] != '%') param = "%" + param;
        if (interval_map.count(param)) {
            interval_map[param].start = -1;
        }
    }
    std::vector<LiveInterval> intervals;
    intervals.reserve(interval_map.size());
    for (auto& [reg, iv] : interval_map) {
        for (int call_idx : call_indices) {
            if (iv.start < call_idx && call_idx < iv.end) {
                iv.crosses_call = true;
                break;
            }
        }
        intervals.push_back(std::move(iv));
    }
    return intervals;
}

void IRGenerator::inline_functions(std::vector<IRFunc>& funcs) {
    std::unordered_map<std::string, IRFunc> func_map;
    for (const auto& fn : funcs) {
        bool is_leaf = true;
        for (const auto& inst : fn.insts) {
            if (inst.op == IROp::CALL) { is_leaf = false; break; }
        }
        if (is_leaf && fn.insts.size() <= 80) {
            func_map[fn.name] = fn;
        }
    }
    int inline_id = 0;
    for (auto& caller : funcs) {
        std::vector<IRInst> hoisted_allocas;
        std::vector<IRInst> new_insts;
        for (const auto& call_inst : caller.insts) {
            if (call_inst.op != IROp::CALL || call_inst.src1 == caller.name || !func_map.count(call_inst.src1)) {
                new_insts.push_back(call_inst);
                continue;
            }
            const auto& callee = func_map[call_inst.src1];
            std::string suffix = ".inl_" + std::to_string(inline_id++);
            std::string exit_label = "L_inl_end" + suffix;
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
        if (!hoisted_allocas.empty()) {
            new_insts.insert(new_insts.begin(), hoisted_allocas.begin(), hoisted_allocas.end());
        }
        caller.insts = std::move(new_insts);
    }
}

void IRGenerator::run_licm(std::vector<int>& idom) {
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
