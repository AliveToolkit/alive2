// include first to avoid ambiguity for comparison operator from
// util/spaceship.h
#include "llvm/MC/MCAsmInfo.h"

#include "backend_tv/lifter.h"
#include "util/sort.h"

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
    // ADDSW
    AArch64::ADDSWri,
    AArch64::ADDSWrs,
    AArch64::ADDSWrx,
    // ADDSX
    AArch64::ADDSXri,
    AArch64::ADDSXrs,
    AArch64::ADDSXrx,
    // SUBSW
    AArch64::SUBSWri,
    AArch64::SUBSWrs,
    AArch64::SUBSWrx,
    // SUBSX
    AArch64::SUBSXri,
    AArch64::SUBSXrs,
    AArch64::SUBSXrx,
    // ANDSW
    AArch64::ANDSWri,
    AArch64::ANDSWrr,
    AArch64::ANDSWrs,
    // ANDSX
    AArch64::ANDSXri,
    AArch64::ANDSXrr,
    AArch64::ANDSXrs,
    // BICS
    AArch64::BICSWrs,
    AArch64::BICSXrs,
    // ADCS
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
    AArch64::CCMPXi,    AArch64::LDRXui};

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
  outs() << "searching for BB named '" << (string)name << "'\n";
  BasicBlock *BB = nullptr;
  for (auto &bb : Fn) {
    outs() << "  we have a BB named '" << (string)bb.getName() << "'\n";
    if (bb.getName() == name) {
      BB = &bb;
      break;
    }
  }
  assert(BB && "BB not found");
  return BB;
}

struct MCOperandEqual {
  enum Kind { reg = (1 << 2) - 1, immedidate = (1 << 3) - 1 };
  bool operator()(const MCOperand &lhs, const MCOperand &rhs) const {
    return ((lhs.isReg() && rhs.isReg() && (lhs.getReg() == rhs.getReg())) ||
            (lhs.isImm() && rhs.isImm() && (lhs.getImm() == rhs.getImm())) ||
            (lhs.isExpr() && rhs.isExpr() && (lhs.getExpr() == rhs.getExpr())));
  }
};

struct MCOperandHash {
  enum Kind {
    reg = (1 << 2) - 1,
    immedidate = (1 << 3) - 1,
    symbol = (1 << 4) - 1
  };
  size_t operator()(const MCOperand &op) const;
};

size_t MCOperandHash::operator()(const MCOperand &op) const {
  unsigned prefix;
  unsigned id;

  if (op.isReg()) {
    prefix = Kind::reg;
    id = op.getReg();
  } else if (op.isImm()) {
    prefix = Kind::immedidate;
    id = op.getImm();
  } else if (op.isExpr()) {
    prefix = Kind::symbol;
    auto expr = op.getExpr();
    if (expr->getKind() == MCExpr::ExprKind::SymbolRef) {
      const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*expr);
      const MCSymbol &Sym = SRE.getSymbol();
      outs() << "label : " << Sym.getName() << '\n'; // FIXME remove when done
      id = Sym.getOffset();
    } else {
      assert("unsupported mcExpr" && false);
    }
  } else {
    assert("no" && false);
  }

  return hash<unsigned long>()(prefix * id);
}

// do not delete this line
mc::RegisterMCTargetOptionsFlags MOF;

class MCInstWrapper {
private:
  MCInst instr;
  vector<unsigned> op_ids;
  map<unsigned, string>
      phi_blocks; // This is pretty wasteful but I'm not sure how to add
                  // MCExpr operands to the underlying MCInst phi instructions
public:
  MCInstWrapper(MCInst _instr) : instr(_instr) {
    op_ids.resize(instr.getNumOperands(), 0);
  }

  MCInst &getMCInst() {
    return instr;
  }

  // use to assign ids when adding the arugments to phi-nodes
  void pushOpId(unsigned id) {
    op_ids.push_back(id);
  }

  void setOpId(unsigned index, unsigned id) {
    assert(op_ids.size() > index && "Invalid index");
    op_ids[index] = id;
  }

  unsigned getOpId(unsigned index) {
    return op_ids[index];
  }

  void setOpPhiBlock(unsigned index, const string &block_name) {
    phi_blocks[index] = block_name;
  }

  const string &getOpPhiBlock(unsigned index) const {
    return phi_blocks.at(index);
  }

  unsigned getOpcode() const {
    return instr.getOpcode();
  }

