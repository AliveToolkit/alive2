<?php

// pick x benchmarks randomly
$dstdir = 'alive2_bench';

if ($argc !== 2)
  die("Usage: php $argv[0] <num bench>\n");

$files = glob('bench_*/*.smt2');

`mkdir -p $dstdir`;

$i = 0;
foreach (array_rand($files, $argv[1]) as $n) {
  $f = $files[$n];
  $dst = preg_replace('@bench_(.*)/.*\.smt2@S',
                      "$dstdir/" . str_pad($i++, 3, '0', STR_PAD_LEFT) .
                      "_\\1.smt2", $f);
  copy($f, $dst);
}

`tar c $dstdir | xz -9 -e -T0 > $dstdir.tar.xz`;
