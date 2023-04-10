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
%token IDENTIFIER NUM IF WHILE INT VOID EXTERN RETURN FUNC '=' '(' ')' '{' '}' ';'

%nonassoc IFX
%nonassoc ELSE
%left EQ GEQ LEQ '>' '<'
%left '+' '-'
%left '*' '/'
%nonassoc NEG
%start program

/******************** RULES ********************/
%%
/* mini_c programs start with optional declarations of "print" and "read" functions
   followed by the primary function definition */
program : def_func main_func| main_func
def_func : def_func func_extern | func_extern
func_extern : type_extern FUNC '(' type ')' ';'

/* variations on type declarations */
type_extern: EXTERN INT | EXTERN VOID
type_required : INT | VOID
type : type_required | 

main_func : func '{' blocks '}'
func : type_required IDENTIFIER '(' params ')'
params : INT IDENTIFIER | // maximum of one parameter (must be INT)

/* supports nested code blocks separated by curly braces */
blocks : blocks block | block
block : '{' blocks '}'
	  | IF '(' condition ')' block %prec IFX
	  | IF '(' condition ')' block ELSE block
	  | WHILE '(' condition ')' block
	  | statement

/* a statement in mini_c should only be one of the following */
statement : declaration 
		  | assignment 
		  | function_call 
		  | return

declaration : INT IDENTIFIER ';' 
assignment : IDENTIFIER '=' expression ';'
function_call : FUNC '(' expression ')' ';' // for predefined functions "print" and "read"
return : RETURN '(' expression ')' ';'

/* arithmetic operations */
expression : operand 
 | '-' expression %prec NEG
 | expression '+' expression 
 | expression '-' expression 
 | expression '*' expression
 | expression '/' expression
 | '(' expression ')' 

operand : NUM | IDENTIFIER 
 
 /* conditional operations (only for use inside 'if' and 'while' blocks) */
condition : expression EQ expression
		  | expression GEQ expression
		  | expression LEQ expression
		  | expression '>' expression
		  | expression '<' expression

%%
/******************** SUBROUTINES ********************/
/*
 * Allows user to specify a valid file path as a command-line argument
 * (i.e. './parser.out test.c') to run the parser on.
 */
int main(int argc, char** argv){
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
