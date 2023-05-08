
#include "../../common/ast.h"
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>
#include <vector>
#include <string>
#include <string.h>
#include <unordered_map>
#include <stdio.h>
#include <stdbool.h>

void generateStmtIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef &builder, LLVMValueRef func);
LLVMValueRef generate(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef builder);
void generateNodeIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef builder, LLVMValueRef func);
#define prt(x) if(x) { printf("%s\n", x); }



LLVMModuleRef generateIR(astNode *root, const char *module_name) {

    LLVMModuleRef module = LLVMModuleCreateWithName(module_name);
    LLVMSetTarget(module, "x86_64-pc-linux-gnu");

    LLVMBuilderRef builder = LLVMCreateBuilder();
    LLVMValueRef func;

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
    LLVMDisposeBuilder(builder);
    
    return module;
}

void generateNodeIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef builder, LLVMValueRef func) {
    switch (node->type) {
        case ast_prog: {
            generateNodeIR(node->prog.func, module, ptr_map, builder, func);
            break;
        }

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

void generateStmtIR(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef &builder, LLVMValueRef func) {

	switch (node->stmt.type) {
        
		case ast_block: {
			// push each statement inside of the current block statement onto node stack
			vector<astNode*> *slist = node->stmt.block.stmt_list;
			for (int i = 0; i < slist->size(); i++) {
                
				generateNodeIR(slist->at(i), module, ptr_map, builder, func);
			}
			break;
		}
		case ast_decl: {
			LLVMValueRef decl = LLVMBuildAlloca(builder, LLVMInt32Type(), node->stmt.decl.name);
            LLVMSetAlignment(decl, 4);
            std::pair<string, LLVMValueRef> ptr_entry (node->stmt.decl.name, decl);
            ptr_map.insert(ptr_entry);
			break;
		}
		case ast_asgn: {
            LLVMBuildStore(builder, generate(node->stmt.asgn.rhs, module, ptr_map, builder), ptr_map.at(node->stmt.asgn.lhs->var.name));
			break;

		}
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
		case ast_while: {
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

		case ast_call: {
            generate(node, module, ptr_map, builder);
			break;
		}
		case ast_ret: {
            LLVMValueRef ret_val = generate(node->stmt.ret.expr, module, ptr_map, builder);
            LLVMBuildRet(builder, ret_val);
			break;
		}
	}
}

LLVMValueRef generate(astNode *node, LLVMModuleRef module, std::unordered_map<string, LLVMValueRef> &ptr_map, LLVMBuilderRef builder) {
    switch (node->type) {
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
        case ast_uexpr: {
            LLVMValueRef expr = generate(node->uexpr.expr, module, ptr_map, builder);
            if (node->uexpr.op == uminus) {
                LLVMValueRef zero = LLVMConstInt(LLVMInt32Type(), 0, 1);
                return LLVMBuildSub(builder, zero, expr, "");
            }
        }
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
        case ast_stmt: {

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
        case ast_cnst: {
            return LLVMConstInt(LLVMInt32Type(), node->cnst.value, 1);
        }
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

