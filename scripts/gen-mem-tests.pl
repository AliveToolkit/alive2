#!/usr/bin/perl -w

use strict;

##################################

my $n = 0;
my $FIELDS = 8;

my @types = ("char", "short", "int", "long");
my @quals = ("signed", "unsigned");

sub go() {
    open my $OUTF, ">foo.c" or die;

    print $OUTF "struct s {\n";
    for (my $i=0; $i<$FIELDS; ++$i) {
	print $OUTF "  " . $quals[rand @quals];
	print $OUTF " " . $types[rand @types];
	print $OUTF " f${i};\n";
    }
    print $OUTF "};\n";
    print $OUTF "\n";
    print $OUTF "void f(struct s *p) {\n";
    print $OUTF "  int x = 0;\n";
    for (my $i=0; $i<12; ++$i) {
	my $f = int(rand($FIELDS));
	if (rand() < 0.5) {
	    print $OUTF "  x += p->f${f};\n";
	} else {
	    print $OUTF "  p->f${f} = x;\n";
	}
    }
    print $OUTF "}\n";
    my $llvmfn = "offset-test-${n}.aarch64.ll";
    $n++;
    system "clang -S -O -fno-strict-aliasing -emit-llvm foo.c -o $llvmfn";
}

for (my $i=0; $i<200; ++$i) {
    go();
}
