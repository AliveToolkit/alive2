#include "tv/utils.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Passes/PassPlugin.h"
#include "llvm/Support/raw_ostream.h"
#include <algorithm>
#include <fstream>
#include <string>
#include <vector>

using namespace std;

namespace {

llvm::cl::OptionCategory alive_cmdargs("Alive2 translation validation options");

llvm::cl::opt<string> batch_out("tv-batch-out",
  llvm::cl::desc("The output file path to store the list of batched opts"),
  llvm::cl::cat(alive_cmdargs), llvm::cl::Required);

struct batch {
  int st_n; // The bitcode *after* this optimization is used as src.
            // 1 is the first optimization.
  std::string st_name;
  int ed_n; // The bitcode after this optimization is used as tgt.
            // -1 if not determined yet.
  std::string ed_name;
};

vector<batch> batches;
string last_opt_name;
int last_opt_n = 0;

void emitBatchedOptList() {
  ofstream fout(batch_out);
  if (batches.size() == 0) {
    // batch whole opts from the begin to the end
    batch s = {1, "", last_opt_n, last_opt_name};
    batches.emplace_back(move(s));
  } else {
    auto &itm = batches.back();
    assert(itm.ed_n == -1);
    itm.ed_n = last_opt_n;
    itm.ed_name = last_opt_name;
  }

  for (auto &itm : batches) {
    fout << itm.st_n << " " << itm.st_name << "\n";
    fout << itm.ed_n << " " << itm.ed_name << "\n";
  }
  fout.close();
}


struct FinalizeBatchPass : public llvm::PassInfoMixin<FinalizeBatchPass> {
  llvm::PreservedAnalyses run(llvm::Module &M,
                              llvm::ModuleAnalysisManager &AM) {
    emitBatchedOptList();
    return llvm::PreservedAnalyses::all();
  }
};

extern "C" ::llvm::PassPluginLibraryInfo LLVM_ATTRIBUTE_WEAK
llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "Alive2 Translation Validation", "",
    [](llvm::PassBuilder &PB) {
      // For clang tv, manually run TVPass after each pass
      PB.registerOptimizerLastEPCallback(
          [](llvm::ModulePassManager &MPM,
             llvm::PassBuilder::OptimizationLevel) {
            MPM.addPass(FinalizeBatchPass());
          });
      PB.getPassInstrumentationCallbacks()->registerAfterPassCallback(
          [](llvm::StringRef P, llvm::Any IR,
                  const llvm::PreservedAnalyses &PA) {
        if (last_opt_n == 0) {
          // clang tv does not know 'the initial bitcode'.
          // We can only get the bitcode after at least one optimization is
          // performed.
          ++last_opt_n;
          last_opt_name = P.str();
          return;
        }
        if (tv::do_skip(P.str())) {
          // Unsupported opt found, batch until the prev opt
          if (!batches.empty() && batches.back().ed_n == -1) {
            auto &itm = batches.back();
            itm.ed_n = last_opt_n;
            itm.ed_name = move(last_opt_name);
          }
        } else {
          if (batches.empty() || batches.back().ed_n != -1) {
            // Start a new batch. last_opt_n is begin.
            batch s = {last_opt_n, move(last_opt_name), -1, ""};
            batches.emplace_back(move(s));
          } // Otherwise, keep batching
        }
        ++last_opt_n;
        last_opt_name = P.str();
      });
    }
  };
}

}
