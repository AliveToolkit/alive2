#pragma once
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/IntrinsicInst.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Verifier.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Support/Error.h"
#include "llvm/Transforms/Scalar/LICM.h"
#include "llvm/Transforms/Scalar/NewGVN.h"
#include "llvm/Transforms/Utils/Cloning.h"
#include "llvm/Transforms/Utils/FunctionComparator.h"

#include <algorithm>
#include <climits>
#include <ctime>
#include <memory>
#include <random>
#include <string>
#include <vector>

using std::string;

class Random {
  static std::random_device rd;
  static unsigned seed;
  static std::mt19937 mt;
  static std::uniform_int_distribution<int> dist;
  static llvm::SmallVector<unsigned> usedInts;
  static llvm::SmallVector<double> usedDoubles;
  static llvm::SmallVector<float> usedFloats;
  static unsigned getExtremeInt(llvm::IntegerType *ty);
  static unsigned getBitmask(llvm::IntegerType *ty);
  static double getExtremeDouble();
  static float getExtremeFloat();
  static unsigned getUsedInt(llvm::IntegerType *ty);
  static double getUsedDouble();
  static float getUsedFloat();

public:
  static int getRandomInt() {
    return dist(mt);
  }

  static std::mt19937& getRNG() {
    return mt;
  }

  static void addUsedInt(unsigned i) {
    usedInts.push_back(i);
  }
  static double getRandomDouble();
  static float getRandomFloat();
  static unsigned getRandomUnsigned() {
    return abs(dist(mt));
  }
  static unsigned getRandomUnsigned(unsigned bits) {
    return abs(dist(mt)) % (1u << bits);
  }
  static bool getRandomBool() {
    return dist(mt) & 1;
  }
  static unsigned getSeed() {
    return seed;
  }
  static void setSeed(unsigned seed_) {
    seed = seed_;
    mt = std::mt19937(seed);
  }
  static unsigned getRandomLLVMInt(llvm::IntegerType *ty);
  static double getRandomLLVMDouble();
  static float getRandomLLVMFloat();
};

/*
  This is a class for holding dominated value with a backup function.
         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-G(rear)
  all operations on rear can be restored by a 'backup' operation

         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-G(rear)

  (update on rear)

         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-H(rear)

  (restore)

         /E-F-G(backup)
  A-B-C-D(domInst)
         \E-F-G(rear)
*/
class DominatedValueVector {
  std::vector<llvm::Value *> domInst, backup, rear;
  bool hasBackup;

public:
  DominatedValueVector() : hasBackup(false){};
  ~DominatedValueVector() {
    domInst.clear();
    backup.clear();
    rear.clear();
  }
  llvm::Value *&operator[](size_t idx) {
    if (idx < domInst.size()) {
      return domInst[idx];
    } else {
      return rear[idx - domInst.size()];
    }
  }

  void push_back_tmp(llvm::Value *val) {
    if (hasBackup) {
      rear.push_back(val);
    } else {
      domInst.push_back(val);
    }
  }

  void pop_back_tmp() {
    if (hasBackup) {
      rear.pop_back();
    } else {
      domInst.pop_back();
    }
  }

  void push_back(llvm::Value *val) {
    if (hasBackup) {
      backup.push_back(val);
      rear.push_back(val);
    } else {
      domInst.push_back(val);
    }
  }

  void pop_back() {
    if (hasBackup) {
      backup.pop_back();
      rear.pop_back();
    } else {
      domInst.pop_back();
    }
  }

  void startBackup() {
    hasBackup = true;
  }

  /**
   * rear would be clear.
   * all elements in backup would be push_back to domInst and backup clear;
   */
  void deleteBackup() {
    hasBackup = false;
    while (!backup.empty()) {
      domInst.push_back(backup.back());
      backup.pop_back();
    }
    rear.clear();
  }

  void restoreBackup() {
    if (hasBackup) {
      rear = backup;
    }
  }

  void clear() {
    hasBackup = false;
    domInst.clear();
    rear.clear();
    backup.clear();
  }

  void resize(size_t sz) {
    if (hasBackup) {
      if (sz <= domInst.size()) {
        deleteBackup();
        domInst.resize(sz);
      } else {
        rear.resize(sz - domInst.size());
        backup.resize(sz - domInst.size());
      }
    } else {
      domInst.resize(sz);
    }
  }

