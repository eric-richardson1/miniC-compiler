/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * ir_generator.c - implements functions necessary for performing LLVM IR generation
 */

#include "ir_generator.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <stdio.h>
#include <stdbool.h>

/***************************************** FUNCTION HEADERS *****************************************/

void generateStmtIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, 
                            LLVMValueRef> &ptr_map, LLVMBuilderRef &builder, LLVMValueRef func);
                            
LLVMValueRef generate(astNode *node, LLVMModuleRef module, std::unordered_map<string, 
                                                    LLVMValueRef> &ptr_map, LLVMBuilderRef builder);

void generateNodeIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, 
                                    LLVMValueRef> &ptr_map, LLVMBuilderRef builder, LLVMValueRef func);

void cleanUpIR(LLVMModuleRef module);


/***************************************** IMPLEMENTATION *****************************************/

/*********************** see "ir_generator.h" for details ***********************/
LLVMModuleRef generateIR(astNode *root, const char *module_name) {

    // boilerplate module and builder instantiation
    LLVMModuleRef module = LLVMModuleCreateWithName(module_name);
    LLVMSetTarget(module, "x86_64-pc-linux-gnu");

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMValueRef func;

    // used to keep track of which pointers should be used at any given point
    std::unordered_map<std::string, LLVMValueRef> ptr_map;

     // extern print declaration
    LLVMTypeRef print_param_types[] = { LLVMInt32Type() };
    LLVMTypeRef print_func_type = LLVMFunctionType(LLVMVoidType(), print_param_types, 1, 0);
    LLVMValueRef extern_print = LLVMAddFunction(module, "print", print_func_type);
    LLVMSetLinkage(extern_print, LLVMExternalLinkage);

    // extern read declaration
    LLVMTypeRef read_param_types[] = { };
    LLVMTypeRef read_func_type = LLVMFunctionType(LLVMInt32Type(), read_param_types, 0, 0);
    LLVMValueRef extern_read = LLVMAddFunction(module, "read", read_func_type);
    LLVMSetLinkage(extern_read, LLVMExternalLinkage);

    generateNodeIR(root, module, ptr_map, builder, func);
    cleanUpIR(module);
    LLVMDisposeBuilder(builder);
    
    return module;
}
/* outermost level of recursion: initializes the 'program' and 'function' nodes and passes off 
   generic 'ast_stmt' nodes */
void generateNodeIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef builder, LLVMValueRef func) {
    switch (node->type) {
        case ast_prog: {
            generateNodeIR(node->prog.func, module, ptr_map, builder, func);
            break;
        }

        // due to miniC constraints, this case should only be hit when we encounter the definition
        // of the main, user-defined function
        case ast_func: {
            LLVMTypeRef param_types[] = {};
            int num_params = 0;
            if (node->func.param != NULL) {
                param_types[0] = LLVMInt32Type();
                num_params = 1;
            }

            LLVMTypeRef func_type = LLVMFunctionType(LLVMInt32Type(), param_types, num_params, 0);
            func = LLVMAddFunction(module, node->func.name, func_type);
            LLVMBasicBlockRef func_block = LLVMAppendBasicBlock(func, "");
            LLVMPositionBuilderAtEnd(builder, func_block);

            // a function parameter serves as a variable declaration and an indirect store of the passed parameter
            // into the declared variable
            if (num_params == 1) {
                LLVMValueRef param = LLVMBuildAlloca(builder, LLVMInt32Type(), node->func.param->var.name);
                LLVMSetAlignment(param, 4);

                std::pair<string, LLVMValueRef> ptr_entry (node->func.param->var.name, param);
                ptr_map.insert(ptr_entry);
                LLVMBuildStore(builder, LLVMGetParam(func, 0), param);
            }
            

            generateNodeIR(node->func.body, module, ptr_map, builder, func);
            break;
        }
        case ast_stmt: {
            generateStmtIR(node, module, ptr_map, builder, func);
            break;
        }
        default: {
            fprintf(stderr, "Error: Invalid node type encountered in IR generation\n");
            exit(1);

        }
    }

}

/* takes an 'ast_stmt' node as parameter and generates the corresponding LLVM IR associated with statement 
   type (statements must be one of ast_block, ast_decl, ast_asgn, ast_if, ast_while, ast_call, or ast_ret )*/
void generateStmtIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef &builder, LLVMValueRef func) {

	switch (node->stmt.type) {
        
        // handle each statement within the block statement as a separate node
		case ast_block: {
			vector<astNode*> *slist = node->stmt.block.stmt_list;
			for (int i = 0; i < slist->size(); i++) {
                
				generateNodeIR(slist->at(i), module, ptr_map, builder, func);
			}
			break;
		}
        // allocate memory for a pointer when a variable is declared
		case ast_decl: {
			LLVMValueRef decl = LLVMBuildAlloca(builder, LLVMInt32Type(), node->stmt.decl.name);
            LLVMSetAlignment(decl, 4);
            std::pair<string, LLVMValueRef> ptr_entry (node->stmt.decl.name, decl);
            ptr_map.insert(ptr_entry);
			break;
		}
        // build a store instruction for a variable assignment
		case ast_asgn: {
            LLVMBuildStore(builder, generate(node->stmt.asgn.rhs, module, ptr_map, builder), ptr_map.at(node->stmt.asgn.lhs->var.name));
			break;

		}
        // create if/else basic blocks, position builder accordingly
		case ast_if: {
            LLVMValueRef cond = generate(node->stmt.ifn.cond, module, ptr_map, builder);
            LLVMBasicBlockRef if_BB = LLVMAppendBasicBlock(func, "");
            LLVMBasicBlockRef final;

            if (node->stmt.ifn.else_body != NULL) {
                LLVMBasicBlockRef else_BB = LLVMAppendBasicBlock(func, "");
                final = LLVMAppendBasicBlock(func, "");

                LLVMBuildCondBr(builder, cond, if_BB, else_BB);
                LLVMPositionBuilderAtEnd(builder, if_BB);
                generateNodeIR(node->stmt.ifn.if_body, module, ptr_map, builder, func);
                LLVMBuildBr(builder, final);

                LLVMPositionBuilderAtEnd(builder, else_BB);
                generateNodeIR(node->stmt.ifn.else_body, module, ptr_map, builder, func);
                LLVMBuildBr(builder, final);

            }
            else {
                final = LLVMAppendBasicBlock(func, "");
                LLVMBuildCondBr(builder, cond, if_BB, final);
                LLVMPositionBuilderAtEnd(builder, if_BB);
                generateNodeIR(node->stmt.ifn.if_body, module, ptr_map, builder, func);
                LLVMBuildBr(builder, final);
            }
            LLVMPositionBuilderAtEnd(builder, final);
			break;
		}
        // create while block
		case ast_while: {
            // need this 'condition checking' block in order to imitate looping
            LLVMBasicBlockRef check_BB = LLVMAppendBasicBlock(func, ""); 

            LLVMBasicBlockRef while_body = LLVMAppendBasicBlock(func, "");
            LLVMBasicBlockRef final = LLVMAppendBasicBlock(func, "");

            LLVMBuildBr(builder, check_BB);
            LLVMPositionBuilderAtEnd(builder, check_BB);

            LLVMValueRef cond = generate(node->stmt.whilen.cond, module, ptr_map, builder);
            LLVMBuildCondBr(builder, cond, while_body, final);

            LLVMPositionBuilderAtEnd(builder, while_body);
            generateNodeIR(node->stmt.whilen.body, module, ptr_map, builder, func);
            LLVMBuildBr(builder, check_BB);

            LLVMPositionBuilderAtEnd(builder, final);

			break;
		}

        // create a call instruction
		case ast_call: {
            generate(node, module, ptr_map, builder);
			break;
		}
        // create a return instruction
		case ast_ret: {
            LLVMValueRef ret_val = generate(node->stmt.ret.expr, module, ptr_map, builder);
            LLVMBuildRet(builder, ret_val);
			break;
		}
	}
}

/* innermost level of recursion: builds instructions for arithmetic expressions, comparisons, 
   loads, and stores */
