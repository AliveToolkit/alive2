<?php
// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

$file   = @$argv[1];
$optarg = @$argv[2];
$alive  = @$argv[3];
if (!$file || !$optarg || !$alive)
  die("usage: php run.php file.ll.bz2 llvm-opt-pass-name build/alive <llvm/build/opt>\n");

$optbin = empty($argv[4]) ? 'opt' : $argv[4];

$ll = substr($file, 0, -4);

`bunzip2 -k $file`;
$txt = file_get_contents($ll);

$regex = '/define \S* @([^(]+)([^{]*)\{([^}]+)\}/S';

preg_match_all($regex, $txt, $m, PREG_SET_ORDER);
foreach ($m as $opt) {
  $orig[$opt[1]] = $opt[3];
}

$txt = `$optbin -S -$optarg $ll`;
$output = '';
preg_match_all($regex, $txt, $m, PREG_SET_ORDER);
foreach ($m as $opt) {
  $o = $orig[$opt[1]];
  $output .= "Name: $opt[1]$o=>$opt[3]\n";
}

file_put_contents("$ll.alive.opt", $output);

echo `$alive -root-only $ll.alive.opt 2>&1`;

unlink($ll);
unlink("$ll.alive.opt");