  string findTargetLabel() {
    auto num_operands = instr.getNumOperands();
    for (unsigned i = 0; i < num_operands; ++i) {
      auto op = instr.getOperand(i);
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

  // FIXME: for phi instructions and figure out to use register names rather
  // than numbers
  void print() const {
    outs() << "< MCInstWrapper " << getOpcode() << " ";
    unsigned idx = 0;
    for (auto it = instr.begin(); it != instr.end(); ++it) {
      if (it->isReg()) {
        if (getOpcode() == AArch64::PHI && idx >= 1) {
          outs() << "<Phi arg>:[(" << it->getReg() << "," << op_ids[idx] << "),"
                 << getOpPhiBlock(idx) << "]>";
        } else {
          outs() << "<MCOperand Reg:(" << it->getReg() << ", " << op_ids[idx]
                 << ")>";
        }
      } else if (it->isImm()) {
        outs() << "<MCOperand Imm:" << it->getImm() << ">";
      } else if (it->isExpr()) {
        outs() << "<MCOperand Expr:>"; // FIXME
      } else {
        assert("MCInstWrapper printing an unsupported operand" && false);
      }
      idx++;
    }
    outs() << ">\n";
  }
};

// Represents a basic block of machine instructions
class MCBasicBlock {
private:
  string name;
  using SetTy = SetVector<MCBasicBlock *>;
  vector<MCInstWrapper> Instrs;
  SetTy Succs;
  SetTy Preds;

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

  auto &getPreds() {
    return Preds;
  }

  void addInst(MCInstWrapper &inst) {
    Instrs.push_back(inst);
  }

  void addInstBegin(MCInstWrapper &&inst) {
    Instrs.insert(Instrs.begin(), std::move(inst));
  }

  void addSucc(MCBasicBlock *succ_block) {
    Succs.insert(succ_block);
  }

  void addPred(MCBasicBlock *pred_block) {
    Preds.insert(pred_block);
  }

  auto predBegin() {
    return Preds.begin();
  }

  auto predEnd() {
    return Preds.end();
  }

  auto succBegin() const {
    return Succs.begin();
  }

  auto succEnd() const {
    return Succs.end();
  }

  void print() const {
    for (auto &instr : Instrs) {
      instr.print();
    }
  }
};

// hacky way of returning supported volatile registers
vector<unsigned> volatileRegisters() {
  vector<unsigned> res;
  // integer registers
  for (unsigned int i = AArch64::X0; i <= AArch64::X17; ++i) {
    res.push_back(i);
  }

  // fp registers
  for (unsigned int i = AArch64::Q0; i <= AArch64::Q0; ++i) {
    res.push_back(i);
  }

  return res;
}

// Represents a machine function
class MCFunction {
  string name;
  unsigned label_cnt{0};
  using BlockSetTy = SetVector<MCBasicBlock *>;
  unordered_map<MCBasicBlock *, BlockSetTy> dom;
  unordered_map<MCBasicBlock *, BlockSetTy> dom_frontier;
  unordered_map<MCBasicBlock *, BlockSetTy> dom_tree;

  unordered_map<MCOperand, BlockSetTy, MCOperandHash, MCOperandEqual> defs;
  unordered_map<MCBasicBlock *,
                unordered_set<MCOperand, MCOperandHash, MCOperandEqual>>
      phis; // map from block to variable names that need phi-nodes in those
            // blocks
  unordered_map<MCBasicBlock *,
                unordered_map<MCOperand, vector<pair<unsigned, string>>,
                              MCOperandHash, MCOperandEqual>>
      phi_args;
  vector<MCOperand> fn_args;

public:
  MCInstrAnalysis *IA;
  MCInstPrinter *IP;
  MCRegisterInfo *MRI;
  vector<MCBasicBlock> BBs;
  unordered_map<MCBasicBlock *, BlockSetTy> dom_tree_inv;

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
    return nullptr;
  }

  // Make sure that we have an entry label with no predecessors
  void addEntryBlock() {
    // If we have an empty assembly function, we need to add an entry block with
    // a return instruction
    if (BBs.empty()) {
      auto new_block = addBlock("entry");
      MCInst ret_instr;
      ret_instr.setOpcode(AArch64::RET);
      ret_instr.addOperand(MCOperand::createReg(AArch64::X0));
      new_block->addInstBegin(std::move(ret_instr));
    }

    if (BBs.size() == 1)
      return;

    bool add_entry_block = false;
    const auto &first_block = BBs[0];
    for (unsigned i = 0; i < BBs.size(); ++i) {
      auto &cur_bb = BBs[i];
      auto &last_mc_instr = cur_bb.getInstrs().back();
      if (IA->isConditionalBranch(last_mc_instr.getMCInst()) ||
          IA->isUnconditionalBranch(last_mc_instr.getMCInst())) {
        string target = last_mc_instr.findTargetLabel();
        if (target == first_block.getName()) {
          add_entry_block = true;
          break;
        }
      }
    }

    if (add_entry_block) {
      outs() << "Added arm_tv_entry block\n";
      BBs.emplace(BBs.begin(), "arm_tv_entry");
      MCInst jmp_instr;
      jmp_instr.setOpcode(AArch64::B);
      jmp_instr.addOperand(MCOperand::createImm(1));
      BBs[0].addInstBegin(std::move(jmp_instr));
    }
  }

  void postOrderDFS(MCBasicBlock &curBlock, BlockSetTy &visited,
                    vector<MCBasicBlock *> &postOrder) {
    visited.insert(&curBlock);
    for (auto succ : curBlock.getSuccs()) {
      if (find(visited.begin(), visited.end(), succ) == visited.end())
        postOrderDFS(*succ, visited, postOrder);
    }
    postOrder.push_back(&curBlock);
  }

  vector<MCBasicBlock *> postOrder() {
    vector<MCBasicBlock *> postOrder;
    BlockSetTy visited;
    for (auto &curBlock : BBs) {
      if (visited.count(&curBlock) == 0)
        postOrderDFS(curBlock, visited, postOrder);
    }
    return postOrder;
  }

  // compute the domination relation
  void generateDominator() {
    auto blocks = postOrder();
    reverse(blocks.begin(), blocks.end());
    outs() << "postOrder\n";
    for (auto &curBlock : blocks) {
      outs() << curBlock->getName() << "\n";
      dom[curBlock] = BlockSetTy();
      for (auto &b : blocks)
        dom[curBlock].insert(b);
    }

    outs() << "printing dom before\n";
    printGraph(dom);
    while (true) {
      bool changed = false;
      for (auto &curBlock : blocks) {
        BlockSetTy newDom = intersect(curBlock->getPreds(), dom);
        newDom.insert(curBlock);

        if (newDom != dom[curBlock]) {
          changed = true;
          dom[curBlock] = newDom;
        }
      }
      if (!changed)
        break;
    }
    outs() << "printing dom after\n";
    printGraph(dom);
  }

  void generateDominatorFrontier() {
    auto dominates = invertGraph(dom);
    outs() << "printing dom_inverse\n";
    printGraph(dominates);
    for (auto &[block, domSet] : dom) {
      BlockSetTy dominated_succs;
      dom_frontier[block] = BlockSetTy();
      for (auto &dominated : dominates[block]) {
        auto &temp_succs = dominated->getSuccs();
        for (auto &elem : temp_succs)
          dominated_succs.insert(elem);

        for (auto &b : dominated_succs) {
          if (b == block || dominates[block].count(b) == 0)
            dom_frontier[block].insert(b);
        }
      }
    }
    outs() << "printing dom_frontier\n";
    printGraph(dom_frontier);
    return;
  }

  void generateDomTree() {
    auto dominates = invertGraph(dom);
    outs() << "printing dom_inverse\n";
    printGraph(dominates);
    outs() << "-----------------\n";
    unordered_map<MCBasicBlock *, BlockSetTy> s_dom;
    for (auto &[block, children] : dominates) {
      s_dom[block] = BlockSetTy();
      for (auto &child : children) {
        if (child != block) {
          s_dom[block].insert(child);
        }
      }
    }

    unordered_map<MCBasicBlock *, BlockSetTy> child_dom;

    for (auto &[block, children] : s_dom) {
      child_dom[block] = BlockSetTy();
      for (auto &child : children) {
        for (auto &child_doominates : s_dom[child]) {
          child_dom[block].insert(child_doominates);
        }
      }
    }

    for (auto &[block, children] : s_dom) {
      for (auto &child : children) {
        if (child_dom[block].count(child) == 0) {
          dom_tree[block].insert(child);
        }
      }
    }

    outs() << "printing s_dom\n";
    printGraph(s_dom);
    outs() << "-----------------\n";

    outs() << "printing child_dom\n";
    printGraph(child_dom);
    outs() << "-----------------\n";

    outs() << "printing dom_tree\n";
    printGraph(dom_tree);
    outs() << "-----------------\n";

    dom_tree_inv = invertGraph(dom_tree);
    outs() << "printing dom_tree_inv\n";
    printGraph(dom_tree_inv);
    outs() << "-----------------\n";
  }

  // compute a map from each variable to its defining block
  void findDefiningBlocks() {
    for (auto &block : BBs) {
      for (auto &w_instr : block.getInstrs()) {
        auto &mc_instr = w_instr.getMCInst();
        // need to check for special instructions like ret and branch
        // need to check for special destination operands like WZR

        if (IA->isCall(mc_instr))
          report_fatal_error("Function calls not supported yet");

        if (IA->isReturn(mc_instr) || IA->isBranch(mc_instr)) {
          continue;
        }

        assert(mc_instr.getNumOperands() > 0 && "MCInst with zero operands");

        // CHECK: if there is an ARM instruction that writes to two variables
        auto &dst_operand = mc_instr.getOperand(0);

        assert((dst_operand.isReg() || dst_operand.isImm()) &&
               "unsupported destination operand");

        if (dst_operand.isImm()) {
          outs() << "destination operand is an immediate. printing the "
                    "instruction and skipping it\n";
          w_instr.print();
          continue;
        }

        auto dst_reg = dst_operand.getReg();
        // skip constant registers like WZR
        if (dst_reg == AArch64::WZR || dst_reg == AArch64::XZR)
          continue;

        defs[dst_operand].insert(&block);
      }
    }

    // temp for debugging
    for (auto &[var, blockSet] : defs) {
      outs() << "defs for \n";
      var.print(outs(), MRI);
      outs() << "\n";
      for (auto &block : blockSet) {
        outs() << block->getName() << ",";
      }
      outs() << "\n";
    }
  }

  void findPhis() {
    for (auto &[var, block_set] : defs) {
      vector<MCBasicBlock *> block_list(block_set.begin(), block_set.end());
      for (unsigned i = 0; i < block_list.size(); ++i) {
        // auto& df_blocks = dom_frontier[block_list[i]];
        for (auto block_ptr : dom_frontier[block_list[i]]) {
          if (phis[block_ptr].count(var) == 0) {
            phis[block_ptr].insert(var);

            if (find(block_list.begin(), block_list.end(), block_ptr) ==
                block_list.end()) {
              block_list.push_back(block_ptr);
            }
          }
        }
      }
    }
    // temp for debugging
    outs()
        << "mapping from block name to variable names that require phi nodes "
           "in block\n";
    for (auto &[block, varSet] : phis) {
      outs() << "phis for: " << block->getName() << "\n";
      for (auto &var : varSet) {
        var.print(outs(), MRI);
        outs() << "\n";
      }
      outs() << "-------------\n";
    }
  }

  // FIXME: this is duplicated code. need to refactor
  void findArgs(Function *src_fn) {
    unsigned arg_num = 0;

    for (auto &arg : src_fn->args()) {
      auto *ty = arg.getType();
      if (!ty->isIntegerTy())
        report_fatal_error("Only integer-typed arguments supported for now");
      // FIXME. Do a switch statement to figure out which register to start from
      auto start = ty->getIntegerBitWidth() == 32 ? AArch64::W0 : AArch64::X0;
      auto mcarg = MCOperand::createReg(start + (arg_num++));
      fn_args.push_back(std::move(mcarg));
    }

    // temp for debugging
    outs() << "printing fn_args\n";
    for (auto &arg : fn_args) {
      arg.print(outs(), MRI);
      outs() << "\n";
    }
  }

  // go over 32 bit registers and replace them with the corresponding 64 bit
  // FIXME: this will probably have some uninteded consequences that we need to
  // identify
  void rewriteOperands() {

    // FIXME: this lambda is pretty hacky and brittle
    auto in_range_rewrite = [&](MCOperand &op) {
      if (op.isReg()) {
        if (op.getReg() >= AArch64::W0 &&
            op.getReg() <= AArch64::W28) { // FIXME: Why 28?
          op.setReg(op.getReg() + AArch64::X0 - AArch64::W0);
          // FIXME refactor and also need to deal with vector register aliases
        } else if (op.getReg() >= AArch64::B0 && op.getReg() <= AArch64::B31) {
          op.setReg(op.getReg() + AArch64::Q0 - AArch64::B0);
        } else if (op.getReg() >= AArch64::H0 && op.getReg() <= AArch64::H31) {
          op.setReg(op.getReg() + AArch64::Q0 - AArch64::H0);
        } else if (op.getReg() >= AArch64::S0 && op.getReg() <= AArch64::S31) {
          op.setReg(op.getReg() + AArch64::Q0 - AArch64::S0);
        } else if (op.getReg() >= AArch64::D0 && op.getReg() <= AArch64::D31) {
          op.setReg(op.getReg() + AArch64::Q0 - AArch64::D0);
        } else if (!(op.getReg() >= AArch64::X0 &&
                     op.getReg() <= AArch64::X28) &&
                   !(op.getReg() >= AArch64::Q0 &&
                     op.getReg() <= AArch64::Q31) &&
                   !(op.getReg() <= AArch64::XZR &&
                     op.getReg() >= AArch64::WZR) &&
                   !(op.getReg() == AArch64::NoRegister) &&
                   !(op.getReg() == AArch64::LR) &&
                   !(op.getReg() == AArch64::SP)) {
          // temporarily fix to print the name of unsupported register when
          // encountered
          string buff;
          raw_string_ostream str_stream(buff);
          op.print(str_stream, MRI);
          stringstream error_msg;
          error_msg << "Unsupported registers detected in the Assembly: "
                    << str_stream.str();
          report_fatal_error(error_msg.str().c_str());
        }
      }
    };

    for (auto &fn_arg : fn_args)
      in_range_rewrite(fn_arg);

    for (auto &block : BBs) {
      for (auto &w_instr : block.getInstrs()) {
        auto &mc_instr = w_instr.getMCInst();
        for (unsigned i = 0; i < mc_instr.getNumOperands(); ++i) {
          auto &operand = mc_instr.getOperand(i);
          in_range_rewrite(operand);
        }
      }
    }

    outs() << "printing fn_args after rewrite\n";
    for (auto &arg : fn_args) {
      arg.print(outs(), MRI);
      outs() << "\n";
    }

    outs() << "printing MCInsts after rewriting operands\n";
    printBlocks();
  }

  void ssaRename() {
    unordered_map<MCOperand, vector<unsigned>, MCOperandHash, MCOperandEqual>
        stack;
    unordered_map<MCOperand, unsigned, MCOperandHash, MCOperandEqual> counters;

    outs() << "SSA rename\n";

    // auto printStack = [&](unordered_map<MCOperand,
    // vector<unsigned>,
    //                                          MCOperandHash, MCOperandEqual>
    //                           s) {
    //   for (auto &[var, stack_vec] : s) {
    //     outs() << "stack for ";
    //     var.print(outs(), MRI);
    //     outs() << "\n";
    //     for (auto &stack_item : stack_vec) {
    //       outs() << stack_item << ",";
    //     }
    //     outs() << "\n";
    //   }
    // };

    auto pushFresh = [&](const MCOperand &op) {
      if (counters.find(op) == counters.end()) {
        counters[op] = 2; // Set the stack to 2 to account for input registers
                          // and renaming (freeze + extension)
      }
      auto fresh_id = counters[op]++;
      auto &var_stack = stack[op];
      var_stack.insert(var_stack.begin(), fresh_id);
      return fresh_id;
    };

    function<void(MCBasicBlock *)> rename;
    rename = [&](MCBasicBlock *block) {
      auto old_stack = stack;
      outs() << "renaming block: " << block->getName() << "\n";
      block->print();
      outs() << "----\n";
      for (auto &phi_var : phis[block]) {

        MCInst new_phi_instr;
        new_phi_instr.setOpcode(AArch64::PHI);
        new_phi_instr.addOperand(MCOperand::createReg(phi_var.getReg()));
        new_phi_instr.dump_pretty(outs(), IP, " ", MRI);

        MCInstWrapper new_w_instr(new_phi_instr);
        block->addInstBegin(std::move(new_w_instr));
        auto phi_dst_id = pushFresh(phi_var);
        outs() << "phi_dst_id: " << phi_dst_id << "\n";
        block->getInstrs()[0].setOpId(0, phi_dst_id);
      }
      outs() << "after phis\n";
      block->print();
      outs() << "----\n";

      outs() << "renaming instructions\n";
      for (auto &w_instr : block->getInstrs()) {
        auto &mc_instr = w_instr.getMCInst();

        if (mc_instr.getOpcode() == AArch64::PHI) {
          continue;
        }

        assert(mc_instr.getNumOperands() > 0 && "MCInst with zero operands");

        // nothing to rename
        if (mc_instr.getNumOperands() == 1) {
          continue;
        }

        // mc_instr.dump_pretty(outs(), IP, " ", MRI);
        // outs() << "\n";
        // outs() << "printing stack\n";
        // printStack(stack);
        // outs() << "printing operands\n";
        unsigned i = 1;
        if (instrs_no_write.contains(mc_instr.getOpcode())) {
          outs() << "iterating from first element in rename\n";
          i = 0;
        }

        for (; i < mc_instr.getNumOperands(); ++i) {
          auto &op = mc_instr.getOperand(i);
          if (!op.isReg())
            continue;

          // hacky way of not renaming the element index for ins instruction
          // variants
          if ((i == 1) && (ins_variant.contains(mc_instr.getOpcode())))
            continue;

          if (op.getReg() == AArch64::WZR || op.getReg() == AArch64::XZR)
            continue;

          op.print(outs(), MRI);
          outs() << "\n";

          auto &arg_id = stack[op][0];
          w_instr.setOpId(i, arg_id);
        }
        outs() << "printing operands done\n";
        if (instrs_no_write.contains(mc_instr.getOpcode()))
          continue;

        outs() << "renaming dst\n";
        auto &dst_op = mc_instr.getOperand(0);
        dst_op.print(outs(), MRI);
        auto dst_id = pushFresh(dst_op);
        w_instr.setOpId(0, dst_id);
        outs() << "\n";
      }

      outs() << "renaming phi args in block's successors\n";

      for (auto s_block : block->getSuccs()) {
        outs() << block->getName() << " -> " << s_block->getName() << "\n";

        for (auto &phi_var : phis[s_block]) {
          if (stack.find(phi_var) == stack.end()) {
            phi_var.print(outs(), MRI);
            assert(false && "phi var not in stack");
          }
          assert(stack[phi_var].size() > 0 && "phi var stack empty");

          if (phi_args[s_block].find(phi_var) == phi_args[s_block].end()) {
            phi_args[s_block][phi_var] = vector<pair<unsigned, string>>();
          }
          outs() << "phi_arg[" << s_block->getName() << "][" << phi_var.getReg()
                 << "]=" << stack[phi_var][0] << "\n";
          phi_args[s_block][phi_var].push_back(
              make_pair(stack[phi_var][0], block->getName()));
        }
      }

      for (auto b : dom_tree[block])
        rename(b);

      stack = old_stack;
    };

    auto entry_block_ptr = &(BBs[0]);

    entry_block_ptr->getInstrs()[0].print();

    for (auto &arg : fn_args) {
      stack[arg] = vector<unsigned>();
      pushFresh(arg);
    }

    outs() << "adding volatile registers\n";

    auto v_registers = volatileRegisters();
    for (auto reg_num : v_registers) {
      // for (unsigned int i = AArch64::X0; i <= AArch64::X17; i++) {
      bool found_reg = false;
      for (const auto &arg : fn_args) {
        if (arg.getReg() == reg_num) {
          found_reg = true;
          break;
        }
      }

      if (!found_reg) {
        outs() << "adding volatile: " << reg_num << "\n";
        auto vol_reg = MCOperand::createReg(reg_num);
        stack[vol_reg] = vector<unsigned>();
        pushFresh(vol_reg);
      }
    }

    // add SP to the stack
    auto sp_reg = MCOperand::createReg(AArch64::SP);
    stack[sp_reg] = vector<unsigned>();
    pushFresh(sp_reg);

    rename(entry_block_ptr);
    outs() << "printing MCInsts after renaming operands\n";
    printBlocks();

    outs() << "printing phi args\n";
    for (auto &[block, phi_vars] : phi_args) {
      outs() << "block: " << block->getName() << "\n";
      for (auto &[phi_var, args] : phi_vars) {
        outs() << "phi_var: " << phi_var.getReg() << "\n";
        for (auto arg : args) {
          outs() << arg.first << "-" << arg.second << ", ";
        }
        outs() << "\n";
      }
    }

    outs() << "-----------------\n"; // adding args to phi-nodes
    for (auto &[block, phi_vars] : phi_args) {
      for (auto &w_instr : block->getInstrs()) {
        auto &mc_instr = w_instr.getMCInst();
        if (mc_instr.getOpcode() != AArch64::PHI)
          break;

        auto phi_var = mc_instr.getOperand(0);
        unsigned index = 1;
        outs() << "phi arg size " << phi_args[block][phi_var].size() << "\n";
        for (auto var_id_label_pair : phi_args[block][phi_var]) {
          outs() << "index = " << index
                 << ", var_id = " << var_id_label_pair.first << "\n";
          mc_instr.addOperand(MCOperand::createReg(phi_var.getReg()));
          w_instr.pushOpId(var_id_label_pair.first);
          w_instr.setOpPhiBlock(index, var_id_label_pair.second);
          w_instr.print();
          index++;
        }
      }
    }

    outs() << "printing MCInsts after adding args to phi-nodes\n";
    for (auto &b : BBs) {
      outs() << b.getName() << ":\n";
      b.print();
    }
  }

  // helper function to compute the intersection of predecessor dominator sets
  BlockSetTy intersect(BlockSetTy &preds,
                       unordered_map<MCBasicBlock *, BlockSetTy> &dom) {
    BlockSetTy ret;
    if (preds.size() == 0)
      return ret;
    if (preds.size() == 1)
      return dom[*preds.begin()];
    ret = dom[*preds.begin()];
    auto second = ++preds.begin();
    for (auto it = second; it != preds.end(); ++it) {
      auto &pred_set = dom[*it];
      BlockSetTy new_ret;
      for (auto &b : ret) {
        if (pred_set.count(b) == 1)
          new_ret.insert(b);
      }
      ret = new_ret;
    }
    return ret;
  }

  // helper function to invert a graph
  unordered_map<MCBasicBlock *, BlockSetTy>
  invertGraph(unordered_map<MCBasicBlock *, BlockSetTy> &graph) {
    unordered_map<MCBasicBlock *, BlockSetTy> res;
    for (auto &curBlock : graph) {
      for (auto &succ : curBlock.second)
        res[succ].insert(curBlock.first);
    }
    return res;
  }

  // Debug function to print domination info
  void printGraph(unordered_map<MCBasicBlock *, BlockSetTy> &graph) {
    for (auto &curBlock : graph) {
      outs() << curBlock.first->getName() << ": ";
      for (auto &dst : curBlock.second)
        outs() << dst->getName() << " ";
      outs() << "\n";
    }
  }

  void printBlocks() {
    outs() << "# of Blocks (orig print blocks) = " << BBs.size() << '\n';
    outs() << "-------------\n";
    int i = 0;
    for (auto &block : BBs) {
      outs() << "block " << i << ", name= " << block.getName() << '\n';
      for (auto &inst : block.getInstrs()) {
        inst.getMCInst().dump_pretty(outs(), IP, " ", MRI);
        outs() << '\n';
      }
      i++;
    }
  }
};

// Some variables that we need to maintain as we're performing arm-tv
map<pair<unsigned, unsigned>, Value *> mc_cache;
unordered_map<MCOperand, unique_ptr<StructType *>, MCOperandHash,
              MCOperandEqual>
    overflow_aggregate_types;
vector<unique_ptr<VectorType *>> lifted_vector_types;

// Add IR value to cache
void mc_add_identifier(unsigned reg, unsigned version, Value *v) {
  mc_cache.emplace(make_pair(reg, version), v);
  outs() << "mc_cache: adding " << reg << ", " << version << "\n";
}

Value *mc_get_operand(unsigned reg, unsigned version) {
  outs() << "mc_cache: looking for " << reg << ", " << version;
  if (auto I = mc_cache.find(make_pair(reg, version)); I != mc_cache.end()) {
    outs() << "  found it"
           << "\n";
    return I->second;
  }
  outs() << "  did not find it"
         << "\n";
  return nullptr;
}

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
unordered_map<MCBasicBlock *, unordered_map<unsigned, Value *>> cur_vol_regs;

BasicBlock *getBB(Function &F, MCOperand &jmp_tgt) {
  assert(jmp_tgt.isExpr() && "[getBB] expected expression operand");
  assert((jmp_tgt.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
         "[getBB] expected symbol ref as jump operand");
  const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt.getExpr());
  const MCSymbol &Sym = SRE.getSymbol();
  StringRef name = Sym.getName();
  outs() << "jump target: " << Sym.getName().str() << '\n';
  BasicBlock *BB = nullptr;
  for (auto &bb : F) {
    if (bb.getName() == name) {
      BB = &bb;
      break;
    }
  }
  assert(BB && "basic block not found");
  return BB;
}

class arm2llvm_ {
  Module *LiftedModule{nullptr};
  LLVMContext &LLVMCtx = LiftedModule->getContext();
  MCFunction &MF;
  // const DataLayout &DL;
  Function &srcFn;
  MCBasicBlock *MCBB{nullptr}; // the current machine block
  unsigned blockCount{0};
  BasicBlock *CurrBB{nullptr}; // the current block

