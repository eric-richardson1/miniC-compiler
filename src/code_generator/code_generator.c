
#include "code_generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <algorithm>
#include <string>
#include <stdbool.h>

using namespace std;

bool isReturnTypeVoid(LLVMValueRef call_inst) {
    if (LLVMIsACallInst(call_inst)) {
        LLVMTypeRef func_type = LLVMGetCalledFunctionType(call_inst);
        LLVMTypeRef ret_type = LLVMGetReturnType(func_type);

        if (ret_type == LLVMVoidType()) {
            return true;
        }
    }
    return false;
}

void computeLiveness(LLVMBasicBlockRef bb, std::unordered_map<LLVMValueRef, int> &inst_index, std::unordered_map<LLVMValueRef, std::pair<int, int>> &live_range) {
    int i = 0;
    
    for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
        
        if (LLVMIsAAllocaInst(instruction)) {
            continue;
        }
        std::pair<LLVMValueRef, int> index_entry (instruction, i);
        inst_index.insert(index_entry);

        i += 1;
    }

    for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
        if (LLVMIsAAllocaInst(instruction) || LLVMIsAStoreInst(instruction) || LLVMIsABranchInst(instruction) || LLVMIsAReturnInst(instruction) || isReturnTypeVoid(instruction)) {
            continue;
        }
        
        LLVMUseRef def_use = LLVMGetFirstUse(instruction);
        
        if (def_use == NULL) {
            int use_idx = inst_index.at(instruction);
            std::pair<int, int> single_use (use_idx, use_idx);
            std::pair<LLVMValueRef, std::pair<int, int>> single_use_entry (instruction, single_use);
            live_range.insert(single_use_entry);
        }
        else {
            LLVMValueRef last_use_inst = NULL;

            for (LLVMValueRef use_inst = LLVMGetUser(def_use); use_inst; use_inst = LLVMGetNextInstruction(use_inst)) {
                int num_operands = LLVMGetNumOperands(use_inst);
                for (int i = 0; i < num_operands; i++) {
                    if (instruction == LLVMGetOperand(use_inst, i)) {
                        last_use_inst = use_inst;
                    }
                }
            }

            std::pair<int, int> range (inst_index.at(instruction), inst_index.at(last_use_inst));

            
            std::pair<LLVMValueRef, std::pair<int, int>> range_entry (instruction, range);
            live_range.insert(range_entry);
        }
        
    }



}

bool reg_cmp(std::pair<LLVMValueRef, std::pair<int, int>> &a, std::pair<LLVMValueRef, std::pair<int, int>> &b) {
    return a.second.second > b.second.second;
}

std::vector<LLVMValueRef> sortLivenessMap(std::unordered_map<LLVMValueRef, std::pair<int, int>> &live_range) {
    std::vector<LLVMValueRef> res;
    std::vector<std::pair<LLVMValueRef, std::pair<int, int>>> intermediate_res;

    for (std::unordered_map<LLVMValueRef, std::pair<int, int>>::iterator iter = live_range.begin(); iter != live_range.end(); iter++) {
        intermediate_res.push_back(*iter);
    }

    std::sort(intermediate_res.begin(), intermediate_res.end(), reg_cmp);

    for (std::vector<std::pair<LLVMValueRef, std::pair<int, int>>>::iterator iter = intermediate_res.begin(); iter != intermediate_res.end(); iter++) {

        std::pair<LLVMValueRef, std::pair<int, int>> ele = *iter;

        res.push_back(ele.first);
    }

    return res;

}

LLVMValueRef findSpill(LLVMBasicBlockRef bb, std::vector<LLVMValueRef> &sorted_vals, std::unordered_map<LLVMValueRef, int> &reg_map) {
    for (int i = 0; i < sorted_vals.size(); i++) {
        if (reg_map.count(sorted_vals.at(i)) && reg_map.at(sorted_vals.at(i)) != SPILL) {
            return sorted_vals.at(i);
        }
    }
    return NULL;
}

