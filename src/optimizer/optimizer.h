/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * optimizer.h - defines functions necessary for performing LLVM IR optimizations
 * such as common subexpression elimination, constant folding, constant propagation,
 * and dead code elimination
 */

#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <llvm-c/Core.h>
#include <stdbool.h>

/*
 * GLOBAL NOTES:
 *      All functions defined below take one parameter, namely 'LLVMValueRef function', except for
 *      'optimize()', which is discussed in greater detail below. For all other functions, the 'function'
 *      can be any function defined in the LLVM module. However, because miniC only allows for
 *      one user-defined function per file, these functions are only really useful for optimizing
 *      the single main function.  
 *
 *      For each of the functions below, it is assumed that the 'LLVMValueRef function' being passed belongs to 
 *      a valid LLVM module, produced by the IR Generator.
 */


 /*
  * Returns:
  *     TRUE, if any common subexpressions were successfully eliminated
  *     FALSE, otherwise
  */
bool eliminateCommonSubExpressions(LLVMValueRef function);

 /*
  * Returns:
  *     TRUE, if any constants were successfully folded
  *     FALSE, otherwise
  */
bool foldConstants(LLVMValueRef function);

 /*
  * Returns:
  *     TRUE, if any constants were successfully propagated
  *     FALSE, otherwise
  */
bool propagateConstants(LLVMValueRef function);


 /*
  * Returns:
  *     TRUE, if any dead code was successfully eliminated
  *     FALSE, otherwise
  */
bool eliminateDeadCode(LLVMValueRef function);


 /*
  * Returns:
  *     VOID
  *
  * Notes:
  *     This function calls all four of the above optimizations in a loop until reaching an iteration
  *     in which they all return false. In effect, this ensures that all optimization procedures are 
  *     performed as many times as needed in order achieve maximal optimization.
  */
void optimizeFunction(LLVMValueRef function);

 /*
  * Returns:
  *     VOID
  *
  * Notes:
  *     This function calls 'optimizeFunction()' on each function present within the module. For 
  *     the purposes of a miniC program, it will only call 'optimizeFunction()' once for the single
  *     user-defined function.
  */
void optimize(LLVMModuleRef module);

#endif