  llvm::Value *&back() {
    return rear.empty() ? domInst.back() : rear.back();
  }
  bool inBackup() const {
    return hasBackup;
  }
  size_t size() const {
    return domInst.size() + rear.size();
  }
  size_t tmp_size() const {
    return rear.size();
  }
  size_t empty() const {
    return domInst.empty() && (!hasBackup || rear.empty());
  }
  int find(llvm::Value *val) const {
    for (size_t i = 0; i < domInst.size(); ++i)
      if (val == domInst[i])
        return i;
    if (hasBackup) {
      for (size_t i = 0; i < rear.size(); ++i)
        if (val == rear[i])
          return i + domInst.size();
    }
    return -1;
  }
};

class FunctionComparatorWrapper : public llvm::FunctionComparator {
public:
  FunctionComparatorWrapper(const llvm::Function *func1,
                            const llvm::Function *func2,
                            llvm::GlobalNumberState *GN)
      : llvm::FunctionComparator(func1, func2, GN){};
  bool isSameSignature() const {
    return compareSignature() == 0;
  }
};

class LLVMFunctionComparator {
  llvm::GlobalNumberState gn;
  FunctionComparatorWrapper wrapper;

public:
  LLVMFunctionComparator()
      : gn(llvm::GlobalNumberState()),
        wrapper(FunctionComparatorWrapper(nullptr, nullptr, nullptr)) {}
  bool compareSignature(const llvm::Function *func1,
                        const llvm::Function *func2) {
    gn.clear();
    wrapper = FunctionComparatorWrapper(func1, func2, &gn);
    return wrapper.isSameSignature();
  }
};

class mutator_util {
  static LLVMFunctionComparator comparator;
  const static std::vector<llvm::Instruction::BinaryOps> integerBinaryOps;
  const static std::vector<llvm::Instruction::BinaryOps> floatBinaryOps;
  const static std::vector<llvm::Intrinsic::ID> integerBinaryIntrinsic;
  const static std::vector<llvm::Intrinsic::ID> floatBinaryIntrinsic;
  const static std::vector<llvm::Intrinsic::ID> integerUnaryIntrinsic;
  const static std::vector<llvm::Intrinsic::ID> floatUnaryIntrinsic;

public:
  static void removeTBAAMetadata(llvm::Module *M);
  static llvm::Value *insertGlobalVariable(llvm::Module *m, llvm::Type *ty);
  static std::string
  insertFunctionArguments(llvm::Function *f,
                          llvm::SmallVector<llvm::Type *> tys,
                          llvm::ValueToValueMapTy &VMap);
  static void propagateFunctionsInModule(llvm::Module *M, size_t num);
  static llvm::Value *
  updateIntegerSize(llvm::Value *integer, llvm::Type *newIntTy,
                    llvm::Instruction *insertBefore = nullptr);
  static bool compareSignature(const llvm::Function *func1,
                               const llvm::Function *func2) {
    return comparator.compareSignature(func1, func2);
  }
  static llvm::Instruction *
  getRandomIntegerInstruction(llvm::Value *val1, llvm::Value *val2,
                              llvm::Instruction *insertBefore = nullptr);
  static llvm::Instruction *
  getRandomFloatInstruction(llvm::Value *val1, llvm::Value *val2,
                            llvm::Instruction *insertBefore = nullptr);
  static llvm::Instruction *
  getRandomIntegerBinaryInstruction(llvm::Value *val1, llvm::Value *val2,
                                    llvm::Instruction *insertBefore = nullptr);
  static llvm::Instruction *
  getRandomFloatBinaryInstruction(llvm::Value *val1, llvm::Value *val2,
                                  llvm::Instruction *insertBefore = nullptr);
  static llvm::Instruction *
  getRandomIntegerIntrinsic(llvm::Value *val1, llvm::Value *val2,
                            llvm::Instruction *insertBefore);
  static llvm::Instruction *
  getRandomFloatInstrinsic(llvm::Value *val1, llvm::Value *val2,
                           llvm::Instruction *insertBefore);

  template <typename EleTy, typename T>
  static EleTy findRandomInArray(llvm::ArrayRef<EleTy> array, T val,
                                 std::function<bool(EleTy, T)> predicate,
                                 EleTy failed) {
    for (size_t i = 0, pos = Random::getRandomUnsigned() % array.size();
         i < array.size(); ++i, ++pos) {
      if (pos == array.size()) {
        pos = 0;
      }
      if (predicate(array[pos], val)) {
        return array[pos];
      }
    }
    return failed;
  }

  static bool isPadInstruction(llvm::Instruction *inst) {
    return llvm::isa<llvm::LandingPadInst>(inst) ||
           llvm::isa<llvm::CleanupPadInst>(inst) ||
           llvm::isa<llvm::CatchPadInst>(inst);
  }
};