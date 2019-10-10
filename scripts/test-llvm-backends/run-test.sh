set -e

# this has to be the native width used by opt-fuzz
WIDTH=32

# opt-fuzz emits too many arguments, get rid of unneeded ones
opt -strip foo.ll -S -o - | ~/alive2/scripts/test-llvm-backends/unused-arg-elimination.pl | opt -strip -S -o foo2.ll

# IR -> object code
clang -c -O foo2.ll -o foo.o

# object code -> IR
~/retdec-install/bin/retdec-decompiler.py foo.o

# ABI allows unused bits of return to be trashed; add an instruction
# masking these bits off. also strip out whatever register names the
# decompiler decided to use.
~/alive2/scripts/test-llvm-backends/maskret.pl $WIDTH < foo.o.ll | opt -strip -S -o foo-fixed.o.ll

# translation validation
~/alive2/build/alive-tv foo2.ll foo-fixed.o.ll --disable-poison-input --disable-undef-input
