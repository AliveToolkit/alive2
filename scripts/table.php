<?php

$data = `grep 'ERROR: ' *.txt | grep -v Invalid`;
$data = explode("\n", trim($data));

$errors = array(
  'Out of memory; skipping function.' => 'OOM',
  'Timeout' => 'timeout',
  'Source is more defined than target' => 'domain',
  "Source and target don't have the same return domain" => 'noreturn',
  'Target is more poisonous than source' => 'poison',
  "Target's return value is more undefined" => 'undef',
  'Value mismatch' => 'retval',
  'Mismatch in memory' => 'memory',
  'Precondition is always false' => 'pre',
  "Couldn't prove the correctness of the transformation" => 'approx',
  'SMT Error' => 'SMT error',
  'Unsupported' => 'unsupported',
);

foreach ($data as $line) {
  preg_match('/(.+)\.txt:ERROR: (.+)/S', $line, $m);
  $err = @$errors[$m[2]];
  if (!$err) {
    foreach ($errors as $key => $val) {
      if (strpos($m[2], $key) === 0) {
        $err = $val;
        goto found;
      }
    }
    continue;
  }
found:
  @++$table[$m[1]][$err];
}

$max_file = 0;
foreach ($table as $file => $errs) {
  $max_file = max($max_file, strlen($file));
}

echo str_repeat(' ', $max_file);
foreach ($errors as $dummy => $err) {
  echo ' | ', $err;
}
echo " | total |\n";


foreach ($table as $file => $errs) {
  $total = 0;
  echo str_pad($file, $max_file), ' | ';
  foreach ($errors as $dummy => $err) {
    $len = strlen($err);
    if (isset($errs[$err])) {
      echo str_pad($errs[$err], $len, ' ', STR_PAD_BOTH), ' | ';
      $total += $errs[$err];
    } else {
       echo str_repeat(' ', $len), ' | ';
    }
  }
  echo str_pad($total, 5, ' ', STR_PAD_BOTH), " |\n";
}
