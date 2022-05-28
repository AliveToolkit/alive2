#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"

void optimize_module(llvm::StringRef optArgs,llvm::Module *M);
void optimize_function(llvm::StringRef optArgs, llvm::Function *func);
