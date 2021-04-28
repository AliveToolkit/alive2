#include "utils.h"
#include <algorithm>
#include <iostream>
#include <sstream>
using namespace std;

static const char* skip_pass_list[] = {
  "ArgumentPromotionPass",
  "DeadArgumentEliminationPass",
  "EliminateAvailableExternallyPass",
  "EntryExitInstrumenterPass",
  "GlobalOptPass",
  "HotColdSplittingPass",
  "InferFunctionAttrsPass", // IPO
  "InlinerPass",
  "IPSCCPPass",
  "ModuleInlinerWrapperPass",
  "OpenMPOptPass",
  "PostOrderFunctionAttrsPass", // IPO
  "TailCallElimPass",
};
namespace tv {

bool do_skip(const llvm::StringRef &pass0) {
  auto pass = pass0.str();
  return any_of(skip_pass_list, end(skip_pass_list),
                [&](auto skip) { return pass == skip; });
}

}