// include first to avoid ambiguity for comparison operator from
// util/spaceship.h
#include "llvm/MC/MCAsmInfo.h"

#include "backend_tv/mc.h"
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

namespace {

set<int> s_flag = {
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
};

set<int> instrs_32 = {
    AArch64::ADDWrx,  AArch64::ADDSWrs,  AArch64::ADDSWri,  AArch64::ADDWrs,
    AArch64::ADDWri,  AArch64::ADDSWrx,  AArch64::ASRVWr,   AArch64::SUBWri,
    AArch64::SUBWrs,  AArch64::SUBWrx,   AArch64::SUBSWrs,  AArch64::SUBSWri,
    AArch64::SUBSWrx, AArch64::SBFMWri,  AArch64::CSELWr,   AArch64::ANDWri,
    AArch64::ANDWrr,  AArch64::ANDWrs,   AArch64::ANDSWri,  AArch64::ANDSWrr,
    AArch64::ANDSWrs, AArch64::MADDWrrr, AArch64::MSUBWrrr, AArch64::EORWri,
    AArch64::CSINVWr, AArch64::CSINCWr,  AArch64::MOVZWi,   AArch64::MOVNWi,
    AArch64::MOVKWi,  AArch64::LSLVWr,   AArch64::LSRVWr,   AArch64::ORNWrs,
    AArch64::UBFMWri, AArch64::BFMWri,   AArch64::ORRWrs,   AArch64::ORRWri,
    AArch64::SDIVWr,  AArch64::UDIVWr,   AArch64::EXTRWrri, AArch64::EORWrs,
    AArch64::RORVWr,  AArch64::RBITWr,   AArch64::CLZWr,    AArch64::REVWr,
    AArch64::CSNEGWr, AArch64::BICWrs,   AArch64::BICSWrs,  AArch64::EONWrs,
    AArch64::REV16Wr, AArch64::Bcc,      AArch64::CCMPWr,   AArch64::CCMPWi};

set<int> instrs_64 = {
    AArch64::ADDXrx,    AArch64::ADDSXrs,   AArch64::ADDSXri,
    AArch64::ADDXrs,    AArch64::ADDXri,    AArch64::ADDSXrx,
    AArch64::ASRVXr,    AArch64::SUBXri,    AArch64::SUBXrs,
    AArch64::SUBXrx,    AArch64::SUBSXrs,   AArch64::SUBSXri,
    AArch64::SUBSXrx,   AArch64::SBFMXri,   AArch64::CSELXr,
    AArch64::ANDXri,    AArch64::ANDXrr,    AArch64::ANDXrs,
    AArch64::ANDSXri,   AArch64::ANDSXrr,   AArch64::ANDSXrs,
    AArch64::MADDXrrr,  AArch64::MSUBXrrr,  AArch64::EORXri,
    AArch64::CSINVXr,   AArch64::CSINCXr,   AArch64::MOVZXi,
    AArch64::MOVNXi,    AArch64::MOVKXi,    AArch64::LSLVXr,
    AArch64::LSRVXr,    AArch64::ORNXrs,    AArch64::UBFMXri,
    AArch64::BFMXri,    AArch64::ORRXrs,    AArch64::ORRXri,
    AArch64::SDIVXr,    AArch64::UDIVXr,    AArch64::EXTRXrri,
    AArch64::EORXrs,    AArch64::SMADDLrrr, AArch64::UMADDLrrr,
    AArch64::RORVXr,    AArch64::RBITXr,    AArch64::CLZXr,
    AArch64::REVXr,     AArch64::CSNEGXr,   AArch64::BICXrs,
    AArch64::BICSXrs,   AArch64::EONXrs,    AArch64::SMULHrr,
    AArch64::UMULHrr,   AArch64::REV32Xr,   AArch64::REV16Xr,
    AArch64::SMSUBLrrr, AArch64::UMSUBLrrr, AArch64::PHI,
    AArch64::TBZW,      AArch64::TBZX,      AArch64::TBNZW,
    AArch64::TBNZX,     AArch64::B,         AArch64::CBZW,
    AArch64::CBZX,      AArch64::CBNZW,     AArch64::CBNZX,
    AArch64::CCMPXr,    AArch64::CCMPXi,    AArch64::LDRXui};

set<int> instrs_128 = {AArch64::FMOVXDr, AArch64::INSvi64gpr};

set<int> instrs_no_write = {AArch64::Bcc,    AArch64::B,      AArch64::TBZW,
                            AArch64::TBZX,   AArch64::TBNZW,  AArch64::TBNZX,
                            AArch64::CBZW,   AArch64::CBZX,   AArch64::CBNZW,
                            AArch64::CBNZX,  AArch64::CCMPWr, AArch64::CCMPWi,
                            AArch64::CCMPXr, AArch64::CCMPXi};

set<int> ins_variant = {AArch64::INSvi64gpr};

bool has_s(int instr) {
  return s_flag.contains(instr);
}

// FIXME -- do this without the strings, just keep a map or something
BasicBlock *getBBByName(Function &Fn, StringRef name) {
  cout << "searching for BB named '" << (string)name << "'\n";
  BasicBlock *BB = nullptr;
  for (auto &bb : Fn) {
    cout << "  we have a BB named '" << (string)bb.getName() << "'\n";
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
      errs() << "label : " << Sym.getName() << '\n'; // FIXME remove when done
      id = Sym.getOffset();
    } else {
      assert("unsupported mcExpr" && false);
    }
  } else {
    assert("no" && false);
  }

  return hash<unsigned long>()(prefix * id);
}

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
    cout << "< MCInstWrapper " << getOpcode() << " ";
    unsigned idx = 0;
    for (auto it = instr.begin(); it != instr.end(); ++it) {
      if (it->isReg()) {
        if (getOpcode() == AArch64::PHI && idx >= 1) {
          cout << "<Phi arg>:[(" << it->getReg() << "," << op_ids[idx] << "),"
               << getOpPhiBlock(idx) << "]>";
        } else {
          cout << "<MCOperand Reg:(" << it->getReg() << ", " << op_ids[idx]
               << ")>";
        }
      } else if (it->isImm()) {
        cout << "<MCOperand Imm:" << it->getImm() << ">";
      } else if (it->isExpr()) {
        cout << "<MCOperand Expr:>"; // FIXME
      } else {
        assert("MCInstWrapper printing an unsupported operand" && false);
      }
      idx++;
    }
    cout << ">\n";
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

