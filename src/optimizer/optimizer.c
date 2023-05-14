/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * optimizer.c - provides functions for performing local and global optimizations
 * on an LLVM IR module
 */

#include "optimizer.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <llvm-c/Core.h>

using namespace std;

/***************************************** FUNCTION HEADERS *****************************************/
void removeKills(LLVMValueRef inst, std::unordered_set<LLVMValueRef> &inst_set);
void addKills(LLVMValueRef inst, LLVMBasicBlockRef bb, std::unordered_set<LLVMValueRef> &stores, 
					std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> &KILL_set);
std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMBasicBlockRef>> buildPredecessorMap(LLVMValueRef function);
std::unordered_set<LLVMValueRef> set_union(std::unordered_set<LLVMValueRef> set1, std::unordered_set<LLVMValueRef> set2);
std::unordered_set<LLVMValueRef> set_difference(std::unordered_set<LLVMValueRef> set1, std::unordered_set<LLVMValueRef> set2);


/***************************************** IMPLEMENTATION *****************************************/

/*********************** see "optimizer.h" for details ***********************/
bool eliminateCommonSubExpressions(LLVMValueRef function) {
	bool is_changed = false; 
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {

		// loop over all pairs of instructions
		for (LLVMValueRef first_inst = LLVMGetFirstInstruction(bb); first_inst; first_inst = LLVMGetNextInstruction(first_inst)) {
			
			LLVMOpcode first_inst_op = LLVMGetInstructionOpcode(first_inst);

			// ignore 'lloca' instructions
			if (LLVMIsAAllocaInst(first_inst)) {
				continue;
			}
			int num_operands = LLVMGetNumOperands(first_inst);
			
			for (LLVMValueRef second_inst = LLVMGetNextInstruction(first_inst); second_inst; second_inst = LLVMGetNextInstruction(second_inst)) {
				
				LLVMOpcode second_inst_op = LLVMGetInstructionOpcode(second_inst);
				bool is_safe = true;
				// make sure there isn't a store instruction that could modify potential targets for CSE
				if (LLVMIsALoadInst(first_inst) && LLVMIsAStoreInst(second_inst)) {
					for (int i = 0; i < num_operands; i++) {
						for (int j = 0; j < LLVMGetNumOperands(second_inst); j++) {
							if (LLVMGetOperand(first_inst, i) == LLVMGetOperand(second_inst, j)) {
								is_safe = false;
							}
						}
					}
				}
				// immediate disqualifiers for performing CSE
				if (!is_safe) {
					break;
				}	
				if (first_inst_op != second_inst_op) {
					continue;
				}
				if (num_operands != LLVMGetNumOperands(second_inst)) {
					continue;
				}

				bool can_replace = true;

				// check if all operands are the same
				for (int i = 0; i < num_operands; i++) {
					if (LLVMGetOperand(first_inst, i) != LLVMGetOperand(second_inst, i)) {
						
						can_replace = false;

						break;
					}
				}
				// if so, replace all uses of the second instruction with the first instruction
				if (can_replace) {
					is_changed = true;
					LLVMReplaceAllUsesWith(second_inst, first_inst);

				}

			}
		}
	}
	return is_changed;
}

/*********************** see "optimizer.h" for details ***********************/
bool eliminateDeadCode(LLVMValueRef function) {
	bool is_changed = false;
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		LLVMValueRef curr_instruction = LLVMGetFirstInstruction(bb);
		while (curr_instruction) {
			// these four instruction types can never be deleted because they might have indirect consequences
			if (LLVMIsAStoreInst(curr_instruction) || LLVMIsACallInst(curr_instruction) || LLVMIsAAllocaInst(curr_instruction) || LLVMIsATerminatorInst(curr_instruction)) {
				curr_instruction = LLVMGetNextInstruction(curr_instruction);
				continue;
			}
			LLVMUseRef use = LLVMGetFirstUse(curr_instruction);
			// check if the instruction is ever used
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
	}
	return is_changed;
}

/*********************** see "optimizer.h" for details ***********************/
bool foldConstants(LLVMValueRef function) {
	bool is_changed = false;
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
			LLVMOpcode opcode = LLVMGetInstructionOpcode(instruction);

			// constant folding is only possible with multiplication, addition, or subtraction
			if (opcode == LLVMMul || opcode == LLVMAdd || opcode == LLVMSub) {
				int num_operands = LLVMGetNumOperands(instruction);
				bool is_all_const = true;

				// both operands must be constants
				if (!LLVMIsAConstantInt(LLVMGetOperand(instruction, 0)) || !LLVMIsAConstantInt(LLVMGetOperand(instruction, 1))) {
					continue;
				}
				
				// create new instruction depending on opcode
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
				LLVMReplaceAllUsesWith(instruction, new_instruction); // insert new instruction in place of old
			}
		}
	}
	return is_changed;
}

