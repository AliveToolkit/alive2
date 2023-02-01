// include first to avoid ambiguity for comparison operator from
// util/spaceship.h
#include "llvm/MC/MCAsmInfo.h"

#include "backend_tv/lifter.h"

#include "llvm/ADT/BitVector.h"
#include "llvm/ADT/DenseSet.h"
#include "llvm/ADT/SetVector.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/AsmParser/Parser.h"
#include "llvm/Bitcode/BitcodeReader.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/InitializePasses.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCInstBuilder.h"
#include "llvm/MC/MCInstPrinter.h"
#include "llvm/MC/MCInstrAnalysis.h"
#include "llvm/MC/MCInstrInfo.h"
#include "llvm/MC/MCParser/MCAsmParser.h"
#include "llvm/MC/MCParser/MCTargetAsmParser.h"
#include "llvm/MC/MCRegisterInfo.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm/MC/MCSubtargetInfo.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/MCTargetOptionsCommandFlags.h"
#include "llvm/MC/TargetRegistry.h"
#include "llvm/Pass.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/MathExtras.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/Signals.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Target/TargetMachine.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/Transforms/Utils/Cloning.h"

#define GET_INSTRINFO_ENUM
#include "Target/AArch64/AArch64GenInstrInfo.inc"

#define GET_REGINFO_ENUM
#include "Target/AArch64/AArch64GenRegisterInfo.inc"

#include <algorithm>
#include <fstream>
#include <iostream>
#include <ranges>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace std;
using namespace llvm;
using namespace lifter;

// avoid collisions with the upstream AArch64 namespace
namespace llvm::AArch64 {
const unsigned N = 100000000;
const unsigned Z = 100000001;
const unsigned C = 100000002;
const unsigned V = 100000003;
} // namespace llvm::AArch64

namespace {

const set<int> s_flag = {
    AArch64::ADDSWri, AArch64::ADDSWrs,
    AArch64::ADDSWrx,
    AArch64::ADDSXri,
    AArch64::ADDSXrs,
    AArch64::ADDSXrx,
    AArch64::SUBSWri,
    AArch64::SUBSWrs,
    AArch64::SUBSWrx,
    AArch64::SUBSXri,
    AArch64::SUBSXrs,
    AArch64::SUBSXrx,
    AArch64::ANDSWri,
    AArch64::ANDSWrr,
    AArch64::ANDSWrs,
    AArch64::ANDSXri,
    AArch64::ANDSXrr,
    AArch64::ANDSXrs,
    AArch64::BICSWrs,
    AArch64::BICSXrs,
    AArch64::ADCSXr,
    AArch64::ADCSWr,
};

const set<int> instrs_32 = {
    AArch64::ADDWrx,   AArch64::ADDSWrs, AArch64::ADDSWri, AArch64::ADDWrs,
    AArch64::ADDWri,   AArch64::ADDSWrx, AArch64::ADCWr,   AArch64::ADCSWr,
    AArch64::ASRVWr,   AArch64::SUBWri,  AArch64::SUBWrs,  AArch64::SUBWrx,
    AArch64::SUBSWrs,  AArch64::SUBSWri, AArch64::SUBSWrx, AArch64::SBFMWri,
    AArch64::CSELWr,   AArch64::ANDWri,  AArch64::ANDWrr,  AArch64::ANDWrs,
    AArch64::ANDSWri,  AArch64::ANDSWrr, AArch64::ANDSWrs, AArch64::MADDWrrr,
    AArch64::MSUBWrrr, AArch64::EORWri,  AArch64::CSINVWr, AArch64::CSINCWr,
    AArch64::MOVZWi,   AArch64::MOVNWi,  AArch64::MOVKWi,  AArch64::LSLVWr,
    AArch64::LSRVWr,   AArch64::ORNWrs,  AArch64::UBFMWri, AArch64::BFMWri,
    AArch64::ORRWrs,   AArch64::ORRWri,  AArch64::SDIVWr,  AArch64::UDIVWr,
    AArch64::EXTRWrri, AArch64::EORWrs,  AArch64::RORVWr,  AArch64::RBITWr,
    AArch64::CLZWr,    AArch64::REVWr,   AArch64::CSNEGWr, AArch64::BICWrs,
    AArch64::BICSWrs,  AArch64::EONWrs,  AArch64::REV16Wr, AArch64::Bcc,
    AArch64::CCMPWr,   AArch64::CCMPWi};

const set<int> instrs_64 = {
    AArch64::ADDXrx,    AArch64::ADDSXrs,   AArch64::ADDSXri,
    AArch64::ADDXrs,    AArch64::ADDXri,    AArch64::ADDSXrx,
    AArch64::ADCXr,     AArch64::ADCSXr,    AArch64::ASRVXr,
    AArch64::SUBXri,    AArch64::SUBXrs,    AArch64::SUBXrx,
    AArch64::SUBSXrs,   AArch64::SUBSXri,   AArch64::SUBSXrx,
    AArch64::SBFMXri,   AArch64::CSELXr,    AArch64::ANDXri,
    AArch64::ANDXrr,    AArch64::ANDXrs,    AArch64::ANDSXri,
    AArch64::ANDSXrr,   AArch64::ANDSXrs,   AArch64::MADDXrrr,
    AArch64::MSUBXrrr,  AArch64::EORXri,    AArch64::CSINVXr,
    AArch64::CSINCXr,   AArch64::MOVZXi,    AArch64::MOVNXi,
    AArch64::MOVKXi,    AArch64::LSLVXr,    AArch64::LSRVXr,
    AArch64::ORNXrs,    AArch64::UBFMXri,   AArch64::BFMXri,
    AArch64::ORRXrs,    AArch64::ORRXri,    AArch64::SDIVXr,
    AArch64::UDIVXr,    AArch64::EXTRXrri,  AArch64::EORXrs,
    AArch64::SMADDLrrr, AArch64::UMADDLrrr, AArch64::RORVXr,
    AArch64::RBITXr,    AArch64::CLZXr,     AArch64::REVXr,
    AArch64::CSNEGXr,   AArch64::BICXrs,    AArch64::BICSXrs,
    AArch64::EONXrs,    AArch64::SMULHrr,   AArch64::UMULHrr,
    AArch64::REV32Xr,   AArch64::REV16Xr,   AArch64::SMSUBLrrr,
    AArch64::UMSUBLrrr, AArch64::PHI,       AArch64::TBZW,
    AArch64::TBZX,      AArch64::TBNZW,     AArch64::TBNZX,
    AArch64::B,         AArch64::CBZW,      AArch64::CBZX,
    AArch64::CBNZW,     AArch64::CBNZX,     AArch64::CCMPXr,
    AArch64::CCMPXi,    AArch64::LDRXui,    AArch64::MSR,
    AArch64::MRS};

const set<int> instrs_128 = {AArch64::FMOVXDr, AArch64::INSvi64gpr};

const set<int> instrs_no_write = {
    AArch64::Bcc,    AArch64::B,     AArch64::TBZW,   AArch64::TBZX,
    AArch64::TBNZW,  AArch64::TBNZX, AArch64::CBZW,   AArch64::CBZX,
    AArch64::CBNZW,  AArch64::CBNZX, AArch64::CCMPWr, AArch64::CCMPWi,
    AArch64::CCMPXr, AArch64::CCMPXi};

const set<int> ins_variant = {AArch64::INSvi64gpr};

bool has_s(int instr) {
  return s_flag.contains(instr);
}

// FIXME -- do this without the strings, just keep a map or something
BasicBlock *getBBByName(Function &Fn, StringRef name) {
  for (auto &bb : Fn) {
    if (bb.getName() == name)
      return &bb;
  }
  assert(false && "BB not found");
}

// do not delete this line
mc::RegisterMCTargetOptionsFlags MOF;

string findTargetLabel(MCInst &Inst) {
  auto num_operands = Inst.getNumOperands();
  for (unsigned i = 0; i < num_operands; ++i) {
    auto op = Inst.getOperand(i);
    if (op.isExpr()) {
      auto expr = op.getExpr();
      if (expr->getKind() == MCExpr::ExprKind::SymbolRef) {
        const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*expr);
        const MCSymbol &Sym = SRE.getSymbol();
        return Sym.getName().str();
      }
    }
  }
  assert(false && "Could not find target label in arm branch instruction");
  UNREACHABLE();
}

void print(MCInst &Inst) {
  *out << "< MCInst " << Inst.getOpcode() << " ";
  for (auto it = Inst.begin(); it != Inst.end(); ++it) {
    if (it->isReg()) {
      *out << "<MCOperand Reg:(" << it->getReg() << ")>";
    } else if (it->isImm()) {
      *out << "<MCOperand Imm:" << it->getImm() << ">";
    } else if (it->isExpr()) {
      *out << "<MCOperand Expr:>"; // FIXME
    } else {
      assert("MCInst printing an unsupported operand" && false);
    }
  }
  *out << ">\n";
}

// Represents a basic block of machine instructions
class MCBasicBlock {
private:
  string name;
  using SetTy = SetVector<MCBasicBlock *>;
  vector<MCInst> Instrs;
  SetTy Succs;

public:
  MCBasicBlock(string _name) : name(_name) {}
  // MCBasicBlock(const MCBasicBlock&) =delete;

  const string &getName() const {
    return name;
  }

  auto &getInstrs() {
    return Instrs;
  }

  auto size() const {
    return Instrs.size();
  }

  auto &getSuccs() {
    return Succs;
  }

  void addInst(MCInst &inst) {
    Instrs.push_back(inst);
  }

  void addInstBegin(MCInst &&inst) {
    Instrs.insert(Instrs.begin(), std::move(inst));
  }

  void addSucc(MCBasicBlock *succ_block) {
    Succs.insert(succ_block);
  }

  auto succBegin() const {
    return Succs.begin();
  }

  auto succEnd() const {
    return Succs.end();
  }
};

// Represents a machine function
class MCFunction {
  string name;
  unsigned label_cnt{0};
  using BlockSetTy = SetVector<MCBasicBlock *>;

public:
  MCInstrAnalysis *IA;
  MCInstPrinter *IP;
  MCRegisterInfo *MRI;
  vector<MCBasicBlock> BBs;

  MCFunction() {}
  MCFunction(string _name) : name(_name) {}

  void setName(string _name) {
    name = _name;
  }

  MCBasicBlock *addBlock(string b_name) {
    return &BBs.emplace_back(b_name);
  }

  string getName() {
    return name;
  }

  string getLabel() {
    return name + to_string(++label_cnt);
  }

  MCBasicBlock *findBlockByName(string b_name) {
    for (auto &bb : BBs)
      if (bb.getName() == b_name)
        return &bb;
    assert(false && "block not found");
  }

  void addEntryBlock() {
    *out << "adding fresh entry block to lifted code\n";
    BBs.emplace(BBs.begin(), "arm_tv_entry");
    MCInst jmp_instr;
    jmp_instr.setOpcode(AArch64::B);
    jmp_instr.addOperand(MCOperand::createImm(1));
    BBs[0].addInstBegin(std::move(jmp_instr));
  }
  
  void checkEntryBlock() {
    // If we have an empty assembly function, we need to add an entry block with
    // a return instruction
    if (BBs.empty()) {
      *out << "adding entry block to empty function\n";
      auto new_block = addBlock("entry");
      MCInst ret_instr;
      ret_instr.setOpcode(AArch64::RET);
      new_block->addInstBegin(std::move(ret_instr));
    }

    // LLVM doesn't let the entry block be a jump target, but assembly
    // does, so let's fix that up if necessary. alternatively, we
    // could simply add the new block unconditionally and let
    // simplifyCFG clean up the mess.
    const auto &first_block = BBs[0];
    for (unsigned i = 0; i < BBs.size(); ++i) {
      auto &cur_bb = BBs[i];
      auto &last_mc_instr = cur_bb.getInstrs().back();
      if (IA->isConditionalBranch(last_mc_instr) ||
          IA->isUnconditionalBranch(last_mc_instr)) {
        string target = findTargetLabel(last_mc_instr);
        if (target == first_block.getName()) {
	  addEntryBlock();
	  return;
        }
      }
    }
  }

