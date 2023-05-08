/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * semantic_analysis.c - provides functions for performing semantic analysis on an 
 * abstract syntax tree.
 */

#include "../../common/ast.h"
#include <vector>
#include <unordered_set>
#include <stdbool.h>
#include <string>
#include <stdio.h>

/***************************************** FUNCTION HEADERS *****************************************/
bool isValidAST(astNode *root);
void processStmt(astNode *node, std::vector<astNode*> &node_stack, 
	std::vector<std::unordered_set<std::string>> &sym_stack, std::unordered_set<astNode*> &to_pop);

/***************************************** FUNCTION DEFINITIONS *****************************************/
/* Takes root node of a miniC program's AST as parameter and determines if 
   any variables were used before they were declared */
bool isValidAST(astNode *root) {
	std::unordered_set<astNode*> to_pop; // set used to revisit block/function nodes
	std::vector<astNode*> node_stack; // stack used for tree traversal
	std::vector<std::unordered_set<std::string>> sym_stack; // stack used for maintaining symbol tables

	node_stack.push_back(root);

	// Performs a preorder traversal on the AST
	while (!node_stack.empty()) {
		
		astNode *node = node_stack.back();
		node_stack.pop_back();

		switch (node->type) {
			case ast_prog: {
				node_stack.push_back(node->prog.func);
				break;
			}

			case ast_func: {
				// pop symbol table at top of stack if this function block has already been processed
				if (to_pop.count(node)) {
					sym_stack.pop_back();
					to_pop.erase(node);
					break;
				}
				to_pop.insert(node); // revisit this node later
				node_stack.push_back(node);
				
				// initialize new symbol table
				std::unordered_set<std::string> curr_sym_table;
				if (node->func.param != NULL) {
					curr_sym_table.insert(node->func.param->var.name); // function parameters also serve as variable declarations
				}
				sym_stack.push_back(curr_sym_table);
				node_stack.push_back(node->func.body);
				break;
			}
			case ast_stmt: {
				processStmt(node, node_stack, sym_stack, to_pop);
				break;
			}
			case ast_var: {
				const char *curr_var = node->var.name;
				bool found = false;
				// check if current variable appears in an available symbol table
				for (int i = 0; i < sym_stack.size(); i++) {
					if (sym_stack.at(i).count(curr_var)) {
						found = true;
					}
				}
				if (!found) {
					fprintf(stderr, "Error: variable '%s' used before declared\n", curr_var);
					return false;
				}
				break;
			}
			case ast_rexpr: {
				node_stack.push_back(node->rexpr.rhs);
				node_stack.push_back(node->rexpr.lhs);
				break;
			}
			case ast_bexpr: {
				node_stack.push_back(node->bexpr.rhs);
				node_stack.push_back(node->bexpr.lhs);
				break;
			}
			case ast_uexpr: {
				node_stack.push_back(node->uexpr.expr);
				break;
			}
			default: {
				break;
			}
		}
	}
	return true;
}

/* Helper function for analyzeAST(): takes an AST statement node (as well as the node stack, 
   symbol table stack, and 'to_pop' set) as parameters and processes the individual statement separately */
void processStmt(astNode *node, std::vector<astNode*> &node_stack, 
	std::vector<std::unordered_set<std::string>> &sym_stack, std::unordered_set<astNode*> &to_pop) {
	switch (node->stmt.type) {
		case ast_block: {
			// pop symbol table at top of stack if this block has already been processed
			if (to_pop.count(node)) {
				sym_stack.pop_back();
				to_pop.erase(node);
				break;
			}
			to_pop.insert(node); // revisit this node later
			node_stack.push_back(node);

			std::unordered_set<std::string> curr_sym_table;
			sym_stack.push_back(curr_sym_table);
			
			// push each statement inside of the current block statement onto node stack
			vector<astNode*> *slist = node->stmt.block.stmt_list;
			for (int i = slist->size() - 1; i >= 0; i--) {
				node_stack.push_back(slist->at(i));
			}
			break;
		}
		// add newly declared variable name to the symbol table at the top of the stack
		case ast_decl: {
			sym_stack.back().insert(node->stmt.decl.name);
			break;
		}
		case ast_asgn: {
			node_stack.push_back(node->stmt.asgn.rhs);
			node_stack.push_back(node->stmt.asgn.lhs);
			break;

		}
		case ast_if: {
			if (node->stmt.ifn.else_body != NULL) {
				node_stack.push_back(node->stmt.ifn.else_body);
			}
			node_stack.push_back(node->stmt.ifn.if_body);
			node_stack.push_back(node->stmt.ifn.cond);
			break;
		}
		case ast_while: {
			node_stack.push_back(node->stmt.whilen.body);
			node_stack.push_back(node->stmt.whilen.cond);
			break;
		}

		case ast_call: {
			if (node->stmt.call.param != NULL) {
				node_stack.push_back(node->stmt.call.param);
			}
			break;
		}
		case ast_ret: {
			node_stack.push_back(node->stmt.ret.expr);
			break;
		}
	}
}