// utility function
[[maybe_unused]] bool isIntegerRegister(const MCOperand &op) {
  if (!op.isReg())
    return false;

  if ((AArch64::W0 <= op.getReg() && op.getReg() <= AArch64::W30) ||
      (AArch64::X0 <= op.getReg() && op.getReg() <= AArch64::X28)) {
    return true;
  }

  return false;
}

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
  MCInstrAnalysis *Ana_ptr;
  MCInstPrinter *IP_ptr;
  MCRegisterInfo *MRI_ptr;
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
      if (Ana_ptr->isConditionalBranch(last_mc_instr.getMCInst()) ||
          Ana_ptr->isUnconditionalBranch(last_mc_instr.getMCInst())) {
        string target = last_mc_instr.findTargetLabel();
        if (target == first_block.getName()) {
          add_entry_block = true;
          break;
        }
      }
    }

    if (add_entry_block) {
      cout << "Added arm_tv_entry block\n";
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
    cout << "postOrder\n";
    for (auto &curBlock : blocks) {
      cout << curBlock->getName() << "\n";
      dom[curBlock] = BlockSetTy();
      for (auto &b : blocks)
        dom[curBlock].insert(b);
    }

    cout << "printing dom before\n";
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
    cout << "printing dom after\n";
    printGraph(dom);
  }

  void generateDominatorFrontier() {
    auto dominates = invertGraph(dom);
    cout << "printing dom_inverse\n";
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
    cout << "printing dom_frontier\n";
    printGraph(dom_frontier);
    return;
  }

  void generateDomTree() {
    auto dominates = invertGraph(dom);
    cout << "printing dom_inverse\n";
    printGraph(dominates);
    cout << "-----------------\n";
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

    cout << "printing s_dom\n";
    printGraph(s_dom);
    cout << "-----------------\n";

    cout << "printing child_dom\n";
    printGraph(child_dom);
    cout << "-----------------\n";

    cout << "printing dom_tree\n";
    printGraph(dom_tree);
    cout << "-----------------\n";

    dom_tree_inv = invertGraph(dom_tree);
    cout << "printing dom_tree_inv\n";
    printGraph(dom_tree_inv);
    cout << "-----------------\n";
  }

  // compute a map from each variable to its defining block
  void findDefiningBlocks() {
    for (auto &block : BBs) {
      for (auto &w_instr : block.getInstrs()) {
        auto &mc_instr = w_instr.getMCInst();
        // need to check for special instructions like ret and branch
        // need to check for special destination operands like WZR

        if (Ana_ptr->isCall(mc_instr))
          report_fatal_error("Function calls not supported yet");

        if (Ana_ptr->isReturn(mc_instr) || Ana_ptr->isBranch(mc_instr)) {
          continue;
        }

        assert(mc_instr.getNumOperands() > 0 && "MCInst with zero operands");

        // CHECK: if there is an ARM instruction that writes to two variables
        auto &dst_operand = mc_instr.getOperand(0);

        assert((dst_operand.isReg() || dst_operand.isImm()) &&
               "unsupported destination operand");

        if (dst_operand.isImm()) {
          cout << "destination operand is an immediate. printing the "
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
      cout << "defs for \n";
      var.print(errs(), MRI_ptr);
      cout << "\n";
      for (auto &block : blockSet) {
        cout << block->getName() << ",";
      }
      cout << "\n";
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
    cout << "mapping from block name to variable names that require phi nodes "
            "in block\n";
    for (auto &[block, varSet] : phis) {
      cout << "phis for: " << block->getName() << "\n";
      for (auto &var : varSet) {
        var.print(errs(), MRI_ptr);
        cout << "\n";
      }
      cout << "-------------\n";
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
    cout << "printing fn_args\n";
    for (auto &arg : fn_args) {
      arg.print(errs(), MRI_ptr);
      cout << "\n";
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
          op.print(str_stream, MRI_ptr);
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

    cout << "printing fn_args after rewrite\n";
    for (auto &arg : fn_args) {
      arg.print(errs(), MRI_ptr);
      cout << "\n";
    }

    cout << "printing MCInsts after rewriting operands\n";
    printBlocks();
  }

  void ssaRename() {
    unordered_map<MCOperand, vector<unsigned>, MCOperandHash, MCOperandEqual>
        stack;
    unordered_map<MCOperand, unsigned, MCOperandHash, MCOperandEqual> counters;

    cout << "SSA rename\n";

    // auto printStack = [&](unordered_map<MCOperand,
    // vector<unsigned>,
    //                                          MCOperandHash, MCOperandEqual>
    //                           s) {
    //   for (auto &[var, stack_vec] : s) {
    //     errs() << "stack for ";
    //     var.print(errs(), MRI_ptr);
    //     errs() << "\n";
    //     for (auto &stack_item : stack_vec) {
    //       cout << stack_item << ",";
    //     }
    //     cout << "\n";
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
      cout << "renaming block: " << block->getName() << "\n";
      block->print();
      cout << "----\n";
      for (auto &phi_var : phis[block]) {

        MCInst new_phi_instr;
        new_phi_instr.setOpcode(AArch64::PHI);
        new_phi_instr.addOperand(MCOperand::createReg(phi_var.getReg()));
        new_phi_instr.dump_pretty(errs(), IP_ptr, " ", MRI_ptr);

        MCInstWrapper new_w_instr(new_phi_instr);
        block->addInstBegin(std::move(new_w_instr));
        auto phi_dst_id = pushFresh(phi_var);
        cout << "phi_dst_id: " << phi_dst_id << "\n";
        block->getInstrs()[0].setOpId(0, phi_dst_id);
      }
      cout << "after phis\n";
      block->print();
      cout << "----\n";

      cout << "renaming instructions\n";
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

        // mc_instr.dump_pretty(errs(), IP_ptr, " ", MRI_ptr);
        // errs() << "\n";
        // errs() << "printing stack\n";
        // printStack(stack);
        // errs() << "printing operands\n";
        unsigned i = 1;
        if (instrs_no_write.contains(mc_instr.getOpcode())) {
          cout << "iterating from first element in rename\n";
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

          op.print(errs(), MRI_ptr);
          errs() << "\n";

          auto &arg_id = stack[op][0];
          w_instr.setOpId(i, arg_id);
        }
        errs() << "printing operands done\n";
        if (instrs_no_write.contains(mc_instr.getOpcode()))
          continue;

        errs() << "renaming dst\n";
        auto &dst_op = mc_instr.getOperand(0);
        dst_op.print(errs(), MRI_ptr);
        auto dst_id = pushFresh(dst_op);
        w_instr.setOpId(0, dst_id);
        errs() << "\n";
      }

      errs() << "renaming phi args in block's successors\n";

      for (auto s_block : block->getSuccs()) {
        errs() << block->getName() << " -> " << s_block->getName() << "\n";

        for (auto &phi_var : phis[s_block]) {
          if (stack.find(phi_var) == stack.end()) {
            phi_var.print(errs(), MRI_ptr);
            assert(false && "phi var not in stack");
          }
          assert(stack[phi_var].size() > 0 && "phi var stack empty");

          if (phi_args[s_block].find(phi_var) == phi_args[s_block].end()) {
            phi_args[s_block][phi_var] = vector<pair<unsigned, string>>();
          }
          errs() << "phi_arg[" << s_block->getName() << "][" << phi_var.getReg()
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

    cout << "adding volatile registers\n";

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
        cout << "adding volatile: " << reg_num << "\n";
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
    cout << "printing MCInsts after renaming operands\n";
    printBlocks();

    cout << "printing phi args\n";
    for (auto &[block, phi_vars] : phi_args) {
      cout << "block: " << block->getName() << "\n";
      for (auto &[phi_var, args] : phi_vars) {
        cout << "phi_var: " << phi_var.getReg() << "\n";
        for (auto arg : args) {
          cout << arg.first << "-" << arg.second << ", ";
        }
        cout << "\n";
      }
    }

    cout << "-----------------\n"; // adding args to phi-nodes
    for (auto &[block, phi_vars] : phi_args) {
      for (auto &w_instr : block->getInstrs()) {
        auto &mc_instr = w_instr.getMCInst();
        if (mc_instr.getOpcode() != AArch64::PHI)
          break;

        auto phi_var = mc_instr.getOperand(0);
        unsigned index = 1;
        cout << "phi arg size " << phi_args[block][phi_var].size() << "\n";
        for (auto var_id_label_pair : phi_args[block][phi_var]) {
          cout << "index = " << index
               << ", var_id = " << var_id_label_pair.first << "\n";
          mc_instr.addOperand(MCOperand::createReg(phi_var.getReg()));
          w_instr.pushOpId(var_id_label_pair.first);
          w_instr.setOpPhiBlock(index, var_id_label_pair.second);
          w_instr.print();
          index++;
        }
      }
    }

    cout << "printing MCInsts after adding args to phi-nodes\n";
    for (auto &b : BBs) {
      cout << b.getName() << ":\n";
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
      cout << curBlock.first->getName() << ": ";
      for (auto &dst : curBlock.second)
        cout << dst->getName() << " ";
      cout << "\n";
    }
  }

  void printBlocks() {
    cout << "# of Blocks (orig print blocks) = " << BBs.size() << '\n';
    cout << "-------------\n";
    int i = 0;
    for (auto &block : BBs) {
      errs() << "block " << i << ", name= " << block.getName() << '\n';
      for (auto &inst : block.getInstrs()) {
        inst.getMCInst().dump_pretty(errs(), IP_ptr, " ", MRI_ptr);
        errs() << '\n';
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
// unsigned type_id_counter{0};

// Keep track of which oprands had their type adjusted and their original
// bitwidth
vector<pair<unsigned, unsigned>> new_input_idx_bitwidth;
unsigned int orig_ret_bitwidth{64};
bool has_ret_attr{false};

// IR::Type *uadd_overflow_type(MCOperand op, int size) {
//   assert(op.isReg());
//
//   auto p = overflow_aggregate_types.try_emplace(op);
//   auto &st = p.first->second;
//   vector<IR::Type *> elems;
//   vector<bool> is_padding{false, false, true};
//
//   if (p.second) {
//     auto add_res_ty = &llvm_util::get_int_type(size);
//     auto add_ov_ty = &llvm_util::get_int_type(1);
//     auto padding_ty = &llvm_util::get_int_type(24);
//     elems.push_back(add_res_ty);
//     elems.push_back(add_ov_ty);
//     elems.push_back(padding_ty);
//     st = make_unique<IR::StructType>("ty_" + to_string(type_id_counter++),
//                                  std::move(elems), std::move(is_padding));
//   }
//   return st.get();
// }

// Add IR value to cache
void mc_add_identifier(unsigned reg, unsigned version, Value *v) {
  mc_cache.emplace(make_pair(reg, version), v);
  cout << "mc_cache: adding " << reg << ", " << version << endl;
}

Value *mc_get_operand(unsigned reg, unsigned version) {
  cout << "mc_cache: looking for " << reg << ", " << version;
  if (auto I = mc_cache.find(make_pair(reg, version)); I != mc_cache.end()) {
    cout << "  found it" << endl;
    return I->second;
  }
  cout << "  did not find it" << endl;
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

// replicate is a pseudocode function used in the ARM ISA
// replicate's documentation isn't particularly clear, but it takes a bit-vector
// of size M, and duplicates it N times, returning a bit-vector of size M*N
// reference:
// https://developer.arm.com/documentation/ddi0596/2020-12/Shared-Pseudocode/Shared-Functions?lang=en#impl-shared.Replicate.2
APInt replicate(APInt bits, unsigned N) {
  auto bitsWidth = bits.getBitWidth();
  auto newInt = APInt(bitsWidth * N, 0);
  auto mask = APInt(bitsWidth * N, bits.getZExtValue());
  for (size_t i = 0; i < N; i++)
    newInt |= (mask << (bitsWidth * i));
  return newInt;
}

// adapted from the arm ISA DecodeBitMasks:
// https://developer.arm.com/documentation/ddi0596/2020-12/Shared-Pseudocode/AArch64-Instrs?lang=en#impl-aarch64.DecodeBitMasks.4
// Decode AArch64 bitfield and logical immediate masks which use a similar
// encoding structure
// TODO: this is super broken
[[maybe_unused]] tuple<APInt, APInt> decode_bit_mask(bool immNBit,
                                                     uint32_t _imms,
                                                     uint32_t _immr,
                                                     bool immediate, int M) {
  APInt imms(6, _imms);
  APInt immr(6, _immr);

  auto notImm = APInt(6, _imms);
  notImm.flipAllBits();

  auto concatted = APInt(1, (immNBit ? 1 : 0)).concat(notImm);
  auto len = concatted.getBitWidth() - concatted.countLeadingZeros() - 1;

  // Undefined behavior
  assert(len >= 1);
  assert(M >= (1 << len));

  auto levels = APInt::getAllOnes(len).zext(6);

  auto S = (imms & levels);
  auto R = (immr & levels);

  auto diff = S - R;

  auto esize = (1 << len);
  auto d = APInt(len - 1, diff.getZExtValue());

  auto welem = APInt::getAllOnes(S.getZExtValue() + 1).zext(esize).rotr(R);
  auto telem = APInt::getAllOnes(d.getZExtValue() + 1).zext(esize);

  auto wmask = replicate(welem, esize);
  auto tmask = replicate(telem, esize);

  return {welem.trunc(M), telem.trunc(M)};
}

// Values currently holding ZNCV bits, for each basicblock respectively
unordered_map<MCBasicBlock *, Value *> cur_vs;
unordered_map<MCBasicBlock *, Value *> cur_zs;
unordered_map<MCBasicBlock *, Value *> cur_ns;
unordered_map<MCBasicBlock *, Value *> cur_cs;

// Values currently holding the latest definition for a volatile register, for
// each basic block currently used by vector instructions only
unordered_map<MCBasicBlock *, unordered_map<unsigned, Value *>> cur_vol_regs;

BasicBlock *get_basic_block(Function &F, MCOperand &jmp_tgt) {
  assert(jmp_tgt.isExpr() && "[get_basic_block] expected expression operand");
  assert((jmp_tgt.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
         "[get_basic_block] expected symbol ref as jump operand");
  const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt.getExpr());
  const MCSymbol &Sym = SRE.getSymbol();
  StringRef name = Sym.getName();
  cout << "jump target: " << Sym.getName().str() << '\n';
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
  Module *LiftedModule;
  LLVMContext &LLVMCtx = LiftedModule->getContext();
  MCFunction &MF;
  // const DataLayout &DL;
  Function &srcFn;
  MCBasicBlock *MCBB; // the current machine block
  unsigned blockCount{0};
  BasicBlock *CurrBB; // the current block

  MCInstPrinter *instrPrinter;
  MCRegisterInfo *registerInfo;
  vector<pair<PHINode *, MCInstWrapper *>> lift_todo_phis;

  MCInstWrapper *wrapper{nullptr};

  unsigned instructionCount;
  unsigned curId;
  bool ret_void{false};

  Type *get_int_type(int bits) {
    assert(bits > 0);
    return Type::getIntNTy(LLVMCtx, bits);
  }

  Value *intconst(uint64_t val, int bits) {
    return ConstantInt::get(LLVMCtx, llvm::APInt(bits, val));
  }

  [[noreturn]] void visitError(MCInstWrapper &I) {
    // flush must happen before error is printed to make sure the error
    // comes out nice and pretty when combing the stdout/stderr in scripts
    cout.flush();

    errs() << "ERROR: Unsupported arm instruction: "
           << instrPrinter->getOpcodeName(I.getMCInst().getOpcode()) << "\n";
    errs().flush();
    cerr.flush();
    exit(-1); // FIXME handle this better
  }

  int get_size(int instr) {
    if (instrs_32.contains(instr))
      return 32;
    if (instrs_64.contains(instr))
      return 64;
    if (instrs_128.contains(instr))
      return 128;
    cout << "get_size encountered unknown instruction" << endl;
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
          intconst(encodedShift & 0x3f, typ->getIntegerBitWidth()));
    default:
      // FIXME: handle other case (msl)
      report_fatal_error("shift type not supported");
    }
    return createBinop(
        value, intconst(encodedShift & 0x3f, typ->getIntegerBitWidth()), op);
  }

  Value *reg_shift(int value, int size, int encodedShift) {
    return reg_shift(intconst(value, size), encodedShift);
  }

  Value *getIdentifier(unsigned reg, unsigned id) {
    return mc_get_operand(reg, id);
  }

  // TODO: make it so that lshr generates code on register lookups
  //  some instructions make use of this, and the semantics need to be worked
  //  out
  Value *get_value(int idx, int shift = 0) {
    auto inst = wrapper->getMCInst();
    auto op = inst.getOperand(idx);
    auto size = get_size(inst.getOpcode());

    assert(op.isImm() || op.isReg());

    Value *v = nullptr;
    auto ty = get_int_type(size);

    if (op.isImm()) {
      v = intconst(op.getImm(), size);
    } else if (op.getReg() == AArch64::XZR) {
      v = intconst(0, 64);
    } else if (op.getReg() == AArch64::WZR) {
      v = intconst(0, 32);
    } else if (size == 128) { // FIXME this needs to be generalized
      auto tmp = getIdentifier(op.getReg(), wrapper->getOpId(idx));
      v = createZExt(tmp, ty);
    } else if (size == 64) {
      v = getIdentifier(op.getReg(), wrapper->getOpId(idx));
    } else if (size == 32) {
      auto tmp = getIdentifier(op.getReg(), wrapper->getOpId(idx));
      v = createTrunc(tmp, ty);
    } else {
      assert(false && "unhandled case in get_value*");
    }

    if (shift != 0)
      v = reg_shift(v, shift);

    return v;
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
    return new AllocaInst(ty, 0, sz, NameStr, CurrBB);
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

  LoadInst *createLoad(Type *ty, Value *ptr) {
    return new LoadInst(ty, ptr, next_name(), CurrBB);
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
    return CallInst::Create(decl, {v, intconst(0, 1)}, next_name(), CurrBB);
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

  FreezeInst *createFreeze(Value *v, const string &NameStr = "") {
    return new FreezeInst(v, (NameStr == "") ? next_name() : NameStr, CurrBB);
  }

  CastInst *createTrunc(Value *v, Type *t, const string &NameStr = "") {
    return CastInst::Create(Instruction::Trunc, v, t,
                            (NameStr == "") ? next_name() : NameStr, CurrBB);
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
    cout << "entering add_phi_params" << endl;
    assert(phi_mc_wrapper->getOpcode() == AArch64::PHI &&
           "cannot add params to non-phi instr");
    for (unsigned i = 1; i < phi_mc_wrapper->getMCInst().getNumOperands();
         ++i) {
      assert(phi_mc_wrapper->getMCInst().getOperand(i).isReg());
      cout << "<Phi arg>:[("
           << phi_mc_wrapper->getMCInst().getOperand(i).getReg() << ","
           << phi_mc_wrapper->getOpId(i) << "),"
           << phi_mc_wrapper->getOpPhiBlock(i) << "]>\n";
      string block_name(phi_mc_wrapper->getOpPhiBlock(i));
      auto val =
          mc_get_operand(phi_mc_wrapper->getMCInst().getOperand(i).getReg(),
                         phi_mc_wrapper->getOpId(i));
      assert(val != nullptr);
      cout << "block name = " << block_name << endl;
      phi_instr->addIncoming(val, getBBByName(*(phi_instr->getParent()->getParent()), block_name));
      cout << "i is = " << i << endl;
    }
    cout << "exiting add_phi_params" << endl;
  }

  void add_identifier(Value *v) {
    auto reg = wrapper->getMCInst().getOperand(0).getReg();
    auto version = wrapper->getOpId(0);
    // TODO: this probably should be in visit
    instructionCount++;
    mc_add_identifier(reg, version, v);
  }

  // store will store an IR value using the current instruction's destination
  // register.
  // All values are kept track of in their full-width counterparts to simulate
  // registers. For example, a x0 register would be kept track of in the bottom
  // bits of w0. Optionally, there is an "s" or signed flag that can be used
  // when writing smaller bit-width values to half or full-width registers which
  // will perform a small sign extension procedure.
  void store(Value *v, bool s = false) {
    if (wrapper->getMCInst().getOperand(0).getReg() == AArch64::WZR) {
      instructionCount++;
      return;
    }

    // direcly add the value to the value cache
    if (v->getType()->getIntegerBitWidth() == 64 ||
        v->getType()->getIntegerBitWidth() == 128) {
      add_identifier(v);
      return;
    }

    size_t regSize = get_size(wrapper->getMCInst().getOpcode());

    // regSize should only be 32/64
    assert(regSize == 32 || regSize == 64);

    // if the s flag is set, the value is smaller than 32 bits,
    // and the register we are storing it in _is_ 32 bits, we sign extend
    // to 32 bits before zero-extending to 64
    if (s && regSize == 32 && v->getType()->getIntegerBitWidth() < 32) {
      auto sext32 = createSExt(v, get_int_type(32));
      auto zext64 = createZExt(sext32, get_int_type(64));
      add_identifier(zext64);
    } else {
      auto op = s ? Instruction::SExt : Instruction::ZExt;
      auto new_val = createCast(v, get_int_type(64), op);
      add_identifier(new_val);
    }
  }

  Value *retrieve_pstate(unordered_map<MCBasicBlock *, Value *> &pstate_map,
                         MCBasicBlock *bb) {
    auto pstate_val = pstate_map[bb];
    if (pstate_val)
      return pstate_val;
    cout << "retrieving pstate for block " << bb->getName() << endl;
    assert(
        bb->getPreds().size() == 1 &&
        "pstate can only be retrieved for blocks with up to one predecessor");
    auto pred_bb = bb->getPreds().front();
    auto pred_pstate = pstate_map[pred_bb];
    assert(pred_pstate != nullptr && "pstate must be defined for predecessor");
    pstate_map[bb] = pred_pstate;
    return pred_pstate;
  }

  Value *evaluate_condition(uint64_t cond, MCBasicBlock *bb) {
    // cond<0> == '1' && cond != '1111'
    auto invert_bit = (cond & 1) && (cond != 15);

    cond >>= 1;

    auto cur_v = retrieve_pstate(cur_vs, bb);
    auto cur_z = retrieve_pstate(cur_zs, bb);
    auto cur_n = retrieve_pstate(cur_ns, bb);
    auto cur_c = retrieve_pstate(cur_cs, bb);

    assert(cur_v != nullptr && cur_z != nullptr && cur_n != nullptr &&
           cur_c != nullptr && "condition not initialized");

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
          createICmp(ICmpInst::Predicate::ICMP_EQ, cur_c, intconst(1, 1));
      // Z == 0
      auto z_cond =
          createICmp(ICmpInst::Predicate::ICMP_EQ, cur_z, intconst(0, 1));
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
          createICmp(ICmpInst::Predicate::ICMP_EQ, cur_z, intconst(0, 1));
      res = createAnd(n_eq_v, z_cond);
      break;
    }
    case 7:
      res = intconst(1, 1);
      break;
    default:
      assert(false && "invalid condition code");
      break;
    }

    assert(res != nullptr && "condition code was not generated");

    if (invert_bit)
      res = createXor(res, intconst(1, 1));

    return res;
  }

  void set_z(Value *val) {
    auto zero = intconst(0, val->getType()->getIntegerBitWidth());
    auto z = createICmp(ICmpInst::Predicate::ICMP_EQ, val, zero);
    cur_zs[MCBB] = z;
  }

  void set_n(Value *val) {
    auto zero = intconst(0, val->getType()->getIntegerBitWidth());
    auto n = createICmp(ICmpInst::Predicate::ICMP_SLT, val, zero);
    cur_ns[MCBB] = n;
  }

public:
  arm2llvm_(Module *LiftedModule, MCFunction &MF,
            Function &srcFn, MCInstPrinter *instrPrinter,
            MCRegisterInfo *registerInfo)
      : LiftedModule(LiftedModule), MF(MF), srcFn(srcFn),
        instrPrinter(instrPrinter), registerInfo(registerInfo),
        instructionCount(0), curId(0) {}

  // Visit an MCInstWrapper instructions and convert it to LLVM IR
  void mc_visit(MCInstWrapper &I, Function &Fn) {
    auto opcode = I.getOpcode();
    auto &mc_inst = I.getMCInst();
    wrapper = &I;
    curId = 0;

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
      auto a = get_value(1);
      Value *b = nullptr;

      switch (opcode) {
      case AArch64::ADDWrx:
      case AArch64::ADDSWrx:
      case AArch64::ADDXrx:
      case AArch64::ADDSXrx: {
        auto size = get_size(opcode);
        auto ty = get_int_type(size);
        auto extendImm = mc_inst.getOperand(3).getImm();
        auto extendType = ((extendImm >> 3) & 0x7);

        cout << "extendImm: " << extendImm << ", extendType: " << extendType
             << "\n";

        auto isSigned = extendType / 4;

        // extendSize is necessary so that we can start with the word size
        // ARM wants us to (byte, half, full) and then sign extend to a new
        // size. Without extendSize being used for a trunc, a lot of masking
        // and more manual work to sign extend would be necessary
        unsigned extendSize = 8 << (extendType % 4);
        auto shift = extendImm & 0x7;

        b = get_value(2);

        // Make sure to not to trunc to the same size as the parameter.
        // Sometimes ADDrx is generated using 32 bit registers and "extends" to
        // a 32 bit value. This is seen as a type error by LLVM, but is valid
        // ARM
        if (extendSize != (unsigned)size) {
          auto truncType = get_int_type(extendSize);
          b = createTrunc(b, truncType);
          b = createCast(b, ty,
                         isSigned ? Instruction::SExt : Instruction::ZExt);
        }

        // shift may not be there, it may just be the extend
        if (shift != 0)
          b = createShl(b, intconst(shift, size));
        break;
      }
      default:
        b = get_value(2, mc_inst.getOperand(3).getImm());
        break;
      }

      if (has_s(opcode)) {
        auto sadd = createSAddOverflow(a, b);
        auto result = createExtractValue(sadd, {0});
        auto new_v = createExtractValue(sadd, {1});

        auto uadd = createUAddOverflow(a, b);
        auto new_c = createExtractValue(uadd, {1});

        cur_vs[MCBB] = new_v;
        cur_cs[MCBB] = new_c;
        set_n(result);
        set_z(result);
        store(result);
      }

      store(createAdd(a, b));
      break;
    }
    case AArch64::ASRVWr:
    case AArch64::ASRVXr: {
      auto size = get_size(opcode);
      auto a = get_value(1);
      auto b = get_value(2);

      auto shift_amt = createBinop(b, intconst(size, size), Instruction::URem);
      auto res = createAShr(a, shift_amt);
      store(res);
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
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      assert(mc_inst.getNumOperands() == 4); // dst, lhs, rhs, shift amt
      assert(mc_inst.getOperand(3).isImm());

      // convert lhs, rhs operands to IR::Values
      auto a = get_value(1);
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
        b = get_value(2);

        // Make sure to not to trunc to the same size as the parameter.
        // Sometimes SUBrx is generated using 32 bit registers and "extends" to
        // a 32 bit value. This is seen as a type error by LLVM, but is valid
        // ARM
        if (extendSize != ty->getIntegerBitWidth()) {
          auto truncType = get_int_type(extendSize);
          b = createTrunc(b, truncType);
          b = createCast(b, ty,
                         isSigned ? Instruction::SExt : Instruction::ZExt);
        }

        // shift may not be there, it may just be the extend
        if (shift != 0)
          b = createShl(b, intconst(shift, size));
        break;
      }
      default:
        b = get_value(2, mc_inst.getOperand(3).getImm());
      }

      // make sure that lhs and rhs conversion succeeded, type lookup succeeded
      if (!ty || !a || !b)
        visitError(I);

      if (has_s(opcode)) {
        auto ssub = createSSubOverflow(a, b);
        auto result = createExtractValue(ssub, {0});
        auto new_v = createExtractValue(ssub, {1});
        cur_cs[MCBB] = createICmp(ICmpInst::Predicate::ICMP_UGE, a, b);
        cur_zs[MCBB] = createICmp(ICmpInst::Predicate::ICMP_EQ, a, b);
        cur_vs[MCBB] = new_v;
        set_n(result);
        store(result);
      } else {
        auto sub = createSub(a, b);
        store(sub);
      }
      break;
    }
    case AArch64::CSELWr:
    case AArch64::CSELXr: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      assert(mc_inst.getNumOperands() == 4); // dst, lhs, rhs, cond
      // TODO decode condition and find the approprate cond val
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isReg());
      assert(mc_inst.getOperand(3).isImm());

      auto a = get_value(1);
      auto b = get_value(2);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      if (!ty || !a || !b)
        visitError(I);

      auto result = createSelect(cond_val, a, b);
      store(result);
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
      auto size = get_size(opcode);
      Value *rhs = nullptr;
      if (mc_inst.getOperand(2).isImm()) {
        auto imm = decodeLogicalImmediate(mc_inst.getOperand(2).getImm(), size);
        rhs = intconst(imm, size);
      } else {
        rhs = get_value(2);
      }

      // We are in a ANDrs case. We need to handle a shift
      if (mc_inst.getNumOperands() == 4) {
        // the 4th operand (if it exists) must be an immediate
        assert(mc_inst.getOperand(3).isImm());
        rhs = reg_shift(rhs, mc_inst.getOperand(3).getImm());
      }

      auto and_op = createAnd(get_value(1), rhs);

      if (has_s(opcode)) {
        set_n(and_op);
        set_z(and_op);
        cur_cs[MCBB] = intconst(0, 1);
        cur_vs[MCBB] = intconst(0, 1);
      }

      store(and_op);
      break;
    }
    case AArch64::MADDWrrr:
    case AArch64::MADDXrrr: {
      auto mul_lhs = get_value(1, 0);
      auto mul_rhs = get_value(2, 0);
      auto addend = get_value(3, 0);

      auto mul = createMul(mul_lhs, mul_rhs);
      auto add = createAdd(mul, addend);
      store(add);
      break;
    }
    case AArch64::UMADDLrrr: {
      auto size = get_size(opcode);
      auto mul_lhs = get_value(1, 0);
      auto mul_rhs = get_value(2, 0);
      auto addend = get_value(3, 0);

      auto lhs_masked = createAnd(mul_lhs, intconst(0xffffffffUL, size));
      auto rhs_masked = createAnd(mul_rhs, intconst(0xffffffffUL, size));
      auto mul = createMul(lhs_masked, rhs_masked);
      auto add = createAdd(mul, addend);
      store(add);
      break;
    }
    case AArch64::SMADDLrrr: {
      // Signed Multiply-Add Long multiplies two 32-bit register values,
      // adds a 64-bit register value, and writes the result to the 64-bit
      // destination register.
      auto mul_lhs = get_value(1, 0);
      auto mul_rhs = get_value(2, 0);
      auto addend = get_value(3, 0);

      auto i32 = get_int_type(32);
      auto i64 = get_int_type(64);

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
      store(add);
      break;
    }
    case AArch64::SMSUBLrrr:
    case AArch64::UMSUBLrrr: {
      auto size = get_size(opcode);
      // SMSUBL: Signed Multiply-Subtract Long.
      // UMSUBL: Unsigned Multiply-Subtract Long.
      Value *mul_lhs = nullptr;
      Value *mul_rhs = nullptr;
      if (wrapper->getMCInst().getOperand(1).getReg() == AArch64::WZR) {
        mul_lhs = intconst(0, size);
      } else {
        mul_lhs = get_value(1, 0);
      }

      if (wrapper->getMCInst().getOperand(2).getReg() == AArch64::WZR) {
        mul_rhs = intconst(0, size);
      } else {
        mul_rhs = get_value(2, 0);
      }

      auto minuend = get_value(3, 0);
      auto i32 = get_int_type(32);
      auto i64 = get_int_type(64);

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
      store(subtract);
      break;
    }
    case AArch64::SMULHrr:
    case AArch64::UMULHrr: {
      // SMULH: Signed Multiply High
      // UMULH: Unsigned Multiply High
      auto mul_lhs = get_value(1, 0);
      auto mul_rhs = get_value(2, 0);

      auto i64 = get_int_type(64);
      auto i128 = get_int_type(128);

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
      auto shift = createLShr(mul, intconst(64, 128));

      // Truncate to the proper size:
      auto trunc = createTrunc(shift, i64);
      store(trunc);
      break;
    }
    case AArch64::MSUBWrrr:
    case AArch64::MSUBXrrr: {
      auto mul_lhs = get_value(1, 0);
      auto mul_rhs = get_value(2, 0);
      auto minuend = get_value(3, 0);
      auto mul = createMul(mul_lhs, mul_rhs);
      auto sub = createSub(minuend, mul);
      store(sub);
      break;
    }
    case AArch64::SBFMWri:
    case AArch64::SBFMXri: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      auto src = get_value(1);
      auto immr = mc_inst.getOperand(2).getImm();
      auto imms = mc_inst.getOperand(3).getImm();

      auto r = intconst(immr, size);
      //      auto s = intconst(imms, size);

      // arithmetic shift right (ASR) alias is perferred when:
      // imms == 011111 and size == 32 or when imms == 111111 and size = 64
      if ((size == 32 && imms == 31) || (size == 64 && imms == 63)) {
        auto dst = createAShr(src, r);
        store(dst);
        return;
      }

      // SXTB
      if (immr == 0 && imms == 7) {
        auto i8 = get_int_type(8);
        auto trunc = createTrunc(src, i8);
        auto dst = createSExt(trunc, ty);
        store(dst);
        return;
      }

      // SXTH
      if (immr == 0 && imms == 15) {
        auto i16 = get_int_type(16);
        auto trunc = createTrunc(src, i16);
        auto dst = createSExt(trunc, ty);
        store(dst);
        return;
      }

      // SXTW
      if (immr == 0 && imms == 31) {
        auto i32 = get_int_type(32);
        auto trunc = createTrunc(src, i32);
        auto dst = createSExt(trunc, ty);
        store(dst);
        return;
      }

      // SBFIZ
      if (imms < immr) {
        auto pos = size - immr;
        auto width = imms + 1;
        auto mask = ((uint64_t)1 << (width)) - 1;
        auto bitfield_mask = (uint64_t)1 << (width - 1);

        auto masked = createAnd(src, intconst(mask, size));
        auto bitfield_lsb = createAnd(src, intconst(bitfield_mask, size));
        auto insert_ones = createOr(masked, intconst(~mask, size));
        auto bitfield_lsb_set = createICmp(ICmpInst::Predicate::ICMP_NE,
                                           bitfield_lsb, intconst(0, size));
        auto res = createSelect(bitfield_lsb_set, insert_ones, masked);
        auto shifted_res = createShl(res, intconst(pos, size));
        store(shifted_res);
        return;
      }
      // FIXME: this requires checking if SBFX is preferred.
      // For now, assume this is always SBFX
      auto width = imms + 1;
      auto mask = ((uint64_t)1 << (width)) - 1;
      auto pos = immr;

      auto masked = createAnd(src, intconst(mask, size));
      auto l_shifted = createShl(masked, intconst(size - width, size));
      auto shifted_res =
          createAShr(l_shifted, intconst(size - width + pos, size));
      store(shifted_res);
      return;
    }
    case AArch64::CCMPWi:
    case AArch64::CCMPWr:
    case AArch64::CCMPXi:
    case AArch64::CCMPXr: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      assert(mc_inst.getNumOperands() == 4);

      auto lhs = get_value(0);
      auto imm_rhs = get_value(1);

      if (!ty || !lhs || !imm_rhs)
        visitError(I);

      auto imm_flags = mc_inst.getOperand(2).getImm();
      auto imm_v_val = intconst((imm_flags & 1) ? 1 : 0, 1);
      auto imm_c_val = intconst((imm_flags & 2) ? 1 : 0, 1);
      auto imm_z_val = intconst((imm_flags & 4) ? 1 : 0, 1);
      auto imm_n_val = intconst((imm_flags & 8) ? 1 : 0, 1);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      auto ssub = createSSubOverflow(lhs, imm_rhs);
      auto result = createExtractValue(ssub, {0});
      auto zero_val = intconst(0, result->getType()->getIntegerBitWidth());

      auto new_n = createICmp(ICmpInst::Predicate::ICMP_SLT, result, zero_val);
      auto new_z = createICmp(ICmpInst::Predicate::ICMP_EQ, lhs, imm_rhs);
      auto new_c = createICmp(ICmpInst::Predicate::ICMP_UGE, lhs, imm_rhs);
      auto new_v = createExtractValue(ssub, {1});

      auto new_n_flag = createSelect(cond_val, new_n, imm_n_val);
      auto new_z_flag = createSelect(cond_val, new_z, imm_z_val);
      auto new_c_flag = createSelect(cond_val, new_c, imm_c_val);
      auto new_v_flag = createSelect(cond_val, new_v, imm_v_val);
      store(new_n_flag);
      cur_ns[MCBB] = new_n_flag;
      store(new_z_flag);
      cur_zs[MCBB] = new_z_flag;
      store(new_c_flag);
      cur_cs[MCBB] = new_c_flag;
      store(new_v_flag);
      cur_vs[MCBB] = new_v_flag;
      break;
    }
    case AArch64::EORWri:
    case AArch64::EORXri: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      assert(mc_inst.getNumOperands() == 3); // dst, src, imm
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isImm());

      auto a = get_value(1);
      auto decoded_immediate =
          decodeLogicalImmediate(mc_inst.getOperand(2).getImm(), size);
      auto imm_val = intconst(decoded_immediate,
                              size); // FIXME, need to decode immediate val
      if (!ty || !a || !imm_val)
        visitError(I);

      auto res = createXor(a, imm_val);
      store(res);
      break;
    }
    case AArch64::EORWrs:
    case AArch64::EORXrs: {
      auto lhs = get_value(1);
      auto rhs = get_value(2, mc_inst.getOperand(3).getImm());
      auto result = createXor(lhs, rhs);
      store(result);
      break;
    }
    case AArch64::CSINVWr:
    case AArch64::CSINVXr:
    case AArch64::CSNEGWr:
    case AArch64::CSNEGXr: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      // csinv dst, a, b, cond
      // if (cond) a else ~b
      assert(mc_inst.getNumOperands() == 4); // dst, lhs, rhs, cond
      // TODO decode condition and find the approprate cond val
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isReg());
      assert(mc_inst.getOperand(3).isImm());

      auto a = get_value(1);
      auto b = get_value(2);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      if (!ty || !a || !b)
        visitError(I);

      auto neg_one = intconst(-1, size);
      auto inverted_b = createXor(b, neg_one);

      if (opcode == AArch64::CSNEGWr || opcode == AArch64::CSNEGXr) {
        auto negated_b = createAdd(inverted_b, intconst(1, size));
        auto ret = createSelect(cond_val, a, negated_b);
        store(ret);
        break;
      }

      auto ret = createSelect(cond_val, a, inverted_b);
      store(ret);
      break;
    }
    case AArch64::CSINCWr:
    case AArch64::CSINCXr: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      assert(mc_inst.getOperand(1).isReg() && mc_inst.getOperand(2).isReg());
      assert(mc_inst.getOperand(3).isImm());

      auto a = get_value(1);
      auto b = get_value(2);

      auto cond_val_imm = mc_inst.getOperand(3).getImm();
      auto cond_val = evaluate_condition(cond_val_imm, MCBB);

      auto inc = createAdd(b, intconst(1, ty->getIntegerBitWidth()));
      auto sel = createSelect(cond_val, a, inc);

      store(sel);
      break;
    }
    case AArch64::MOVZWi:
    case AArch64::MOVZXi: {
      auto size = get_size(opcode);
      assert(mc_inst.getOperand(0).isReg());
      assert(mc_inst.getOperand(1).isImm());
      auto lhs = get_value(1, mc_inst.getOperand(2).getImm());
      auto rhs = intconst(0, size);
      auto ident = createAdd(lhs, rhs);
      store(ident);
      break;
    }
    case AArch64::MOVNWi:
    case AArch64::MOVNXi: {
      auto size = get_size(opcode);
      assert(mc_inst.getOperand(0).isReg());
      assert(mc_inst.getOperand(1).isImm());
      assert(mc_inst.getOperand(2).isImm());

      auto lhs = get_value(1, mc_inst.getOperand(2).getImm());
      auto neg_one = intconst(-1, size);
      auto not_lhs = createXor(lhs, neg_one);

      store(not_lhs);
      break;
    }
    case AArch64::LSLVWr:
    case AArch64::LSLVXr: {
      auto size = get_size(opcode);
      auto zero = intconst(0, size);
      auto lhs = get_value(1);
      auto rhs = get_value(2);
      auto exp = createFShl(lhs, zero, rhs);
      store(exp);
      break;
    }
    case AArch64::LSRVWr:
    case AArch64::LSRVXr: {
      auto size = get_size(opcode);
      auto zero = intconst(0, size);
      auto lhs = get_value(1);
      auto rhs = get_value(2);
      auto exp = createFShr(zero, lhs, rhs);
      store(exp);
      break;
    }
    case AArch64::ORNWrs:
    case AArch64::ORNXrs: {
      auto size = get_size(opcode);
      auto lhs = get_value(1);
      auto rhs = get_value(2, mc_inst.getOperand(3).getImm());

      auto neg_one = intconst(-1, size);
      auto not_rhs = createXor(rhs, neg_one);
      auto ident = createOr(lhs, not_rhs);
      store(ident);
      break;
    }
    case AArch64::MOVKWi:
    case AArch64::MOVKXi: {
      auto size = get_size(opcode);
      auto dest = get_value(1);
      auto lhs = get_value(2, mc_inst.getOperand(3).getImm());

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

      auto bottom_bits = intconst(bitmask, size);
      auto cleared = createAnd(dest, bottom_bits);
      auto ident = createOr(cleared, lhs);
      store(ident);
      break;
    }
    case AArch64::UBFMWri:
    case AArch64::UBFMXri: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      auto src = get_value(1);
      auto immr = mc_inst.getOperand(2).getImm();
      auto imms = mc_inst.getOperand(3).getImm();

      // LSL is preferred when imms != 31 and imms + 1 == immr
      if (size == 32 && imms != 31 && imms + 1 == immr) {
        auto dst = createShl(src, intconst(31 - imms, size));
        store(dst);
        return;
      }

      // LSL is preferred when imms != 63 and imms + 1 == immr
      if (size == 64 && imms != 63 && imms + 1 == immr) {
        auto dst = createShl(src, intconst(63 - imms, size));
        store(dst);
        return;
      }

      // LSR is preferred when imms == 31 or 63 (size - 1)
      if (imms == size - 1) {
        auto dst = createLShr(src, intconst(immr, size));
        store(dst);
        return;
      }

      // UBFIZ
      if (imms < immr) {
        auto pos = size - immr;
        auto width = imms + 1;
        auto mask = ((uint64_t)1 << (width)) - 1;
        auto masked = createAnd(src, intconst(mask, size));
        auto shifted = createShl(masked, intconst(pos, size));
        store(shifted);
        return;
      }

      // UXTB
      if (immr == 0 && imms == 7) {
        auto mask = ((uint64_t)1 << 8) - 1;
        auto masked = createAnd(src, intconst(mask, size));
        auto zexted = createZExt(masked, ty);
        store(zexted);
        // assert(false && "UXTB not supported");
        return;
      }
      // UXTH
      if (immr == 0 && imms == 15) {
        assert(false && "UXTH not supported");
        return;
      }

      // UBFX
      // FIXME: this requires checking if UBFX is preferred.
      // For now, assume this is always UBFX
      // we mask from lsb to lsb + width and then perform a logical shift right
      auto width = imms + 1;
      auto mask = ((uint64_t)1 << (width)) - 1;
      auto pos = immr;

      auto masked = createAnd(src, intconst(mask, size));
      auto shifted_res = createLShr(masked, intconst(pos, size));
      store(shifted_res);
      return;
      // assert(false && "UBFX not supported");
    }
    case AArch64::BFMWri:
    case AArch64::BFMXri: {
      auto size = get_size(opcode);
      auto dst = get_value(1);
      auto src = get_value(2);

      auto immr = mc_inst.getOperand(3).getImm();
      auto imms = mc_inst.getOperand(4).getImm();

      if (imms >= immr) {
        auto bits = (imms - immr + 1);
        auto pos = immr;

        auto mask = (((uint64_t)1 << bits) - 1) << pos;

        auto masked = createAnd(src, intconst(mask, size));
        auto shifted = createLShr(masked, intconst(pos, size));
        auto cleared = createAnd(dst, intconst((uint64_t)(-1) << bits, size));
        auto res = createOr(cleared, shifted);
        store(res);
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
      auto bitfield = createAnd(src, intconst(~((uint64_t)-1 << bits), size));

      // move the bitfield into position
      auto moved = createShl(bitfield, intconst(pos, size));

      // carve out a place for the bitfield
      auto masked = createAnd(dst, intconst(mask, size));
      // place the bitfield
      auto res = createOr(masked, moved);
      store(res);
      return;
    }
    case AArch64::ORRWri:
    case AArch64::ORRXri: {
      auto size = get_size(opcode);
      auto lhs = get_value(1);
      auto imm = mc_inst.getOperand(2).getImm();
      auto decoded = decodeLogicalImmediate(imm, size);
      auto result = createOr(lhs, intconst(decoded, size));
      store(result);
      break;
    }
    case AArch64::ORRWrs:
    case AArch64::ORRXrs: {
      auto lhs = get_value(1);
      auto rhs = get_value(2, mc_inst.getOperand(3).getImm());
      auto result = createOr(lhs, rhs);
      store(result);
      break;
    }
    case AArch64::SDIVWr:
    case AArch64::SDIVXr: {
      auto lhs = get_value(1);
      auto rhs = get_value(2);
      auto result = createSDiv(lhs, rhs);
      store(result);
      break;
    }
    case AArch64::UDIVWr:
    case AArch64::UDIVXr: {
      auto lhs = get_value(1);
      auto rhs = get_value(2);
      auto result = createUDiv(lhs, rhs);
      store(result);
      break;
    }
    case AArch64::EXTRWrri:
    case AArch64::EXTRXrri: {
      auto op1 = get_value(1);
      auto op2 = get_value(2);
      auto shift = get_value(3);
      auto result = createFShr(op1, op2, shift);
      store(result);
      break;
    }
    case AArch64::RORVWr:
    case AArch64::RORVXr: {
      auto op = get_value(1);
      auto shift = get_value(2);
      auto result = createFShr(op, op, shift);
      store(result);
      break;
    }
    case AArch64::RBITWr:
    case AArch64::RBITXr: {
      auto op = get_value(1);
      auto result = createBitReverse(op);
      store(result);
      break;
    }
    case AArch64::REVWr:
    case AArch64::REVXr: {
      auto op = get_value(1);
      auto result = createBSwap(op);
      store(result);
      break;
    }
    case AArch64::CLZWr:
    case AArch64::CLZXr: {
      auto op = get_value(1);
      auto result = createCtlz(op);
      store(result);
      break;
    }
    case AArch64::EONWrs:
    case AArch64::EONXrs:
    case AArch64::BICWrs:
    case AArch64::BICXrs:
    case AArch64::BICSWrs:
    case AArch64::BICSXrs: {
      auto size = get_size(opcode);
      // BIC:
      // return = op1 AND NOT (optional shift) op2
      // EON:
      // return = op1 XOR NOT (optional shift) op2

      auto op1 = get_value(1);
      auto op2 = get_value(2);

      // If there is a shift to be performed on the second operand
      if (mc_inst.getNumOperands() == 4) {
        // the 4th operand (if it exists) must b an immediate
        assert(mc_inst.getOperand(3).isImm());
        op2 = reg_shift(op2, mc_inst.getOperand(3).getImm());
      }

      // Perform NOT
      auto neg_one = intconst(-1, size);
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
        set_n(ret);
        set_z(ret);
        cur_cs[MCBB] = intconst(0, 1);
        cur_vs[MCBB] = intconst(0, 1);
      }

      store(ret);
      break;
    }
    case AArch64::REV16Xr: {
      // REV16Xr: Reverse bytes of 64 bit value in 16-bit half-words.
      auto size = get_size(opcode);
      auto val = get_value(1);
      auto first_part = createShl(val, intconst(8, size));
      auto first_part_and =
          createAnd(first_part, intconst(0xFF00FF00FF00FF00UL, size));
      auto second_part = createLShr(val, intconst(8, size));
      auto second_part_and =
          createAnd(second_part, intconst(0x00FF00FF00FF00FFUL, size));
      auto combined_val = createOr(first_part_and, second_part_and);
      store(combined_val);
      break;
    }
    case AArch64::REV16Wr:
    case AArch64::REV32Xr: {
      // REV16Wr: Reverse bytes of 32 bit value in 16-bit half-words.
      // REV32Xr: Reverse bytes of 64 bit value in 32-bit words.
      auto size = get_size(opcode);
      auto val = get_value(1);

      // Reversing all of the bytes, then performing a rotation by half the
      // width reverses bytes in 16-bit halfwords for a 32 bit int and reverses
      // bytes in a 32-bit word for a 64 bit int
      auto reverse_val = createBSwap(val);
      auto ret = createFShr(reverse_val, reverse_val, intconst(size / 2, size));
      store(ret);
      break;
    }
    // assuming that the source is always an x register
    // This might not be the case but we need to look at the assembly emitter
    case AArch64::FMOVXDr: {
      // zero extended x register to 128 bits
      auto val = get_value(1);
      const auto &cur_mc_instr = wrapper->getMCInst();
      auto &op_0 = cur_mc_instr.getOperand(0);
      auto q_reg_val = cur_vol_regs[MCBB][op_0.getReg()];
      assert(q_reg_val && "volatile register's value not in cache");

      // zero out the bottom 64-bits of the vector register by shifting
      auto q_shift = createLShr(q_reg_val, intconst(64, 128));
      auto q_cleared = createShl(q_shift, intconst(64, 128));
      auto mov_res = createOr(q_cleared, val);
      store(mov_res);
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
      auto val = get_value(3);
      auto q_reg_val = cur_vol_regs[MCBB][op_0.getReg()];
      // FIXME make this a utility function that uses index and size
      auto mask = intconst(-1, 128);

      if (op_index > 0) {
        mask = createShl(mask, intconst(64, 128));
        val = createShl(val, intconst(64, 128));
      }

      auto q_cleared = createShl(q_reg_val, mask);
      auto mov_res = createAnd(q_cleared, val);
      store(mov_res);
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
      auto stack_gep = getIdentifier(AArch64::SP, op_0.getReg());
      assert(stack_gep && "loaded identifier should not be null!");
      auto loaded_val = createLoad(get_int_type(64), stack_gep);
      store(loaded_val);
      break;
    }
    case AArch64::RET: {
      // for now we're assuming that the function returns an integer or void
      // value
      auto retTy = srcFn.getReturnType();
      if (auto *vecRetTy = dyn_cast<VectorType>(retTy)) {
        cout << "returning vector type\n";
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
        //  cout << "reg num=" << reg << "\n";
        //  if (reg > largest_vect_register) {
        //    largest_vect_register = reg;
        //  }
        //}
        //
        // cout << "largest vect register=" << largest_vect_register-AArch64::Q0
        // << "\n";

        auto elem_ret_typ = get_int_type(elem_bitwidth);
        // need to trunc if the element's bitwidth is less than 128
        if (elem_bitwidth < 128) {
          for (unsigned i = 0; i < elts; ++i) {
            auto vect_reg_val = cur_vol_regs[MCBB][AArch64::Q0 + i];
            assert(vect_reg_val && "register value to return cannot be null!");
            auto trunc = createTrunc(vect_reg_val, elem_ret_typ);
            vec = createInsertElement(vec, trunc, intconst(i, 32));
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
          cout << "return width = " << retWidth << endl;
          auto val =
              getIdentifier(mc_inst.getOperand(0).getReg(), I.getOpId(0));
          if (val) {
            if (retWidth < val->getType()->getIntegerBitWidth())
              val = createTrunc(val, get_int_type(retWidth));

            // for don't care bits we need to mask them off before returning
            if (has_ret_attr && (orig_ret_bitwidth < 32)) {
              assert(retWidth >= orig_ret_bitwidth);
              assert(retWidth == 64);
              auto trunc = createTrunc(val, get_int_type(32));
              val = createZExt(trunc, get_int_type(64));
            }
            createReturn(val);
          } else {
            // Hacky solution to deal with functions where the assembly
            // is just a ret instruction
            cout << "hack: returning poison" << endl;
            createReturn(PoisonValue::get(get_int_type(retWidth)));
          }
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

      auto dst_ptr = get_basic_block(Fn, mc_inst.getOperand(0));
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
      cout << "bcc target: " << Sym.getName().str() << '\n';
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
      auto operand = get_value(0);
      assert(operand != nullptr && "operand is null");
      auto cond_val =
          createICmp(ICmpInst::Predicate::ICMP_EQ, operand,
                     intconst(0, operand->getType()->getIntegerBitWidth()));
      auto dst_true = get_basic_block(Fn, mc_inst.getOperand(1));
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
      auto operand = get_value(0);
      assert(operand != nullptr && "operand is null");
      auto cond_val =
          createICmp(ICmpInst::Predicate::ICMP_NE, operand,
                     intconst(0, operand->getType()->getIntegerBitWidth()));

      auto dst_true = get_basic_block(Fn, mc_inst.getOperand(1));
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
      auto size = get_size(opcode);
      auto operand = get_value(0);
      assert(operand != nullptr && "operand is null");
      auto bit_pos = mc_inst.getOperand(1).getImm();
      auto shift = createLShr(operand, intconst(bit_pos, size));
      auto cond_val = createTrunc(shift, get_int_type(1));

      auto &jmp_tgt_op = mc_inst.getOperand(2);
      assert(jmp_tgt_op.isExpr() && "expected expression");
      assert((jmp_tgt_op.getExpr()->getKind() == MCExpr::ExprKind::SymbolRef) &&
             "expected symbol ref as bcc operand");
      const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*jmp_tgt_op.getExpr());
      const MCSymbol &Sym =
          SRE.getSymbol(); // FIXME refactor this into a function
      auto *dst_false = getBBByName(Fn, Sym.getName());

      cout << "current mcblock = " << MCBB->getName() << endl;
      cout << "Curr BB=" << CurrBB->getName().str() << endl;
      cout << "jump target = " << Sym.getName().str() << endl;
      assert(MCBB->getSuccs().size() == 2 && "expected 2 successors");

      const string *dst_true_name;
      for (auto &succ : MCBB->getSuccs()) {
        if (succ->getName() != Sym.getName()) {
          dst_true_name = &succ->getName();
          break;
        }
      }
      auto *dst_true = getBBByName(Fn, *dst_true_name);

      switch (opcode) {
      case AArch64::TBNZW:
      case AArch64::TBNZX:
        createBranch(cond_val, dst_false, dst_true);
        break;
      default:
        createBranch(cond_val, dst_true, dst_false);
      }

      break;
    }
    case AArch64::PHI: {
      auto size = get_size(opcode);
      auto ty = get_int_type(size);
      auto result = createPhi(ty);
      cout << "pushing phi in todo : " << endl;
      wrapper->print();
      auto p = make_pair(result, wrapper);
      lift_todo_phis.push_back(p);
      store(result);
      break;
    }
    default:
      Fn.print(errs());
      cout << "\nError "
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

    cout << "about to start creating basic blocks.\n";
    for (auto v : util::top_sort(edges)) {
      cout << "creating a basic block called " << bbs[v]->getName() << "\n";
      auto bb = BasicBlock::Create(LLVMCtx, bbs[v]->getName(), &Fn);
      sorted_bbs.emplace_back(bb, bbs[v]);
    }
    cout << "done creating basic blocks.\n";

    // setup BB so subsequent add_instr calls work
    CurrBB = sorted_bbs[0].first;
  }

  Function *run() {
    auto Fn = Function::Create(srcFn.getFunctionType(), GlobalValue::ExternalLinkage, 0,
                               MF.getName(), LiftedModule);

    cout << "function name: '" << MF.getName() << "'" << endl;

    vector<pair<BasicBlock *, MCBasicBlock *>> sorted_bbs;
    createBBs(sorted_bbs, *Fn);

    unsigned argNum = 0;
    unsigned idx = 0;
    for (auto &Arg : Fn->args()) {
      // generate names and values for the input arguments
      // FIXME this is pretty convoluted and needs to be cleaned up
      auto operand = MCOperand::createReg(AArch64::X0 + argNum);

      Value *stored = createFreeze(&Arg, next_name(operand.getReg(), 1));

      mc_add_identifier(operand.getReg(), 1, &Arg);
      assert(Arg.getType()->getIntegerBitWidth() == 64 &&
             "at this point input type should be 64 bits");

      if (!new_input_idx_bitwidth.empty() &&
          (argNum == new_input_idx_bitwidth[idx].first)) {
        auto op = Instruction::ZExt;
        if (Arg.hasSExtAttr())
          op = Instruction::SExt;

        auto truncated_type = get_int_type(new_input_idx_bitwidth[idx].second);
        stored =
            createTrunc(stored, truncated_type, next_name(operand.getReg(), 2));
        auto extended_type = get_int_type(64);
        if (truncated_type->getIntegerBitWidth() == 1) {
          stored = createCast(stored, get_int_type(32), op,
                              next_name(operand.getReg(), 3));
          stored =
              createZExt(stored, extended_type, next_name(operand.getReg(), 4));
        } else {
          if (truncated_type->getIntegerBitWidth() < 32) {
            stored = createCast(stored, get_int_type(32), op,
                                next_name(operand.getReg(), 3));
            stored = createZExt(stored, extended_type,
                                next_name(operand.getReg(), 4));
          } else {
            stored = createCast(stored, extended_type, op,
                                next_name(operand.getReg(), 4));
          }
        }
        idx++;
      }
      instructionCount++;
      mc_add_identifier(operand.getReg(), 2, stored);
      argNum++;
    }
    cout << "created non-vector args" << endl;

    // FIXME: Hacky way of supporting parameters passed via the stack
    // need to properly model the parameter passing rules described in
    // the spec
    if (argNum > 8) {
      auto num_stack_args = argNum - 8; // x0-x7 are passed via registers
      cout << "num_stack_args = " << num_stack_args << "\n";

      // add stack with 16 slots, 8 bytes each
      auto alloc_size = intconst(16, 64);
      auto ty = Type::getInt64Ty(LLVMCtx);
      auto alloca = createAlloca(ty, alloc_size, "stack");
      mc_add_identifier(AArch64::SP, 3, alloca);

      for (unsigned i = 0; i < num_stack_args; ++i) {
        unsigned reg_num = AArch64::X8 + i;
        cout << "reg_num = " << reg_num << "\n";

	vector<Value *> idxlist{ intconst(i, 64) };
	auto get_xi = createGEP(ty, alloca, idxlist, "stack_" + to_string(8 + i));
        // FIXME, need to use version similar to the offset to address the stack
        // pointer or alternatively a more elegant solution altogether
        mc_add_identifier(AArch64::SP, reg_num, get_xi);
	createStore(getIdentifier(reg_num, 2), get_xi);
      }
    }

    auto poison_val = PoisonValue::get(get_int_type(64));
    auto vect_poison_val = PoisonValue::get(get_int_type(128));
    cout << "argNum = " << argNum << "\n";
    cout << "entry mc_bb = " << sorted_bbs[0].second->getName() << "\n";
    auto entry_mc_bb = sorted_bbs[0].second;
    // add remaining volatile registers and set them to unitialized value
    // we chose frozen poison value to give a random yet determinate value
    // FIXME: using the number of arguments to the function to determine which
    // registers are uninitialized is hacky and will break when passing FP
    // arguments

    for (unsigned int i = AArch64::X0 + argNum; i <= AArch64::X17; ++i) {
      auto val = createOr(poison_val, intconst(0, 64), next_name(i, 3));
      auto val_frozen = createFreeze(val, next_name(i, 4));
      mc_add_identifier(i, 2, val_frozen);
    }

    for (unsigned int i = AArch64::Q0; i <= AArch64::Q3; ++i) {
      auto val = createOr(vect_poison_val, intconst(0, 128), next_name(i, 3));
      auto val_frozen = createFreeze(val, next_name(i, 4));
      mc_add_identifier(i, 2, val_frozen);
      cur_vol_regs[entry_mc_bb][i] = val_frozen;
    }

    for (auto &[llvm_bb, mc_bb] : sorted_bbs) {
      cout << "visiting bb: " << mc_bb->getName() << endl;
      CurrBB = llvm_bb;
      MCBB = mc_bb;
      auto &mc_instrs = mc_bb->getInstrs();

      for (auto &mc_instr : mc_instrs) {
        cout << "before visit\n";
        mc_instr.print();
        mc_visit(mc_instr, *Fn);
        cout << "after visit\n";
      }

      if (!CurrBB->getTerminator()) {
        assert(MCBB->getSuccs().size() == 1 &&
               "expected 1 successor for block with no terminator");
        auto *dst = getBBByName(*Fn, MCBB->getSuccs()[0]->getName());
        createBranch(dst);
      }

      blockCount++;
    }

    cout << "\n----------lifted-arm-target-missing-phi-params----------\n";
    Fn->print(outs());
    cout << "lift_todo_phis.size() = " << lift_todo_phis.size() << endl;

    int tmp_index = 0;
    for (auto &[phi, phi_mc_wrapper] : lift_todo_phis) {
      cout << "index = " << tmp_index
           << "opcode =" << phi_mc_wrapper->getOpcode() << endl;
      tmp_index++;
      add_phi_params(phi, phi_mc_wrapper);
    }
    return Fn;
  }
};

