<?php

$txt = file_get_contents($argv[1]);

preg_match_all('/\[Z3RC\] newObj (0x\w+) (.*)/S', $txt, $m, PREG_SET_ORDER);
foreach ($m as $obj) {
  $obj_str[$obj[1]] = $obj[2];
  $obj_ref[$obj[1]] = 0;
}

preg_match_all('/\[Z3RC\] (inc|dec)Ref (0x\w+)/S', $txt, $m, PREG_SET_ORDER);
foreach ($m as $obj) {
  if ($obj[1] == 'inc') {
    ++$obj_ref[$obj[2]];
  } else {
    if ($obj_ref[$obj[2]] <= 0)
      echo "ERROR: dec_ref <= 0: $m[1]\n";
    --$obj_ref[$obj[2]];
  }
}

foreach ($obj_ref as $obj => $refs) {
  if ($refs != 0) {
    echo "LEAK [$refs]: $obj $obj_str[$obj]\n";
  }
}
