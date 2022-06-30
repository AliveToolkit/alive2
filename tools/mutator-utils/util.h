#pragma once
#include "llvmStress.h"
#include "llvm/ADT/StringExtras.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/InstIterator.h"
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

class LLVMUtil {
  static LLVMFunctionComparator comparator;
  const static std::vector<llvm::Instruction::BinaryOps> integerBinaryOps;
  const static std::vector<llvm::Instruction::BinaryOps> floatBinaryOps;
  const static std::vector<llvm::Intrinsic::ID> integerBinaryIntrinsic;
  const static std::vector<llvm::Intrinsic::ID> floatBinaryIntrinsic;
  const static std::vector<llvm::Intrinsic::ID> integerUnaryIntrinsic;
  const static std::vector<llvm::Intrinsic::ID> floatUnaryIntrinsic;

public:
  static void optimizeModule(llvm::Module *M, bool newGVN = false,
                             bool licm = false);
  static void optimizeFunction(llvm::Function *f, bool newGVN = false,
                               bool licm = false);
  static void removeTBAAMetadata(llvm::Module *M);
  static llvm::Value *insertGlobalVariable(llvm::Module *m, llvm::Type *ty);
  static void insertFunctionArguments(llvm::Function *f,
                                      llvm::SmallVector<llvm::Type *> tys,
                                      llvm::ValueToValueMapTy &VMap);
  static void insertRandomCodeBefore(llvm::Instruction *inst);
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
};