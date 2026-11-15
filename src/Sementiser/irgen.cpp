#include "irgen.h"

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
    body_insts.push_back({IROp::LABEL, label, "", "", {}});
    block_terminated = false;
}