/*********************** see "optimizer.h" for details ***********************/
bool propagateConstants(LLVMValueRef function) {

	bool is_changed = false;

	std::unordered_set<LLVMValueRef> stores; // keep track of all store instructions
	std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> GEN_set; 
	std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> KILL_set;

	// construct GEN set
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		std::unordered_set<LLVMValueRef> gen_stores_init;
		std::pair<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> gen_entry (bb, gen_stores_init);
		GEN_set.insert(gen_entry);
		
		for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {

			if (LLVMIsAStoreInst(instruction)) {
				stores.insert(instruction);
				
				removeKills(instruction, GEN_set.at(bb));
				GEN_set.at(bb).insert(instruction);
			}
		}
	}
	
	// construct KILL set
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		std::unordered_set<LLVMValueRef> kill_stores_init;
		std::pair<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> kill_entry (bb, kill_stores_init);
		KILL_set.insert(kill_entry);

		for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
			if (LLVMIsAStoreInst(instruction)) {
				addKills(instruction, bb, stores, KILL_set);
			}
			
		}
	}

	// predecessor map: key = basic block, value = set containing immediate predecessors of the key
	std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMBasicBlockRef>> pred_map = buildPredecessorMap(function);

	std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> IN_set;
	std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> OUT_set;

	// initialize OUT and IN
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		std::unordered_set<LLVMValueRef> in_set_init;
		std::pair<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> in_init (bb, in_set_init);
		IN_set.insert(in_init);
	}
	OUT_set = GEN_set;

	bool change = true;
	while (change) {
		change = false;
		for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
			// update IN set to contain the union of OUT sets corresponding to the current basic block's predecessors
			for (std::unordered_set<LLVMBasicBlockRef>::iterator iter = pred_map.at(bb).begin(); iter != pred_map.at(bb).end(); iter++) {
				IN_set.at(bb).insert(OUT_set.at(*iter).begin(), OUT_set.at(*iter).end());
			}

			// OUT[bb] = union(GEN[bb], IN[bb] - KILL[bb])
			std::unordered_set<LLVMValueRef> oldout = OUT_set.at(bb);
			OUT_set.at(bb) = set_union(GEN_set.at(bb), set_difference(IN_set.at(bb), KILL_set.at(bb)));

			// keep looping if OUT set continues to change
			if (OUT_set.at(bb) != oldout) {
				change = true;
			}
		} 
	}

	// search for load instructions that can be replaced
	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		std::unordered_set<LLVMValueRef> R;
		R = IN_set.at(bb);
		std::unordered_set<LLVMValueRef> to_delete;

		for (LLVMValueRef instruction = LLVMGetFirstInstruction(bb); instruction; instruction = LLVMGetNextInstruction(instruction)) {
			if (LLVMIsAStoreInst(instruction)) {
				removeKills(instruction, R);
				R.insert(instruction);
			}

			if (LLVMIsALoadInst(instruction)) {
				LLVMValueRef ptr = LLVMGetOperand(instruction, 0);

				std::unordered_set<LLVMValueRef> potential_const_stores;
				for (std::unordered_set<LLVMValueRef>::iterator iter = R.begin(); iter != R.end(); iter++) {
					
					if (LLVMGetOperand(*iter, 1) == ptr) {
						potential_const_stores.insert(*iter);
					}
				}
				
				int const_val;
				bool first_pass = true;
				bool can_replace = true;
				for (std::unordered_set<LLVMValueRef>::iterator iter = potential_const_stores.begin(); iter != potential_const_stores.end(); iter++) {
					
					LLVMValueRef op = LLVMGetOperand(*iter, 0);
					if (!LLVMIsAConstantInt(op)) {
						can_replace = false;
						break;
					}

					// acquire the value that all future instructions in 'potential_const_stores' should be compared against
					if (first_pass) {
						const_val = LLVMConstIntGetSExtValue(op);

						first_pass = false;
						continue;
					}
					if (const_val != LLVMConstIntGetSExtValue(op)) {
						can_replace = false;
						break;
					}
				}

				if (can_replace) {
					to_delete.insert(instruction);
					LLVMReplaceAllUsesWith(instruction, LLVMConstInt(LLVMInt32Type(), const_val, 1));
					is_changed = true;
				}
				


			}
			
		}
		// delete load instructions that were propagated
		std::unordered_set<LLVMValueRef>::iterator iter = to_delete.begin();
		while (iter != to_delete.end()) {
			std::unordered_set<LLVMValueRef>::iterator next = iter;
			next++;
			LLVMInstructionEraseFromParent(*iter);
			iter = next;
		}
	}
	return is_changed;
}

