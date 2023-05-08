/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * main.c - entry point for the entire compiler, defines required 'main' function
 */

#include "../common/ast.h"
#include <stdio.h>
#include <stdbool.h>
#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>

/****************** FUNCTION HEADERS ******************/
extern astNode *parse(const char*);
extern bool isValidAST(astNode *node);
extern LLVMModuleRef generateIR(astNode *root, const char *module_name);

/* Takes the filepath of a miniC program as input */
int main(int argc, char** argv) {
	if (argc == 1) {
		fprintf(stderr, "Missing argument: miniC program filepath\n");
		return 1;
	}
	else if (argc > 2) {
		fprintf(stderr, "Error: too many arguments provided\n");
		return 2;
	}
	const char *filename = argv[1];
	astNode *root = parse(filename);
	if (!isValidAST(root)) {
		freeNode(root);
		return 3;
	}
	LLVMModuleRef module = generateIR(root, filename);
	LLVMDumpModule(module);
	freeNode(root);
	return 0;
}
