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

$data = `egrep '  [0-9]+ (in)?correct transformations' *.txt`;
$data = explode("\n", trim($data));
foreach ($data as $line) {
  preg_match('/(.+)\.txt:  (\d+) (in)?/S', $line, $m);
  $col = isset($m[3]) ? 'bugs found' : 'verified';
  $table[$m[1]][$col] = $m[2];
}

$data = `egrep '^[.0-9]+user [.0-9]+system' *.txt`;
$data = explode("\n", trim($data));
foreach ($data as $line) {
  preg_match('/(.+)\.txt:(\d+)/S', $line, $m);
  $time[$m[1]] = $m[2];
}

foreach ($table as $file => $errs) {
  $total = 0;
  foreach ($errors as $dummy => $err) {
    $toprint[$file][$err] = @$errs[$err];
    $total += @$errs[$err];
  }
  $toprint[$file]['total']      = $total;
  $toprint[$file]['verified']   = @$table[$file]['verified'];
  $toprint[$file]['bugs found'] = @$table[$file]['bugs found'];
  $toprint[$file][' time ']     = @$time[$file];
}
print_table($toprint);

echo "\n\nSummary:\n";
foreach ($table as $file => $errs) {
  $kind = explode('.', $file)[1];
  @$summary[$kind]['verified'] += @$errs['verified'];
  @$summary[$kind]['bugs found'] += @$errs['bugs found'];
  @$summary[$kind][' time '] += @$time[$file];
}
print_table($summary);


function print_table($table) {
  $max_row = 0;
  foreach ($table as $row => $data) {
    $max_row = max($max_row, strlen($row));
  }
  echo str_repeat(' ', $max_row);

  foreach (current($table) as $col => $n) {
    echo " | $col";
  }
  echo " |\n";
  foreach ($table as $row => $data) {
    echo str_pad($row, $max_row);
    foreach ($data as $col => $n) {
      echo ' | ', str_pad($n, strlen($col), ' ', STR_PAD_LEFT);
    }
    echo " |\n";
  }
}