  MCInstPrinter *instrPrinter{nullptr};
  MCRegisterInfo *registerInfo{nullptr};
  vector<pair<PHINode *, MCInstWrapper *>> lift_todo_phis;

  MCInstWrapper *wrapper{nullptr};

  unsigned instructionCount;
  unsigned curId;
  bool ret_void{false};

  map<unsigned, Value *> RegFile;

  Type *getIntTy(int bits) {
    // just trying to catch silly errors
    assert(bits > 0 && bits <= 129);
    return Type::getIntNTy(LLVMCtx, bits);
  }

  Value *getIntConst(uint64_t val, int bits) {
    return ConstantInt::get(LLVMCtx, llvm::APInt(bits, val));
  }

  [[noreturn]] void visitError(MCInstWrapper &I) {
    // flush must happen before error is printed to make sure the error
    // comes out nice and pretty when combing the stdout/stderr in scripts
    outs().flush();

    outs() << "ERROR: Unsupported arm instruction: "
           << instrPrinter->getOpcodeName(I.getMCInst().getOpcode()) << "\n";
    outs().flush();
    exit(-1); // FIXME handle this better
  }

  int getInstSize(int instr) {
    if (instrs_32.contains(instr))
      return 32;
    if (instrs_64.contains(instr))
      return 64;
    if (instrs_128.contains(instr))
      return 128;
    outs() << "getInstSize encountered unknown instruction"
           << "\n";
    visitError(*wrapper);
    UNREACHABLE();
  }