std::unordered_map<LLVMValueRef, int> allocateRegisters(LLVMValueRef function, std::unordered_map<LLVMValueRef, int> &inst_index, std::unordered_map<LLVMValueRef, std::pair<int, int>> &live_range) {
    std::unordered_map<LLVMValueRef, int> reg_map;
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
        std::unordered_set<int> available_registers ({EBX, ECX, EDX});
        
        computeLiveness(bb, inst_index, live_range);
        
        for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
            if (LLVMIsAAllocaInst(instruction)) { 
                continue;
            }
            if (LLVMIsAStoreInst(instruction) || LLVMIsABranchInst(instruction) || LLVMIsAReturnInst(instruction) || isReturnTypeVoid(instruction)) {
                int num_operands = LLVMGetNumOperands(instruction);
                
                for (int i = 0; i < num_operands; i++) {
                    LLVMValueRef op = LLVMGetOperand(instruction, i);
                    
                    if (live_range.count(op)) {
                        
                        if (live_range.at(op).second <= inst_index.at(instruction) && reg_map.count(op) && reg_map.at(op) != SPILL) {
                            available_registers.insert(reg_map.at(op));
                        }
                    }
                    
                    
                }
   
            }
            else {
                
                LLVMOpcode opcode = LLVMGetInstructionOpcode(instruction);
                if (opcode == LLVMAdd || opcode == LLVMSub || opcode == LLVMMul) {
                    
                    
                    LLVMValueRef first_op = LLVMGetOperand(instruction, 0);

                    if (reg_map.count(first_op) && reg_map.at(first_op) != SPILL && live_range.at(first_op).second <= inst_index.at(instruction)) {
                        
                        std::unordered_set<int>::iterator reg_it = available_registers.begin();
                        int reg = *reg_it;
                        
                        available_registers.erase(reg);
                        

                        std::pair<LLVMValueRef, int> reg_entry (instruction, reg);
                        reg_map.insert(reg_entry);

                        LLVMValueRef second_op = LLVMGetOperand(instruction, 1);
                        
                        if (reg_map.count(second_op) && reg_map.at(second_op) != SPILL && live_range.at(second_op).second <= inst_index.at(instruction)) {
                            available_registers.insert(reg_map.at(second_op));
                        }
                        
                        
                        
                    }
                    
                    
                    
                }
                else if (!available_registers.empty()) {
 
                    std::unordered_set<int>::iterator reg_it = available_registers.begin();
                    
                    int reg = *reg_it;
                    
                    available_registers.erase(reg);
                    
                    std::pair<LLVMValueRef, int> reg_entry (instruction, reg);
                    
                    reg_map.insert(reg_entry);
                    

                    int num_operands = LLVMGetNumOperands(instruction);
                    for (int i = 0; i < num_operands; i++) {
                        LLVMValueRef op = LLVMGetOperand(instruction, i);

                        if (live_range.count(op) && live_range.at(op).second <= inst_index.at(instruction) && reg_map.count(op) && reg_map.at(op) != SPILL) {
                            available_registers.insert(reg_map.at(op));
                        }
                       
                    }
    
                }
                else {
                    vector<LLVMValueRef> sorted_vals = sortLivenessMap(live_range);
                    LLVMValueRef to_spill = findSpill(bb, sorted_vals, reg_map);

                    if (live_range.at(to_spill).second < live_range.at(instruction).second) {
                        
                        std::pair<LLVMValueRef, int> spill_entry (instruction, SPILL);
                        reg_map.insert(spill_entry);
                    }
                    else {
                        int reg = reg_map.at(to_spill);
                        std::pair<LLVMValueRef, int> reg_entry (instruction, reg);
                        reg_map.insert(reg_entry);

                        reg_map.erase(to_spill);
                        std::pair<LLVMValueRef, int> spill_entry (to_spill, SPILL);
                        reg_map.insert(spill_entry);
                    }
                   
                }
    
            }
            int num_operands = LLVMGetNumOperands(instruction);
            for (int i = 0; i < num_operands; i++) {
                LLVMValueRef op = LLVMGetOperand(instruction, i);

                if (live_range.count(op) && live_range.at(op).second <= inst_index.at(instruction) && reg_map.count(op) && reg_map.at(op) != SPILL) {
                    
                    available_registers.insert(reg_map.at(op));
                }
                    
            }
                
        }
        
        inst_index.clear();
        live_range.clear();

    }
    return reg_map;
    
}

