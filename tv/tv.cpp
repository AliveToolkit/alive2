// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/config.h"
#include "llvm/IR/Function.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"
#include <array>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>

using namespace IR;
using namespace tools;
using namespace util;
using namespace std;
using llvm::cast, llvm::errs;

namespace {

string_view s(llvm::StringRef str) {
  return { str.data(), str.size() };
}

smt::smt_initializer smt_init;
array<unique_ptr<IntType>, 65> int_types;
unordered_map<string, Function> fns;
unordered_map<const llvm::Value*, Value*> identifiers;

struct alive_init {
  alive_init() {
    int_types[0] = nullptr;

    for (unsigned i = 1; i <= 64; ++i) {
      int_types[i] = make_unique<IntType>("i" + to_string(i), i);
    }
  }
} alive_init;


Type* llvm_type2alive(const llvm::Type *ty) {
  switch (ty->getTypeID()) {
  case llvm::Type::VoidTyID:
    return &Type::voidTy;
  case llvm::Type::IntegerTyID: {
    auto bits = cast<llvm::IntegerType>(ty)->getBitWidth();
    return bits <= 64 ? int_types[bits].get() : nullptr;
  }
  default:
    errs() << "Unsupported type: " << *ty << '\n';
    return nullptr;
  }
}


unique_ptr<Instr> llvm_instr2alive(const llvm::Instruction &i) {
  switch (i.getOpcode()) {
  case llvm::Instruction::Add:
  case llvm::Instruction::Sub:
  case llvm::Instruction::Mul:
  case llvm::Instruction::Shl:
  {
    auto ty = llvm_type2alive(i.getType());
    // FIXME: what about const operands?
    auto op1 = identifiers[i.getOperand(0)];
    auto op2 = identifiers[i.getOperand(1)];
    if (!ty || !op1 || !op2)
      goto err;
    // FIXME
    auto op = make_unique<BinOp>(*ty, i.getName().str(), *op1, *op2, BinOp::Add);
    identifiers.emplace(&i, op.get());
    return op;
  }
  default:
    break;
  }
err:
  errs() << "Unsupported instruction: " << i << '\n';
  return {};
}


optional<Function> llvm2alive(const llvm::Function &f) {
  identifiers.clear();

  auto type = llvm_type2alive(f.getReturnType());
  if (!type)
    return {};

  Function Fn(*type, f.getName());

  for (auto &arg : f.args()) {
    auto ty = llvm_type2alive(arg.getType());
    if (!ty)
      return {};
    auto val = make_unique<Input>(*ty, arg.getName().str());
    identifiers.emplace(&arg, val.get());
    Fn.addInput(move(val));
  }

  for (auto &bb : f) {
    auto &BB = Fn.getBB(s(bb.getName()));
    for (auto &i : bb) {
      if (auto I = llvm_instr2alive(i))
        BB.addIntr(move(I));
      else
        return {};
    }
  }

  return move(Fn);
}


struct TVPass : public llvm::FunctionPass {
  static char ID;

  //TransformPrintOpts print_opts;
  bool FatalErrors = true;

  TVPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override {
    auto fn = llvm2alive(F);
    if (fn)
      fn->print(cout);


    //outs() << "verifying " << F.getName() << '\n';
    if (/* !V->verify(F)*/ false && FatalErrors) {
      //errs() << "in function " << F.getName() << '\n';
      //report_fatal_error("Broken function found, compilation aborted!");
    }
    return false;
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};

char TVPass::ID = 0;
llvm::RegisterPass<TVPass> X("tv", "Translation Validator", false, false);

}