  void printBlocks() {
    *out << "# of Blocks (orig print blocks) = " << BBs.size() << '\n';
    *out << "-------------\n";
    int i = 0;
    for (auto &block : BBs) {
      *out << "block " << i << ", name= " << block.getName() << '\n';
      for (auto &Inst : block.getInstrs()) {
	std::string sss;
	llvm::raw_string_ostream ss(sss);
        Inst.dump_pretty(ss, IP, " ", MRI);
        *out << sss << '\n';
      }
      i++;
    }
  }
};

// Code taken from llvm. This should be okay for now. But we generally
// don't want to trust the llvm implementation so we need to complete my
// implementation at function decode_bit_mask
uint64_t ror(uint64_t elt, unsigned size) {
  return ((elt & 1) << (size - 1)) | (elt >> 1);
}

/// decodeLogicalImmediate - Decode a logical immediate value in the form
/// "N:immr:imms" (where the immr and imms fields are each 6 bits) into the
/// integer value it represents with regSize bits.
uint64_t decodeLogicalImmediate(uint64_t val, unsigned regSize) {
  // Extract the N, imms, and immr fields.
  unsigned N = (val >> 12) & 1;
  unsigned immr = (val >> 6) & 0x3f;
  unsigned imms = val & 0x3f;

  assert((regSize == 64 || N == 0) && "undefined logical immediate encoding");
  int len = 31 - countLeadingZeros((N << 6) | (~imms & 0x3f));
  assert(len >= 0 && "undefined logical immediate encoding");
  unsigned size = (1 << len);
  unsigned R = immr & (size - 1);
  unsigned S = imms & (size - 1);
  assert(S != size - 1 && "undefined logical immediate encoding");
  uint64_t pattern = (1ULL << (S + 1)) - 1;
  for (unsigned i = 0; i < R; ++i)
    pattern = ror(pattern, size);

  // Replicate the pattern to fill the regSize.
  while (size != regSize) {
    pattern |= (pattern << size);
    size *= 2;
  }
  return pattern;
}

// Values currently holding the latest definition for a volatile register, for
// each basic block currently used by vector instructions only
// TODO remove this
unordered_map<MCBasicBlock *, unordered_map<unsigned, Value *>> cur_vol_regs;

BasicBlock *getBB(Function &F, MCOperand &jmp_tgt) {
  assert(jmp_tgt.isExpr() && "[getBB] expected expression operand");
  assert((jmp_tgt.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
         "[getBB] expected symbol ref as jump operand");
  const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt.getExpr());
  const MCSymbol &Sym = SRE.getSymbol();
  StringRef name = Sym.getName();
  for (auto &bb : F) {
    if (bb.getName() == name)
      return &bb;
  }
  assert(false && "basic block not found");
}

class arm2llvm_ {
  Module *LiftedModule{nullptr};
  LLVMContext &Ctx = LiftedModule->getContext();
  MCFunction &MF;
  Function &srcFn;
  MCBasicBlock *MCBB{nullptr};
  BasicBlock *LLVMBB{nullptr};
  MCInstPrinter *instrPrinter{nullptr};
  MCInst *CurInst{nullptr};
  unsigned instCount{0};
  bool ret_void{false};
  map<unsigned, Value *> RegFile;
  Value *stackMem{nullptr};
  
  Type *getIntTy(int bits) {
    // just trying to catch silly errors, remove this sometime
    assert(bits > 0 && bits <= 129);
    return Type::getIntNTy(Ctx, bits);
  }

  Value *getIntConst(uint64_t val, int bits) {
    return ConstantInt::get(Ctx, llvm::APInt(bits, val));
  }

  [[noreturn]] void visitError(MCInst &I) {
    out->flush();
    string str(instrPrinter->getOpcodeName(I.getOpcode()));
    *out << "ERROR: Unsupported arm instruction: "
           << str << "\n";
    out->flush();
    exit(-1); // FIXME handle this better
  }

  unsigned getInstSize(int instr) {
    if (instrs_32.contains(instr))
      return 32;
    if (instrs_64.contains(instr))
      return 64;
    if (instrs_128.contains(instr))
      return 128;
    *out << "getInstSize encountered unknown instruction"
           << "\n";
    visitError(*CurInst);
    UNREACHABLE();
  }

  // from getShiftType/getShiftValue:
  // https://github.com/llvm/llvm-project/blob/93d1a623cecb6f732db7900baf230a13e6ac6c6a/llvm/lib/Target/AArch64/MCTargetDesc/AArch64AddressingModes.h#L74
  // LLVM encodes its shifts into immediates (ARM doesn't, it has a shift
  // field in the instruction)
  Value *reg_shift(Value *value, int encodedShift) {
    int shift_type = (encodedShift >> 6) & 0x7;
    auto W = value->getType()->getIntegerBitWidth();
    auto exp = getIntConst(encodedShift & 0x3f, W);

    switch (shift_type) {
    case 0:
      return createShl(value, exp);
    case 1:
      return createLShr(value, exp);
    case 2:
      return createAShr(value, exp);
    case 3:
      // ROR shift
      return createFShr(
          value, value,
          exp);
    default:
      // FIXME: handle other case (msl)
      report_fatal_error("shift type not supported");
    }
  }

  Value *reg_shift(int value, int size, int encodedShift) {
    return reg_shift(getIntConst(value, size), encodedShift);
  }

  // lifted instructions are named using the number of the ARM
  // instruction they come from
  string nextName() {
    stringstream ss;
    ss << "a" << instCount << "_";
    return ss.str();
  }

  AllocaInst *createAlloca(Type *ty, Value *sz, const string &NameStr) {
    auto A = new AllocaInst(ty, 0, sz, NameStr, LLVMBB);
    // TODO initialize ARM memory cells to freeze poison?
    return A;
  }

  GetElementPtrInst *createGEP(Type *ty, Value *v, ArrayRef<Value *> idxlist,
                               const string &NameStr) {
    return GetElementPtrInst::Create(ty, v, idxlist, NameStr, LLVMBB);
  }

  void createBranch(Value *c, BasicBlock *t, BasicBlock *f) {
    BranchInst::Create(t, f, c, LLVMBB);
  }

  void createBranch(BasicBlock *dst) {
    BranchInst::Create(dst, LLVMBB);
  }

  LoadInst *createLoad(Type *ty, Value *ptr, const string &NameStr = "") {
    return new LoadInst(ty, ptr, (NameStr == "") ? nextName() : NameStr,
                        LLVMBB);
  }

  void createStore(Value *v, Value *ptr) {
    new StoreInst(v, ptr, LLVMBB);
  }

