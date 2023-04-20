/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * parser.y - parses the tokenized "mini_c" input program according to the
 * grammar defined in the 'RULES' section below and builds its corresponding
 * abstract syntax tree. 
 */

/******************** DEFINITIONS ********************/
%{
	#include "../common/ast.h"
	#include <stdio.h>
	
	extern int yylex();
	extern int yylex_destroy();
	extern int yywrap();
	astNode *root;
	astNode *parse(const char *filename);
	int yyerror(const char *);
	extern FILE *yyin;
%}

%union {
	int ival;
	char *sval;
	astNode *node;
	vector<astNode*> *nodeVec;
}

%token <sval> IDENTIFIER 
%token <ival> NUM 
%token IF WHILE INT VOID EXTERN RETURN PRINT READ '=' '(' ')' '{' '}' ';'
%nonassoc IFX
%nonassoc ELSE
%left EQ GEQ LEQ '>' '<'
%left '+' '-'
%left '*' '/'
%type <nodeVec> stmts var_decls
%type <node> stmt call_stmt return_stmt block_stmt decl asgn_stmt while_loop
%type <node> extern_print extern_read def_params function_def
%type <node> term expr condition

%start program

/******************** RULES ********************/
%%
/* mini_c programs start with mandatory declarations of "print" and "read" functions
   followed by the primary function definition */
program : extern_print extern_read function_def {root = createProg($1, $2, $3);}

extern_print : EXTERN VOID PRINT '(' INT ')' ';' {$$ = createExtern("print");}

extern_read : EXTERN INT READ '(' ')' ';' {$$ = createExtern("read");}

/* main function definition followed by a curly-brace-separated block statment */
function_def : INT IDENTIFIER '(' def_params ')' '{' block_stmt '}' {
	$$ = createFunc($2, $4, $7);
	free($2);
}

/* main function can have at most one parameter */
def_params : INT IDENTIFIER {
	$$ = createVar($2);
	free($2);
}
		   | {$$ = NULL;}

/* block statements contain a vector of variable declarations followed by a
   vector of statements */
block_stmt : var_decls stmts {
	vector<astNode*> *block = $1;
	// combine 'var_decls' and 'stmts' vectors
	for (int i = 0; i < $2->size(); i++) {
		block->push_back($2->at(i));
	}
	$$ = createBlock(block);
	delete $2;
} 
		   | stmts {$$ = createBlock($1);}

/* allows for any number of subsequent variable declarations */
var_decls : var_decls decl {
	$$ = $1;
	$$->push_back($2);
} 		  
		  | decl {
	$$ = new vector<astNode*>();
	$$->push_back($1);
}

decl : INT IDENTIFIER ';' {
	$$ = createDecl($2);
	free($2);
}

/* miniC programs are composed of a series of 'stmt' rules as defined below*/
stmts : stmts stmt {
	$$ = $1;
	$$->push_back($2);
}
	  | stmt {
	$$ = new vector<astNode*>();
	$$->push_back($1);
}

/* a statement in miniC must be one of the following */
stmt : asgn_stmt {$$ = $1;}
	 | IF '(' condition ')' stmt %prec IFX {$$ = createIf($3, $5);}
	 | IF '(' condition ')' stmt ELSE stmt {$$ = createIf($3, $5, $7);}
	 | while_loop {$$ = $1;}
	 | '{' block_stmt '}' {$$ = $2;}
	 | call_stmt ';' {$$ = $1;}
	 | return_stmt {$$ = $1;}

/* most basic component - either an integer or variable name*/
term : IDENTIFIER {
	$$ = createVar($1);
	free($1);
}	 
	 | NUM {$$ = createCnst($1);}

/* arithmetic operations and function calls */
expr : term {$$ = $1;}
	 | term '+' term {$$ = createBExpr($1, $3, add);}
	 | term '-' term {$$ = createBExpr($1, $3, sub);}
	 | term '*' term {$$ = createBExpr($1, $3, mul);}
	 | term '/' term {$$ = createBExpr($1, $3, divide);}
	 | '-' term {$$ = createUExpr($2, uminus);}
	 | call_stmt  {$$ = $1;}

/* boolean expressions (should only be used inside IF and WHILE statements)*/
condition : term '<' term {$$ = createRExpr($1, $3, lt);}
		  | term '>' term {$$ = createRExpr($1, $3, gt); }
		  | term EQ term {$$ = createRExpr($1, $3, eq);}
		  | term LEQ term {$$ = createRExpr($1, $3, le);}
		  | term GEQ term {$$ = createRExpr($1, $3, ge);}

asgn_stmt : IDENTIFIER '=' expr ';' {
	astNode *varNode = createVar($1);
	free($1);
	$$ = createAsgn(varNode, $3);
}

while_loop : WHILE '(' condition ')' stmt {$$ = createWhile($3, $5);}

/* 'print' requires a parameter value, 'read' does not */
call_stmt : PRINT '(' term ')' {$$ = createCall("print", $3);}
		  | READ '(' ')' {$$ = createCall("read", NULL);}

return_stmt : RETURN '(' expr ')' ';' {$$ = createRet($3);}		
%%

/* takes the filename of a miniC program as parameter and returns the root node of 
   a fully constructed AST corresponding to the program*/ 
astNode *parse(const char *filename) {
	yyin = fopen(filename, "r");
	yyparse();
	fclose(yyin);
	yylex_destroy();
	return root;
}

/* catches errors while parsing */
int yyerror(const char *message){
	fprintf(stderr, "%s\n", message);
	return 1;
}
