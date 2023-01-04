#include "ir/function.h"

#include "llvm/IR/Module.h"

std::pair<llvm::Function *, llvm::Function *>
  lift_func(llvm::Module &OrigModule, llvm::Module &LiftedModule,
            bool asm_input, std::string opt_file2,
            bool opt_asm_only,
            llvm::Function *srcFnLLVM);