std::unordered_map<LLVMValueRef, int> getOffsetMap(LLVMValueRef function, int *local_mem) {
    std::unordered_map<LLVMValueRef, int> offset_map;
    LLVMValueRef param;

    if (LLVMCountParams(function)) {
        param = LLVMGetParam(function, 0);
        *local_mem = 4;
    }
    
    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
        for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
            if (instruction == param) {
                continue;
            }
            if (LLVMIsAAllocaInst(instruction)) {
                *local_mem -= 4;
                std::pair<LLVMValueRef, int> offset_entry (instruction, *local_mem);
                offset_map.insert(offset_entry);
            }
            if (LLVMIsAStoreInst(instruction) && LLVMGetOperand(instruction, 0) == param) {
                LLVMValueRef param_var = LLVMGetOperand(instruction, 1);
                if (offset_map.count(param_var)) {
                    offset_map.erase(param_var);
                }
                std::pair<LLVMValueRef, int> offset_entry (param_var, 8);
                offset_map.insert(offset_entry);
            }
            else if (LLVMIsAStoreInst(instruction)) {
                LLVMValueRef ptr_val = LLVMGetOperand(instruction, 0);
                LLVMValueRef ptr_loc = LLVMGetOperand(instruction, 1);           
                
                std::pair<LLVMValueRef, int> offset_entry (ptr_val, offset_map.at(ptr_loc));
                offset_map.insert(offset_entry);

            }
            else if (LLVMIsALoadInst(instruction)) {
               
                LLVMValueRef ptr_loc = LLVMGetOperand(instruction, 0);
                std::pair<LLVMValueRef, int> offset_entry (instruction, offset_map.at(ptr_loc));
                offset_map.insert(offset_entry);
                
            }

            
        }
    }
    *local_mem *= -1;
    if (param != NULL) {
        *local_mem += 8;
    }
    return offset_map;
}

std::unordered_map<LLVMBasicBlockRef, std::string> createBBLabels(LLVMValueRef function) {
    
    std::unordered_map<LLVMBasicBlockRef, std::string> bb_labels;

    LLVMBasicBlockRef first_bb = LLVMGetFirstBasicBlock(function);
    const char *first_label = ".LFB0";

    std::pair<LLVMBasicBlockRef, std::string> first (first_bb, first_label);
    bb_labels.insert(first);

    int count = 1;
    for (LLVMBasicBlockRef bb = LLVMGetNextBasicBlock(first_bb); bb; bb = LLVMGetNextBasicBlock(bb)) {
        std::string label = ".L" + std::to_string(count);
        std::pair<LLVMBasicBlockRef, std::string> bblabel_entry (bb, label);
        bb_labels.insert(bblabel_entry);

        count += 1;
    }
    return bb_labels;
}

void printDirectives(LLVMModuleRef module, LLVMValueRef function, FILE *fp) {
    size_t flen;
    const char *fname = LLVMGetSourceFileName(module, &flen);

    size_t func_len;
    const char *func_name = LLVMGetValueName2(function, &func_len);

    fprintf(fp, "\t.file\t\"%s\"\n", fname);
    fprintf(fp, "\t.text\n");
    fprintf(fp, "\t.globl\t%s\n", func_name);
    fprintf(fp, "\t.type\t%s, @function\n", func_name);
    fprintf(fp, "%s:\n", func_name);
    fprintf(fp, ".LFB0:\n");
    fprintf(fp, "\tpushl\t%%ebp\n");
    fprintf(fp, "\tmovl\t%%esp, %%ebp\n");

}

void printFunctionEnd(FILE *fp) {
    fprintf(fp, "\tpopl\t%%ebx\n");
    fprintf(fp, "\tleave\n");
    fprintf(fp, "\tret\n");
}

