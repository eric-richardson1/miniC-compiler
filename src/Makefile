lex_file = tokenizer
yacc_file = parser
compile: parser/$(lex_file).l parser/$(yacc_file).y ../common/ast.h
	lex parser/$(lex_file).l
	yacc -d -v parser/$(yacc_file).y

	clang++ `llvm-config-15 --cxxflags --ldflags --libs core` -o compile lex.yy.c y.tab.c ../common/ast.c parser/semantic_analysis.c ir_generator/ir_generator.c main.cpp

clean:
	rm lex.yy.c y.tab.c y.tab.h y.output compile
