#include <utility>
#include <vector>

// FIXME get rid of these eventually
#include "llvm/IR/Function.h"
#include "llvm/IR/Module.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Support/SourceMgr.h"

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

std::unique_ptr<llvm::MemoryBuffer> generateAsm(llvm::Module &OrigModule,
						llvm::SmallString<1024> &Asm);

std::pair<llvm::Function *, llvm::Function *>
liftFunc(llvm::Module *OrigModule, llvm::Module *LiftedModule,
	 llvm::Function *srcFnLLVM, std::unique_ptr<llvm::MemoryBuffer> MB);

}