const char *getRegisterStr(int reg) {
    switch (reg) {
        case EAX: {
            return "eax";
        }
        case EBX: {
            return "ebx";
        }
        case ECX: {
            return "ecx";
        }
        case EDX: {
            return "edx";
        }
        default: {
            return "";
        }
    }
}


void generateAssembly(LLVMModuleRef module) {
    
    FILE *fp = fopen("func.s", "w");
    std::unordered_map<LLVMValueRef, int> inst_index;
    std::unordered_map<LLVMValueRef, std::pair<int, int>> live_range;

    LLVMValueRef function = LLVMGetLastFunction(module);
    std::unordered_map<LLVMValueRef, int> reg_map = allocateRegisters(function, inst_index, live_range);
    

    int local_mem = 0;
    std::unordered_map<LLVMValueRef, int> offset_map = getOffsetMap(function, &local_mem);

    
    std::unordered_map<LLVMBasicBlockRef, std::string> bb_labels = createBBLabels(function);
    printDirectives(module, function, fp);
    fprintf(fp, "\tsubl\t$%d, %%esp\n", local_mem);
    fprintf(fp, "\tpushl\t%%ebx\n");

    for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
        if (bb != LLVMGetFirstBasicBlock(function)) {
            fprintf(fp, "\n%s:\n", bb_labels.at(bb).c_str());
        }
        

        for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
            switch (LLVMGetInstructionOpcode(instruction)) {
                case LLVMRet: {
                    LLVMValueRef ret_val = LLVMGetOperand(instruction, 0);
                    if (LLVMIsAConstantInt(ret_val)) {
                        int const_val = LLVMConstIntGetSExtValue(ret_val);
                        fprintf(fp, "\tmovl\t$%d, %%eax\n", const_val);
                    }
                    else if (reg_map.count(ret_val) && reg_map.at(ret_val) != SPILL) {
                        fprintf(fp, "\tmovl\t%%%s, %%eax\n", getRegisterStr(reg_map.at(ret_val)));
                    }
                    else {
                        int offset = offset_map.at(ret_val);
                        fprintf(fp, "\tmovl\t%d(%%ebp), %%eax\n", offset);
                    }
                    printFunctionEnd(fp);
                    break;
                }
                case LLVMLoad: {
                    if (reg_map.count(instruction) && reg_map.at(instruction) != SPILL) {
                        LLVMValueRef load_val = LLVMGetOperand(instruction, 0);
                        int offset = offset_map.at(load_val);
                        
                        fprintf(fp, "\tmovl\t%d(%%ebp), %%%s\n", offset, getRegisterStr(reg_map.at(instruction)));
                    }
                    break;
                }
                case LLVMStore: {
                    LLVMValueRef store_val = LLVMGetOperand(instruction, 0);
                    LLVMValueRef store_loc = LLVMGetOperand(instruction, 1);
                    if (LLVMCountParams(function) && store_val == LLVMGetParam(function, 0)) {
                        break;
                    }
                    int offset_loc = offset_map.at(store_loc);
                    if (LLVMIsAConstantInt(store_val)) {
                        int const_val = LLVMConstIntGetSExtValue(store_val);
                        fprintf(fp, "\tmovl\t$%d, %d(%%ebp)\n", const_val, offset_loc);
                    }
                    else if (reg_map.count(store_val) && reg_map.at(store_val) != SPILL) {
                        fprintf(fp, "\tmovl\t%%%s, %d(%%ebp)\n", getRegisterStr(reg_map.at(store_val)), offset_loc);
                    }
                    else {
                        int offset_val = offset_map.at(store_val);
                        fprintf(fp, "\tmovl\t%d(%%ebp), %%eax\n", offset_val);
                        fprintf(fp, "\tmovl\t%%eax, %d(%%ebp)\n", offset_loc);
                    }
                    break;
                }
                case LLVMCall: {
                    fprintf(fp, "\tpushl\t%%ebx\n");
                    fprintf(fp, "\tpushl\t%%ecx\n");
                    fprintf(fp, "\tpushl\t%%edx\n");

                    LLVMValueRef func_call = LLVMGetCalledValue(instruction);

                    if (LLVMCountParams(func_call)) {
                        LLVMValueRef param = LLVMGetOperand(instruction, 0);
                        if (LLVMIsAConstantInt(param)) {
                            int const_val = LLVMConstIntGetSExtValue(param);
                            fprintf(fp, "\tpushl\t$%d\n", const_val);
                        }
                        else if (reg_map.count(param) && reg_map.at(param) != SPILL) {
                            fprintf(fp, "\tpushl\t%%%s\n", getRegisterStr(reg_map.at(param)));
                        }
                        else {
                            int offset = offset_map.at(param);
                            fprintf(fp, "\tpushl\t%d(%%ebp)\n", offset);
                        }
                    }
                    
                    size_t call_len;
                    const char *call_name = LLVMGetValueName2(func_call, &call_len);

                    fprintf(fp, "\tcall\t%s@PLT\n", call_name);

                    if (LLVMCountParams(func_call)) {
                        fprintf(fp, "\taddl\t$4, %%esp\n");
                    }

                    fprintf(fp, "\tpopl\t%%edx\n");
                    fprintf(fp, "\tpopl\t%%ecx\n");
                    fprintf(fp, "\tpopl\t%%ebx\n");

                    if (!LLVMCountParams(func_call)) {
                        if (reg_map.count(instruction) && reg_map.at(instruction) != SPILL) {
                            
                            fprintf(fp, "\tmovl\t%%eax, %%%s\n", getRegisterStr(reg_map.at(instruction)));
                        }
                        else {
                            int offset = offset_map.at(instruction);
                            fprintf(fp, "\tmovl\t%%eax, %d(%%ebp)\n", offset);
                        }
                    }
                    break;
                }
                case LLVMBr: {
                    if (!LLVMIsConditional(instruction)) {
                        LLVMValueRef bb_value = LLVMGetOperand(instruction, 0);
                        LLVMBasicBlockRef bb_ref = LLVMValueAsBasicBlock(bb_value);
                        const char *jump_to = bb_labels.at(bb_ref).c_str();
                        fprintf(fp, "\tjmp\t%s\n", jump_to); 
                    }
                    else {
                        LLVMValueRef if_bb_val = LLVMGetOperand(instruction, 2);
                        LLVMValueRef else_bb_val = LLVMGetOperand(instruction, 1);
                        
                        LLVMBasicBlockRef if_bb = LLVMValueAsBasicBlock(if_bb_val);
                        LLVMBasicBlockRef else_bb = LLVMValueAsBasicBlock(else_bb_val);

                        const char *if_label = bb_labels.at(if_bb).c_str();
                        const char *else_label = bb_labels.at(else_bb).c_str();

                        LLVMValueRef cond = LLVMGetCondition(instruction);
                        LLVMIntPredicate predicate = LLVMGetICmpPredicate(cond);

                        switch (predicate) {
                            case LLVMIntEQ: {
                                fprintf(fp, "\tje\t%s\n", if_label);
                                break;
                            }
                            case LLVMIntSLT: {
                                fprintf(fp, "\tjl\t%s\n", if_label);
                                break;
                            }
                            case LLVMIntSLE: {
                                fprintf(fp, "\tjle\t%s\n", if_label);
                                break;
                            }
                            case LLVMIntSGT: {
                                fprintf(fp, "\tjg\t%s\n", if_label);
                                break;
                            }
                            case LLVMIntSGE: {
                                fprintf(fp, "\tjge\t%s\n", if_label);
                                break;
                            }
                            default: {
                                fprintf(stderr, "Error: Invalid comparison predicate\n");
                                break;
                            }
                        }
                        fprintf(fp, "\tjmp\t%s\n", else_label);
                    }
                    break;
                }
                case LLVMAdd: {
                    int reg;
                    if (reg_map.count(instruction) && reg_map.at(instruction) != SPILL) {
                        reg = reg_map.at(instruction);
                    }
                    else {
                        reg = EAX;
                    }

                    LLVMValueRef op1 = LLVMGetOperand(instruction, 0);
                    LLVMValueRef op2 = LLVMGetOperand(instruction, 1);

                   
                    
                    if (LLVMIsAConstantInt(op1)) {
                        int const_val_op1 = LLVMConstIntGetSExtValue(op1);
                        fprintf(fp, "\tmovl\t$%d, %%%s\n", const_val_op1, getRegisterStr(reg));
                    }
                    else if (reg_map.count(op1) && reg_map.at(op1) != SPILL) {
                        if (reg_map.at(op1) != reg) {
                            fprintf(fp, "\tmovl\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op1)), getRegisterStr(reg));
                        }    
                    }
                    else {
                        int offset_op1 = offset_map.at(op1);
                        fprintf(fp, "\tmovl\t%d(%%ebp), %%%s\n", offset_op1, getRegisterStr(reg));
                    }

                    if (LLVMIsAConstantInt(op2)) {
                        int const_val_op2 = LLVMConstIntGetSExtValue(op2);
                        fprintf(fp, "\taddl\t$%d, %%%s\n", const_val_op2, getRegisterStr(reg));
                    }
                    else if (reg_map.count(op2) && reg_map.at(op2) != SPILL) {
                        fprintf(fp, "\taddl\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op2)), getRegisterStr(reg));
                        
                    }
                    else {
                        int offset_op2 = offset_map.at(op2);
                        fprintf(fp, "\taddl\t%d(%%ebp), %%%s\n", offset_op2, getRegisterStr(reg));
                    }

                    if (reg == EAX) {
                        int offset_res = offset_map.at(instruction);
                        fprintf(fp, "\tmovl\t%%eax, %d(%%ebp)\n", offset_res);
                    }
                    break;
                }

                case LLVMMul: {
                    int reg;
                    if (reg_map.count(instruction) && reg_map.at(instruction) != SPILL) {
                        reg = reg_map.at(instruction);
                    }
                    else {
                        reg = EAX;
                    }

                    LLVMValueRef op1 = LLVMGetOperand(instruction, 0);
                    LLVMValueRef op2 = LLVMGetOperand(instruction, 1);

                   
                    
                    if (LLVMIsAConstantInt(op1)) {
                        int const_val_op1 = LLVMConstIntGetSExtValue(op1);
                        fprintf(fp, "\tmovl\t$%d, %%%s\n", const_val_op1, getRegisterStr(reg));
                    }
                    else if (reg_map.count(op1) && reg_map.at(op1) != SPILL) {
                        if (reg_map.at(op1) != reg) {
                            fprintf(fp, "\tmovl\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op1)), getRegisterStr(reg));
                        }    
                    }
                    else {
                        int offset_op1 = offset_map.at(op1);
                        fprintf(fp, "\tmovl\t%d(%%ebp), %%%s\n", offset_op1, getRegisterStr(reg));
                    }

                    if (LLVMIsAConstantInt(op2)) {
                        int const_val_op2 = LLVMConstIntGetSExtValue(op2);
                        fprintf(fp, "\timull\t$%d, %%%s\n", const_val_op2, getRegisterStr(reg));
                    }
                    else if (reg_map.count(op2) && reg_map.at(op2) != SPILL) {
                        fprintf(fp, "\timull\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op2)), getRegisterStr(reg));
                        
                    }
                    else {
                        int offset_op2 = offset_map.at(op2);
                        fprintf(fp, "\timull\t%d(%%ebp), %%%s\n", offset_op2, getRegisterStr(reg));
                    }

                    if (reg == EAX) {
                        int offset_res = offset_map.at(instruction);
                        fprintf(fp, "\tmovl\t%%eax, %d(%%ebp)\n", offset_res);
                    }
                    break;
                }
                case LLVMSub: {
                    int reg;
                    if (reg_map.count(instruction) && reg_map.at(instruction) != SPILL) {
                        reg = reg_map.at(instruction);
                    }
                    else {
                        reg = EAX;
                    }

                    LLVMValueRef op1 = LLVMGetOperand(instruction, 0);
                    LLVMValueRef op2 = LLVMGetOperand(instruction, 1);

                   
                    
                    if (LLVMIsAConstantInt(op1)) {
                        int const_val_op1 = LLVMConstIntGetSExtValue(op1);
                        fprintf(fp, "\tmovl\t$%d, %%%s\n", const_val_op1, getRegisterStr(reg));
                    }
                    else if (reg_map.count(op1) && reg_map.at(op1) != SPILL) {
                        if (reg_map.at(op1) != reg) {
                            fprintf(fp, "\tmovl\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op1)), getRegisterStr(reg));
                        }    
                    }
                    else {
                        int offset_op1 = offset_map.at(op1);
                        fprintf(fp, "\tmovl\t%d(%%ebp), %%%s\n", offset_op1, getRegisterStr(reg));
                    }

                    if (LLVMIsAConstantInt(op2)) {
                        int const_val_op2 = LLVMConstIntGetSExtValue(op2);
                        fprintf(fp, "\tsubl\t$%d, %%%s\n", const_val_op2, getRegisterStr(reg));
                    }
                    else if (reg_map.count(op2) && reg_map.at(op2) != SPILL) {
                        fprintf(fp, "\tsubl\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op2)), getRegisterStr(reg));
                        
                    }
                    else {
                        int offset_op2 = offset_map.at(op2);
                        fprintf(fp, "\tsubl\t%d(%%ebp), %%%s\n", offset_op2, getRegisterStr(reg));
                    }

                    if (reg == EAX) {
                        int offset_res = offset_map.at(instruction);
                        fprintf(fp, "\tmovl\t%%eax, %d(%%ebp)\n", offset_res);
                    }
                    break;
                }
                case LLVMICmp: {
                    int reg;
                    if (reg_map.count(instruction) && reg_map.at(instruction) != SPILL) {
                        reg = reg_map.at(instruction);
                    }
                    else {
                        reg = EAX;
                    }

                    LLVMValueRef op1 = LLVMGetOperand(instruction, 0);
                    
                    LLVMValueRef op2 = LLVMGetOperand(instruction, 1);

                   
                    
                    if (LLVMIsAConstantInt(op1)) {
                        int const_val_op1 = LLVMConstIntGetSExtValue(op1);
                        fprintf(fp, "\tmovl\t$%d, %%%s\n", const_val_op1, getRegisterStr(reg));
                    }
                    if (reg_map.count(op1) && reg_map.at(op1) != SPILL) {
                        if (reg_map.at(op1) != reg) {

                            fprintf(fp, "\tmovl\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op1)), getRegisterStr(reg));
                        }    
                    }
                    else {
                        int offset_op1 = offset_map.at(op1);
                        fprintf(fp, "\tmovl\t%d(%%ebp), %%%s\n", offset_op1, getRegisterStr(reg));
                    }

                    if (LLVMIsAConstantInt(op2)) {
                        int const_val_op2 = LLVMConstIntGetSExtValue(op2);
                        fprintf(fp, "\tcmpl\t$%d, %%%s\n", const_val_op2, getRegisterStr(reg));
                    }
                    else if (reg_map.count(op2) && reg_map.at(op2) != SPILL) {
                        fprintf(fp, "\tcmpl\t%%%s, %%%s\n", getRegisterStr(reg_map.at(op2)), getRegisterStr(reg));
                        
                    }
                    else {
                        int offset_op2 = offset_map.at(op2);
                        fprintf(fp, "\tcmpl\t%d(%%ebp), %%%s\n", offset_op2, getRegisterStr(reg));
                    }

                    if (reg == EAX) {
                        int offset_res = offset_map.at(instruction);
                        fprintf(fp, "\tmovl\t%%eax, %d(%%ebp)\n", offset_res);
                    }
                    break;
                }

            }
        }

    }

    fclose(fp);
}