// simple helper function that returns the union of two unordered sets
std::unordered_set<LLVMValueRef> set_union(std::unordered_set<LLVMValueRef> set1, std::unordered_set<LLVMValueRef> set2) {
	std::unordered_set<LLVMValueRef> res;
	res = set1;
	res.insert(set2.begin(), set2.end());
	return res;
}

// simple helper function that returns the difference between two unordered sets
std::unordered_set<LLVMValueRef> set_difference(std::unordered_set<LLVMValueRef> set1, std::unordered_set<LLVMValueRef> set2) {
	std::unordered_set<LLVMValueRef> res;
	res = set1;
	for (std::unordered_set<LLVMValueRef>::iterator iter = set1.begin(); iter != set1.end(); iter++) {
		if (set2.find(*iter) == set2.end()) {
			res.insert(*iter);
		}
	}
	return res;
}

// helper function that builds the predecessor map of all basic blocks in a function
std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMBasicBlockRef>> buildPredecessorMap(LLVMValueRef function) {
	std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMBasicBlockRef>> pred_map;

	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		std::unordered_set<LLVMBasicBlockRef> pred_set_init;
		std::pair<LLVMBasicBlockRef, std::unordered_set<LLVMBasicBlockRef>> pred_map_entry (bb, pred_set_init);
		pred_map.insert(pred_map_entry);
	}

	for (LLVMBasicBlockRef bb = LLVMGetFirstBasicBlock(function); bb; bb = LLVMGetNextBasicBlock(bb)) {
		LLVMValueRef terminator = LLVMGetBasicBlockTerminator(bb);
		int num_successors = LLVMGetNumSuccessors(terminator);

		for (int i = 0; i < num_successors; i++) {
			LLVMBasicBlockRef successor = LLVMGetSuccessor(terminator, i);
			pred_map.at(successor).insert(bb);
		}
	}
	return pred_map;

}

// removes all store instructions in 'inst_set' that are killed by 'inst'
void removeKills(LLVMValueRef inst, std::unordered_set<LLVMValueRef> &inst_set) {
	LLVMValueRef op1 = LLVMGetOperand(inst, 1);

	std::unordered_set<LLVMValueRef>::iterator iter = inst_set.begin();
	while (iter != inst_set.end()) {
		if (LLVMGetOperand(*iter, 1) == op1) {
			std::unordered_set<LLVMValueRef>::iterator next = iter;
			next++;
			inst_set.erase(iter);
			iter = next;
			continue;
		}
		iter++;
	}
	
}

// inserts all instructions in 'stores' that are killed by 'inst' into the KILL set
void addKills(LLVMValueRef inst, LLVMBasicBlockRef bb, std::unordered_set<LLVMValueRef> &stores, std::unordered_map<LLVMBasicBlockRef, std::unordered_set<LLVMValueRef>> &KILL_set) {
	LLVMValueRef op1 = LLVMGetOperand(inst, 1);
	for (unordered_set<LLVMValueRef>::iterator iter = stores.begin(); iter != stores.end(); iter++) {
		if (*iter == inst) {
			continue;
		}

		if (LLVMGetOperand(*iter, 1) == op1) {
			KILL_set.at(bb).insert(*iter);
		}
	}
}


/*********************** see "optimizer.h" for details ***********************/
void optimizeFunction(LLVMValueRef function){
	bool optimizing = true;
	while (optimizing) {
		bool CP = propagateConstants(function);
		bool CSE = eliminateCommonSubExpressions(function);
		bool CF = foldConstants(function);
		bool DC = eliminateDeadCode(function);
		optimizing = CP || CSE || CF || DC;
	}
	
}

/*********************** see "optimizer.h" for details ***********************/
void optimize(LLVMModuleRef module){
	for (LLVMValueRef function = LLVMGetFirstFunction(module); 
			function; 
			function = LLVMGetNextFunction(function)) {

		optimizeFunction(function);
		
 	}
	
}
