/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * ir_generator.h - defines functions necessary for performing LLVM IR generation
 */


#ifndef IR_GENERATOR_H
#define IR_GENERATOR_H

#include "../../common/ast.h"
#include <llvm-c/Core.h>


/*
 * Params: 
 *      astNode *root: the root node of an abstract syntax tree representing a 
 *      miniC program.
 *      
 *      const char* module_name: a string that will be assigned internally as 
 *      the name of the output LLVMModule
 * 
 * Returns:
 *      an LLVMModuleRef that contains unoptimized LLVM IR code corresponding
 *      to the provided abstract syntax tree
 * 
 * Notes: 
 *      This function outputs unoptimized LLVM IR code. It should be called 
 *      AFTER constructing an AST for the miniC program the user wishes to compile. 
 *      The function assumes that the program is semantically correct (i.e. it does 
 *      not perform any validation checks on the AST). 
 */
LLVMModuleRef generateIR(astNode *root, const char *module_name);


#endif