  CallInst *createSSubOverflow(Value *a, Value *b) {
    auto ssub_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::ssub_with_overflow, a->getType());
    return CallInst::Create(ssub_decl, {a, b}, nextName(), LLVMBB);
  }

  CallInst *createSAddOverflow(Value *a, Value *b) {
    auto sadd_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::sadd_with_overflow, a->getType());
    return CallInst::Create(sadd_decl, {a, b}, nextName(), LLVMBB);
  }

  CallInst *createUSubOverflow(Value *a, Value *b) {
    auto usub_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::usub_with_overflow, a->getType());
    return CallInst::Create(usub_decl, {a, b}, nextName(), LLVMBB);
  }

  CallInst *createUAddOverflow(Value *a, Value *b) {
    auto uadd_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::uadd_with_overflow, a->getType());
    return CallInst::Create(uadd_decl, {a, b}, nextName(), LLVMBB);
  }

  ExtractValueInst *createExtractValue(Value *v, ArrayRef<unsigned> idxs) {
    return ExtractValueInst::Create(v, idxs, nextName(), LLVMBB);
  }

  ReturnInst *createReturn(Value *v) {
    return ReturnInst::Create(Ctx, v, LLVMBB);
  }

  CallInst *createFShr(Value *a, Value *b, Value *c) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::fshr, a->getType());
    return CallInst::Create(decl, {a, b, c}, nextName(), LLVMBB);
  }

  CallInst *createFShl(Value *a, Value *b, Value *c) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::fshl, a->getType());
    return CallInst::Create(decl, {a, b, c}, nextName(), LLVMBB);
  }

  CallInst *createBitReverse(Value *v) {
    auto *decl = Intrinsic::getDeclaration(LiftedModule, Intrinsic::bitreverse,
                                           v->getType());
    return CallInst::Create(decl, {v}, nextName(), LLVMBB);
  }

  CallInst *createCtlz(Value *v) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::ctlz, v->getType());
    return CallInst::Create(decl, {v, getIntConst(0, 1)}, nextName(), LLVMBB);
  }

  CallInst *createBSwap(Value *v) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::bswap, v->getType());
    return CallInst::Create(decl, {v}, nextName(), LLVMBB);
  }

  SelectInst *createSelect(Value *cond, Value *a, Value *b) {
    return SelectInst::Create(cond, a, b, nextName(), LLVMBB);
  }

  ICmpInst *createICmp(ICmpInst::Predicate p, Value *a, Value *b) {
    return new ICmpInst(*LLVMBB, p, a, b, nextName());
  }

  BinaryOperator *createBinop(Value *a, Value *b, Instruction::BinaryOps op) {
    return BinaryOperator::Create(op, a, b, nextName(), LLVMBB);
  }

  BinaryOperator *createUDiv(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::UDiv, a, b, nextName(), LLVMBB);
  }

  BinaryOperator *createSDiv(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::SDiv, a, b, nextName(), LLVMBB);
  }

  BinaryOperator *createMul(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Mul, a, b, nextName(), LLVMBB);
  }

  BinaryOperator *createAdd(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Add, a, b, nextName(), LLVMBB);
  }

  BinaryOperator *createSub(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Sub, a, b, nextName(), LLVMBB);
  }

  Value *createRawLShr(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::LShr, a, b, nextName(), LLVMBB);
  }

  Value *createLShr(Value *a, Value *b) {
    auto W = b->getType()->getIntegerBitWidth();
    auto mask = getIntConst(W - 1, W);
    auto masked = BinaryOperator::Create(Instruction::And, mask, b, nextName(), LLVMBB);
    return BinaryOperator::Create(Instruction::LShr, a, masked, nextName(), LLVMBB);
  }

  Value *createRawAShr(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::AShr, a, b, nextName(), LLVMBB);
  }

  Value *createAShr(Value *a, Value *b) {
    auto W = b->getType()->getIntegerBitWidth();
    auto mask = getIntConst(W - 1, W);
    auto masked = BinaryOperator::Create(Instruction::And, mask, b, nextName(), LLVMBB);
    return BinaryOperator::Create(Instruction::AShr, a, masked, nextName(), LLVMBB);
  }

  Value *createRawShl(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Shl, a, b, nextName(), LLVMBB);
  }

  Value *createShl(Value *a, Value *b) {
    auto W = b->getType()->getIntegerBitWidth();
    auto mask = getIntConst(W - 1, W);
    auto masked = BinaryOperator::Create(Instruction::And, mask, b, nextName(), LLVMBB);
    return BinaryOperator::Create(Instruction::Shl, a, masked, nextName(), LLVMBB);
  }

  BinaryOperator *createAnd(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::And, a, b, nextName(), LLVMBB);
  }

  BinaryOperator *createOr(Value *a, Value *b, const string &NameStr = "") {
    return BinaryOperator::Create(
        Instruction::Or, a, b, (NameStr == "") ? nextName() : NameStr, LLVMBB);
  }

  BinaryOperator *createXor(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Xor, a, b, nextName(), LLVMBB);
  }

  BinaryOperator *createLogicalNot(Value *a) {
    auto *ty = a->getType();
    auto NegOne = ConstantInt::getSigned(ty, -1);
    return BinaryOperator::Create(Instruction::Xor, a, NegOne, nextName(),
                                  LLVMBB);
  }

  FreezeInst *createFreeze(Value *v, const string &NameStr = "") {
    return new FreezeInst(v, (NameStr == "") ? nextName() : NameStr, LLVMBB);
  }

  CastInst *createTrunc(Value *v, Type *t, const string &NameStr = "") {
    return CastInst::Create(Instruction::Trunc, v, t,
                            (NameStr == "") ? nextName() : NameStr, LLVMBB);
  }

  // This implements the SMTLIB-like extract operator from Isla traces
  Value *createExtract(Value *v, unsigned n1, unsigned n2,
                       const string &NameStr = "") {
    assert(n1 > n2);
    if (n2 == 0) {
      auto *ty = getIntTy(1 + n1);
      return createTrunc(v, ty);
    } else {
      auto *shift =
          createLShr(v, getIntConst(n2, v->getType()->getIntegerBitWidth()));
      auto *ty = getIntTy(1 + n1 - n2);
      return createTrunc(shift, ty);
    }
  }

  CastInst *createSExt(Value *v, Type *t, const string &NameStr = "") {
    return CastInst::Create(Instruction::SExt, v, t,
                            (NameStr == "") ? nextName() : NameStr, LLVMBB);
  }

  InsertElementInst *createInsertElement(Value *vec, Value *val, Value *index) {
    return InsertElementInst::Create(vec, val, index, nextName(), LLVMBB);
  }

  CastInst *createZExt(Value *v, Type *t, const string &NameStr = "") {
    return CastInst::Create(Instruction::ZExt, v, t,
                            (NameStr == "") ? nextName() : NameStr, LLVMBB);
  }

  CastInst *createCast(Value *v, Type *t, Instruction::CastOps op,
                       const string &NameStr = "") {
    return CastInst::Create(op, v, t, (NameStr == "") ? nextName() : NameStr,
                            LLVMBB);
  }

  [[maybe_unused]] unsigned getRegSize(unsigned Reg) {
    if (Reg >= AArch64::W0 && Reg <= AArch64::W30)
      return 32;
    if (Reg >= AArch64::X0 && Reg <= AArch64::X28)
      return 64;
    assert(false && "unhandled register");
  }

  // return pointer to the backing store for a register, doing the
  // necessary de-aliasing
  Value *dealiasReg(unsigned Reg) {
    unsigned WideReg = Reg;
    if (Reg >= AArch64::W0 && Reg <= AArch64::W30)
      WideReg = Reg - AArch64::W0 + AArch64::X0;
    if (Reg == AArch64::WZR)
      WideReg = AArch64::XZR;
    auto RegAddr = RegFile[WideReg];
    assert(RegAddr);
    return RegAddr;
  }

  // always does a full-width read
  Value *readFromRegister(unsigned Reg, const string &NameStr = "") {
    auto RegAddr = dealiasReg(Reg);
    return createLoad(getIntTy(64), RegAddr, NameStr);
  }

  // TODO: make it so that lshr generates code on register lookups
  // some instructions make use of this, and the semantics need to be
  // worked out
  Value *readFromOperand(int idx, int shift = 0) {
    auto op = CurInst->getOperand(idx);
    auto size = getInstSize(CurInst->getOpcode());
    assert(size == 32 || size == 64);
    assert(op.isImm() || op.isReg());

    Value *V = nullptr;
    if (op.isImm()) {
      V = getIntConst(op.getImm(), size);
    } else {
      V = readFromRegister(op.getReg());
      if (size == 32)
        V = createTrunc(V, getIntTy(32));
    }

    if (shift != 0)
      V = reg_shift(V, shift);

    return V;
  }

  // TODO remove
  void add_identifier(Value *v) {}

  void writeToOutputReg(Value *V, bool s = false) {
    auto Reg = CurInst->getOperand(0).getReg();

    // important!
    if (Reg == AArch64::WZR || Reg == AArch64::XZR)
      return;

    auto W = V->getType()->getIntegerBitWidth();
    if (W != 64 && W != 128) {
      size_t regSize = getInstSize(CurInst->getOpcode());
      assert(regSize == 32 || regSize == 64);

      // if the s flag is set, the value is smaller than 32 bits, and
      // the register we are storing it in _is_ 32 bits, we sign
      // extend to 32 bits before zero-extending to 64
      if (s && regSize == 32 && W < 32) {
        V = createSExt(V, getIntTy(32));
        V = createZExt(V, getIntTy(64));
      } else {
        auto op = s ? Instruction::SExt : Instruction::ZExt;
        V = createCast(V, getIntTy(64), op);
      }
    }
    createStore(V, dealiasReg(Reg));
  }

  Value *getV() {
    return createLoad(getIntTy(1), dealiasReg(AArch64::V));
  }

  Value *getZ() {
    return createLoad(getIntTy(1), dealiasReg(AArch64::Z));
  }

  Value *getN() {
    return createLoad(getIntTy(1), dealiasReg(AArch64::N));
  }

  Value *getC() {
    return createLoad(getIntTy(1), dealiasReg(AArch64::C));
  }

  Value *evaluate_condition(uint64_t cond, MCBasicBlock *bb) {
    // cond<0> == '1' && cond != '1111'
    auto invert_bit = (cond & 1) && (cond != 15);

    cond >>= 1;

    auto cur_v = getV();
    auto cur_z = getZ();
    auto cur_n = getN();
    auto cur_c = getC();

    Value *res = nullptr;
    switch (cond) {
    case 0:
      res = cur_z;
      break; // EQ/NE
    case 1:
      res = cur_c;
      break; // CS/CC
    case 2:
      res = cur_n;
      break; // MI/PL
    case 3:
      res = cur_v;
      break; // VS/VC
    case 4: {
      // HI/LS: PSTATE.C == '1' && PSTATE.Z == '0'
      assert(cur_c != nullptr && cur_z != nullptr &&
             "HI/LS requires C and Z bits to be generated");
      // C == 1
      auto c_cond =
          createICmp(ICmpInst::Predicate::ICMP_EQ, cur_c, getIntConst(1, 1));
      // Z == 0
      auto z_cond =
          createICmp(ICmpInst::Predicate::ICMP_EQ, cur_z, getIntConst(0, 1));
      // C == 1 && Z == 0
      res = createAnd(c_cond, z_cond);
      break;
    }
    case 5:
      // GE/LT PSTATE.N == PSTATE.V
      assert(cur_n != nullptr && cur_v != nullptr &&
             "GE/LT requires N and V bits to be generated");
      res = createICmp(ICmpInst::Predicate::ICMP_EQ, cur_n, cur_v);
      break;
    case 6: {
      // GT/LE PSTATE.N == PSTATE.V && PSTATE.Z == 0
      assert(cur_n != nullptr && cur_v != nullptr && cur_z != nullptr &&
             "GT/LE requires N, V and Z bits to be generated");
      auto n_eq_v = createICmp(ICmpInst::Predicate::ICMP_EQ, cur_n, cur_v);
      auto z_cond =
          createICmp(ICmpInst::Predicate::ICMP_EQ, cur_z, getIntConst(0, 1));
      res = createAnd(n_eq_v, z_cond);
      break;
    }
    case 7:
      res = getIntConst(1, 1);
      break;
    default:
      assert(false && "invalid condition code");
      break;
    }

    assert(res != nullptr && "condition code was not generated");

    if (invert_bit)
      res = createXor(res, getIntConst(1, 1));

    return res;
  }

  void setV(Value *V) {
    assert(V->getType()->getIntegerBitWidth() == 1);
    createStore(V, dealiasReg(AArch64::V));
  }

  void setZ(Value *V) {
    assert(V->getType()->getIntegerBitWidth() == 1);
    createStore(V, dealiasReg(AArch64::Z));
  }

  void setN(Value *V) {
    assert(V->getType()->getIntegerBitWidth() == 1);
    createStore(V, dealiasReg(AArch64::N));
  }

  void setC(Value *V) {
    assert(V->getType()->getIntegerBitWidth() == 1);
    createStore(V, dealiasReg(AArch64::C));
  }

  void setZUsingResult(Value *V) {
    auto W = V->getType()->getIntegerBitWidth();
    auto zero = getIntConst(0, W);
    auto z = createICmp(ICmpInst::Predicate::ICMP_EQ, V, zero);
    setZ(z);
  }

  void setNUsingResult(Value *V) {
    auto W = V->getType()->getIntegerBitWidth();
    auto zero = getIntConst(0, W);
    auto n = createICmp(ICmpInst::Predicate::ICMP_SLT, V, zero);
    setN(n);
  }

