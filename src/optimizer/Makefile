LLVMCODE = optimizer
TEST = ../../test/optimizer_tests/test1

$(LLVMCODE): $(LLVMCODE).c
	clang -g `llvm-config-15 --cflags` -I /usr/include/llvm-c-15 -c $(LLVMCODE).c
	clang++ `llvm-config-15 --cxxflags --ldflags --libs core` -I /usr/include/llvm-c-15 $(LLVMCODE).o -o $@

llvm_file: $(TEST).c
	clang -S -emit-llvm $(TEST).c -o $(TEST).ll

optimize: optimizer.cpp
	clang++ `llvm-config-15 --cxxflags --ldflags --libs core` -o optimize.o optimizer.cpp -I /usr/include/llvm-c-15/

clean: 
	rm -rf $(TEST).ll
	rm -rf $(LLVMCODE)
	rm -rf *.o
