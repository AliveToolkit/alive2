<?php

// filter out benchmarks:
// 1) remove duplicated files
// 2) remove files that use lambdas (non-smtlib compliant)
// 3) remove trivial benchmarks

$files = glob('bench_*/*.smt2');
$hashes = [];

foreach ($files as $f) {
  $hash = md5_file($f);
  if (isset($hashes[$hash])) {
    //echo "remove dup file $f\n";
    unlink($f);
    continue;
  }
  $hashes[$hash] = $f;
}

foreach ($hashes as $h => $f) {
  if (`grep -c '(lambda' $f` > 0) {
    //echo "remove file with lambdas $f\n";
    unlink($f);
    continue;
  }

  $z3 = `z3 -T:1 $f 2>&1`;
  if ($z3 == "sat\n" || $z3 == "unsat\n") {
    //echo "remove trivial file $f\n";
    unlink($f);
  } else if ($z3 != "timeout\n" &&
             $z3 != "unknown\n" &&
             $z3 != "unknown\ntimeout\n") {
    echo "CRASH: $f\n$z3\n\n";
  }
}
