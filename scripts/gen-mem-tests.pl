#!/usr/bin/perl -w

use strict;

# void f(long *x) {
#   *x -= 777;
# }

my $n = 0;

sub go($$) {
    (my $t, my $q) = @_;
    open my $OUTF, ">foo.c" or die;
    print $OUTF "void f(" . $q . " " . $t . " *x) {\n";
    print $OUTF "  *x += " . (int(rand(100) - 50)) . ";\n";
    print $OUTF "}\n";
    my $llvmfn = "simple-ptr-test-${n}.aarch64.ll";
    $n++;
    system "clang -S -O -fno-strict-aliasing -emit-llvm foo.c -o $llvmfn";
}

my @types = ("char", "short", "int", "long");

foreach my $t (@types) {
    go($t, "signed");
    go($t, "unsigned");
}
