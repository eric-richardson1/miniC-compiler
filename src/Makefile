lex_file = tokenizer
yacc_file = parser
$(yacc_file).out: $(lex_file).l $(yacc_file).y
	yacc -d -v $(yacc_file).y
	lex $(lex_file).l
	gcc -o $(yacc_file).out lex.yy.c y.tab.c

clean:
	rm lex.yy.c y.tab.c y.tab.h $(yacc_file).out y.output