public:
  arm2llvm_(Module *LiftedModule, MCFunction &MF, Function &srcFn,
            MCInstPrinter *instrPrinter)
      : LiftedModule(LiftedModule), MF(MF), srcFn(srcFn),
        instrPrinter(instrPrinter), instCount(0) {}

  int64_t getImm(int idx) {
    return CurInst->getOperand(idx).getImm();
  }
  
  // Visit an MCInst and convert it to LLVM IR
  void mc_visit(MCInst &I, Function &Fn) {
    auto opcode = I.getOpcode();
    CurInst = &I;

    auto i1 = getIntTy(1);
    auto i8 = getIntTy(8);
    auto i16 = getIntTy(16);
    auto i32 = getIntTy(32);
    auto i64 = getIntTy(64);
    auto i128 = getIntTy(128);

    switch (opcode) {
    case AArch64::MRS: {
      // https://developer.arm.com/documentation/ddi0595/2021-06/AArch64-Registers/NZCV--Condition-Flags
      auto imm = getImm(1);
      assert(imm == 55824 && "NZCV is the only supported case for MRS");
      
      auto N = createZExt(getN(), i64);
      auto Z = createZExt(getZ(), i64);
      auto C = createZExt(getC(), i64);
      auto V = createZExt(getV(), i64);

      auto NS = createShl(N, getIntConst(31, 64));
      auto NZ = createShl(Z, getIntConst(30, 64));
      auto NC = createShl(C, getIntConst(29, 64));
      auto NV = createShl(V, getIntConst(28, 64));

      Value *res = getIntConst(0, 64);
      res = createOr(res, NS);
      res = createOr(res, NZ);
      res = createOr(res, NC);
      res = createOr(res, NV);
      writeToOutputReg(res);
      break;
    }
    case AArch64::MSR: {
      // https://developer.arm.com/documentation/ddi0595/2021-06/AArch64-Registers/NZCV--Condition-Flags
      auto imm = getImm(0);
      assert(imm == 55824 && "NZCV is the only supported case for MSR");

      auto i64_0 = getIntConst(0, 64);
      auto i64_1 = getIntConst(1, 64);

      auto Nmask = createShl(i64_1, getIntConst(31, 64));
      auto Zmask = createShl(i64_1, getIntConst(30, 64));
      auto Cmask = createShl(i64_1, getIntConst(29, 64));
      auto Vmask = createShl(i64_1, getIntConst(28, 64));
      
      auto reg = readFromOperand(1);
      auto Nval = createAnd(Nmask, reg);
      auto Zval = createAnd(Zmask, reg);
      auto Cval = createAnd(Cmask, reg);
      auto Vval = createAnd(Vmask, reg);

      setN(createICmp(ICmpInst::Predicate::ICMP_NE, Nval, i64_0));
      setZ(createICmp(ICmpInst::Predicate::ICMP_NE, Zval, i64_0));
      setC(createICmp(ICmpInst::Predicate::ICMP_NE, Cval, i64_0));
      setV(createICmp(ICmpInst::Predicate::ICMP_NE, Vval, i64_0));
      break;
    }
    case AArch64::ADDWrs:
    case AArch64::ADDWri:
    case AArch64::ADDWrx:
    case AArch64::ADDSWrs:
    case AArch64::ADDSWri:
    case AArch64::ADDSWrx:
    case AArch64::ADDXrs:
    case AArch64::ADDXri:
    case AArch64::ADDXrx:
    case AArch64::ADDSXrs:
    case AArch64::ADDSXri:
    case AArch64::ADDSXrx: {
      auto a = readFromOperand(1);
      Value *b = nullptr;

      switch (opcode) {
      case AArch64::ADDWrx:
      case AArch64::ADDSWrx:
      case AArch64::ADDXrx:
      case AArch64::ADDSXrx: {
        auto size = getInstSize(opcode);
        auto ty = getIntTy(size);
        auto extendImm = getImm(3);
        auto extendType = ((extendImm >> 3) & 0x7);
        auto isSigned = extendType / 4;

        // extendSize is necessary so that we can start with the word size
        // ARM wants us to (byte, half, full) and then sign extend to a new
        // size. Without extendSize being used for a trunc, a lot of masking
        // and more manual work to sign extend would be necessary
        unsigned extendSize = 8 << (extendType % 4);
        auto shift = extendImm & 0x7;

        b = readFromOperand(2);

        // Make sure to not to trunc to the same size as the parameter.
        // Sometimes ADDrx is generated using 32 bit registers and "extends" to
        // a 32 bit value. This is seen as a type error by LLVM, but is valid
        // ARM
        if (extendSize != (unsigned)size) {
          auto truncType = getIntTy(extendSize);
          b = createTrunc(b, truncType);
          b = createCast(b, ty,
                         isSigned ? Instruction::SExt : Instruction::ZExt);
        }

        // shift may not be there, it may just be the extend
        if (shift != 0)
          b = createShl(b, getIntConst(shift, size));
        break;
      }
      default:
        b = readFromOperand(2, getImm(3));
        break;
      }

      if (has_s(opcode)) {
        auto sadd = createSAddOverflow(a, b);
        auto result = createExtractValue(sadd, {0});
        auto new_v = createExtractValue(sadd, {1});

        auto uadd = createUAddOverflow(a, b);
        auto new_c = createExtractValue(uadd, {1});

        setV(new_v);
        setC(new_c);
        setNUsingResult(result);
        setZUsingResult(result);
        writeToOutputReg(result);
      }

      writeToOutputReg(createAdd(a, b));
      break;
    }
    case AArch64::ADCXr:
    case AArch64::ADCWr:
    case AArch64::ADCSXr:
    case AArch64::ADCSWr: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);

      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      // Deal with size+1 bit integers so that we can easily calculate the c/v
      // PSTATE bits.
      auto tyPlusOne = getIntTy(size + 1);

      auto carry = createZExt(getC(), tyPlusOne);
      auto add = createAdd(createZExt(a, tyPlusOne), createZExt(b, tyPlusOne));
      auto withCarry = createAdd(add, carry);

      writeToOutputReg(createTrunc(withCarry, ty));

      if (has_s(opcode)) {
        // Mask off the sign bit. We could mask with an AND, but APInt semantics
        // might be weird since we're passing in an uint64_t but we'll want a 65
        // bit int
	auto tmp = createRawShl(withCarry, getIntConst(1, size + 1));
        auto masked = createRawLShr(tmp, getIntConst(1, size + 1));

        auto sAdd =
            createAdd(createSExt(a, tyPlusOne), createSExt(b, tyPlusOne));
        auto sWithCarry = createAdd(sAdd, carry);

        setNUsingResult(withCarry);
        setZUsingResult(withCarry);
        setC(createICmp(ICmpInst::Predicate::ICMP_NE, withCarry, masked));
        setV(createICmp(ICmpInst::Predicate::ICMP_NE, sWithCarry, masked));
      }

      break;
    }
    case AArch64::ASRVWr:
    case AArch64::ASRVXr: {
      auto size = getInstSize(opcode);
      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      auto shift_amt =
          createBinop(b, getIntConst(size, size), Instruction::URem);
      auto res = createAShr(a, shift_amt);
      writeToOutputReg(res);
      break;
    }
      // SUBrx is a subtract instruction with an extended register.
      // ARM has 8 types of extensions:
      // 000 -> uxtb
      // 001 -> uxth
      // 010 -> uxtw
      // 011 -> uxtx
      // 100 -> sxtb
      // 110 -> sxth
      // 101 -> sxtw
      // 111 -> sxtx
      // To figure out if the extension is signed, we can use (extendType / 4)
      // Since the types repeat byte, half word, word, etc. for signed and
      // unsigned extensions, we can use 8 << (extendType % 4) to calculate
      // the extension's byte size
    case AArch64::SUBWri:
    case AArch64::SUBWrs:
    case AArch64::SUBWrx:
    case AArch64::SUBSWrs:
    case AArch64::SUBSWri:
    case AArch64::SUBSWrx:
    case AArch64::SUBXri:
    case AArch64::SUBXrs:
    case AArch64::SUBXrx:
    case AArch64::SUBSXrs:
    case AArch64::SUBSXri:
    case AArch64::SUBSXrx: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      assert(CurInst->getNumOperands() == 4); // dst, lhs, rhs, shift amt
      assert(CurInst->getOperand(3).isImm());

      // convert lhs, rhs operands to IR::Values
      auto a = readFromOperand(1);
      Value *b = nullptr;
      switch (opcode) {
      case AArch64::SUBWrx:
      case AArch64::SUBSWrx:
      case AArch64::SUBXrx:
      case AArch64::SUBSXrx: {
        auto extendImm = getImm(3);
        auto extendType = (extendImm >> 3) & 0x7;
        auto isSigned = extendType / 4;
        // extendSize is necessary so that we can start with the word size
        // ARM wants us to (byte, half, full) and then sign extend to a new
        // size. Without extendSize being used for a trunc, a lot of masking
        // and more manual work to sign extend would be necessary
        unsigned extendSize = 8 << (extendType % 4);
        auto shift = extendImm & 0x7;
        b = readFromOperand(2);

        // Make sure to not to trunc to the same size as the parameter.
        // Sometimes SUBrx is generated using 32 bit registers and "extends" to
        // a 32 bit value. This is seen as a type error by LLVM, but is valid
        // ARM
        if (extendSize != ty->getIntegerBitWidth()) {
          auto truncType = getIntTy(extendSize);
          b = createTrunc(b, truncType);
          b = createCast(b, ty,
                         isSigned ? Instruction::SExt : Instruction::ZExt);
        }

        // shift may not be there, it may just be the extend
        if (shift != 0)
          b = createShl(b, getIntConst(shift, size));
        break;
      }
      default:
        b = readFromOperand(2, getImm(3));
      }

      // make sure that lhs and rhs conversion succeeded, type lookup succeeded
      if (!ty || !a || !b)
        visitError(I);

      if (has_s(opcode)) {
        auto ssub = createSSubOverflow(a, b);
        auto result = createExtractValue(ssub, {0});
        auto new_v = createExtractValue(ssub, {1});
        setC(createICmp(ICmpInst::Predicate::ICMP_UGE, a, b));
        setZ(createICmp(ICmpInst::Predicate::ICMP_EQ, a, b));
        setV(new_v);
        setNUsingResult(result);
        writeToOutputReg(result);
      } else {
        auto sub = createSub(a, b);
        writeToOutputReg(sub);
      }
      break;
    }
    case AArch64::CSELWr:
    case AArch64::CSELXr: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      assert(CurInst->getNumOperands() == 4); // dst, lhs, rhs, cond
      // TODO decode condition and find the approprate cond val
      assert(CurInst->getOperand(1).isReg() && CurInst->getOperand(2).isReg());
      assert(CurInst->getOperand(3).isImm());

      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      auto cond_val_imm = getImm(3);
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      if (!ty || !a || !b)
        visitError(I);

      auto result = createSelect(cond_val, a, b);
      writeToOutputReg(result);
      break;
    }
    case AArch64::ANDWri:
    case AArch64::ANDWrr:
    case AArch64::ANDWrs:
    case AArch64::ANDSWri:
    case AArch64::ANDSWrr:
    case AArch64::ANDSWrs:
    case AArch64::ANDXri:
    case AArch64::ANDXrr:
    case AArch64::ANDXrs:
    case AArch64::ANDSXri:
    case AArch64::ANDSXrr:
    case AArch64::ANDSXrs: {
      auto size = getInstSize(opcode);
      Value *rhs = nullptr;
      if (CurInst->getOperand(2).isImm()) {
        auto imm =
            decodeLogicalImmediate(getImm(2), size);
        rhs = getIntConst(imm, size);
      } else {
        rhs = readFromOperand(2);
      }

      // We are in a ANDrs case. We need to handle a shift
      if (CurInst->getNumOperands() == 4) {
        // the 4th operand (if it exists) must be an immediate
        assert(CurInst->getOperand(3).isImm());
        rhs = reg_shift(rhs, getImm(3));
      }

      auto and_op = createAnd(readFromOperand(1), rhs);

      if (has_s(opcode)) {
        setNUsingResult(and_op);
        setZUsingResult(and_op);
        setC(getIntConst(0, 1));
        setV(getIntConst(0, 1));
      }

      writeToOutputReg(and_op);
      break;
    }
    case AArch64::MADDWrrr:
    case AArch64::MADDXrrr: {
      auto mul_lhs = readFromOperand(1);
      auto mul_rhs = readFromOperand(2);
      auto addend = readFromOperand(3);

      auto mul = createMul(mul_lhs, mul_rhs);
      auto add = createAdd(mul, addend);
      writeToOutputReg(add);
      break;
    }
    case AArch64::UMADDLrrr: {
      auto size = getInstSize(opcode);
      auto mul_lhs = readFromOperand(1);
      auto mul_rhs = readFromOperand(2);
      auto addend = readFromOperand(3);

      auto lhs_masked = createAnd(mul_lhs, getIntConst(0xffffffffUL, size));
      auto rhs_masked = createAnd(mul_rhs, getIntConst(0xffffffffUL, size));
      auto mul = createMul(lhs_masked, rhs_masked);
      auto add = createAdd(mul, addend);
      writeToOutputReg(add);
      break;
    }
    case AArch64::SMADDLrrr: {
      // Signed Multiply-Add Long multiplies two 32-bit register values,
      // adds a 64-bit register value, and writes the result to the 64-bit
      // destination register.
      auto mul_lhs = readFromOperand(1);
      auto mul_rhs = readFromOperand(2);
      auto addend = readFromOperand(3);

      // The inputs are automatically zero extended, but we want sign extension,
      // so we need to truncate them back to i32s
      auto lhs_trunc = createTrunc(mul_lhs, i32);
      auto rhs_trunc = createTrunc(mul_rhs, i32);

      // For signed multiplication, must sign extend the lhs and rhs to not
      // overflow
      auto lhs_ext = createSExt(lhs_trunc, i64);
      auto rhs_ext = createSExt(rhs_trunc, i64);

      auto mul = createMul(lhs_ext, rhs_ext);
      auto add = createAdd(mul, addend);
      writeToOutputReg(add);
      break;
    }
    case AArch64::SMSUBLrrr:
    case AArch64::UMSUBLrrr: {
      // SMSUBL: Signed Multiply-Subtract Long.
      // UMSUBL: Unsigned Multiply-Subtract Long.
      auto *mul_lhs = readFromOperand(1);
      auto *mul_rhs = readFromOperand(2);
      auto *minuend = readFromOperand(3);

      // The inputs are automatically zero extended, but we want sign
      // extension for signed, so we need to truncate them back to i32s
      auto lhs_trunc = createTrunc(mul_lhs, i32);
      auto rhs_trunc = createTrunc(mul_rhs, i32);

      Value *lhs_extended = nullptr;
      Value *rhs_extended = nullptr;
      if (opcode == AArch64::SMSUBLrrr) {
        // For signed multiplication, must sign extend the lhs and rhs to not
        // overflow
        lhs_extended = createSExt(lhs_trunc, i64);
        rhs_extended = createSExt(rhs_trunc, i64);
      } else {
        lhs_extended = createZExt(lhs_trunc, i64);
        rhs_extended = createZExt(rhs_trunc, i64);
      }

      auto mul = createMul(lhs_extended, rhs_extended);
      auto subtract = createSub(minuend, mul);
      writeToOutputReg(subtract);
      break;
    }
    case AArch64::SMULHrr:
    case AArch64::UMULHrr: {
      // SMULH: Signed Multiply High
      // UMULH: Unsigned Multiply High
      auto mul_lhs = readFromOperand(1);
      auto mul_rhs = readFromOperand(2);

      // For unsigned multiplication, must zero extend the lhs and rhs to not
      // overflow For signed multiplication, must sign extend the lhs and rhs to
      // not overflow
      Value *lhs_extended = nullptr, *rhs_extended = nullptr;
      if (opcode == AArch64::UMULHrr) {
        lhs_extended = createZExt(mul_lhs, i128);
        rhs_extended = createZExt(mul_rhs, i128);
      } else {
        lhs_extended = createSExt(mul_lhs, i128);
        rhs_extended = createSExt(mul_rhs, i128);
      }

      auto mul = createMul(lhs_extended, rhs_extended);
      // After multiplying, shift down 64 bits to get the top half of the i128
      // into the bottom half
      auto shift = createLShr(mul, getIntConst(64, 128));

      // Truncate to the proper size:
      auto trunc = createTrunc(shift, i64);
      writeToOutputReg(trunc);
      break;
    }
    case AArch64::MSUBWrrr:
    case AArch64::MSUBXrrr: {
      auto mul_lhs = readFromOperand(1);
      auto mul_rhs = readFromOperand(2);
      auto minuend = readFromOperand(3);
      auto mul = createMul(mul_lhs, mul_rhs);
      auto sub = createSub(minuend, mul);
      writeToOutputReg(sub);
      break;
    }
    case AArch64::SBFMWri:
    case AArch64::SBFMXri: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      auto src = readFromOperand(1);
      auto immr = getImm(2);
      auto imms = getImm(3);

      auto r = getIntConst(immr, size);
      //      auto s = getIntConst(imms, size);

      // arithmetic shift right (ASR) alias is perferred when:
      // imms == 011111 and size == 32 or when imms == 111111 and size = 64
      if ((size == 32 && imms == 31) || (size == 64 && imms == 63)) {
        auto dst = createAShr(src, r);
        writeToOutputReg(dst);
        return;
      }

      // SXTB
      if (immr == 0 && imms == 7) {
        auto trunc = createTrunc(src, i8);
        auto dst = createSExt(trunc, ty);
        writeToOutputReg(dst);
        return;
      }

      // SXTH
      if (immr == 0 && imms == 15) {
        auto trunc = createTrunc(src, i16);
        auto dst = createSExt(trunc, ty);
        writeToOutputReg(dst);
        return;
      }

      // SXTW
      if (immr == 0 && imms == 31) {
        auto trunc = createTrunc(src, i32);
        auto dst = createSExt(trunc, ty);
        writeToOutputReg(dst);
        return;
      }

      // SBFIZ
      if (imms < immr) {
        auto pos = size - immr;
        auto width = imms + 1;
        auto mask = ((uint64_t)1 << (width)) - 1;
        auto bitfield_mask = (uint64_t)1 << (width - 1);

        auto masked = createAnd(src, getIntConst(mask, size));
        auto bitfield_lsb = createAnd(src, getIntConst(bitfield_mask, size));
        auto insert_ones = createOr(masked, getIntConst(~mask, size));
        auto bitfield_lsb_set = createICmp(ICmpInst::Predicate::ICMP_NE,
                                           bitfield_lsb, getIntConst(0, size));
        auto res = createSelect(bitfield_lsb_set, insert_ones, masked);
        auto shifted_res = createShl(res, getIntConst(pos, size));
        writeToOutputReg(shifted_res);
        return;
      }
      // FIXME: this requires checking if SBFX is preferred.
      // For now, assume this is always SBFX
      auto width = imms + 1;
      auto mask = ((uint64_t)1 << (width)) - 1;
      auto pos = immr;

      auto masked = createAnd(src, getIntConst(mask, size));
      auto l_shifted = createShl(masked, getIntConst(size - width, size));
      auto shifted_res =
          createAShr(l_shifted, getIntConst(size - width + pos, size));
      writeToOutputReg(shifted_res);
      return;
    }
    case AArch64::CCMPWi:
    case AArch64::CCMPWr:
    case AArch64::CCMPXi:
    case AArch64::CCMPXr: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      assert(CurInst->getNumOperands() == 4);

      auto lhs = readFromOperand(0);
      auto imm_rhs = readFromOperand(1);

      if (!ty || !lhs || !imm_rhs)
        visitError(I);

      auto imm_flags = getImm(2);
      auto imm_v_val = getIntConst((imm_flags & 1) ? 1 : 0, 1);
      auto imm_c_val = getIntConst((imm_flags & 2) ? 1 : 0, 1);
      auto imm_z_val = getIntConst((imm_flags & 4) ? 1 : 0, 1);
      auto imm_n_val = getIntConst((imm_flags & 8) ? 1 : 0, 1);

      auto cond_val_imm = getImm(3);
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      auto ssub = createSSubOverflow(lhs, imm_rhs);
      auto result = createExtractValue(ssub, {0});
      auto zero_val = getIntConst(0, result->getType()->getIntegerBitWidth());

      auto new_n = createICmp(ICmpInst::Predicate::ICMP_SLT, result, zero_val);
      auto new_z = createICmp(ICmpInst::Predicate::ICMP_EQ, lhs, imm_rhs);
      auto new_c = createICmp(ICmpInst::Predicate::ICMP_UGE, lhs, imm_rhs);
      auto new_v = createExtractValue(ssub, {1});

      auto new_n_flag = createSelect(cond_val, new_n, imm_n_val);
      auto new_z_flag = createSelect(cond_val, new_z, imm_z_val);
      auto new_c_flag = createSelect(cond_val, new_c, imm_c_val);
      auto new_v_flag = createSelect(cond_val, new_v, imm_v_val);

      setN(new_n_flag);
      setZ(new_z_flag);
      setC(new_c_flag);
      setV(new_v_flag);
      break;
    }
    case AArch64::EORWri:
    case AArch64::EORXri: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      assert(CurInst->getNumOperands() == 3); // dst, src, imm
      assert(CurInst->getOperand(1).isReg() && CurInst->getOperand(2).isImm());

      auto a = readFromOperand(1);
      auto decoded_immediate =
          decodeLogicalImmediate(getImm(2), size);
      auto imm_val = getIntConst(decoded_immediate,
                                 size); // FIXME, need to decode immediate val
      if (!ty || !a || !imm_val)
        visitError(I);

      auto res = createXor(a, imm_val);
      writeToOutputReg(res);
      break;
    }
    case AArch64::EORWrs:
    case AArch64::EORXrs: {
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2, getImm(3));
      auto result = createXor(lhs, rhs);
      writeToOutputReg(result);
      break;
    }
    case AArch64::CSINVWr:
    case AArch64::CSINVXr:
    case AArch64::CSNEGWr:
    case AArch64::CSNEGXr: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      // csinv dst, a, b, cond
      // if (cond) a else ~b
      assert(CurInst->getNumOperands() == 4); // dst, lhs, rhs, cond
      // TODO decode condition and find the approprate cond val
      assert(CurInst->getOperand(1).isReg() && CurInst->getOperand(2).isReg());
      assert(CurInst->getOperand(3).isImm());

      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      auto cond_val_imm = getImm(3);
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      if (!ty || !a || !b)
        visitError(I);

      auto neg_one = getIntConst(-1, size);
      auto inverted_b = createXor(b, neg_one);

      if (opcode == AArch64::CSNEGWr || opcode == AArch64::CSNEGXr) {
        auto negated_b = createAdd(inverted_b, getIntConst(1, size));
        auto ret = createSelect(cond_val, a, negated_b);
        writeToOutputReg(ret);
        break;
      }

      auto ret = createSelect(cond_val, a, inverted_b);
      writeToOutputReg(ret);
      break;
    }
    case AArch64::CSINCWr:
    case AArch64::CSINCXr: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      assert(CurInst->getOperand(1).isReg() && CurInst->getOperand(2).isReg());
      assert(CurInst->getOperand(3).isImm());

      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      auto cond_val_imm = getImm(3);
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      auto inc = createAdd(b, getIntConst(1, ty->getIntegerBitWidth()));
      auto sel = createSelect(cond_val, a, inc);

      writeToOutputReg(sel);
      break;
    }
    case AArch64::MOVZWi:
    case AArch64::MOVZXi: {
      auto size = getInstSize(opcode);
      assert(CurInst->getOperand(0).isReg());
      assert(CurInst->getOperand(1).isImm());
      auto lhs = readFromOperand(1, getImm(2));
      auto rhs = getIntConst(0, size);
      auto ident = createAdd(lhs, rhs);
      writeToOutputReg(ident);
      break;
    }
    case AArch64::MOVNWi:
    case AArch64::MOVNXi: {
      auto size = getInstSize(opcode);
      assert(CurInst->getOperand(0).isReg());
      assert(CurInst->getOperand(1).isImm());
      assert(CurInst->getOperand(2).isImm());

      auto lhs = readFromOperand(1, getImm(2));
      auto neg_one = getIntConst(-1, size);
      auto not_lhs = createXor(lhs, neg_one);

      writeToOutputReg(not_lhs);
      break;
    }
    case AArch64::LSLVWr:
    case AArch64::LSLVXr: {
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2);
      auto exp = createShl(lhs, rhs);
      writeToOutputReg(exp);
      break;
    }
    case AArch64::LSRVWr:
    case AArch64::LSRVXr: {
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2);
      auto exp = createLShr(lhs, rhs);
      writeToOutputReg(exp);
      break;
    }
    case AArch64::ORNWrs:
    case AArch64::ORNXrs: {
      auto size = getInstSize(opcode);
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2, getImm(3));

      auto neg_one = getIntConst(-1, size);
      auto not_rhs = createXor(rhs, neg_one);
      auto ident = createOr(lhs, not_rhs);
      writeToOutputReg(ident);
      break;
    }
    case AArch64::MOVKWi:
    case AArch64::MOVKXi: {
      auto size = getInstSize(opcode);
      auto dest = readFromOperand(1);
      auto lhs = readFromOperand(2, getImm(3));

      uint64_t bitmask;
      auto shift_amt = getImm(3);

      if (opcode == AArch64::MOVKWi) {
        assert(shift_amt == 0 || shift_amt == 16);
        bitmask = (shift_amt == 0) ? 0xffff0000 : 0x0000ffff;
      } else {
        assert(shift_amt == 0 || shift_amt == 16 || shift_amt == 32 ||
               shift_amt == 48);
        bitmask = ~(((uint64_t)0xffff) << shift_amt);
      }

      auto bottom_bits = getIntConst(bitmask, size);
      auto cleared = createAnd(dest, bottom_bits);
      auto ident = createOr(cleared, lhs);
      writeToOutputReg(ident);
      break;
    }
    case AArch64::UBFMWri:
    case AArch64::UBFMXri: {
      auto size = getInstSize(opcode);
      auto src = readFromOperand(1);
      auto immr = getImm(2);
      auto imms = getImm(3);

      // LSL is preferred when imms != 31 and imms + 1 == immr
      if (size == 32 && imms != 31 && imms + 1 == immr) {
        auto dst = createShl(src, getIntConst(31 - imms, size));
        writeToOutputReg(dst);
        return;
      }

      // LSL is preferred when imms != 63 and imms + 1 == immr
      if (size == 64 && imms != 63 && imms + 1 == immr) {
        auto dst = createShl(src, getIntConst(63 - imms, size));
        writeToOutputReg(dst);
        return;
      }

      // LSR is preferred when imms == 31 or 63 (size - 1)
      if (imms == size - 1) {
        auto dst = createLShr(src, getIntConst(immr, size));
        writeToOutputReg(dst);
        return;
      }

      // UBFIZ
      if (imms < immr) {
        auto pos = size - immr;
        auto width = imms + 1;
        auto mask = ((uint64_t)1 << (width)) - 1;
        auto masked = createAnd(src, getIntConst(mask, size));
        auto shifted = createShl(masked, getIntConst(pos, size));
        writeToOutputReg(shifted);
        return;
      }

      // UXTB
      if (immr == 0 && imms == 7) {
        auto mask = ((uint64_t)1 << 8) - 1;
        auto masked = createAnd(src, getIntConst(mask, size));
        writeToOutputReg(masked);
        return;
      }

      // UXTH
      if (immr == 0 && imms == 15) {
        auto mask = ((uint64_t)1 << 16) - 1;
        auto masked = createAnd(src, getIntConst(mask, size));
        writeToOutputReg(masked);
        return;
      }

      // UBFX
      // FIXME: this requires checking if UBFX is preferred.
      // For now, assume this is always UBFX
      // we mask from lsb to lsb + width and then perform a logical shift right
      auto width = imms + 1;
      auto mask = ((uint64_t)1 << (width)) - 1;
      auto pos = immr;

      auto masked = createAnd(src, getIntConst(mask, size));
      auto shifted_res = createLShr(masked, getIntConst(pos, size));
      writeToOutputReg(shifted_res);
      return;
      // assert(false && "UBFX not supported");
    }
    case AArch64::BFMWri:
    case AArch64::BFMXri: {
      auto size = getInstSize(opcode);
      auto dst = readFromOperand(1);
      auto src = readFromOperand(2);

      auto immr = getImm(3);
      auto imms = getImm(4);

      if (imms >= immr) {
        auto bits = (imms - immr + 1);
        auto pos = immr;

        auto mask = (((uint64_t)1 << bits) - 1) << pos;

        auto masked = createAnd(src, getIntConst(mask, size));
        auto shifted = createLShr(masked, getIntConst(pos, size));
        auto cleared =
            createAnd(dst, getIntConst((uint64_t)(-1) << bits, size));
        auto res = createOr(cleared, shifted);
        writeToOutputReg(res);
        return;
      }

      auto bits = imms + 1;
      auto pos = size - immr;

      // This mask deletes `bits` number of bits starting at `pos`.
      // If the mask is for a 32 bit value, it will chop off the top 32 bits of
      // the 64 bit mask to keep the mask to a size of 32 bits
      auto mask =
          ~((((uint64_t)1 << bits) - 1) << pos) & ((uint64_t)-1 >> (64 - size));

      // get `bits` number of bits from the least significant bits
      auto bitfield =
          createAnd(src, getIntConst(~((uint64_t)-1 << bits), size));

      // move the bitfield into position
      auto moved = createShl(bitfield, getIntConst(pos, size));

      // carve out a place for the bitfield
      auto masked = createAnd(dst, getIntConst(mask, size));
      // place the bitfield
      auto res = createOr(masked, moved);
      writeToOutputReg(res);
      return;
    }
    case AArch64::ORRWri:
    case AArch64::ORRXri: {
      auto size = getInstSize(opcode);
      auto lhs = readFromOperand(1);
      auto imm = getImm(2);
      auto decoded = decodeLogicalImmediate(imm, size);
      auto result = createOr(lhs, getIntConst(decoded, size));
      writeToOutputReg(result);
      break;
    }
    case AArch64::ORRWrs:
    case AArch64::ORRXrs: {
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2, getImm(3));
      auto result = createOr(lhs, rhs);
      writeToOutputReg(result);
      break;
    }
    case AArch64::SDIVWr:
    case AArch64::SDIVXr: {
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2);
      auto result = createSDiv(lhs, rhs);
      writeToOutputReg(result);
      break;
    }
    case AArch64::UDIVWr:
    case AArch64::UDIVXr: {
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2);
      auto result = createUDiv(lhs, rhs);
      writeToOutputReg(result);
      break;
    }
    case AArch64::EXTRWrri:
    case AArch64::EXTRXrri: {
      auto op1 = readFromOperand(1);
      auto op2 = readFromOperand(2);
      auto shift = readFromOperand(3);
      auto result = createFShr(op1, op2, shift);
      writeToOutputReg(result);
      break;
    }
    case AArch64::RORVWr:
    case AArch64::RORVXr: {
      auto op = readFromOperand(1);
      auto shift = readFromOperand(2);
      auto result = createFShr(op, op, shift);
      writeToOutputReg(result);
      break;
    }
    case AArch64::RBITWr:
    case AArch64::RBITXr: {
      auto op = readFromOperand(1);
      auto result = createBitReverse(op);
      writeToOutputReg(result);
      break;
    }
    case AArch64::REVWr:
    case AArch64::REVXr:
      writeToOutputReg(createBSwap(readFromOperand(1)));
      break;
    case AArch64::CLZWr:
    case AArch64::CLZXr: {
      auto op = readFromOperand(1);
      auto result = createCtlz(op);
      writeToOutputReg(result);
      break;
    }
    case AArch64::EONWrs:
    case AArch64::EONXrs:
    case AArch64::BICWrs:
    case AArch64::BICXrs:
    case AArch64::BICSWrs:
    case AArch64::BICSXrs: {
      auto size = getInstSize(opcode);
      // BIC:
      // return = op1 AND NOT (optional shift) op2
      // EON:
      // return = op1 XOR NOT (optional shift) op2

      auto op1 = readFromOperand(1);
      auto op2 = readFromOperand(2);

      // If there is a shift to be performed on the second operand
      if (CurInst->getNumOperands() == 4) {
        // the 4th operand (if it exists) must b an immediate
        assert(CurInst->getOperand(3).isImm());
        op2 = reg_shift(op2, getImm(3));
      }

      // Perform NOT
      auto neg_one = getIntConst(-1, size);
      auto inverted_op2 = createXor(op2, neg_one);

      // Perform final Op: AND for BIC, XOR for EON
      Value *ret = nullptr;
      switch (opcode) {
      case AArch64::BICWrs:
      case AArch64::BICXrs:
      case AArch64::BICSXrs:
      case AArch64::BICSWrs:
        ret = createAnd(op1, inverted_op2);
        break;
      case AArch64::EONWrs:
      case AArch64::EONXrs:
        ret = createXor(op1, inverted_op2);
        break;
      default:
        report_fatal_error("missed case");
      }

      // FIXME: it might be better to have EON instruction separate since there
      //    no "S" instructions for EON
      if (has_s(opcode)) {
        setNUsingResult(ret);
        setZUsingResult(ret);
        setC(getIntConst(0, 1));
        setV(getIntConst(0, 1));
      }

      writeToOutputReg(ret);
      break;
    }
    case AArch64::REV16Xr: {
      // REV16Xr: Reverse bytes of 64 bit value in 16-bit half-words.
      auto size = getInstSize(opcode);
      auto val = readFromOperand(1);
      auto first_part = createShl(val, getIntConst(8, size));
      auto first_part_and =
          createAnd(first_part, getIntConst(0xFF00FF00FF00FF00UL, size));
      auto second_part = createLShr(val, getIntConst(8, size));
      auto second_part_and =
          createAnd(second_part, getIntConst(0x00FF00FF00FF00FFUL, size));
      auto combined_val = createOr(first_part_and, second_part_and);
      writeToOutputReg(combined_val);
      break;
    }
    case AArch64::REV16Wr:
    case AArch64::REV32Xr: {
      // REV16Wr: Reverse bytes of 32 bit value in 16-bit half-words.
      // REV32Xr: Reverse bytes of 64 bit value in 32-bit words.
      auto size = getInstSize(opcode);
      auto val = readFromOperand(1);

      // Reversing all of the bytes, then performing a rotation by half the
      // width reverses bytes in 16-bit halfwords for a 32 bit int and reverses
      // bytes in a 32-bit word for a 64 bit int
      auto reverse_val = createBSwap(val);
      auto ret =
          createFShr(reverse_val, reverse_val, getIntConst(size / 2, size));
      writeToOutputReg(ret);
      break;
    }
    // assuming that the source is always an x register
    // This might not be the case but we need to look at the assembly emitter
    case AArch64::FMOVXDr: {
      // zero extended x register to 128 bits
      auto val = readFromOperand(1);
      auto &op_0 = CurInst->getOperand(0);
      auto q_reg_val = cur_vol_regs[MCBB][op_0.getReg()];
      assert(q_reg_val && "volatile register's value not in cache");

      // zero out the bottom 64-bits of the vector register by shifting
      auto q_shift = createLShr(q_reg_val, getIntConst(64, 128));
      auto q_cleared = createShl(q_shift, getIntConst(64, 128));
      auto mov_res = createOr(q_cleared, val);
      writeToOutputReg(mov_res);
      cur_vol_regs[MCBB][op_0.getReg()] = mov_res;
      break;
    }
    // assuming that the source is always an x register
    // This might not be the case but we need to look at the assembly emitter
    case AArch64::INSvi64gpr: {
      auto &op_0 = CurInst->getOperand(0);
      auto &op_1 = CurInst->getOperand(1);
      assert(op_0.isReg() && op_1.isReg());
      assert((op_0.getReg() == op_1.getReg()) &&
             "this form of INSvi64gpr is not supported yet");
      auto op_index = getImm(2);
      auto val = readFromOperand(3);
      auto q_reg_val = cur_vol_regs[MCBB][op_0.getReg()];
      // FIXME make this a utility function that uses index and size
      auto mask = getIntConst(-1, 128);

      if (op_index > 0) {
        mask = createShl(mask, getIntConst(64, 128));
        val = createShl(val, getIntConst(64, 128));
      }

      auto q_cleared = createShl(q_reg_val, mask);
      auto mov_res = createAnd(q_cleared, val);
      writeToOutputReg(mov_res);
      cur_vol_regs[MCBB][op_0.getReg()] = mov_res;
      break;
    }
    case AArch64::LDRXui: {
      auto &op1 = CurInst->getOperand(1);
      auto &op2 = CurInst->getOperand(2);
      assert(op1.isReg());
      assert(op1.getReg() == AArch64::SP &&
             "only loading from stack supported for now!");
      assert(op2.isImm());
      auto offset = op2.getImm(); // FIXME!! decode properly
      *out << "offset = " << offset << "\n";
      // assert(((offset % 8) == 0) && "stack slots must be aligned");
      auto ptr = createGEP(i64, stackMem, {getIntConst(offset, 64)}, "");
      auto loaded = createLoad(i64, ptr);
      writeToOutputReg(loaded);
      break;
    }
    case AArch64::RET: {
      // for now we're assuming that the function returns an integer or void
      // value
      auto retTy = srcFn.getReturnType();
      if (auto *vecRetTy = dyn_cast<VectorType>(retTy)) {
        *out << "returning vector type\n";
        auto elem_bitwidth = vecRetTy->getScalarType()->getIntegerBitWidth();
        auto poison_val = PoisonValue::get(vecRetTy->getScalarType());
        vector<Constant *> vals;
        auto elts = vecRetTy->getElementCount().getKnownMinValue();
        for (unsigned i = 0; i < elts; ++i)
          vals.emplace_back(poison_val);
        Value *vec = ConstantVector::get(vals);

        // Hacky way to identify how the returned vector is mapped to register
        // in the assembly
        // unsigned int largest_vect_register = AArch64::Q0;
        // for (auto &[reg, val] : cur_vol_regs[MCBB]) {
        //  *out << "reg num=" << reg << "\n";
        //  if (reg > largest_vect_register) {
        //    largest_vect_register = reg;
        //  }
        //}
        //
        // *out << "largest vect register=" <<
        // largest_vect_register-AArch64::Q0
        // << "\n";

        auto elem_ret_typ = getIntTy(elem_bitwidth);
        // need to trunc if the element's bitwidth is less than 128
        if (elem_bitwidth < 128) {
          for (unsigned i = 0; i < elts; ++i) {
            auto vect_reg_val = cur_vol_regs[MCBB][AArch64::Q0 + i];
            assert(vect_reg_val && "register value to return cannot be null!");
            auto trunc = createTrunc(vect_reg_val, elem_ret_typ);
            vec = createInsertElement(vec, trunc, getIntConst(i, 32));
          }
        } else {
          assert(false && "we're not handling this case yet");
        }
        createReturn(vec);
      } else {
	Value *retVal = nullptr;
        if (!ret_void) {
          auto *retTyp = srcFn.getReturnType();
          auto retWidth = retTyp->getIntegerBitWidth();
          retVal = readFromRegister(AArch64::X0);

          if (retWidth < retVal->getType()->getIntegerBitWidth())
            retVal = createTrunc(retVal, getIntTy(retWidth));

	  // mask off any don't-care bits
          if (has_ret_attr && (orig_ret_bitwidth < 32)) {
            assert(retWidth >= orig_ret_bitwidth);
            assert(retWidth == 64);
            auto trunc = createTrunc(retVal, i32);
            retVal = createZExt(trunc, i64);
          }
        }
	createReturn(retVal);
      }
      break;
    }
    case AArch64::B: {
      const auto &op = CurInst->getOperand(0);
      if (op.isImm()) {
        // handles the case when we add an entry block with no predecessors
        auto &dst_name = MF.BBs[getImm(0)].getName();
        auto BB = getBBByName(Fn, dst_name);
        createBranch(BB);
        break;
      }

      auto dst_ptr = getBB(Fn, CurInst->getOperand(0));
      createBranch(dst_ptr);
      break;
    }
    case AArch64::Bcc: {
      auto cond_val_imm = getImm(0);
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      auto &jmp_tgt_op = CurInst->getOperand(1);
      assert(jmp_tgt_op.isExpr() && "expected expression");
      assert((jmp_tgt_op.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
             "expected symbol ref as bcc operand");
      const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt_op.getExpr());
      const MCSymbol &Sym = SRE.getSymbol();
      auto *dst_true = getBBByName(Fn, Sym.getName());

      assert(MCBB->getSuccs().size() == 2 && "expected 2 successors");
      const string *dst_false_name;
      for (auto &succ : MCBB->getSuccs()) {
        if (succ->getName() != Sym.getName()) {
          dst_false_name = &succ->getName();
          break;
        }
      }
      auto *dst_false = getBBByName(Fn, *dst_false_name);

      createBranch(cond_val, dst_true, dst_false);
      break;
    }
    case AArch64::CBZW:
    case AArch64::CBZX: {
      auto operand = readFromOperand(0);
      assert(operand != nullptr && "operand is null");
      auto cond_val =
          createICmp(ICmpInst::Predicate::ICMP_EQ, operand,
                     getIntConst(0, operand->getType()->getIntegerBitWidth()));
      auto dst_true = getBB(Fn, CurInst->getOperand(1));
      assert(MCBB->getSuccs().size() == 2 && "expected 2 successors");

      const string *dst_false_name;
      for (auto &succ : MCBB->getSuccs()) {
        if (succ->getName() != dst_true->getName()) {
          dst_false_name = &succ->getName();
          break;
        }
      }
      auto *dst_false = getBBByName(Fn, *dst_false_name);
      createBranch(cond_val, dst_true, dst_false);
      break;
    }
    case AArch64::CBNZW:
    case AArch64::CBNZX: {
      auto operand = readFromOperand(0);
      assert(operand != nullptr && "operand is null");
      auto cond_val =
          createICmp(ICmpInst::Predicate::ICMP_NE, operand,
                     getIntConst(0, operand->getType()->getIntegerBitWidth()));

      auto dst_true = getBB(Fn, CurInst->getOperand(1));
      assert(MCBB->getSuccs().size() == 2 && "expected 2 successors");

      const string *dst_false_name;
      for (auto &succ : MCBB->getSuccs()) {
        if (succ->getName() != dst_true->getName()) {
          dst_false_name = &succ->getName();
          break;
        }
      }
      auto *dst_false = getBBByName(Fn, *dst_false_name);
      createBranch(cond_val, dst_true, dst_false);
      break;
    }
    case AArch64::TBZW:
    case AArch64::TBZX:
    case AArch64::TBNZW:
    case AArch64::TBNZX: {
      auto size = getInstSize(opcode);
      auto operand = readFromOperand(0);
      assert(operand != nullptr && "operand is null");
      auto bit_pos = getImm(1);
      auto shift = createLShr(operand, getIntConst(bit_pos, size));
      auto cond_val = createTrunc(shift, i1);

      auto &jmp_tgt_op = CurInst->getOperand(2);
      assert(jmp_tgt_op.isExpr() && "expected expression");
      assert((jmp_tgt_op.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
             "expected symbol ref as bcc operand");
      const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt_op.getExpr());
      const MCSymbol &Sym =
          SRE.getSymbol(); // FIXME refactor this into a function
      auto *dst_false = getBBByName(Fn, Sym.getName());

      assert(MCBB->getSuccs().size() == 2 && "expected 2 successors");

      const string *dst_true_name;
      for (auto &succ : MCBB->getSuccs()) {
        if (succ->getName() != Sym.getName()) {
          dst_true_name = &succ->getName();
          break;
        }
      }
      auto *dst_true = getBBByName(Fn, *dst_true_name);

      if (opcode == AArch64::TBNZW || opcode == AArch64::TBNZX)
        createBranch(cond_val, dst_false, dst_true);
      else
        createBranch(cond_val, dst_true, dst_false);
      break;
    }
    default:
      *out << funcToString(&Fn);
      *out << "\nError "
                "detected----------partially-lifted-arm-target----------\n";
      visitError(I);
    }
  }

  // create the storage associated with a register -- all of its
  // asm-level aliases will get redirected here
  void createRegStorage(unsigned Reg, unsigned Width, const string &Name) {
    auto A = createAlloca(getIntTy(Width), getIntConst(1, 64), Name);
    auto F = createFreeze(PoisonValue::get(getIntTy(Width)));
    createStore(F, A);
    RegFile[Reg] = A;
  }

  Value *parameterABIRules(Value *V, bool argIsSExt, unsigned origWidth) {
    auto i32 = getIntTy(32);
    auto i64 = getIntTy(64);
    if (origWidth < 64) {
      auto op = argIsSExt ? Instruction::SExt : Instruction::ZExt;
      auto orig_ty = getIntTy(origWidth);
      V = createTrunc(V, orig_ty, nextName());
      if (origWidth == 1) {
	V = createCast(V, i32, op, nextName());
	V = createZExt(V, i64, nextName());
      } else {
	if (origWidth < 32) {
	  V = createCast(V, i32, op, nextName());
	  V = createZExt(V, i64, nextName());
	} else {
	  V = createCast(V, i64, op, nextName());
	}
      }
    }
    return V;
  }
  
  Function *run() {
    auto i64 = getIntTy(64);

    auto Fn =
        Function::Create(srcFn.getFunctionType(), GlobalValue::ExternalLinkage,
                         0, MF.getName(), LiftedModule);
    *out << "function name: '" << MF.getName() << "'"
           << "\n";

    // create LLVM-side basic blocks
    vector<pair<BasicBlock *, MCBasicBlock *>> BBs;
    for (auto &mbb : MF.BBs) {
      auto bb = BasicBlock::Create(Ctx, mbb.getName(), Fn);
      BBs.push_back(make_pair(bb, &mbb));
    }

    // default to adding instructions to the entry block
    LLVMBB = BBs[0].first;

    // allocate storage for the main register file
    for (unsigned Reg = AArch64::X0; Reg <= AArch64::X28; ++Reg) {
      stringstream Name;
      Name << "X" << Reg - AArch64::X0;
      createRegStorage(Reg, 64, Name.str());
    }

    // we model SP partially symbolically; it stores a byte offset
    // into the stack memory block
    createRegStorage(AArch64::SP, 64, "SP");
    createStore(getIntConst(0, 64), RegFile[AArch64::SP]);

    createRegStorage(AArch64::LR, 64, "LR");

    // initializing to zero makes loads from XZR work; stores are
    // handled in writeToOutputReg()
    createRegStorage(AArch64::XZR, 64, "XZR");
    createStore(getIntConst(0, 64), RegFile[AArch64::XZR]);

    // allocate storage for PSTATE
    createRegStorage(AArch64::N, 1, "N");
    createRegStorage(AArch64::Z, 1, "Z");
    createRegStorage(AArch64::C, 1, "C");
    createRegStorage(AArch64::V, 1, "V");

    // allocate storage for the stack; the initialization has to be
    // unrolled in the IR so that Alive can see all of it

    // FIXME let's do this all in terms of bytes, not doublewords

    const int stackSlots = 16;
    stackMem = createAlloca(i64, getIntConst(stackSlots, 64), "stack");
    for (unsigned Idx = 0; Idx < stackSlots; ++Idx) {
      auto F = createFreeze(PoisonValue::get(i64));
      auto G = createGEP(i64, stackMem, {getIntConst(Idx, 64)}, "");
      createStore(F, G);
    }

    // implement the callee side of the ABI; FIXME -- this code only
    // supports integer parameters <= 64 bits and will require
    // significant generalization to handle large parameters, vectors,
    // and FP values
    unsigned argNum = 0;
    for (auto &arg : Fn->args()) {
      Value *frozenArg = createFreeze(&arg, nextName());
      auto *val = parameterABIRules(frozenArg, arg.hasSExtAttr(), orig_input_width[argNum]);
      // first 8 are passed via registers, after that on the stack
      if (argNum < 8) {
        auto Reg = MCOperand::createReg(AArch64::X0 + argNum).getReg();
        createStore(val, RegFile[Reg]);
      } else {
	auto slot = getIntConst(argNum - 8, 64);
	auto addr = createGEP(i64, stackMem, { slot }, "");
	createStore(val, addr);
      }
      argNum++;
    }

    for (auto &[llvm_bb, mc_bb] : BBs) {
      *out << "visiting bb: " << mc_bb->getName() << "\n";
      LLVMBB = llvm_bb;
      MCBB = mc_bb;
      auto &mc_instrs = mc_bb->getInstrs();

      for (auto &mc_instr : mc_instrs) {
        print(mc_instr);
        mc_visit(mc_instr, *Fn);
        ++instCount;
      }

      // machine code falls through but LLVM isn't allowed to
      if (!LLVMBB->getTerminator()) {
        assert(MCBB->getSuccs().size() == 1 &&
               "expected 1 successor for block with no terminator");
        auto *dst = getBBByName(*Fn, MCBB->getSuccs()[0]->getName());
        createBranch(dst);
      }

    }
    return Fn;
  }
};

