/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * code_generator.h - defines functions necessary for performing assembly code generation
 */

#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H 

#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>

/*
 * Used for easy access to registers eax, ebx, ecx, and edx; also provides a code
 * for spilled register values
 */
typedef enum {
    EAX,
    EBX,
    ECX,
    EDX,
    SPILL
} reg_t;

/*
 * Params: 
 *      LLVMModuleRef module: the module corresponding to the optimized LLVM
 *      IR code created by ir_generator.c and optimized from optimizer.c
 *      
 * 
 * Returns:
 *      void
 * 
 * Notes: 
 *      This function generates the assembly code corresponding to the
 *      generated LLVM IR. The assembly code is written to a file within the
 *      'src' directory called 'func.s'
 */
void generateAssembly(LLVMModuleRef module);

#endif