// Convert an MCFucntion to IR::Function
// Adapted from llvm2alive_ in llvm2alive.cpp with some simplifying assumptions
// FIXME for now, we are making a lot of simplifying assumptions like assuming
// types of arguments.
Function *arm2llvm(Module *OrigModule, MCFunction &MF,
                   Function &srcFn, MCInstPrinter *instrPrinter,
                   MCRegisterInfo *registerInfo) {
  return arm2llvm_(OrigModule, MF, srcFn, instrPrinter, registerInfo)
      .run();
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
  MCInstrAnalysis *Ana_ptr;
  MCInstPrinter *IP_ptr;
  MCRegisterInfo *MRI_ptr;

public:
  MCFunction MF;
  unsigned cnt{0};
  vector<MCInst>
      Insts; // CHECK this should go as it's only being used for pretty printing
             // which makes it unused after fixing MCInstWrapper::print
  using BlockSetTy = SetVector<MCBasicBlock *>;

  MCStreamerWrapper(MCContext &Context, MCInstrAnalysis *_Ana_ptr,
                    MCInstPrinter *_IP_ptr, MCRegisterInfo *_MRI_ptr)
      : MCStreamer(Context), Ana_ptr(_Ana_ptr), IP_ptr(_IP_ptr),
        MRI_ptr(_MRI_ptr) {
    MF.Ana_ptr = Ana_ptr;
    MF.IP_ptr = IP_ptr;
    MF.MRI_ptr = MRI_ptr;
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

    if (Ana_ptr->isTerminator(Inst)) {
      prev_line = ASMLine::terminator;
    } else {
      prev_line = ASMLine::non_term_instr;
    }
    auto &inst_ref = Cur_Inst.getMCInst();
    auto num_operands = inst_ref.getNumOperands();
    for (unsigned i = 0; i < num_operands; ++i) {
      auto op = inst_ref.getOperand(i);
      if (op.isExpr()) {
        auto expr = op.getExpr();
        if (expr->getKind() == MCExpr::ExprKind::SymbolRef) {
          const MCSymbolRefExpr &SRE = cast<MCSymbolRefExpr>(*expr);
          const MCSymbol &Sym = SRE.getSymbol();
          errs() << "target label : " << Sym.getName()
                 << ", offset=" << Sym.getOffset()
                 << '\n'; // FIXME remove when done
        }
      }
    }

    errs() << cnt++ << "  : ";
    Inst.dump_pretty(errs(), IP_ptr, " ", MRI_ptr);
    if (Ana_ptr->isBranch(Inst))
      errs() << ": branch ";
    if (Ana_ptr->isConditionalBranch(Inst))
      errs() << ": conditional branch ";
    if (Ana_ptr->isUnconditionalBranch(Inst))
      errs() << ": unconditional branch ";
    if (Ana_ptr->isTerminator(Inst))
      errs() << ": terminator ";
    errs() << "\n";
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
    errs() << cnt++ << "  : ";
    errs() << "inside Emit Label: symbol=" << Symbol->getName() << '\n';
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
      cout << curBlock.first->getName() << ": ";
      for (auto &dst : curBlock.second)
        cout << dst->getName() << " ";
      cout << "\n";
    }
  }

  // Only call after MF with Basicblocks is constructed to generate the
  // successors for each basic block
  void generateSuccessors() {
    cout << "generating basic block successors" << '\n';
    for (unsigned i = 0; i < MF.BBs.size(); ++i) {
      auto &cur_bb = MF.BBs[i];
      MCBasicBlock *next_bb_ptr = nullptr;
      if (i < MF.BBs.size() - 1)
        next_bb_ptr = &MF.BBs[i + 1];

      if (cur_bb.size() == 0) {
        cout
            << "generateSuccessors, encountered basic block with 0 instructions"
            << '\n';
        continue;
      }
      auto &last_mc_instr = cur_bb.getInstrs().back().getMCInst();
      // handle the special case of adding where we have added a new entry block
      // with no predecessors. This is hacky because I don't know the API to
      // create and MCExpr and have to create a branch with an immediate operand
      // instead
      if (i == 0 && (Ana_ptr->isUnconditionalBranch(last_mc_instr)) &&
          last_mc_instr.getOperand(0).isImm()) {
        cur_bb.addSucc(next_bb_ptr);
        continue;
      }
      if (Ana_ptr->isConditionalBranch(last_mc_instr)) {
        string target = findTargetLabel(last_mc_instr);
        auto target_bb = MF.findBlockByName(target);
        cur_bb.addSucc(target_bb);
        if (next_bb_ptr)
          cur_bb.addSucc(next_bb_ptr);
      } else if (Ana_ptr->isUnconditionalBranch(last_mc_instr)) {
        string target = findTargetLabel(last_mc_instr);
        auto target_bb = MF.findBlockByName(target);
        cur_bb.addSucc(target_bb);
      } else if (Ana_ptr->isReturn(last_mc_instr)) {
        continue;
      } else if (next_bb_ptr) {
        // add edge to next block
        cur_bb.addSucc(next_bb_ptr);
      }
    }
  }

  // Remove empty basic blocks from the machine function
  void removeEmptyBlocks() {
    cout << "removing empty basic blocks" << '\n';
    erase_if(MF.BBs, [](MCBasicBlock b) { return b.size() == 0; });
  }

  // Only call after generateSucessors() has been called
  // generate predecessors for each basic block in a MCFunction
  void generatePredecessors() {
    cout << "generating basic block predecessors" << '\n';
    for (auto &block : MF.BBs) {
      for (auto it = block.succBegin(); it != block.succEnd(); ++it) {
        auto successor = *it;
        successor->addPred(&block);
      }
    }
  }

  void printBlocksMF() {
    cout << "# of Blocks (MF print blocks) = " << MF.BBs.size() << '\n';
    cout << "-------------\n";
    int i = 0;
    for (auto &block : MF.BBs) {
      errs() << "block " << i << ", name= " << block.getName() << '\n';
      for (auto &inst : block.getInstrs()) {
        inst.getMCInst().dump_pretty(errs(), IP_ptr, " ", MRI_ptr);
        errs() << '\n';
      }
      i++;
    }
  }

  void printCFG() {
    cout << "printing arm function CFG" << '\n';
    cout << "successors" << '\n';
    for (auto &block : MF.BBs) {
      cout << block.getName() << ": [";
      for (auto it = block.succBegin(); it != block.succEnd(); ++it) {
        auto successor = *it;
        cout << successor->getName() << ", ";
      }
      cout << "]\n";
    }

    cout << "predecessors" << '\n';
    for (auto &block : MF.BBs) {
      cout << block.getName() << ": [";
      for (auto it = block.predBegin(); it != block.predEnd(); ++it) {
        auto predecessor = *it;
        cout << predecessor->getName() << ", ";
      }
      cout << "]\n";
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

    cout << "After adjusting return:\n";
    for (auto &b : MF.BBs) {
      cout << b.getName() << ":\n";
      b.print();
    }
  }
};