// Convert an MCFunction to IR::Function
// Adapted from llvm2alive_ in llvm2alive.cpp with some simplifying assumptions
// FIXME for now, we are making a lot of simplifying assumptions like assuming
// types of arguments.
Function *arm2llvm(Module *OrigModule, MCFunction &MF, Function &srcFn,
                   MCInstPrinter *instrPrinter, MCRegisterInfo *registerInfo) {
  return arm2llvm_(OrigModule, MF, srcFn, instrPrinter).run();
}

// We're overriding MCStreamerWrapper to generate an MCFunction
// from the arm assembly. MCStreamerWrapper provides callbacks to handle
// different parts of the assembly file. The main callbacks that we're
// using right now are emitInstruction and emitLabel to access the
// instruction and labels in the arm assembly.
class MCStreamerWrapper final : public MCStreamer {
  enum ASMLine { none = 0, label = 1, non_term_instr = 2, terminator = 3 };

private:
  MCBasicBlock *temp_block{nullptr};
  bool first_label{true};
  unsigned prev_line{0};
  MCInstrAnalysis *IA;
  MCInstPrinter *IP;
  MCRegisterInfo *MRI;

public:
  MCFunction MF;
  unsigned cnt{0};
  using BlockSetTy = SetVector<MCBasicBlock *>;

