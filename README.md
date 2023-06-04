# miniC-compiler
A compiler for a subset of the 'C' programming language

## Usage
To build the program, navigate to the 'src' directory and run the following command: \
``make``

This will produce the executable './compile'. This executable expects the user to pass
the path to a miniC file as an argument. To execute the program, run: \
``./compile [miniC-file]``

There are several example miniC files provided in the test directory. For example, the command \
``./compile ../test/final_tests/p1.c``
will run the program on '../test/final_tests/p1.c'.

After running the program, two files will be generated within the 'src' directory:
* 'func.ll' - This is the optimized LLVM IR corresponding to the input miniC program. 
It is created by 'ir_generator.c' and optimized by 'optimizer.c'.
* 'func.s' - This is the assembly code corresponding to the input miniC program. It is created
by 'code_generator.c'

To test the generated assembly code, use the 'main.c' file located in the test directory. From the 'src'
directory, run the command: \
``gcc -o main.out -m32 ../test/final_tests/main.c func.s``

Finally, in order to test the program, simply run the executable: \
``./main.out``

Note: By default, 'main.c' passes the integer 5 as a parameter to the function.
