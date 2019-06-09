#!/bin/bash
timeout 1000 $HOME/llvm/build/bin/opt -load=$HOME/alive2/build/tv/tv.so -tv-exit-on-error -tv $@ -tv -tv-smt-to=10000
