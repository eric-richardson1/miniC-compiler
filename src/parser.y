/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * parser.y - parses the tokenized "mini_c" input program according to the
 * grammar defined in the 'RULES' section below. 
 */

/******************** DEFINITIONS ********************/
%{
	#include "../common/ast.h"
	#include <vector>
	#include <stdio.h>
	extern int yylex();
	extern int yylex_destroy();
	extern int yywrap();
	astNode *root;
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
%type <node> term expr call_stmt asgn_stmt stmt condition while_loop block_stmt decl return_stmt extern_print extern_read def_params function_def

%start program

/******************** RULES ********************/
%%
/* mini_c programs start with mandatory declarations of "print" and "read" functions
   followed by the primary function definition */
program : extern_print extern_read function_def {
	root = createProg($1, $2, $3);
}
extern_print : EXTERN VOID PRINT '(' INT ')' ';' {
	$$ = createExtern("print");
}
extern_read : EXTERN INT READ '(' ')' ';' {
	$$ = createExtern("read");
}

/* Main function definition followed by a curly-brace-separated block statment */
function_def : INT IDENTIFIER '(' def_params ')' '{' block_stmt '}' {
				$$ = createFunc($2, $4, $7);
				free($2);
			
			}

def_params : INT IDENTIFIER {
				$$ = createVar($2);
				free($2);
			}
		   | {
			   $$ = NULL;
		   }

/* supports nested code blocks separated by curly braces */
block_stmt : var_decls stmts {
			vector<astNode*> *block = $1;
			for (int i = 0; i < $2->size(); i++) {
				block->push_back($2->at(i));
			}
			$$ = createBlock(block);
			delete $2;

		   } 
		   | stmts {
				$$ = createBlock($1);	
				
		   }	
var_decls : var_decls decl {
			$$ = $1;
			$$->push_back($2);
		} | decl {
			$$ = new vector<astNode*>();
			$$->push_back($1);
		}
decl : INT IDENTIFIER ';' {
		$$ = createDecl($2);
		free($2);
	}


stmts : stmts stmt {
		$$ = $1;
		$$->push_back($2);
	}
	  | stmt {
		$$ = new vector<astNode*>();
		$$->push_back($1);
	}		// supports subsequent statements

/* a statement in miniC must be one of the following */
stmt : asgn_stmt {
		$$ = $1;
	}
	 | IF '(' condition ')' stmt %prec IFX {
		 $$ = createIf($3, $5);
	}
	 | IF '(' condition ')' stmt ELSE stmt {
		 $$ = createIf($3, $5, $7);
	 }
	 | while_loop {
		 $$ = $1;
	 }
	 | '{' block_stmt '}' {
		 $$ = $2;
	 }
	 | call_stmt ';' {
		 $$ = $1;
	}
	 | return_stmt {
		 $$ = $1;
	 }

term : IDENTIFIER {
	$$ = createVar($1);
	free($1);
}	 | NUM {
	$$ = createCnst($1);
}
/* arithmetic operations and function calls */
expr : term {
		$$ = $1;
	}
	 | term '+' term {
		 $$ = createBExpr($1, $3, add);
	 }
	 | term '-' term {
		 $$ = createBExpr($1, $3, sub);
	 }
	 | term '*' term {
		 $$ = createBExpr($1, $3, mul);
	 }
	 | term '/' term {
		 $$ = createBExpr($1, $3, divide);
	 }
	 | '-' term {
		 $$ = createUExpr($2, uminus);
	 }
	 | call_stmt  {
		 $$ = $1;
	 }

/* boolean expressions (should only be used inside IF and WHILE statements)*/
condition : term '<' term {
			$$ = createRExpr($1, $3, lt);
		}
		  | term '>' term {
			$$ = createRExpr($1, $3, gt); 
		}
		  | term EQ term {
			$$ = createRExpr($1, $3, eq);
		}
		  | term LEQ term {
			$$ = createRExpr($1, $3, le);
		}
		  | term GEQ term {
			$$ = createRExpr($1, $3, ge);
		}

asgn_stmt : IDENTIFIER '=' expr ';' {
	astNode *varNode = createVar($1);
	free($1);
	$$ = createAsgn(varNode, $3);
}
while_loop : WHILE '(' condition ')' stmt {
			$$ = createWhile($3, $5);
		}
call_stmt : PRINT '(' term ')' {
			$$ = createCall("print", $3);
		}
		  | READ '(' ')' {
			  $$ = createCall("read", NULL);

		}
return_stmt : RETURN '(' expr ')' ';' {
			$$ = createRet($3);
		}		
%%

int main(int argc, char** argv) {
	if (argc == 2){
		yyin = fopen(argv[1], "r");
	}
	yyparse();

	if (yyin != stdin) {
		fclose(yyin);
	}
	yylex_destroy();

	printNode(root, 5);
	freeNode(root);
	return 0;
}

int yyerror(const char *message){
	fprintf(stderr, "%s\n", message);
	return 1;
}
