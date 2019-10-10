#!/usr/bin/perl -w

use strict;

my $ok = 0;

my $W = $ARGV[0];
die unless defined $W;

my $mask = (1<<$W)-1;

while (my $line = <STDIN>) {
    if ($line =~ /^\s*ret (i[0-9]+) %(.*)$/) {
        my $rettype = $1;
        my $ident = $2;
        my $real = "%real_${ident}";
        print "$real = and $rettype $mask, %${ident}\n";
        print "ret $rettype $real\n";
        $ok = 1;
    } else {
        print $line;
    }
}

die "failed!!!" unless $ok;
