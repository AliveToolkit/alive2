#!/bin/bash

set -e

# IPO passes aren't supported ATM
PASSES="argpromotion deadargelim inline ipsccp mergefunc partial-inliner"

TV="-tv"
for p in $PASSES; do
  for arg in $@; do
    if [[ $arg == *"$p"* ]]; then
      TV=""
      break
    fi
  done
done

timeout 1000 $HOME/llvm/build/bin/opt -load=$HOME/alive2/build/tv/tv.so -tv-exit-on-error $TV $@ $TV -tv-smt-to=10000 -tv-report-dir=$HOME/alive2/build/logs -tv-smt-stats
