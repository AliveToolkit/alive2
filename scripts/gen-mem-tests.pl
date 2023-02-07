#!/usr/bin/perl -w

use strict;

##################################

my $n = 0;
my $FIELDS = 8;

my @types = ("char", "short", "int", "long");
my @quals = ("signed", "unsigned");

sub go() {
    open my $OUTF, ">foo.c" or die;

    print $OUTF "struct {\n";
    for (my $i=0; $i<$FIELDS; ++$i) {
	print $OUTF "  " . $quals[rand @quals];
	print $OUTF " " . $types[rand @types];
	print $OUTF " f${i};\n";
    }
    print $OUTF "} s;\n";
    print $OUTF "\n";
    my $ty = $quals[rand @quals] . " " . $types[rand @types];
    print $OUTF "$ty f($ty x) {\n";
    for (my $i=0; $i<12; ++$i) {
	my $f = int(rand($FIELDS));
	if (rand() < 0.5) {
	    print $OUTF "  x += s.f${f};\n";
	} else {
	    print $OUTF "  s.f${f} = x;\n";
	}
    }
    print $OUTF "  return x;\n";
    print $OUTF "}\n";
    my $llvmfn = "global-test-${n}.aarch64.ll";
    $n++;
    system "clang -S -O -fno-strict-aliasing -emit-llvm foo.c -o $llvmfn";
}

for (my $i=0; $i<100; ++$i) {
    go();
}