// Return variables that are read before being written in the basic block
[[maybe_unused]] auto FindReadBeforeWritten(vector<MCInst> &instrs,
                                            MCInstrAnalysis *Ana_ptr) {
  unordered_set<MCOperand, MCOperandHash, MCOperandEqual> reads;
  unordered_set<MCOperand, MCOperandHash, MCOperandEqual> writes;
  // TODO for writes, should only apply to instructions that update a
  // destination register
  for (auto &I : instrs) {
    if (Ana_ptr->isReturn(I))
      continue;
    assert(I.getNumOperands() > 0 && "MCInst with zero operands");
    for (unsigned j = 1; j < I.getNumOperands(); ++j) {
      if (!writes.contains(I.getOperand(j)))
        reads.insert(I.getOperand(j));
    }
    writes.insert(I.getOperand(0));
  }

  return reads;
}

// Return variable that are read before being written in the basicblock
[[maybe_unused]] auto FindReadBeforeWritten(MCBasicBlock &block,
                                            MCInstrAnalysis *Ana_ptr) {
  auto mcInstrs = block.getInstrs();
  unordered_set<MCOperand, MCOperandHash, MCOperandEqual> reads;
  unordered_set<MCOperand, MCOperandHash, MCOperandEqual> writes;
  // TODO for writes, should only apply to instructions that update a
  // destination register
  for (auto &WI : mcInstrs) {
    auto &I = WI.getMCInst();
    if (Ana_ptr->isReturn(I))
      continue;
    assert(I.getNumOperands() > 0 && "MCInst with zero operands");
    for (unsigned j = 1; j < I.getNumOperands(); ++j) {
      if (!writes.contains(I.getOperand(j)))
        reads.insert(I.getOperand(j));
    }
    writes.insert(I.getOperand(0));
  }

  return reads;
}