  // from getShiftType/getShiftValue:
  // https://github.com/llvm/llvm-project/blob/93d1a623cecb6f732db7900baf230a13e6ac6c6a/llvm/lib/Target/AArch64/MCTargetDesc/AArch64AddressingModes.h#L74
  // LLVM encodes its shifts into immediates (ARM doesn't, it has a shift
  // field in the instruction)
  Value *reg_shift(Value *value, int encodedShift) {
    int shift_type = (encodedShift >> 6) & 0x7;
    auto typ = value->getType();

    Instruction::BinaryOps op;
    switch (shift_type) {
    case 0:
      op = Instruction::Shl;
      break;
    case 1:
      op = Instruction::LShr;
      break;
    case 2:
      op = Instruction::AShr;
      break;
    case 3:
      // ROR shift
      return createFShr(
          value, value,
          getIntConst(encodedShift & 0x3f, typ->getIntegerBitWidth()));
    default:
      // FIXME: handle other case (msl)
      report_fatal_error("shift type not supported");
    }
    return createBinop(
        value, getIntConst(encodedShift & 0x3f, typ->getIntegerBitWidth()), op);
  }

  Value *reg_shift(int value, int size, int encodedShift) {
    return reg_shift(getIntConst(value, size), encodedShift);
  }

  // Generates string name for the next instruction
  string next_name() {
    assert(wrapper);
    stringstream ss;
    if (instrs_no_write.contains(wrapper->getOpcode())) {
      ss << "tx" << ++curId << "x" << instructionCount << "x" << blockCount;
    } else {
      ss << registerInfo->getName(wrapper->getMCInst().getOperand(0).getReg())
         << "_" << wrapper->getOpId(0) << "x" << ++curId << "x"
         << instructionCount << "x" << blockCount;
    }
    return ss.str();
  }

  string next_name(unsigned reg_num, unsigned id_num) {
    stringstream ss;
    ss << registerInfo->getName(reg_num) << "_" << id_num;
    return ss.str();
  }

  AllocaInst *createAlloca(Type *ty, Value *sz, const string &NameStr) {
    auto A = new AllocaInst(ty, 0, sz, NameStr, CurrBB);
    // TODO initialize ARM memory cells to freeze poison?
    return A;
  }

  GetElementPtrInst *createGEP(Type *ty, Value *v, ArrayRef<Value *> idxlist,
                               const string &NameStr) {
    return GetElementPtrInst::Create(ty, v, idxlist, NameStr, CurrBB);
  }

  void createBranch(Value *c, BasicBlock *t, BasicBlock *f) {
    BranchInst::Create(t, f, c, CurrBB);
  }

  void createBranch(BasicBlock *dst) {
    BranchInst::Create(dst, CurrBB);
  }

  LoadInst *createLoad(Type *ty, Value *ptr, const string &NameStr = "") {
    return new LoadInst(ty, ptr, (NameStr == "") ? next_name() : NameStr, CurrBB);
  }

  void createStore(Value *v, Value *ptr) {
    new StoreInst(v, ptr, CurrBB);
  }

  CallInst *createSSubOverflow(Value *a, Value *b) {
    auto ssub_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::ssub_with_overflow, a->getType());
    return CallInst::Create(ssub_decl, {a, b}, next_name(), CurrBB);
  }

