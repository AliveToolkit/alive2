#include "ir/function.h"

namespace llvm {
  class Function;
  class Module;
}

namespace lifter {

llvm::Function *adjust(llvm::Function *srcFn);

std::pair<llvm::Function *, llvm::Function *>
  lift_func(llvm::Module *OrigModule, llvm::Module *LiftedModule,
            bool asm_input, std::string opt_file2,
            bool opt_asm_only,
            llvm::Function *srcFnLLVM);

}
