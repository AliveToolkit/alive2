// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include "smt/smt.h"
#include "smt/solver.h"
#include "tools/transform.h"
#include "util/config.h"
#include "llvm/IR/InstVisitor.h"
#include "llvm/IR/Operator.h"
#include "llvm/Pass.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/raw_ostream.h"
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace IR;
using namespace tools;
using namespace util;
using namespace std;
using llvm::cast, llvm::dyn_cast, llvm::isa, llvm::errs;
using llvm::LLVMContext;

namespace {

llvm::cl::opt<bool> opt_error_fatal(
  "tv-exit-on-error", llvm::cl::desc("Alive: exit on error"),
  llvm::cl::init(false));

llvm::cl::opt<unsigned> opt_smt_to(
  "tv-smt-to", llvm::cl::desc("Alive: timeout for SMT queries"),
  llvm::cl::init(1000), llvm::cl::value_desc("ms"));

llvm::cl::opt<bool> opt_se_verbose(
  "tv-se-verbose", llvm::cl::desc("Alive: symbolic execution verbose mode"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_stats(
  "tv-smt-stats", llvm::cl::desc("Alive: show SMT statistics"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_skip(
  "tv-smt-skip", llvm::cl::desc("Alive: skip SMT queries"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_smt_verbose(
  "tv-smt-verbose", llvm::cl::desc("Alive: SMT verbose mode"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_tactic_verbose(
  "tv-tactic-verbose", llvm::cl::desc("Alive: SMT Tactic verbose mode"),
  llvm::cl::init(false));

llvm::cl::opt<bool> opt_print_dot(
  "tv-dot", llvm::cl::desc("Alive: print .dot file with CFG of each function"),
  llvm::cl::init(false));

#if 0
string_view s(llvm::StringRef str) {
  return { str.data(), str.size() };
}
#endif

optional<smt::smt_initializer> smt_init;
TransformPrintOpts print_opts;
vector<unique_ptr<IntType>> int_types;
unordered_map<string, pair<Function, unsigned>> fns;
unordered_map<const llvm::Value*, Value*> identifiers;
Function *current_fn;

// cache Value*'s names
unordered_map<const llvm::Value*, std::string> value_names;
unsigned value_id_counter; // for %0, %1, etc..


struct alive_init {
  alive_init() {
    int_types.resize(65);
    int_types[1] = make_unique<IntType>("i1", 1);
  }
} alive_init;

void reset_state() {
  identifiers.clear();
  current_fn = nullptr;
  value_names.clear();
  value_id_counter = 0;
}


string value_name(const llvm::Value &v) {
  auto &name = value_names[&v];
  if (!name.empty())
    return name;

  if (!v.getName().empty())
    return name = '%' + v.getName().str();
  return name = '%' + to_string(value_id_counter++);
}

Type* llvm_type2alive(const llvm::Type *ty) {
  switch (ty->getTypeID()) {
  case llvm::Type::VoidTyID:
    return &Type::voidTy;
  case llvm::Type::IntegerTyID: {
    auto bits = cast<llvm::IntegerType>(ty)->getBitWidth();
    if (bits >= int_types.size())
      int_types.resize(bits + 1);
    if (!int_types[bits])
      int_types[bits] = make_unique<IntType>("i" + to_string(bits), bits);
    return int_types[bits].get();
  }
  default:
    errs() << "Unsupported type: " << *ty << '\n';
    return nullptr;
  }
}


Value* get_operand(llvm::Value *v) {
  if (isa<llvm::Instruction>(v) || isa<llvm::Argument>(v))
    return identifiers[v];

  auto ty = llvm_type2alive(v->getType());
  if (!ty)
    return nullptr;

  // TODO: cache these?
  if (auto cnst = dyn_cast<llvm::ConstantInt>(v)) {
    unique_ptr<IntConst> c;
    if (cnst->getBitWidth() <= 64)
      c = make_unique<IntConst>(*ty, cnst->getZExtValue());
    else
      c = make_unique<IntConst>(*ty, cnst->getValue().toString(10, false));
    auto ret = c.get();
    current_fn->addConstant(move(c));
    return ret;
  }

  if (isa<llvm::UndefValue>(v)) {
    auto val = make_unique<UndefValue>(*ty);
    auto ret = val.get();
    current_fn->addUndef(move(val));
    return ret;
  }

  return nullptr;
}

BasicBlock& getBB(const llvm::BasicBlock *bb) {
  return current_fn->getBB(value_name(*bb));
}


const unordered_map<unsigned, BinOp::Op> llvm_binop2alive = {
#define LLVM_BINOP(x, y) { x, y },
#include "tv/tv.h"
#undef LLVM_BINOP
};

#define PARSE_UNOP()                       \
  auto ty = llvm_type2alive(i.getType());  \
  auto val = get_operand(i.getOperand(0)); \
  if (!ty || !val)                         \
    return error(i)

#define PARSE_BINOP()                     \
  auto ty = llvm_type2alive(i.getType()); \
  auto a = get_operand(i.getOperand(0));  \
  auto b = get_operand(i.getOperand(1));  \
  if (!ty || !a || !b)                    \
    return error(i)

#define PARSE_TRIOP()                     \
  auto ty = llvm_type2alive(i.getType()); \
  auto a = get_operand(i.getOperand(0));  \
  auto b = get_operand(i.getOperand(1));  \
  auto c = get_operand(i.getOperand(2));  \
  if (!ty || !a || !b || !c)              \
    return error(i)

#define RETURN_IDENTIFIER(op)         \
  auto ret = op;                      \
  identifiers.emplace(&i, ret.get()); \
  return ret

class llvm2alive : public llvm::InstVisitor<llvm2alive, unique_ptr<Instr>> {
  llvm::Function &f;
  using RetTy = unique_ptr<Instr>;

public:
  llvm2alive(llvm::Function &f) : f(f) {}

  RetTy visitBinaryOperator(llvm::BinaryOperator &i) {
    PARSE_BINOP();
    auto op_I = llvm_binop2alive.find(i.getOpcode());
    if (op_I == llvm_binop2alive.end())
      return error(i);
    auto alive_op = op_I->second;

    unsigned flags = BinOp::None;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoSignedWrap())
      flags |= BinOp::NSW;
    if (isa<llvm::OverflowingBinaryOperator>(i) && i.hasNoUnsignedWrap())
      flags |= BinOp::NUW;
    if (isa<llvm::PossiblyExactOperator>(i) && i.isExact())
      flags = BinOp::Exact;

    RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, alive_op,
                                         (BinOp::Flags)flags));
  }

  RetTy visitSExtInst(llvm::SExtInst &i) {
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<ConversionOp>(*ty, value_name(i), *val,
                                                ConversionOp::SExt));
  }

  RetTy visitZExtInst(llvm::ZExtInst &i) {
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<ConversionOp>(*ty, value_name(i), *val,
                                                ConversionOp::ZExt));
  }

  RetTy visitTruncInst(llvm::TruncInst &i) {
    PARSE_UNOP();
    RETURN_IDENTIFIER(make_unique<ConversionOp>(*ty, value_name(i), *val,
                                                ConversionOp::Trunc));
  }

  RetTy visitICmpInst(llvm::ICmpInst &i) {
    PARSE_BINOP();
    ICmp::Cond cond;
    switch (i.getPredicate()) {
    case llvm::CmpInst::ICMP_EQ:  cond = ICmp::EQ; break;
    case llvm::CmpInst::ICMP_NE:  cond = ICmp::NE; break;
    case llvm::CmpInst::ICMP_UGT: cond = ICmp::UGT; break;
    case llvm::CmpInst::ICMP_UGE: cond = ICmp::UGE; break;
    case llvm::CmpInst::ICMP_ULT: cond = ICmp::ULT; break;
    case llvm::CmpInst::ICMP_ULE: cond = ICmp::ULE; break;
    case llvm::CmpInst::ICMP_SGT: cond = ICmp::SGT; break;
    case llvm::CmpInst::ICMP_SGE: cond = ICmp::SGE; break;
    case llvm::CmpInst::ICMP_SLT: cond = ICmp::SLT; break;
    case llvm::CmpInst::ICMP_SLE: cond = ICmp::SLE; break;
    default:
      UNREACHABLE();
    }
    RETURN_IDENTIFIER(make_unique<ICmp>(*int_types[1].get(), value_name(i),
                                        cond, *a, *b));
  }

  RetTy visitSelectInst(llvm::SelectInst &i) {
    PARSE_TRIOP();
    RETURN_IDENTIFIER(make_unique<Select>(*ty, value_name(i), *a, *b, *c));
  }

  RetTy visitPHINode(llvm::PHINode &i) {
    Phi::ValTy values;
    auto ty = llvm_type2alive(i.getType());
    if (!ty)
      return error(i);

    for (unsigned idx = 0, e = i.getNumIncomingValues(); idx != e; ++idx) {
      auto op = get_operand(i.getIncomingValue(idx));
      if (!op)
        return error(i);
      values.emplace_back(*op, value_name(*i.getIncomingBlock(idx)));
    }

    RETURN_IDENTIFIER(make_unique<Phi>(*ty, value_name(i), move(values)));
  }

  RetTy visitBranchInst(llvm::BranchInst &i) {
    auto &dst_true = getBB(i.getSuccessor(0));
    if (i.isUnconditional())
      return make_unique<Branch>(dst_true);

    auto &dst_false = getBB(i.getSuccessor(1));
    auto cond = get_operand(i.getCondition());
    if (!cond)
      return error(i);
    return make_unique<Branch>(*cond, dst_true, dst_false);
  }

  RetTy visitSwitchInst(llvm::SwitchInst &i) {
    auto cond = get_operand(i.getCondition());
    if (!cond)
      return error(i);
    auto op = make_unique<Switch>(*cond, getBB(i.getDefaultDest()));

    for (auto &Case : i.cases()) {
      op->addTarget(*get_operand(Case.getCaseValue()),
                    getBB(Case.getCaseSuccessor()));
    }
    return op;
  }

  RetTy visitReturnInst(llvm::ReturnInst &i) {
    if (i.getNumOperands() == 0) {
      assert(i.getType()->isVoidTy());
      return make_unique<Return>(Type::voidTy, Value::voidVal);
    }
    PARSE_UNOP();
    return make_unique<Return>(*ty, *val);
  }

  RetTy visitUnreachableInst(llvm::UnreachableInst &i) {
    auto fals = get_operand(llvm::ConstantInt::getFalse(i.getContext()));
    return make_unique<Assume>(*fals, /*if_non_poison=*/false);
  }

  RetTy visitIntrinsicInst(llvm::IntrinsicInst &i) {
    switch (i.getIntrinsicID()) {
    case llvm::Intrinsic::assume:
    {
      PARSE_UNOP();
      return make_unique<Assume>(*val, false);
    }
    case llvm::Intrinsic::sadd_sat:
    case llvm::Intrinsic::uadd_sat:
    case llvm::Intrinsic::ssub_sat:
    case llvm::Intrinsic::usub_sat:
    case llvm::Intrinsic::cttz:
    case llvm::Intrinsic::ctlz:
    {
      PARSE_BINOP();
      BinOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::sadd_sat: op = BinOp::SAdd_Sat; break;
      case llvm::Intrinsic::uadd_sat: op = BinOp::UAdd_Sat; break;
      case llvm::Intrinsic::ssub_sat: op = BinOp::SSub_Sat; break;
      case llvm::Intrinsic::usub_sat: op = BinOp::USub_Sat; break;
      case llvm::Intrinsic::cttz:     op = BinOp::Cttz; break;
      case llvm::Intrinsic::ctlz:     op = BinOp::Ctlz; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<BinOp>(*ty, value_name(i), *a, *b, op));
    }
    case llvm::Intrinsic::bitreverse:
    case llvm::Intrinsic::bswap:
    case llvm::Intrinsic::ctpop:
    {
      PARSE_UNOP();
      UnaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::bitreverse: op = UnaryOp::BitReverse; break;
      case llvm::Intrinsic::bswap:      op = UnaryOp::BSwap; break;
      case llvm::Intrinsic::ctpop:      op = UnaryOp::Ctpop; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<UnaryOp>(*ty, value_name(i), *val, op));
    }
    case llvm::Intrinsic::fshl:
    case llvm::Intrinsic::fshr:
    {
      PARSE_TRIOP();
      TernaryOp::Op op;
      switch (i.getIntrinsicID()) {
      case llvm::Intrinsic::fshl: op = TernaryOp::FShl; break;
      case llvm::Intrinsic::fshr: op = TernaryOp::FShr; break;
      default: UNREACHABLE();
      }
      RETURN_IDENTIFIER(make_unique<TernaryOp>(*ty, value_name(i), *a, *b, *c,
                                               op));
    }
    case llvm::Intrinsic::expect:
    {
      PARSE_UNOP();
      RETURN_IDENTIFIER(make_unique<CopyOp>(*ty, value_name(i), *val));
    }

    // do nothing intrinsics
    case llvm::Intrinsic::dbg_addr:
    case llvm::Intrinsic::donothing:
      return {};

    default:
      break;
    }
    return error(i);
  }

  RetTy visitDbgInfoIntrinsic(llvm::DbgInfoIntrinsic&) { return {}; }
  RetTy visitInstruction(llvm::Instruction &i) { return error(i); }

  RetTy error(llvm::Instruction &i) {
    errs() << "Unsupported instruction: " << i << '\n';
    return {};
  }

  bool handleMetadata(llvm::Instruction &llvm_i, Instr &i, BasicBlock &BB) {
    llvm::SmallVector<pair<unsigned, llvm::MDNode*>, 8> MDs;
    llvm_i.getAllMetadataOtherThanDebugLoc(MDs);

    for (auto &[ID, Node] : MDs) {
      switch (ID) {
      case LLVMContext::MD_range:
      {
        Value *range = nullptr;
        for (unsigned op = 0, e = Node->getNumOperands(); op < e; ++op) {
          auto *low =
            llvm::mdconst::extract<llvm::ConstantInt>(Node->getOperand(op));
          auto *high =
            llvm::mdconst::extract<llvm::ConstantInt>(Node->getOperand(++op));

          auto op_name = to_string(op / 2);
          auto l = make_unique<ICmp>(*int_types[1].get(),
                                     "$range_l$" + op_name + value_name(llvm_i),
                                     ICmp::SGE, i, *get_operand(low));

          auto h = make_unique<ICmp>(*int_types[1].get(),
                                     "$range_h$" + op_name + value_name(llvm_i),
                                     ICmp::SLT, i, *get_operand(high));

          auto r = make_unique<BinOp>(*int_types[1].get(),
                                      "$range$" + op_name + value_name(llvm_i),
                                      *l.get(), *h.get(), BinOp::And);

          auto r_ptr = r.get();
          BB.addInstr(move(l));
          BB.addInstr(move(h));
          BB.addInstr(move(r));

          if (range) {
            auto range_or = make_unique<BinOp>(*int_types[1].get(),
                                               "$rangeOR$" + op_name +
                                                 value_name(llvm_i),
                                               *range, *r_ptr, BinOp::Or);
            range = range_or.get();
            BB.addInstr(move(range_or));
          } else {
            range = r_ptr;
          }
        }

        if (range)
          BB.addInstr(make_unique<Assume>(*range, true));
        break;
      }
      default:
        errs() << "Unsupported metadata: " << ID << '\n';
        return false;
      }
    }
    return true;
  }

  optional<Function> run() {
    reset_state();

    auto type = llvm_type2alive(f.getReturnType());
    if (!type)
      return {};

    Function Fn(*type, f.getName());
    current_fn = &Fn;

    for (auto &arg : f.args()) {
      auto ty = llvm_type2alive(arg.getType());
      if (!ty)
        return {};
      auto val = make_unique<Input>(*ty, value_name(arg));
      identifiers.emplace(&arg, val.get());
      Fn.addInput(move(val));
    }

    // create all BBs upfront to keep LLVM's order
    // FIXME: this can go away once we have CFG analysis
    for (auto &bb : f) {
      Fn.getBB(value_name(bb));
    }

    for (auto &bb : f) {
      auto &BB = Fn.getBB(value_name(bb));
      for (auto &i : bb) {
        if (auto I = visit(i)) {
          auto alive_i = I.get();
          BB.addInstr(move(I));

          if (i.hasMetadataOtherThanDebugLoc() &&
              !handleMetadata(i, *alive_i, BB))
            return {};
        } else
          return {};
      }
    }

    return move(Fn);
  }
};


struct TVPass : public llvm::FunctionPass {
  static char ID;

  TVPass() : FunctionPass(ID) {}

  bool runOnFunction(llvm::Function &F) override {
    auto fn = llvm2alive(F).run();
    if (!fn) {
      fns.erase(F.getName());
      return false;
    }

    auto [old_fn, inserted] = fns.try_emplace(fn->getName(), move(*fn), 0);

    if (opt_print_dot) {
      auto &f = inserted ? old_fn->second.first : *fn;
      ofstream file(f.getName() + '.' + to_string(old_fn->second.second++)
                      + ".dot");
      CFG(f).printDot(file);
    }

    if (inserted)
      return false;

    smt_init->reset();
    Transform t;
    t.src = move(old_fn->second.first);
    t.tgt = move(*fn);
    TransformVerify verifier(t, false);
    t.print(cerr, print_opts);

    if (Errors errs = verifier.verify()) {
      cerr << "Transformation doesn't verify!\n" << errs << endl;
      if (opt_error_fatal && !errs.isTimeout())
        llvm::report_fatal_error("Alive2: Transform doesn't verify; aborting!");
    } else {
      cerr << "Transformation seems to be correct!\n\n";
    }

    old_fn->second.first = move(t.tgt);
    return false;
  }

  bool doInitialization(llvm::Module&) override {
    static bool done = false;
    if (done)
      return false;
    done = true;

    smt::solver_print_queries(opt_smt_verbose);
    smt::solver_tactic_verbose(opt_tactic_verbose);
    smt::set_query_timeout(to_string(opt_smt_to));
    config::skip_smt = opt_smt_skip;
    config::symexec_print_each_value = opt_se_verbose;

    smt_init.emplace();
    return false;
  }

  bool doFinalization(llvm::Module&) override {
    static bool showed_stats = false;
    if (opt_smt_stats && !showed_stats) {
      smt::solver_print_stats(cerr);
      showed_stats = true;
    }
    smt_init.reset();
    return false;
  }

  void getAnalysisUsage(llvm::AnalysisUsage &AU) const override {
    AU.setPreservesAll();
  }
};

char TVPass::ID = 0;
llvm::RegisterPass<TVPass> X("tv", "Translation Validator", false, false);

}