  MCStreamerWrapper(MCContext &Context, MCInstrAnalysis *_IA,
                    MCInstPrinter *_IP, MCRegisterInfo *_MRI)
      : MCStreamer(Context), IA(_IA), IP(_IP), MRI(_MRI) {
    MF.IA = IA;
    MF.IP = IP;
    MF.MRI = MRI;
  }

  // We only want to intercept the emission of new instructions.
  virtual void emitInstruction(const MCInst &Inst,
                               const MCSubtargetInfo & /* unused */) override {

    assert(prev_line != ASMLine::none);

    if (prev_line == ASMLine::terminator)
      temp_block = MF.addBlock(MF.getLabel());
    MCInst curInst(Inst);
    temp_block->addInst(curInst);

    prev_line =
        IA->isTerminator(Inst) ? ASMLine::terminator : ASMLine::non_term_instr;
    auto num_operands = curInst.getNumOperands();
    for (unsigned i = 0; i < num_operands; ++i) {
      auto op = curInst.getOperand(i);
      if (op.isExpr()) {
        auto expr = op.getExpr();
        if (expr->getKind() == MCExpr::ExprKind::SymbolRef) {
          const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*expr);
          const MCSymbol &Sym = SRE.getSymbol();
          *out << "target label : " << (string)Sym.getName()
                 << ", offset=" << to_string(Sym.getOffset())
                 << '\n'; // FIXME remove when done
        }
      }
    }

