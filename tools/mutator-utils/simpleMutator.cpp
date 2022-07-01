#include "simpleMutator.h"

void StubMutator::moveToNextInst() {
  ++iit;
  if (iit == bit->end()) {
    moveToNextBlock();
  }
}

void StubMutator::moveToNextBlock() {
  ++bit;
  if (bit == fit->end()) {
    moveToNextFunction();
  } else {
    iit = bit->begin();
  }
}

void StubMutator::moveToNextFunction() {
  ++fit;
  while (fit == pm->end() || fit->isDeclaration()) {
    if (fit == pm->end()) {
      fit = pm->begin();
    } else {
      ++fit;
    }
  }
  bit = fit->begin();
  iit = bit->begin();
  currFunction = fit->getName();
}

bool StubMutator::init() {
  fit = pm->begin();
  while (fit != pm->end()) {
    bit = fit->begin();
    while (bit != fit->end()) {
      iit = bit->begin();
      if (iit == bit->end()) {
        ++bit;
        continue;
      }
      currFunction = fit->getName();
      return true;
    }
    ++fit;
  }
  return false;
}

void StubMutator::mutateModule(const std::string &outputFileName) {
  if (debug) {
    llvm::errs() << "current inst:\n";
    iit->print(llvm::errs());
    llvm::errs() << "\n";
  }
  llvm::errs() << "stub mutation visited.\n";
  if (debug) {
    llvm::errs() << "current basic block\n";
    bit->print(llvm::errs());
    std::error_code ec;
    llvm::raw_fd_ostream fout(outputFileName, ec);
    fout << *pm;
    fout.close();
    llvm::errs() << "file wrote to " << outputFileName << "\n";
  }
  moveToNextInst();
}

void StubMutator::saveModule(const std::string &outputFileName) {
  std::error_code ec;
  llvm::raw_fd_ostream fout(outputFileName, ec);
  fout << *pm;
  fout.close();
  llvm::errs() << "file wrote to " << outputFileName << "\n";
}

std::string StubMutator::getCurrentFunction() const {
  return currFunction;
}

bool Mutator::openInputFile(const string &inputFile) {
  auto MB =
      ExitOnErr(errorOrToExpected(llvm::MemoryBuffer::getFile(inputFile)));
  llvm::SMDiagnostic Diag;
  pm = getLazyIRModule(std::move(MB), Diag, context,
                       /*ShouldLazyLoadMetadata=*/true);
  if (!pm) {
    Diag.print("", llvm::errs(), false);
    return false;
  }
  ExitOnErr(pm->materializeAll());
  return true;
}