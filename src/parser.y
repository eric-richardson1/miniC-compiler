/* Author: Eric Richardson
 * Dartmouth CS57, Spring 2023
 * parser.y - parses the tokenized "mini_c" input program according to the
 * grammar defined in the 'RULES' section below. 
 */

/******************** DEFINITIONS ********************/
%{
	#include <stdio.h>
	extern int yylex();
	extern int yylex_destroy();
	extern int yywrap();
	int yyerror(char *);
	extern FILE *yyin;
%}
%token IDENTIFIER NUM IF WHILE INT VOID EXTERN RETURN PRINT READ '=' '(' ')' '{' '}' ';'

%nonassoc IFX
%nonassoc ELSE
%left EQ GEQ LEQ '>' '<'
%left '+' '-'
%left '*' '/'
%start program

/******************** RULES ********************/
%%
/* mini_c programs start with mandatory declarations of "print" and "read" functions
   followed by the primary function definition */
program : extern function_def
extern : extern_print extern_read
extern_print : EXTERN VOID PRINT '(' INT ')' ';'
extern_read : EXTERN INT READ '(' ')' ';'

/* Main function definition followed by a curly-brace-separated block statment */
function_def : func_header '{' block_stmt '}'
func_header : INT IDENTIFIER '(' def_params ')'

def_params : INT IDENTIFIER | // maximum of one parameter (must be INT)
call_params : term |		// function calls don't require type declarations

/* supports nested code blocks separated by curly braces */
block_stmt : var_decls stmts | stmts
var_decls : var_decls decl | decl
decl : INT IDENTIFIER ';' 


stmts : stmts stmt | stmt // supports subsequent statements

/* a statement in miniC must be one of the following */
stmt : asgn_stmt
	 | IF '(' condition ')' stmt %prec IFX
	 | IF '(' condition ')' stmt ELSE stmt
	 | while_loop
	 | '{' block_stmt '}'
	 | call_stmt ';'
	 | return_stmt

term : IDENTIFIER | NUM
/* arithmetic operations and function calls */
expr : term
	 | term '+' term 
	 | term '-' term 
	 | term '*' term 
	 | term '/' term 
	 | '-' term 
	 | call_stmt 

/* boolean expressions (should only be used inside IF and WHILE statements)*/
condition : term '<' term
		  | term '>' term
		  | term EQ term
		  | term LEQ term
		  | term GEQ term

asgn_stmt : IDENTIFIER '=' expr ';'
while_loop : WHILE '(' condition ')' stmt
call_stmt : PRINT '(' call_params ')' | READ '(' call_params ')'
return_stmt : RETURN '(' expr ')' ';'
%%

int main(int argc, char** argv) {
	if (argc == 2){
		yyin = fopen(argv[1], "r");
		yyparse();
		fclose(yyin);
		yylex_destroy();
	}	
	return 0;
}

int yyerror(char * message){
	fprintf(stderr, "%s\n", message);
}