Function *adjustSrcInputs(Function *srcFn) {
  vector<Type *> new_argtypes;
  unsigned idx = 0;

  for (auto &v : srcFn->args()) {
    auto *ty = v.getType();    
    if (!ty->isIntegerTy()) // FIXME
      report_fatal_error("[Unsupported Function Argument]: Only int types "
                         "supported for now");
    auto orig_width = ty->getIntegerBitWidth();
    if (orig_width > 64) // FIXME
      report_fatal_error("[Unsupported Function Argument]: Only int types 64 "
                         "bits or smaller supported for now");
    if (orig_width < 64) {
      new_input_idx_bitwidth.emplace_back(idx, orig_width);
    }
    new_argtypes.emplace_back(Type::getIntNTy(srcFn->getContext(), 64));
    // FIXME Do we need to update the value_cache?
    idx++;
  }

  FunctionType *NFTy = FunctionType::get(srcFn->getReturnType(), new_argtypes, false);
  Function *NF = Function::Create(NFTy, srcFn->getLinkage(), srcFn->getAddressSpace(),
                                  srcFn->getName(), srcFn->getParent());
  NF->copyAttributesFrom(srcFn);
  // FIXME -- copy over argument attributes
  NF->splice(NF->begin(), srcFn);
  NF->takeName(srcFn);
  for (Function::arg_iterator I = srcFn->arg_begin(), E = srcFn->arg_end(),
         I2 = NF->arg_begin();
       I != E; ++I, ++I2) {
    if (I->getType()->getIntegerBitWidth() < 64) {
      auto name = I->getName().substr(I->getName().rfind('%')) + "_t";
      auto trunc = new TruncInst(I2,
                                 I->getType(),
                                 name,
                                 NF->getEntryBlock().getFirstNonPHI());
      I->replaceAllUsesWith(trunc);      
    } else {
      I->replaceAllUsesWith(&*I2);
    }
  }

  // FIXME -- doesn't matter if we're just dealing with one function,
  // but if we're lifting modules with important calls, we need to
  // replace uses of the function with NF, see code in
  // DeadArgumentElimination.cpp
  
  srcFn->eraseFromParent();
  return NF;
}