LLVMValueRef generate(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef builder) {
    switch (node->type) {
        // arithmetic expressions
        case ast_bexpr: {
            LLVMValueRef lhs = generate(node->bexpr.lhs, module, ptr_map, builder);
            LLVMValueRef rhs = generate(node->bexpr.rhs, module, ptr_map, builder);
            if (node->bexpr.op == mul) {
                return LLVMBuildMul(builder, lhs, rhs, "");
            }
            else if (node->bexpr.op == add) {
                return LLVMBuildAdd(builder, lhs, rhs, "");
            }
            else if (node->bexpr.op == sub) {
                return LLVMBuildSub(builder, lhs, rhs, "");
            }
            else {
                return LLVMBuildSDiv(builder, lhs, rhs, "");
            }
        }
        // handles unary minus expressions by the subtracting the value from zero
        case ast_uexpr: {
            LLVMValueRef expr = generate(node->uexpr.expr, module, ptr_map, builder);
            if (node->uexpr.op == uminus) {
                LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 1);
                return LLVMBuildSub(builder, zero, expr, "");
            }
        }
        // comparison expressions
        case ast_rexpr: {
            LLVMValueRef lhs = generate(node->rexpr.lhs, module, ptr_map, builder);
            LLVMValueRef rhs = generate(node->rexpr.rhs, module, ptr_map, builder);
            if (node->rexpr.op == lt) {
                return LLVMBuildICmp(builder, LLVMIntSLT, lhs, rhs, "");
            }
            else if (node->rexpr.op == gt) {
                return LLVMBuildICmp(builder, LLVMIntSGT, lhs, rhs, "");
            }
            else if (node->rexpr.op == le) {
                return LLVMBuildICmp(builder, LLVMIntSLE, lhs, rhs, "");
            }
            else if (node->rexpr.op == ge) {
                return LLVMBuildICmp(builder, LLVMIntSGE, lhs, rhs, "");
            }
            else {
                return LLVMBuildICmp(builder, LLVMIntEQ, lhs, rhs, "");
            }
            break;

        }
        // special edge case: since a 'call_stmt' is parsed as an expression, call 
        // instructions are built here
        case ast_stmt: {

                // note: no need to check for 'ast_stmt->type' since the only time this function gets called
                // on an ast_stmt is after verifying it is a call_stmt

                // if called function takes no parameters, it must be 'read()', otherwise it is 'print()'
                if (node->stmt.call.param == NULL) {
                    LLVMValueRef fn = LLVMGetNamedFunction(module, "read");
                    LLVMTypeRef read_param_types[] = {};
                    LLVMTypeRef read_func_type = LLVMFunctionType(LLVMInt32Type(), read_param_types, 0, 0);
                    return LLVMBuildCall2(builder, read_func_type, fn, NULL, 0, "");
                }
                else {
                    LLVMValueRef fn = LLVMGetNamedFunction(module, "print");
                    
                    LLVMTypeRef print_param_types[] = { LLVMInt32Type() };
                    LLVMTypeRef print_func_type = LLVMFunctionType(LLVMVoidType(), print_param_types, 1, 0);
                    
                    LLVMValueRef *param = (LLVMValueRef *)malloc(sizeof(LLVMValueRef));
                    param[0] = generate(node->stmt.call.param, module, ptr_map, builder);
                    LLVMValueRef call = LLVMBuildCall2(builder, print_func_type, fn, param, 1, "");
                    free(param);
                    return call;
                }
            
        }
        // create constant integer LLVMValueRef
        case ast_cnst: {
            return LLVMConstInt(LLVMInt32Type(), node->cnst.value, 1);
        }
        // build load instructions when encountering a variable
        case ast_var: {
            LLVMValueRef ptr = ptr_map.at(node->var.name);
            return LLVMBuildLoad2(builder, LLVMInt32Type(), ptr, "");
            
        }
        default: {
            fprintf(stderr, "Error: Invalid node type encountered in IR generator\n");
            exit(1);
        }
    }
}

/* simple cleanup function called after the initial IR has been generated; this eliminates any code that 
   it is guaranteed to be unreachable (i.e. it comes after a return statement in a basic block) */
void cleanUpIR(LLVMModuleRef module) {
    for (LLVMValueRef function =  LLVMGetFirstFunction(module); function; function = LLVMGetNextFunction(function)) {
        for (LLVMBasicBlockRef basicBlock = LLVMGetFirstBasicBlock(function); basicBlock; basicBlock = LLVMGetNextBasicBlock(basicBlock)) {
            bool can_delete = false;
            LLVMValueRef instruction = LLVMGetFirstInstruction(basicBlock);
            while (instruction) {
                if (can_delete) {
                    LLVMValueRef next_instruction = LLVMGetNextInstruction(instruction);
                    LLVMInstructionEraseFromParent(instruction);
                    instruction = next_instruction;
                    continue;
                }
                if (LLVMIsAReturnInst(instruction)) {
                    can_delete = true;
                }
                instruction = LLVMGetNextInstruction(instruction);
            }
        }	
 	}
}