    *out << cnt++ << "  : ";
    std::string sss;
    llvm::raw_string_ostream ss(sss);
    Inst.dump_pretty(ss, IP, " ", MRI);
    *out << sss;
    if (IA->isBranch(Inst))
      *out << ": branch ";
    if (IA->isConditionalBranch(Inst))
      *out << ": conditional branch ";
    if (IA->isUnconditionalBranch(Inst))
      *out << ": unconditional branch ";
    if (IA->isTerminator(Inst))
      *out << ": terminator ";
    *out << "\n";
  }

  virtual bool emitSymbolAttribute(MCSymbol *Symbol,
                                   MCSymbolAttr Attribute) override {
    return true;
  }

  virtual void emitCommonSymbol(MCSymbol *Symbol, uint64_t Size,
                                Align ByteAlignment) override {}

  virtual void emitZerofill(MCSection *Section, MCSymbol *Symbol = nullptr,
                            uint64_t Size = 0, Align ByteAlignment = Align(1),
                            SMLoc Loc = SMLoc()) override {}

  virtual void emitLabel(MCSymbol *Symbol, SMLoc Loc) override {
    // Assuming the first label encountered is the function's name
    // Need to figure out if there is a better way to get access to the
    // function's name
    if (first_label) {
      MF.setName(Symbol->getName().str() + "-tgt");
      first_label = false;
    }
    string cur_label = Symbol->getName().str();
    temp_block = MF.addBlock(cur_label);
    prev_line = ASMLine::label;
  }

  string findTargetLabel(MCInst &Inst) {
    auto num_operands = Inst.getNumOperands();
    for (unsigned i = 0; i < num_operands; ++i) {
      auto op = Inst.getOperand(i);
      if (op.isExpr()) {
        auto expr = op.getExpr();
        if (expr->getKind() == MCExpr::ExprKind::SymbolRef) {
          const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*expr);
          const MCSymbol &Sym = SRE.getSymbol();
          return Sym.getName().str();
        }
      }
    }

    assert(false && "Could not find target label in arm branch instruction");
    UNREACHABLE();
  }

  // Make sure that we have an entry label with no predecessors
  void checkEntryBlock() {
    MF.checkEntryBlock();
  }

  // Only call after MF with Basicblocks is constructed to generate the
  // successors for each basic block
  void generateSuccessors() {
    *out << "generating basic block successors" << '\n';
    for (unsigned i = 0; i < MF.BBs.size(); ++i) {
      auto &cur_bb = MF.BBs[i];
      MCBasicBlock *next_bb_ptr = nullptr;
      if (i < MF.BBs.size() - 1)
        next_bb_ptr = &MF.BBs[i + 1];

      if (cur_bb.size() == 0) {
        *out
            << "generateSuccessors, encountered basic block with 0 instructions"
            << '\n';
        continue;
      }
      auto &last_mc_instr = cur_bb.getInstrs().back();
      // handle the special case of adding where we have added a new entry block
      // with no predecessors. This is hacky because I don't know the API to
      // create and MCExpr and have to create a branch with an immediate operand
      // instead
      if (i == 0 && (IA->isUnconditionalBranch(last_mc_instr)) &&
          last_mc_instr.getOperand(0).isImm()) {
        cur_bb.addSucc(next_bb_ptr);
        continue;
      }
      if (IA->isConditionalBranch(last_mc_instr)) {
        string target = findTargetLabel(last_mc_instr);
        auto target_bb = MF.findBlockByName(target);
        cur_bb.addSucc(target_bb);
        if (next_bb_ptr)
          cur_bb.addSucc(next_bb_ptr);
      } else if (IA->isUnconditionalBranch(last_mc_instr)) {
        string target = findTargetLabel(last_mc_instr);
        auto target_bb = MF.findBlockByName(target);
        cur_bb.addSucc(target_bb);
      } else if (IA->isReturn(last_mc_instr)) {
        continue;
      } else if (next_bb_ptr) {
        // add edge to next block
        cur_bb.addSucc(next_bb_ptr);
      }
    }
  }

  // Remove empty basic blocks, including .Lfunc_end
  void removeEmptyBlocks() {
    erase_if(MF.BBs, [](MCBasicBlock b) { return b.size() == 0; });
  }

  void printBlocksMF() {
    *out << "# of Blocks (MF print blocks) = " << MF.BBs.size() << '\n';
    *out << "-------------\n";
    int i = 0;
    for (auto &block : MF.BBs) {
      *out << "block " << i << ", name= " << block.getName() << '\n';
      for (auto &inst : block.getInstrs()) {
	std::string sss;
	llvm::raw_string_ostream ss(sss);
        inst.dump_pretty(ss, IP, " ", MRI);
        *out << sss << '\n';
      }
      i++;
    }
  }
};

} // namespace

