<?php
// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

$dump    = @$argv[1];
$logsdir = @$argv[2];

if (!$dump || !$logsdir || !is_file($dump) || !is_dir($logsdir))
  die("usage: php gen-dashboard.php <dump.txt> <logs>\n");

$dump = file_get_contents($dump);
preg_match('/\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\nFailed Tests \((\d+)\):\n(.*)/Ss', $dump, $m);

$num_failures = (int)$m[1];
echo "Failures:\t\t$num_failures\n";

preg_match_all('/LLVM ::\s*(\S+)/S', $m[2], $m);
$test_failures = $m[1];

@mkdir('web');
@mkdir('web/data');
@mkdir('web/data/logs');

$fail_without_undef = 0;

foreach ($test_failures as $test) {
  if (!preg_match('@'.$test.'\' FAILED \*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\*\n'.
                  'Script:\n.*?Exit Code:[^\n]+\n\n'.
                  '(?:Command Output \(stdout\):\n--.*?\n--\n)?'.
                  'Command Output \(stderr\):\n--(.*?)\n--\n@Ss', $dump, $m)) {
    die("Failed to get data for $test\n");
  }

  $err = $m[1];
  $stderr = store_log(preg_replace('/Report written to \S+/S', '', $err));

  preg_match('/Report written to (\S+)/S', $err, $m);
  if (empty($m[1])) {
    if (strstr($err, '(core dumped)') !== false) {
      $error = 'Crash';
    } else {
      $error = '?';
    }
    $log = store_log('');
  } else {
    $log = file_get_contents($m[1]);
    if (preg_match_all('/ERROR: (.+)/S', $log, $m)) {
      $error = end($m[1]);
    } else {
      $error = '?';
    }
    $log = store_log($log);
  }
  
  // TODO: check if correct without undef
  $ok_wo_undef = correct_without_undef($test);
  if (!$ok_wo_undef)
   ++$fail_without_undef;
  $tests[] = array($test, $ok_wo_undef, $stderr, $log, $error);
}

echo "Failures wo undef:\t$fail_without_undef\n";

$tests_csv = '';
foreach ($tests as $t) {
  $tests_csv .= implode(',', $t) . "\n";
}

$tests_csv = store_log($tests_csv);
$alive_git = implode(',', git_info('~/alive2'));
$llvm_git  = implode(',', git_info('~/llvm'));
$entry = "$tests_csv,$num_failures,$fail_without_undef,$alive_git,$llvm_git\n";

file_put_contents('web/data/data.txt', $entry, FILE_APPEND);


function correct_without_undef($test) {
  echo "Testing $test without undef.. ";
  $output = `~/llvm/build/bin/llvm-lit -vv -Dopt="\$HOME/alive2/scripts/opt-alive.sh -tv-disable-undef-input" ~/llvm/llvm/test/$test 2>&1`;
  $ok = preg_match('/Expected Passes\s*:\s*1/S', $output) === 1;
  echo ($ok ? "OK\n" : "buggy\n");
  return $ok;
}

function store_log($txt) {
  $md5 = substr(md5($txt), 0, 16);
  file_put_contents("web/data/logs/$md5.txt", $txt);
  return $md5;
}

// returns [hash, timestamp]
function git_info($dir) {
  return explode(' ', trim(`cd $dir && git show -q --format='%h %ct'`));
}
