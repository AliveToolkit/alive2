#!/usr/bin/perl -w

use strict;
use autodie;

# N.B. this script will not work in general, only on code emitted by
# opt-fuzz!!

# FIXME: make it process an entire file, not just one function

# FIXME: also do argument renaming?

my $rettype;
my $fname;
my @code;
my $in_func = 0;
my %available;
my %referenced;
my %args;
my @sorted_args;

while (my $line = <STDIN>) {
    # assuming no bad line breaks!
    if ($line =~ /define (i[0-9]+) @(func[0-9]+)\((.*)\).*\{/) {
        $rettype = $1;
        $fname = $2;
        my $argstr = $3;
	my @arglist = split(',', $argstr);
	my $argnum = 0;
	foreach my $arg (@arglist) {
	    my $name;
	    my $type;
	    if ($arg =~ /^\s*(i[0-9]+)$/) {
		$type = $1;
		$name = "%${argnum}";
		$argnum++;
	    } elsif ($arg =~ /^\s*(i[0-9]+)\s+(%[0-9a-zA-Z\._]+)$/) {
                die "explicit argument names not permitted, run opt -strip";
	    } else {
		die "unrecognized argument format '$arg'";
	    }
	    die if exists $args{$name};
	    $args{$name} = $type;
            push @sorted_args, $name;
	}
	%available = %args;
        $in_func = 1;
        next;
    }
    die "oops, unmatched function!" if $line =~ /define /;
    if ($in_func) {        
	if ($line =~ /\}/) {
            push @code, $line;
            print "define $rettype \@${fname}(";
            my $first = 1;
            foreach my $a (@sorted_args) {
                next unless $referenced{$a};
                print ", " unless $first;
                $first = 0;
                my $n = $a;
                substr($n, 0, 1) = "%x";
                print "$args{$a} $n";
            }
            print ") {\n";
            foreach my $l (@code) {
                print $l;
            }
	    $in_func = 0;
            undef $rettype;
            undef $fname;
            @code = ();
            $in_func = 0;
            %available = ();
            %referenced = ();
            %args = ();
            @sorted_args = ();
	} else {
	    my $seen_eq = 0;
	    my $id_num = 0;
            my $ret = 0;
	    for (my $i=0; $i < length($line); $i++) {
		my $s = substr($line, $i);
                $ret = 1 if ($s =~ /^ret /);
                $seen_eq = 1 if ($s =~ /^=/);
		if ($s =~ /^(%[0-9a-zA-Z\._]+)/) {
                    my $id = $1;
		    if ($id_num == 0 && !$ret) {
                        die if $seen_eq;
                        die if $available{$id};
                        die if $referenced{$id};
                        $available{$id} = 1;                        
		    } else {
                        die unless $seen_eq || $ret;
                        die unless $available{$id};
                        $referenced{$id} = 1;
		    }
		    $id_num++;
                    substr($line, $i, 1) = "%x";
		}
	    }
            push @code, $line;
	}
    } else {
        # code outside of functions is just printed unchanged
        print $line;
    }
}

