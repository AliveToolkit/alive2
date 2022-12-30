#!/bin/bash

FILES=`ls -d -1 $1*.ll`

for f in $FILES; do
  out="${f%.ll}.srctgt.ll"
  mv "$f" "$out"
  sed -i '1s/^/; TEST-ARGS: -backend-tv --disable-undef-input --disable-poison-input\n\n/' "$out"
done
