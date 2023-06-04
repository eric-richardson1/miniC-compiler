#ifndef CODE_GENERATOR_H
#define CODE_GENERATOR_H 

#include <llvm-c/Core.h>
#include <llvm-c/IRReader.h>
#include <llvm-c/Types.h>

typedef enum {
    EAX,
    EBX,
    ECX,
    EDX,
    SPILL
} reg_t;


void generateAssembly(LLVMModuleRef module);

#endif