  CallInst *createSAddOverflow(Value *a, Value *b) {
    auto sadd_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::sadd_with_overflow, a->getType());
    return CallInst::Create(sadd_decl, {a, b}, next_name(), CurrBB);
  }

  CallInst *createUSubOverflow(Value *a, Value *b) {
    auto usub_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::usub_with_overflow, a->getType());
    return CallInst::Create(usub_decl, {a, b}, next_name(), CurrBB);
  }

  CallInst *createUAddOverflow(Value *a, Value *b) {
    auto uadd_decl = Intrinsic::getDeclaration(
        LiftedModule, Intrinsic::uadd_with_overflow, a->getType());
    return CallInst::Create(uadd_decl, {a, b}, next_name(), CurrBB);
  }

  ExtractValueInst *createExtractValue(Value *v, ArrayRef<unsigned> idxs) {
    return ExtractValueInst::Create(v, idxs, next_name(), CurrBB);
  }

  PHINode *createPhi(Type *ty) {
    return PHINode::Create(ty, 0, next_name(), CurrBB);
  }

  ReturnInst *createReturn(Value *v) {
    return ReturnInst::Create(LLVMCtx, v, CurrBB);
  }

  CallInst *createFShr(Value *a, Value *b, Value *c) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::fshr, a->getType());
    return CallInst::Create(decl, {a, b, c}, next_name(), CurrBB);
  }

  CallInst *createFShl(Value *a, Value *b, Value *c) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::fshl, a->getType());
    return CallInst::Create(decl, {a, b, c}, next_name(), CurrBB);
  }

  CallInst *createBitReverse(Value *v) {
    auto *decl = Intrinsic::getDeclaration(LiftedModule, Intrinsic::bitreverse,
                                           v->getType());
    return CallInst::Create(decl, {v}, next_name(), CurrBB);
  }

  CallInst *createCtlz(Value *v) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::ctlz, v->getType());
    return CallInst::Create(decl, {v, getIntConst(0, 1)}, next_name(), CurrBB);
  }

  CallInst *createBSwap(Value *v) {
    auto *decl =
        Intrinsic::getDeclaration(LiftedModule, Intrinsic::bswap, v->getType());
    return CallInst::Create(decl, {v}, next_name(), CurrBB);
  }

  SelectInst *createSelect(Value *cond, Value *a, Value *b) {
    return SelectInst::Create(cond, a, b, next_name(), CurrBB);
  }

  ICmpInst *createICmp(ICmpInst::Predicate p, Value *a, Value *b) {
    return new ICmpInst(*CurrBB, p, a, b, next_name());
  }

  BinaryOperator *createBinop(Value *a, Value *b, Instruction::BinaryOps op) {
    outs() << "a.width = " << a->getType()->getIntegerBitWidth() << "\n";
    outs() << "b.width = " << b->getType()->getIntegerBitWidth() << "\n";
    return BinaryOperator::Create(op, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createUDiv(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::UDiv, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createSDiv(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::SDiv, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createMul(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Mul, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createAdd(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Add, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createSub(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Sub, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createLShr(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::LShr, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createAShr(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::AShr, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createShl(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Shl, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createAnd(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::And, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createOr(Value *a, Value *b, const string &NameStr = "") {
    return BinaryOperator::Create(
        Instruction::Or, a, b, (NameStr == "") ? next_name() : NameStr, CurrBB);
  }

  BinaryOperator *createXor(Value *a, Value *b) {
    return BinaryOperator::Create(Instruction::Xor, a, b, next_name(), CurrBB);
  }

  BinaryOperator *createLogicalNot(Value *a) {
    auto *ty = a->getType();
    auto NegOne = ConstantInt::getSigned(ty, -1);
    return BinaryOperator::Create(Instruction::Xor, a, NegOne, next_name(),
                                  CurrBB);
  }

  FreezeInst *createFreeze(Value *v, const string &NameStr = "") {
    return new FreezeInst(v, (NameStr == "") ? next_name() : NameStr, CurrBB);
  }

  CastInst *createTrunc(Value *v, Type *t, const string &NameStr = "") {
    return CastInst::Create(Instruction::Trunc, v, t,
                            (NameStr == "") ? next_name() : NameStr, CurrBB);
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
                            (NameStr == "") ? next_name() : NameStr, CurrBB);
  }

  InsertElementInst *createInsertElement(Value *vec, Value *val, Value *index) {
    return InsertElementInst::Create(vec, val, index, next_name(), CurrBB);
  }

  CastInst *createZExt(Value *v, Type *t, const string &NameStr = "") {
    return CastInst::Create(Instruction::ZExt, v, t,
                            (NameStr == "") ? next_name() : NameStr, CurrBB);
  }

  CastInst *createCast(Value *v, Type *t, Instruction::CastOps op,
                       const string &NameStr = "") {
    return CastInst::Create(op, v, t, (NameStr == "") ? next_name() : NameStr,
                            CurrBB);
  }

  void add_phi_params(PHINode *phi_instr, MCInstWrapper *phi_mc_wrapper) {
    outs() << "entering add_phi_params"
           << "\n";
    assert(phi_mc_wrapper->getOpcode() == AArch64::PHI &&
           "cannot add params to non-phi instr");
    for (unsigned i = 1; i < phi_mc_wrapper->getMCInst().getNumOperands();
         ++i) {
      assert(phi_mc_wrapper->getMCInst().getOperand(i).isReg());
      outs() << "<Phi arg>:[("
             << phi_mc_wrapper->getMCInst().getOperand(i).getReg() << ","
             << phi_mc_wrapper->getOpId(i) << "),"
             << phi_mc_wrapper->getOpPhiBlock(i) << "]>\n";
      string block_name(phi_mc_wrapper->getOpPhiBlock(i));
      auto val =
          mc_get_operand(phi_mc_wrapper->getMCInst().getOperand(i).getReg(),
                         phi_mc_wrapper->getOpId(i));
      assert(val != nullptr);
      outs() << "block name = " << block_name << "\n";
      phi_instr->addIncoming(
          val, getBBByName(*(phi_instr->getParent()->getParent()), block_name));
      outs() << "i is = " << i << "\n";
    }
    outs() << "exiting add_phi_params"
           << "\n";
  }

  // return pointer to the backing store for a register, doing the
  // necessary de-aliasing
  Value *getRegStorage(unsigned Reg) {
    // FIXME do this better?
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
    auto RegAddr = getRegStorage(Reg);
    return createLoad(getIntTy(64), RegAddr, NameStr);
  }

  // TODO: make it so that lshr generates code on register lookups
  // some instructions make use of this, and the semantics need to be
  // worked out
  Value *readFromOperand(int idx, int shift = 0) {
    auto inst = wrapper->getMCInst();
    auto op = inst.getOperand(idx);
    auto size = getInstSize(inst.getOpcode());
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

  void add_identifier(Value *v) {
    auto reg = wrapper->getMCInst().getOperand(0).getReg();
    auto version = wrapper->getOpId(0);
    // TODO: this probably should be in visit
    instructionCount++;
    mc_add_identifier(reg, version, v);
  }

  void writeToOutputReg(Value *V, bool s = false) {
    auto Reg = wrapper->getMCInst().getOperand(0).getReg();
    cout << "output register = " << Reg << "\n";

    // important!
    if (Reg == AArch64::WZR || Reg == AArch64::XZR) {
      instructionCount++;
      return;
    }

    auto W = V->getType()->getIntegerBitWidth();
    if (W != 64 && W != 128) {
      size_t regSize = getInstSize(wrapper->getMCInst().getOpcode());
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
    add_identifier(V);
    createStore(V, getRegStorage(Reg));
  }

  Value *getV() {
    return createLoad(getIntTy(1), getRegStorage(AArch64::V));
  }

  Value *getZ() {
    return createLoad(getIntTy(1), getRegStorage(AArch64::Z));
  }

  Value *getN() {
    return createLoad(getIntTy(1), getRegStorage(AArch64::N));
  }

  Value *getC() {
    return createLoad(getIntTy(1), getRegStorage(AArch64::C));
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
    createStore(V, getRegStorage(AArch64::V));
  }

  void setZ(Value *V) {
    assert(V->getType()->getIntegerBitWidth() == 1);
    createStore(V, getRegStorage(AArch64::Z));
  }

  void setN(Value *V) {
    assert(V->getType()->getIntegerBitWidth() == 1);
    createStore(V, getRegStorage(AArch64::N));
  }

  void setC(Value *V) {
    assert(V->getType()->getIntegerBitWidth() == 1);
    createStore(V, getRegStorage(AArch64::C));
  }

  void setZUsingResult(Value *V) {
    auto W = V->getType()->getIntegerBitWidth();
    assert(W == 32 || W == 64);
    auto zero = getIntConst(0, W);
    auto z = createICmp(ICmpInst::Predicate::ICMP_EQ, V, zero);
    setZ(z);
  }

  void setNUsingResult(Value *V) {
    auto W = V->getType()->getIntegerBitWidth();
    assert(W == 32 || W == 64);
    auto zero = getIntConst(0, W);
    auto n = createICmp(ICmpInst::Predicate::ICMP_SLT, V, zero);
    setN(n);
  }

public:
  arm2llvm_(Module *LiftedModule, MCFunction &MF, Function &srcFn,
            MCInstPrinter *instrPrinter, MCRegisterInfo *registerInfo)
      : LiftedModule(LiftedModule), MF(MF), srcFn(srcFn),
        instrPrinter(instrPrinter), registerInfo(registerInfo),
        instructionCount(0), curId(0) {}

#if 1
  void revInst(Value *V) {
    writeToOutputReg(createBSwap(V));
  }
#else
#include "code0.cpp"
  void revInst(Value *V) {
    auto w = v->getType()->getIntegerBitWidth();
    assert(w == 32 || w == 64);
    return revInst_0(v);
  }
#endif

  // Visit an MCInstWrapper instructions and convert it to LLVM IR
  void mc_visit(MCInstWrapper &I, Function &Fn) {
    auto opcode = I.getOpcode();
    auto &mc_inst = I.getMCInst();
    wrapper = &I;
    curId = 0;

    auto i1 = getIntTy(1);
    auto i8 = getIntTy(8);
    auto i16 = getIntTy(16);
    auto i32 = getIntTy(32);
    auto i64 = getIntTy(64);
    auto i128 = getIntTy(128);

    switch (opcode) {
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
        auto extendImm = mc_inst.getOperand(3).getImm();
        auto extendType = ((extendImm >> 3) & 0x7);

        outs() << "extendImm: " << extendImm << ", extendType: " << extendType
               << "\n";

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
        b = readFromOperand(2, mc_inst.getOperand(3).getImm());
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

      auto carry = createZExt(getZ(), tyPlusOne);
      auto add = createAdd(createZExt(a, tyPlusOne), createZExt(b, tyPlusOne));
      auto withCarry = createAdd(add, carry);

      writeToOutputReg(createTrunc(withCarry, ty));

      if (has_s(opcode)) {
        // Mask off the sign bit. We could mask with an AND, but APInt semantics
        // might be weird since we're passing in an uint64_t but we'll want a 65
        // bit int
        auto masked = createLShr(createShl(withCarry, getIntConst(1, size + 1)),
                                 getIntConst(1, size + 1));

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
      assert(mc_inst.getNumOperands() == 4); // dst, lhs, rhs, shift amt
      assert(mc_inst.getOperand(3).isImm());

      // convert lhs, rhs operands to IR::Values
      auto a = readFromOperand(1);
      Value *b = nullptr;
      switch (opcode) {
      case AArch64::SUBWrx:
      case AArch64::SUBSWrx:
      case AArch64::SUBXrx:
      case AArch64::SUBSXrx: {
        auto extendImm = mc_inst.getOperand(3).getImm();
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
        b = readFromOperand(2, mc_inst.getOperand(3).getImm());
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
      assert(mc_inst.getNumOperands() == 4); // dst, lhs, rhs, cond
      // TODO decode condition and find the approprate cond val
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isReg());
      assert(mc_inst.getOperand(3).isImm());

      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
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
      if (mc_inst.getOperand(2).isImm()) {
        auto imm = decodeLogicalImmediate(mc_inst.getOperand(2).getImm(), size);
        rhs = getIntConst(imm, size);
      } else {
        rhs = readFromOperand(2);
      }

      // We are in a ANDrs case. We need to handle a shift
      if (mc_inst.getNumOperands() == 4) {
        // the 4th operand (if it exists) must be an immediate
        assert(mc_inst.getOperand(3).isImm());
        rhs = reg_shift(rhs, mc_inst.getOperand(3).getImm());
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
      auto mul_lhs = readFromOperand(1, 0);
      auto mul_rhs = readFromOperand(2, 0);
      auto addend = readFromOperand(3, 0);

      auto mul = createMul(mul_lhs, mul_rhs);
      auto add = createAdd(mul, addend);
      writeToOutputReg(add);
      break;
    }
    case AArch64::UMADDLrrr: {
      auto size = getInstSize(opcode);
      auto mul_lhs = readFromOperand(1, 0);
      auto mul_rhs = readFromOperand(2, 0);
      auto addend = readFromOperand(3, 0);

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
      auto mul_lhs = readFromOperand(1, 0);
      auto mul_rhs = readFromOperand(2, 0);
      auto addend = readFromOperand(3, 0);

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
      auto size = getInstSize(opcode);
      // SMSUBL: Signed Multiply-Subtract Long.
      // UMSUBL: Unsigned Multiply-Subtract Long.
      Value *mul_lhs = nullptr;
      Value *mul_rhs = nullptr;
      if (wrapper->getMCInst().getOperand(1).getReg() == AArch64::WZR) {
        mul_lhs = getIntConst(0, size);
      } else {
        mul_lhs = readFromOperand(1, 0);
      }

      if (wrapper->getMCInst().getOperand(2).getReg() == AArch64::WZR) {
        mul_rhs = getIntConst(0, size);
      } else {
        mul_rhs = readFromOperand(2, 0);
      }

      auto minuend = readFromOperand(3, 0);

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
      auto mul_lhs = readFromOperand(1, 0);
      auto mul_rhs = readFromOperand(2, 0);

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
      auto mul_lhs = readFromOperand(1, 0);
      auto mul_rhs = readFromOperand(2, 0);
      auto minuend = readFromOperand(3, 0);
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
      auto immr = mc_inst.getOperand(2).getImm();
      auto imms = mc_inst.getOperand(3).getImm();

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
      assert(mc_inst.getNumOperands() == 4);

      auto lhs = readFromOperand(0);
      auto imm_rhs = readFromOperand(1);

      if (!ty || !lhs || !imm_rhs)
        visitError(I);

      auto imm_flags = mc_inst.getOperand(2).getImm();
      auto imm_v_val = getIntConst((imm_flags & 1) ? 1 : 0, 1);
      auto imm_c_val = getIntConst((imm_flags & 2) ? 1 : 0, 1);
      auto imm_z_val = getIntConst((imm_flags & 4) ? 1 : 0, 1);
      auto imm_n_val = getIntConst((imm_flags & 8) ? 1 : 0, 1);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
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
      assert(mc_inst.getNumOperands() == 3); // dst, src, imm
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isImm());

      auto a = readFromOperand(1);
      auto decoded_immediate =
          decodeLogicalImmediate(mc_inst.getOperand(2).getImm(), size);
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
      auto rhs = readFromOperand(2, mc_inst.getOperand(3).getImm());
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
      assert(mc_inst.getNumOperands() == 4); // dst, lhs, rhs, cond
      // TODO decode condition and find the approprate cond val
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isReg());
      assert(mc_inst.getOperand(3).isImm());

      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
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
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isReg());
      assert(mc_inst.getOperand(3).isImm());

      auto a = readFromOperand(1);
      auto b = readFromOperand(2);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      auto inc = createAdd(b, getIntConst(1, ty->getIntegerBitWidth()));
      auto sel = createSelect(cond_val, a, inc);

      writeToOutputReg(sel);
      break;
    }
    case AArch64::MOVZWi:
    case AArch64::MOVZXi: {
      auto size = getInstSize(opcode);
      assert(mc_inst.getOperand(0).isReg());
      assert(mc_inst.getOperand(1).isImm());
      auto lhs = readFromOperand(1, mc_inst.getOperand(2).getImm());
      auto rhs = getIntConst(0, size);
      auto ident = createAdd(lhs, rhs);
      writeToOutputReg(ident);
      break;
    }
    case AArch64::MOVNWi:
    case AArch64::MOVNXi: {
      auto size = getInstSize(opcode);
      assert(mc_inst.getOperand(0).isReg());
      assert(mc_inst.getOperand(1).isImm());
      assert(mc_inst.getOperand(2).isImm());

      auto lhs = readFromOperand(1, mc_inst.getOperand(2).getImm());
      auto neg_one = getIntConst(-1, size);
      auto not_lhs = createXor(lhs, neg_one);

      writeToOutputReg(not_lhs);
      break;
    }
    case AArch64::LSLVWr:
    case AArch64::LSLVXr: {
      auto size = getInstSize(opcode);
      auto zero = getIntConst(0, size);
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2);
      auto exp = createFShl(lhs, zero, rhs);
      writeToOutputReg(exp);
      break;
    }
    case AArch64::LSRVWr:
    case AArch64::LSRVXr: {
      auto size = getInstSize(opcode);
      auto zero = getIntConst(0, size);
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2);
      auto exp = createFShr(zero, lhs, rhs);
      writeToOutputReg(exp);
      break;
    }
    case AArch64::ORNWrs:
    case AArch64::ORNXrs: {
      auto size = getInstSize(opcode);
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2, mc_inst.getOperand(3).getImm());

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
      auto lhs = readFromOperand(2, mc_inst.getOperand(3).getImm());

      uint64_t bitmask;
      auto shift_amt = mc_inst.getOperand(3).getImm();

      if (opcode == AArch64::MOVKWi) {
        assert(shift_amt == 0 || shift_amt == 16);
        bitmask = (shift_amt == 0) ? 0xffff0000 : 0xffff;
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
      auto immr = mc_inst.getOperand(2).getImm();
      auto imms = mc_inst.getOperand(3).getImm();

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

      auto immr = mc_inst.getOperand(3).getImm();
      auto imms = mc_inst.getOperand(4).getImm();

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
      auto imm = mc_inst.getOperand(2).getImm();
      auto decoded = decodeLogicalImmediate(imm, size);
      auto result = createOr(lhs, getIntConst(decoded, size));
      writeToOutputReg(result);
      break;
    }
    case AArch64::ORRWrs:
    case AArch64::ORRXrs: {
      auto lhs = readFromOperand(1);
      auto rhs = readFromOperand(2, mc_inst.getOperand(3).getImm());
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
      revInst(readFromOperand(1));
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
      if (mc_inst.getNumOperands() == 4) {
        // the 4th operand (if it exists) must b an immediate
        assert(mc_inst.getOperand(3).isImm());
        op2 = reg_shift(op2, mc_inst.getOperand(3).getImm());
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
        // set n/z, clear c/v
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
      const auto &cur_mc_instr = wrapper->getMCInst();
      auto &op_0 = cur_mc_instr.getOperand(0);
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
      const auto &cur_mc_instr = wrapper->getMCInst();
      auto &op_0 = cur_mc_instr.getOperand(0);
      auto &op_1 = cur_mc_instr.getOperand(1);
      assert(op_0.isReg() && op_1.isReg());
      assert((op_0.getReg() == op_1.getReg()) &&
             "this form of INSvi64gpr is not supported yet");
      auto op_index = cur_mc_instr.getOperand(2).getImm();
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
      const auto &cur_mc_instr = wrapper->getMCInst();
      auto &op_0 = cur_mc_instr.getOperand(0);
      auto &op_1 = cur_mc_instr.getOperand(1);
      assert(op_0.isReg() && op_1.isReg());
      assert(op_1.getReg() == AArch64::SP &&
             "only loading from stack supported for now!");
      [[maybe_unused]] auto bits = readFromRegister(AArch64::SP);
      // FIXME -- this'll work after we allocate a stack
      auto loaded_val = createLoad(i64, nullptr, "foo");
      writeToOutputReg(loaded_val);
      break;
    }
    case AArch64::RET: {
      // for now we're assuming that the function returns an integer or void
      // value
      auto retTy = srcFn.getReturnType();
      if (auto *vecRetTy = dyn_cast<VectorType>(retTy)) {
        outs() << "returning vector type\n";
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
        //  outs() << "reg num=" << reg << "\n";
        //  if (reg > largest_vect_register) {
        //    largest_vect_register = reg;
        //  }
        //}
        //
        // outs() << "largest vect register=" <<
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
        if (ret_void) {
          createReturn(nullptr);
        } else {
          auto *retTyp = srcFn.getReturnType();
          auto retWidth = retTyp->getIntegerBitWidth();
          outs() << "return width = " << retWidth << "\n";
          auto val = readFromRegister(mc_inst.getOperand(0).getReg());

          if (retWidth < val->getType()->getIntegerBitWidth())
            val = createTrunc(val, getIntTy(retWidth));

          // for don't care bits we need to mask them off before returning
          if (has_ret_attr && (orig_ret_bitwidth < 32)) {
            assert(retWidth >= orig_ret_bitwidth);
            assert(retWidth == 64);
            auto trunc = createTrunc(val, i32);
            val = createZExt(trunc, i64);
          }
          createReturn(val);
        }
      }
      break;
    }
    case AArch64::B: {
      const auto &op = mc_inst.getOperand(0);
      if (op.isImm()) {
        // handles the case when we add an entry block with no predecessors
        auto &dst_name = MF.BBs[mc_inst.getOperand(0).getImm()].getName();
        auto BB = getBBByName(Fn, dst_name);
        createBranch(BB);
        break;
      }

      auto dst_ptr = getBB(Fn, mc_inst.getOperand(0));
      createBranch(dst_ptr);
      break;
    }
    case AArch64::Bcc: {
      auto cond_val_imm = mc_inst.getOperand(0).getImm();
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      auto &jmp_tgt_op = mc_inst.getOperand(1);
      assert(jmp_tgt_op.isExpr() && "expected expression");
      assert((jmp_tgt_op.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
             "expected symbol ref as bcc operand");
      const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt_op.getExpr());
      const MCSymbol &Sym = SRE.getSymbol();
      outs() << "bcc target: " << Sym.getName().str() << '\n';
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
      auto dst_true = getBB(Fn, mc_inst.getOperand(1));
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

      auto dst_true = getBB(Fn, mc_inst.getOperand(1));
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
      auto bit_pos = mc_inst.getOperand(1).getImm();
      auto shift = createLShr(operand, getIntConst(bit_pos, size));
      auto cond_val = createTrunc(shift, i1);

      auto &jmp_tgt_op = mc_inst.getOperand(2);
      assert(jmp_tgt_op.isExpr() && "expected expression");
      assert((jmp_tgt_op.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
             "expected symbol ref as bcc operand");
      const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt_op.getExpr());
      const MCSymbol &Sym =
          SRE.getSymbol(); // FIXME refactor this into a function
      auto *dst_false = getBBByName(Fn, Sym.getName());

      outs() << "current mcblock = " << MCBB->getName() << "\n";
      outs() << "Curr BB=" << CurrBB->getName().str() << "\n";
      outs() << "jump target = " << Sym.getName().str() << "\n";
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
    case AArch64::PHI: {
      auto size = getInstSize(opcode);
      auto ty = getIntTy(size);
      auto result = createPhi(ty);
      outs() << "pushing phi in todo : "
             << "\n";
      wrapper->print();
      auto p = make_pair(result, wrapper);
      lift_todo_phis.push_back(p);
      writeToOutputReg(result);
      break;
    }
    default:
      Fn.print(outs());
      outs() << "\nError "
                "detected----------partially-lifted-arm-target----------\n";
      visitError(I);
    }
  }

  void createBBs(vector<pair<BasicBlock *, MCBasicBlock *>> &sorted_bbs,
                 Function &Fn) {
    util::edgesTy edges;
    vector<MCBasicBlock *> bbs;
    unordered_map<MCBasicBlock *, unsigned> bb_map;

    auto bb_num = [&](MCBasicBlock *bb) {
      auto [I, inserted] = bb_map.emplace(bb, bbs.size());
      if (inserted) {
        bbs.emplace_back(bb);
        edges.emplace_back();
      }
      return I->second;
    };

    for (auto &bb : MF.BBs) {
      auto n = bb_num(&bb);
      for (auto it = bb.succBegin(); it != bb.succEnd(); ++it) {
        auto succ_ptr = *it;
        auto n_dst = bb_num(succ_ptr);
        edges[n].emplace(n_dst);
      }
    }

    outs() << "about to start creating basic blocks.\n";
    for (auto v : util::top_sort(edges)) {
      outs() << "creating a basic block called " << bbs[v]->getName() << "\n";
      auto bb = BasicBlock::Create(LLVMCtx, bbs[v]->getName(), &Fn);
      sorted_bbs.emplace_back(bb, bbs[v]);
    }
    outs() << "done creating basic blocks.\n";

    // default to adding instructions to the entry block
    CurrBB = sorted_bbs[0].first;
  }

  // create the storage associated with a register -- all of its
  // asm-level aliases will get redirected here
  void createRegStorage(unsigned Reg, unsigned Width, const string &Name) {
    RegFile[Reg] = createAlloca(getIntTy(Width), getIntConst(1, 64), Name);
  }

  Function *run() {
    auto i32 = getIntTy(32);
    auto i64 = getIntTy(64);
    auto i128 = getIntTy(128);

    auto Fn =
        Function::Create(srcFn.getFunctionType(), GlobalValue::ExternalLinkage,
                         0, MF.getName(), LiftedModule);
    outs() << "function name: '" << MF.getName() << "'"
           << "\n";

    vector<pair<BasicBlock *, MCBasicBlock *>> sorted_bbs;
    createBBs(sorted_bbs, *Fn);

    // allocate storage for the main register file; FIXME not worrying
    // about X29 and X30 -- code that touches them should trip out and
    // we'll fix this later
    for (unsigned Reg = AArch64::X0; Reg <= AArch64::X28; ++Reg) {
      stringstream Name;
      Name << "X" << Reg - AArch64::X0;
      createRegStorage(Reg, 64, Name.str());
    }
    createRegStorage(AArch64::SP, 64, "SP");
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

    // allocate storage for the stack

    // store parameters into registers and stack slots
    unsigned argNum = 0;
    for (auto &Arg : Fn->args()) {
      auto operand = MCOperand::createReg(AArch64::X0 + argNum);
      auto Reg = operand.getReg();
      Value *V = createFreeze(&Arg, next_name(Reg, 1));

      auto orig_width = orig_input_width[argNum];
      // TODO maybe this is in a separate function
      if (orig_width < 64) {
        auto op = Arg.hasSExtAttr() ? Instruction::SExt : Instruction::ZExt;
        auto orig_ty = getIntTy(orig_width);
        V = createTrunc(V, orig_ty, next_name(Reg, 2));
        if (orig_width == 1) {
          V = createCast(V, i32, op, next_name(Reg, 3));
          V = createZExt(V, i64, next_name(Reg, 4));
        } else {
          if (orig_width < 32) {
            V = createCast(V, i32, op, next_name(Reg, 3));
            V = createZExt(V, i64, next_name(Reg, 4));
          } else {
            V = createCast(V, i64, op, next_name(Reg, 4));
          }
        }
      }
      // TODO also into stack slots
      createStore(V, RegFile[Reg]);
      argNum++;
    }

    // TODO vector registers

    argNum = 0;
    for (auto &Arg : Fn->args()) {
      // generate names and values for the input arguments
      // FIXME this is pretty convoluted and needs to be cleaned up
      auto operand = MCOperand::createReg(AArch64::X0 + argNum);

      Value *stored = createFreeze(&Arg, next_name(operand.getReg(), 1));

      mc_add_identifier(operand.getReg(), 1, &Arg);
      assert(Arg.getType()->getIntegerBitWidth() == 64 &&
             "at this point input type should be 64 bits");

      auto orig_width = orig_input_width[argNum];
      if (orig_width < 64) {
        auto op = Instruction::ZExt;
        if (Arg.hasSExtAttr())
          op = Instruction::SExt;

        auto truncated_type = getIntTy(orig_width);
        stored =
            createTrunc(stored, truncated_type, next_name(operand.getReg(), 2));
        if (truncated_type->getIntegerBitWidth() == 1) {
          stored = createCast(stored, i32, op, next_name(operand.getReg(), 3));
          stored = createZExt(stored, i64, next_name(operand.getReg(), 4));
        } else {
          if (truncated_type->getIntegerBitWidth() < 32) {
            stored =
                createCast(stored, i32, op, next_name(operand.getReg(), 3));
            stored = createZExt(stored, i64, next_name(operand.getReg(), 4));
          } else {
            stored =
                createCast(stored, i64, op, next_name(operand.getReg(), 4));
          }
        }
      }
      instructionCount++;
      mc_add_identifier(operand.getReg(), 2, stored);
      argNum++;
    }
    outs() << "created non-vector args"
           << "\n";

    // FIXME: Hacky way of supporting parameters passed via the stack
    // need to properly model the parameter passing rules described in
    // the spec
    if (argNum > 8) {
      auto num_stack_args = argNum - 8; // x0-x7 are passed via registers
      outs() << "num_stack_args = " << num_stack_args << "\n";

      // add stack with 16 slots, 8 bytes each
      auto alloc_size = getIntConst(16, 64);
      auto ty = Type::getInt64Ty(LLVMCtx);
      auto alloca = createAlloca(ty, alloc_size, "stack");
      mc_add_identifier(AArch64::SP, 3, alloca);

      for (unsigned i = 0; i < num_stack_args; ++i) {
        unsigned reg_num = AArch64::X8 + i;
        outs() << "reg_num = " << reg_num << "\n";

        vector<Value *> idxlist{getIntConst(i, 64)};
        auto get_xi =
            createGEP(ty, alloca, idxlist, "stack_" + to_string(8 + i));
        // FIXME, need to use version similar to the offset to address the stack
        // pointer or alternatively a more elegant solution altogether
        mc_add_identifier(AArch64::SP, reg_num, get_xi);
        createStore(readFromRegister(AArch64::SP, "stack_"), get_xi);
      }
    }

    auto poison_val = PoisonValue::get(i64);
    auto vect_poison_val = PoisonValue::get(i128);
    outs() << "argNum = " << argNum << "\n";
    outs() << "entry mc_bb = " << sorted_bbs[0].second->getName() << "\n";
    auto entry_mc_bb = sorted_bbs[0].second;
    // add remaining volatile registers and set them to unitialized value
    // we chose frozen poison value to give a random yet determinate value
    // FIXME: using the number of arguments to the function to determine which
    // registers are uninitialized is hacky and will break when passing FP
    // arguments

    for (unsigned int i = AArch64::X0 + argNum; i <= AArch64::X17; ++i) {
      auto val = createOr(poison_val, getIntConst(0, 64), next_name(i, 3));
      auto val_frozen = createFreeze(val, next_name(i, 4));
      mc_add_identifier(i, 2, val_frozen);
    }

    for (unsigned int i = AArch64::Q0; i <= AArch64::Q3; ++i) {
      auto val =
          createOr(vect_poison_val, getIntConst(0, 128), next_name(i, 3));
      auto val_frozen = createFreeze(val, next_name(i, 4));
      mc_add_identifier(i, 2, val_frozen);
      cur_vol_regs[entry_mc_bb][i] = val_frozen;
    }

    for (auto &[llvm_bb, mc_bb] : sorted_bbs) {
      outs() << "visiting bb: " << mc_bb->getName() << "\n";
      CurrBB = llvm_bb;
      MCBB = mc_bb;
      auto &mc_instrs = mc_bb->getInstrs();

      for (auto &mc_instr : mc_instrs) {
        outs() << "before visit\n";
        mc_instr.print();
        mc_visit(mc_instr, *Fn);
        outs() << "after visit\n";
      }

      if (!CurrBB->getTerminator()) {
        assert(MCBB->getSuccs().size() == 1 &&
               "expected 1 successor for block with no terminator");
        auto *dst = getBBByName(*Fn, MCBB->getSuccs()[0]->getName());
        createBranch(dst);
      }

      blockCount++;
    }

    outs() << "\n----------lifted-arm-target-missing-phi-params----------\n";
    Fn->print(outs());
    outs() << "lift_todo_phis.size() = " << lift_todo_phis.size() << "\n";

    int tmp_index = 0;
    for (auto &[phi, phi_mc_wrapper] : lift_todo_phis) {
      outs() << "index = " << tmp_index
             << "opcode =" << phi_mc_wrapper->getOpcode() << "\n";
      tmp_index++;
      add_phi_params(phi, phi_mc_wrapper);
    }

    outs() << "returning from run method"
           << "\n";

    return Fn;
  }
};

// Convert an MCFucntion to IR::Function
// Adapted from llvm2alive_ in llvm2alive.cpp with some simplifying assumptions
// FIXME for now, we are making a lot of simplifying assumptions like assuming
// types of arguments.
Function *arm2llvm(Module *OrigModule, MCFunction &MF, Function &srcFn,
                   MCInstPrinter *instrPrinter, MCRegisterInfo *registerInfo) {
  return arm2llvm_(OrigModule, MF, srcFn, instrPrinter, registerInfo).run();
}

// We're overriding MCStreamerWrapper to generate an MCFunction
// from the arm assembly. MCStreamerWrapper provides callbacks to handle
// different parts of the assembly file. The main callbacks that we're
// using right now are emitInstruction and emitLabel to access the
// instruction and labels in the arm assembly.
//
// FIXME for now, we're using this class to generate the MCFunction and
// also print the MCFunction and to convert the MCFunction into SSA form.
// We should move this implementation somewhere else
// TODO we'll need to implement some of the other callbacks to extract more
// information from the asm file. For example, it would be useful to extract
// debug info to determine the number of function parameters.
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
  vector<MCInst>
      Insts; // CHECK this should go as it's only being used for pretty printing
             // which makes it unused after fixing MCInstWrapper::print
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

    if (prev_line == ASMLine::terminator) {
      temp_block = MF.addBlock(MF.getLabel());
    }
    MCInstWrapper Cur_Inst(Inst);
    temp_block->addInst(Cur_Inst);
    Insts.push_back(Inst);

    prev_line = IA->isTerminator(Inst) ? ASMLine::terminator : ASMLine::non_term_instr;
    auto &inst_ref = Cur_Inst.getMCInst();
    auto num_operands = inst_ref.getNumOperands();
    for (unsigned i = 0; i < num_operands; ++i) {
      auto op = inst_ref.getOperand(i);
      if (op.isExpr()) {
        auto expr = op.getExpr();
        if (expr->getKind() == MCExpr::ExprKind::SymbolRef) {
          const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*expr);
          const MCSymbol &Sym = SRE.getSymbol();
          outs() << "target label : " << Sym.getName()
                 << ", offset=" << Sym.getOffset()
                 << '\n'; // FIXME remove when done
        }
      }
    }

    outs() << cnt++ << "  : ";
    Inst.dump_pretty(outs(), IP, " ", MRI);
    if (IA->isBranch(Inst))
      outs() << ": branch ";
    if (IA->isConditionalBranch(Inst))
      outs() << ": conditional branch ";
    if (IA->isUnconditionalBranch(Inst))
      outs() << ": unconditional branch ";
    if (IA->isTerminator(Inst))
      outs() << ": terminator ";
    outs() << "\n";
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
    outs() << cnt++ << "  : ";
    outs() << "inside Emit Label: symbol=" << Symbol->getName() << '\n';
  }

  string findTargetLabel(MCInst &inst_ref) {
    auto num_operands = inst_ref.getNumOperands();
    for (unsigned i = 0; i < num_operands; ++i) {
      auto op = inst_ref.getOperand(i);
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
  void addEntryBlock() {
    MF.addEntryBlock();
  }

  void postOrderDFS(MCBasicBlock &curBlock, BlockSetTy &visited,
                    vector<MCBasicBlock *> &postOrder) {
    visited.insert(&curBlock);
    for (auto succ : curBlock.getSuccs()) {
      if (find(visited.begin(), visited.end(), succ) == visited.end()) {
        postOrderDFS(*succ, visited, postOrder);
      }
    }
    postOrder.push_back(&curBlock);
  }

  vector<MCBasicBlock *> postOrder() {
    vector<MCBasicBlock *> postOrder;
    BlockSetTy visited;
    for (auto &curBlock : MF.BBs) {
      if (visited.count(&curBlock) == 0) {
        postOrderDFS(curBlock, visited, postOrder);
      }
    }
    return postOrder;
  }

  // compute the domination relation
  void generateDominator() {
    MF.generateDominator();
  }

  void generateDominatorFrontier() {
    MF.generateDominatorFrontier();
  }

  void generateDomTree() {
    MF.generateDomTree();
  }

  // compute a map from each variable to its defining block
  void findDefiningBlocks() {
    MF.findDefiningBlocks();
  }

  void findPhis() {
    MF.findPhis();
  }

  // FIXME: this is duplicated code. need to refactor
  void findArgs(Function *src_fn) {
    MF.findArgs(src_fn);
  }

  void rewriteOperands() {
    MF.rewriteOperands();
  }

  void ssaRename() {
    MF.ssaRename();
  }

  // helper function to compute the intersection of predecessor dominator sets
  BlockSetTy intersect(BlockSetTy &preds,
                       unordered_map<MCBasicBlock *, BlockSetTy> &dom) {
    BlockSetTy ret;
    if (preds.size() == 0) {
      return ret;
    }
    if (preds.size() == 1) {
      return dom[*preds.begin()];
    }
    ret = dom[*preds.begin()];
    auto second = ++preds.begin();
    for (auto it = second; it != preds.end(); ++it) {
      auto &pred_set = dom[*it];
      BlockSetTy new_ret;
      for (auto &b : ret) {
        if (pred_set.count(b) == 1)
          new_ret.insert(b);
      }
      ret = new_ret;
    }
    return ret;
  }

  // helper function to invert a graph
  unordered_map<MCBasicBlock *, BlockSetTy>
  invertGraph(unordered_map<MCBasicBlock *, BlockSetTy> &graph) {
    unordered_map<MCBasicBlock *, BlockSetTy> res;
    for (auto &curBlock : graph) {
      for (auto &succ : curBlock.second)
        res[succ].insert(curBlock.first);
    }
    return res;
  }

  // Debug function to print domination info
  void printGraph(unordered_map<MCBasicBlock *, BlockSetTy> &graph) {
    for (auto &curBlock : graph) {
      outs() << curBlock.first->getName() << ": ";
      for (auto &dst : curBlock.second)
        outs() << dst->getName() << " ";
      outs() << "\n";
    }
  }

  // Only call after MF with Basicblocks is constructed to generate the
  // successors for each basic block
  void generateSuccessors() {
    outs() << "generating basic block successors" << '\n';
    for (unsigned i = 0; i < MF.BBs.size(); ++i) {
      auto &cur_bb = MF.BBs[i];
      MCBasicBlock *next_bb_ptr = nullptr;
      if (i < MF.BBs.size() - 1)
        next_bb_ptr = &MF.BBs[i + 1];

      if (cur_bb.size() == 0) {
        outs()
            << "generateSuccessors, encountered basic block with 0 instructions"
            << '\n';
        continue;
      }
      auto &last_mc_instr = cur_bb.getInstrs().back().getMCInst();
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

  // Remove empty basic blocks from the machine function
  void removeEmptyBlocks() {
    outs() << "removing empty basic blocks" << '\n';
    erase_if(MF.BBs, [](MCBasicBlock b) { return b.size() == 0; });
  }

  // Only call after generateSucessors() has been called
  // generate predecessors for each basic block in a MCFunction
  void generatePredecessors() {
    outs() << "generating basic block predecessors" << '\n';
    for (auto &block : MF.BBs) {
      for (auto it = block.succBegin(); it != block.succEnd(); ++it) {
        auto successor = *it;
        successor->addPred(&block);
      }
    }
  }

  void printBlocksMF() {
    outs() << "# of Blocks (MF print blocks) = " << MF.BBs.size() << '\n';
    outs() << "-------------\n";
    int i = 0;
    for (auto &block : MF.BBs) {
      outs() << "block " << i << ", name= " << block.getName() << '\n';
      for (auto &inst : block.getInstrs()) {
        inst.getMCInst().dump_pretty(outs(), IP, " ", MRI);
        outs() << '\n';
      }
      i++;
    }
  }

  void printCFG() {
    outs() << "printing arm function CFG" << '\n';
    outs() << "successors" << '\n';
    for (auto &block : MF.BBs) {
      outs() << block.getName() << ": [";
      for (auto it = block.succBegin(); it != block.succEnd(); ++it) {
        auto successor = *it;
        outs() << successor->getName() << ", ";
      }
      outs() << "]\n";
    }

    outs() << "predecessors" << '\n';
    for (auto &block : MF.BBs) {
      outs() << block.getName() << ": [";
      for (auto it = block.predBegin(); it != block.predEnd(); ++it) {
        auto predecessor = *it;
        outs() << predecessor->getName() << ", ";
      }
      outs() << "]\n";
    }
  }

  // findLastRetWrite will do a breadth-first search through the dominator tree
  // looking for the last write to X0.
  int findLastRetWrite(MCBasicBlock *bb) {
    set<MCBasicBlock *> to_search = {bb};
    while (to_search.size() != 0) { // exhaustively search previous BBs
      set<MCBasicBlock *> next_search =
          {}; // blocks to search in the next iteration
      for (auto &b : to_search) {
        auto instrs = b->getInstrs();
        for (auto it = instrs.rbegin(); it != instrs.rend();
             it++) { // iterate backwards looking for writes
          const auto &inst = it->getMCInst();
          const auto &firstOp = inst.getOperand(0);
          if (firstOp.isReg() && firstOp.getReg() == AArch64::X0) {
            return it->getOpId(0);
          }
        }
        for (auto &new_b : MF.dom_tree_inv[b]) {
          next_search.insert(new_b);
        }
      }
      to_search = next_search;
    }

    // if we've found no write to X0, we just need to return version 2,
    // which corresponds to the function argument X0 after freeze
    return 2;
  }

  // TODO: @Nader this should just fall out of our SSA implementation
  void adjustReturns() {
    for (auto &block : MF.BBs) {
      for (auto &instr : block.getInstrs()) {
        if (instr.getOpcode() == AArch64::RET) {
          auto lastWriteID = findLastRetWrite(&block);
          auto &retArg = instr.getMCInst().getOperand(0);
          instr.setOpId(0, lastWriteID);
          retArg.setReg(AArch64::X0);
        }
      }
    }

    outs() << "After adjusting return:\n";
    for (auto &b : MF.BBs) {
      outs() << b.getName() << ":\n";
      b.print();
    }
  }
};

} // namespace

namespace lifter {

unsigned int orig_ret_bitwidth;
bool has_ret_attr;
const Target *Targ;

vector<unsigned> orig_input_width;

void reset() {
  static bool runAlready = false;

  if (!runAlready) {
    LLVMInitializeAArch64TargetInfo();
    LLVMInitializeAArch64Target();
    LLVMInitializeAArch64TargetMC();
    LLVMInitializeAArch64AsmParser();
    LLVMInitializeAArch64AsmPrinter();

    string Error;
    Targ = TargetRegistry::lookupTarget(TripleName, Error);
    if (!Targ) {
      outs() << Error;
      exit(-1);
    }

    runAlready = true;
  }

  // FIXME this is a pretty error-prone way to reset the state,
  // probably should just encapsulate this in a class
  orig_ret_bitwidth = 64;
  has_ret_attr = false;
  orig_input_width.clear();
  mc_cache.clear();
  overflow_aggregate_types.clear();
  lifted_vector_types.clear();
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

  // FIXME remove printing of the mcInsts
  // For now, print the parsed instructions for debug puropses
  outs() << "\n\nPretty Parsed MCInsts:\n";
  for (auto I : MCSW.Insts) {
    I.dump_pretty(outs(), IPtemp.get(), " ", MRI.get());
    outs() << '\n';
  }

  outs() << "\n\nParsed MCInsts:\n";
  for (auto I : MCSW.Insts) {
    I.dump_pretty(outs());
    outs() << '\n';
  }

  outs() << "\n\n";

  MCSW.printBlocksMF();
  MCSW.removeEmptyBlocks(); // remove empty basic blocks, including .Lfunc_end
  MCSW.printBlocksMF();

  MCSW.addEntryBlock();
  MCSW.generateSuccessors();
  MCSW.generatePredecessors();
  MCSW.findArgs(srcFn); // needs refactoring
  MCSW.rewriteOperands();
  MCSW.printCFG();
  MCSW.generateDominator();
  MCSW.generateDominatorFrontier();
  MCSW.findDefiningBlocks();
  MCSW.findPhis();
  MCSW.generateDomTree();
  MCSW.ssaRename();
  MCSW.adjustReturns(); // needs refactoring

  outs() << "after SSA conversion\n";
  MCSW.printBlocksMF();

  auto lifted =
      arm2llvm(LiftedModule, MCSW.MF, *srcFn, IPtemp.get(), MRI.get());

  if (llvm::verifyModule(*LiftedModule, &llvm::outs()))
    llvm::report_fatal_error("Lifted module is broken, this should not happen");

  return make_pair(srcFn, lifted);
}

} // namespace lifter
