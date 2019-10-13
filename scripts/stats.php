<?php

$logs = @$argv[1];

if (sizeof($argv) != 2 || !is_dir($logs))
  die("Use: $argv[0] <log dir>\n");


$errors      = array();
$stats       = array();
$knownfns    = array();
$unsupported = array();

foreach (glob("logs/*.txt") as $f) {
  $txt = file_get_contents($f);
  preg_match_all('/ERROR: (.+)/S', $txt, $m);
  foreach($m[1] as $err) {
    @++$errors[$err];
  }

  preg_match_all('/Num ([a-zA-Z]+): +(\d+)/S', $txt, $m, PREG_SET_ORDER);
  foreach ($m as $stat) {
    @$stats[$stat[1]] += (int)$stat[2];
  }

  preg_match_all('/Unsupported (?:instruction|type):\s*(.+)/S', $txt, $m);
  foreach ($m[1] as $str) {
    if (preg_match('/%\S+ = ([^%[(]+)/S', $str, $m2)) {
      @++$unsupported[trim($m2[1])];
    } else {
      @++$unsupported[trim($str)];
    }
  }

  preg_match_all('/Unsupported metadata:\s*(\d+)/S', $txt, $m);
  foreach ($m[1] as $str) {
    @++$unsupported["metadata $str"];
  }

  preg_match_all('/call.+(@[^ (]+).*; WARNING: unknown known function/S', $txt, $m);
  foreach ($m[1] as $str) {
    @++$knownfns[$str];
  }
}

echo "SMT Statistics:\n";
foreach ($stats as $stat => $n) {
  if ($n > 0)
    echo str_pad("$stat:", 10), "$n\n";
}

echo "\nErrors:\n";
arsort($errors);
foreach ($errors as $err => $count) {
  echo "$count\t$err\n";
}

echo "\nUnsupported IR features (Top 20):\n";
arsort($unsupported);
$i = 0;
foreach ($unsupported as $un => $count) {
  echo "$count\t$un\n";
  if (++$i == 20)
    break;
}

echo "\nUnsupported known functions (Top 20):\n";
arsort($knownfns);
$i = 0;
foreach ($knownfns as $fn => $count) {
  echo "$count\t$fn\n";
  if (++$i == 20)
    break;
}
