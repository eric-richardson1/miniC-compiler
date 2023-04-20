lex_file = tokenizer
yacc_file = parser
compile: $(lex_file).l $(yacc_file).y ../common/ast.h
	yacc -d -v $(yacc_file).y
	lex $(lex_file).l
	g++ -o compile lex.yy.c y.tab.c ../common/ast.c semantic_analysis.c main.c

clean:
	rm lex.yy.c y.tab.c y.tab.h y.output compile
