#!/usr/bin/perl -w

use strict;
use autodie;
use File::Temp;
use Getopt::Long;

#####################################################################

my $ALIVETV = $ENV{"HOME"}."/alive2/build/alive-tv";
my $MINW = 4;
my $MAXW = 256;

#####################################################################

# TODO
# - check for UB-related outputs for negative tests
# - maybe run widths near the timeout a couple of times to find the first timeout more reliably?
# - configurable timeout
# - probably eventually need to support multiple widths per test case: short, regular, long

my $DEBUG = 0;

die "can't execute '${ALIVETV}'" unless -x $ALIVETV;

GetOptions ("debug" => \$DEBUG)
    or die("Error in command line arguments\n");

my $SPINNER_COUNT = -1;
my @spinner = ("|", "/", "-", "\\");

sub spin() {
    $SPINNER_COUNT = 0 if ++$SPINNER_COUNT == @spinner;
    print $spinner[$SPINNER_COUNT]."\b";
}

sub check($$$) {
    (my $f, my $x, my $positive) = @_;
    my $tmpll = File::Temp->new(SUFFIX => '.ll');
    system "m4 --define=iX=i${x} --define=X=${x} ${f} > ${tmpll}";
    system "cat ${tmpll}" if $DEBUG;
    my $tmpout = File::Temp->new();
    system "${ALIVETV} ${tmpll} > $tmpout 2>&1";
    system "cat ${tmpout}" if $DEBUG;
    open my $INF, "<$tmpout" or die;
    while (my $line = <$INF>) {
        return 1 if $positive && $line =~ /Transformation seems to be correct/;
        return 1 if !$positive && $line =~ /Value mismatch/;
    }
    close $INF;
    return 0;
}

my $total = 0;
my $n = 0;

sub test_file($$) {
    (my $f, my $positive) = @_;
    if ($DEBUG) {
        check($f, $MINW, $positive);
    } else {
        print "$f : ";
        die "doesn't even work at ${MINW}" unless check($f, $MINW, $positive);
        my $x;
        for ($x = $MINW + 1; $x < $MAXW; $x++) {
            spin();
            last unless check($f, $x, $positive);
        }
        $total += $x;
        $n++;
        print "$x\n";
    }
}

if (scalar(@ARGV) > 0) {
    foreach my $f (@ARGV) {
        die "can't read file '$f'" unless -r $f;
        if ($f =~ /^positive/) {
            test_file($f, 1);
        } elsif ($f =~ /^negative/) {
            test_file($f, 0);
        } else {
        }
    }
} else {
    my @pos = glob "positive/*.ll";
    print "\nfound ".scalar(@pos)." positive tests.\n";
    foreach my $f (@pos) {
        test_file($f, 1);
    }
    
    my @neg = glob "negative/*.ll";
    print "\nfound ".scalar(@neg)." negative tests.\n";
    foreach my $f (@neg) {
        test_file($f, 0);
    }
}

if ($n > 0) {
    my $avg = (0.0 + $total) / $n;
    printf "\naverage score: %.1f\n\n", $avg;
}

#####################################################################
