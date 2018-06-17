#!/bin/bash
# Copyright (c) 2018-present The Alive2 Authors.
# Distributed under the MIT license that can be found in the LICENSE file.

FILES=$(ls input-2/*)
OPTS="instsimplify instcombine O2 gvn newgvn sccp"
ALIVE="~/alive2-build/alive"
OPT="~/llvm-build/bin/opt"

for optarg in $OPTS ; do
  parallel --joblog joblog.$optarg.log \
    "mkdir -p output/$optarg/{//} && \
     /usr/bin/time php run.php {} $optarg $ALIVE $OPT &> output/$optarg/{.}.txt" \
    ::: $FILES
done
