<?php

$data = `grep 'ERROR: ' *.txt | grep -v Unsupported | grep -v Timeout | grep -v Invalid`;
$data = explode("\n", trim($data));

$errors = array(
  'Out of memory; skipping function.' => 'OOM',
  'Source is more defined than target' => 'domain',
  'Mismatch in memory' => 'memory',
  'Precondition is always false' => 'precondition',
  'Target is more poisonous than source' => 'poison',
);

foreach ($data as $line) {
  preg_match('/(.+)\.txt:ERROR: (.+)/S', $line, $m);
  @++$table[$m[1]][$errors[$m[2]]];
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
