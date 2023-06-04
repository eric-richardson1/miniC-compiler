/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * semantic_analysis.h - defines functions necessary for performing semantic analysis on an 
 * abstract syntax tree.
 */

#ifndef SEMANTIC_ANALYSIS_H
#define SEMANTIC_ANALYSIS_H

#include "../ast/ast.h"

/*
 * Params: 
 *      astNode *root: the root node of an abstract syntax tree representing a 
 *      miniC program.
 *      
 * 
 * Returns:
 *      TRUE, if the provided abstract syntax tree is semantically valid (i.e. there
 *      are no variables that are used before they are declared)
 *
 *      FALSE, OTHERWISE
 */
bool isValidAST(astNode *root);



#endif