Function *adjustSrcReturn(Function *srcFn) {
  // FIXME -- there's some work to be done here for checking zeroext
  
  if (!srcFn->hasRetAttribute(Attribute::SExt))
    return srcFn;

  auto *ret_typ = srcFn->getReturnType();
  orig_ret_bitwidth = ret_typ->getIntegerBitWidth();
  cout << "original return bitwidth = " << orig_ret_bitwidth << endl;

  // FIXME
  if (!ret_typ->isIntegerTy())
    report_fatal_error("[Unsupported Function Return]: Only int types "
                       "supported for now");

  // FIXME
  if (orig_ret_bitwidth > 64)
    report_fatal_error("[Unsupported Function Return]: Only int types 64 "
                       "bits or smaller supported for now");

  // don't need to do any extension if the return type is exactly 32 bits
  if (orig_ret_bitwidth == 64 || orig_ret_bitwidth == 32)
    return srcFn;

  // starting here we commit to returning a copy instead of the
  // original function
  
  has_ret_attr = true;
  auto *i32ty = Type::getIntNTy(srcFn->getContext(), 32);
  auto *i64ty = Type::getIntNTy(srcFn->getContext(), 64);

  // build this first to avoid iterator invalidation
  vector<ReturnInst *> RIs;  
  for (auto &BB : *srcFn)
    for (auto &I : BB)
      if (auto *RI = dyn_cast<ReturnInst>(&I))
        RIs.push_back(RI);

  for (auto *RI : RIs) {
    auto retVal = RI->getReturnValue();
    if (orig_ret_bitwidth < 32) {
      auto sext = new SExtInst(retVal, i32ty,
                               retVal->getName() + "_sext",
                               RI);
      auto zext = new ZExtInst(sext, i64ty,
                               retVal->getName() + "_zext",
                               RI);
      ReturnInst::Create(srcFn->getContext(),
                         zext, RI);
    } else {
      auto sext = new SExtInst(retVal, i64ty,
                               retVal->getName() + "_sext",
                               RI);
      ReturnInst::Create(srcFn->getContext(),
                         sext, RI);
    }
    RI->eraseFromParent();
  }

  // FIXME this is duplicate code, factor it out
  FunctionType *NFTy = FunctionType::get(i64ty,
                                        srcFn->getFunctionType()->params(), false);
  Function *NF = Function::Create(NFTy, srcFn->getLinkage(), srcFn->getAddressSpace(),
                                  srcFn->getName(), srcFn->getParent());
  NF->copyAttributesFrom(srcFn);
  // FIXME -- copy over argument attributes
  NF->splice(NF->begin(), srcFn);
  NF->takeName(srcFn);
  for (Function::arg_iterator I = srcFn->arg_begin(), E = srcFn->arg_end(),
         I2 = NF->arg_begin();
       I != E; ++I, ++I2)
    I->replaceAllUsesWith(&*I2);

  // FIXME -- if we're lifting modules with important calls, we need to replace
  // uses of the function with NF, see code in DeadArgumentElimination.cpp
  
  srcFn->eraseFromParent();
  return NF;
}

} // namespace

