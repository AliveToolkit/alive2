// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "llvm_util/vscale.h"
#include "tools/transform.h"

#include "llvm/ADT/SmallPtrSet.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/Operator.h"
#include <algorithm>

using namespace std;

namespace {

bool hasScalableTypeAttr(const llvm::AttributeList &attrs) {
  for (auto set : attrs) {
    for (auto attr : set) {
      if (attr.isTypeAttribute() && attr.getValueAsType()->isScalableTy())
        return true;
    }
  }
  return false;
}

}

namespace llvm_util {

bool referencesVScale(const llvm::Function &F) {
  vector<const llvm::Value *> worklist { &F };
  for (auto &BB : F)
    for (auto &I : BB)
      worklist.push_back(&I);

  llvm::SmallPtrSet<const llvm::Value *, 32> visited;
  while (!worklist.empty()) {
    auto V = worklist.back();
    worklist.pop_back();
    if (!visited.insert(V).second)
      continue;
    if (V->getType()->isScalableTy())
      return true;

    if (auto fn = llvm::dyn_cast<llvm::Function>(V)) {
      if (fn->getIntrinsicID() == llvm::Intrinsic::vscale ||
          hasScalableTypeAttr(fn->getAttributes()))
        return true;
      for (auto ty : fn->getFunctionType()->subtypes())
        if (ty->isScalableTy())
          return true;
      // Calls are not inlined by the validator.
      continue;
    }
    if (auto gep = llvm::dyn_cast<llvm::GEPOperator>(V)) {
      if (gep->getSourceElementType()->isScalableTy())
        return true;
    }
    if (auto alloc = llvm::dyn_cast<llvm::AllocaInst>(V)) {
      if (alloc->getAllocatedType()->isScalableTy())
        return true;
    }
    if (auto call = llvm::dyn_cast<llvm::CallBase>(V)) {
      if (hasScalableTypeAttr(call->getAttributes()))
        return true;
    }
    if (auto user = llvm::dyn_cast<llvm::User>(V))
      for (auto &op : user->operands())
        worklist.push_back(op.get());
  }
  return false;
}

smt::expr vscaleInRange(const llvm::Function &F, const smt::expr &vscale) {
  auto attr = F.getFnAttribute(llvm::Attribute::VScaleRange);
  if (!attr.isValid())
    return true;
  auto in_range = vscale.uge(attr.getVScaleRangeMin());
  if (auto max = attr.getVScaleRangeMax())
    in_range &= vscale.ule(*max);
  return in_range;
}

optional<vector<unsigned>>
getVScales(const llvm::Function &src, const llvm::Function &tgt,
           bool bidirectional, unsigned min, unsigned max) {
  auto vscale = smt::expr::mkVar("vscale", 32);
  auto admitted = vscaleInRange(src, vscale);
  if (bidirectional)
    admitted |= vscaleInRange(tgt, vscale);
  tools::TypingAssignments types(vscale.isPowerOf2() && admitted &&
                                 vscale.uge(min) && vscale.ule(max));
  vector<unsigned> scales;
  for (; types; ++types)
    scales.push_back(types.getUInt(vscale));
  if (types.hasError())
    return {};
  sort(scales.begin(), scales.end());
  return scales;
}

}
