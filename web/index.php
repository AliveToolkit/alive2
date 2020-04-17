<?php
// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

include 'include.php';

// 0: tests_csv, 1: num_failures, 2: fail_without_undef, 3: alive_git_hash,
// 4: alive_git_time, 5: llvm_git_hash, 6: llvm_git_time
$data = array();
foreach (file('data/data.txt') as $entry) {
  $data[] = explode(',', trim($entry));
}

$hash = @$_REQUEST['hash'];
$test = @$_REQUEST['test'];

if ($hash && $test) {
  $run = get_all_runs($data, $hash);
  $tests = get_run_tests($run[0]);
  foreach ($tests as $t) {
    if ($t[0] === $test)
      break;
  }

  $name_html = htmlspecialchars($t[0]);
  html_header("Test Failure: $name_html");

  if (IS_ADMIN && isset($_POST['comment'])) {
    save_comment($t[0], $_POST['comment']);
    echo "<p>New comment saved!</p>\n";
  }

  echo "<p>Test source: <a href=\"https://github.com/llvm/llvm-project/blob/$run[5]/llvm/test/$name_html\">git</a></p>\n";

  $comments = get_comments();
  if (!empty($comments[$t[0]]))
    echo "<p>Comments: {$comments[$t[0]][0]}</p>\n";

  echo "<h2>Log:</h2>\n<pre>";
  echo htmlspecialchars(file_get_contents("data/logs/$t[3].txt")), "</pre>\n";

  echo "<h2>stderr:</h2>\n<pre>";
  echo htmlspecialchars(file_get_contents("data/logs/$t[2].txt")), "</pre>\n";

  if ($t[1])
    echo "<p>&nbsp;</p><p>NOTE: This test would pass if undef didn't exist!</p>";

  if (IS_ADMIN) {
    $c = htmlspecialchars(@$comments[$t[0]][1]);
    echo <<< HTML
<p>&nbsp;</p>
<h2>Change comments (destructive action!!):</h2>
<form action="index.php" method="post">
<input type="hidden" name="hash" value="$hash">
<input type="hidden" name="test" value="$name_html">
<textarea rows="2" cols = "80" name= "comment">$c</textarea>
<br>
<input type="submit" value="Save">
</form>
<p>For LLVM bugs, enter as LLVM#42.</p>

HTML;
  }
}
else if ($hash) {
  $run = get_all_runs($data, $hash);
  $tests = get_run_tests($run[0]);
  $comments = get_comments();
  html_header("Run $run[5] - " . format_date($run[6]));
  echo <<< HTML
<p>$run[1] failures ($run[2] ignoring undef)<br>
LLVM git: <a href="https://github.com/llvm/llvm-project/commit/$run[5]">$run[5]</a><br>
Alive2 git: <a href="https://github.com/AliveToolkit/alive2/commit/$run[3]">$run[3]</a></p>

<p>Failed tests:</p>
<table id="tabletests" class="tablesorter" style="width:auto">
<thead><tr>
<th>Test Name</th><th>Ok if undef ignored?</th><th>Failure reason</th><th>Comments</th>
</tr></thead>
<tbody>
HTML;

  foreach ($tests as $test) {
    $check = $test[1] ? '&#x2713;' : '&nbsp;';
    echo "<tr><td><a href=\"index.php?hash=$hash&amp;test=".
         htmlspecialchars(urlencode($test[0])). "\">".
         htmlspecialchars($test[0])."</a></td>".
         "<td style=\"text-align:center\">$check</td>".
         "<td>".htmlspecialchars($test[4])."</td>".
         "<td>". @$comments[$test[0]][0] ."</td></tr>\n";
  }

  echo <<< HTML
</tbody></table>
<script>
$(function() {
  $("#tabletests").tablesorter({
    theme: 'blue',
    widgets: [ 'zebra', 'resizable', 'stickyHeaders' ],
    widgetOptions: {
      storage_storageType: 's',
      resizable_addLastColumn: true
    }});
});
</script>

HTML;
}
else {
  html_header('Project Zero LLVM Bugs');
  do_plot($data);

  echo <<< HTML
<p>&nbsp;</p>
<p>Last 5 runs:</p>
<table>
<tr>
<th>LLVM commit</th><th>Date/Time</th><th>Failures</th>
</tr>

HTML;

  $n_runs = sizeof($data);
  // 5 runs is: [n_runs-5, ..., n_runs-1]
  for ($i = sizeof($data)-1; $i >= max($n_runs-5, 0); --$i) {
    $t = $data[$i];
    $date = format_date($t[6]);
    echo "<tr><td><a href=\"index.php?hash=$t[0]\">$t[5]</a></td>".
         "<td>$date</td><td>$t[1]</td></tr>\n";
  }
  echo "</table>\n";
}