pair<Function *, Function *> lift_func(Module &OrigModule, Module &LiftedModule, bool asm_input,
                                       string opt_file2, bool opt_asm_only,
                                       Function *srcFn) {

  // FIXME -- both adjustSrcInputs and adjustSrcReturn create an
  // entirely new function, this is slow and not elegant, probably
  // merge these together
  outs() << "\n---------- src.ll ----------\n";
  srcFn->print(outs());
  srcFn = adjustSrcInputs(srcFn);
  outs() << "\n---------- src.ll ---- changed-input -\n";
  srcFn->print(outs());
  srcFn = adjustSrcReturn(srcFn);
  outs() << "\n---------- src.ll ---- changed-return -\n";
  srcFn->print(outs());

  LLVMInitializeAArch64TargetInfo();
  LLVMInitializeAArch64Target();
  LLVMInitializeAArch64TargetMC();
  LLVMInitializeAArch64AsmParser();
  LLVMInitializeAArch64AsmPrinter();

  string Error;
  const char *TripleName = "aarch64-arm-none-eabi";
  auto Target = TargetRegistry::lookupTarget(TripleName, Error);
  if (!Target) {
    cerr << Error;
    exit(-1);
  }
  TargetOptions Opt;
  const char *CPU = "apple-a12";
  auto RM = optional<Reloc::Model>();
  unique_ptr<TargetMachine> TM(
      Target->createTargetMachine(TripleName, CPU, "", Opt, RM));
  SmallString<1024> Asm;
  raw_svector_ostream Dest(Asm);

  legacy::PassManager pass;
  if (TM->addPassesToEmitFile(pass, Dest, nullptr, CGFT_AssemblyFile)) {
    cerr << "Failed to generate assembly";
    exit(-1);
  }
  pass.run(OrigModule);

  // FIXME only do this in verbose mode, or something
  cout << "\n----------arm asm----------\n\n";
  for (size_t i = 0; i < Asm.size(); ++i)
    cout << Asm[i];
  cout << "-------------\n";
  cout << "\n\n";
  Triple TheTriple(TripleName);

  auto MCOptions = mc::InitMCTargetOptionsFromFlags();
  unique_ptr<MCRegisterInfo> MRI(Target->createMCRegInfo(TripleName));
  assert(MRI && "Unable to create target register info!");

  unique_ptr<MCAsmInfo> MAI(
      Target->createMCAsmInfo(*MRI, TripleName, MCOptions));
  assert(MAI && "Unable to create MC asm info!");

  unique_ptr<MCSubtargetInfo> STI(
      Target->createMCSubtargetInfo(TripleName, CPU, ""));
  assert(STI && "Unable to create subtarget info!");
  assert(STI->isCPUStringValid(CPU) && "Invalid CPU!");

  MCContext Ctx(TheTriple, MAI.get(), MRI.get(), STI.get());
  SourceMgr SrcMgr;

  if (asm_input) {
    ExitOnError ExitOnErr;
    auto MB_Asm =
        ExitOnErr(errorOrToExpected(MemoryBuffer::getFile(opt_file2)));
    assert(MB_Asm);
    cout << "reading asm from file\n";
    for (auto it = MB_Asm->getBuffer().begin(); it != MB_Asm->getBuffer().end();
         ++it) {
      cout << *it;
    }
    cout << "-------------\n";
    SrcMgr.AddNewSourceBuffer(std::move(MB_Asm), SMLoc());
  } else {
    auto Buf = MemoryBuffer::getMemBuffer(Asm.c_str());
    SrcMgr.AddNewSourceBuffer(std::move(Buf), SMLoc());
  }

  unique_ptr<MCInstrInfo> MCII(Target->createMCInstrInfo());
  assert(MCII && "Unable to create instruction info!");

  unique_ptr<MCInstPrinter> IPtemp(
      Target->createMCInstPrinter(TheTriple, 0, *MAI, *MCII, *MRI));

  auto Ana = make_unique<MCInstrAnalysis>(MCII.get());

  MCStreamerWrapper MCSW(Ctx, Ana.get(), IPtemp.get(), MRI.get());

  unique_ptr<MCAsmParser> Parser(createMCAsmParser(SrcMgr, Ctx, MCSW, *MAI));
  assert(Parser);

  MCTargetOptions Opts;
  Opts.PreserveAsmComments = false;
  unique_ptr<MCTargetAsmParser> TAP(
      Target->createMCAsmParser(*STI, *Parser, *MCII, Opts));
  assert(TAP);
  Parser->setTargetParser(*TAP);
  Parser->Run(true); // ??

  // FIXME remove printing of the mcInsts
  // For now, print the parsed instructions for debug puropses
  cout << "\n\nPretty Parsed MCInsts:\n";
  for (auto I : MCSW.Insts) {
    I.dump_pretty(errs(), IPtemp.get(), " ", MRI.get());
    errs() << '\n';
  }

  cout << "\n\nParsed MCInsts:\n";
  for (auto I : MCSW.Insts) {
    I.dump_pretty(errs());
    errs() << '\n';
  }

  if (opt_asm_only) {
    cout << "arm instruction count = " << MCSW.Insts.size() << "\n";
    cout.flush();
    errs().flush();
    cerr.flush();
    exit(0);
  }

  cout << "\n\n";

  if (srcFn->isVarArg())
    report_fatal_error("Varargs not supported");

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

  cout << "after SSA conversion\n";
  MCSW.printBlocksMF();

  return make_pair(srcFn, arm2llvm(&LiftedModule, MCSW.MF, *srcFn, IPtemp.get(),
                                       MRI.get()));
}
