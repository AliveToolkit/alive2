#!/bin/bash

set -e

# IPO passes aren't supported ATM
# safe-stack: introduces non-cost globals
# place-safepoints: places new function calls (@do_safepoint)
# loop-extract: extracts a top-level loop into a distinct function
# extract-blocks: extract specified blocks into a distinct function
# attributor, functionattrs: inter procedural pass that deduces and/or propagates attributes
# metarenamer: anonymizes function names
PASSES="argpromotion deadargelim globalopt hotcoldsplit inline ipconstprop ipsccp mergefunc partial-inliner tbaa loop-extract extract-blocks safe-stack place-safepoints attributor functionattrs metarenamer lowertypetests extract-blocks openmpopt prune-eh -Os -Oz -O1 -O2 -O3"
PASSES_SIMPLIFYLIB="-instcombine -Os -Oz -O1 -O2 -O3"

TV="-tv"
IO_NOBUILTIN="-tv-io-nobuiltin"
for arg in $@; do
  for p in $PASSES; do
    if [[ $arg == *"$p"* ]]; then
      TV=""
      break
    fi
  done
  for p in $PASSES_SIMPLIFYLIB; do
    if [[ $arg == "$p" ]]; then
      IO_NOBUILTIN=""
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
timeout 1000 $HOME/llvm/build/bin/opt -load=$HOME/alive2/build/tv/$TV_SHAREDLIB -tv-exit-on-error $TV $@ $TV -tv-smt-to=10000 -tv-report-dir=$HOME/alive2/build/logs -tv-smt-stats $IO_NOBUILTIN
