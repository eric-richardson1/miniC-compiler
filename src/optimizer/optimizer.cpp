#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>
#include <string.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>

using namespace std;
#define prt(x) if(x) { printf("%s\n", x); }

LLVMModuleRef createLLVMModel(char * filename){
	char *err = 0;

	LLVMMemoryBufferRef ll_f = 0;
	LLVMModuleRef m = 0;

	LLVMCreateMemoryBufferWithContentsOfFile(filename, &ll_f, &err);

	if (err != NULL) { 
		prt(err);
		return NULL;
	}
	
	LLVMParseIRInContext(LLVMGetGlobalContext(), ll_f, &m, &err);

	if (err != NULL) {
		prt(err);
	}

	return m;
}

bool eliminateCommonSubExpressions(LLVMBasicBlockRef bb) {
	bool is_changed = false;
	for (LLVMValueRef first_inst = LLVMGetFirstInstruction(bb); first_inst; first_inst = LLVMGetNextInstruction(first_inst)) {
		
		LLVMOpcode first_inst_op = LLVMGetInstructionOpcode(first_inst);

		if (LLVMIsAAllocaInst(first_inst)) {
			continue;
		}
	

		int num_operands = LLVMGetNumOperands(first_inst);
		
		for (LLVMValueRef second_inst = LLVMGetNextInstruction(first_inst); second_inst; second_inst = LLVMGetNextInstruction(second_inst)) {
			
			LLVMOpcode second_inst_op = LLVMGetInstructionOpcode(second_inst);
			bool is_safe = true;
			// make sure 
			if (LLVMIsALoadInst(first_inst) && LLVMIsAStoreInst(second_inst)) {
				for (int i = 0; i < num_operands; i++) {
					for (int j = 0; j < LLVMGetNumOperands(second_inst); j++) {
						if (LLVMGetOperand(first_inst, i) == LLVMGetOperand(second_inst, j)) {
							is_safe = false;
						}
					}
				}
			}
			if (!is_safe) {
				break;
			}	

			if (first_inst_op != second_inst_op) {
				continue;
			}
			if (num_operands != LLVMGetNumOperands(second_inst)) {
				continue;
			}

			bool is_load = first_inst_op == LLVMLoad;
			bool can_replace = true;

			for (int i = 0; i < num_operands; i++) {
				if (LLVMGetOperand(first_inst, i) != LLVMGetOperand(second_inst, i)) {
					
					can_replace = false;

					break;
				}
			}
			
			if (can_replace) {
				is_changed = true;
				LLVMReplaceAllUsesWith(second_inst, first_inst);

			}

		}
	}
	return is_changed;
}

bool eliminateDeadCode(LLVMBasicBlockRef bb) {
	bool is_changed = false;
	LLVMValueRef curr_instruction = LLVMGetFirstInstruction(bb);
	while (curr_instruction) {
		if (LLVMIsAStoreInst(curr_instruction) || LLVMIsACallInst(curr_instruction) || LLVMIsATerminatorInst(curr_instruction)) {
			curr_instruction = LLVMGetNextInstruction(curr_instruction);
			continue;
		}
		LLVMUseRef use = LLVMGetFirstUse(curr_instruction);
		if (use == NULL) {
			is_changed = true;
			LLVMValueRef next_instruction = LLVMGetNextInstruction(curr_instruction);
			LLVMInstructionEraseFromParent(curr_instruction);
			curr_instruction = next_instruction;
		}
		else {
			curr_instruction = LLVMGetNextInstruction(curr_instruction);
		}
	}
	return is_changed;
}

bool foldConstants(LLVMBasicBlockRef bb) {
	bool is_changed = false;
	for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
		LLVMOpcode opcode = LLVMGetInstructionOpcode(instruction);
		if (opcode == LLVMMul || opcode == LLVMAdd || opcode == LLVMSub) {
			int num_operands = LLVMGetNumOperands(instruction);
			bool is_all_const = true;

			if (!LLVMIsAConstantInt(LLVMGetOperand(instruction, 0)) || !LLVMIsAConstantInt(LLVMGetOperand(instruction, 1))) {
				continue;
			}
			
			LLVMValueRef new_instruction;
			if (opcode == LLVMMul) {
				new_instruction = LLVMConstMul(LLVMGetOperand(instruction, 0), LLVMGetOperand(instruction, 1));

			}
			else if (opcode == LLVMAdd) {
				new_instruction = LLVMConstAdd(LLVMGetOperand(instruction, 0), LLVMGetOperand(instruction, 1));
			}
			else {
				new_instruction = LLVMConstSub(LLVMGetOperand(instruction, 0), LLVMGetOperand(instruction, 1));
			}
			is_changed = true;
			LLVMReplaceAllUsesWith(instruction, new_instruction);
		}
	}
	return is_changed;
}

// bool propagateConstants(LLVMValueRef function) {

// 	std::unordered_map<LLVMValueRef, std::unordered_set<LLVMValueRef>> ptr_map;
// 	std::unordered_map<LLVMValueRef, std::unordered_set<LLVMValueRef>> GEN_set;


// 	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
// 		for (LLVMValueRef inst = LLVMGetFirstInstruction(bb); inst; inst = LLVMGetNextInstruction(inst)) {
// 			if (LLVMIsAAllocaInst(inst)) {
// 				std::unordered_set<LLVMValueRef> store_entry;
// 				std::pair<LLVMValueRef, std::unordered_set<LLVMValueRef>> ptr_entry (inst, store_entry);
				
// 				ptr_map.insert(ptr_entry);
				
// 			}
			
// 			if (LLVMIsAStoreInst(inst)) {				
// 				LLVMValueRef ptr = LLVMGetOperand(inst, 1);
// 				ptr_map.at(ptr).insert(inst);
// 			}	
// 		}
// 	}

// 	return true;
// }



void walkBBInstructions(LLVMBasicBlockRef bb) {
	bool optimizing = true;
	while (optimizing) {
		bool CSE = eliminateCommonSubExpressions(bb);
		bool DC = eliminateDeadCode(bb);
		bool CF = foldConstants(bb);
		optimizing = CSE || DC || CF;
	}
}


void walkBasicblocks(LLVMValueRef function){
	for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function);
 			 basicBlock;
  			 basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
		printf("\nIn basic block\n");
		walkBBInstructions(basicBlock);
	}
}

void walkFunctions(LLVMModuleRef module){
	for (LLVMValueRef function =  LLVMGetFirstFunction(module); 
			function; 
			function = LLVMGetNextFunction(function)) {
		
		const char* funcName = LLVMGetValueName(function);	

		printf("\nFunction Name: %s\n", funcName);

		walkBasicblocks(function);
 	}
}

void walkGlobalValues(LLVMModuleRef module){
	for (LLVMValueRef gVal =  LLVMGetFirstGlobal(module);
                        gVal;
                        gVal = LLVMGetNextGlobal(gVal)) {

                const char* gName = LLVMGetValueName(gVal);
                printf("Global variable name: %s\n", gName);
        }
}

int main(int argc, char** argv)
{
	LLVMModuleRef m;

	if (argc == 2){
		m = createLLVMModel(argv[1]);
	}
	else {
		m = NULL;
		return 1;
	}

	if (m != NULL){
		//LLVMDumpModule(m);
		walkFunctions(m);
		LLVMDumpModule(m);
	}
	else {
	    printf("m is NULL\n");
	}
	
	return 0;
}
