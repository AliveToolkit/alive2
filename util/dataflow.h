#pragma once

// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

#include "ir/function.h"
#include <unordered_map>

namespace util {

template <typename DataTy>
class DenseDataFlow {
  std::unordered_map<const IR::BasicBlock*, DataTy> bb_data;
  DataTy glb_data, ret_data;

public:
  DenseDataFlow(const IR::Function &f) {
    bb_data.emplace(&f.getFirstBB(), DataTy());

    // We assume BBs were top sorted already
    for (auto &bb : f.getBBs()) {
      auto I = bb_data.find(bb);
      if (I == bb_data.end()) // unreachable
        continue;
      auto &data = I->second;

      for (auto &inst : bb->instrs()) {
        data.exec(inst, glb_data);

        if (dynamic_cast<const IR::Return*>(&inst)) {
          ret_data.merge(data);
        }
      }

      for (auto &dst : bb->targets()) {
        auto [I, inserted] = bb_data.try_emplace(&dst, data);
        if (!inserted) {
          I->second.merge(data);
        }
      }
    }
  }

  const auto& getResult() const { return ret_data; }
};

}
