#!/bin/bash
timeout 60 $HOME/llvm/build/bin/opt -load=$HOME/alive2/build/tv/tv.so -tv $@ -tv
