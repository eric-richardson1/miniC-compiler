/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * main.c - entry point for the entire compiler, defines required 'main' function
 */

#include "../common/ast.h"
#include <stdio.h>

/****************** FUNCTION HEADERS ******************/
extern astNode *parse(const char*);
extern void analyzeAST(astNode *node);

/* Takes the filepath of a miniC program as input */
int main(int argc, char** argv) {
	astNode *root;
	if (argc == 2){
		root = parse(argv[1]);
	}
	else {
		fprintf(stderr, "Missing argument: miniC filepath\n");
		return 1;
	}
	
	analyzeAST(root);
	freeNode(root);
	return 0;
}
