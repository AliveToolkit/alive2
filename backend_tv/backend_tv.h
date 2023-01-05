#include <utility>
#include <vector>

#include "ir/function.h"

namespace llvm {
  class Function;
  class Module;
}

namespace lifter {

// FIXME
extern std::vector<std::pair<unsigned, unsigned>> new_input_idx_bitwidth;
extern unsigned orig_ret_bitwidth;
extern bool has_ret_attr;

void init();

llvm::Function *adjustSrc(llvm::Function *srcFn);

std::pair<llvm::Function *, llvm::Function *>
liftFunc(llvm::Module *OrigModule, llvm::Module *LiftedModule,
	 bool asm_input, std::string opt_file2,
	 bool opt_asm_only,
	 llvm::Function *srcFnLLVM);

}
