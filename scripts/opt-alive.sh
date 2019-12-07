#!/bin/bash

set -e

# IPO passes aren't supported ATM
PASSES="argpromotion deadargelim globalopt hotcoldsplit inline ipconstprop ipsccp mergefunc partial-inliner tbaa insert-gcov-profiling -Os -Oz -O1 -O2 -O3"

TV="-tv"
for p in $PASSES; do
  for arg in $@; do
    if [[ $arg == *"$p"* ]]; then
      TV=""
      break
    fi
  done
done

if [[ "$OSTYPE" == "darwin"* ]]; then
  # Mac
  TV_SHAREDLIB=tv.dylib
else
  # Linux, Cygwin/Msys, or Win32?
  TV_SHAREDLIB=tv.so
fi
timeout 1000 $HOME/llvm/build/bin/opt -load=$HOME/alive2/build/tv/$TV_SHAREDLIB -tv-exit-on-error $TV $@ $TV -tv-smt-to=10000 -tv-report-dir=$HOME/alive2/build/logs -tv-smt-stats