namespace lifter {

std::ostream *out;
vector<unsigned> orig_input_width;
unsigned int orig_ret_bitwidth;
bool has_ret_attr;
const Target *Targ;

void reset() {
  static bool initialized = false;

  if (!initialized) {
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmParser();
    LLVMInitializeAArch64AsmPrinter();

    string Error;
    Targ = TargetRegistry::lookupTarget(TripleName, Error);
    if (!Targ) {
      *out << Error;
      exit(-1);
    }

    initialized = true;
  }

  // FIXME this is a pretty error-prone way to reset the state,
  // probably should just encapsulate this in a class
  orig_ret_bitwidth = 64;
  has_ret_attr = false;
  orig_input_width.clear();
  cur_vol_regs.clear();
}

pair<Function *, Function *> liftFunc(Module *OrigModule, Module *LiftedModule,
                                      Function *srcFn,
                                      unique_ptr<MemoryBuffer> MB) {
  llvm::SourceMgr SrcMgr;
  SrcMgr.AddNewSourceBuffer(std::move(MB), llvm::SMLoc());

  unique_ptr<MCInstrInfo> MCII(Targ->createMCInstrInfo());
  assert(MCII && "Unable to create instruction info!");

  Triple TheTriple(TripleName);

  auto MCOptions = mc::InitMCTargetOptionsFromFlags();
  unique_ptr<MCRegisterInfo> MRI(Targ->createMCRegInfo(TripleName));
  assert(MRI && "Unable to create target register info!");

  unique_ptr<MCSubtargetInfo> STI(
      Targ->createMCSubtargetInfo(TripleName, CPU, ""));
  assert(STI && "Unable to create subtarget info!");
  assert(STI->isCPUStringValid(CPU) && "Invalid CPU!");

  unique_ptr<MCAsmInfo> MAI(Targ->createMCAsmInfo(*MRI, TripleName, MCOptions));
  assert(MAI && "Unable to create MC asm info!");
  unique_ptr<MCInstPrinter> IPtemp(
      Targ->createMCInstPrinter(TheTriple, 0, *MAI, *MCII, *MRI));

  auto Ana = make_unique<MCInstrAnalysis>(MCII.get());

  MCContext Ctx(TheTriple, MAI.get(), MRI.get(), STI.get());
  MCStreamerWrapper MCSW(Ctx, Ana.get(), IPtemp.get(), MRI.get());

  unique_ptr<MCAsmParser> Parser(createMCAsmParser(SrcMgr, Ctx, MCSW, *MAI));
  assert(Parser);

  MCTargetOptions Opts;
  Opts.PreserveAsmComments = false;
  unique_ptr<MCTargetAsmParser> TAP(
      Targ->createMCAsmParser(*STI, *Parser, *MCII, Opts));
  assert(TAP);
  Parser->setTargetParser(*TAP);
  Parser->Run(true); // ??

  MCSW.printBlocksMF();
  MCSW.removeEmptyBlocks();
  MCSW.checkEntryBlock();
  MCSW.generateSuccessors();
  MCSW.printBlocksMF();

  auto lifted =
      arm2llvm(LiftedModule, MCSW.MF, *srcFn, IPtemp.get(), MRI.get());

  std::string sss;
  llvm::raw_string_ostream ss(sss);
  if (llvm::verifyModule(*LiftedModule, &ss)) {
    *out << sss << "\n\n";
    out->flush();
    llvm::report_fatal_error("Lifted module is broken, this should not happen");
  }

  return make_pair(srcFn, lifted);
}

} // namespace lifter