html_footer();


function format_date($ts) {
  return date('d/M/Y', $ts);
}

function get_all_runs($data, $hash) {
  $found = false;
  foreach ($data as $t) {
    if ($t[0] === $hash) {
      $found = true;
      break;
    }
  }

  if (!$found)
    die('Hash not found');

  $t[] = file_get_contents("data/logs/$t[0].txt");
  return $t;
}

// 0: test name, 1: ok with undef?, 2: stderr hash, 3: log hash, 4: error reason
function get_run_tests($hash) {
  $data = array();
  foreach (file("data/logs/$hash.txt") as $entry) {
    $data[] = explode(',', trim($entry));
  }
  return $data;
}

function get_comments() {
  foreach(file('data/comments.txt') as $line) {
    $a = explode(',', trim($line));
    if (sizeof($a) === 2)
      $t = htmlspecialchars($a[1]);
      $t = preg_replace('/LLVM#(\d+)/', '<a href="https://llvm.org/PR\1">LLVM PR\1</a>', $t);
      $comments[$a[0]] = array($t, $a[1]);
  }
  return $comments;
}

function save_comment($test, $c) {
  $cs = get_comments();
  $c = trim($c);
  $cs[$test] = array($c, $c);
  $txt = '';
  foreach ($cs as $f => $c) {
    if ($c[1])
      $txt .= "$f,$c[1]\n";
  }
  file_put_contents('data/comments.txt', $txt);
}

function do_plot($data) {
  $labels = array();
  $bugs = array();
  $bugs_wo_undef = array();

  foreach ($data as $t) {
    $labels[]        = format_date($t[6]);
    $bugs[]          = $t[1];
    $bugs_wo_undef[] = $t[2];
  }

  $labels        = implode("',\n'", $labels);
  $bugs          = implode(",\n", $bugs);
  $bugs_wo_undef = implode(",\n", $bugs_wo_undef);

  echo <<< HTML
<div style="width:75%">
  <canvas id="llvmbugs"></canvas>
</div>
<script>
var ctx = document.getElementById('llvmbugs').getContext('2d');
new Chart(ctx, {
  type: 'line',
  data: {
    labels: ['$labels'],
    datasets: [{
      label: 'Failures',
      fill: false,
      backgroundColor: 'rgba(255, 0, 0, 0.5)',
      borderColor: 'rgba(255, 0, 0, 0.5)',
      data: [
        $bugs
      ]
    }, {
      label: 'Failures if undef ignored',
      fill: false,
      backgroundColor: 'rgba(0, 0, 255, 0.5)',
      borderColor: 'rgba(0, 0, 255, 0.5)',
      data: [
        $bugs_wo_undef
      ],
    }]
  },
  options: {
    tooltips: {
      mode: 'index',
      intersect: false,
    },
    hover: {
      mode: 'nearest',
      intersect: true
    },
    scales: {
      xAxes: [{
        display: true,
        scaleLabel: {
          display: true,
          labelString: 'Date & LLVM commit'
        }
      }],
      yAxes: [{
        display: true,
        scaleLabel: {
          display: true,
          labelString: 'Alive2 Test Failures'
        },
        ticks: {
          beginAtZero: true
        }
      }]
    }
  }
});
</script>

HTML;
}
